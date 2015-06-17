REG_HELP()
{

	echo "These are the Test Cases for the Registry Manager"
	echo
	echo "1 TC_INFRA_FUNC_REG_ADD_REMOVE_OBJECTS"
	echo "2 TC_INFRA_FUNC_REG_ADD_REMOVE_ATTRIBUTE"
	echo "3 TC_INFRA_FUNC_REG_ADD_REMOVE_CONNECTION"
	echo "4 TC_INFRA_FUNC_REG_GET_INFO"
	echo "5 TC_INFRA_FUNC_REG_GET_ATTR_INFO"
	echo "6 TC_INFRA_FUNC_REG_ITERATOR"
	echo "7 TC_INFRA_STRESS_REG_MEMORY_LEAK"
	echo "8 TC_INFRA_STRESS_REG_THREAD_SAFE"
	echo "9 TC_INFRA_FUNC_REG_DATA_TYPE_ADD_REMOVE"


}


TC_INFRA_FUNC_REG_ADD_REMOVE_OBJECTS()
{
	echo ----------------------------------------------------------
	echo "Adding and Removing Objects"
	echo ----------------------------------------------------------
	
	echo REG_FUNCT_TEST > $LOCATION

	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_ADD_REMOVE_ATTRIBUTE()
{
	echo ----------------------------------------------------------
	echo "Adding and Removing Attributes"
	echo ----------------------------------------------------------
	
	echo REG_FUNCT_TEST > $LOCATION
	
	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_ADD_REMOVE_CONNECTION()
{
	echo ----------------------------------------------------------
	echo "Adding and Removing Connections"
	echo ----------------------------------------------------------
	
	echo REG_FUNCT_TEST > $LOCATION
	
	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_GET_INFO()
{
	echo ----------------------------------------------------------
	echo "Getting Registry Info"
	echo ----------------------------------------------------------
	
	echo REG_DYN_TEST_START> $LOCATION

	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_GET_ATTR_INFO()
{
	echo ----------------------------------------------------------
	echo "Getting Attribute Info"
	echo ----------------------------------------------------------
	
	echo REG_DYN_TEST_START> $LOCATION

	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_ITERATOR()
{
	
	echo ----------------------------------------------------------
	echo "Testing Iterator Functionality"
	echo ----------------------------------------------------------
	
	echo REG_ITERATOR_TEST > $LOCATION

	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_STRESS_REG_MEMORY_LEAK()
{
	echo ----------------------------------------------------------
	echo "Testing for Memory Leak"
	echo ----------------------------------------------------------
	
	echo REG_MEMORYLEAK_TEST > $LOCATION

	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_STRESS_REG_THREAD_SAFE()
{
	echo ----------------------------------------------------------
	echo "Testing Add and Removal from Various Threads"
	echo ----------------------------------------------------------
	
	echo REG_STRESS_TEST> $LOCATION
	
	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

TC_INFRA_FUNC_REG_DATA_TYPE_ADD_REMOVE()
{
	echo ----------------------------------------------------------
	echo "Testing Add and Removal of Different Datatypes"
	echo ----------------------------------------------------------

	echo REG_DATATYPE_TEST > $LOCATION
	
	echo ----------------------------------------------------------
	echo "Test Case Successful"
	echo ----------------------------------------------------------
}

LOCATION="/sys/kernel/infrastructure/infra/test"

if [ "$1" == '--help' -o "$1" == "" ]; then
        REG_HELP
else
        echo "Invalid Option : Please Type source EVTTest.sh --help for options"

fi


if
        ! lsmod | grep -q -s infra_test
then
        modprobe infra_test
fi
