ifeq ($(SDK2_SOURCE_CRYPTODEV),)
$(error SDK2_SOURCE_CRYPTODEV must be specified to build the CryptoDevLinux module)
endif

BUILD_CRYPTODEV := $(BUILD_DIR)/cryptodev-linux

KBUILD_EXTRA_SYMBOLS += $(BUILD_CRYPTODEV)/Module.symvers
CRYPTODEV_MODDIR := cryptodev-linux

CRYPTODEV_KCOMMAND = $(KERNEL_COMMAND) M=$(BUILD_CRYPTODEV) \
		INSTALL_MOD_DIR=$(CRYPTODEV_MODDIR)

cryptodev-linux: $(BUILD_CRYPTODEV) .cryptodev-linux_version
	$(ECHO) Building cryptodev-linux modules
	$(Q)$(CRYPTODEV_KCOMMAND) modules

# Cryptodev-linux Makefile expects to build version.h target before modules
.cryptodev-linux_version: $(BUILD_CRYPTODEV)
	$(Q)$(MAKE) -C $(BUILD_CRYPTODEV) version.h

.modules_install_cryptodev-linux:
	$(ECHO) Installing cryptodev-linux modules
	$(ECHO) first, removing old modules from $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)
	$(Q)rm -rf $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)/$(CRYPTODEV_MODDIR)
	$(Q)$(CRYPTODEV_KCOMMAND) modules_install

$(BUILD_CRYPTODEV): config.in $(SDK2_SOURCE_CRYPTODEV) $(BUILD_DIR) .FORCE
	$(ECHO) Removing cryptodev-linux intermediates, and recreating links
	$(Q)mkdir -p $(BUILD_CRYPTODEV)
	$(Q)find $(BUILD_CRYPTODEV) -type l -exec rm {} \;
	$(Q)cp -prs $(SDK2_SOURCE_CRYPTODEV)/* $(BUILD_CRYPTODEV)

.clean_cryptodev-linux:
	$(ECHO) Deleting cryptodev-linux build dir
	$(Q)rm -rf $(BUILD_CRYPTODEV)

.headers_install_cryptodev-linux: $(BUILD_CRYPTODEV)
	$(ECHO) Installing cryptodev headers
	install -D $(BUILD_CRYPTODEV)/crypto/cryptodev.h $(TARGET_MOD_DIR)/usr/include/crypto/cryptodev.h

cryptodev-linux_tests: $(BUILD_CRYPTODEV)
	$(ECHO) Building cryptodev-linux tests
	$(MAKE) -C $(BUILD_CRYPTODEV)/tests install CC=$(CROSS_COMPILE)gcc LD_LIBRARY_PATH=$(TARGET_MOD_DIR)/usr/lib INSTALL_DIR=$(TARGET_MOD_DIR)/root/cryptodev INCLUDES=$(TARGET_MOD_DIR)/usr/include

MODULE_RULES += cryptodev-linux
INSTALL_RULES += .modules_install_cryptodev-linux .headers_install_cryptodev-linux
SETUP_RULES += $(BUILD_CRYPTODEV)
CLEAN_RULES += .clean_cryptodev-linux

HELP_RULES += .cryptodev-linux_help
.cryptodev-linux_help:
	@printf "\n cryptodev-linux specific targets:\n"
	@printf "\t cryptodev-linux - Build just the cryptodev-linux modules\n"
	@printf "\t cryptodev-linux_headers - Install cryptodev-linux headers\n"
	@printf "\t cryptodev-linux_tests - Install cryptodev-linux tests\n"
