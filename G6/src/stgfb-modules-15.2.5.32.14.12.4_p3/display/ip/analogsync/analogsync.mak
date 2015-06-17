# Classes for the analog syncs.

ifeq ($(CONFIG_DEBUG_DENC_SYNC),y)
EXTRA_CXXFLAGS += -DCONFIG_DEBUG_DENC_SYNC
endif

ifeq ($(CONFIG_DEBUG_HDF_SYNC),y)
EXTRA_CXXFLAGS += -DCONFIG_DEBUG_HDF_SYNC
endif

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/analogsync/,                    \
                   stmfwloader.cpp                                                    \
                   stmanalogsync.cpp)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/analogsync/denc/,               \
                   stmdencsync.cpp)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/analogsync/denc/fw_gen/,        \
                   fw_gen.c                                                           \
                   getlevel.c)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/analogsync/hdf/,                \
                   stmhdfsync.cpp)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/analogsync/hdf/fw_gen/,         \
                   fw_gen.c                                                           \
                   getlevel.c)
