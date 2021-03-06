#!/bin/sh
#!./busybox sh

#######################################################################
# This script is ment to be run with the `rebuild` build system
# If you do not have it, here there is a dumb fallback that always
# build everything. You can enable it just by executing thid file
# directly, without setting the REBUILD environment variable

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

# End of the dumb fallback
#######################################################################

set -e
case "$TARGET" in

  all )
    "$REBUILD" ifchange full.test
    echo ">> building all"
    cat full.test
  ;;
  
  "full.test" )
    "$REBUILD" ifchange "the separator.test" partial.test alt.test
    echo ">> building full.test"
    cat "the separator.test" alt.test partial.test > "$OUTPUT"
  ;;

  "partial.test" )
    "$REBUILD" ifchange a.test b.test "the separator.test"
    echo ">> building partial.test"
    cat a.test b.test "the separator.test" > "$OUTPUT"
  ;;

  "alt.test" )
    source_a="alt_a.test.txt"
    source_b="alt_b.test.txt"
    if [ -f "$source_a" ] ; then
      "$REBUILD" ifchange "$source_a"
      echo ">> building alt.test (alt)"
      cat "$source_a" > "$OUTPUT"
    else
      "$REBUILD" ifchange "$source_b"
      "$REBUILD" ifcreate "$source_a"
      echo ">> building alt.test"
      cat "$source_b" > "$OUTPUT"
    fi
  ;;

  "failing_return" )
    "$REBUILD" ifchange fail_ret
    echo ">> building failing_return"
  ;;

  "fail_ret" )
    "$REBUILD" ifchange fail_a good
    echo ">> building failing_ret"
  ;;

  "fail_a" )
    echo ">> building fail_a"
    exit 13
  ;;

  "out_and_fail" )
    "$REBUILD" ifchange out_and_fail_slave
    echo ">> building out_and_fail"
  ;;

  "out_and_fail_slave" )
    "$REBUILD" ifchange good_x
    echo ">> building out_and_fail_slave"
    echo "ok" > "$OUTPUT"
    exit 13
  ;;

  "good_x" )
    echo ">> building good_x"
    echo "ok" > "$OUTPUT"
  ;;

  "good" )
    echo ">> building good"
    echo "ok" > "$OUTPUT"
  ;;

  "subfolder.test" )
    "$REBUILD" ifchange subfolder/x.test subfolder/y.test
    echo ">> building subfolder.test"
    cat subfolder/x.test subfolder/y.test > "$OUTPUT"
  ;;

  "subproject.test" )
    # TODO : find clean way to launch sub-project build
    REBUILD_BUILDER="$PWD/subproject/build.cmd" "$REBUILD" ifchange subproject/full.sub
    echo ">> building subproject.test"
    cat subproject/full.sub > "$OUTPUT"
  ;;

  *".test" )
    case "$REBUILD_PREFIX" in
      *"subfolder"* )
        source="$OUTPUT.txt"
        "$REBUILD" ifchange "$source"
        echo ">> building subfolder generic $TARGET"
        cat "$source" > "$OUTPUT"
      ;;
      * )
        source="$TARGET.txt"
        "$REBUILD" ifchange "$source"
        echo ">> building generic $TARGET"
        cat "$source" > "$OUTPUT"
      ;;
    esac
  ;;

  clea[rn] )
    echo ">> building clean/r"
    rm -fR build/ *.test
  ;;

  cycle )
    "$REBUILD" ifchange cycle_a
    echo ">> cycle done" | tee "$OUTPUT"
  ;;

  cycle_a )
    "$REBUILD" ifchange cycle_b
    echo ">> cycle_a done" | tee "$OUTPUT"
  ;;

  cycle_b )
    "$REBUILD" ifchange cycle_a
    echo ">> cycle_b done" | tee "$OUTPUT"
  ;;

  cycle_c )
    "$REBUILD" ifchange cycle_g cycle_d
    echo ">> cycle_c done" | tee "$OUTPUT"
  ;;

  cycle_d )
    "$REBUILD" ifchange cycle_e
    echo ">> cycle_d done" | tee "$OUTPUT"
  ;;

  cycle_e )
    "$REBUILD" ifchange cycle_f
    echo ">> cycle_e done" | tee "$OUTPUT"
  ;;

  cycle_f )
    "$REBUILD" ifchange cycle_g cycle_h cycle_d
    echo ">> cycle_b done" | tee "$OUTPUT"
  ;;

  cycle_g )
    echo ">> cycle_g done" | tee "$OUTPUT"
  ;;

  cycle_h )
    echo ">> cycle_h done" | tee "$OUTPUT"
  ;;

esac

