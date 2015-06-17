STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/sync/,            \
                   stmsync.cpp                                                \
                   stmawg.cpp)

include $(STG_TOPDIR)/display/ip/sync/dvo/dvo.mak
