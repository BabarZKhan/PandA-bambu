#!/bin/bash
$(dirname $0)/../../etc/scripts/test_panda.py --tool=bambu -llibm-tests_list -o output_libm-testsClang -b$(dirname $0) --table=libm-testsClang.tex --name="LibmTestsClang" \
   --args="--configuration-name=soft-floatC4 --compiler=I386_CLANG4 --soft-float --simulate --experimental-setup=BAMBU -lm --reset-type=sync -DNO_MAIN -O0 -DFAITHFULLY_ROUNDED" \
   --args="--configuration-name=soft-floatC5 --compiler=I386_CLANG5 --soft-float --simulate --experimental-setup=BAMBU -lm --reset-type=sync -DNO_MAIN -O0 -DFAITHFULLY_ROUNDED" \
  $@
return_value=$?
if test $return_value != 0; then
   exit $return_value
fi
exit 0
