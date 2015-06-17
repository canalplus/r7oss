ifeq ($(SDK2_SOURCE_CRYPTO_ENGINE),)
$(error SDK2_SOURCE_CRYPTO_ENGINE must be specified to build the CryptoEngine module)
endif

# For backward compatibility
export CRYPTO_ENGINE=$(SDK2_SOURCE_CRYPTO_ENGINE)

BUILD_CRYPTO_ENGINE := $(BUILD_DIR)/crypto_engine

CRYPTO_ENGINE_KBUILD:=$(BUILD_CRYPTO_ENGINE)/linux
KBUILD_EXTRA_SYMBOLS += $(CRYPTO_ENGINE_KBUILD)/Module.symvers
CRYPTO_ENGINE_MODDIR:=crypto_engine

CRYPTO_ENGINE_COMMAND=$(KERNEL_COMMAND) M=$(CRYPTO_ENGINE_KBUILD) INSTALL_MOD_DIR=$(CRYPTO_ENGINE_MODDIR) \
		  TREE_ROOT=$(SDK2_SOURCE_CRYPTO_ENGINE) \
		  CONFIG_INFRASTRUCTURE_PATH=$(INFRASTRUCTURE) \
		  CONFIG_MULTICOM_PATH=$(MULTICOM4) \
		  CONFIG_TRANSPORT_ENGINE_PATH=$(TRANSPORT_ENGINE) \

crypto_engine: $(BUILD_CRYPTO_ENGINE)
	$(ECHO) Building crypto_engine modules
	$(Q)$(CRYPTO_ENGINE_COMMAND) modules

.headers_install_crypto_engine:
	$(ECHO) Installing crypto_engine headers
	$(Q)$(KERNEL_MAKE) -C $(BUILD_CRYPTO_ENGINE) headers_install INSTALL_HDR_PATH=$(TARGET_MOD_DIR)

.modules_install_crypto_engine:
	$(ECHO) Installing crypto_engine modules
	$(ECHO) first, removing old modules from $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)
	$(Q)rm -rf $(TARGET_MOD_DIR)/lib/modules/$(KERNELRELEASE)/$(CRYPTO_ENGINE_MODDIR)
	$(Q)$(CRYPTO_ENGINE_COMMAND) modules_install

$(BUILD_CRYPTO_ENGINE): config.in $(SDK2_SOURCE_CRYPTO_ENGINE) $(BUILD_DIR) .FORCE
	$(ECHO) Removing crypto_engine intermediates, and recreating links
	$(Q)mkdir -p $(BUILD_CRYPTO_ENGINE)
	$(Q)find $(BUILD_CRYPTO_ENGINE) -type l -exec rm {} \;
	$(Q)cp -prs $(SDK2_SOURCE_CRYPTO_ENGINE)/* $(BUILD_CRYPTO_ENGINE)
	$(Q)cp -pfs $(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)/stm_ce_sp/key-rules.h $(SDK2_SOURCE_CRYPTO_ENGINE)/include/

.clean_crypto_engine:
	$(ECHO) Deleting crypto_engine build dir
	$(Q)rm -rf $(BUILD_CRYPTO_ENGINE)

MODULE_RULES+=crypto_engine
INSTALL_RULES+=.modules_install_crypto_engine .headers_install_crypto_engine
SETUP_RULES+=$(BUILD_CRYPTO_ENGINE)
CLEAN_RULES+=.clean_crypto_engine

HELP_RULES+=.crypto_engine_help
.crypto_engine_help:
	@printf "\n crypto_engine specific targets:\n"
	@printf "        crypto_engine - Build just the crypto engine modules\n"
