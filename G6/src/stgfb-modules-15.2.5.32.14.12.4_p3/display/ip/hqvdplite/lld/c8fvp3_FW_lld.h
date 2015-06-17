/********************************************************************************
* COPYRIGHT (C) STMicroelectronics 2011.
*
*
* File name   : c8fvp3_FW_lld.h $Revision: 1420 $
*
* Last modification done by $Author: achoul $ on $Date:: 2011-01-31 $
*
********************************************************************************/

/*!
 * \file c8fvp3_FW_lld.h
 *
 */

#ifndef C8FVP3_FW_LLD_H_
#define C8FVP3_FW_LLD_H_

#include "c8fvp3_FW_lld_global.h"
#include "c8fvp3_FW_download_code.h"
#include "c8fvp3_FW_plug.h"
#include "c8fvp3_HVSRC_Lut.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Do initialization of the IP
 *
 * \param[in] base_address base address of the IP
 */
void c8fvp3_BootIp(gvh_u32_t base_address);

/*!
 * \brief to wait until reset is done
 *
 * \param[in] base_address base address of the IP
 */
gvh_bool_t c8fvp3_IsBootIpDone (gvh_u32_t base_address);

/*!
 * \brief Do initialization of the vsync mode
 *
 * \param[in] base_address base address of the IP
 * \param[in] vsyncmode as define in the mailbox register definition and c8fvp3_hqvdplite_api_define.h
 */
void c8fvp3_SetVsyncMode(gvh_u32_t base_address, gvh_u32_t vsyncmode);

/*!
 * \brief Do initialization of the next command address
 *
 * \param[in] base_address base address of the IP
 * \param[in] cmd_address address of the next command to process in LMI
 */
extern void c8fvp3_SetCmdAddress(gvh_u32_t base_address, gvh_u32_t cmd_address);

/*!
 * \brief Generate a soft vsync
 *
 * \param[in] base_address base address of the IP
 */
void c8fvp3_GenSoftVsync(gvh_u32_t base_address);

/*!
 * \brief return true when FW ready to process a command
 *
 * \param[in] base_address base address of the IP
 */
gvh_bool_t c8fvp3_IsReady(gvh_u32_t base_address);

/*!
 * \brief return true when a command is pending in next command mode
 *
 * \param[in] base_address base address of the IP
 */
gvh_bool_t c8fvp3_IsCmdPending(gvh_u32_t base_address);

/*!
 * \brief return true when command finished
 *
 * \param[in] base_address base address of the IP
 */
gvh_bool_t c8fvp3_GetEopStatus(gvh_u32_t base_address);

/*!
 * \brief Clear End of Picture status bit
 *
 * \param[in] base_address base address of the IP
 */
void c8fvp3_ClearInfoToHost(gvh_u32_t base_address);

/*!
 * \brief Clear Irq
 *
 * \param[in] base_address base address of the IP
 */
void c8fvp3_ClearIrqToHost(gvh_u32_t base_address);

/*!
 * \brief Set HVSRC Lut according to command settings and filter strength
 *
 * \param[out] FD_HQVDP_Cmd_p command to be process, LUT are set by this function
 * \param[in] filter_strength_yv vertical luma filter strength
 * \param[in] filter_strength_cv vertical chroma filter strength
 * \param[in] filter_strength_yh horizontal luma filter strength
 * \param[in] filter_strength_ch horizontal chroma filter strength
 */
void c8fvp3_SetHvsrcLut(c8fvp3_hqvdplite_api_struct_t *FD_HQVDP_Cmd_p,
                        gvh_u8_t filter_strength_yv,
                        gvh_u8_t filter_strength_cv,
                        gvh_u8_t filter_strength_yh,
                        gvh_u8_t filter_strength_ch);

/*!
 * \brief Set 3D Viewport according to left viewport and Offset3D.
 *  the command provide input viewport size/origin and initphases
 *  all the information related to input and output format must be set before calling this.
 *
 * \param[out] FD_HQVDP_Cmd_p command to be process, 3D viewport are set
 * \param[in]  offset3D is a lateral offset between 2 views, this function is neutral if Offset3D=0
 */
gvh_u32_t c8fvp3_Set3DViewPort(c8fvp3_hqvdplite_api_struct_t *FD_HQVDP_Cmd_p, gvh_s16_t offset3D);

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif

