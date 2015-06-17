bin_PROGRAMS = dlist_test get_class_dev get_device get_driver write_attr \
		get_bus_devices_list get_classdev_parent testlibsysfs
BUILT_SOURCES = test.h
CLEANFILES = test.h		
test.h:
	./create-test
dlist_test_SOURCES = dlist_test.c
get_class_dev_SOURCES = get_class_dev.c
get_device_SOURCES = get_device.c
get_driver_SOURCES = get_driver.c
get_bus_devices_list_SOURCES = get_bus_devices_list.c
write_attr_SOURCES = write_attr.c
get_classdev_parent_SOURCES = get_classdev_parent.c
testlibsysfs_SOURCES = test.c test_bus.c test_class.c test_dir.c test_driver.c \
		       test_root.c test_utils.c testout.c test-defs.h \
		       libsysfs.conf create-test
INCLUDES = -I../include
LDADD = ../lib/libsysfs.la
CFLAGS = -Wall -Wshadow -W -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls

