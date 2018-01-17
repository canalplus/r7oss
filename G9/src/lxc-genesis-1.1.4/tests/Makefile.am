testdir = $(prefix)/tests/$(PACKAGE)

configsdir = $(testdir)/configs

dist_configs_DATA  =
dist_configs_DATA += configs/comments.conf
dist_configs_DATA += configs/env_assignement_next_line.conf
dist_configs_DATA += configs/env_multiple_values.conf
dist_configs_DATA += configs/env_no_assignment.conf
dist_configs_DATA += configs/env_no_end_of_line.conf
dist_configs_DATA += configs/env_no_value.conf
dist_configs_DATA += configs/env_valid_name.conf
dist_configs_DATA += configs/env_valid_value.conf
dist_configs_DATA += configs/group_assignement_next_line.conf
dist_configs_DATA += configs/group_multiple_values.conf
dist_configs_DATA += configs/group_no_assignment.conf
dist_configs_DATA += configs/group_no_end_of_line.conf
dist_configs_DATA += configs/group_no_value.conf
dist_configs_DATA += configs/rlimit_assignement_next_line.conf
dist_configs_DATA += configs/rlimit_invalid_value.conf
dist_configs_DATA += configs/rlimit_multiple_values.conf
dist_configs_DATA += configs/rlimit_no_assignment.conf
dist_configs_DATA += configs/rlimit_no_current.conf
dist_configs_DATA += configs/rlimit_no_end_of_line.conf
dist_configs_DATA += configs/rlimit_no_maximum.conf
dist_configs_DATA += configs/rlimit_no_value.conf
dist_configs_DATA += configs/umask_assignement_next_line.conf
dist_configs_DATA += configs/umask_invalid_value.conf
dist_configs_DATA += configs/umask_multiple_values.conf
dist_configs_DATA += configs/umask_no_assignment.conf
dist_configs_DATA += configs/umask_no_end_of_line.conf
dist_configs_DATA += configs/umask_no_value.conf
dist_configs_DATA += configs/user_assignement_next_line.conf
dist_configs_DATA += configs/user_multiple_values.conf
dist_configs_DATA += configs/user_no_assignment.conf
dist_configs_DATA += configs/user_no_end_of_line.conf
dist_configs_DATA += configs/user_no_value.conf
dist_configs_DATA += configs/valid.conf

AM_CPPFLAGS  =
AM_CPPFLAGS += -I$(top_builddir)/src
AM_CPPFLAGS += -I$(top_srcdir)/src

AM_CFLAGS = $(cunit_CFLAGS)

AM_LDFLAGS = $(cunit_LIBS)

test_PROGRAMS = unit_tests

unit_tests_SOURCES  =
unit_tests_SOURCES += main.c
unit_tests_SOURCES += tests.c
unit_tests_SOURCES += tests.h

# tests
unit_tests_SOURCES += test_config.c

unit_tests_LDADD = $(top_builddir)/src/liblxc_genesis_core.a
