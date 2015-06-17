/***********************************************************************
 *
 * File: display/ip/tvout/stmvip.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_VIP_H
#define _STM_VIP_H

#include <stm_display.h>

#include <display/ip/displaytiming/stmvtg.h>

typedef enum tvo_vip_sync_id_e
{
    TVO_VIP_SYNC_DENC_IDX
  , TVO_VIP_SYNC_SDDCS_IDX
  , TVO_VIP_SYNC_SDVTG_IDX
  , TVO_VIP_SYNC_TTXT_IDX
  , TVO_VIP_SYNC_HDF_IDX
  , TVO_VIP_SYNC_HDDCS_IDX
  , TVO_VIP_SYNC_HDMI_IDX
  , TVO_VIP_SYNC_DVO_IDX
  , TVO_VIP_SYNC_NBR
} tvo_vip_sync_id_t;

#define TVO_VIP_SYNC_TYPE_DENC              (1L << TVO_VIP_SYNC_DENC_IDX)
#define TVO_VIP_SYNC_TYPE_SDDCS             (1L << TVO_VIP_SYNC_SDDCS_IDX)
#define TVO_VIP_SYNC_TYPE_SDVTG             (1L << TVO_VIP_SYNC_SDVTG_IDX)
#define TVO_VIP_SYNC_TYPE_TTXT              (1L << TVO_VIP_SYNC_TTXT_IDX)
#define TVO_VIP_SYNC_TYPE_HDF               (1L << TVO_VIP_SYNC_HDF_IDX)
#define TVO_VIP_SYNC_TYPE_HDDCS             (1L << TVO_VIP_SYNC_HDDCS_IDX)
#define TVO_VIP_SYNC_TYPE_HDMI              (1L << TVO_VIP_SYNC_HDMI_IDX)
#define TVO_VIP_SYNC_TYPE_DVO               (1L << TVO_VIP_SYNC_DVO_IDX)


typedef enum tvo_vip_input_color_channel_e
{
  /*
   * These are deliberately ordered to match the hardware programming values
   */
    TVO_VIP_Y_G
  , TVO_VIP_CB_B
  , TVO_VIP_CR_R
} tvo_vip_input_color_channel_t;


typedef enum tvo_vip_video_source_e
{
    TVO_VIP_MAIN_VIDEO
  , TVO_VIP_AUX_VIDEO
  , TVO_VIP_DENC123
  , TVO_VIP_DENC456
  , TVO_VIP_MAIN_FILTERED_422
} tvo_vip_video_source_t;


typedef enum tvo_vip_sync_source_e
{
    TVO_VIP_MAIN_SYNCS
  , TVO_VIP_AUX_SYNCS
} tvo_vip_sync_source_t;


typedef enum tvo_vip_capabilities_e
{
    TVO_VIP_BYPASS_INPUT_IS_RGB         = (1L<<0) // The pre-formatter bypass input is RGB data not YUV (converted channel is therefore YUV)
  , TVO_VIP_HAS_MAIN_FILTERED_422_INPUT = (1L<<1) // Supports input from a main pre-formatter 422 adaptive filter
  , TVO_VIP_INPUT_IS_INVERTED           = (1L<<2) // The pre-formatter output is inverted
} tvo_vip_capabilities_t;


class CSTmVIP
{
public:
  CSTmVIP(CDisplayDevice               *pDev,
          uint32_t                      ulVIPRegs,
          uint32_t                      ulSyncType,
          const stm_display_sync_id_t  *sync_sel_map,
          uint32_t                      caps = 0);

  virtual ~CSTmVIP(void);

  void SoftReset(void);
  bool SetInputParams(const tvo_vip_video_source_t, uint32_t format, const stm_display_signal_range_t);
  bool SetForceColor(const bool EnableFC, uint32_t R_Cr, uint32_t G_Y, uint32_t B_Cb);
  void SetColorChannelOrder(tvo_vip_input_color_channel_t red_output,
                            tvo_vip_input_color_channel_t green_output,
                            tvo_vip_input_color_channel_t blue_output);

  uint32_t GetCapabilities(void) const { return m_ulCapabilities; }

  void SelectSync(const tvo_vip_sync_source_t);

protected:
  uint32_t *m_pDevRegs;
  uint32_t  m_ulVIPRegOffset;
  uint32_t  m_ulCapabilities;
  uint32_t  m_ulSyncType;
  uint32_t  m_ulVipCfg;
  uint32_t  m_ulRChannel;
  uint32_t  m_ulGChannel;
  uint32_t  m_ulBChannel;


  const stm_display_sync_id_t   *m_sync_sel_map;

  void WriteReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_ulVIPRegOffset + reg), val); }
  uint32_t ReadReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_ulVIPRegOffset + reg)); }
  /* bit field Setting*/
  void SetBitField(uint32_t & Value, int Offset, int Width, uint32_t bitFieldValue)
  {
    const uint32_t BIT_FIELD_MASK = (((uint32_t)0xffffffff << (Offset+Width)) |
                                    ~((uint32_t)0xffffffff << Offset));
    Value &= BIT_FIELD_MASK;
    Value |= ((bitFieldValue << Offset) & ~BIT_FIELD_MASK);
  }

private:
  CSTmVIP(const CSTmVIP&);
  CSTmVIP& operator=(const CSTmVIP&);

  static const char *source_names[];
};


#endif //_STM_VIP_H
