include $(STG_TOPDIR)/display/ip/sync/dvo/fw_gen/fw.mak

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/sync/dvo/,              \
                   stmsyncdvo.cpp                                            \
                   stmawgdvo.cpp)
