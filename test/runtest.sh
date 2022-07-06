#!/bin/sh

alias lineref="_lineref_ \"\$LINENO\""

test_def(){

lineref do_test "build from zero" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "target alredy built" full.test "$RBE" <<EOF
>> building all
EOF

rm full.test partial.test 'the separator.test'

lineref do_test "removed a full branch of the graph" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "---------------------------" > 'the separator.test.txt'

lineref do_test "change deep in the graph" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "A" > 'a.test.txt'
echo "B" > 'b.test.txt'

lineref do_test "two brother changes" full.test "$RBE" all <<EOF
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "a" > 'a.test.txt'
echo "===========================" > 'the separator.test.txt'

lineref do_test "two unrelated changes" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building partial.test
>> building full.test
>> building all
EOF

mv _alt_a.test.txt alt_a.test.txt

lineref do_test "creation of an optional source" full.test "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_a.test.txt

lineref do_test "change in a optional source" full.test "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_b.test.txt

lineref do_test "change in a source masked by another optional one" full.test "$RBE" all <<EOF
>> building all
EOF

mv alt_a.test.txt _alt_a.test.txt

lineref do_test "remove of an optional source" full.test "$RBE" all <<EOF
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "dumb fallback" full.test $DUMBBUILD all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

lineref do_test "cycle" full.test "$RBE" cycle <<EOF
EOF

lineref do_test "cycle" full.test "$RBE" cycle_c <<EOF
>> cycle_g done
>> cycle_h done
EOF

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
  output="$2"
  cmd="$3"
  shift 3
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

# ---------------------------------------------------------------------------------

main(){
  test_setup
  test_def
  test_end
}

main

