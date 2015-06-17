# Copy this Script in the infrastructure folder on the target system


load_all()
{
	echo "Load all the infra modules"
	insmod ./wrapper/stm_wrapper.ko
	insmod ./arch/stm_arch.ko
	insmod ./registry/stm_registry.ko
	insmod ./event/stm_event.ko
	insmod ./memsrcsink/stm_memsrcsink.ko
}

load_test()
{
	insmod ./event/evt_test.ko
	insmod ./registry/registry_test.ko
}

unload_all()
{
	echo "Unload all the infra modules"
	rmmod stm_memsrcsink.ko
	rmmod stm_event.ko
	rmmod stm_registry.ko
	rmmod stm_arch.ko
	rmmod stm_wrapper.ko
}

unload_test()
{
	rmmod registry_test.ko
	rmmod evt_test.ko
}

 if [ "$#" -eq "0" ] ; then
  echo "Syntax: load.sh <ARG>"
  echo "1.Load all the infra modules"
  echo "2.Unload all the infra modules"
  echo "3.Load all the test modules"
  echo "4.Unload all the test modules"
 elif [ "$1" -eq "1" ] ; then
  load_all
 elif [ "$1" -eq "2" ] ; then
  unload_all
 elif [ "$1" -eq "3" ] ; then
  load_test
 elif [ "$1" -eq "4" ] ; then
  unload_test
 else
  echo "******ERROR: WRONG ARG ************"
 fi
