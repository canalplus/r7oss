bin_PROGRAMS = tdaemon tclient

tdaemon_SOURCES = tdaemon.cpp
tdaemon_LDADD = $(wylog_LIBS) $(dbusxx_LIBS) $(top_srcdir)/src/libdbus-c++-1.la -L/usr/targets/lamar-1.4.5.17-test/root//usr/lib -lwylog
tdaemon_LDADD += $(wytools2_LIBS) 
tdaemon_CFLAGS = -DWYLOG_MODULE=\"tdaemon\" -DDEBUG -rdynamic \
					-I$(top_srcdir)/test/tester/include \
					-I$(top_srcdir)/test/tester/src \
					-I$(top_srcdir)/include
tdaemon_CXXFLAGS = $(tdaemon_CFLAGS)

tclient_SOURCES = tclient.cpp
tclient_LDADD = $(wylog_LIBS) $(dbusxx_LIBS) $(top_srcdir)/src/libdbus-c++-1.la -L/usr/targets/lamar-1.4.5.17-test/root//usr/lib -lwylog
tclient_CFLAGS = -DWYLOG_MODULE=\"tclient\" -DDEBUG -rdynamic \
					-I$(top_srcdir)/test/tester/include \
					-I$(top_srcdir)/test/tester/src \
					-I$(top_srcdir)/include
tclient_CXXFLAGS = $(tclient_CFLAGS)

#noinst_HEADERS = tdaemon.h test-adaptor.h test-proxy.h hal-adaptor.h hal-proxy.h
noinst_HEADERS = tdaemon.h test-adaptor.h test-proxy.h

test-adaptor.h: $(top_srcdir)/test/tester/xml/test-dbus.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$(notdir $@)

test-proxy.h: $(top_srcdir)/test/tester/xml/test-dbus.xml
	$(DBUSXX_XML2CPP) $< --proxy=$(notdir $@)

#test-errors.h: $(top_srcdir)/xml/test-dbus.xml
#	/usr/bin/cpp-errors-str-gen test-errors $<

#hal-adaptor.h: $(top_srcdir)/xml/hal-dbus.xml
#	$(DBUSXX_XML2CPP) $< --adaptor=$(notdir $@)

#hal-proxy.h: $(top_srcdir)/xml/hal-dbus.xml
#	$(DBUSXX_XML2CPP) $< --proxy=$(notdir $@)

#BUILT_SOURCES = test-adaptor.h test-proxy.h test-errors.h hal-adaptor.h hal-proxy.h
BUILT_SOURCES = test-adaptor.h test-proxy.h 
CLEANFILES = ${BUILT_SOURCES}
