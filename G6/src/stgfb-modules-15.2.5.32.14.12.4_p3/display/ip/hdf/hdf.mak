# Classes for the analog HDFormatter.

ifneq ($(CONFIG_HDF_V4_3),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdf/,                   \
                   stmhdf.cpp                                                 \
                   stmhdf_v4_3.cpp)

endif

ifneq ($(CONFIG_HDF_V5_0),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdf/,                   \
                   stmhdf.cpp                                                 \
                   stmhdf_v5_0.cpp)

endif
