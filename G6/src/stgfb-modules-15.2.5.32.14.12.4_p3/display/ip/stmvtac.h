/***********************************************************************
 *
 * File: display/ip/stmvtac.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMVTAC_H
#define _STMVTAC_H

#include <stm_display_output.h>
#include <vibe_os.h>

#define STM_VTAC_VSYNC_DELAY  (-6)
#define STM_VTAC_HSYNC_DELAY  (-6)

#define ABSOLUTE_ADDRESS(Offset, reg) (m_pDevRegs + ((Offset+reg)>>2))

typedef enum stm_display_vtac_id_e
{
    STM_VTAC_MAIN
  , STM_VTAC_AUX
} stm_display_vtac_id_t;

typedef enum stm_display_vtac_mode_e
{
    STM_VTAC_MODE_MAIN_RGB_14_445500
  , STM_VTAC_MODE_MAIN_RGB_12_445500
  , STM_VTAC_MODE_MAIN_RGB_12_371250
  , STM_VTAC_MODE_MAIN_RGB_10_371250
  , STM_VTAC_MODE_MAIN_RGB_10_297000
  , STM_VTAC_MODE_DVO_12_445500
  , STM_VTAC_MODE_DVO_10_445500
  , STM_VTAC_MODE_DVO_10_371250
  , STM_VTAC_MODE_DVO_8_371250
  , STM_VTAC_MODE_AUX_10_121500
  , STM_VTAC_MODE_AUX_10_114750
  , STM_VTAC_MODE_AUX_10_108000
  , STM_VTAC_MODE_AUX_8_101250
  , STM_VTAC_MODE_AUX_8_94500
  , STM_VTAC_MODE_AUX_8_87750

  ,STM_VTAC_MODE_UNKNOWN_MODE
} stm_display_vtac_mode_t;


typedef struct stm_display_vtac_params_s
{
  bool  even_parity_en;
  bool  odd_parity_en;
  bool  alpha_en;
  bool  gfx_en;
  bool  vid_en;
} stm_display_vtac_params_t;

typedef struct stm_display_vtac_phy_params_s
{
  uint32_t  cfg0;
  uint32_t  cfg1;
} stm_display_vtac_phy_params_t;

class CSTmVTAC
{
public:
  CSTmVTAC (CDisplayDevice       *pDev,
            stm_display_vtac_id_t ID,
            uint32_t              syscfg_video_base,
            uint32_t              vtac_tx_offset,
            uint32_t              vtac_rx_offset);

  virtual ~CSTmVTAC (void);

  bool Start (const stm_display_vtac_mode_t Mode);
  void Stop  (void);

  virtual bool ConfigVTACPHYParams(void);
  bool SetVTACParams (const stm_display_vtac_params_t  );
  bool GetVTACParams (stm_display_vtac_params_t * const );

protected:
  stm_display_vtac_id_t  m_ID;
  uint32_t *m_pDevRegs;
  uint32_t  m_uSYSCFGVideoOffset;
  uint32_t  m_uTXOffset;
  uint32_t  m_uRXOffset;

  stm_display_vtac_params_t     m_VTAC_params;
  stm_display_vtac_mode_t       m_PendingModeIndex;
  stm_display_vtac_mode_t       m_CurrentModeIndex;

  bool ProgramVTACSettings(void);

  /* SYS CFG VIDEO VTAC Read/Write methodes */
  uint32_t ReadSYSCFGVideoReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uSYSCFGVideoOffset+reg)); }
  void WriteSYSCFGVideoReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uSYSCFGVideoOffset+reg), val); }

  /* TX VTAC Read/Write methodes */
  uint32_t ReadTXReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uTXOffset+reg)); }
  void WriteTXReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uTXOffset+reg), val); }
  /* RX VTAC Read/Write methodes */
  uint32_t ReadRXReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uRXOffset+reg)); }
  void WriteRXReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uRXOffset+reg), val); }

private:

  static const stm_display_vtac_phy_params_t *m_pVTACPHYCFGOffset;
  CSTmVTAC(const CSTmVTAC&);
  CSTmVTAC& operator=(const CSTmVTAC&);
};

#endif /* _STMVTAC_H */
