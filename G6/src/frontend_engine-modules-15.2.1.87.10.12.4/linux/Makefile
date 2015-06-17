# Get the source directory with symlinks resolved
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
export STM_FE_TOPDIR := $(shell cd $(BUILD_SOURCE_PATH) && readlink -e ../)

subdir-ccflags-y := -Werror -Wall

obj-y += arch/common/
obj-y += driver/
obj-$(CONFIG_FRONTEND_ENGINE_STV090X) += driver/demod/stv090x/
obj-$(CONFIG_FRONTEND_ENGINE_STV0910) += driver/demod/stv0910/
obj-$(CONFIG_FRONTEND_ENGINE_STV0900) += driver/demod/stv0900/
obj-$(CONFIG_FRONTEND_ENGINE_STV0367) += driver/demod/stv0367/
obj-$(CONFIG_FRONTEND_ENGINE_STV0913) += driver/demod/stv0913/
obj-$(CONFIG_FRONTEND_ENGINE_MXL584) += driver/demod/mxl584/
obj-$(CONFIG_FRONTEND_ENGINE_MXL582) += driver/demod/mxl582/
obj-$(CONFIG_FRONTEND_ENGINE_MXL214) += driver/demod/mxl214/
obj-$(CONFIG_FRONTEND_ENGINE_SILABS) += driver/demod/silabs/
obj-y += driver/demod/dummy_demod/
obj-$(CONFIG_FRONTEND_ENGINE_REMOTE) += driver/demod/remote/pcpd_lla/
obj-$(CONFIG_FRONTEND_ENGINE_REMOTE) += driver/demod/remote/tsserver_lla/
obj-$(CONFIG_FRONTEND_ENGINE_TDA18212) += driver/demod/tuner/tda18212/
obj-y += driver/demod/tuner/
obj-$(CONFIG_FRONTEND_ENGINE_STV0900) += driver/demod/vglna/
obj-$(CONFIG_FRONTEND_ENGINE_STV0910) += driver/demod/vglna/
obj-y += driver/demod/lnb/
obj-y += driver/demod/diseqc/
obj-y += driver/demod/rf_matrix/
obj-y += driver/demod/util/
obj-$(CONFIG_FRONTEND_ENGINE_IPFE) += driver/ip/
obj-y += driver/stm_os_wrapper/
