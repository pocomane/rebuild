#!/bin/sh

alias lineref="_lineref_ \"\$LINENO\""

test_def(){

lineref do_test "build from zero" "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "target alredy built" "$RBE" <<EOF
>> building all
EOF

rm full.test partial.test 'the separator.test'

lineref do_test "removed a full branch of the graph" "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "---------------------------" > 'the separator.test.txt'

lineref do_test "change deep in the graph" "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "A" > 'a.test.txt'
echo "B" > 'b.test.txt'

lineref do_test "two brother changes" "$RBE" all <<EOF
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "a" > 'a.test.txt'
echo "===========================" > 'the separator.test.txt'

lineref do_test "two unrelated changes" "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building partial.test
>> building full.test
>> building all
EOF

mv _alt_a.test.txt alt_a.test.txt

lineref do_test "creation of an optional source" "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_a.test.txt

lineref do_test "change in a optional source" "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_b.test.txt

lineref do_test "change in a source masked by another optional one" "$RBE" all <<EOF
>> building all
EOF

mv alt_a.test.txt _alt_a.test.txt

lineref do_test "remove of an optional source" "$RBE" all <<EOF
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "dumb fallback" $DUMBBUILD all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "cycle" "$RBE" cycle <<EOF
EOF

lineref do_test "cycle" "$RBE" cycle_c <<EOF
>> cycle_g done
>> cycle_h done
EOF

lineref do_test "sources in subfolder" "$RBE" subfolder.test <<EOF
>> building generic subfolder/x.test
>> building generic subfolder/y.test
>> building subfolder.test
EOF

lineref check_in_db ./ subfolder/x.test
lineref check_no_db ./subfolder

}

# ---------------------------------------------------------------------------------

die(){
  echo "runtest stop - last check at line $line - $@"
  exit 13
}

test_setup(){
  CALLDIR="$(readlink -f "$(dirname "$0")")"

  set +x
  set -e

  cd "$CALLDIR/.."
  rm -fR test.run/
  mkdir -p test.run
  cd test.run

  cp -fR ../test/* ./

  if [ "$RBE" != "" ] ; then
    DUMBBUILD="./build.cmd"
  else

    # POSIX test
    gcc -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o rebuild ../rebuild.c
    DUMBBUILD="./build.cmd"
    export RBE="./rebuild"

    # # WINDOWS test
    # gcc -std=c99 -Wall -D_WIN32 -o rebuild ../rebuild.c
    # mv build.cmd build.sh
    # mv build.bat build.cmd
    # DUMBBUILD="./build.sh"
    # export RBE="./rebuild.exe"
  fi
  chmod ugo+x ./build.cmd
  chmod ugo+x "$DUMBBUILD"

  mkdir -p subfolder
  cat > the\ separator.test.txt <<EOF
===========================
EOF
  cat > a.test.txt <<EOF
a
EOF
  cat > alt_b.test.txt <<EOF
alt_b
EOF
  cat > b.test.txt <<EOF
b
EOF
  cat > _alt_a.test.txt <<EOF
alt_a
EOF
  cat > subfolder/x.test.txt <<EOF
x
EOF
  cat > subfolder/y.test.txt <<EOF
x
EOF

  set -x
}

line=0
_lineref_(){
  set +e
  set +x
  line="$1"
  shift
  set -e
  set -x
  "$@"
}

count=0
do_test(){
  set +e
  set +x
  info="$1"
  cmd="$2"
  shift 2
  if [ "$count" = 0 ] ; then
    rm -f test_*.out diff_*.left diff_*.right diff_*.out
  fi
  count=$((count+1))
  cat > diff_"$count".right
  echo "..........................................................................."
  echo "-> TEST $count"
  ("$cmd" "$@" 2>&1 )| tee test_"$count".out
  # ("$cmd" -n "$@" 2>&1 )| tee test_"$count".out
  echo "'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''"
  cat test_"$count".out | grep '^>>' | tr -d '\r' > diff_"$count".left
  diff diff_"$count".left diff_"$count".right | tee diff_"$count".out
  difout="$(cat "diff_$count.out")"
  [ "$?" = 0 ] && [ "$difout" = "" ] || die "test n.$count '$info' FAILED"
  set -e
  set -x
}

test_end(){
  echo "all is ok"
}

_get_db_path_(){
  if [ "$2" = "" ] ; then
    DBPATH="$1/.rebuild"
  else
    DBPATH="$1/.rebuild/${2}_dep.txt"
  fi
}

check_in_db(){
  _get_db_path_ "$1" "$2"
  if [ \! -f "$DBPATH" ]; then
    die "db not found for $@"
    exit 13
  fi
  return 0
}

check_no_db(){
  _get_db_path_ "$1"
  if [ -e "$DBPATH" ]; then
    die "unexpected db found in $@"
    exit 13
  fi
  return 0
}

# ---------------------------------------------------------------------------------

main(){
  test_setup
  test_def
  test_end
}

main

