Steps to test MSS using user app and linux char driver .
======================================================

1) Use scripts/msstest_user.sh to load infra_test modules
and create device files for mem sink and src .
2) Copy mss.h to /target/usr/linux/stm/
3) Copy msstest_user.c to  /target/root/
3) Build user app
	cc msstest_user.c -o msstest_user.o
4) Run ./msstest_user.o
