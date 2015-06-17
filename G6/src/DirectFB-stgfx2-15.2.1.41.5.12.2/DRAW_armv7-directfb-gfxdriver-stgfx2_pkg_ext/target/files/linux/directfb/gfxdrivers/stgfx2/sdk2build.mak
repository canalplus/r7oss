ifeq ($(SDK2_SOURCE_BLITTER),)
$(error SDK2_SOURCE_BLITTER must be specified to build the blitter kernel module)
endif
ENVIRONMENT+=SDK2_SOURCE_BLITTER

BUILD_BLITTER:=$(BUILD_DIR)/blitter

BLITTER_KBUILD:=$(BUILD_BLITTER)/linux/kernel
KBUILD_EXTRA_SYMBOLS += $(BLITTER_KBUILD)/Module.symvers
BLITTER_MODDIR:=blitter

BLITTER_COMMAND=$(KERNEL_COMMAND) M=$(BLITTER_KBUILD) INSTALL_MOD_DIR=$(BLITTER_MODDIR) \
		  STM_BLITTER_TOPDIR=$(SDK2_SOURCE_BLITTER)
BLITTER_COMMAND_TESTS=$(BLITTER_COMMAND) M=$(BLITTER_KBUILD)/../tests

blitter: $(BUILD_BLITTER)
	$(ECHO) Building blitter modules
	$(Q)$(BLITTER_COMMAND) modules

blitter_tests: $(BUILD_BLITTER)
	$(ECHO) Building blitter test environment
	$(Q)$(BLITTER_COMMAND_TESTS) modules

.modules_install_blitter_preinst:

.modules_install_blitter: .modules_install_blitter_preinst
	$(ECHO) Installing blitter modules
	$(ECHO) first, removing old modules from $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)
	$(Q)rm -rf $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)/$(BLITTER_MODDIR)
	$(Q)$(BLITTER_COMMAND) modules_install

.modules_install_blitter_tests:
	$(ECHO) Installing blitter modules
	$(Q)$(BLITTER_COMMAND_TESTS) modules_install

$(BUILD_BLITTER): config.in $(SDK2_SOURCE_BLITTER) $(BUILD_DIR) .FORCE
	$(ECHO) Removing blitter intermediates, and recreating links
	$(Q)mkdir -p $(BUILD_BLITTER)
	$(Q)find $(BUILD_BLITTER) -type l -exec rm {} \;
	$(Q)cp -prs $(SDK2_SOURCE_BLITTER)/* $(BUILD_BLITTER)

.clean_blitter:
	$(ECHO) Deleting blitter build dir: $(BUILD_BLITTER)
	$(Q)rm -rf $(BUILD_BLITTER)

MODULE_RULES+=blitter blitter_utils
INSTALL_RULES+=.modules_install_blitter
##SETUP_RULES+=$(BUILD_BLITTER)
CLEAN_RULES+=.clean_blitter


# Userspace

# Blitter utils
USERSPACE_BLITTER_UTILS:=$(USERSPACE_DIR)/blitter

packages+=blitter_utils

$(USERSPACE_BLITTER_UTILS): config.in $(SDK2_SOURCE_TEST_UTILS) $(USERSPACE_DIR) .FORCE
	$(ECHO) Removing userspace blitter intermediates, and recreating links
	$(Q)mkdir -p $(USERSPACE_BLITTER_UTILS)
	$(Q)find $(USERSPACE_BLITTER_UTILS) -type l -exec rm {} \;
	$(Q)cp -prs $(SDK2_SOURCE_BLITTER)/* $(USERSPACE_BLITTER_UTILS)


blitter_utils: $(USERSPACE_BLITTER_UTILS)
	$(ECHO) Building blitter utilities
	$(Q)$(MAKE) -C $(USERSPACE_BLITTER_UTILS)/linux/utils all

.install_blitter_utils: $(USERSPACE_BLITTER_UTILS) blitter_utils
	$(ECHO) Installing blitter utilities
	$(Q)$(MAKE) -C $(USERSPACE_BLITTER_UTILS)/linux/utils install

.clean_blitter_utils:
.distclean_blitter_utils:
	$(ECHO) Deleting userspace blitter utils build dir: $(USERSPACE_BLITTER_UTILS)
	$(Q)rm -rf $(USERSPACE_BLITTER_UTILS)

EXTRA_RULES+=blitter_utils
EXTRA_INSTALL_RULES+=.install_blitter_utils
CLEAN_RULES+=.clean_blitter_utils
DISTCLEAN_RULES+=.distclean_blitter_utils

############################################
# Stgfx2
USERSPACE_STGFX2:=$(USERSPACE_DIR)/blitter-stgfx2

packages+=stgfx2

ifeq ($(ARCH),armv7)
else ifeq ($(ARCH),armv7_uclibc)
else ifeq ($(ARCH),sh4)
else ifeq ($(ARCH),sh4_uclibc)
else
$(error Wrong arch: $(ARCH))
endif

stgfx2_makefiles     := $(USERSPACE_STGFX2)/Makefile $(USERSPACE_STGFX2)/linux/directfb/gfxdrivers/stgfx2/Makefile

$(USERSPACE_STGFX2)/.autom4te.cfg: $(USERSPACE_STGFX2) $(STM_HOST_TOOLS_PREFIX)/share/autoconf/autom4te.cfg
	sed s,/opt/STM/STLinux-2.4/host,$(STM_HOST_TOOLS_PREFIX),g $(STM_HOST_TOOLS_PREFIX)/share/autoconf/autom4te.cfg > $(USERSPACE_STGFX2)/.autom4te.cfg

$(USERSPACE_STGFX2)/configure: $(USERSPACE_STGFX2) $(USERSPACE_STGFX2)/.autom4te.cfg $(USERSPACE_STGFX2)/configure.ac $(USERSPACE_STGFX2)/Makefile.am $(USERSPACE_STGFX2)/linux/directfb/gfxdrivers/stgfx2/Makefile.am $(USERSPACE_STGFX2)/sdk2build.mak
	cd $(USERSPACE_STGFX2) && autoreconf --verbose --force --install

stgfx2_autoreconf: $(USERSPACE_STGFX2)/.autom4te.cfg $(USERSPACE_STGFX2)/configure

.SECONDEXPANSION:
$(stgfx2_makefiles): $(USERSPACE_STGFX2)/configure $$(patsubst %,%.in,$$@)
	cd $(USERSPACE_STGFX2) && ./configure --build=x86_64-pc-linux-gnu --host=$(_stm_target_config) --prefix=/usr --exec-prefix=/usr --bindir=/usr/bin --sbindir=/usr/sbin --sysconfdir=/etc --datadir=/usr/share --includedir=/usr/include --libdir=/usr/lib --libexecdir=/usr/libexec --localstatedir=/var --sharedstatedir=/usr/share --mandir=/usr/share/man --infodir=/usr/share/info --with-sysroot=$(TARGET_DIR)

stgfx2_configure: $(stgfx2_makefiles)

stgfx2: $(USERSPACE_STGFX2) $(USERSPACE_STGFX2)/Makefile $(USERSPACE_STGFX2)/linux/directfb/gfxdrivers/stgfx2/Makefile
	cd $(USERSPACE_STGFX2) && make

.install_stgfx2: $(USERSPACE_STGFX2)/Makefile $(USERSPACE_STGFX2)/linux/directfb/gfxdrivers/stgfx2/Makefile
	cd $(USERSPACE_STGFX2) && make DESTDIR=$(TARGET_DIR) install

.clean_stgfx2: $(USERSPACE_STGFX2)/Makefile $(USERSPACE_STGFX2)/linux/directfb/gfxdrivers/stgfx2/Makefile
	cd $(USERSPACE_STGFX2) && make clean

.stgfx2_help:
	@printf "\n STGFX2 specific targets:\n"
	@printf "        stgfx2 - Build just the stgfx2.so directfb acceleration libraries\n"

.distclean_stgfx2:
	$(ECHO) Deleting userspace stgfx2 build dir: $(USERSPACE_STGFX2)
	$(Q)rm -rf $(USERSPACE_STGFX2)

$(USERSPACE_STGFX2): config.in $(SDK2_SOURCE_BLITTER) $(USERSPACE_DIR)
	$(ECHO) Removing stgfx2 intermediates, and recreating links
	$(Q)mkdir -p $(USERSPACE_STGFX2)
	$(Q)[ -f $(USERSPACE_STGFX2)/Makefile ] && rm $(USERSPACE_STGFX2)/Makefile || true
	$(Q)find $(USERSPACE_STGFX2) -type l -exec rm {} \;
	$(Q)cp -prs $(SDK2_SOURCE_BLITTER)/* $(USERSPACE_STGFX2)
	$(Q)rm $(USERSPACE_STGFX2)/Makefile

EXTRA_RULES+=stgfx2
EXTRA_INSTALL_RULES+=.install_stgfx2
CLEAN_RULES+=.clean_stgfx2
HELP_RULES+=.stgfx2_help
DISTCLEAN_RULES+=.distclean_stgfx2



HELP_RULES+=.blitter_help
.blitter_help:
	@printf "\n blitter specific targets:\n"
	@printf "         blitter - Build just the blitter modules\n"
	@printf "   blitter_tests - Build the blitter test environment\n"
	@printf "   blitter_utils - Build the blitter utilities\n"
