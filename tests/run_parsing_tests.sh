#!/bin/bash

ECHO=`which echo`

DIFF_FLAGS="-u"
case "$(uname)" in
  *W32*)
    DIFF_FLAGS="-wu"
    ;;
esac

TEST_BIN="$1"
TESTPATH=`dirname $0`

SUCCESS_MARKER=$'\033[32mSUCCESS\033[0m'
FAILURE_MARKER=$'\033[31mFAILURE\033[0m'

${ECHO} "using test binary: $TEST_BIN"

test_bin_name=`basename $TEST_BIN`

tests_succeeded=0
tests_total=0

for file in $TESTPATH/parsing-cases/*.json ; do
  allow_comments=""
  allow_garbage=""
  allow_multiple=""
  allow_partials=""

  # if the filename starts with dc_, we disallow comments for this test
  case $(basename $file) in
    ac_*)
      allow_comments="-c "
    ;;
    ag_*)
      allow_garbage="-g "
     ;;
    am_*)
     allow_multiple="-m ";
     ;;
    ap_*)
     allow_partials="-p ";
    ;;
  esac
  fileShort=`basename $file`
  testName=`echo $fileShort | sed -e 's/\.json$//'`

  ${ECHO} -n " test ($testName): "
  iter=1
  success=$SUCCESS_MARKER

  # parse with a read buffer size ranging from 1-31 to stress stream parsing
  while [ $iter -lt 32  ] && [ $success = $SUCCESS_MARKER ] ; do
    $TEST_BIN $allow_partials $allow_comments $allow_garbage $allow_multiple -b $iter < $file > ${file}.test  2>&1
    diff ${DIFF_FLAGS} "${file}.gold" "${file}.test" > "${file}.out"
    if [ $? -eq 0 ] ; then
      if [ $iter -eq 31 ] ; then tests_succeeded=$(( $tests_succeeded + 1 )) ; fi
    else
      success=$FAILURE_MARKER
      iter=32
      ${ECHO}
      cat ${file}.out
    fi
    iter=$(( iter + 1 ))
    rm ${file}.test ${file}.out
  done

  ${ECHO} $success
  tests_total=$(( tests_total + 1 ))
done

${ECHO} $tests_succeeded/$tests_total tests successful

if [ $tests_succeeded != $tests_total ] ; then
  exit 1
fi

exit 0
