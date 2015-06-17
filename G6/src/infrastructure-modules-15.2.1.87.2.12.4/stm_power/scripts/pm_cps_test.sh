#!/bin/sh

CPS_HELP()
{
	if [ "$1" == "-i" ]; then
		CPS_INTERACTIVE
		return
	fi

	echo "SYNOPSIS:"
	echo "			TESTCASE  [iterations] [optional AV Playback]"
	echo
	echo "For Example:"
	echo
	echo -e "${GREEN}	TC_CPS_LIVE_PLAYBACK_IR 10 -av ${NC}"
	echo
	echo "OPTIONAL AV PLAYBAACK:"
	echo "-av : This option will try to play live video at the end of every test case"
	echo
	echo -e "${RED}     NOTE : -av option WILL NOT WORK IN BARE KERNEL MODE ${NC}"
	echo
	echo "DIRECT RUN MODE:"
	echo "-o  : This followed by the option number will simply run one of the testcases below"
	echo
	echo "For Example:"
	echo
	echo -e "${GREEN}	$0 -o 13${NC} "
	echo
	echo "INTERACTIVE MODE:"
	echo "-i : Set interactive mode"
	echo
	echo "For Example:"
	echo -e "${GREEN}        $0 -i ${NC}"
	echo
	echo -e "${GREEN}These are the test cases for the CPS Test"
	echo " 1. TC_CPS_BARE_KERNEL_RTC"
	echo " 2. TC_CPS_BARE_KERNEL_IR"
	echo " 3. TC_CPS_BARE_KERNEL_ETHERNET"
	echo " 4. TC_CPS_BARE_KERNEL_ALL"
	echo " 5. TC_CPS_LOAD_UNLOAD_KERNEL_RTC"
	echo " 6. TC_CPS_LOAD_UNLOAD_KERNEL_IR"
	echo " 7. TC_CPS_LOAD_UNLOAD_KERNEL_ETHERNET"
	echo " 8. TC_CPS_LOAD_UNLOAD_KERNEL_ALL"
	echo " 9. TC_CPS_LIVE_PLAYBACK_RTC"
	echo "10. TC_CPS_LIVE_PLAYBACK_IR"
	echo "11. TC_CPS_LIVE_PLAYBACK_ETHERNET"
	echo "12. TC_CPS_LIVE_PLAYBACK_ALL"
	echo "13. CPS_PRELIMINARY_TEST"
	echo "14. TC_CPS_LIVE_PLAYBACK_OVERNIGHT"
	echo -e "15. TC_CPS_SLEEP_OVERNIGHT"
	echo -e "echo 16. TC_CPS_LIVE_PLAYBACK_CEC ${NC}"
	echo

}

run_cps()
{
	case $1 in
		1 )
		TC_CPS_BARE_KERNEL_RTC
		;;
		2 )
		TC_CPS_BARE_KERNEL_IR
		;;
		3 )
		TC_CPS_BARE_KERNEL_ETHERNET
		;;
		4 )
		TC_CPS_BARE_KERNEL_ALL
		;;
		5 )
		TC_CPS_LOAD_UNLOAD_KERNEL_RTC
		;;
		6 )
		TC_CPS_LOAD_UNLOAD_KERNEL_IR
		;;
		7 )
		TC_CPS_LOAD_UNLOAD_KERNEL_ETHERNET
		;;
		8 )
		TC_CPS_LOAD_UNLOAD_KERNEL_ALL
		;;
		9 )
		TC_CPS_LIVE_PLAYBACK_RTC
		;;
		10 )
		TC_CPS_LIVE_PLAYBACK_IR
		;;
		11 )
		TC_CPS_LIVE_PLAYBACK_ETHERNET
		;;
		12 )
		TC_CPS_LIVE_PLAYBACK_ALL
		;;
		13 )
		CPS_PRELIMINARY_TEST
		;;
		14 )
		TC_CPS_LIVE_PLAYBACK_OVERNIGHT
		;;
		15 )
		TC_CPS_SLEEP_OVERNIGHT
		;;
		16 )
		TC_CPS_LIVE_PLAYBACK_CEC
		;;
		* )
		echo -e "Invalid or No Option. Please Type${GREEN} $0 -h ${NC} for options."

		return
		;;
	esac

}



CPS_INTERACTIVE()
{
	echo "You have Entered the Interactive Mode..."
	echo
	echo "Do you want to Run: "
	echo
	echo "1. A Preliminary Test"
	echo "2. A Bare Kernel Test "
	echo "3. A Loaded and Unloaded Kernel Test"
	echo "4. A Live Playback Test"
	echo "5. An Overnight Zap Test"
	echo "6. An Overnight Sleep Test"
	echo "7. Exit"
	echo
	echo "Select one of the Options (1,2,3,4,5)"
	read OPTION

	case $OPTION in
        1 )
			CPS_PRELIMINARY_TEST
            ;;
        2 )
			BARE_KERNEL=1
			LOADED_AND_UNLOADED=0
			LIVE=0
			get_wakeup_mode
            ;;
        3 )
			BARE_KERNEL=0
			LOADED_AND_UNLOADED=1
			LIVE=0
			get_wakeup_mode
            ;;
		4 )
			BARE_KERNEL=0
			LOADED_AND_UNLOADED=0
			LIVE=1
			get_wakeup_mode
            ;;
		5)
			echo "How many times do you want to run this test ?"
			read overnight_iterations
			TC_CPS_LIVE_PLAYBACK_OVERNIGHT $overnight_iterations
			;;
		6)
			TC_CPS_SLEEP_OVERNIGHT
			;;
        * ) #DEFAULT
            return
    esac
}

get_wakeup_mode()
{
	echo "Which mode of wakeup do you want to set?"
	echo "1. RTC - CPU will wakeup by itself after a given time."
	echo "2. IR - CPU will be woken up by the IR Remote"
	echo "3. ETHERNET - CPU will be woken up through the LAN"
	echo "4. ALL - CPU will sleep and wake up sequentially by each of the above reasons"
	echo "5. Go to Previous Menu"
	echo "6. Exit"
	read WAKEUP_MODE
	case $WAKEUP_MODE in
        1 )
			echo "How many seconds do you want the device to sleep for?"
			read TIME
			mode=1
            ;;
        2 )
			mode=2
            ;;
        3 )
			mode=3
            ;;
	4 )
			mode=4
            ;;
	5 )
			CPS_INTERACTIVE
            ;;
        * ) #DEFAULT
            return
    esac

	if [ $BARE_KERNEL == 0 ]; then
		echo "Do you want to check for live Playback after the Test?"
		read choice
		if [ "$choice" == 'y' -o "$choice" == 'Y' ]; then
			PLAY_OPTION=1
		else
			PLAY_OPTION=0
		fi
	fi

	start_interactive_test
}

start_interactive_test()
{

if [ $BARE_KERNEL == 1 ]; then
    case $mode in
		1 )
		TC_CPS_BARE_KERNEL_RTC
		;;

		2 )
		TC_CPS_BARE_KERNEL_IR
		;;

		3 )
		TC_CPS_BARE_KERNEL_ETHERNET
		;;

		4 )
		TC_CPS_BARE_KERNEL_ALL
		;;
		5 )
		CPS_INTERACTIVE #Go Back to Previous Menu
            ;;
        * ) #DEFAULT
            return
    esac
fi

if [ $LOADED_AND_UNLOADED == 1 ]; then
    case $mode in
        1 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LOAD_UNLOAD_KERNEL_RTC 1 -av
		else
			TC_CPS_LOAD_UNLOAD_KERNEL_RTC
		fi
			;;

		2 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LOAD_UNLOAD_KERNEL_IR 1 -av
		else
			TC_CPS_LOAD_UNLOAD_KERNEL_IR
		fi
		    ;;

		3 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LOAD_UNLOAD_KERNEL_ETHERNET 1 -av
		else
			TC_CPS_LOAD_UNLOAD_KERNEL_ETHERNET
		fi
	        ;;

		4 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LOAD_UNLOAD_KERNEL_ALL 1 -av
		else
			TC_CPS_LOAD_UNLOAD_KERNEL_ALL
		fi
			;;
		5 )
			CPS_INTERACTIVE #Go Back to Previous Menu
            ;;
        * ) #DEFAULT
            return
    esac
fi

if [ $LIVE == 1 ]; then
    case $mode in
        1 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LIVE_PLAYBACK_RTC 1 -av
		else
			TC_CPS_LIVE_PLAYBACK_RTC
		fi
			;;

		2 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LIVE_PLAYBACK_IR 1 -av
		else
			TC_CPS_LIVE_PLAYBACK_IR
		fi
		    ;;

		3 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LIVE_PLAYBACK_ETHERNET 1 -av
		else
			TC_CPS_LIVE_PLAYBACK_ETHERNET
		fi
	        ;;

		4 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_CPS_LIVE_PLAYBACK_ALL 1 -av
		else
			TC_CPS_LIVE_PLAYBACK_ALL
		fi
			;;
		5 )
			CPS_INTERACTIVE #Go Back to Previous Menu
            ;;
        * ) #DEFAULT
            return
    esac
fi
}


TC_CPS_RTC()
{
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting RTC Test"
	echo -e "----------------------------------------------------------${NC}"
	echo +$1 > /sys/class/rtc/rtc1/wakealarm
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in CPS Mode"
	echo "Please Wait. The Device Should Automatically Wakeup in $1 seconds"
	echo "----------------------------------------------------------"
	echo hom > /sys/power/state
	echo
	sleep 3
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Timer...!!!"
	echo -e "----------------------------------------------------------${NC}"

}

TC_CPS_IR()
{

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting IR Test"
	echo "Enabling Wakeup on IR Mode..."
	echo -e "----------------------------------------------------------${NC}"
	modprobe stm_power_ir
	lirc=$(find /sys/devices/ -maxdepth 2 | grep lirc)
	echo enabled > $lirc/power/wakeup
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in Low Power Mode"
	echo "Please Point IR Remote to the STB and Press Power Button"
	echo -e "${RED}YOU MAY HAVE TO PRESS MORE THAN ONCE..."
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------${NC}"
	echo hom > /sys/power/state
	echo
	sleep 3
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by IR Signal"
	echo -e "----------------------------------------------------------${NC}"


}

TC_CPS_CEC()
{

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Enabling Wakeup on cec message"
	echo -e "----------------------------------------------------------${NC}"
	modprobe stm_cec
	cec_dev=$(find /sys/devices/ --maxdepth 2 | grep cec)
	echo enabled > $cec_dev/power/wakeup
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in Low Power Mode"
	echo "Please send a cec message to wake up the stb"
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------${NC}"
	echo hom > /sys/power/state
	echo
	sleep 3
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by cec message"
	echo -e "----------------------------------------------------------${NC}"


}

TC_CPS_ETHERNET()
{
	echo
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting Ethernet Test"
	echo "Enabling Wakeup on Ethernet Mode..."
	echo -e "----------------------------------------------------------${NC}"
	ethtool -s eth0 wol g
	echo
	echo ----------------------------------------------------------
	echo "The ipconfig Parameters are shown below:-"
	echo
	ifconfig
	echo ----------------------------------------------------------
	echo
	sleep 2
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in CPS Mode"
	echo "Please Wake up the Device using the Wake on Lan App"
	echo "In the Wake on Lan App:-"
	echo
	echo "1. Use the 'HWAddr' for the MAC Address"
	echo "2. Use the 'inet addr' for the Internet Address"
	echo "3. Use Subnet Mask as 255.255.255.255"
	echo
	echo "4. Now, send the Magic Packet"
	echo ----------------------------------------------------------
	echo hom > /sys/power/state
	echo
	sleep 3
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Ethernet"
	echo -e "----------------------------------------------------------${NC}"

}

cps_tests()
{
	echo --------------------TEST 1 : Wakeup on RTC----------------
	echo
	echo "How many seconds do you want the device to sleep for?"
	read TIME
	TC_CPS_RTC $TIME
	sleep 3
	echo --------------------TEST 2 : Wakeup on IR-----------------
	echo
	TC_CPS_IR
	sleep 3
	echo -----------------TEST 3: Wake up on Ethernet------------
	echo
	TC_CPS_ETHERNET
	sleep 2
}



load_and_unload_modules()
{
	echo ----------------------------------------------------------
	echo "Loading Modules"
	echo ----------------------------------------------------------
	sleep 2
	./framework_go.sh
	echo ----------------------------------------------------------
	echo "Unloading Modules"
	echo ----------------------------------------------------------
	sleep 2
	source module_rm.sh
}

AV_PLAY()
{
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Loading Modules"
	echo ----------------------------------------------------------
	sleep 2
	./framework_go.sh
	echo ----------------------------------------------------------
	echo "Playing Live DD_NATIONAL_S1_India"
	echo -e "${RED}Please check Playback${LIGHT_PURPLE}"
	echo ----------------------------------------------------------
	echo
	(sleep 10; echo 'q') | gst-apps "dvb://DD_NATIONAL_S1_India" --avsync-disable
	echo
	echo ----------------------------------------------------------
	echo "Unloading Modules"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2
	source module_rm.sh
}


CPS_PRELIMINARY_TEST()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Performing Preliminary Tests Before CPS Tests"
	echo -e "----------------------------------------------------------${NC}"
	echo
	echo "Test 1"
	echo -e "${RED}Please check Whether Modules are Loading and Unloading Properly${NC}"
	echo
	load_and_unload_modules
	sleep 2
	echo
	echo "Test 2"
	echo -e "${RED}Please check Whether the TV is showing Live Playback${NC}"
	sleep 2
	AV_PLAY
	sleep 2
	echo -e "${GREEN}----------------------------------------------------------"
	echo -e "Preliminary Tests Successful... ${Red}Now Please Reset the Board${GREEN}"
	echo -e "----------------------------------------------------------${NC}"
}

TC_CPS_BARE_KERNEL_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi


	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		sleep 3
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test on Bare Kernel"
		echo "Sleeping for $TIME seconds"
		echo -e "----------------------------------------------------------${NC}"
		sleep 3
		TC_CPS_RTC $TIME
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	TIME=20

}



TC_CPS_LOAD_UNLOAD_KERNEL_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi



	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Loading and Unloading Kernel"
		echo "Sleeping for $TIME seconds"
		echo -e "----------------------------------------------------------${NC}"
		load_and_unload_modules

		TC_CPS_RTC $TIME
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
	TIME=20
}


TC_CPS_LIVE_PLAYBACK_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi


	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo "Sleeping for $TIME seconds"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		TC_CPS_RTC $TIME
		echo ----------------------------------------------------------
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
	TIME=20
}




TC_CPS_BARE_KERNEL_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_CPS_IR
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

}

TC_CPS_BARE_KERNEL_CEC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_CPS_CEC
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

}


TC_CPS_LOAD_UNLOAD_KERNEL_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_and_unload_modules
		TC_CPS_IR
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
}



TC_CPS_LIVE_PLAYBACK_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		TC_CPS_IR
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
}

TC_CPS_LIVE_PLAYBACK_CEC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		TC_CPS_CEC
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
}




TC_CPS_BARE_KERNEL_ETHERNET()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_CPS_ETHERNET
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done
}



TC_CPS_LOAD_UNLOAD_KERNEL_ETHERNET()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_and_unload_modules
		TC_CPS_ETHERNET
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
}




TC_CPS_LIVE_PLAYBACK_ETHERNET()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		TC_CPS_ETHERNET
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAYN
	fi
}


TC_CPS_BARE_KERNEL_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		cps_tests
		echo
		echo ----------------------------------------------------------
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	TIME=20

}



TC_CPS_LOAD_UNLOAD_KERNEL_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_and_unload_modules
		cps_tests
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi

	TIME=20
}



TC_CPS_LIVE_PLAYBACK_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		cps_tests
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
	done
	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		AV_PLAY
	fi
	TIME=20
}

TC_CPS_SLEEP_OVERNIGHT()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	echo -e "How many hours do you want the device to sleep for?"
	read TIME_IN_HRS
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting RTC Overnight Sleep Test"
	echo -e "----------------------------------------------------------${NC}"
	echo +$[TIME_IN_HRS*3600]  > /sys/class/rtc/rtc1/wakealarm
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in CPS Mode"
	echo "The Device Should Wakeup in $TIME_IN_HRS Hours"
	echo -e "${RED}If it doesn't wakeup, Reset the board${LIGHT_PURPLE}"
	echo -e "----------------------------------------------------------${NC}"
	echo hom > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Timer...!!!"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2
	AV_PLAY
}

TC_CPS_LIVE_PLAYBACK_OVERNIGHT()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		CPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1000
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting CPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		echo "Sleeping for 10 seconds"
		TC_CPS_RTC 10

		AV_PLAY
		echo "Sleeping for 40 seconds"
		TC_CPS_RTC 40

		AV_PLAY
		echo "Sleeping for 100 seconds"
		TC_CPS_RTC 100

		echo -e "${YELLOW}Test Run $i Times..."
	done
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Overnight Test successful "
	echo -e "----------------------------------------------------------${NC}"
}


TIME=20
BARE_KERNEL=0
LOADED_AND_UNLOADED=0
LIVE=0
mode=0
TESTCASE=

#colour codes
GREEN='\e[0;32m'
RED='\e[0;31m'
BLACK='\e[0;30m'
DARK_GRAY='\e[1;30m'
BLUE='\e[0;34m'
LIGHT_BLUE='\e[1;34m'
LIGHT_GREEN='\e[1;32m'
CYAN='\e[0;36m'
LIGHT_CYAN='\e[1;36m'
LIGHT_RED='\e[1;31m'
PURPLE='\e[0;35m'
LIGHT_PURPLE='\e[1;35m'
BROWN='\e[0;33m'
YELLOW='\e[1;33m'
LIGHT_GRAY='\e[0;37m'
NC='\e[0m'

while getopts "hio:" OPT
do
	case $OPT in
		h)
			CPS_HELP
			;;
		i)
			CPS_HELP -i
			;;
		o)
			run_cps $OPTARG
			;;
		?)
			echo -e "Please type ${GREEN} $0 -h ${NC}for options"
			;;
	esac
done
