
#!/bin/bash
# test all

if
    ! lsmod | grep -q -s memsrcsink_tests
then
    modprobe infra_test
fi

#set -e

debugfs_dir=/sys/kernel/debug
mss_dir=$debugfs_dir/memsrcsink_test/data

INIT0="$mss_dir/init0"
INIT1="$mss_dir/init1"
INIT2="/$mss_dir/init2"
INIT3="/$mss_dir/init3"

#cat $INIT1
#cat $INIT2
#cat $INIT3
#cat $INIT4

function test_sinkapi(){
    echo detach>$mss_dir/sink_api
    echo delete> $mss_dir/sink_api
    echo new> $mss_dir/sink_api
    echo attach> $mss_dir/sink_api
    echo set_iomode> $mss_dir/sink_api
    echo set_ctrl> $mss_dir/sink_api
    echo get_ctrl> $mss_dir/sink_api
    return 0
}

function test_srcapi(){
    echo detach>$mss_dir/src_api
    echo delete> $mss_dir/src_api
    echo new> $mss_dir/src_api
    echo attach> $mss_dir/src_api
    return 0
}

function set_srcsink_iomode() {

    echo *set_srcsink_iomode $1*
    case $1 in
    0)
        echo "** sink0 mode BLOCKING"
	echo "** src0 mode BLOCKING"
        export SINK="$mss_dir/sink0"
	export SRC="$mss_dir/src0"
        export INIT="$mss_dir/init0"
        ;;
    1)
        echo "** sink1 mode BLOCKING"
	echo "** src1 mode NON BLOCKING"
        export SINK="$mss_dir/sink1"
        export SRC="$mss_dir/src1"
        export INIT="$mss_dir/init1"
        ;;
    2)
       echo "** sink2 mode NON BLOCKING"
       echo "** src2  mode BLOCKING"
       export SINK="$mss_dir/sink2"
       export SRC="$mss_dir/src2"
       export INIT="$mss_dir/init2"
	;;
   3)
       echo "** sink3 mode NON BLOCKING"
       echo "** src3  mode NON BLOCKING"
       export SINK="$mss_dir/src3"
       export SRC="$mss_dir/src3"
       export INIT="$mss_dir/init3"
 	;;
    *)
        return 1
        ;;
    esac
    return 0
}



function set_buffersize() {
    echo *set_buffersize $1*
    case $1 in
    0)
        echo "**** pull_count  = 1024"
        echo "**** producer_count = 1024"

	echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/producer_count"


	echo "*** push_count = 1024"
	echo "*** consumer_count = 1024"

	echo 1024 > "$debugfs_dir/memsrcsink_test/consumer_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"

        ;;
    1)
        echo "**** pull_count  = 512"
        echo "**** producer_count = 1024"

	echo 512 > "$debugfs_dir/memsrcsink_test/pull_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/producer_count"


	echo "*** push_count = 512"
	echo "*** consumer_count = 1024"

	echo 512 > "$debugfs_dir/memsrcsink_test/consumer_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"

        ;;
    2)
        echo "**** pull_count  = 1024"
        echo "**** producer_count = 512"

	echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
        echo 512 > "$debugfs_dir/memsrcsink_test/producer_count"


	echo "*** push_count = 1024"
	echo "*** consumer_count = 512"

	echo 512 > "$debugfs_dir/memsrcsink_test/consumer_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"
        ;;
    3)
        echo "**** pull_count  = 1024"
        echo "**** producer_count = 512"

	echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
        echo 512 > "$debugfs_dir/memsrcsink_test/producer_count"


	echo "*** push_count = 512"
	echo "*** consumer_count = 1024"

	echo 512 > "$debugfs_dir/memsrcsink_test/consumer_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"
        ;;
    4)
        echo "**** pull_count  = 783"
        echo "**** producer_count = 1024"

	echo 1024 > "$debugfs_dir/memsrcsink_test/pull_count"
        echo 512 > "$debugfs_dir/memsrcsink_test/producer_count"


	echo "*** push_count = 512"
	echo "*** consumer_count = 1024"

	echo 512 > "$debugfs_dir/memsrcsink_test/consumer_count"
        echo 1024 > "$debugfs_dir/memsrcsink_test/push_count"
        ;;
    *)
        return 1
        ;;
    esac
    return 0
}

function run_functest() {
    echo *run_functest $1*
    mycat=cat
    mysleep=sleep
    #$mycat $INIT
    case $1 in
    0)
        echo "****** sink & src &"
        $mycat $SRC &
        $mysleep 2
        $mycat $SINK &
    ;;
    1)
        echo "****** sink & src &"
        $mycat $SINK &
        $mysleep 2
        $mycat $SRC &
    ;;


  #  2)
  #     echo "****** (src ; src )& sink &"
  #     ($mycat $SRC ; $mycat $SRC) &
  #      $mysleep 2
  #      $mycat $SINK &
  #  ;;
  #  3)
  #      echo "****** (sink ; sink)& src &"
  #      ($mycat $SINK ; $mycat $SINK) &
  #      $mysleep 2
  #      $mycat $SRC &
  #  ;;
  #  4)
  #      echo "****** sink & (sink  ; sink)&"
  #     $mycat $SRC &
  #     $mysleep 2
  #      ($mycat $SINK ; $mycat $SINK) &
  #  ;;
  #  5)
  #      echo "****** sink & (src ; src)&"
  #      $mycat $SINK &
  #      $mysleep 2
  #      ($mycat $SRC ; $mycat $SRC) &
  #  ;;
   *)
        echo "****** no more command"
        return 1
        ;;
    esac

    $mysleep 2

    return 0
}

function resetall {
   cat $INIT0
   cat $INIT1
   cat $INIT2
   cat $INIT3

   return 0
}

function kernel_memtype_all {
	echo vmalloc_push >$mss_dir/kernel_memtype
	echo vmalloc_pull >$mss_dir/kernel_memtype
	return 0
}

function testall {
   test_sinkapi
   test_srcapi

   resetall


    mode=0
    while set_srcsink_iomode $mode ;
    do
        buffersize=0
        while set_buffersize $buffersize ;
        do
            runmode=0
            while
            echo "****** mode $mode buffersize $buffersize runmode $runmode"
            run_functest $runmode ;
            do
                echo
                let runmode++
            done
            let buffersize++
        done
        let mode++
    done
    sleep 2
    kernel_memtype_all
	return 0
}

CMDLINEPARAM=3     #  Expect at least command-line parameter.

print_help(){

	echo "*************************************************************************************"
	echo "	Usage : ./msstest_debugfs.sh --help"
	echo ""
	echo "Use these three arguments together --iomode = <>  --buffer_size = <>  --runmode = <>"
	echo "--testall  : run all tests supported by test suite"
	echo "--resetall : resets 4 interfaces each of sink and src"
	echo "--testsinkapi : tests sink apis  new , attach ,detach , delete"
	echo "--testsrcapi : tests src apis  new , attach ,detach , delete"
	echo "--kernel_memtype_all : test push pull interfaces when memeory type is kernel and kernel_vmalloc"
	echo ""
	echo "*************************************************************************************"
}

while [[ $1 = -* ]]; do
   arg=$1;
   shift

   case $arg in
   	--help)
		print_help
		break
	    ;;

	  --testall)
	       testall
	       break
	     ;;

	 --testsinkapi)
	 	test_sinkapi
		break
	   ;;

	  --testsrcapi)
	  	test_srcapi
		break
	   ;;

	 --resetall)
	 	resetall
		break
	   ;;

	  --kernel_memtype_all)
		  kernel_memtype_all
		  break
	    ;;

	 --iomode)
	    set_srcsink_iomode $1 || echo "Bad mode"
	    shift
	    ;;

	  --buffersize)
    	    set_buffersize $1 || echo "Bad buffer size"
	    shift
	    ;;

	  --runmode)
    	    run_functest $1 || echo "Bad runmode"
	    shift
	    ;;

	*)
	 echo "See help"
	 echo "Usage: $0 {--help|--testall|--resetall|--testsinkapi|--testsrcapi | --kernel_memtype_all}"
         break 1

	esac
done
