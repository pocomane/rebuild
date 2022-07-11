#!/bin/sh
#!./busybox sh

set -e
case "$TARGET" in

  all )
    "$REBUILD" ifchange full.sub
    echo ">> building all (sub)"
  ;;

  "full.sub" )
    "$REBUILD" ifchange x.sub y.sub
    echo ">> building full.sub (sub)"
    cat x.sub y.sub > "$OUTPUT"
  ;;

  "x.sub" )
    "$REBUILD" ifchange x.sub.txt z.sub
    echo ">> building x.sub (sub)"
    cat z.sub x.sub.txt > "$OUTPUT"
  ;;

  *".sub" )
    source="$TARGET.txt"
    "$REBUILD" ifchange "$source"
    echo ">> building generic $TARGET (sub)"
    cat "$source" > "$OUTPUT"
  ;;

esac

