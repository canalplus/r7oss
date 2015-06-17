/********************************************************************************
* COPYRIGHT (C) STMicroelectronics 2011.
*
*
* File name   : c8fvp3_FW_plug.h $Revision: 1420 $
*
* Last modification done by $Author: achoul $ on $Date:: 2011-01-31 $
*
********************************************************************************/

/*!
 * \file c8fvp3_FW_plug.h
 *
 */

#ifndef C8FVP3_FW_PLUG_H_
#define C8FVP3_FW_PLUG_H_

#include "hqvdp_ucode_256_rd.h"
#include "hqvdp_ucode_256_wr.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

#define c8fvp3_CONFIG_RD_UCODEPLUG(base_address) \
c8fvp3_config_ucodeplug(base_address+HQVDP_LITE_STR_READ_RDPLUG_BASEADDR,HQVDP_UcodeRdPlug,HQVDP_UCODERDPLUG_SIZE)

#define c8fvp3_CONFIG_WR_UCODEPLUG(base_address) \
c8fvp3_config_ucodeplug(base_address+HQVDP_LITE_STR_WRITE_WRPLUG_BASEADDR,HQVDP_UcodeWrPlug,HQVDP_UCODEWRPLUG_SIZE)

#define c8fvp3_CONFIG_RD_REGPLUG(base_address,page_size,min_opc,max_opc,max_chk,max_msg,min_space,control) \
c8fvp3_config_regplug(base_address+HQVDP_LITE_STR_READ_RDPLUG_BASEADDR,page_size,min_opc,max_opc,max_chk,max_msg,min_space,control)

#define c8fvp3_CONFIG_WR_REGPLUG(base_address,page_size,min_opc,max_opc,max_chk,max_msg,min_space,control) \
c8fvp3_config_regplug(base_address+HQVDP_LITE_STR_WRITE_WRPLUG_BASEADDR,page_size,min_opc,max_opc,max_chk,max_msg,min_space,control)

/*!
 * \brief Do initialization of the ucode of a plug
 *
 * \param[in] base_address base address of the plug micro code memory
 * \param[in] UcodePlug    microcode
 * \param[in] UcodeSize    microcode size in bytes
 */
gvh_u32_t c8fvp3_config_ucodeplug(gvh_u32_t base_address,
                                  gvh_u32_t *UcodePlug,
                                  gvh_u32_t UcodeSize);


/*!
 * \brief Do initialization of the registers of a plug
 * see plug documentation for details on parameters
 *
 * \param[in] base_address base address of the plug registers
 * \param[in] page_size page size
 * \param[in] min_opc min op code
 * \param[in] max_opc max op code
 * \param[in] max_chk max chunk
 * \param[in] max_msg max message
 * \param[in] min_space minimum space between requests
 * \param[in] control control register
 */
gvh_u32_t c8fvp3_config_regplug(gvh_u32_t base_address,
                                gvh_u32_t page_size,
                                gvh_u32_t min_opc,
                                gvh_u32_t max_opc,
                                gvh_u32_t max_chk,
                                gvh_u32_t max_msg,
                                gvh_u32_t min_space,
                                gvh_u32_t control);

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif
