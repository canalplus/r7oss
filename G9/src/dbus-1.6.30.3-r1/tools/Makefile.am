configdir=$(sysconfdir)/dbus-1

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(DBUS_STATIC_BUILD_CPPFLAGS) \
	$(DBUS_X_CFLAGS) \
	-DDBUS_COMPILATION \
	-DDBUS_MACHINE_UUID_FILE=\""$(localstatedir)/lib/dbus/machine-id"\" \
	$(NULL)

# if assertions are enabled, improve backtraces
AM_LDFLAGS = @R_DYNAMIC_LDFLAG@

bin_PROGRAMS = \
	dbus-launch \
	dbus-monitor \
	dbus-send \
	dbus-request-name \
	dbus-wait-dbus \
	$(NULL)

if DBUS_UNIX
bin_PROGRAMS += \
	dbus-cleanup-sockets \
	dbus-uuidgen \
	$(NULL)
endif

dbus_send_SOURCES=				\
	dbus-print-message.c			\
	dbus-print-message.h			\
	dbus-send.c

dbus_monitor_SOURCES=				\
	dbus-monitor.c				\
	dbus-print-message.c			\
	dbus-print-message.h

if DBUS_WIN
dbus_launch_SOURCES=				\
	dbus-launch-win.c			\
	dbus-launch.h
else
dbus_launch_SOURCES=				\
	dbus-launch.c				\
	dbus-launch-x11.c			\
	dbus-launch.h
endif

dbus_request_name_SOURCES= 			\
	dbus-request-name.c

dbus_wait_dbus_SOURCES= 			\
	dbus-wait-dbus.c

dbus_cleanup_sockets_SOURCES=			\
	dbus-cleanup-sockets.c

dbus_uuidgen_SOURCES=				\
	dbus-uuidgen.c

dbus_send_LDADD = \
	$(top_builddir)/dbus/libdbus-1.la \
	$(NULL)

dbus_monitor_LDADD = \
	$(top_builddir)/dbus/libdbus-1.la \
	$(NETWORK_libs) \
	$(NULL)

dbus_uuidgen_LDADD = \
	$(top_builddir)/dbus/libdbus-1.la \
	$(NULL)

dbus_launch_LDADD = \
	$(DBUS_X_LIBS) \
	$(NULL)

dbus_request_name_LDADD = \
	$(top_builddir)/dbus/libdbus-1.la \
	$(NULL)

dbus_wait_dbus_LDADD = \
	$(top_builddir)/dbus/libdbus-1.la \
	$(NULL)

if ENABLE_LXC_GENESIS
dbus_cleanup_sockets_CFLAGS=-fPIE
dbus_cleanup_sockets_LDFLAGS=-pie
dbus_send_CFLAGS=-fPIE
dbus_send_LDFLAGS=-pie
dbus_uuidgen_CFLAGS=-fPIE
dbus_uuidgen_LDFLAGS=-pie
dbus_monitor_CFLAGS=-fPIE
dbus_monitor_LDFLAGS=-pie
dbus_launch_CFLAGS=-fPIE
dbus_launch_LDFLAGS=-pie
dbus_request_name_CFLAGS=-fPIE
dbus_request_name_LDFLAGS=-pie
dbus_wait_dbus_CFLAGS=-fPIE
dbus_wait_dbus_LDFLAGS=-pie
endif ENABLE_LXC_GENESIS

EXTRA_DIST = run-with-tmp-session-bus.sh strtoll.c strtoull.c
CLEANFILES = 				\
	run-with-tmp-session-bus.conf

# create the /var/lib/dbus directory for dbus-uuidgen
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/lib/dbus

installcheck-local:
	test -d $(DESTDIR)$(localstatedir)/lib/dbus
