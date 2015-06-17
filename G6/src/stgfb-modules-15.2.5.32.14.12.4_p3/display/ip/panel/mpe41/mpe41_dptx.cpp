/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_dptx.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/DisplayMixer.h>
#include <display/generic/Output.h>

#include "mpe41_dptx.h"
#include "mpe41_dptx_regs.h"

#define DPTX_WAIT(x) (vibe_os_stall_execution (x))
#define DELAY 50000

CSTmMPE41DPTx::CSTmMPE41DPTx(CDisplayDevice *pDev,
                 uint32_t ulDPTxRegs): CSTmDPTx(pDev)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pDevRegs = (uint32_t*)pDev->GetCtrlRegisterBase();
    m_ulDPTxOffset = ulDPTxRegs;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmMPE41DPTx::~CSTmMPE41DPTx() {}

uint32_t CSTmMPE41DPTx::Start(const stm_display_mode_t* pModeLine)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if(pModeLine==NULL)
        return STM_OUT_INVALID_VALUE;
   /*
    * http://mssp.thl.st.com/projects/IP_DEV_STATUS/videoip/
    * VideoAE/Newman%20Cut0%20Validation/01_Scripts/chip_bringup_gdb3_output_stage/
    * DPTx/aa_dptx_01_rgb1080p_turbo_auto.c
    */

    DPTX32_Write(DPTX0_CLK_CTRL, 0x4);
    DPTX32_Write(DPTX0_PLL_CTRL, 0x80);
    DPTX_WAIT(DELAY);
    DPTX32_Write(DPTX0_PLL_CTRL, 0xa4);

    DPTX32_Write(DPTX0_TXPLL_NS, 0x0);
    DPTX32_Write(DPTX_IN_SEL, 0x0);
    DPTX32_Write(DPTX0_CONTROL, 1);
    DPTX32_Write(DPTX0_RESET_CTRL, 0x1f);
    DPTX_WAIT(DELAY);
    DPTX32_Write(DPTX0_RESET_CTRL, 0x0);

    DPTX32_Write(DPTX0_PHY_CTRL, 0xff);
    DPTX32_Write(DPTX0_AUX_PHY_CTRL, 0x60);
    DPTX32_Write(DPTX0_RTERM_CTRL, 0x00000083);

    DPTX_WAIT(DELAY);
    DPTX32_Read(DPTX0_RCAL_RESULTS);

    DPTX32_Write(DPTX0_PHYFIFO_CTRL, 0x00);
    DPTX32_Write(DPTX0_L0_GAIN, 0x00000028);
    DPTX32_Write(DPTX0_L0_GAIN1, 0x00000040);
    DPTX32_Write(DPTX0_L0_GAIN2, 0x00000055);
    DPTX32_Write(DPTX0_L0_GAIN3, 0x00000080);
    DPTX32_Write(DPTX0_L0_PRE_EMPH, 0x00000000);
    DPTX32_Write(DPTX0_L0_PRE_EMPH1, 0x00000010);
    DPTX32_Write(DPTX0_L0_PRE_EMPH2, 0x00000021);
    DPTX32_Write(DPTX0_L0_PRE_EMPH3, 0x00000045);
    DPTX32_Write(DPTX0_PLL_CLK_MEAS_CTRL, 0x00000001);
    DPTX32_Write(DPTX0_PHYA_IRQ_STATUS, 0x00000003);
    DPTX32_Write(DPTX0_AUX_CONTROL, 0x20);
    DPTX32_Write(DPTX0_AUX_CLK_DIV, 0x65);
    DPTX32_Write(DPTX0_PHY_CTRL, 0x00);

    DPTX_WAIT(DELAY);
    DPTX32_Read(DPTX0_PHYA_IRQ_STATUS);

    DPTX32_Write(DPTX0_LINK_CONTROL, 0xf);
    DPTX32_Write(DPTX0_LT_CONTROL, 0x4);
    DPTX32_Write(DPTX0_LL_LINK_BW_SET, 0x0c);
    DPTX32_Write(DPTX0_LL_LANE_COUNT_SET, 0x84);
    DPTX32_Write(DPTX0_L0_PATTERN_CTRL, 0x1);
    DPTX32_Write(DPTX0_L1_PATTERN_CTRL, 0x1);
    DPTX32_Write(DPTX0_L2_PATTERN_CTRL, 0x1);
    DPTX32_Write(DPTX0_L3_PATTERN_CTRL, 0x1);
    DPTX32_Write(DPTX0_LL_TRAINING_L0_SET, 0xa);
    DPTX32_Write(DPTX0_LL_TRAINING_L1_SET, 0xa);
    DPTX32_Write(DPTX0_LL_TRAINING_L2_SET, 0xa);
    DPTX32_Write(DPTX0_LL_TRAINING_L3_SET, 0xa);
    DPTX32_Write(DPTX0_LL_TRAINING_PATTERN_SET, 0x01);
    DPTX32_Write(DPTX0_SYM_GEN_CTRL, 0x0);
    DPTX32_Write(DPTX0_SYM_GEN0, 0x155);
    DPTX32_Write(DPTX0_SYM_GEN1, 0x155);
    DPTX32_Write(DPTX0_SYM_GEN2, 0x154);
    DPTX32_Write(DPTX0_SYM_GEN3, 0x154);
    DPTX_WAIT(DELAY);

    DPTX32_Write(DPTX0_LL_TRAINING_PATTERN_SET, 0x02);
    DPTX32_Write(DPTX0_MS_FORMAT_0, 0x41);
    DPTX32_Write(DPTX0_MS_FORMAT_1, 0x00);
    DPTX32_Write(DPTX0_MS_LTA_CTRL, 0x27);
    DPTX32_Write(DPTX0_MS_LTA_CTRL, 0x22);
    DPTX32_Write(DPTX0_TPG_CTRL, 0x41);

    DPTX32_Write(DPTX0_MS_HTOTAL, pModeLine->mode_timing.pixels_per_line);
    DPTX32_Write(DPTX0_MS_HACT_START, pModeLine->mode_params.active_area_start_pixel);
    DPTX32_Write(DPTX0_MS_HACT_WIDTH, pModeLine->mode_params.active_area_width);
    DPTX32_Write(DPTX0_MS_HSYNC_WIDTH, pModeLine->mode_timing.hsync_width);
    DPTX32_Write(DPTX0_MS_VTOTAL, pModeLine->mode_timing.lines_per_frame);
    DPTX32_Write(DPTX0_MS_VACT_START, pModeLine->mode_params.active_area_start_line);
    DPTX32_Write(DPTX0_MS_VACT_WIDTH, pModeLine->mode_params.active_area_height);
    DPTX32_Write(DPTX0_MS_VSYNC_WIDTH, pModeLine->mode_timing.vsync_width);
    DPTX32_Write(DPTX0_MS_M, 0x002c);
    DPTX32_Write(DPTX0_MS_N, 0x0030);
    DPTX32_Write(DPTX0_MS_CTRL, 0x01);
    DPTX32_Write(DPTX0_MS_UPDATE, 0x01);
    DPTX_WAIT(DELAY);

    DPTX32_Write(DPTX0_LINK_CONTROL, 0x2f);
    DPTX32_Write(DPTX0_SCRAMBLER_SEED, 0xfffE);
    DPTX32_Write(DPTX0_LL_TRAINING_PATTERN_SET, 0x00);

    DPTX_WAIT(DELAY);
    DPTX32_Read(DPTX0_LT_STATUS);

    DPTX32_Write(DPTX0_L0_PATTERN_CTRL, 0x5);
    DPTX32_Write(DPTX0_L1_PATTERN_CTRL, 0x5);
    DPTX32_Write(DPTX0_L2_PATTERN_CTRL, 0x5);
    DPTX32_Write(DPTX0_L3_PATTERN_CTRL, 0x5);
    DPTX32_Write(DPTX0_MS_IDP_DATA_LN_CNT, 0x4);
    DPTX32_Write(DPTX0_MS_IDP_PHY_LN_CNT, 0x4);
    DPTX32_Write(DPTX0_MS_IDP_PACK_CTRL, 0x4);
    DPTX32_Write(DPTX0_MS_UPDATE, 0x1);

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return STM_OUT_OK;
}
