/********************************************************************************
 * COPYRIGHT (C) STMicroelectronics 2007.
 *
 * File name   : c8fvp3_hqrie_api_defines.h $Revision: 1420 $
 *
 * Last modification done by $Author: arabh $ on $Date:: 2007-10-23 $
 * 
 ********************************************************************************/

/*!
 * \file c8fvp3_hqvdplite_api_defines.h
 * \brief  Included api defines that are shared between the firmawre and host
 * firmware
 *
 */

/* Define to prevent recursive inclusion */
#ifndef _HQVDPLITE_API_DEFINES_H_
#define _HQVDPLITE_API_DEFINES_H_

/* Exported defines ----------------------------------------------------------- */

#define FORMAT_2DTV  (0)
#define FORMAT_3DTV_SBS (1)
#define FORMAT_3DTV_TAB (2)
#define FORMAT_3DTV_FP  (3)
#define DOLBY_BASE_LAYER (4)
#define DOLBY_BASE_ENHANCED_LAYER (5)
 
#define CHROMA_SAMPLING_420 (0)
#define CHROMA_SAMPLING_422 (1)
#define CHROMA_SAMPLING_444 (2)
#define CHROMA_SAMPLING_NO_ALPHA_444 (3)

#define CHROMA_SAMPLING_MASK  (0x00000003)
#define CHROMA_SAMPLING_SHIFT (0x00000000)

#define SINGLE_NOT_DUAL_BUFFER_MASK  (0x00000004)
#define SINGLE_NOT_DUAL_BUFFER_SHIFT (0x00000002)

#define MACROBLOCK_NOT_RASTER_MASK  (0x00000008)
#define MACROBLOCK_NOT_RASTER_SHIFT (0x00000003)

#define TOP_CONFIG_NB_OF_FIELD_3 (0x0)
#define TOP_CONFIG_NB_OF_FIELD_4 (0x1)
#define TOP_CONFIG_NB_OF_FIELD_5 (0x2)

#define CSDI_CONFIG_MAIN_MODE_BYPASS        (0x0)
#define CSDI_CONFIG_MAIN_MODE_FIELD_MERGING (0x1)
#define CSDI_CONFIG_MAIN_MODE_DEI           (0x2)
#define CSDI_CONFIG_MAIN_MODE_LFM           (0x3)

#define CSDI_CONFIG_LUMA_INTERP_VERTICAL    (0x0)
#define CSDI_CONFIG_LUMA_INTERP_DIRECTIONAL (0x1)
#define CSDI_CONFIG_LUMA_INTERP_MEDIAN      (0x2)
#define CSDI_CONFIG_LUMA_INTERP_3D          (0x3)

#define CSDI_CONFIG_CHROMA_INTERP_VERTICAL    (0x0)
#define CSDI_CONFIG_CHROMA_INTERP_DIRECTIONAL (0x1)
#define CSDI_CONFIG_CHROMA_INTERP_MEDIAN      (0x2)
#define CSDI_CONFIG_CHROMA_INTERP_3D          (0x3)

#define CSDI_CONFIG_MD_MODE_OFF   (0x0)
#define CSDI_CONFIG_MD_MODE_INIT  (0x1)
#define CSDI_CONFIG_MD_MODE_LOW   (0x2)
#define CSDI_CONFIG_MD_MODE_FULL  (0x3)
#define CSDI_CONFIG_MD_MODE_SHORT (0x4)

/* FMD block size in luma frame pixels */
#define FMD_BLK_HEIGHT   (40)
#define FMD_BLK_WIDTH    (40)

#define IQI_LE_LUT_WIDTH (64)

/* mask for FW status in INFO_XP70 register */
#define mailbox_INFO_XP70_FW_STATUS_MASK (0x0000E000)

/* meaning of mailbox INFO_XP70 register. not defined in hardware */
#define mailbox_INFO_XP70_FW_READY_MASK (0x00008000)
#define mailbox_INFO_XP70_FW_READY_SHIFT (15)

#define mailbox_INFO_XP70_FW_PROCESSING_MASK (0x00004000)
#define mailbox_INFO_XP70_FW_PROCESSING_SHIFT (14)

#define mailbox_INFO_XP70_FW_INITQUEUES_MASK (0x00002000)
#define mailbox_INFO_XP70_FW_INITQUEUES_SHIFT (13)

/* this is legacy for INITQUEUES|READY */
#define mailbox_INFO_XP70_FW_LOAD_CMD_MASK (0x00000A000)

/* meaning of mailbox INFO_HOST register. not defined in hardware */
#define mailbox_INFO_HOST_FW_EOP_MASK  (0x00000001)
#define mailbox_INFO_HOST_FW_EOP_SHIFT (0)

/* Vsync mode selection*/
#define mailbox_SOFT_VSYNC_VSYNC_MODE_HW (0x00000000)
#define mailbox_SOFT_VSYNC_VSYNC_MODE_SW_CMD (0x00000001)
#define mailbox_SOFT_VSYNC_VSYNC_MODE_SW_CTRL (0x00000003)

#define CRC_RESET_VALUE (0xffffffff)

#define HQVDP_PROFILE_SAMPLE_SIZE (96)
#define PROFILE_ITERNUM_MIN_VALUE (32)
typedef struct {
#ifndef HQVDPLITE_API_FOR_STAPI
  gvh_u32_t loadcmd;
  gvh_u32_t initqueues;
  gvh_u32_t endprocessing;
  gvh_u32_t intervsync;
  gvh_u32_t initcmd;
  gvh_u32_t initviewport;
  gvh_u32_t initpicture;
  gvh_u32_t allocate;
  gvh_u32_t initstr;
  gvh_u16_t HQVDPLITE_processcycle[HQVDP_PROFILE_SAMPLE_SIZE];
  gvh_u16_t HQVDPLITE_waitingcycle[HQVDP_PROFILE_SAMPLE_SIZE];
  gvh_u16_t HQVDPLITE_stbusrdcycle[HQVDP_PROFILE_SAMPLE_SIZE];
} s_PROFILE;
#else
  U32 loadcmd;
  U32 initqueues;
  U32 endprocessing;
  U32 intervsync;
  U32 initcmd;
  U32 initviewport;
  U32 initpicture;
  U32 allocate;
  U32 initstr;
  U16 HQVDPLITE_processcycle[HQVDP_PROFILE_SAMPLE_SIZE];
  U16 HQVDPLITE_waitingcycle[HQVDP_PROFILE_SAMPLE_SIZE];
  U16 HQVDPLITE_stbusrdcycle[HQVDP_PROFILE_SAMPLE_SIZE];
} HQVDPLITE_Profile_t;
#endif

#endif 
