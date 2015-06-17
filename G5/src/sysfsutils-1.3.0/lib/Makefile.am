lib_LTLIBRARIES = libsysfs.la
libsysfs_la_SOURCES = sysfs_utils.c sysfs_dir.c sysfs_bus.c sysfs_class.c \
	sysfs_device.c sysfs_driver.c sysfs.h dlist.c
INCLUDES = -I../include
libsysfs_la_LDFLAGS = -version-info 1:3:0
libsysfs_la_CFLAGS = -Wall -W -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
