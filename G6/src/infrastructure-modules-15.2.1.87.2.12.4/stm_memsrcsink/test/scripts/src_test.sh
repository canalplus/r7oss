#!/bin/sh

debugfs_dir=/sys/kernel/debug

if
    ! lsmod | grep -q -s memsrcsink_tests
        then
	        modprobe infra_test
fi

	export SRC="$debugfs_dir/memsrcsink_test/data/src0"
	export INIT="$debugfs_dir/memsrcsink_test/data/init0"

	echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"

	#will be used in future . Right now bytes are hardcode in test case
	#echo -n "i" > "$debugfs_dir/memsrsrc_test/in_fill"
	#echo -n "o" > "$debugfs_dir/memsrsrc_test/out_fill"

	cat $INIT
	cat $SRC
	cat $INIT

