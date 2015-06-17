#!/bin/sh

HPS_HELP()
{
	if [ "$1" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	echo "SYNOPSIS:"
	echo "			TESTCASE  [iterations] [options]"
	echo
	echo "For Example:"
	echo
	echo -e "${GREEN}	TC_HPS_LOAD_KERNEL_IR 10 -av ${NC}"
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
	echo
	echo "    -i : Set interactive mode"
	echo
	echo -e "${GREEN}       $0 -i ${NC}"

	echo "These are the test cases for the HPS Test"
	echo " 1 TC_HPS_BARE_KERNEL_RTC"
	echo " 2 TC_HPS_BARE_KERNEL_IR"
	echo " 3 TC_HPS_BARE_KERNEL_ETHERNET"
	echo " 4 TC_HPS_BARE_KERNEL_UART"
	echo " 5 TC_HPS_BARE_KERNEL_ALL"
	echo " 6 TC_HPS_LOAD_KERNEL_RTC"
	echo " 7 TC_HPS_LOAD_KERNEL_IR"
	echo " 8 TC_HPS_LOAD_KERNEL_ETHERNET"
	echo " 9 TC_HPS_LOAD_KERNEL_UART"
	echo "10 TC_HPS_LOAD_KERNEL_ALL"
	echo "11 TC_HPS_LIVE_PLAYBACK_RTC"
	echo "12 TC_HPS_LIVE_PLAYBACK_IR"
	echo "13 TC_HPS_LIVE_PLAYBACK_ETHERNET"
	echo "14 TC_HPS_LIVE_PLAYBACK_UART"
	echo "15 TC_HPS_LIVE_PLAYBACK_ALL"
	echo "16 HPS_PRELIMINARY_TEST"
	echo "17 TC_HPS_LIVE_PLAYBACK_OVERNIGHT"
	echo "18 TC_HPS_SLEEP_OVERNIGHT"
	echo "19 TC_HPS_LIVE_PLAYBACK_CEC"
	echo "20 TC_HPS_FRAMEWORK_RTC"
	echo

}

run_hps()
{
	case $1 in
		1 )
		TC_HPS_BARE_KERNEL_RTC
		;;
		2 )
		TC_HPS_BARE_KERNEL_IR
		;;
		3 )
		TC_HPS_BARE_KERNEL_ETHERNET
		;;
		4 )
		TC_HPS_BARE_KERNEL_UART
		;;
		5 )
		TC_HPS_BARE_KERNEL_ALL
		;;
		6 )
		TC_HPS_LOAD_KERNEL_RTC
		;;
		7 )
		TC_HPS_LOAD_KERNEL_IR
		;;
		8 )
		TC_HPS_LOAD_KERNEL_ETHERNET
		;;
		9 )
		TC_HPS_LOAD_KERNEL_UART
		;;
		10 )
		TC_HPS_LOAD_KERNEL_ALL
		;;
		11 )
		TC_HPS_LIVE_PLAYBACK_RTC
		;;
		12 )
		TC_HPS_LIVE_PLAYBACK_IR
		;;
		13 )
		TC_HPS_LIVE_PLAYBACK_ETHERNET
		;;
		14 )
		TC_HPS_LIVE_PLAYBACK_UART
		;;
		15 )
		TC_HPS_LIVE_PLAYBACK_ALL
		;;
		16 )
		HPS_PRELIMINARY_TEST
		;;
		17 )
		TC_HPS_LIVE_PLAYBACK_OVERNIGHT
		;;
		18 )
		TC_HPS_SLEEP_OVERNIGHT
		;;
		19 )
		TC_HPS_LIVE_PLAYBACK_CEC
		;;
		20 )
		TC_HPS_FRAMEWORK_RTC
		;;
		* )
		echo -e "Invalid or No Option. Please Type${GREEN} $0 -h ${NC} for options."

		return
		;;
	esac

}

HPS_INTERACTIVE()
{
	echo "You have Entered the Interactive Mode..."
	echo
	echo "Do you want to Run: "
	echo
	echo "1. A Preliminary Test"
	echo "2. A Bare Kernel Test "
	echo "3. A Loaded Kernel Test"
	echo "4. A Live Playback Test"
	echo "5. An Overnight Zap Test"
	echo "6. An Overnight Sleep Test"
	echo "7. Exit"
	echo
	echo "Select one of the Options (1,2,3,4,5)"
	read OPTION

	case $OPTION in
        1 )
			HPS_PRELIMINARY_TEST
            ;;
        2 )
			BARE_KERNEL=1
			LOADED=0
			LIVE=0
			get_wakeup_mode
            ;;
        3 )
			BARE_KERNEL=0
			LOADED=1
			LIVE=0
			get_wakeup_mode
            ;;
	4 )
			BARE_KERNEL=0
			LOADED=0
			LIVE=1
			get_wakeup_mode
            ;;
	5)
			echo "How many times do you want to run this test ?"
			read overnight_iterations
			TC_HPS_LIVE_PLAYBACK_OVERNIGHT $overnight_iterations
			;;
	6)
			TC_HPS_SLEEP_OVERNIGHT
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
	echo "4. UART - CPU will be woken up through the UART"
	echo "5. ALL - CPU will sleep and wake up sequentially by each of the above reasons"
	echo "6. Go to Previous Menu"
	echo "7. Exit"
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
			mode=5
            ;;
	6 )
			HPS_INTERACTIVE
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
		TC_HPS_BARE_KERNEL_RTC

		;;

		2 )
		TC_HPS_BARE_KERNEL_IR

		;;

		3 )
		TC_HPS_BARE_KERNEL_ETHERNET

	        ;;

		4 )
		TC_HPS_BARE_KERNEL_UART

	        ;;

		5 )
		TC_HPS_BARE_KERNEL_ALL

		;;

		6 )
		HPS_INTERACTIVE #Go Back to Previous Menu

		;;
        * ) #DEFAULT
            return
    esac
fi

if [ $LOADED == 1 ]; then
    case $mode in
        1 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LOAD_KERNEL_RTC 1 -av
		else
			TC_HPS_LOAD_KERNEL_RTC
		fi
			;;

		2 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LOAD_KERNEL_IR 1 -av
		else
			TC_HPS_LOAD_KERNEL_IR
		fi
		    ;;

		3 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LOAD_ETHERNET 1 -av
		else
			TC_HPS_LOAD_KERNEL_ETHERNET
		fi
	        ;;
		4 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LOAD_KERNEL_UART 1 -av
		else
			TC_HPS_LOAD_KERNEL_UART
		fi
	        ;;

		5 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LOAD_KERNEL_ALL 1 -av
		else
			TC_HPS_LOAD_KERNEL_ALL
		fi
			;;
		6 )
			HPS_INTERACTIVE #Go Back to Previous Menu
            ;;
        * ) #DEFAULT
            return
    esac
fi

if [ $LIVE == 1 ]; then
    case $mode in
        1 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LIVE_PLAYBACK_RTC 1 -av
		else
			TC_HPS_LIVE_PLAYBACK_RTC
		fi
			;;

		2 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LIVE_PLAYBACK_IR 1 -av
		else
			TC_HPS_LIVE_PLAYBACK_IR
		fi
		    ;;

		3 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LIVE_PLAYBACK_ETHERNET 1 -av
		else
			TC_HPS_LIVE_PLAYBACK_ETHERNET
		fi
	        ;;
		4 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LIVE_PLAYBACK_UART 1 -av
		else
			TC_HPS_LIVE_PLAYBACK_UART
		fi
	        ;;

		5 )
		if [ $PLAY_OPTION == 1 ]; then
			TC_HPS_LIVE_PLAYBACK_ALL 1 -av
		else
			TC_HPS_LIVE_PLAYBACK_ALL
		fi
			;;
		6 )
			HPS_INTERACTIVE #Go Back to Previous Menu
            ;;
        * ) #DEFAULT
            return
    esac
fi
}

TC_HPS_RTC()
{

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting RTC Test"
	echo -e "----------------------------------------------------------${NC}"
	echo +$1 > /sys/class/rtc/rtc0/wakealarm
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in HPS Mode"
	echo "Please Wait. The Device Should Automatically Wakeup"
	echo "If it doesn't, Go to the UART Window and Press any key"
	echo -e "${RED}If it stll doesn't wakeup, Reset the board${LIGHT_PURPLE}"
	echo -e "----------------------------------------------------------${NC}"
	echo mem > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Timer...!!!"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2

}

TC_HPS_IR()
{
	modprobe stm_power_hps_ir

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting IR Test"
	echo "Enabling Wakeup on IR Mode..."
	echo -e "----------------------------------------------------------${NC}"
	lirc=$(find /sys/devices/ -maxdepth 2 | grep lirc)
	echo enabled > $lirc/power/wakeup
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in HPS Mode"
	echo "Please Point IR Remote to the STB and Press Power Button"
	echo -e "${RED}YOU MAY HAVE TO PRESS MORE THAN ONCE...${NC}"
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------${NC}"
	echo mem > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by IR Signal"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2

}

TC_HPS_CEC()
{

	modprobe stm_cec

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Enabling Wakeup on CEC message"
	echo -e "----------------------------------------------------------${NC}"
	cec_dev=$(find /sys/devices/ -maxdepth 2 | grep cec)
	echo enabled > $cec_dev/power/wakeup
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in HPS Mode"
	echo "Please switch off/on the TV "
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------${NC}"
	echo mem > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by CEC message"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2

}

TC_HPS_ETHERNET()
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
	echo "Device in HPS Mode"
	echo "Please Wake up the Device using the Wake on Lan App"
	echo "In the Wake on Lan App:-"
	echo
	echo "1. Use the 'HWAddr' for the MAC Address"
	echo "2. Use the 'inet addr' for the Internet Address"
	echo "3. Use Subnet Mask as 255.255.255.255"
	echo
	echo "4. Now, send the Magic Packet"
	echo ----------------------------------------------------------
	echo mem > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Ethernet"
	echo -e "----------------------------------------------------------${NC}"

}

TC_HPS_UART()
{
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting UART Test"
	echo "Enabling Wakeup by UART Mode..."
	echo -e "----------------------------------------------------------${NC}"

	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in HPS Mode"
	echo "Please Go to the UART Window and Press any key"
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------${NC}"
	echo mem > /sys/power/state

	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by UART"
	echo -e "----------------------------------------------------------${NC}"
}


hps_tests()
{
	echo --------------------TEST 1 : Wakeup on RTC----------------
	echo
	TC_HPS_RTC $TIME
	sleep 3
	echo --------------------TEST 2 : Wakeup on IR-----------------
	echo
	TC_HPS_IR
	sleep 3
	echo --------------------TEST 3 : Wakeup on UART-----------------
	echo
	TC_HPS_UART
	sleep 3
	echo -----------------TEST 4: Wake up on Ethernet------------
	echo
	TC_HPS_ETHERNET
	sleep 2
}



load_modules()
{
	echo ----------------------------------------------------------
	echo "Loading Modules"
	echo ----------------------------------------------------------
	sleep 2
	./framework_go.sh
}

unload_modules()
{
	echo ----------------------------------------------------------
	echo "Unloading Modules"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2
	source module_rm.sh
}
AV_PLAY()
{
	echo ----------------------------------------------------------
	echo "Playing Live DD_NATIONAL_S1_India"
	echo -e "${RED}Please check Playback${LIGHT_PURPLE}"
	echo ----------------------------------------------------------
	echo
	(sleep 10; echo 'q') | gst-apps "dvb://DD_NATIONAL_S1_India" --avsync-disable
	echo

}


 HPS_PRELIMINARY_TEST()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	echo -e "${CYAN}----------------------------------------------------------"
	echo "Performing Preliminary Tests Before  HPS Tests"
	echo -e "----------------------------------------------------------${NC}"
	echo
	echo "Test 1"
	echo -e "${RED}Please check Whether Modules are Loading and Unloading Properly${NC}"
	echo
	load_modules
	sleep 2
	unload_modules
	sleep 2
	echo
	echo "Test 2"
	echo -e "${RED}Please check Whether the TV is showing Live Playback${NC}"
	sleep 2
	load_modules
	sleep 2
	AV_PLAY
	sleep 2
	unload_modules
	echo -e "${GREEN}----------------------------------------------------------"
	echo -e "Preliminary Tests Successful... ${Red}Now Please Reset the Board${GREEN}"
	echo -e "----------------------------------------------------------${NC}"
}

TC_HPS_BARE_KERNEL_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_HPS_RTC $TIME
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

}



TC_HPS_LOAD_KERNEL_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))

	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		TC_HPS_RTC $TIME
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		load_modules
		AV_PLAY
		unload_modules
	fi
}



TC_HPS_LIVE_PLAYBACK_RTC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		TC_HPS_RTC $TIME
		echo ----------------------------------------------------------
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi
}



TC_HPS_BARE_KERNEL_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_HPS_IR
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

}



TC_HPS_LOAD_KERNEL_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		TC_HPS_IR
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi

}

TC_HPS_LOAD_KERNEL_CEC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		TC_HPS_CEC
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi

}


TC_HPS_LIVE_PLAYBACK_IR()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		TC_HPS_IR
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		load_modules
		AV_PLAY
		unload_modules
	fi

}

TC_HPS_LIVE_PLAYBACK_CEC()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		TC_HPS_CEC
		AV_PLAY
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		load_modules
		AV_PLAY
		unload_modules
	fi

}


TC_HPS_BARE_KERNEL_ETHERNET()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_HPS_ETHERNET
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done


}


TC_HPS_LOAD_KERNEL_ETHERNET()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi


	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		TC_HPS_ETHERNET
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi
}


TC_HPS_LIVE_PLAYBACK_ETHERNET()
{

	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		TC_HPS_ETHERNET
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi
}


TC_HPS_BARE_KERNEL_UART()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ] ; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		TC_HPS_UART
		echo
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done


}

TC_HPS_LOAD_KERNEL_UART()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi
	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		TC_HPS_UART
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi

}

TC_HPS_LIVE_PLAYBACK_UART()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi
	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		TC_HPS_UART
		echo ----------------------------------------------------------
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		AV_PLAY
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
		load_modules
		AV_PLAY
		unload_modules
	fi
}


TC_HPS_BARE_KERNEL_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" -o "$2" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi
	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		hps_tests
		echo
		echo ----------------------------------------------------------
		echo "Wakeup successful on Bare Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done



}

TC_HPS_LOAD_KERNEL_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi
	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		hps_tests
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Loading and Unloading Kernel"
		echo -e "----------------------------------------------------------${NC}"
		echo
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi

}

TC_HPS_LIVE_PLAYBACK_ALL()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1
	else
		ITERATIONS=$1
	fi
	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting  HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		hps_tests
		echo -e "${GREEN}----------------------------------------------------------"
		echo "Wakeup successful after Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		unload_modules
	done

	if [ "$2" == '-av' -o "$1" == '-av' ]; then
        load_modules
		AV_PLAY
		unload_modules
	fi

}

TC_HPS_LIVE_PLAYBACK_OVERNIGHT()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi


	if [ "$1" == "" -o "$1" == "-av" ]; then
		ITERATIONS=1000
	else
		ITERATIONS=$1
	fi

	for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
	do
		echo -e "${CYAN}----------------------------------------------------------"
		echo "Starting HPS Test by Playing Live TV"
		echo -e "----------------------------------------------------------${NC}"
		load_modules
		AV_PLAY
		echo "Sleeping for 10 seconds"
		TC_HPS_RTC 10

		AV_PLAY
		echo "Sleeping for 40 seconds"
		TC_HPS_RTC 40

		AV_PLAY
		echo "Sleeping for 100 seconds"
		TC_HPS_RTC 100

		AV_PLAY

		unload_modules

		echo -e "${YELLOW}Test Run $i Times..."
	done
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Overnight Test successful "
	echo -e "----------------------------------------------------------${NC}"

}

TC_HPS_SLEEP_OVERNIGHT()
{
	if [ "$1" == "-i" -o "$2" == "-i" ]; then
		HPS_INTERACTIVE
	fi

	echo -e "How many hours do you want the device to sleep for?"
	read TIME_IN_HRS
	echo -e "${CYAN}----------------------------------------------------------"
	echo "Starting RTC Overnight Sleep Test"
	echo -e "----------------------------------------------------------${NC}"
	echo +$[TIME_IN_HRS*3600]  > /sys/class/rtc/rtc0/wakealarm
	echo
	echo -e "${LIGHT_PURPLE}----------------------------------------------------------"
	echo "Device in HPS Mode"
	echo "The Device Should Wakeup in $TIME_IN_HRS Hours"
	echo -e "${RED}If it doesn't wakeup, Reset the board${LIGHT_PURPLE}"
	echo "----------------------------------------------------------${NC}"
	echo mem > /sys/power/state
	echo
	echo -e "${GREEN}----------------------------------------------------------"
	echo "Successfully Woken up by Timer...!!!"
	echo -e "----------------------------------------------------------${NC}"
	sleep 2
	load_modules
	AV_PLAY
}

TC_HPS_LOAD_KERNEL_RTC_INC()
{
TIME=10
for ((  i = 0 ;  i <= 2;  i=i+1  ))
do
        TIME=$[TIME+5]
        echo -e "${CYAN}----------------------------------------------------------"
        echo "Starting HPS Test by Loading and Unloading Kernel"
        echo -e "----------------------------------------------------------${NC}"
        load_modules
        echo
        echo
        echo " " >> clocklog.txt
        echo ---------------------------------------------------------- >> clocklog.txt
        echo "Sleeping for $TIME" >> clocklog.txt
        echo ---------------------------------------------------------- >> clocklog.txt
        echo " " >> clocklog.txt
        cat /proc/clocks >> clocklog.txt
        cat /proc/clocks
        echo
        echo
        echo +$TIME > /sys/class/rtc/rtc0/wakealarm
        echo -e "${YELLOW}----------------------------------------------------------"
        echo "Sleeping for "$TIME" seconds"
        echo -e "----------------------------------------------------------${NC}"
        echo mem > /sys/power/state

        echo -e "${GREEN}----------------------------------------------------------"
        echo "All Test Cases successful after Loading and Unloading Kernel"
        echo -e "----------------------------------------------------------"
        echo
        done

for ((  j = 0 ;  j <= 2;  j=j+1  ))
do
        echo -e "${YELLOW}Sleeping for $TIME"
        echo
        echo +$TIME > /sys/class/rtc/rtc0/wakealarm
        echo mem > /sys/power/state
        echo " " >> clocklog.txt
        echo -e "${CYAN}----------------------------------------------------------" >> clocklog.txt
        echo "Sleeping for $TIME" >> clocklog.txt
        echo -e "----------------------------------------------------------${NC}" >> clocklog.txt
        echo " " >> clocklog.txt
        cat /proc/clocks >> clocklog.txt
        echo
        echo -e "${GREEN}Good Morning after $TIME ${NC}"
done

}

TC_HPS_FRAMEWORK_RTC()
{
        if [ "$1" == "-i" -o "$2" == "-i" ]; then
                HPS_INTERACTIVE
        fi

        if [ "$1" == "" -o "$1" == "-av" ]; then
                ITERATIONS=1
        else
                ITERATIONS=$1
        fi

        for ((  i = 1 ;  i <= $ITERATIONS;  i=i+1  ))
        do
                TC_HPS_RTC 20
                echo ----------------------------------------------------------
                echo "Wakeup successful (Should have Taken 20 Seconds!!)"
                echo -e "----------------------------------------------------------${NC}"
                unload_modules
        done

}

BARE_KERNEL=0
LOADED=0
LIVE=0
mode=0
TIME=20
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
			HPS_HELP
			;;
		i)
			HPS_HELP -i
			;;
		o)
			run_hps $OPTARG
			;;
		?)
			echo -e "Please type ${GREEN} $0 -h ${NC}for options"
			;;
	esac
done
