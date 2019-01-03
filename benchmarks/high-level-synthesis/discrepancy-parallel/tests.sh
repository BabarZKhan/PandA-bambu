#!/bin/bash
`dirname $0`/../../../etc/scripts/test_panda.py --tool=bambu \
	--args="-v4 --configuration-name=baseline --simulate --no-clean --std=c11 --disable-function-proxy" \
	--args="-v4 --configuration-name=vcd --simulate --generate-vcd --no-clean --std=c11 --disable-function-proxy" \
	--args="-v4 --configuration-name=discrepancy --simulate --no-clean --std=c11 --discrepancy --discrepancy-no-load-pointers --discrepancy-force-uninitialized --disable-function-proxy" \
	-ltest_list -o output_dir -b`dirname $0` --table=results.tex $@
return_value=$?
if test $return_value != 0; then
	exit $return_value
fi
exit 0
