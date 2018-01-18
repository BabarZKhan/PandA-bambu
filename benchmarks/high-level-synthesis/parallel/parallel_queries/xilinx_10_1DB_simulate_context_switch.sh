#!/bin/bash
COMMONARGS="--compiler=I386_GCC49 --std=c99 --experimental-set=BAMBU -O3 -v3 -fno-delete-null-pointer-checks -fopenmp --pragma-parse --mem-delay-read=20 --mem-delay-write=20 --channels-type=MEM_ACC_11 --memory-allocation-policy=NO_BRAM --no-iob --device-name=xc7vx690t-3ffg1930-VVD  --top-fname=test --top-rtldesign-name=search --simulate --clock-period=10 -DMAX_VERTEX_NUMBER=26455 -DMAX_EDGE_NUMBER=100573 --context_switch=8"
 NAME=`basename $0 .sh`
`dirname $0`/../../../../etc/scripts/test_panda.py --spider-style="../lib/latex_format_bambu_results_xilinx.xml" --tool=bambu -l`dirname $0`/list_1DB -ooutput_$NAME --commonargs="$COMMONARGS" \
      --args="--configuration-name=4W-4CH -DN_THREADS=4 --num-threads=4 --memory-banks-number=4 --channels-number=2"\
      -t1440m --table=$NAME.tex --benchmarks_root=`dirname $0` --name="$NAME" $@
return_value=$?
if test $return_value != 0; then
   exit $return_value
fi
exit 0

