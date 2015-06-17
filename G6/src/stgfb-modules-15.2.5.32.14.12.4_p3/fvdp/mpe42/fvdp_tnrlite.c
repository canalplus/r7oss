/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_tnr.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_regs.h"
#include "fvdp_common.h"
#include "fvdp_hostupdate.h"

#include "fvdp_tnrlite.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */
const VQ_TNR_Parameters_t TNRDefaultVQTable

    =
{
    {
        sizeof(VQ_TNR_Parameters_t),
        FVDP_TNR,
        0
    },
    1, // EnableGlobalNoiseAdaptive
    1, // EnableGlobalMotionAdaptive
    3, // GlobalMotionStdTh
    4800, // GlobalMotionHighTh
    20, // GlobalMotionPanTh
    -1, // GlobalMotionStd
    -2, // GlobalMotionHigh
    -3, // GlobalMotionPan
    {
        { // 0
            0, 0, 0, 0, 0, 3, 3, 20, 24, 20,
        },
        { // 1
            0, 0, 0, 0, 0, 3, 3, 22, 26, 22,
        },
        { // 2
            0, 0, 0, 0, 0, 3, 3, 24, 28, 24,
        },
        { // 3
            0, 0, 0, 0, 0, 3, 3, 26, 30, 26,
        },
        { // 4
            0, 0, 0, 0, 0, 3, 3, 28, 32, 28,
        },
        { // 5
            0, 0, 0, 0, 0, 3, 3, 30, 34, 30,
        },
        { // 6
            0, 0, 0, 0, 0, 3, 3, 32, 36, 32,
        },
        { // 7
            0, 0, 0, 0, 0, 3, 3, 34, 38, 34,
        },
        { // 8
            0, 0, 0, 0, 0, 3, 3, 36, 40, 36,
        },
        { // 9
        	0, 0, 0, 0, 0, 3, 3, 40, 44, 40,
        },
        { // 10
            0, 0, 0, 0, 0, 3, 3, 44, 48, 44,
        },
        { // 11
            0, 0, 0, 0, 0, 3, 3, 50, 58, 50,
        },
    }
};

/* Private Macros ----------------------------------------------------------- */
#define TNR_ENC_REG32_Write(Addr,Val)              REG32_Write(Addr+TNR_ENC_BASE_ADDRESS,Val)
#define TNR_ENC_REG32_Read(Addr)                   REG32_Read(Addr+TNR_ENC_BASE_ADDRESS)
#define TNR_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+TNR_ENC_BASE_ADDRESS,BitsToSet)
#define TNR_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+TNR_ENC_BASE_ADDRESS,BitsToClear)
#define TNR_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+TNR_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


/* Functions ---------------------------------------------------------------- */

/******************************************************************************
 FUNCTION       : TnrLite_Init
 USAGE          : Initialization function for tnr3, it should be called after power up
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 PRE_CONDITION  : Associated register should be set before calling this routine
 RETURN         : None
******************************************************************************/
void TnrLite_Init (void)
{
    // from Programming model for TNR pup
    TNR_ENC_REG32_Write(TNR3_CTRL, 0x100CD);
    TNR_ENC_REG32_Write(TNR3_CTRL2, 0x2203);
    TNR_ENC_REG32_Write(TNR3_SNR_K_MATH, 0xfff);
    TNR_ENC_REG32_Write(TNR3_Y_NOISE, 0); //    /1
    TNR_ENC_REG32_Write(TNR3_UV_NOISE, 0);//  1
    TNR_ENC_REG32_Write(TNR3_K_LOGIC, 0xE79);
    TNR_ENC_REG32_Write(TNR3_LA_LUT1, 0x108);
    TNR_ENC_REG32_Write(TNR3_LA_LUT2, 0x18a);
    TNR_ENC_REG32_Write(TNR3_FLESHTONE_YMIN, 0xd8);
    TNR_ENC_REG32_Write(TNR3_FLESHTONE_YMAX, 0x320);
    TNR_ENC_REG32_Write(TNR3_FLESHTONE_U, 0x403);
    TNR_ENC_REG32_Write(TNR3_FLESHTONE_V, 0x4c3);
    TNR_ENC_REG32_Write(TNR3_UV_DIF_MAX, 0x16);

    // see Programming model for global noise 1.6
    TNR_ENC_REG32_Write(TNR3_NOISE_THRESH, 0xEB10);
    HostUpdate_SetUpdate_LITE(FVDP_ENC,LITE_HOST_UPDATE_TNR_SYNC,FORCE_UPDATE);
}

/******************************************************************************
 FUNCTION       : TnrLite_Enable
 USAGE          : Enable Tnr, after initialization of changing vq data
 INPUT          : bool LumaOnly - if true, tnr works only on luma channel (not uv)
 OUTPUT         : None
 GLOBALS        : None
 PRE_CONDITION  : Associated register should be set before calling this routine
 RETURN         : None
******************************************************************************/
void TnrLite_Enable(bool LumaOnly)
{
    if (LumaOnly)
    {
        TNR_ENC_REG32_Clear(TNR3_CTRL, TNR3_Y_BYPASS_MASK | TNR3_FLESHTONE_EN_MASK);
        TNR_ENC_REG32_Set(TNR3_CTRL, TNR3_UV_BYPASS_MASK);
    }
    else
    {
        TNR_ENC_REG32_Set(TNR3_CTRL, TNR3_FLESHTONE_EN_MASK);
        TNR_ENC_REG32_Clear(TNR3_CTRL, TNR3_Y_BYPASS_MASK | TNR3_UV_BYPASS_MASK);
    }
    TNR_ENC_REG32_Set(TNR3_CONFIG, TNR3_EN_MASK);
    HostUpdate_SetUpdate_LITE(FVDP_ENC,LITE_HOST_UPDATE_TNR_SYNC,FORCE_UPDATE);
}

/******************************************************************************
 FUNCTION       : TnrLite_Disable
 USAGE          : Disables or suspends tnr operation, bypassing tnr block.
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 PRE_CONDITION  : Associated register should be set before calling this routine
 RETURN         : None
******************************************************************************/
void TnrLite_Disable(void)
{
    TNR_ENC_REG32_Set(TNR3_CTRL, TNR3_Y_BYPASS_MASK | TNR3_UV_BYPASS_MASK);
    TNR_ENC_REG32_Clear(TNR3_CONFIG, TNR3_EN_MASK);
    HostUpdate_SetUpdate_LITE(FVDP_ENC,LITE_HOST_UPDATE_TNR_SYNC,FORCE_UPDATE);
}

/******************************************************************************
 FUNCTION       : TnrLite_SetVqData
 USAGE          : Disables software generation of frame reset, Clears enable bit and mask
 INPUT          : VQ_TNR_Adaptive_Parameters_t *vq_data_p - pointer to vqdata
                      bool LumaOnly - if true bypass chroma chanel
                      uint32_t H_Active - horizontal size
                      uint32_t V_Active - vertical size
 OUTPUT         : None
 GLOBALS        : None
 PRE_CONDITION  : Associated register should be set before calling this routine
 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t TnrLite_SetVqData(VQ_TNR_Adaptive_Parameters_t *vq_data_p, bool LumaOnly, uint32_t H_Active, uint32_t V_Active)
{
    if (vq_data_p == 0)
    {
        vq_data_p = (VQ_TNR_Adaptive_Parameters_t *)&TNRDefaultVQTable.AdaptiveData[0];
    }

    /* SNR related  settings */
    if (vq_data_p->EnableSNR == 0)
    {
        TNR_ENC_REG32_Set(TNR3_CTRL2, TNR3_BYPASS_SNR_Y_MASK|TNR3_BYPASS_SNR_UV_MASK);
    }
    else
    {
        TNR_ENC_REG32_Write(TNR3_SNR_K_MATH,
            ((vq_data_p->SNRLumaSensitivity << TNR3_Y_SNR_K_GRAD_OFFSET) & TNR3_Y_SNR_K_GRAD_MASK) |
            ((vq_data_p->SNRChromaSensitivity << TNR3_UV_SNR_K_GRAD_OFFSET) & TNR3_UV_SNR_K_GRAD_MASK));
        TNR_ENC_REG32_ClearAndSet(TNR3_CTRL, TNR3_Y_H_COEF_SEL_MASK | TNR3_UV_H_COEF_SEL_MASK, 0,\
            ((vq_data_p->SNRLumaStrength << TNR3_Y_H_COEF_SEL_OFFSET) & TNR3_Y_H_COEF_SEL_MASK) |
            ((vq_data_p->SNRChromaStrength << TNR3_UV_H_COEF_SEL_OFFSET) & TNR3_UV_H_COEF_SEL_MASK));

        TNR_ENC_REG32_Clear(TNR3_CTRL2, TNR3_BYPASS_SNR_Y_MASK|TNR3_BYPASS_SNR_UV_MASK);
    }

    /* Set TNR Flesh Tone parameters */
    TNR_ENC_REG32_ClearAndSet(TNR3_GRAD1, TNR3_FLESHTONE_GRAD_MASK, TNR3_FLESHTONE_GRAD_OFFSET, vq_data_p->FleshtoneGrad);
    if (LumaOnly)
    {
        TNR_ENC_REG32_Clear(TNR3_CTRL, TNR3_Y_BYPASS_MASK | TNR3_FLESHTONE_EN_MASK);
        TNR_ENC_REG32_Set(TNR3_CTRL, TNR3_UV_BYPASS_MASK);
    }
    else
    {
        TNR_ENC_REG32_Set(TNR3_CTRL, TNR3_FLESHTONE_EN_MASK);
        TNR_ENC_REG32_Clear(TNR3_CTRL, TNR3_Y_BYPASS_MASK | TNR3_UV_BYPASS_MASK);
    }

    TNR_ENC_REG32_Write(TNR3_K_MIN,
        ((vq_data_p->YKMin << TNR3_Y_K_MIN_OFFSET) & TNR3_Y_K_MIN_MASK) |
        ((vq_data_p->UVKMin << TNR3_UV_K_MIN_OFFSET) & TNR3_UV_K_MIN_MASK));

    TNR_ENC_REG32_ClearAndSet(TNR3_GRAD1, TNR3_LUMA_GRAD_MASK, TNR3_LUMA_GRAD_OFFSET, vq_data_p->YGrad);
    TNR_ENC_REG32_Write(TNR3_GRAD2, vq_data_p->ChromaGrad & TNR3_CHROMA_GRAD_MASK);

    TNR_ENC_REG32_Write(TNR3_IP_HACTIVE, H_Active);
    TNR_ENC_REG32_Write(TNR3_IP_VACTIVE, V_Active);

    HostUpdate_SetUpdate_LITE(FVDP_ENC,LITE_HOST_UPDATE_TNR_SYNC,FORCE_UPDATE);
    return FVDP_OK;
}

