ifeq ($(subdir), csu)
CPPFLAGS-start.S = -D__ASSEMBLY__
endif

# [SC] st200 compiler does not support complex type
no-complex-fcts = yes

ifeq ($(subdir),gnulib)
sysdep_routines += __init_trampoline
endif
