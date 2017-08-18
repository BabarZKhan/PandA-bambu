#!/bin/bash
script=`readlink -e $0`
root_dir=`dirname $script`
export PATH=../../src:../../../src:/opt/panda/bin:$PATH

mkdir -p sim
cd sim
echo "# HLS synthesis, testbench generation and simulation with ICARUS"
bambu -v2 -O3 $root_dir/module.c --simulate --generate-tb=$root_dir/test.xml --simulator=ICARUS --pretty-print=a.c --channels-type=MEM_ACC_NN --device-name=EP2C70F896C6-R --memory-allocation-policy=EXT_PIPELINED_BRAM --experimental-setup=BAMBU --flopoco
return_value=$?
if test $return_value != 0; then
   exit $return_value
fi
cd ..

mkdir -p synth
cd synth
echo "# HLS synthesis, testbench generation, simulation with ICARUS and RTL synthesis with Quartus"
bambu -v3 -O3 $root_dir/module.c --generate-tb=$root_dir/test.xml --evaluation --simulator=ICARUS --pretty-print=a.c --channels-type=MEM_ACC_NN --device-name=EP2C70F896C6-R --memory-allocation-policy=EXT_PIPELINED_BRAM --experimental-setup=BAMBU --flopoco
return_value=$?
if test $return_value != 0; then
   exit $return_value
fi
cd ..
exit 0
