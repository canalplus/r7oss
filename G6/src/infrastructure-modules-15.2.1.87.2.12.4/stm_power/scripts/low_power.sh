#!/bin/sh

if
    ! lsmod | grep -q -s stm_power
then
    modprobe stm_power
fi

#colour codes
GREEN='\e[0;32m'
RED='\e[0;31m'
YELLOW='\e[1;33m'
NC='\e[0m'

#deafult values
soc_file="h415"
profile_info="Idle State"
profile_name="idle_state"
action_info="Enable"
action_name="enabled"

handle_fe()
{
    device_name=$1
    action=$2
    soc_file=$3

    if [ "$action" == "disabled" ] ; then
           pass="suspended"
	   fail="active"
    elif [ "$action" == "enabled" ] ; then
           pass="active"
           fail="suspended"
    fi

    if [ "$soc_file" == "h407" ] ; then
	  path="soc.3"
    else
	  path="soc.2"
    fi

    echo $path
    cat /proc/clocks > clk_prev.txt
    if [ "$action" == "disabled" ] ; then
	find /sys/devices/$path/ -maxdepth 1 | grep fe_stv | xargs --replace=arg find arg -maxdepth 1 | grep stm-fe | xargs --replace=var  sh -c "echo disabled > var/power/runtime_status"
    else
	find /sys/devices/$path/ -maxdepth 1 | grep fe_stv | xargs --replace=arg find arg -maxdepth 1 | grep stm-fe | xargs --replace=var  sh -c "echo enabled > var/power/runtime_status"
    fi

    cat /proc/clocks > clk_new.txt
    find /sys/devices/$path/ -maxdepth 1 | grep fe_stv | xargs --replace=arg find arg -maxdepth 1 | grep stm-fe >> dev.txt
    cat "dev.txt" | (while read device_name ; do
                ls $device_name &> /dev/null
                err=$?
                if [ "$err" == 0 ] ; then
                        get_device_status $device_name $pass $fail
                fi
                done)
    rm -f "dev.txt"


}

handling_special_devices()
{
    device_name=$1
    action=$2
    soc_file=$3

	case $device_name in
        stm_fe)
                handle_fe $device_name $action $soc_file
                ;;
        *)
                echo -e "${RED}Invalid Input: "$device_name "${NC}"
                ;;
        esac

}

get_device_pm_status()
{
    device_name=$1
    fail_status=$3
    pass_status=$2
    ls $1 &> /dev/null
    err=$?
    if [ "$err" == 0 ] ; then
        status=$(cat $device_name/power/runtime_status)
        if [ "$status"  == "$fail_status" ] ; then
                echo -e "${RED}[Error] "$device_name "is still " $status"${NC}"
        elif [ "$status" == "unsupported" ] ; then
                echo -e "${YELLOW}"$device_name "is " $status"${NC}"
        else
                echo -e "${GREEN}"$device_name "is " $status"${NC}"
        fi
    fi
}

get_device_clk_status()
{
	clk_diff=$(diff -a --suppress-common-lines -y clk_prev.txt clk_new.txt | wc -l)
	if [ "$clk_diff" == 0 ] ; then
		echo -e "${YELLOW}""No difference in clock status""${NC}"
	else
		echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
		echo "          Clocks Before                                                       Clocks Now"
		echo "---------------------------------------------------------------------------------------"
		echo -e "${GREEN}"
		diff -a --suppress-common-lines -y clk_prev.txt clk_new.txt
		echo -e "{$NC}"
		echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
		echo ""
	fi
}

get_device_status()
{
	echo ""
	echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>DEVICE STATUS>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	get_device_pm_status $1 $2 $3
	get_device_clk_status
	sleep 1
	echo ""
	echo "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<STATUS END<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
	echo ""
}

enable_device_clocks()
{
	fail_status=suspended
	pass_status=active
	DEVICE_LIST=$1
	tac $DEVICE_LIST | (while read device_name ; do
		ls $device_name &> /dev/null
		err=$?
		if [ "$err" == 0 ] ; then
			cat /proc/clocks > clk_prev.txt
			echo $action_name>$device_name/power/runtime_status
			cat /proc/clocks > clk_new.txt
			get_device_status $device_name $pass_status $fail_status
		else
			handling_special_devices $device_name $action_name $soc_file
		fi
		done)
}

disable_device_clocks()
{
	fail_status=active
	pass_status=suspended

	DEVICE_LIST=$1
	cat $DEVICE_LIST | (while read device_name ; do
		ls $device_name &> /dev/null
		err=$?
		if [ "$err" == 0 ] ; then
			cat /proc/clocks > clk_prev.txt
			echo $action_name>$device_name/power/runtime_status
			cat /proc/clocks > clk_new.txt
			get_device_status $device_name $pass_status $fail_status
                else
                        handling_special_devices $device_name $action_name $soc_file
		fi
		done)
}


select_soc()
{
	echo "1. H415"
	echo "2. H416"
	echo "3. H315"
	echo "4. H407"
	echo -n "Select Soc: "
	read soc
	echo ""
	case $soc in
	1)
		soc_file="h415"
		;;
	2)
		soc_file="h416"
		;;
	3)
		soc_file="h315"
		;;
	4)
		soc_file="h407"
		;;
	*)
		echo "Invalid Input"
		;;
	esac

	echo "Soc selected: "$soc_file
	echo ""
}


select_profile()
{
	echo 1. Gaming Standby Mode
	echo 2. Background Record Mode
	echo 3. Network Standby Mode
	echo 4. IP Background Mode
	echo 5. Idle State
	echo 6. EPG Update
	echo 7. BASIC HD
	echo 8. BASIC SD
	echo 9. DUAL Decode

	echo -n "Please select one of the above profile :"
	read profile_option
	case $profile_option in
	1)
		profile_info="Gaming Standby Mode"
		profile_name="gaming_standby_mode"
		;;
	2)
		profile_info="Background Record Mode"
		profile_name="background_record_mode"
		;;
	3)
		profile_info="Network Standby Mode"
		profile_name="network_standby_mode"
		;;
	4)
		profile_info="IP Background Mode"
		profile_name="ip_background_mode"
		;;
	5)
		profile_info="Idle State"
		profile_name="idle_state"
		;;
	6)
		profile_info="EPG Update"
		profile_name="epg_update"
		;;
	7)
		profile_info="BASIC HD"
		profile_name="basic_hd"
		;;
	8)
		profile_info="BASIC SD"
		profile_name="basic_sd"
		;;
	9)
		profile_info="DUAL Decode"
		profile_name="dual_decode"
		;;
	*)
		echo "******ERROR: WRONG ARGUMENT ************"
		;;
	esac

}

select_action()
{
	echo "What do you want to do?"
	echo -n "Press <e> to ENABLE or <d> to DISABLE the profile :"
	read action_option
	if [ "$action_option" == "d" ] ; then
	   action_info="Disable"
	   action_name="disabled"
	elif [ "$action_option" == "e" ] ; then
	   action_info="Enable"
	   action_name="enabled"
	else
	  echo "******ERROR: WRONG ARGUMENT ************"
	  exit
	fi
}

select_soc
select_profile
select_action
echo Selected Profile: $profile_info
echo Selected Action : $action_info

DEVICE_LIST=./"$profile_name"_"$soc_file".txt
echo "Referring file "$DEVICE_LIST "for "$profile_info

if [ "$action_option" == "e" ] ; then
	enable_device_clocks $DEVICE_LIST
else
	disable_device_clocks $DEVICE_LIST
fi
