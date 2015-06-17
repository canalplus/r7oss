
ifndef  INSTALL_MOD_PATH
${error INSTALL_MOD_PATH not defined }
endif

ifndef  ARCH
${error ARCH not defined }
endif

ifndef  CROSS_COMPILE
${error CROSS_COMPILE not defined }
endif

ifndef  KDIR
${error KDIR not defined }
endif

ccflags-y = -g  -D_GNU_SOURCE -Wall -lpthread

obj-m := mttsample_mod.o
obj-m := kptrace_legacy.o

all:
	$(CROSS_COMPILE)gcc -DARCH_$(ARCH) $(ccflags-y) -o mttsample mttsample.c -lpthread -lrt -lmtt

install:
	sudo cp mttsample $(INSTALL_MOD_PATH)/root
clean:
	rm -fr .*.cmd .tmp_versions *.symvers *.o *.ko *.mod.c *.order *~ mttsample
