
testsdir = $(prefix)/tests/@PACKAGE_NAME@
tests_PROGRAMS = test_all test_proxy

test_all_SOURCES = \
	test_timeout.cpp \
	test_flood_meth.cpp \
	test_flood_sign.cpp \
	test_flood.h \
	test_flood.cpp \
	test_flood_sign_multi.cpp \
	test_queue_deadlock.cpp \
	test_pipe_deadlock.cpp \
	test_flood_multi_dispatch.cpp \
	test_connection.cpp \
	test_proxy_creation.cpp

#	test_flood_sig_handle_while_method.cpp \ THIS TESTS NEEDS A HUGE CLEANUP (don't know where it comes from) !
#	test_flood_dynamic_proxy.cpp
test_all_LDADD = \
	$(top_builddir)/src/libdbus-c++-1.la \
	$(builddir)/libtest.la \
	$(cppunit_LIBS)
test_all_CXXFLAGS = \
	-I$(top_srcdir)/src/ -I$(top_srcdir)/include/ \
	$(cppunit_CFLAGS)

test_proxy_SOURCES = test_proxy.cpp test_flood.cpp test_flood.h
test_proxy_LDADD = \
	$(top_builddir)/src/libdbus-c++-1.la \
	$(builddir)/libtest.la \
	$(cppunit_LIBS)
test_proxy_CXXFLAGS = \
	-I$(top_srcdir)/src/ -I$(top_srcdir)/include/ \
	$(cppunit_CFLAGS) -lrt -lpthread

noinst_LTLIBRARIES = libtest.la

libtest_la_SOURCES  = test_main.cpp test_helpers.cpp test_helpers.h
libtest_la_CXXFLAGS = \
	-I$(top_srcdir)/src/ -I$(top_srcdir)/include/ \
	$(cppunit_CFLAGS) $(dbus_CFLAGS)
libtest_la_LIBADD    = $(cppunit_LIBS) -lpthread -lrt

