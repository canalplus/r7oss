#!/bin/sh

#install the dependent modules
modprobe stmfb
modprobe infra_test

# Copy this Script in the infrastructure folder on the target system
LOCATION=/sys/kernel/infrastructure/infra/test
e=$(printf "\e")
N="$e[0m"
G="$e[0;32m"
Y="$e[1;33m"
R="$e[0;31m"

TC_INFRA_FUNC_CEC_READ_BROADCAST()
{
	TC="TC_INFRA_FUNC_CEC_READ_BROADCAST"
	cec_entry_msg $TC
	cec_init
	MSG="Waiting for 5 sec for CEC message transactions"
	cec_test_msg $MSG
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_RECEIVE_BUFFER()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_RECEIVE_BUFFER"
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_READ_BROADCAST()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_READ_BROADCAST "
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_GET_CEC_EVT()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_GET_CEC_EVT"
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_SEND_CEC_MSG_BROADCAST()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_SEND_CEC_MSG_BROADCAST"
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_SEND_CEC_MSG_DIRECT()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_SEND_CEC_MSG_DIRECT"
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_ABORT()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_ABORT"
	cec_init
	cec_test_msg "Waiting for 1 sec and the abort"
	sleep 1
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_GET_READ_ERROR_EVT()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_GET_READ_ERROR_EVT"
	cec_init
	cec_test_msg "Waiting for 5 sec for CEC message transactions"
	sleep 5
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_SEND_MSG_TO_INVALID_DEV()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_SEND_MSG_TO_INVALID_DEV"
	cec_init
	cec_test_msg "Waiting for 1 sec for CEC message transactions"
	sleep 1
	cec_test_msg "Sending a cec message to an invalid dev addr 0x2"
	echo "CEC_SEND_MSG 0x3 0x2" > $LOCATION
	cec_test_msg "Sending should fail"
	sleep 1
	cec_term
	cec_exit_msg
}

TC_INFRA_FUNC_CEC_INSMOD_RMMOD()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_INSMOD_RMMOD"
	cec_init
	cec_test_msg "Waiting for 1 sec for CEC message transactions"
	sleep 1
	cec_term
	cec_test_msg "Removing CEC Module"
	modprobe -r cec_test
	cec_test_msg "Inserting CEC Module"
	modprobe cec_test
	cec_test_msg "Removing CEC Module"
	modprobe -r cec_test
	cec_test_msg "Inserting CEC Module"
	modprobe cec_test

	cec_exit_msg
}

TC_INFRA_FUNC_CEC_ABORT()
{
	cec_entry_msg "TC_INFRA_FUNC_CEC_ABORT"
	cec_init
	cec_test_msg "Waiting for 1 sec for CEC message transactions"
	sleep 1
	cec_test_msg "Sending a cec message to an invalid dev addr 0x2"
	echo "CEC_SEND_MSG 0x3 0x2" > $LOCATION &
	cec_test_msg "Trying to term while send message is blocked."
	cec_term

	cec_test_msg "Reinit to check if term was OK"
	cec_init
	cec_test_msg "Waiting for 1 sec for CEC message transactions"
	sleep 1
	cec_term

	cec_exit_msg
}

TC_INFRA_FUNC_CEC_PM_HPS()
{

	cec_entry_msg "TC_INFRA_FUNC_CEC_INSMOD_RMMOD"
	cec_test_msg "Removing stmfb modules"
	modprobe -r stmfb
	cec_test_msg "inserting only cec to avoid any dependency on stmfb"
	cec_init
	cec_test_msg "Waiting for 1 sec for CEC message transactions"
	sleep 1
	#echo "CEC_SEND_MSG 0x3 0x2" > $LOCATION &
	echo "Enabling Wakeup on CEC message"
	cec_dev=$(find /sys/devices/ -maxdepth 2 | grep cec)
	echo enabled > $cec_dev/power/wakeup
	cec_test_msg "Device in HPS Mode"
	cec_test_msg "Please switch off/on the TV "
	echo mem > /sys/power/state
	echo "Successfully Woken up by CEC message"
	sleep 2
	cec_term
	modprobe stmfb
	cec_exit_msg
}

cec_test_msg()
{
echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
echo "+	"$1
echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
echo ""
}

cec_entry_msg()
{
echo ""${G}
echo ">>>>>>>>>>>>> $1 started>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
echo ${N}""
}

cec_exit_msg()
{
echo ""${G}
echo "<<<<<<<<<<<<<< CEC Test Ended <<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
echo ${N}""
}


cec_init()
{
	echo "CEC_INIT" > $LOCATION
	echo "CEC_DO_LOGIC_SETUP" > $LOCATION
}

cec_term()
{
	echo "CEC_TERM" > $LOCATION
}

if
        ! lsmod | grep -q -s cec_test
then
        modprobe cec_test
fi


