#!/bin/sh

debugfs_dir=/sys/kernel/debug

if
    ! lsmod | grep -q -s memsrcsink_tests
    then
        modprobe infra_test
fi



debugfs_dir=/sys/kernel/debug
mss_dir=$debugfs_dir/memsrcsink_test/data

export INIT0="$mss_dir/init0"
export INIT1="$mss_dir/init1"
export INIT2="/$mss_dir/init2"
export INIT3="/$mss_dir/init3"

#echo detach>$mss_dir/sink_api
#echo delete> $mss_dir/sink_api
#echo new> $mss_dir/sink_api
#echo attach> $mss_dir/sink_api


cat $INIT0
if [ $? != 0 ] ; then
    echo "INIT0 not ok"
fi

cat $INIT1
if [ $? = 0 ] ; then
    echo "INIT1 not ok"
fi

cat $INIT2
cat $INIT3


export SINK0="$debugfs_dir/memsrcsink_test/data/sink0"
export SINK1="$debugfs_dir/memsrcsink_test/data/sink1"
export SINK2="$debugfs_dir/memsrcsink_test/data/sink2"
export SINK3="$debugfs_dir/memsrcsink_test/data/sink3"


# testing memsink when pull_count = producer_count
echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
echo 1024 > "$debugfs_dir/memsrcsink_test/producer_count"

# sink0 and sink1 are blocking interfaces
cat $INIT0
cat $SINK0
cat $INIT0

cat $INIT1
cat $SINK1
cat $INIT1


# sink2 and sink3 are non blocking interfaces
cat $INIT2
cat $SINK2
cat $INIT2

cat $INIT3
cat $SINK3
cat $INIT3



# testing memsink when pull_count > producer_count
echo 2024 > "$debugfs_dir/memsrcsink_test/pull_count"
echo 1024 > "$debugfs_dir/memsrcsink_test/producer_count"

# sink0 and sink1 are blocking interfaces
cat $INIT0
cat $SINK0
cat $INIT0

cat $INIT1
cat $SINK1
cat $INIT1


# sink2 and sink3 are non blocking interfaces
cat $INIT2
cat $SINK2
cat $INIT2

cat $INIT3
cat $SINK3
cat $INIT3


# testing memsink when pull_count < producer_count
echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
echo 512 > "$debugfs_dir/memsrcsink_test/producer_count"

# sink0 and sink1 are blocking interfaces
cat $INIT0
cat $SINK0
cat $INIT0

cat $INIT1
cat $SINK1
cat $INIT1


# sink2 and sink3 are non blocking interfaces
cat $INIT2
cat $SINK2
cat $INIT2

cat $INIT3
cat $SINK3
cat $INIT3








