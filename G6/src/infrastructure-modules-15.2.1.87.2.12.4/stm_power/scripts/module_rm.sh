#!/bin/sh
RED='\e[0;31m'
NC='\e[0m'
GREEN='\e[0;32m'

echo -e "${GREEN} +++ Unloading SDK2 Modules +++"

remove_modules()
{
	mod_list=$1;
	for  each in $mod_list; do
		rmmod -v $each;
		if [ $? -ne 0 ]; then
			echo -e "${RED} $each unloading failed"
			echo -n -e "$NC}"
			exit;
		fi
	done
}

module_count=$(cat /proc/modules | grep -e "^stlinuxtv" | cut -d" " -f3)
#check if there are any modules dependent on stlinuxtv
if [ $((module_count)) -ne 0 ]; then
	# look for all the modules dependent on stlinuxtv
	module_list=$(cat /proc/modules | grep -e "^stlinuxtv" | cut -d" " -f4 | sed -e 's,\,, ,g')

	echo -e -n "${GREEN}"
	#remove all the dependent modules
	remove_modules "$module_list"
	echo -n -e "${NC}"
fi

#remove stlinuxtv now.
#it cannot be removed through the below loop because it is then removed
#out of order after other stkpi kos. This results in kernel oops.
if
    lsmod | grep -q -s stlinuxtv
then
    echo -n -e "${GREEN}"
    rmmod -v stlinuxtv
    echo -n -e "${NC}"
fi

exit_val=1

#keep looping till their are sdk2 modules inserted
while [ 1 ]
do

#collect a list of modules whose user count is zero
module_list=$(cat /proc/modules | grep 'permanent' -v |  cut -d" " -f3,1 | grep -w 0 | cut -d" " -f1 | grep 'nls_cp850' -v | grep 'ipv6' -v)
module_count=$(echo $module_list | wc -w)

#if list is empty the break the loop
if [ $module_count -eq 0 ]; then
	module_list=$(cat /proc/modules | grep 'permanent' -v |  cut -d" " -f3,1 | cut -d" " -f1 | grep 'nls_cp850' -v | grep 'ipv6' -v)
	module_count=$(echo $module_list | wc -w)
	if [ $((module_count)) -ne 0 ]; then
		echo -e "${RED}[ERROR] +++ Module removal failed +++"
		echo -e "++++ Following modules could not be removed ++++"
		lsmod
		echo -n -e "${NC}"
	fi
	break;
fi

echo -n -e "${GREEN}"
#remove all the modules is the list with user count zero.
#remove all the dependent modules
remove_modules $module_list
echo -n -e "${NC}"

done

