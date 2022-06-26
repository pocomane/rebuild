#!/bin/sh

alias do_test="_do_test_ \"\$LINENO\""

test_def(){

do_test "build from zero" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

do_test "target alredy built" full.test "$RBE" all <<EOF
>> building all
EOF

rm full.test partial.test 'the separator.test'

do_test "removed a full branch of the graph" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "---------------------------" > 'the separator.test.txt'

do_test "change deep in the graph" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "A" > 'a.test.txt'
echo "B" > 'b.test.txt'

do_test "two brother changes" full.test "$RBE" all <<EOF
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building full.test
>> building all
EOF

echo "a" > 'a.test.txt'
echo "===========================" > 'the separator.test.txt'

do_test "two unrelated changes" full.test "$RBE" all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building partial.test
>> building full.test
>> building all
EOF

mv _alt_a.test.txt alt_a.test.txt

do_test "creation of an optional source" full.test "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_a.test.txt

do_test "change in a optional source" full.test "$RBE" all <<EOF
>> building alt.test (alt)
>> building full.test
>> building all
EOF

echo "xxx" >> alt_b.test.txt

do_test "change in a source masked by another optional one" full.test "$RBE" all <<EOF
>> building all
EOF

mv alt_a.test.txt _alt_a.test.txt

do_test "remove of an optional source" full.test "$RBE" all <<EOF
>> building alt.test
>> building full.test
>> building all
EOF

do_test "dumb fallback" full.test $DUMBBUILD all <<EOF
>> building generic the separator.test
>> building generic a.test
>> building generic b.test
>> building partial.test
>> building alt.test
>> building full.test
>> building all
EOF

do_test "cycle" full.test "$RBE" cycle <<EOF
EOF

do_test "cycle" full.test "$RBE" cycle_c <<EOF
>> cycle_g done
>> cycle_h done
EOF

}

# ---------------------------------------------------------------------------------

die(){
  echo "$@"
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

  cp ../test/* ./

  if [ "$RBE" != "" ] ; then
    DUMBBUILD="./build.cmd"
  else

    # POSIX test
    gcc -g -o ./rebuild ../rebuild.c
    DUMBBUILD="./build.cmd"
    export RBE="./rebuild"

    # WINDOES test
    # gcc -g -o ./rebuild.exe ../rebuild.c
    # mv build.cmd build.sh
    # mv build.bat build.cmd
    # DUMBBUILD="./build.sh"
    # export RBE="./rebuild.exe"
  fi
  chmod ugo+x ./build.cmd
  chmod ugo+x "$DUMBBUILD"

  set -x
}

count=0
_do_test_(){
  set +e
  set +x
  info="$1 - $2"
  output="$3"
  cmd="$4"
  shift 4
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
  [ "$?" = 0 ] && [ "$difout" = "" ] || die "test $count failed: $info"
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

