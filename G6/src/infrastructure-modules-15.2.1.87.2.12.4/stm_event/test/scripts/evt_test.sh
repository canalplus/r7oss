############ These are the test cases fro the EVENT MANAGER KPI ############################

EVT_HELP()
{
	echo "These are the Test Cases for Event Manager"
	echo
	echo " 1. TC_INFRA_API_EVT_INIT_SUBSCRIPTION"
	echo " 2. TC_INFRA_API_EVT_DEL_SUBSCRIPTION"
	echo " 3. TC_INFRA_API_EVT_MOD_SUBSCRIPTION"
	echo " 4. TC_INFRA_API_EVT_SET_INTERFACE"
	echo " 5. TC_INFRA_API_EVT_SIGNAL"
	echo " 6. TC_INFRA_API_EVT_WAIT"
	echo " 7. TC_INFRA_FUNC_EVT_GENERATE_CONTINOUS"
	echo " 8. TC_INFRA_FUNC_EVT_CHECK_LOSS_OF_EVT"
	echo " 9. TC_INFRA_FUNC_EVT_WAIT_INFINITY"
	echo "10. TC_INFRA_FUNC_EVT_DEL_SUBSCRIPTION_WHEN_ON_WAIT"
	echo "11. TC_INFRA_FUNC_EVT_GENERATE_EVT_SIMULTANEOUS"
	echo "12. TC_INFRA_FUNC_SIGNAL_EVT_FROM_INTERRUPT"
	echo "13. TC_INFRA_API_EVT_ADD_DEL_SUBSCRIPTION"
	echo
	echo "For Testing All Cases, Type:"
	echo
	echo "		source evt_test.sh --testall"
}



TC_INFRA_API_EVT_INIT_SUBSCRIPTION()
{
	echo RUNNING TESTCASE TC_INFRA_API_EVT_INIT_SUBSCRIPTION
	echo ----------------------------------------------------------
	echo "               INITIALIZING SIGNAL                      "
	echo ----------------------------------------------------------
	sleep 2
	sig_init 2
	echo ----------------------------------------------------------
	echo "              INITIALIZING SUBSCRIPTION                 " 
	echo ----------------------------------------------------------
	sleep 2
	sub_init 1
	echo ----------------------------------------------------------
	echo "               CREATING SUBSCRIPTION                    "
	echo ----------------------------------------------------------
	sleep 2
	sub_create 1
	echo ----------------------------------------------------------
	echo "        SUBSCRIPTION CREATION SUCCESSFUL...             "
	echo ----------------------------------------------------------
	sub_del 1

	sig_term 2
	sub_term 1

}

TC_INFRA_API_EVT_ADD_DEL_SUBSCRIPTION()
{
	echo RUNNING TESTCASE TC_INFRA_API_EVT_ADD_DEL_SUBSCRIPTION
	sig_init 2
	sig_init 3
	sub_init 1
	echo "dump before Test case starts                            "
	cat $DUMP_SIGNALER
	cat $DUMP_SUBSCRIBER
	echo ----------------------------------------------------------
	echo "               CREATING SUBSCRIPTION 1                  "
	echo ----------------------------------------------------------
	sub_create 1
	sig_init 1
	sub_init 3
	echo ----------------------------------------------------------
	echo "               CREATING SUBSCRIPTION 3                  "
	echo ----------------------------------------------------------
	sub_create 3
	cat $DUMP_SIGNALER
	cat $DUMP_SUBSCRIBER
	echo ----------------------------------------------------------
	echo "               DELETING SUBSCRIPTION 1                  "
	echo ----------------------------------------------------------
	sleep 2
	sub_del 1
	echo ----------------------------------------------------------
	echo "               DELETING SUBSCRIPTION 3                  "
	echo ----------------------------------------------------------
	sleep 2
	sub_del 3
	echo "dump after Test case ends                               "
	cat $DUMP_SIGNALER
	cat $DUMP_SUBSCRIBER
	sig_term 3
	sig_term 2
	sig_term 1
	sub_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "        SUBSCRIPTION DELETION SUCCESSFUL...             "
	echo ----------------------------------------------------------
	echo "check dumps before and after test case are same         "
}

TC_INFRA_API_EVT_DEL_SUBSCRIPTION()
{

	echo RUNNING TESTCASE TC_INFRA_API_EVT_DEL_SUBSCRIPTION
	sig_init 2
	sig_init 3
	sub_init 1

	sub_create 1
	
	echo ----------------------------------------------------------
	echo "               DELETING SUBSCRIPTION                    "        
	echo ----------------------------------------------------------
	sleep 2
	sub_del 1


	sig_term 3
	sig_term 2
	sub_term 1
	echo ----------------------------------------------------------
	echo "        SUBSCRIPTION DELETION SUCCESSFUL...             "
	echo ----------------------------------------------------------

}


TC_INFRA_API_EVT_MOD_SUBSCRIPTION()
{

	echo RUNNING TESTCASE TC_INFRA_API_EVT_MOD_SUBSCRIPTION
	sig_init 2
	sig_init 3
	sub_init 1

	sub_create 1
	
	echo ----------------------------------------------------------
	echo "               MODIFYING SUBSCRIPTION                   "       
	echo ----------------------------------------------------------
	sleep 2
	sub_mod 1

	sub_del 1

	sig_term 3
	sig_term 2
	sub_term 1
	echo ----------------------------------------------------------
	echo "         SUBSCRIPTION MODIFICATION SUCCESSFUL...        "
	echo ----------------------------------------------------------

}


TC_INFRA_API_EVT_SET_INTERFACE()
{

	echo RUNNING TESTCASE TC_INFRA_API_EVT_SET_INTERFACE
	sig_init 2
	sig_init 3
	sub_init 1

	sub_create 1
	echo ----------------------------------------------------------
	echo "               SETTING INTERFACE                        "       
	echo ----------------------------------------------------------
	sleep 3
	sub_inter 1

	sub_del 1

	sig_term 3
	sig_term 2
	sub_term 1
	echo ----------------------------------------------------------
	echo "        INTERFACE CREATION SUCCESSFUL...                "
	echo ----------------------------------------------------------
}


TC_INFRA_API_EVT_SIGNAL()
{

	echo RUNNING TESTCASE TC_INFRA_API_EVT_SIGNAL
	sig_init 1
	sub_init 3

	sub_create 3
	sub_inter 3
	sub_start 3
	echo ----------------------------------------------------------
	echo "               GENERATING SIGNAL                        "    
	echo ----------------------------------------------------------
	generate_signal 1 &
	sub_get 3
	
	
	
	sub_stop 3
	sig_stop 1
	


	sub_del 3

	sig_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "        SIGNAL GENERATION SUCCESSFUL...                 "
	echo ----------------------------------------------------------
	
}


TC_INFRA_API_EVT_WAIT()
{

	echo RUNNING TESTCASE TC_INFRA_API_EVT_WAIT
	sig_init 1
	sub_init 3

	sub_create 3
	sub_inter 3

	sub_start 3

	echo ----------------------------------------------------------
	echo "               WAITING FOR SIGNAL...                    "
	echo ----------------------------------------------------------
	generate_signal 1 &
	sub_get 3
    	
	
	
	sub_stop 3
	sig_stop 1
	

	sub_del 3

	sig_term 1
	sub_term 3
	sleep 3
	echo ----------------------------------------------------------
	echo "              EVENT WAIT SUCCESSFUL...                  "
	echo ----------------------------------------------------------
	
}


TC_INFRA_FUNC_EVT_GENERATE_CONTINOUS()
{

	echo RUNNING TESTCASE TC_INFRA_FUNC_EVT_GENERATE_CONTINOUS
	sub_init 2
	sig_init 1
	sig_init 2

	sub_create 2
	sub_inter 2

	sig_start 1 &
	sig_start 2 &
	sleep 5
	echo ----------------------------------------------------------
	echo "         GENERATING CONTINUOUS EVENT                    "      
	echo ----------------------------------------------------------
	sub_start 2
	sub_run 2
	sleep 3
	
	sub_stop 2
	sig_stop 1
	sig_stop 2
	
	sig_term 1
	sig_term 2
	sub_term 2
	echo ----------------------------------------------------------
	echo "     CONTINUOUS EVENT GENERATION SUCCESSFUL...          "
	echo ----------------------------------------------------------
}


TC_INFRA_FUNC_EVT_CHECK_LOSS_OF_EVT()
{

	echo RUNNING TESTCASE TC_INFRA_FUNC_EVT_CHECK_LOSS_OF_EVT
	sub_init 3
	sig_init 1

	sub_create 3
	sub_inter 3

	sub_start 3
	echo ----------------------------------------------------------
	echo "             GENERATING EVENTS                          "
	echo ----------------------------------------------------------
    	sig_start 1 &
	echo ----------------------------------------------------------
	echo "             WAITING FOR EVENT                          "
	echo ----------------------------------------------------------
	sleep 5		
	sub_get 3
	
	sub_stop 3
	sig_stop 1
	
	sub_del 3

	sig_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "           LOSS OF EVENT DETECTED                       "
	echo "            TEST CASE SUCCESSFUL...                     "
	echo ----------------------------------------------------------

}


TC_INFRA_FUNC_EVT_WAIT_INFINITY()
{

	echo RUNNING TESTCASE TC_INFRA_FUNC_EVT_WAIT_INFINITY

	sig_init 1
	sub_init 3

	sub_create 3
	sub_inter 3

	

	sub_get 3 &
	
	echo ----------------------------------------------------------
	echo "               WAITING FOR SIGNAL                       "
	echo ----------------------------------------------------------
	sleep 10	
	generate_signal 1

	sub_stop 3
	sig_stop 1
	
	sub_del 3

	sig_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "             WAITING FOR INFINITY                       "
	echo "             TEST CASE SUCCESSFUL...                    "
	echo ----------------------------------------------------------
}


TC_INFRA_FUNC_EVT_DEL_SUBSCRIPTION_WHEN_ON_WAIT() 
{

	echo RUNNING TESTCASE TC_INFRA_FUNC_EVT_DEL_SUBSCRIPTION_WHEN_ON_WAIT

	sig_init 1
	sub_init 3

	sub_create 3

	sub_inter 3

	sub_start 3
	sleep 2
	echo ----------------------------------------------------------
	echo "                WAITING FOR SIGNAL                      "
	echo ----------------------------------------------------------
	
	sub_get 3 &
	sleep 2
	echo ----------------------------------------------------------
	echo "         DELETING SUBSCRIPTION WHEN ON WAIT             "
	echo ----------------------------------------------------------
	delete_subscription_on_wait 3
	echo ----------------------------------------------------------
	echo "                     WAIT PROCESS TERMINATED            "
	echo "		 DUE TO SUBSCRIPTION DELETION                 " 
	echo ----------------------------------------------------------
	sub_stop 3
	sig_stop 1
	
	sig_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "                TEST CASE SUCCESSFUL...                 "
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_SIGNAL_EVT_FROM_INTERRUPT()
{

	echo TC_INFRA_FUNC_SIGNAL_EVT_FROM_INTERRUPT

	sig_init 1
	echo "SIG_INSTALL_INTR 1" > $LOCATION

	sub_init 3

	sub_create 3

	sub_inter 3
	echo "SUB_SUBSCRIBE_INTERRUPT_EVT 3" > $LOCATION
	sub_start 3

	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	echo "+ Try to ping the board to generate IP soft interrupt   +"
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	sleep 3

	sub_get 3 &
	sleep 1
	echo ----------------------------------------------------------
	echo "         DELETING SUBSCRIPTION              "
	echo ----------------------------------------------------------
	delete_subscription_on_wait 3
	sub_stop 3
	sig_stop 1

	echo "SIG_UNINSTALL_INTR 1" > $LOCATION
	sig_term 1
	sub_term 3
	echo ----------------------------------------------------------
	echo "                TEST CASE SUCCESSFUL...                 "
	echo ----------------------------------------------------------
}


TC_INFRA_FUNC_EVT_GENERATE_EVT_SIMULTANEOUS()
{

	echo RUNNING TESTCASE TC_INFRA_FUNC_EVT_GENERATE_EVT_SIMULTANEOUS
	sig_init 2
	sig_init 3

	sub_init 1
	sub_init 3

	sub_create 1
	sub_create 3
	
	sub_start 1
	sub_run 1

	sub_start 3
	echo ----------------------------------------------------------
	echo "         GENERATING SIMULTANEOUS EVENTS                 "
	echo ----------------------------------------------------------
	sleep 2

	sub_inter 1
	sub_inter 3

	echo ----------------------------------------------------------
	echo "                GENERATING SIGNAL1                      "
	echo ----------------------------------------------------------
	sig_start 2 &
	
	sleep 2
	echo ----------------------------------------------------------
	echo "                GENERATING SIGNAL3                      "
	echo ----------------------------------------------------------
	sig_start 3 & 
	
	echo ----------------------------------------------------------
	echo "               TWO SIGNALS RUNNING                      "
	echo ----------------------------------------------------------
	echo ----------------------------------------------------------
	echo "          WAITING FOR RESPECTIVE EVENTS                 "  
	echo ----------------------------------------------------------
	sleep 4
	sub_get 3 &
	echo ----------------------------------------------------------
	echo "                  3 Received                        "
	echo ---------------------------------------------------------- 
	sleep 4
	echo ----------------------------------------------------------
	echo "                  2 Received                        "
	echo ---------------------------------------------------------- 

	
	sub_stop 1
	sub_stop 3
	
	sig_stop 2
	sig_stop 3
		

	sub_del 1
	sub_del 3

	sig_term 2
	sig_term 3
	sub_term 1
	sub_term 3

	echo ----------------------------------------------------------
	echo "              BOTH SIGNALS RECEIVED BY                  "  
	echo "               RESPECTIVE SUBSCRIBERS...                "   
	echo "               TEST CASE SUCCESSFUL...                  "
	echo ----------------------------------------------------------
}

generate_signal()
{
	sleep 1
	echo SIG_START $1 > /sys/kernel/infrastructure/infra/test
	sleep 10
}

delete_subscription_on_wait()
{
	sleep 5
	echo SUB_DEL_SUBSCRIP $1 > /sys/kernel/infrastructure/infra/test
}

sig_init()
{
	echo SIG_INIT $1 > $LOCATION
}

sig_start()
{
	echo SIG_START $1 > $LOCATION
}

sig_signal()
{
	echo SIG_SIGNAL $1 > $LOCATION
}

sig_stop()
{
	echo SIG_STOP $1 > $LOCATION
}

sig_term()
{
	echo SIG_TERM $1 > $LOCATION
}

sub_init()
{
	echo SUB_INIT $1 > $LOCATION
}

sub_create()
{
	echo SUB_CRE_SUBSCRIP $1 > $LOCATION
}

sub_mod()
{
	echo SUB_MOD_SUBSCRIP $1 > $LOCATION
}

sub_inter()
{
	echo SUB_SET_INTER $1 > $LOCATION
}

sub_start()
{
	echo SUB_START $1 > $LOCATION
}

sub_run()
{
	echo SUB_RUN $1 > $LOCATION
}

sub_stop()
{
	echo SUB_STOP $1 > $LOCATION
}

sub_get()
{
	echo SUB_GET_EVT $1 > $LOCATION
}

sub_del()
{
	echo SUB_DEL_SUBSCRIP $1 > $LOCATION
}

sub_term()
{
	echo SUB_TERM $1 > $LOCATION
}


LOCATION="/sys/kernel/infrastructure/infra/test"

DUMP_SIGNALER=/"sys/kernel/debug/stm_event/dump_signalers"
DUMP_SUBSCRIBER="/sys/kernel/debug/stm_event/dump_subscribers"

if
	! lsmod | grep -q -s infra_test
then
	modprobe infra_test
fi


if [ "$1" == '--testall' ]; then
	
	echo " 1. TC_INFRA_API_EVT_INIT_SUBSCRIPTION"
	TC_INFRA_API_EVT_INIT_SUBSCRIPTION
	echo " 2. TC_INFRA_API_EVT_DEL_SUBSCRIPTION"
	TC_INFRA_API_EVT_DEL_SUBSCRIPTION
	echo " 3. TC_INFRA_API_EVT_MOD_SUBSCRIPTION"
	TC_INFRA_API_EVT_MOD_SUBSCRIPTION
	echo " 4. TC_INFRA_API_EVT_SET_INTERFACE"
	TC_INFRA_API_EVT_SET_INTERFACE
	echo " 5. TC_INFRA_API_EVT_SIGNAL"
	TC_INFRA_API_EVT_SIGNAL
	echo " 6. TC_INFRA_API_EVT_WAIT"
	TC_INFRA_API_EVT_WAIT
	echo " 7. TC_INFRA_FUNC_EVT_GENERATE_CONTINOUS"
	TC_INFRA_FUNC_EVT_GENERATE_CONTINOUS
	echo " 8. TC_INFRA_FUNC_EVT_CHECK_LOSS_OF_EVT"
	TC_INFRA_FUNC_EVT_CHECK_LOSS_OF_EVT
	echo " 9. TC_INFRA_FUNC_EVT_WAIT_INFINITY"
	TC_INFRA_FUNC_EVT_WAIT_INFINITY
	echo "10. TC_INFRA_FUNC_EVT_DEL_SUBSCRIPTION_WHEN_ON_WAIT"
	TC_INFRA_FUNC_EVT_DEL_SUBSCRIPTION_WHEN_ON_WAIT
	echo "11. TC_INFRA_FUNC_EVT_GENERATE_EVT_SIMULTANEOUS"
	TC_INFRA_FUNC_EVT_GENERATE_EVT_SIMULTANEOUS
	echo "12. TC_INFRA_FUNC_SIGNAL_EVT_FROM_INTERRUPT"
	TC_INFRA_FUNC_SIGNAL_EVT_FROM_INTERRUPT
	echo "13. TC_INFRA_API_EVT_ADD_DEL_SUBSCRIPTION"
	TC_INFRA_API_EVT_ADD_DEL_SUBSCRIPTION
fi


if [ "$1" == '--help' -o "$1" == "" ]; then
	EVT_HELP
else 
if [ "$1" == 'testall' ]; then
	echo "Invalid Option : Please Type source evt_test.sh --help for options"

else
	echo ""
	echo "Completed all the tests."
fi
fi






