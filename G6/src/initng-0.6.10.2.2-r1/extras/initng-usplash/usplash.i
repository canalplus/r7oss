service system/usplash {
	exec start = /bin/true;
	exec stop =  /sbin/initng-usplash-shutdown;
}
