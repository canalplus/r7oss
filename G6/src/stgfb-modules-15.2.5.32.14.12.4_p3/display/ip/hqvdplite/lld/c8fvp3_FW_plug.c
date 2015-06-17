#include "c8fvp3_FW_lld_global.h"
#include "hqvdp_ucode_256_rd.h"
#include "hqvdp_ucode_256_wr.h"
#include "InStreamer.h"

gvh_u32_t c8fvp3_config_ucodeplug(gvh_u32_t base_address,
                                  gvh_u32_t *UcodePlug,
                                  gvh_u32_t UcodeSize)
{
  gvh_u32_t cnt = 0;
  for (cnt = 0; cnt < UcodeSize; cnt ++) {
    WriteRegister(base_address + (cnt*sizeof(*UcodePlug)), *(UcodePlug + cnt));
  }
  return 0;
}


gvh_u32_t c8fvp3_config_regplug(gvh_u32_t base_address,
                                gvh_u32_t page_size,
                                gvh_u32_t min_opc,
                                gvh_u32_t max_opc,
                                gvh_u32_t max_chk,
                                gvh_u32_t max_msg,
                                gvh_u32_t min_space,
                                gvh_u32_t control) 
{
  WriteRegister(base_address + IN_STREAMER_PAGE_SIZE_REG_OFFSET, page_size);
  WriteRegister(base_address + IN_STREAMER_MIN_OPC_REG_OFFSET,   min_opc);
  WriteRegister(base_address + IN_STREAMER_MAX_OPC_REG_OFFSET,   max_opc);
  WriteRegister(base_address + IN_STREAMER_MAX_CHK_REG_OFFSET,   max_chk);
  WriteRegister(base_address + IN_STREAMER_MAX_MSG_REG_OFFSET,   max_msg);
  WriteRegister(base_address + IN_STREAMER_MIN_SPACE_REG_OFFSET, min_space);
  WriteRegister(base_address + IN_STREAMER_CONTROL_REG_OFFSET,   control);
  return 0;
}
