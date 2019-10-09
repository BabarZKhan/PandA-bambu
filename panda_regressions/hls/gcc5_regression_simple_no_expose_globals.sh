#!/bin/bash
$(dirname $0)/../../etc/scripts/test_panda.py gcc_regression_simple --tool=bambu \
   --args="--configuration-name=GCC5_O0 -O0 -lm --simulate --do-not-expose-globals --experimental-setup=BAMBU --compiler=I386_GCC5" \
   --args="--configuration-name=GCC5_O1 -O1 -lm --simulate --do-not-expose-globals --experimental-setup=BAMBU --compiler=I386_GCC5" \
   --args="--configuration-name=GCC5_O2 -O2 -lm --simulate --do-not-expose-globals --experimental-setup=BAMBU --compiler=I386_GCC5" \
   --args="--configuration-name=GCC5_O3 -O3 -lm --simulate --do-not-expose-globals --experimental-setup=BAMBU --compiler=I386_GCC5" \
   -o output_gcc5_simple_do_not_expose_globals -b$(dirname $0) --table=output_gcc5_simple_do_not_expose_globals.tex --name="Gcc5RegressionSimpleDNEG" $@
exit $?
