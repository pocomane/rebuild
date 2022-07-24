
# Rebuild - REcursive BUILD system

`Rebuild` is the most simple build system I know (except maybe ad-hoc build scripts)
that still supports incremental build. It is written in `C` with very few
system-dependent calls, it supports unix (linux, macos, BSD, etc) and windows,
and no malloc/free were harmed in the source of this software.

It is strongly inspired by the `Reco` build system: all the smart
ideas come from that project, however, for sake of clarity, I will present
`Rebuild` by its own, relegating the comparison to a dedicated section.

`Rebuild` is public domain, and it is provided with NO WARRANTIES.

# Usage

You have to write an external build script (EBS) in the file `build.cmd`, in
any language you like, interpreted or compiled too. Than you can run `rebuild`
in the same folder.

this will build the `all` target, otherwise you can specify a different target
by command line:
```
rebuild target_name
```

The EBS will inform `rebuild` of the target dependencies and it will execute
the build instructions (see next section for more details); it pratically
reasembles the content of, for example, a `Makefile`. It does not have to check
which target is up-to-date or not, that is hadled by the `rebuild` system: it
will take care to recursivelly call the EBS over the target that needs to be
rebuilt. For more details see the rest of the documentation.

# Build

Static binaries for Linux (x86 and arm) and windows (x86) are in the
[release page](https://github.com/pocomane/rebuild/releases).
However you may want to build it by yourself.

`Rebuild` should be built simply with:
```
gcc -o rebuild rebuild.c
```

Hoever some compiler installation may have incompatible defaults, so the complete
line should be something like:
```
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -o rebuild rebuild.c
```
or
```
gcc -std=c99 -D_WIN32 -o rebuild rebuild.c
```

Because of how `rebuild` works, and due to its simple and quick compilation,
you may consider to reditribute its source within your project. So you can make
a simple wrapper to compile it before to build your project, i.e.:
```
#!/bin/sh
gcc -o rebuild rebuild.c && ./rebuild
```

# How it works

When you run `rebuild all` in a directory, it simply recall the EBS with the
environment variable `TARGET` set to `all`. This is not much useful,
however another way to run it is with
```
rebuild ifchange my_app.exe
```
that will call the EBS, with the `my_app.exe` argument, only if the file `my_app.exe`
is out of date (see the following dicussion for more detail). The core
mechanism rely on the fact that the EBS can recursively call `rebuild`. Let's
for example implement an EBS as a `sh` script.  So, we create an executable
file named `build.cmd` with the following contents:
```
#!/bin/sh
set -e
case "$TARGET" in
    all )
        "$REBUILD" ifchange my_app.exe
        ;;
    my_app.exe )
        "$REBUILD" ifchange main.o lib.o
        gcc -o "$OUTPUT" -c my_target.c
        ;;
    main.o )
        "$REBUILD" ifchange main.c
        gcc -o "$OUTPUT" -c main.c
    lib.o )
        "$REBUILD" ifchange lib.c
        gcc -o "$OUTPUT" -c lib.c
esac
```

There are other two environment variables se by `rebuild`: `REBUILD` containing
the path to the caller `rebuild`, and `OUTPUT` that contains the path where
`rebuild` expects that EBS put its output.  The build went more or less in the
following way
- We run `rebuild all` in the same directory of `build.cmd` so, `build.cmd` is executed
  with the `TARGET=all`; it is a text file with a shebang, so the kernel simply
  run `/bin/sh` with `build.cmd` as input
- The only thing the script does with the `all` target is to recall `rebuild ifchange`
  on the `my_app.exe` target
- `rebuild ifchange` will store on the disk the fact that `all` depend on `my_app.exe`;
  then the first time it is executed it just will recall again `build.cmd` with the new
  `TARGET=my_app.exe`
- Subsequent invocation will call the EBS only if one of the dependencies of
  `my_app.exe` (tracked in the previous run) differs from the last time `my_app.exe`
  was built.
- In case of deeper dependencies, `rebuild ifchange` will check also the dependency
  of the dependency to decide if call the EBS, and so on; it is all tracked in the
  previous run
- So targets starts to recursevelly call each other until something should really
  be made, for example the `main.o` target, will call `main.c`, but there is nothing
  to do for it, so the EBS will immediatelly return wihtout error, so `main.o`
  can continue and `gcc -o "$OUTPUT" -c main.c` is executed. This show the main
  contract between `rebuild` and the EBS: the latter must produce the result in
  the path passed in `TARGET` and if no errors occurs, the former can
  continue copyng the result in the right place, calling other targets, and so on.
- In case one of the call to EBS ends with error, `rebuild` will dispacth the error
  to the parent EBS call that is written so that it will emmediatelly stop with
  an error (due to the `set -e` at the beginning); pratically the error is propagated
  all the way up the process stack, stopping the build process.

To decide if a dependency differs from the last run, `rebuild` will store on
disk an hash and a timestamp after each successful build. The timestamp of the
dependency file is compared to the one stored on disk, so there is never a
comparison between timestamp of target and dependency (like `gnu make` does for
example). If the timestamps matches also the hash is compared. If also it
matches, and it is the same for all the dependencies, the target is considered
up to date.

Please note that `rebuild`, to track its operation, uses also other environment
variables described in the followin sections. The EBS must take care to not
modify them before calling `rebuild` recursively.

# Automatic prerequisites and targets

With `rebuild` multiple target pattern, or procedurally generated prerequisites
have nothing special: the former are a match over `$TARGET` while the
letter can be calculete just before being passed to `rebuild ifcreate` as
arguments. For example, the automatic `gcc` dependency discover can be used as
follows:
```
#!/bin/sh
set -e
case "$TARGET" in
    all )
        "$REBUILD" ifchange main.o
        gcc -o "$OUTPUT" *.o
        ;;
    *.o )
        FNAM="$(echo "$TARGET" | sed 's/\.o$//')"
        DEPS="$(gcc -MM "FNAM.c" | sed 's/\\$//g' | tr -d '\n' | sed 's/^[^:]*://')"
        "$REBUILD" ifchange "$FNAM.c" $DEPS
        gcc -o "$OUTPUT" -c "$FNAM".c
esac
```

# Optional prerequisites

Let's say that the build depends on two files `a` and `b` in a "Cascade" logic: if
`a` exists it will be used, otherwise it will be fallback to `b`. This happen
for example with the `gcc` includes: there are a list of place into look for the
header, and the first found will be used.

if we first run the build script without the file `a`, the EBS will found `b` only,
will add it to the dependency, and will go on. If after the build, we create the
`a` file  and we re-run the build script, nothing will be done. This is because
the dependency upon `a` was not generated before. For this reason there
is the `ifcreate` directive.

The core idea is the following:
```
#!/bin/sh
set -e
case "$TARGET" in
  all )
    "$REBUILD" ifchange alt
    echo "doing the top-level stuff..."

  "alt.test" )
    if [ -f "alt_a.txt" ] ; then
      "$REBUILD" ifchange alt_a.txt
      echo "doing the alt_a stuff..."
    else
      "$REBUILD" ifchange alt_b.txt
      "$REBUILD" ifcreate alt_a.txt
      echo "doing the alt_b stuff..."
    fi
  ;;
esac
```

If our build find the `a` file, all works like before, however if it is not
found, `b` will be used and stored in the usual `ifchange` mode, but also `a`
is stored in the `ifcreate` mode. This instruct `rebuild` to re-run the EBS for
the current target if in future the file `a` will be created. Note that the
`ifchange` will re-run the build also if a previously existing file is deleted,
so tor the `a` case, just an `ifchange` clause is needed.

Classical build system like `make` do not support at all this kind of dependencies,
but using it in `rebuild` could be still quite difficult to use, sice normally
the build utilities do not handle such cases; for example `gcc -M` generate the
dependency of a `.c` files, but it does not emit the the optional one.

# Target folders

The target system is strictly linked to the file tree structure. In fact, when
a target contains one or more '/' chars, only the part after the last '/' is
considered to be the target name, i.e. it will be exported in the `TARGET` variable.
The rest will be considered as a subpath in which to move before launching EBS.

For example:
```
#!/bin/sh
set -e
case "$TARGET" in
    all )
        # liking
        "$REBUILD" ifchange main.o lib_a/a.o lib_b/b.o
        gcc -o "$OUTPUT" main.o lib_a/a.o lib_b/b.o
        ;;
    *.o )
        # compilation
        FNAM="$(echo "$TARGET" | sed 's/\.o$//')"
        "$REBUILD" ifchange "$FNAM.c" $DEPS
        gcc -o "$OUTPUT" -c "$FNAM".c
esac
```

will work in the following way:
- The compiliation rule will be run in the launching directory for the target `main.o`
- The compilation rule will be run in the `lib_a` folder for the target `a.o`
- The compilation rule will be run in the `lib_b` folder for the target `b.o`
- The linking rule will be run in the launching folder

This let's you to define general rules to be run in every folders, however you
may want to differentiate the command between subfolder; we suggest to do this
signaling the condition with an environment variable, instead of analyzing '$PWD':
```
#!/bin/sh
set -e
case "$TARGET" in
    all )
        # liking
        "$REBUILD" ifchange main.o
        SUBFOLDER=lib_a "$REBUILD" ifchange lib_a/a.o
        SUBFOLDER=lib_b "$REBUILD" ifchange lib_b/b.o
        gcc -o "$OUTPUT" main.o lib_a/a.o lib_b/b.o
        ;;
    *.o )
        case "$SUBFOLDER" in
            "lib_a" )
              echo "handle subdir lib_a"
              ;;
            "lib_b" )
              echo "handle subdir lib_a"
              ;;
            "" )
              echo "handle standard rule"
              ;;

... etc ...
```

Another way is to use another `build.cmd` in the sub-folder:
```
#!/bin/sh
set -e
case "$TARGET" in
    all )
        # liking
        REBUILD_BUILDER="" "$REBUILD" ifchange lib_a/a.o
        REBUILD_BUILDER="" "$REBUILD" ifchange lib_b/b.o
        gcc -o "$OUTPUT" main.o lib_a/a.o lib_b/b.o
        ;;
    *.o )
        # compilation
        FNAM="$(echo "$TARGET" | sed 's/\.o$//')"
        "$REBUILD" ifchange "$FNAM.c" $DEPS
        gcc -o "$OUTPUT" -c "$FNAM".c
esac
```
Note that in `REBUILD_BUILDER` you can also specify the full path for the new
EBS, but if it is a `build.cmd` file in the new dir, you can just set it blank,
so `rebuild` will search for the default one in the new directory.

This mechanism can be use also when you have a sub-project built with `rebuild`:
place it in a subdirectory, and from the parent project just call it with
```
REBUILD_BUILDER="" "$REBUILD" ifchange lib_a/a.o
```
For what we said, the rebuild system of the sub-project will be correctly
called; however there is a difference with respect to call `rebuild` manually
in the sub-directory: `rebuild` will use the same build database of the parent
project, instead of creating a new one inside the sub-project folder. So
building from the parent or from the child will result in two indipendent
built, but the output files will be in the same position.  This can result in
duplicate work, e.g. the first time you run in the parent and in the child, all
the targets will be made two times. However this conservative behaviour lets
`rebuild` to always have a correct build after each run (although the previous
one could be incomplete).

# Per-target build file

The `build.cmd` can be a simple dispacher, that select the file to be used with
some useful logic; for example, one can first search a specific build file for
the target, and if it not find, search for a generic one able to handle a whole
kind of file:
```
#!/bin/sh
if [ -e "$REBULD_TARGET.do" ] ; then
  buildfile="$REBUILT_TARGET.do"
else
  default="$(echo "$TARGET" | sed 's/.*\(\.[^\.]*\)$/default\1/')"
  if [ -e "$default.do" ] ; then
    buildfile="$default.do"
  else
    echo "no .do file found"
    return 13
  fi
fi
"./$buildfile" "$@"
```

Pratically, if we want to compile `main.o`, we first search for the rules in
`main.o.do`, if that file is not found, we look for `default.o.do`.

This let us also to add another interesting feature: dependency from the build
rules, i.e.  the ability to automatically rebuild a target when its build rule
change. To do this we just need to track the right dependencies in the previous
script:
```
#!/bin/sh
if [ -e "$TARGET.do" ] ; then
  buildfile="$TARGET.do"
  "$REBUILD" ifchange "$buildfile"
else
  default="$(echo "$TARGET" | sed 's/.*\(\.[^\.]*\)$/default\1/')"
  if [ -e "$default.do" ] ; then
    buildfile="$default.do"
    "$REBUILD" ifchange "$buildfile"
    "$REBUILD" ifcreate "$TARGET.do"
  else
    echo "no .do file found"
    return 13
  fi
fi
"./$buildfile" "$@"
```

Theorically you can do the same also with the single file `build.cmd` but it is
of little utility: everytime the build script is changed, ALL the target will
be redone. Instead in this way, for example, you can get only the `.o` remade
when a compilation flag is changed, or some more granular control.

# Environment variables and option

The following environment variables are set by rebuild, but it never uses them
directly; they are ment to pass information to EBS:

- `REBUILD` contains the path to the caller `rebuild`, so EBS can easly recall it.
- `TARGET` contains the name of the target to be build.
- `OUTPUT` contains the path where `rebuild` expects that EBS will place its output.

The following environment variables are used to configure some aspect of
`rebuild`:
- `REBUILD_CHECK_TIME` if set to `0` the timestamp will not be check to consider
  a target up to date.
- `REBUILD_CHECK_HASH` if set to `0` the hash will not be check to consider
  a target up to date.
- `REBUILD_VERBOSITY` if set to a value betweeen `1` or `9` will print more or
  less information during the execution (the default is `0` that prints the
  errors only).

// Execution tracking
These variables are used by `rebuild` to track the execution between recursive
calls, however some of them may be configured by the user to change the default
paths:
- `REBUILD_BUILDER` contains the path to the EBS, the user may overwrite it
  to run a different EBS.
- `REBUILD_DATABASE` contains the path to the database folder.
- `REBUILD_SEQUENCE` contains a list of the current chain of target that are
  built; the user should NEVER overwrite it, since it is very specific to the
  `rebuild` opertaions.

# The rebuild database

BLA bla // TODO : document it !

# Dumb fallback

A dumb version for the `rebuild` system, that always considers the target
out-of-date, can be implemented very quickly. This let you to embed in the EBS
itself a fallback in case a fully featured `rebuild` is
not present on the machine. For example, to let the previous EBS to be stand-alone,
one have just to put at the top:
```
# TODO : do chdir in the target folder !
if [ -z "$REBUILD" ] ; then
  TARGET="$1"
  REBUILD="rebuild_fallback_"
  REBUILD_EBS="$(readlink -f "$0")"
  if [ -z "$DUMB_DB" ] ; then
    DUMB_DB="./build/.rebuild.dumb.db"
    rm -fR "$DUMB_DB"
    mkdir -p "$DUMB_DB"
    DUMB_DB="$(readlink -f "$DUMB_DB")"
  fi
fi

rebuild_fallback_(){
  case "$1" in
    ifcreate ) shift ;;
    ifchange ) shift ;;
    target ) shift ;;
  esac
  for i in "$@" ; do
    if [ -f "$DUMB_DB/$i" ] ; then
      echo "dumb_rebuild: $i - was already done"
      return 0
    fi
    rm -f "$i.wip"
    echo "dumb_rebuild: $i - rebuilding"
    REBUILD="$REBUILD" REBUILD_EBS="$REBUILD_EBS" TARGET="$i" OUTPUT="$i.wip" DUMB_DB="$DUMB_DB" "$REBUILD_EBS"
    r=$?
    if [ "$r" = "0" ] ; then
      [ -f "$i.wip" ] && mv -- "$i.wip" "$i"
      touch "$DUMB_DB/$i"
      echo "dumb_rebuild: $i - done"
    else
      rm -f "$i.wip"
      echo "dumb_rebuild: $i - failed"
      exit $?
    fi
  done
}

```

So to compile the code, you can call it directly:
```
./build.cmd all
```

Such simple fallback still supports targets, procedural prerequisites, and all the
other `rebuild` features. The only thing not supported is the incremental build
that is truely needed only during the developing. For a source release, a
"Build always everything" policy could be enough.

# Comparison with Redo

The history and general working of `Redo`, the `Rebuild` main ispiration, can be read
[here](http://jdebp.info/FGA/introduction-to-redo.html). Quickly, it was
firstly described informally by [Daniel J. Bernstein in
2003](http://cr.yp.to/redo.html), than implemented by various people, in
[c](https://github.com/leahneukirchen/redo-c),
[python](https://github.com/apenwarr/redo) or
[sh](http://jdebp.info/Softwares/redo/grosskurth-redo.html).

The main idea is the same, however there are some differences.

First, `Redo` integrates a policy to discover rule-files, similar to the one showed
in a previous section: to compile a `main.o` file it will search for `main.o.do`,
or `default.o.do` if not found. We think that embedding such policy in `rebuild`
would reduce considerably its flexibility: which file use, where to search for it
and so on, is too much project dependent.

Furthermore, `Redo` will use either a temporary path and the standard output of the
builder script as generated file. This will cause some confusion: what is the
output if both the stdout and the temporary path is filled by the script? We
think the temporary path is enough, lets leave the stdout for the information
about the building process.

Another difference is that `Redo` is composed of trhee separated program `redo`,
`redo-ifcreate` and `redo-ifchange`. Using the single command we hope to make easly
use `rebuild` without a proper, system-wide, installation. `Rebuild` is simple, and
simple should be its usage and deploy.

`Redo` uses also command line argument to pass some information between the
processes, notably the target name and output path. Since the main use case for
EBS are shell script, we found `gcc -o "$OUTPUT" main.c` more readable than
`gcc -o "$1" main.c`. Moreover `OUTPUT`, `REBUILD_PARENT`, etc, are not meant
to be passed by the user to `rebuild` as parameters, so the environment variables
seems a better fit.

# Limitations

The target names are threated like raw byte array, but the newline (0x0D) and the
null (0x00) character have special meaning. So the target can be specified in any
format that do not use such bytes to encode other characters. For exampl, ASCII or
UTF8 will work, but UTF16 will not.

Under unix, you can place anything in the EBS default path `build.cmd`: if it is
a binary it will launched, otherwise, if it stargs with a shebang line, the kernel
will take care to launch the proper interpreter. This is the core mechanism that
let you to write EBS in any language. However Windows does not have such mechanism:
it simply decides what to do looking at the file extension. We choose the `.cmd`
exension exactly because it is parsed by `cmd.exe` by default, and so you can
always write a wrapper in it.

Another way to workaround this windows limitation is to use the `REBUILD_BUILDER`
environment variable to select a different EBS with the right extension, and write
a wrapper just to set such variable and launch `rebuild`.

