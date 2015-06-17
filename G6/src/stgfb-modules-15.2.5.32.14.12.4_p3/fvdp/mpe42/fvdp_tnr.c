/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_tnr.c
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
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
#include "fvdp_system.h"
#include "fvdp_tnr.h"

/* Private Macros ----------------------------------------------------------- */
#define TNR_REG32_Write(Addr,Val)              REG32_Write(Addr+TNR_FE_BASE_ADDRESS,Val)
#define TNR_REG32_Read(Addr)                   REG32_Read(Addr+TNR_FE_BASE_ADDRESS)
#define TNR_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+TNR_FE_BASE_ADDRESS,BitsToSet)
#define TNR_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+TNR_FE_BASE_ADDRESS,BitsToClear)
#define TNR_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+TNR_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

/* Private Structure -------------------------------------------------------- */
typedef enum TnrCtrlFlagType_e
{
    TNR_PASSTHRU_ENABLE     = BIT0,        // TNR block pass through (TNR Disable)
    TNR_PASSTHRU_DISABLE    = BIT1,        // Use TNR block, (TNR Enable)
} TnrCtrlFlagType_t;
/* Private Variables (static)------------------------------------------------ */
TnrCtrlFlagType_t TnrCtrlflag = 0;

/* Functions ---------------------------------------------------------------- */

/******************************************************************************
 FUNCTION       : Tnr_Init
 USAGE          : Initialization function for tnr, it should be called after power up
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Tnr_Init (FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(HandleContext_p->CH);
    pHW_PSIConfig->HWPSIState[FVDP_TNR] = FVDP_PSI_OFF;

    TNR_REG32_Set(TNR_CTRL,TNR_FE_TNR_EN_MASK);
    TNR_REG32_Set(TNR_CTRL,TNR_READ_ACTIVE_MASK);

    TNR_REG32_Clear(TNR_CTRL,TNR_PASSTHRU_Y_MASK);
    TNR_REG32_Clear(TNR_CTRL,TNR_PASSTHRU_UV_MASK);
    TNR_REG32_Clear(TNR_CTRL,TNR_WINDOW_EN_MASK);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}

/******************************************************************************
 FUNCTION       : Tnr_Enable
 USAGE          : Enable Tnr, after initialization of changing vq data
 INPUT          : bool InterlacedInput -True if input is interlaced, false otherwise
                 uint32_t H_Active - horizontal input size
                 uint32_t V_Active -vertical input size (input to TNR block, progressive)
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Tnr_Enable(bool InterlacedInput, uint32_t H_Active, uint32_t V_Active)
{
    //TNR_REG32_Clear(TNR_CTRL,TNR_FE_TNR_EN_MASK);

    if(TnrCtrlflag & TNR_PASSTHRU_DISABLE) // already enable
    {
        return;
    }

    if (InterlacedInput)
    {
        TNR_REG32_Clear(TNR_SNR_1,TNR_SNR_PROG_FLAG_MASK);
    }
    else
    {
        TNR_REG32_Set(TNR_SNR_1,TNR_SNR_PROG_FLAG_MASK);
    }
    TNR_REG32_Write(TNR_IP_HACTIVE,H_Active);
    TNR_REG32_Write(TNR_IP_VACTIVE,V_Active);

    TnrCtrlflag |= TNR_PASSTHRU_DISABLE; // Use TNR block, (TNR Enable)

    TNR_REG32_Set(TNR_CTRL,TNR_FE_TNR_EN_MASK);

    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}

/******************************************************************************
 FUNCTION       : Tnr_Bypas
 USAGE          : Disables or suspends tnr operation, bypassing tnr block.
 INPUT          :
                  uint32_t H_Active - horizontal input size
                  uint32_t V_Active -vertical input size (input to TNR block, progressive)
 OUTPUT         : None
 GLOBALS        : None
 RETURN         :  None
******************************************************************************/
void Tnr_Bypas (uint32_t H_Active, uint32_t V_Active)
{
    if(TnrCtrlflag & TNR_PASSTHRU_ENABLE) // already disable
    {
        return;
    }

    TNR_REG32_Write(TNR_IP_HACTIVE, H_Active);
    TNR_REG32_Write(TNR_IP_VACTIVE, V_Active);

    TnrCtrlflag |= TNR_PASSTHRU_ENABLE; // TNR block pass through (TNR Disable)

    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Tnr_UpdateTnrControlRegister(void)
 USAGE          : Update Tnr Control Register at next input update
                  after call Tnr_Bypas() or Tnr_Enable
 INPUT          : void
 OUTPUT         : FVDP_ERROR : Require Re-configure TNR
                  FVDP_OK
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
FVDP_Result_t Tnr_UpdateTnrControlRegister(void)
{
    if((TnrCtrlflag & TNR_PASSTHRU_DISABLE) && (TnrCtrlflag & TNR_PASSTHRU_ENABLE))
    {
        // TNR enable and disable are call at same input update.
        //Re-configure TNR
        TnrCtrlflag &= ~(TNR_PASSTHRU_DISABLE|TNR_PASSTHRU_ENABLE);

        return FVDP_ERROR;
    }
    if(TnrCtrlflag & TNR_PASSTHRU_DISABLE)  // Use TNR block (TNR Enable)
    {
        TNR_REG32_Clear(TNR_CTRL,TNR_PASSTHRU_Y_MASK|TNR_PASSTHRU_UV_MASK);
        TnrCtrlflag &= (~TNR_PASSTHRU_DISABLE);
    }
    else if(TnrCtrlflag & TNR_PASSTHRU_ENABLE)  // TNR block pass through (TNR Disable)
    {
        TNR_REG32_Set(TNR_CTRL,TNR_PASSTHRU_Y_MASK|TNR_PASSTHRU_UV_MASK);
        TnrCtrlflag &= (~TNR_PASSTHRU_ENABLE);
    }

    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : Tnr_Demo
 USAGE          : Defines are where TNR is applied, other area (demo) is bypassed.
 INPUT          : uint32_t H_Start - horizontal start position of the window
                 uint32_t H_End - horizontal end position of the window
                 uint32_t V_Start - vertical start position of the window
                 uint32_t V_End - vertical end position of the window
                 bool enable - enable (TRUE) or disable demo mode
 OUTPUT         : None
 GLOBALS        : None
 RETURN         :  None
******************************************************************************/
void Tnr_Demo(uint32_t H_Start, uint32_t H_End, uint32_t V_Start, uint32_t V_End, bool enable)
{
    TNR_REG32_ClearAndSet(TNR_WINDOW_X,TNR_X_START_MASK, TNR_X_START_OFFSET, H_Start);
    TNR_REG32_ClearAndSet(TNR_WINDOW_X,TNR_X_END_MASK, TNR_X_END_OFFSET, H_Start);
    TNR_REG32_ClearAndSet(TNR_WINDOW_Y,TNR_Y_START_MASK, TNR_Y_START_OFFSET, H_End);
    TNR_REG32_ClearAndSet(TNR_WINDOW_Y,TNR_Y_END_MASK, TNR_Y_END_OFFSET, V_End);
    if (enable) {
        TNR_REG32_Set(TNR_CTRL,TNR_WINDOW_EN_MASK);
    } else {
        TNR_REG32_Clear(TNR_CTRL,TNR_WINDOW_EN_MASK);
    }
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}

/******************************************************************************
 FUNCTION       : Tnr_SetVqData
 USAGE          : Disables software generation of frame reset, Clears enable bit and mask
 INPUT          : VQ_TNR_Adaptive_Parameters_t *vq_data_p - pointer to vqdata
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t Tnr_SetVqData(const VQ_TNR_Advanced_Parameters_t *vq_data_p)
{
    if (vq_data_p == 0)
        return FVDP_ERROR;
    TNR_REG32_Clear(TNR_CTRL,TNR_READ_ACTIVE_MASK);

    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_1, TNR_TNRK_GAIN_0_MASK, TNR_TNRK_GAIN_0_OFFSET, vq_data_p->TNR_TNRK_GAIN_0);//   Byte    0   255 80  Motion gain for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_1, TNR_TNRK_GAIN_1_MASK, TNR_TNRK_GAIN_1_OFFSET,    vq_data_p->TNR_TNRK_GAIN_1);// Byte    0   255 80  Motion gain for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_1, TNR_TNRK_GAIN_2_MASK, TNR_TNRK_GAIN_2_OFFSET,    vq_data_p->TNR_TNRK_GAIN_2);// Byte    0   255 80  Motion gain for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_1, TNR_TNRK_GAIN_3_MASK, TNR_TNRK_GAIN_3_OFFSET,   vq_data_p->TNR_TNRK_GAIN_3);// Byte    0   255 80  Motion gain for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_2, TNR_TNRK_LIMIT_0_MASK, TNR_TNRK_LIMIT_0_OFFSET,    vq_data_p->TNR_TNRK_LIMIT_0);//    2 bytes 0   1024    750 K limit for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_2, TNR_TNRK_LIMIT_1_MASK, TNR_TNRK_LIMIT_1_OFFSET,   vq_data_p->TNR_TNRK_LIMIT_1);//    2 bytes 0   1024    100 K limit for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_3, TNR_TNRK_LIMIT_2_MASK, TNR_TNRK_LIMIT_2_OFFSET,   vq_data_p->TNR_TNRK_LIMIT_2);//    2 bytes 0   1024    900 K limit for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_3, TNR_TNRK_LIMIT_3_MASK, TNR_TNRK_LIMIT_3_OFFSET,   vq_data_p->TNR_TNRK_LIMIT_3);//    2 bytes 0   1024    100 K limit for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_6, TNR_TNRK_GAIN_UV_0_MASK, TNR_TNRK_GAIN_UV_0_OFFSET,   vq_data_p->TNR_TNRK_GAIN_UV_0);//  Byte    0   255 80  Motion gain for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_6, TNR_TNRK_GAIN_UV_1_MASK, TNR_TNRK_GAIN_UV_1_OFFSET,  vq_data_p->TNR_TNRK_GAIN_UV_1);//  Byte    0   255 80  Motion gain for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_6, TNR_TNRK_GAIN_UV_2_MASK, TNR_TNRK_GAIN_UV_2_OFFSET,    vq_data_p->TNR_TNRK_GAIN_UV_2);//  Byte    0   255 80  Motion gain for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_6, TNR_TNRK_GAIN_UV_3_MASK, TNR_TNRK_GAIN_UV_3_OFFSET,    vq_data_p->TNR_TNRK_GAIN_UV_3);//  Byte    0   255 80  Motion gain for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_7, TNR_TNRK_LIMIT_UV_0_MASK, TNR_TNRK_LIMIT_UV_0_OFFSET,   vq_data_p->TNR_TNRK_LIMIT_UV_0);// 2 bytes 0   1023    750 K limit for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_7, TNR_TNRK_LIMIT_UV_1_MASK, TNR_TNRK_LIMIT_UV_1_OFFSET,    vq_data_p->TNR_TNRK_LIMIT_UV_1);// 2 bytes 0   1023    100 K limit for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_8, TNR_TNRK_LIMIT_UV_2_MASK, TNR_TNRK_LIMIT_UV_2_OFFSET,    vq_data_p->TNR_TNRK_LIMIT_UV_2);// 2 bytes 0   1023    900 K limit for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_8, TNR_TNRK_LIMIT_UV_3_MASK, TNR_TNRK_LIMIT_UV_3_OFFSET,   vq_data_p->TNR_TNRK_LIMIT_UV_3);// 2 bytes 0   1023    100 K limit for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_5, TNR_TNRK_M2_GAIN_MASK, TNR_TNRK_M2_GAIN_OFFSET,   vq_data_p->TNR_TNRK_M2_GAIN);//    Byte    0   255 16  Extra gain on top for M2 case: i.e the pixel is identified as noise
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_5, TNR_TNRK_MULT_M1_MASK, TNR_TNRK_MULT_M1_OFFSET,    vq_data_p->TNR_TNRK_MULT_M1);//    Byte    0   3   2   Noise gain1 under M1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_4, TNR_TNRK_GAIN_M1_MASK, TNR_TNRK_GAIN_M1_OFFSET,    vq_data_p->TNR_TNRK_GAIN_M1);//    Byte    0   31  16  Noise gain2 under M1 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_4, TNR_TNRK_OFFSET_M1_MASK, TNR_TNRK_OFFSET_M1_OFFSET,    vq_data_p->TNR_TNRK_OFFSET_M1);//  2 bytes -255    255 -12 Noise offset under M1 case for Y
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_9, TNR_TNRK_OFFSET_UV_M1_MASK, TNR_TNRK_OFFSET_UV_M1_OFFSET,    vq_data_p->TNR_TNRK_OFFSET_UV_M1);//   2 bytes -255    255 -18 Noise offset under M1 case for UV
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_5, TNR_TNRK_MULT_M2_MASK, TNR_TNRK_MULT_M2_OFFSET,   vq_data_p->TNR_TNRK_MULT_M2);//    Byte    0   3   2   Noise gain1 under M2 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_4, TNR_TNRK_GAIN_M2_MASK, TNR_TNRK_GAIN_M2_OFFSET, vq_data_p->TNR_TNRK_GAIN_M2);//    Byte    0   31  16  Noise gain2 under M2 case
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_4, TNR_TNRK_OFFSET_M2_MASK, TNR_TNRK_OFFSET_M2_OFFSET,    vq_data_p->TNR_TNRK_OFFSET_M2);//  2 bytes -255    255 -12 Noise offset under M2 case for Y
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_9, TNR_TNRK_OFFSET_UV_M2_MASK, TNR_TNRK_OFFSET_UV_M2_OFFSET,   vq_data_p->TNR_TNRK_OFFSET_UV_M2);//   2 bytes -255    255 -18 Noise offset under M2 case for UV
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_4, TNR_TNRK_CLIP_LUT_MASK, TNR_TNRK_CLIP_LUT_OFFSET,    vq_data_p->TNR_TNRK_CLIP_LUT);//   Bool            0   Clamp K to limit
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_5, TNR_TNRK_FIRST_LIMIT_MASK, TNR_TNRK_FIRST_LIMIT_OFFSET,    vq_data_p->TNR_TNRK_FIRST_LIMIT);//    2 bytes 0   1023    950 Limit the motion->K LUT
    TNR_REG32_ClearAndSet(TNR_TNR_KLOGIC_5, TNR_TNRK_SECOND_LIMIT_MASK, TNR_TNRK_SECOND_LIMIT_OFFSET,    vq_data_p->TNR_TNRK_SECOND_LIMIT);//   2 bytes 0   1023    850 Start position for a smooth transition to limit in the motion->K LUT

    TNR_REG32_ClearAndSet(TNR_CTRL ,TNR_KLOGIC_LPF_SEL_MASK ,TNR_KLOGIC_LPF_SEL_OFFSET ,  vq_data_p->TNR_KLOGIC_LPF_SEL);//  1 bytes 0   2   2   Final filter for K between current and previous
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_1 ,TNR_STNRK_GAIN_0_MASK ,TNR_STNRK_GAIN_0_OFFSET,    vq_data_p->TNR_STNRK_GAIN_0);//    1 byte  0   255 80  Motion gain for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_1 ,TNR_STNRK_GAIN_1_MASK ,TNR_STNRK_GAIN_1_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_1);//    1 byte  0   255 80  Motion gain for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_1 ,TNR_STNRK_GAIN_2_MASK ,TNR_STNRK_GAIN_2_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_2);//    1 byte  0   255 255 Motion gain for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_1 ,TNR_STNRK_GAIN_3_MASK ,TNR_STNRK_GAIN_3_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_3);//    1 byte  0   255 255 Motion gain for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_2 ,TNR_STNRK_LIMIT_0_MASK ,TNR_STNRK_LIMIT_0_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_0);//   1 byte  0   255 255 K limit for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_2 ,TNR_STNRK_LIMIT_1_MASK ,TNR_STNRK_LIMIT_1_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_1);//   1 byte  0   255 255 K limit for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_2 ,TNR_STNRK_LIMIT_2_MASK ,TNR_STNRK_LIMIT_2_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_2);//   1 byte  0   255 0   K limit for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_2 ,TNR_STNRK_LIMIT_3_MASK ,TNR_STNRK_LIMIT_3_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_3);//   1 byte  0   255 255 K limit for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_5 ,TNR_STNRK_GAIN_UV_0_MASK ,TNR_STNRK_GAIN_UV_0_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_UV_0);// 1 byte  0   255 80  Motion gain for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_5 ,TNR_STNRK_GAIN_UV_1_MASK ,TNR_STNRK_GAIN_UV_1_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_UV_1);// 1 byte  0   255 80  Motion gain for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_5 ,TNR_STNRK_GAIN_UV_2_MASK ,TNR_STNRK_GAIN_UV_2_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_UV_2);// 1 byte  0   255 255 Motion gain for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_5 ,TNR_STNRK_GAIN_UV_3_MASK ,TNR_STNRK_GAIN_UV_3_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_UV_3);// 1 byte  0   255 255 Motion gain for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_6 ,TNR_STNRK_LIMIT_UV_0_MASK ,TNR_STNRK_LIMIT_UV_0_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_UV_0);//    1 byte  0   255 255 K limit for highnoise=0 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_6 ,TNR_STNRK_LIMIT_UV_1_MASK ,TNR_STNRK_LIMIT_UV_1_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_UV_1);//    1 byte  0   255 255 K limit for highnoise=0 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_6 ,TNR_STNRK_LIMIT_UV_2_MASK ,TNR_STNRK_LIMIT_UV_2_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_UV_2);//    1 byte  0   255 0   K limit for highnoise=1 and fleshtone=0 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_6 ,TNR_STNRK_LIMIT_UV_3_MASK ,TNR_STNRK_LIMIT_UV_3_OFFSET ,   vq_data_p->TNR_STNRK_LIMIT_UV_3);//    1 byte  0   255 255 K limit for highnoise=1 and fleshtone=1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_4 ,TNR_STNRK_M2_GAIN_MASK , TNR_STNRK_M2_GAIN_OFFSET,   vq_data_p->TNR_STNRK_M2_GAIN);//   Byte    0   255 16  Extra gain on top for M2 case: i.e the pixel is identified as noise
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_3 ,TNR_STNRK_GAIN_M1_MASK ,TNR_STNRK_GAIN_M1_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_M1);//   Byte    0   31  32  Noise gain1 under M1 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_3 ,TNR_STNRK_OFFSET_M1_MASK ,TNR_STNRK_OFFSET_M1_OFFSET ,   vq_data_p->TNR_STNRK_OFFSET_M1);// 2 bytes -255    255 -12 Noise offset under M1 case for Y
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_7 ,TNR_STNRK_OFFSET_UV_M1_MASK ,TNR_STNRK_OFFSET_UV_M1_OFFSET ,   vq_data_p->TNR_STNRK_OFFSET_UV_M1);//  2 bytes -255    255 -10 Noise offset under M1 case for UV
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_3 ,TNR_STNRK_GAIN_M2_MASK ,TNR_STNRK_GAIN_M2_OFFSET ,   vq_data_p->TNR_STNRK_GAIN_M2);//   Byte    0   31  32  Noise gain1 under M2 case
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_3 ,TNR_STNRK_OFFSET_M2_MASK ,TNR_STNRK_OFFSET_M2_OFFSET ,   vq_data_p->TNR_STNRK_OFFSET_M2);// 2 bytes -255    255 -12 Noise offset under M2 case for Y
    TNR_REG32_ClearAndSet(TNR_STNR_KLOGIC_7 ,TNR_STNRK_OFFSET_UV_M2_MASK ,TNR_STNRK_OFFSET_UV_M2_OFFSET ,   vq_data_p->TNR_STNRK_OFFSET_UV_M2);//  2 bytes -255    255 -10 Noise offset under M2 case for UV

    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_EN_MASK ,TNR_SNR_EN_OFFSET ,   vq_data_p->TNR_SNR_en);//  Bool            1   Enable SNR luma noise reduction
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_EN_MASK ,TNR_SNR_UV_EN_OFFSET ,   vq_data_p->TNR_SNR_UV_EN);//   Bool            0   Enable SNR Chroma noise reduction
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_MED_EN_MASK ,TNR_SNR_MED_EN_OFFSET ,   vq_data_p->TNR_SNR_MED_EN);//  Bool            1   Enable Median filter in Luma channel
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_MED_BIT_MASK ,TNR_SNR_MED_BIT_OFFSET ,   vq_data_p->TNR_SNR_MED_BIT);// Bool            1   Select the direction of Luma Median filter
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_DIAG_DISP_FLAG_MASK ,TNR_SNR_DIAG_DISP_FLAG_OFFSET ,   vq_data_p->TNR_SNR_DIAG_DISP_FLAG);//  Bool            0   Enable disable diagonal median.
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_FORCE_THRESHOLD_MASK ,TNR_SNR_FORCE_THRESHOLD_OFFSET ,   vq_data_p->TNR_SNR_FORCE_THRESHOLD);// Bool            0   Force correlated averager coefficient
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_SIGMA_P_MASK ,TNR_SNR_SIGMA_P_OFFSET ,   vq_data_p->TNR_SNR_SIGMA_P);// Byte    0   31  0   Forced correlated averager coefficient Sigma_p
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_SIGMA_N_MASK ,TNR_SNR_SIGMA_N_OFFSET ,   vq_data_p->TNR_SNR_SIGMA_N);// Byte    0   31  0   Forced correlated averager coefficient Sigma_n
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_DITHER_ENABLE_MASK ,TNR_SNR_DITHER_ENABLE_OFFSET ,   vq_data_p->TNR_SNR_DITHER_ENABLE);//   Bool            0   Luma dithering enable
    TNR_REG32_ClearAndSet(TNR_SNR_1 ,TNR_SNR_NO_BITS_MASK ,TNR_SNR_NO_BITS_OFFSET ,   vq_data_p->TNR_SNR_NO_BITS);// Bool            2   Luma dithering bit select
    TNR_REG32_ClearAndSet(TNR_SNR_2 ,TNR_SNR_NOISE_THRES_BASED_MASK ,TNR_SNR_NOISE_THRES_BASED_OFFSET ,   vq_data_p->TNR_SNR_NOISE_THRES_BASED);//   Byte    0   255 0   Base noise in Luma Median filter. If system smaller than this value, force Noise_low_thr to 0
    TNR_REG32_ClearAndSet(TNR_SNR_2 ,TNR_SNR_BIG_THRESHOLD_MASK ,TNR_SNR_BIG_THRESHOLD_OFFSET ,   vq_data_p->TNR_SNR_BIG_THRESHOLD);//   Byte    0   255 80  Big threshold to determine the direction. When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    TNR_REG32_ClearAndSet(TNR_SNR_2 ,TNR_SNR_LOW_THRESHOLD_MASK ,TNR_SNR_LOW_THRESHOLD_OFFSET ,   vq_data_p->TNR_SNR_LOW_THRESHOLD);//   Byte    0   255 4   Low threshold to determine the flat region. It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_LPF_SEL_MASK ,TNR_SNR_UV_LPF_SEL_OFFSET ,   vq_data_p->TNR_SNR_UV_LPF_SEL);//  bool            1   Chroma low pass filter coefficient select
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_DIV_SEL_MASK ,TNR_SNR_UV_DIV_SEL_OFFSET ,   vq_data_p->TNR_SNR_UV_DIV_SEL);//  bool            1   Chroma differential data low pass filter coefficient select
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_BIT_SEL_MASK ,TNR_SNR_UV_BIT_SEL_OFFSET ,   vq_data_p->TNR_SNR_UV_BIT_SEL);//  1 byte  0   7   0   Chroma bit select
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_OFFSET_MASK ,TNR_SNR_UV_OFFSET_OFFSET ,   vq_data_p->TNR_SNR_UV_OFFSET);//   1 byte  0   63  5   SNR chroma K logic offset
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_GAIN_MASK ,TNR_SNR_UV_GAIN_OFFSET ,   vq_data_p->TNR_SNR_UV_GAIN);// 1 byte  0   127 60  SNR chroma K logic gain
    TNR_REG32_ClearAndSet(TNR_SNR_3 ,TNR_SNR_UV_LIMIT_MASK ,TNR_SNR_UV_LIMIT_OFFSET ,   vq_data_p->TNR_SNR_UV_LIMIT);//    1 byte  0   255 128 SNR chroma K logic limit
    TNR_REG32_ClearAndSet(TNR_SNR_4 ,TNR_SNR_UV_BIG_THRESHOLD_MASK ,TNR_SNR_UV_BIG_THRESHOLD_OFFSET ,   vq_data_p->TNR_SNR_UV_BIG_THRESHOLD);//    2 bytes 0   1023    325 Big threshold to determine the direction. When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    TNR_REG32_ClearAndSet(TNR_SNR_4 ,TNR_SNR_UV_LOW_THRESHOLD_MASK ,TNR_SNR_UV_LOW_THRESHOLD_OFFSET ,   vq_data_p->TNR_SNR_UV_LOW_THRESHOLD);//    2 bytes 0   1023    50  Low threshold to determine the flat region. It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_BITS_MASK ,TNR_MDET_LUMA_BITS_OFFSET ,   vq_data_p->TNR_MDET_LUMA_BITS);//      Bool            1   Select luma adaptive control data path bits
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_GAIN_MASK ,TNR_MDET_LUMA_GAIN_OFFSET ,   vq_data_p->TNR_MDET_LUMA_GAIN);//  Byte    0   63  20  Special gain for low luma area
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_GAIN_SEL_MASK ,TNR_MDET_LUMA_GAIN_SEL_OFFSET ,   vq_data_p->TNR_MDET_LUMA_GAIN_SEL);//  Bool            1   Luma calculation block weighted or not
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_BIT_SEL_MASK ,TNR_MDET_LUMA_BIT_SEL_OFFSET ,   vq_data_p->TNR_MDET_LUMA_BIT_SEL);//   Byte    0   7   2   Division fact for luma calculation
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_CTL_GAIN_MASK ,TNR_MDET_LUMA_CTL_GAIN_OFFSET ,   vq_data_p->TNR_MDET_LUMA_CTL_GAIN);//  Byte    0   15  1   Luma calculation gain
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LUMA_LOW_THRES_MASK ,TNR_MDET_LUMA_LOW_THRES_OFFSET ,   vq_data_p->TNR_MDET_LUMA_LOW_THRES);// Byte    0   63  0   Low luma threshold
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_MEDIAN_SELECT_MASK ,TNR_MDET_MEDIAN_SELECT_OFFSET ,   vq_data_p->TNR_MDET_MEDIAN_SELECT);//  Byte    0   3   3   Luma motion median filter select
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_AVG_SEL_MASK ,TNR_MDET_AVG_SEL_OFFSET ,   vq_data_p->TNR_MDET_AVG_SEL);//    Bool            0   MOT_1 (motion case) average method select
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_NC_WINDOW_SIZE_MASK ,TNR_MDET_NC_WINDOW_SIZE_OFFSET ,   vq_data_p->TNR_MDET_NC_WINDOW_SIZE);// Bool            9   MOT_2(noise case) window size select
    TNR_REG32_ClearAndSet(TNR_MDETECT_2 ,TNR_MDET_MOT2_RND_EN_MASK ,TNR_MDET_MOT2_RND_EN_OFFSET ,   vq_data_p->TNR_MDET_MOT2_RND_EN);//    Bool            0   Enable rounding in MOT_2 path
    TNR_REG32_ClearAndSet(TNR_MDETECT_2 ,TNR_MDET_MOT2_SHIFT_MASK ,TNR_MDET_MOT2_SHIFT_OFFSET ,   vq_data_p->TNR_MDET_MOT2_SHIFT);// Byte    0   3   0   Left shift for MOT_2 path
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_LPF_EN_MASK ,TNR_MDET_LPF_EN_OFFSET ,   vq_data_p->TNR_MDET_LPF_EN);// Bool            1   Enable final low pass filter for motion data
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_ND_ENABLE_MASK ,TNR_MDET_ND_ENABLE_OFFSET ,   vq_data_p->TNR_MDET_ND_ENABLE);//      Bool            1   Enable Noise Discriminator
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_ND_FORCE_FLAG_MASK  ,TNR_MDET_ND_FORCE_FLAG_OFFSET ,   vq_data_p->TNR_MDET_ND_FORCE_FLAG);//  Bool            0   Force noise discriminator output to noise instead of motion
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_ND_PRECISION_MASK ,TNR_MDET_ND_PRECISION_OFFSET ,   vq_data_p->TNR_MDET_ND_PRECISION);//   Bool            1   Noise Discriminator precision
    TNR_REG32_ClearAndSet(TNR_MDETECT_1 ,TNR_MDET_ND_CNT_THRES_MASK ,TNR_MDET_ND_CNT_THRES_OFFSET ,   vq_data_p->TNR_MDET_ND_CNT_THRES);//   Byte    0   7   3   Noise Discriminator counter threshold
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_U_MIN_MASK ,TNR_FT_U_MIN_OFFSET ,   vq_data_p->TNR_FT_u_min);//    Byte    0   255 7   Define the flesh tone U region minimum
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_U_MAX_MASK ,TNR_FT_U_MAX_OFFSET ,   vq_data_p->TNR_FT_u_max);//    Byte    0   255 30  Define the flesh tone U region maximum
    TNR_REG32_ClearAndSet(TNR_FLESHTONE_2 ,TNR_FT_V_MIN_MASK ,TNR_FT_V_MIN_OFFSET ,   vq_data_p->TNR_FT_v_min);//    Byte    0   255 9   Define the flesh tone V region minimum
    TNR_REG32_ClearAndSet(TNR_FLESHTONE_2 ,TNR_FT_V_MAX_MASK ,TNR_FT_V_MAX_OFFSET ,   vq_data_p->TNR_FT_v_max);//    Byte    0   255 32  Define the flesh tone V region maximum
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_UV_DIFF_MAX_MASK ,TNR_FT_UV_DIFF_MAX_OFFSET ,   vq_data_p->TNR_FT_uv_diff_max);//  Byte    0   255 26  Define the UV difference maximum threshold
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_LPF_BYPASS_MASK ,TNR_FT_LPF_BYPASS_OFFSET ,   vq_data_p->TNR_FT_lpf_bypass);//   Bool            0   Enable bypass low pass filter for fleshtone
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_Y_FIX_EN_MASK ,TNR_FT_Y_FIX_EN_OFFSET ,   vq_data_p->TNR_FT_Y_FIX_EN);// Bool            0   Enable fleshtone detection on the luma path
    TNR_REG32_ClearAndSet(TNR_FLESTONE_1 ,TNR_FT_UV_FIX_EN_MASK ,TNR_FT_UV_FIX_EN_OFFSET ,   vq_data_p->TNR_FT_UV_FIX_EN);//    Bool            0   Enable fleshtone detection on th chroma path
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_ENABLE_MASK ,TNR_EDGE_ENABLE_OFFSET ,   vq_data_p->TNR_EDGE_ENABLE);// Bool            1   Enable edge detection
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_TAPS_MASK ,TNR_EDGE_TAPS_OFFSET ,   vq_data_p->TNR_EDGE_TAPS);//   Bool            3   Select edge detection taps
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_DIFF_SEL_MASK ,TNR_EDGE_DIFF_SEL_OFFSET ,   vq_data_p->TNR_EDGE_DIFF_SEL);//   Bool            2   Max control for luma difference
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_GAIN_M1_MASK ,TNR_EDGE_GAIN_M1_OFFSET ,   vq_data_p->TNR_Edge_gain_m1);//    Byte    0   63  11  M1 case edge gain
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_GAIN_M2_MASK ,TNR_EDGE_GAIN_M2_OFFSET ,   vq_data_p->TNR_Edge_gain_m2);//    Byte    0   63  11  M2 case edge gain
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_EXP_EN_MASK ,TNR_EDGE_EXP_EN_OFFSET ,   vq_data_p->TNR_Edge_exp_en);// Bool            1   Enable expansion filtering
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_EXP_TAP_MASK ,TNR_EDGE_EXP_TAP_OFFSET ,   vq_data_p->TNR_Edge_exp_tap);//    Bool            0   Number of taps for exp filtering
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_LPF_EN_MASK ,TNR_EDGE_LPF_EN_OFFSET ,   vq_data_p->TNR_Edge_lpf_en);// Bool            3   Enable low pass filter (when expansion filtering is disabled)
    TNR_REG32_ClearAndSet(TNR_EDGE ,TNR_EDGE_LPF_TAPS_MASK ,TNR_EDGE_LPF_TAPS_OFFSET ,   vq_data_p->TNR_EDGE_LPF_TAPS);//   Bool            0   Low pass filter tap selection
    TNR_REG32_ClearAndSet(TNR_DITHER ,TNR_DITHER_BYPASS_MASK ,TNR_DITHER_BYPASS_OFFSET ,   vq_data_p->TNR_DITHER_BYPASS);//   Bool            1   Bypass anti dithering filter
    TNR_REG32_ClearAndSet(TNR_DITHER ,TNR_DITHER_K_MASK ,TNR_DITHER_K_OFFSET ,   vq_data_p->TNR_DITHER_K);//    2 Byte  0   1023    128 Coefficient for anti dithering filter
    TNR_REG32_ClearAndSet(TNR_STICKY_LOGIC ,TNR_STICKY_ENABLE_MASK ,TNR_STICKY_ENABLE_OFFSET ,   vq_data_p->TNR_STICKY_ENABLE);//   Bool            1   Sticky pixel control
    TNR_REG32_ClearAndSet(TNR_STICKY_LOGIC ,TNR_STICKY_THRES_Y_MASK ,TNR_STICKY_THRES_Y_OFFSET ,   vq_data_p->TNR_STICKY_THRES_Y);//  Byte    0   15  0   Threshold for sticky pixel control in Y channel. Filtered difference smaller than this value will be considered as sticky pixel
    TNR_REG32_ClearAndSet(TNR_STICKY_LOGIC ,TNR_STICKY_THRES_UV_MASK ,TNR_STICKY_THRES_UV_OFFSET ,   vq_data_p->TNR_STICKY_THRES_UV);// Byte    0   15  0   Threshold for sticky pixel control in UV channel.
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);

    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : Tnr_LoadDefaultVqData
 USAGE          : Loads default vq data to the hardware registers
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : void
******************************************************************************/
void Tnr_LoadDefaultVqData(void)
{
    VQ_TNR_Advanced_Parameters_t TnrDefault;

    TnrDefault.TNR_TNRK_GAIN_0 = 80;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_TNRK_GAIN_1 = 80;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_TNRK_GAIN_2 = 80;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_TNRK_GAIN_3 = 80;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_TNRK_LIMIT_0 = 750;//	2 bytes	0	1024	750	K limit for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_TNRK_LIMIT_1 = 100;//	2 bytes	0	1024	100	K limit for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_TNRK_LIMIT_2 = 900;//	2 bytes	0	1024	900	K limit for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_TNRK_LIMIT_3 = 100;//	2 bytes	0	1024	100	K limit for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_TNRK_GAIN_UV_0 = 80;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_TNRK_GAIN_UV_1 = 80;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_TNRK_GAIN_UV_2 = 80;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_TNRK_GAIN_UV_3 = 80;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_TNRK_LIMIT_UV_0 = 750;//	2 bytes	0	1023	750	K limit for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_TNRK_LIMIT_UV_1 = 100;//	2 bytes	0	1023	100	K limit for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_TNRK_LIMIT_UV_2 = 900;//	2 bytes	0	1023	900	K limit for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_TNRK_LIMIT_UV_3 = 100;//	2 bytes	0	1023	100	K limit for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_TNRK_M2_GAIN = 16;//	Byte	0	255	16	Extra gain on top for M2 case: i.e the pixel is identified as noise
    TnrDefault.TNR_TNRK_MULT_M1 = 2;//	Byte	0	3	2	Noise gain1 under M1 case
    TnrDefault.TNR_TNRK_GAIN_M1 = 16;//	Byte	0	31	16	Noise gain2 under M1 case
    TnrDefault.TNR_TNRK_OFFSET_M1 = -12;//	2 bytes	-255	255	-12	Noise offset under M1 case for Y
    TnrDefault.TNR_TNRK_OFFSET_UV_M1 = -18;//	2 bytes	-255	255	-18	Noise offset under M1 case for UV
    TnrDefault.TNR_TNRK_MULT_M2 =  2;//	Byte	0	3	2	Noise gain1 under M2 case
    TnrDefault.TNR_TNRK_GAIN_M2 =  16;//	Byte	0	31	16	Noise gain2 under M2 case
    TnrDefault.TNR_TNRK_OFFSET_M2 = -12;//	2 bytes	-255	255	-12	Noise offset under M2 case for Y
    TnrDefault.TNR_TNRK_OFFSET_UV_M2 = -18;//	2 bytes	-255	255	-18	Noise offset under M2 case for UV
    TnrDefault.TNR_TNRK_CLIP_LUT =  0;//	Bool	 	 	0	Clamp K to limit
    TnrDefault.TNR_TNRK_FIRST_LIMIT = 950;//	2 bytes	0	1023	950	Limit the motion->K LUT
    TnrDefault.TNR_TNRK_SECOND_LIMIT = 850;//	2 bytes	0	1023	850	Start position for a smooth transition to limit in the motion->K LUT
    TnrDefault.TNR_KLOGIC_LPF_SEL = 2;//	1 bytes	0	2	2	Final filter for K between current and previous
    TnrDefault.TNR_STNRK_GAIN_0 = 80;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_STNRK_GAIN_1 = 80;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_STNRK_GAIN_2 = 255;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_STNRK_GAIN_3 = 255;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_STNRK_LIMIT_0 = 255;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_STNRK_LIMIT_1 = 255;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_STNRK_LIMIT_2 = 0;//	1 byte	0	255	0	K limit for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_STNRK_LIMIT_3 = 255;//	1 byte	0	255	255	K limit for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_STNRK_GAIN_UV_0 = 80;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_STNRK_GAIN_UV_1 = 80;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_STNRK_GAIN_UV_2 = 255;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_STNRK_GAIN_UV_3 = 255;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_STNRK_LIMIT_UV_0 = 255;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=0 case
    TnrDefault.TNR_STNRK_LIMIT_UV_1 = 255;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=1 case
    TnrDefault.TNR_STNRK_LIMIT_UV_2 = 0;//	1 byte	0	255	0	K limit for highnoise=1 and fleshtone=0 case
    TnrDefault.TNR_STNRK_LIMIT_UV_3 = 255;//	1 byte	0	255	255	K limit for highnoise=1 and fleshtone=1 case
    TnrDefault.TNR_STNRK_M2_GAIN = 16;//	Byte	0	255	16	Extra gain on top for M2 case: i.e the pixel is identified as noise
    TnrDefault.TNR_STNRK_GAIN_M1 = 31;//	Byte	0	31	32	Noise gain1 under M1 case
    TnrDefault.TNR_STNRK_OFFSET_M1 = -12;//	2 bytes	-255	255	-12	Noise offset under M1 case for Y
    TnrDefault.TNR_STNRK_OFFSET_UV_M1 = -10;//	2 bytes	-255	255	-10	Noise offset under M1 case for UV
    TnrDefault.TNR_STNRK_GAIN_M2 = 31;//	Byte	0	31	32	Noise gain1 under M2 case
    TnrDefault.TNR_STNRK_OFFSET_M2 = -12;//	2 bytes	-255	255	-12	Noise offset under M2 case for Y
    TnrDefault.TNR_STNRK_OFFSET_UV_M2 = -10;//	2 bytes	-255	255	-10	Noise offset under M2 case for UV
    TnrDefault.TNR_SNR_en = 1;//	Bool	 	 	1	Enable SNR luma noise reduction
    TnrDefault.TNR_SNR_UV_EN = 0;//	Bool	 	 	0	Enable SNR Chroma noise reduction
    TnrDefault.TNR_SNR_MED_EN = 1;//	Bool	 	 	1	Enable Median filter in Luma channel
    TnrDefault.TNR_SNR_MED_BIT = 1;//	Bool	 	 	1	Select the direction of Luma Median filter
    TnrDefault.TNR_SNR_DIAG_DISP_FLAG = 0;//	Bool	 	 	0	Enable disable diagonal median.
    TnrDefault.TNR_SNR_FORCE_THRESHOLD = 0;//	Bool	 	 	0	Force correlated averager coefficient
    TnrDefault.TNR_SNR_SIGMA_P = 0;//	Byte	0	31	0	Forced correlated averager coefficient Sigma_p
    TnrDefault.TNR_SNR_SIGMA_N = 0;//	Byte	0	31	0	Forced correlated averager coefficient Sigma_n
    TnrDefault.TNR_SNR_DITHER_ENABLE = 0;//	Bool	 	 	0	Luma dithering enable
    TnrDefault.TNR_SNR_NO_BITS = 0;//	Bool	 	 	2	Luma dithering bit select  0-2bits, 1-4bits
    TnrDefault.TNR_SNR_NOISE_THRES_BASED = 0;//	Byte	0	255	0	Base noise in Luma Median filter.If system smaller than this value, force Noise_low_thr to 0
    TnrDefault.TNR_SNR_BIG_THRESHOLD = 80;//	Byte	0	255	80	Big threshold to determine the direction.When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    TnrDefault.TNR_SNR_LOW_THRESHOLD = 4;//	Byte	0	255	4	Low threshold to determine the flat region.It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    TnrDefault.TNR_SNR_UV_LPF_SEL = 1;//	bool	 	 	1	Chroma low pass filter coefficient select
    TnrDefault.TNR_SNR_UV_DIV_SEL = 1;//	bool	 	 	1	Chroma differential data low pass filter coefficient select
    TnrDefault.TNR_SNR_UV_BIT_SEL = 0;//	1 byte	0	7	0	Chroma bit select
    TnrDefault.TNR_SNR_UV_OFFSET = 5;//	1 byte	0	63	5	SNR chroma K logic offset
    TnrDefault.TNR_SNR_UV_GAIN = 60;//	1 byte	0	127	60	SNR chroma K logic gain
    TnrDefault.TNR_SNR_UV_LIMIT = 128;//	1 byte	0	255	128	SNR chroma K logic limit
    TnrDefault.TNR_SNR_UV_BIG_THRESHOLD = 325;//	2 bytes	0	1023	325	Big threshold to determine the direction.When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    TnrDefault.TNR_SNR_UV_LOW_THRESHOLD = 50;//	2 bytes	0	1023	50	Low threshold to determine the flat region.It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    TnrDefault.TNR_MDET_LUMA_BITS = 1;//     	Bool	 	 	1	Select luma adaptive control data path bits
    TnrDefault.TNR_MDET_LUMA_GAIN = 20;// 	Byte	0	63	20	Special gain for low luma area
    TnrDefault.TNR_MDET_LUMA_GAIN_SEL = 1;// 	Bool	 	 	1	Luma calculation block weighted or not
    TnrDefault.TNR_MDET_LUMA_BIT_SEL = 2;//  	Byte	0	7	2	Division fact for luma calculation
    TnrDefault.TNR_MDET_LUMA_CTL_GAIN = 1;//	Byte	0	15	1	Luma calculation gain
    TnrDefault.TNR_MDET_LUMA_LOW_THRES = 0;//	Byte	0	63	0	Low luma threshold
    TnrDefault.TNR_MDET_MEDIAN_SELECT = 3;//	Byte	0	3	3	Luma motion median filter select
    TnrDefault.TNR_MDET_AVG_SEL = 0;//	Bool	 	 	0	MOT_1 (motion case) average method select
    TnrDefault.TNR_MDET_NC_WINDOW_SIZE = 0;//	Bool	 	 	9	MOT_2(noise case) window size select (0->9, 1->15)
    TnrDefault.TNR_MDET_MOT2_RND_EN = 0;//	Bool	 	 	0	Enable rounding in MOT_2 path
    TnrDefault.TNR_MDET_MOT2_SHIFT = 0;//	Byte	0	3	0	Left shift for MOT_2 path
    TnrDefault.TNR_MDET_LPF_EN = 1;//	Bool	 	 	1	Enable final low pass filter for motion data
    TnrDefault.TNR_MDET_ND_ENABLE = 1;//    	Bool	 	 	1	Enable Noise Discriminator
    TnrDefault.TNR_MDET_ND_FORCE_FLAG = 0;//	Bool	 	 	0	Force noise discriminator output to noise instead of motion
    TnrDefault.TNR_MDET_ND_PRECISION = 1;//	Bool	 	 	1	Noise Discriminator precision
    TnrDefault.TNR_MDET_ND_CNT_THRES = 3;//	Byte	0	7	3	Noise Discriminator counter threshold
    TnrDefault.TNR_FT_u_min = 7;//	Byte	0	255	7	Define the flesh tone U region minimum
    TnrDefault.TNR_FT_u_max = 30;//	Byte	0	255	30	Define the flesh tone U region maximum
    TnrDefault.TNR_FT_v_min = 9;//	Byte	0	255	9	Define the flesh tone V region minimum
    TnrDefault.TNR_FT_v_max = 32;//	Byte	0	255	32	Define the flesh tone V region maximum
    TnrDefault.TNR_FT_uv_diff_max = 26;//	Byte	0	255	26	Define the UV difference maximum threshold
    TnrDefault.TNR_FT_lpf_bypass = 0;//	Bool	 	 	0	Enable bypass low pass filter for fleshtone
    TnrDefault.TNR_FT_Y_FIX_EN = 0;//	Bool	 	 	0	Enable fleshtone detection on the luma path
    TnrDefault.TNR_FT_UV_FIX_EN = 0;//	Bool	 	 	0	Enable fleshtone detection on th chroma path
    TnrDefault.TNR_EDGE_ENABLE = 1;//	Bool	 	 	1	Enable edge detection
    TnrDefault.TNR_EDGE_TAPS = 0;//	Bool	 	 	3	Select edge detection taps 0-3 1-5
    TnrDefault.TNR_EDGE_DIFF_SEL = 2;//	Bool	 	 	2	Max control for luma difference
    TnrDefault.TNR_Edge_gain_m1 = 11;//	Byte	0	63	11	M1 case edge gain
    TnrDefault.TNR_Edge_gain_m2 = 11;//	Byte	0	63	11	M2 case edge gain
    TnrDefault.TNR_Edge_exp_en = 1;//	Bool	 	 	1	Enable expansion filtering
    TnrDefault.TNR_Edge_exp_tap = 0;//	Bool	 	 	0	Number of taps for exp filtering
    TnrDefault.TNR_Edge_lpf_en = 3;//	Bool	 	 	3	Enable low pass filter (when expansion filtering is disabled)
    TnrDefault.TNR_EDGE_LPF_TAPS = 0;//	Bool	 	 	0	Low pass filter tap selection
    TnrDefault.TNR_DITHER_BYPASS = 1;//	Bool	 	 	1	Bypass anti dithering filter
    TnrDefault.TNR_DITHER_K = 128;//	2 Byte	0	1023	128	Coefficient for anti dithering filter
    TnrDefault.TNR_STICKY_ENABLE = 1;//	Bool	 	 	1	Sticky pixel control
    TnrDefault.TNR_STICKY_THRES_Y = 0;//	Byte	0	15	0	Threshold for sticky pixel control in Y channel.Filtered difference smaller than this value will be considered as sticky pixel
    TnrDefault.TNR_STICKY_THRES_UV = 0;//	Byte	0	15	0	Threshold for sticky pixel control in UV channel.

    Tnr_SetVqData(&TnrDefault);
}
