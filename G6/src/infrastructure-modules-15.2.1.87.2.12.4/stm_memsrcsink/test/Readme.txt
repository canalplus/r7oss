Running tests.

Before running this test , it should be ensured that debuggs support in kernel is enabled i.e. CONFIG_DEBUG_FS = y .

1) modprobe infra_test
2) Go to path /sys/kernel/debug/memsrcsink_test


***Debugfs files :

Debugfs creates user/kernel interface with the mem src/sink kernel API.

4 different source and 4 different sink are created (src0, snk0, src1, snk1, src2, snk2, src3, snk3) .

Number |  Push mode (src) | Pull mode (sink)
-------+------------------+------------------
0      |  BLOCKING        | BLOCKING
1      |  NON BLOCKING    | BLOCKING
2      |  BLOCKING        | NON BLOCKING
3      |  NON BLOCKING    | NON BLOCKING


1) Debugfs is also used to reinit the src and sink interfaces . Hence enabling us to check the new , attach , detach , delete APIs for memory source and sink interfaces .
2) Since it is required that the buffer is allocated in user space , the buffer used to push or pull the data wil be same as the buffer which we have got through debugfs "cat" operation . The buffer size as result of "cat" operation is 32K which is 8 pages . Hence is fine for testing .This count is user configuration before dong push / pull using debugfs file .
3) Following  files are also created in debugfs: ( will be used completely in future )
in_size, out_size :  Buffer Size control
These files allow to read/write the pulled/pushed data selecting the size of the buffer.

In the same time 2 file are created in debugfs
in_fill , out_fill  : Will be used to change the content of the buffer to be pushed or pulled  later .

Example ::

root@hdk7108v1:/sys/kernel/debug/memsrsink_test# echo 10 > in_size
root@hdk7108v1:/sys/kernel/debug/memsrsink_test# cat data/push0

An other bash prompt (so an other process) can call
cat data/pull
and it will pull data in the out buffer (with the out_size).


--- Design / File layouts :

For test module , we have following internal components
   1) mss_test_consumer.c  : Act as STKPI component ( e.g : TE ) for memory src object .
   2) mss_test_producer.c  : Act as STKPI component for memory source object .
   3) mss_test_user_interface.c : This is the user interface ( or adaptation layer ) for both src and sink and is responsible for creating memory src and sink objects .

The convention or variable names used  in test module :
 For memory src interface testing : "consumer object " and "mem src object"
 For memory snk interface testing : "producer object " and "mem snk object"


TODO :
 1) Script for automation
 2) Currently "init" file does reinit for both push and pull . It should be  done separately in future .
 2) Add proper Debug message usage
 3) Add proper checks for error handling in Test driver .
 4) Right now since the data is always available , the push / pull is returned always and both modes blocking and non blockiong are behaving in same way . This should be enhanced and notification should be added .
 5) Buffer content can also by configured using debugfs.
