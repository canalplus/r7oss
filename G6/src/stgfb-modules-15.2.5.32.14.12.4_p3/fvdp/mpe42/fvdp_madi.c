/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_madi.c
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
#include "fvdp_madi.h"

/* Private Macros ----------------------------------------------------------- */
#define MADI_REG32_Write(Addr,Val)              REG32_Write(Addr+MADI_FE_BASE_ADDRESS,Val)
#define MADI_REG32_Read(Addr)                   REG32_Read(Addr+MADI_FE_BASE_ADDRESS)
#define MADI_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+MADI_FE_BASE_ADDRESS,BitsToSet)
#define MADI_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+MADI_FE_BASE_ADDRESS,BitsToClear)
#define MADI_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+MADI_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define DEINT_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DEINT_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

/* Functions ---------------------------------------------------------------- */

/******************************************************************************
 FUNCTION       : Madi_Init
 USAGE          : Initialization function for madi, it should be called after power up
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Madi_Init (void)
{
    MADI_REG32_Write(MADI_FILTER_CTRL1, 0x18811104);
    MADI_REG32_Write(MADI_FILTER_CTRL3, 0x18811104);
    MADI_REG32_Write(MADI_FILTER_CTRL4, 0x08001800);
    MADI_REG32_Write(MADI_FILTER_CTRL5, 0x0000000D);
    MADI_REG32_Write(MADI_FILTER_CTRL6, 0x00000000);
    MADI_REG32_Write(MADI_FILTER_CTRL7, 0x03FF0040);
    MADI_REG32_Write(MADI_FILTER_CTRL8, 0x006003FF);
    MADI_REG32_Write(MADI_FILTER_CTRL9, 0x03FF0140);
    MADI_REG32_Write(MADI_FILTER_CTRL10, 0x00000000);
    MADI_REG32_Write(MADI_FILTER_CTRL11, 0x00000000);
    MADI_REG32_Write(MADI_FILTER_CTRL12, 0x03FF03FF);
    MADI_REG32_Write(MADI_FILTER_CTRL13, 0x03FF03FF);

    MADI_REG32_Write(MADI_EXT_NOISE,0);// MADI_EXT_NOISE	1 byte	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL,MADI_MOTION_NOISE_GAIN_MASK,MADI_MOTION_NOISE_GAIN_OFFSET, 0x7);
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL,MADI_NOISE_GAIN_MASK,MADI_NOISE_GAIN_OFFSET,1);//MADI_NOISE_GAIN	1 byte	1

    //MADI_LPF_SHIFT	1 byte	10b
     MADI_REG32_ClearAndSet(MADI_NOISE_CTRL,MADI_LPF_SHIFT_MASK, MADI_LPF_SHIFT_OFFSET,0x2);

    //MADI_NOISE_CLAMP	1 byte	0
     MADI_REG32_ClearAndSet(MADI_NOISE_CTRL,MADI_NOISE_CLAMP_MASK, MADI_NOISE_CLAMP_OFFSET,0);

    //MADI_EXT_NOISE_EN bool	1
     MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, MADI_EXT_NOISE_EN_MASK, MADI_EXT_NOISE_EN_OFFSET,1);

    //MADI_NOISE_METER_SEL	bool	0
     MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, MADI_NOISE_METER_SEL_MASK, MADI_NOISE_METER_SEL_OFFSET,0);

    //MADI_NOISE_FULL_WINDOW	bool	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, MADI_NOISE_FULL_WINDOW_MASK, MADI_NOISE_FULL_WINDOW_OFFSET,0);

    //MADI_NOISE_WIN_SEL	bool	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, MADI_NOISE_WIN_SEL_MASK, MADI_NOISE_WIN_SEL_OFFSET,0);

    //RESET_NOISE_METER bool	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, RESET_NOISE_METER_MASK, RESET_NOISE_METER_OFFSET,0);

    //RESET_NOISE_MEAS	bool	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, RESET_NOISE_MEAS_MASK, RESET_NOISE_MEAS_OFFSET,0);

    //NMEAS_EN	bool	1
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, NMEAS_EN_MASK, NMEAS_EN_OFFSET,1);

    //NMETER_EN	bool	1
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, NMETER_EN_MASK, NMETER_EN_OFFSET,1);

    //NMEAS_Y_P_SEL	bool	0
    MADI_REG32_ClearAndSet(MADI_NOISE_CTRL, NMEAS_Y_P_SEL_MASK, NMEAS_Y_P_SEL_OFFSET,0);

    //MADI_NOISE_THRESH_MIN	1 byte	0x10
    MADI_REG32_ClearAndSet(MADI_NOISE_THRESH, MADI_NOISE_THRESH_MIN_MASK, MADI_NOISE_THRESH_MIN_OFFSET,0x10);

    //MADI_NOISE_THRESH_MAX	1 byte	0xeb
    MADI_REG32_ClearAndSet(MADI_NOISE_THRESH, MADI_NOISE_THRESH_MAX_MASK, MADI_NOISE_THRESH_MAX_OFFSET,0xEB);


    //MADI_EXT_NOISE =0 (This register could be updated by global noise programming model frame by frame.)
    MADI_REG32_Write(MADI_EXT_NOISE,0);

    //This setting will use madi external noise.
    //Madi_stathist_reset_en	0
     MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1, MADI_STATHIST_RESET_EN_MASK, MADI_STATHIST_RESET_EN_OFFSET,0);

    //Madi_stathist_reset_val	2
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL2, MADI_STATHIST_RESET_VAL_MASK, MADI_STATHIST_RESET_VAL_OFFSET,2);

    //MADI_LMVD_LV_BYPASS_EN	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_LV_BYPASS_EN_MASK, MADI_LMVD_LV_BYPASS_EN_OFFSET,0);

    //MADI_LMVD_LV_VAL	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_LV_VAL_MASK, MADI_LMVD_LV_VAL_OFFSET,0);

    //MADI_LMVD_HLG_K_BYPASS_EN	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_HLG_K_BYPASS_EN_MASK, MADI_LMVD_HLG_K_BYPASS_EN_OFFSET,0);

    //MADI_LMVD_HLG_K_VAL	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_HLG_K_VAL_MASK, MADI_LMVD_HLG_K_VAL_OFFSET,0);

    //MADI_LMVD_GLV_RESET_EN	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_GLV_RESET_EN_MASK, MADI_LMVD_GLV_RESET_EN_OFFSET,0);

    //MADI_LMVD_GLV_RESET_VAL	0
    MADI_REG32_ClearAndSet(MADI_LMVD_CTRL1, MADI_LMVD_GLV_RESET_VAL_MASK, MADI_LMVD_GLV_RESET_VAL_OFFSET,0);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Madi_Enable
 USAGE          : Enable Madi, after initialization of changing vq data
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Madi_Enable(void)
{
    MADI_REG32_Set(MADI_CTRL,MADI_FE_MADI_EN_MASK);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}

/******************************************************************************
 FUNCTION       : Madi_Disable
 USAGE          : Disables or suspends Madi operation, bypassing Madi block.
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Madi_Disable(void)
{
    MADI_REG32_Clear(MADI_CTRL,MADI_FE_MADI_EN_MASK);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}

/******************************************************************************
 FUNCTION       : Madi_SetVqData
 USAGE          : Writes all madi vq data to the hardware
 INPUT          : VQ_TNR_Adaptive_Parameters_t *vq_data_p - pointer to vqdata
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t Madi_SetVqData(const VQ_McMadi_Parameters_t *vq_data_p)
{
    if (vq_data_p == 0)
        return FVDP_ERROR;

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL ,DEINTC_DCDI_ENABLE_MASK , DEINTC_DCDI_ENABLE_OFFSET, vq_data_p-> MCMADI_DeinterlacerDcdiEnable);//	Bool				DEINTC_DCDI_EBALE bit in DEINTC_DCDI_CTRL
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL3 ,DEINTC_DCDI_PELCORR_TH_MASK , DEINTC_DCDI_PELCORR_TH_OFFSET, vq_data_p-> MCMADI_DeinterlacerDcdiStrength);//	1 Byte	0 (display as 0%)	15 (display as 100%)	1	DEINTC_DCDI_PERLCORR_TH in DEINTC_DCDI_CTRL3
    MADI_REG32_ClearAndSet(MADI_QUANT_THRESH0 ,MADI_QUANT_THRESH0_MASK , MADI_QUANT_THRESH0_OFFSET, vq_data_p-> MADI_LowMotionTh0);//	1 Byte	0	64 (always <=th1)	1	MADI_QUANT_THRESH0
    MADI_REG32_ClearAndSet(MADI_QUANT_THRESH1 ,MADI_QUANT_THRESH1_MASK , MADI_QUANT_THRESH1_OFFSET, vq_data_p-> MADI_LowMotionTh1);//	1 Byte	0	64(always<th2)	1	MADI_QUANT_THRESH1
    MADI_REG32_ClearAndSet(MADI_QUANT_THRESH2 ,MADI_QUANT_THRESH2_MASK , MADI_QUANT_THRESH2_OFFSET, vq_data_p-> MADI_LowMotionTh2);//	1 Byte	0	64	1	MADI_QUANT_THRESH2
    MADI_REG32_ClearAndSet(MADI_GAIN ,MADI_CHROMA_GAIN_MASK , MADI_CHROMA_GAIN_OFFSET, vq_data_p-> MADI_ChromaGain);//	1 Byte	0	3	1	Link to MADI_CHROMA_GAIN in MADI_GAIN register; the mapping of display vs register is:0-0 33-11 66-10 100-01
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL2 ,MADI_W01_MASK , MADI_W01_OFFSET, vq_data_p-> MADI_MVLHH);//	1 Byte	0	63	1	Map to register MADI_W01
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL2 ,MADI_W10_MASK , MADI_W10_OFFSET, vq_data_p-> MADI_MVHHL);//	1 Byte	0	63	1	Map to register MADI_W10
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL2 ,MADI_W11_MASK , MADI_W11_OFFSET, vq_data_p-> MADI_MVHHH);//	1 Byte	0	63	1	Map to register MADI_W11
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL4 ,MADI_MC_W00_MASK , MADI_MC_W00_OFFSET, vq_data_p-> MADI_MCVLHL);//	1 Byte	0	63	1	Map to register MADI_MC_W00
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL4 ,MADI_MC_W01_MASK , MADI_MC_W01_OFFSET, vq_data_p-> MADI_MCVLHH);//	1 Byte	0	63	1	Map to register MADI_MC_W01
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL4 ,MADI_MC_W10_MASK , MADI_MC_W10_OFFSET, vq_data_p-> MADI_MCVHHL);//	1 Byte	0	63	1	Map to register MADI_MC_W10
    MADI_REG32_ClearAndSet(MADI_FILTER_CTRL4 ,MADI_MC_W11_MASK , MADI_MC_W11_OFFSET, vq_data_p-> MADI_MCVHHH);//	1 Byte	0	63	1	Map to register MADI_MC_W11
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1 ,MADI_STATIC_VDGAIN_EN_MASK , MADI_STATIC_VDGAIN_EN_OFFSET, vq_data_p-> MADI_STATIC_VDGAIN_ENABLE);//	Bool				MADI_STATIC_VDGAIN_EN
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1 ,MADI_STATIC_CLEAN_EN_MASK , MADI_STATIC_CLEAN_EN_OFFSET, vq_data_p-> MADI_STATIC_CLEAN_EN);//	Bool				MADI_STATIC_CLEAN_EN
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1 ,MADI_STATIC_VDGAIN_MAX_DIFF_MASK , MADI_STATIC_VDGAIN_MAX_DIFF_OFFSET, vq_data_p-> MADI_STATIC_VDGAIN_MAX_DIFF);//	1 Byte	0	127	1	MADI_STATIC_VDGAIN_MAX_DIFF
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1 ,MADI_STATIC_VDGAIN_MAX_DIFF_SQ_MASK , MADI_STATIC_VDGAIN_MAX_DIFF_SQ_OFFSET, vq_data_p-> MADI_STATIC_VDGAIN_MAX_DIFF_SQ);//_SQ	2 bytes	0	32767	1	MADI_STATIC_VDGAIN_MAX_DIFF_SQ
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL1 ,MADI_STATIC_VDGAIN_RSHIFT_MASK , MADI_STATIC_VDGAIN_RSHIFT_OFFSET, vq_data_p-> MADI_STATIC_VDGAIN_RSHIFT);//	1 byte	0	15	1
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL2 , MADI_STATIC_VDGAIN_OFFSET_MASK, MADI_STATIC_VDGAIN_OFFSET_OFFSET, vq_data_p-> MADI_STATIC_VDGAIN_OFFSET);//	2 bytes	0	16383	1
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL2 ,MADI_STATIC_DIFF_DEPTH_MASK , MADI_STATIC_DIFF_DEPTH_OFFSET, vq_data_p-> MADI_STATIC_DIFF_DEPTH);//	1 byte	0	15	1	Static history
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL3 ,MADI_STATIC_LV_FRM_DIFF_THRESH_MASK , MADI_STATIC_LV_FRM_DIFF_THRESH_OFFSET, vq_data_p-> MADI_STATIC_LV_FRM_DIFF_THRESH);//	2 bytes	0	1023	1	LV static threshold
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL3 ,MADI_STATIC_FRM_DIFF_THRESH_MASK , MADI_STATIC_FRM_DIFF_THRESH_OFFSET, vq_data_p-> MADI_STATIC_FRM_DIFF_THRESH);//	2 bytes	0	1023	1	static threshold
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL4 ,MADI_STATIC_LARGE_MOTION_GAIN_MASK , MADI_STATIC_LARGE_MOTION_GAIN_OFFSET, vq_data_p-> MADI_STATIC_LARGE_MOTION_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL4 ,MADI_STATIC_LOW_CONTRAST_GAIN_MASK , MADI_STATIC_LOW_CONTRAST_GAIN_OFFSET, vq_data_p-> MADI_STATIC_LOW_CONTRAST_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_STATIC_CTRL4 ,MADI_STATIC_LOW_CONTRAST_OFFSET_MASK , MADI_STATIC_LOW_CONTRAST_OFFSET_OFFSET, vq_data_p-> MADI_STATIC_LOW_CONTRAST_OFFSET);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES1 ,MADI_MDET_LO_THR_MASK , MADI_MDET_LO_THR_OFFSET, vq_data_p-> MADI_MDET_LO_THR);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES1 ,MADI_MDET_HI_THR_MASK , MADI_MDET_HI_THR_OFFSET, vq_data_p-> MADI_MDET_HI_THR);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES2 ,MADI_MDET_LO_GAIN_MASK , MADI_MDET_LO_GAIN_OFFSET, vq_data_p-> MADI_MDET_LO_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES2 ,MADI_MDET_HI_GAIN_MASK , MADI_MDET_HI_GAIN_OFFSET, vq_data_p-> MADI_MDET_HI_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES3 ,MADI_MDET_NORM_GAIN_MASK , MADI_MDET_NORM_GAIN_OFFSET, vq_data_p-> MADI_MDET_NORM_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES3 ,MADI_MDET_THR0_MASK , MADI_MDET_THR0_OFFSET, vq_data_p-> MADI_MDET_THR0);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES4 ,MADI_MDET_THR1_MASK , MADI_MDET_THR1_OFFSET, vq_data_p-> MADI_MDET_THR1);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES4 ,MADI_MDET_THR2_MASK , MADI_MDET_THR2_OFFSET, vq_data_p-> MADI_MDET_THR2);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES5 ,MADI_MDET_LV_LO_THR_MASK , MADI_MDET_LV_LO_THR_OFFSET, vq_data_p-> MADI_MDET_LV_LO_THR);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES5 ,MADI_MDET_LV_HI_THR_MASK , MADI_MDET_LV_HI_THR_OFFSET, vq_data_p-> MADI_MDET_LV_HI_THR);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES6 ,MADI_MDET_LV_LO_GAIN_MASK , MADI_MDET_LV_LO_GAIN_OFFSET, vq_data_p-> MADI_MDET_LV_LO_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES6 ,MADI_MDET_LV_HI_GAIN_MASK , MADI_MDET_LV_HI_GAIN_OFFSET, vq_data_p-> MADI_MDET_LV_HI_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES7 ,MADI_MDET_LV_NORM_GAIN_MASK , MADI_MDET_LV_NORM_GAIN_OFFSET, vq_data_p-> MADI_MDET_LV_NORM_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES7 ,MADI_MDET_LV_THR0_MASK , MADI_MDET_LV_THR0_OFFSET, vq_data_p-> MADI_MDET_LV_THR0);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES8 ,MADI_MDET_LV_THR1_MASK , MADI_MDET_LV_THR1_OFFSET, vq_data_p-> MADI_MDET_LV_THR1);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES8 ,MADI_MDET_LV_THR2_MASK , MADI_MDET_LV_THR2_OFFSET, vq_data_p-> MADI_MDET_LV_THR2);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES9 ,MADI_MDET_TDIFF_BLEND_MASK , MADI_MDET_TDIFF_BLEND_OFFSET, vq_data_p-> MADI_MDET_TDIFF_BLEND);//	1 byte	0	31	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES9 ,MADI_MDET_HTRAN_THR_MASK , MADI_MDET_HTRAN_THR_OFFSET, vq_data_p-> MADI_MDET_HTRAN_THR);//	2 bytes	0	2047	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES9 ,MADI_MDET_FIELDDIFF_THR_MASK , MADI_MDET_FIELDDIFF_THR_OFFSET, vq_data_p-> MADI_MDET_FIELDDIFF_THR);//	2 bytes	0	4095	1
    MADI_REG32_ClearAndSet(MADI_MDET_THRES10 ,MADI_MDET_MDP_GAIN_MASK , MADI_MDET_MDP_GAIN_OFFSET, vq_data_p-> MADI_MDET_MDP_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LASD_THRES1 ,MADI_LASD_NKL_MASK , MADI_LASD_NKL_OFFSET, vq_data_p-> MADI_LASD_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LASD_THRES1 ,MADI_LASD_THR_MASK , MADI_LASD_THR_OFFSET, vq_data_p-> MADI_LASD_THR);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LASD_THRES1 ,MADI_LASD_GAIN_MASK , MADI_LASD_GAIN_OFFSET, vq_data_p-> MADI_LASD_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LASD_THRES2 ,MADI_LASD_LV_NKL_MASK , MADI_LASD_LV_NKL_OFFSET, vq_data_p-> MADI_LASD_LV_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES1 ,MADI_LAMD_NKL_MASK , MADI_LAMD_NKL_OFFSET, vq_data_p-> MADI_LAMD_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES1 ,MADI_LAMD_SMALL_NKL_MASK , MADI_LAMD_SMALL_NKL_OFFSET, vq_data_p-> MADI_LAMD_SMALL_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES2 ,MADI_LAMD_BIG_NKL_MASK , MADI_LAMD_BIG_NKL_OFFSET, vq_data_p-> MADI_LAMD_BIG_NKL);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES2 ,MADI_LAMD_GAIN_MASK , MADI_LAMD_GAIN_OFFSET, vq_data_p-> MADI_LAMD_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES3 ,MADI_LAMD_HTHR_MASK , MADI_LAMD_HTHR_OFFSET, vq_data_p-> MADI_LAMD_HTHR);//	1 byte	0	31	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES3 ,MADI_LAMD_HSP_MASK , MADI_LAMD_HSP_OFFSET, vq_data_p-> MADI_LAMD_HSP);//	1 byte	0	7	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES3 ,MADI_LAMD_VTHR_MASK , MADI_LAMD_VTHR_OFFSET, vq_data_p-> MADI_LAMD_VTHR);//	1 byte	0	31	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES3 ,MADI_LAMD_VSP_MASK , MADI_LAMD_VSP_OFFSET, vq_data_p-> MADI_LAMD_VSP);//	1 byte	0	7	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES4 ,MADI_LAMD_LV_NKL_MASK , MADI_LAMD_LV_NKL_OFFSET, vq_data_p-> MADI_LAMD_LV_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES4 ,MADI_LAMD_LV_SMALL_NKL_MASK , MADI_LAMD_LV_SMALL_NKL_OFFSET, vq_data_p-> MADI_LAMD_LV_SMALL_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES5 ,MADI_LAMD_LV_BIG_NKL_MASK , MADI_LAMD_LV_BIG_NKL_OFFSET, vq_data_p-> MADI_LAMD_LV_BIG_NKL);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES5 ,MADI_LAMD_LV_GAIN_MASK , MADI_LAMD_LV_GAIN_OFFSET, vq_data_p-> MADI_LAMD_LV_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES6 ,MADI_LAMD_LV_HTHR_MASK , MADI_LAMD_LV_HTHR_OFFSET, vq_data_p-> MADI_LAMD_LV_HTHR);//	1 byte	0	31	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES6 ,MADI_LAMD_LV_HSP_MASK , MADI_LAMD_LV_HSP_OFFSET, vq_data_p-> MADI_LAMD_LV_HSP);//	1 byte	0	7	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES6 ,MADI_LAMD_LV_VTHR_MASK , MADI_LAMD_LV_VTHR_OFFSET, vq_data_p-> MADI_LAMD_LV_VTHR);//	1 byte	0	31	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES6 ,MADI_LAMD_LV_VSP_MASK , MADI_LAMD_LV_VSP_OFFSET, vq_data_p-> MADI_LAMD_LV_VSP);//	1 byte	0	7	1
    MADI_REG32_ClearAndSet(MADI_LAMD_THRES7 ,MADI_LAMD_LARGE_MOTION_GAIN_MASK , MADI_LAMD_LARGE_MOTION_GAIN_OFFSET, vq_data_p-> MADI_LAMD_LARGE_MOTION_GAIN);//	1 byte	0	255	1	MADI_LAMD_HD_GAIN
    MADI_REG32_ClearAndSet(MADI_CNST_THRES1 ,MADI_CNST_NKL_LO_MASK , MADI_CNST_NKL_LO_OFFSET, vq_data_p-> MADI_CNST_NKL_LO);//	2 bytes	0	8191	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES1 ,MADI_CNST_NKL_HI_MASK , MADI_CNST_NKL_HI_OFFSET, vq_data_p-> MADI_CNST_NKL_HI);//	2 bytes	0	8191	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES2 ,MADI_CNST_NKL_MASK , MADI_CNST_NKL_OFFSET, vq_data_p-> MADI_CNST_NKL);//	2 bytes	0	8191	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES2 ,MADI_CNST_GAIN_MASK , MADI_CNST_GAIN_OFFSET, vq_data_p-> MADI_CNST_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES2 ,MADI_CNST_Y_OFF_MASK , MADI_CNST_Y_OFF_OFFSET, vq_data_p-> MADI_CNST_Y_OFF);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES3 ,MADI_CNST_LV_NKL_MASK , MADI_CNST_LV_NKL_OFFSET, vq_data_p-> MADI_CNST_LV_NKL);//	2 bytes	0	8191	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES3 ,MADI_CNST_LV_GAIN_MASK , MADI_CNST_LV_GAIN_OFFSET, vq_data_p-> MADI_CNST_LV_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES3 ,MADI_CNST_LV_Y_OFF_MASK , MADI_CNST_LV_Y_OFF_OFFSET, vq_data_p-> MADI_CNST_LV_Y_OFF);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES4 ,MADI_CNST_ST_NKL_MASK , MADI_CNST_ST_NKL_OFFSET, vq_data_p-> MADI_CNST_ST_NKL);//	2 bytes	0	8191	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES4 ,MADI_CNST_ST_GAIN_MASK , MADI_CNST_ST_GAIN_OFFSET, vq_data_p-> MADI_CNST_ST_GAIN);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_CNST_THRES4 ,MADI_CNST_ST_Y_OFF_MASK , MADI_CNST_ST_Y_OFF_OFFSET, vq_data_p-> MADI_CNST_ST_Y_OFF);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES1 ,MADI_LMVD_HLG_OST0_MASK , MADI_LMVD_HLG_OST0_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST0);//	2 bytes	0	64	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on X axis.Typical value is: 0 0 0 0 4 8 12 16 20 24 28
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES1 ,MADI_LMVD_HLG_OST1_MASK , MADI_LMVD_HLG_OST1_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST1);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES1 ,MADI_LMVD_HLG_OST2_MASK , MADI_LMVD_HLG_OST2_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST2);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES2 ,MADI_LMVD_HLG_OST3_MASK , MADI_LMVD_HLG_OST3_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST3);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES3 ,MADI_LMVD_HLG_OST4_MASK , MADI_LMVD_HLG_OST4_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST4);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES3 ,MADI_LMVD_HLG_OST5_MASK , MADI_LMVD_HLG_OST5_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST5);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES4 ,MADI_LMVD_HLG_OST6_MASK , MADI_LMVD_HLG_OST6_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST6);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES4 ,MADI_LMVD_HLG_OST7_MASK , MADI_LMVD_HLG_OST7_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST7);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES5 ,MADI_LMVD_HLG_OST8_MASK , MADI_LMVD_HLG_OST8_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST8);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES5 ,MADI_LMVD_HLG_OST9_MASK , MADI_LMVD_HLG_OST9_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST9);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES6 ,MADI_LMVD_HLG_OST10_MASK , MADI_LMVD_HLG_OST10_OFFSET, vq_data_p-> MADI_LMVD_HLG_OST10);//	2 bytes	0	64	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES7 ,MADI_LMVD_HLG_MD_NOISE_MASK , MADI_LMVD_HLG_MD_NOISE_OFFSET, vq_data_p-> MADI_LMVD_HLG_MD_NOISE);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES7 ,MADI_LMVD_HLG_NKL_MASK , MADI_LMVD_HLG_NKL_OFFSET, vq_data_p-> MADI_LMVD_HLG_NKL);//	1 byte	0	255	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES8 ,MADI_LMVD_HLG_MD_CNST_NKL_MASK , MADI_LMVD_HLG_MD_CNST_NKL_OFFSET, vq_data_p-> MADI_LMVD_HLG_MD_CNST_NKL);//	2 bytes	0	4095	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES8 ,MADI_LMVD_HLG_MD_STILL_NKL_MASK , MADI_LMVD_HLG_MD_STILL_NKL_OFFSET, vq_data_p-> MADI_LMVD_HLG_MD_STILL_NKL);//	2 bytes	0	1023	1
    MADI_REG32_ClearAndSet(MADI_LMVD_THRES9 ,MADI_LMVD_HLG_MD_THR_LO_MASK , MADI_LMVD_HLG_MD_THR_LO_OFFSET, vq_data_p-> MADI_LMVD_HLG_MD_THR_LO);//	1 byte	0	15	1
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
    return    FVDP_OK;
}

/******************************************************************************
 FUNCTION       : Madi_LoadDefaultVqData
 USAGE          : Programms temporary vq data structure then cals Madi_SetVqData
                    to write default vq data to the hardware registers
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Madi_LoadDefaultVqData(void)
{
    VQ_McMadi_Parameters_t MadiDefault;
    MadiDefault.MCMADI_DeinterlacerDcdiEnable = 1;//	Bool				DEINTC_DCDI_EBALE bit in DEINTC_DCDI_CTRL
    MadiDefault.MCMADI_DeinterlacerDcdiStrength = 4;//	1 Byte	0 (display as 0%)	15 (display as 100%)	1	DEINTC_DCDI_PERLCORR_TH in DEINTC_DCDI_CTRL3
    MadiDefault.MADI_LowMotionTh0 = 24;//	1 Byte	0	64 (always <=th1)	1	MADI_QUANT_THRESH0
    MadiDefault.MADI_LowMotionTh1 = 38;//	1 Byte	0	64(always<th2)	1	MADI_QUANT_THRESH1
    MadiDefault.MADI_LowMotionTh2 = 53;//	1 Byte	0	64	1	MADI_QUANT_THRESH2
    MadiDefault.MADI_ChromaGain = 3;//	1 Byte	0	3	1	Link to MADI_CHROMA_GAIN in MADI_GAIN register; the mapping of display vs register is:0-0 33-11 66-10 100-01
    MadiDefault.MADI_MVLHL = 32;//	1 Byte	0	63	1	Map to register MADI_W00
    MadiDefault.MADI_MVLHH = 2;//	1 Byte	0	63	1	Map to register MADI_W01
    MadiDefault.MADI_MVHHL = 5;//	1 Byte	0	63	1	Map to register MADI_W10
    MadiDefault.MADI_MVHHH = 2;//	1 Byte	0	63	1	Map to register MADI_W11
    MadiDefault.MADI_MCVLHL = 0;//	1 Byte	0	63	1	Map to register MADI_MC_W00
    MadiDefault.MADI_MCVLHH = 0;//	1 Byte	0	63	1	Map to register MADI_MC_W01
    MadiDefault.MADI_MCVHHL = 0;//	1 Byte	0	63	1	Map to register MADI_MC_W10
    MadiDefault.MADI_MCVHHH = 0;//	1 Byte	0	63	1	Map to register MADI_MC_W11
    MadiDefault.MADI_STATIC_VDGAIN_ENABLE = 1;//	Bool				MADI_STATIC_VDGAIN_EN
    MadiDefault.MADI_STATIC_CLEAN_EN = 1;//	Bool				MADI_STATIC_CLEAN_EN
    MadiDefault.MADI_STATIC_VDGAIN_MAX_DIFF = 127;//	1 Byte	0	127	1	MADI_STATIC_VDGAIN_MAX_DIFF
    MadiDefault.MADI_STATIC_VDGAIN_MAX_DIFF_SQ = 0x2800;//16384;//_SQ	2 bytes	0	32767	1	MADI_STATIC_VDGAIN_MAX_DIFF_SQ
    MadiDefault.MADI_STATIC_VDGAIN_RSHIFT = 15;//14;//	1 byte	0	15	1
    MadiDefault.MADI_STATIC_VDGAIN_OFFSET = 10240;//	2 bytes	0	16383	1
    MadiDefault.MADI_STATIC_DIFF_DEPTH = 4;//	1 byte	0	15	1	Static history
    MadiDefault.MADI_STATIC_LV_FRM_DIFF_THRESH = 20;//	2 bytes	0	1023	1	LV static threshold
    MadiDefault.MADI_STATIC_FRM_DIFF_THRESH = 0x14;//24;//	2 bytes	0	1023	1	static threshold
    MadiDefault.MADI_STATIC_LARGE_MOTION_GAIN = 48;//	1 byte	0	255	1
    MadiDefault.MADI_STATIC_LOW_CONTRAST_GAIN = 24;//	1 byte	0	255	1
    MadiDefault.MADI_STATIC_LOW_CONTRAST_OFFSET = 4;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_LO_THR = 14;//	2 bytes	0	1023	1
    MadiDefault.MADI_MDET_HI_THR = 32;//	2 bytes	0	1023	1
    MadiDefault.MADI_MDET_LO_GAIN = 0x10;//128;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_HI_GAIN = 60;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_NORM_GAIN = 0x20;//10;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_THR0 = 40;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_THR1 = 60;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_THR2 = 80;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_LV_LO_THR = 14;//	2 bytes	0	1023	1
    MadiDefault.MADI_MDET_LV_HI_THR = 32;//	2 bytes	0	1023	1
    MadiDefault.MADI_MDET_LV_LO_GAIN = 0x10;//128;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_LV_HI_GAIN = 40;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_LV_NORM_GAIN = 0x20;//24;//	1 byte	0	255	1
    MadiDefault.MADI_MDET_LV_THR0 = 100;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_LV_THR1 = 180;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_LV_THR2 = 280;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_TDIFF_BLEND = 0x10;//8;//	1 byte	0	31	1
    MadiDefault.MADI_MDET_HTRAN_THR = 160;//	2 bytes	0	2047	1
    MadiDefault.MADI_MDET_FIELDDIFF_THR = 90;//	2 bytes	0	4095	1
    MadiDefault.MADI_MDET_MDP_GAIN = 9;//	1 byte	0	255	1
    MadiDefault.MADI_LASD_NKL = 0x10;//15;//	2 bytes	0	1023	1
    MadiDefault.MADI_LASD_THR = 0x46;//150;//	1 byte	0	255	1
    MadiDefault.MADI_LASD_GAIN = 16;//	1 byte	0	255	1
    MadiDefault.MADI_LASD_LV_NKL = 0x10;//15;//	2 bytes	0	1023	1
    MadiDefault.MADI_LAMD_NKL = 0x14;//18;//	2 bytes	0	1023	1
    MadiDefault.MADI_LAMD_SMALL_NKL = 0x3ff;//20;//	2 bytes	0	1023	1
    MadiDefault.MADI_LAMD_BIG_NKL = 0xff;//20;//	1 byte	0	255	1
    MadiDefault.MADI_LAMD_GAIN = 24;//	1 byte	0	255	1
    MadiDefault.MADI_LAMD_HTHR = 14;//	1 byte	0	31	1
    MadiDefault.MADI_LAMD_HSP = 2;//	1 byte	0	7	1
    MadiDefault.MADI_LAMD_VTHR = 16;//	1 byte	0	31	1
    MadiDefault.MADI_LAMD_VSP = 1;//	1 byte	0	7	1
    MadiDefault.MADI_LAMD_LV_NKL = 15;//	2 bytes	0	1023	1
    MadiDefault.MADI_LAMD_LV_SMALL_NKL = 0x3ff;//5;//	2 bytes	0	1023	1
    MadiDefault.MADI_LAMD_LV_BIG_NKL = 0xff;//20;//	1 byte	0	255	1
    MadiDefault.MADI_LAMD_LV_GAIN = 8;//	1 byte	0	255	1
    MadiDefault.MADI_LAMD_LV_HTHR = 14;//	1 byte	0	31	1
    MadiDefault.MADI_LAMD_LV_HSP = 5;//	1 byte	0	7	1
    MadiDefault.MADI_LAMD_LV_VTHR = 23;//	1 byte	0	31	1
    MadiDefault.MADI_LAMD_LV_VSP = 1;//	1 byte	0	7	1
    MadiDefault.MADI_LAMD_LARGE_MOTION_GAIN = 24;//	1 byte	0	255	1	MADI_LAMD_HD_GAIN
    MadiDefault.MADI_CNST_NKL_LO = 12;//	2 bytes	0	8191	1
    MadiDefault.MADI_CNST_NKL_HI = 60;//	2 bytes	0	8191	1
    MadiDefault.MADI_CNST_NKL = 48;//	2 bytes	0	8191	1
    MadiDefault.MADI_CNST_GAIN = 0x18;//16;//	1 byte	0	255	1
    MadiDefault.MADI_CNST_Y_OFF = 0;//	1 byte	0	255	1
    MadiDefault.MADI_CNST_LV_NKL = 96;//	2 bytes	0	8191	1
    MadiDefault.MADI_CNST_LV_GAIN = 0x18;//90;//	1 byte	0	255	1
    MadiDefault.MADI_CNST_LV_Y_OFF = 0x0;//20;//	1 byte	0	255	1
    MadiDefault.MADI_CNST_ST_NKL = 8;//	2 bytes	0	8191	1
    MadiDefault.MADI_CNST_ST_GAIN = 0x18;//45;//	1 byte	0	255	1
    MadiDefault.MADI_CNST_ST_Y_OFF = 0x0;//10;//	1 byte	0	255	1
    MadiDefault.MADI_LMVD_HLG_OST0 = 0;//	2 bytes	0	64	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center.There are 21 entries on X axis.Typical value is: 0 0 0 0 4 8 12 16 20 24 28
    MadiDefault.MADI_LMVD_HLG_OST1 = 0;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST2 = 0;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST3 = 0;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST4 = 4;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST5 = 8;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST6 = 12;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST7 = 16;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST8 = 20;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST9 = 24;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_OST10 = 28;//	2 bytes	0	64	1
    MadiDefault.MADI_LMVD_HLG_MD_NOISE = 4;//	1 byte	0	255	1
    MadiDefault.MADI_LMVD_HLG_NKL = 64;//	1 byte	0	255	1
    MadiDefault.MADI_LMVD_HLG_MD_CNST_NKL = 20;//	2 bytes	0	4095	1
    MadiDefault.MADI_LMVD_HLG_MD_STILL_NKL = 0;//	2 bytes	0	1023	1
    MadiDefault.MADI_LMVD_HLG_MD_THR_LO = 3;//	1 byte	0	15	1
    MadiDefault.MADI_LMVD_HLG_MD_gthr_ratio = 0xbb8;//250;//

    Madi_SetVqData(&MadiDefault);
}

/******************************************************************************
 FUNCTION       : Madi_SetPixelNumberForMDThr
 USAGE          : Set PixelNumber For MD Threshold
 INPUT          : input horizontal size and vertical size
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t Madi_SetPixelNumberForMDThr(uint16_t H_Size, uint16_t V_Size)
{
    uint32_t   PixelNumber;

    //If the number of pixels with motion bigger than MADI_LMVD_HLG_MD_THR_LO is more
    //    than this threshold, then global_lv is set For 720x480 use 250. For 720x576 use 300. Range [0..1920*540]
    PixelNumber = (H_Size * V_Size) /345;

    MADI_REG32_ClearAndSet(MADI_LMVD_THRES9 ,MADI_LMVD_HLG_MD_GTHR_MASK , MADI_LMVD_HLG_MD_GTHR_OFFSET, PixelNumber);//	1 byte	0	15	1

    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : Madi_SetLASDThr
 USAGE          : Set LASD Threshold
 INPUT          : input horizontal size and vertical size
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t Madi_SetLASDThr(uint16_t  H_Size, uint16_t V_Size)
{
    uint32_t   LASDThr;

    //This value is 150 for SD and 70 for HD
    if((H_Size > 1700)&&( V_Size > 500))
    {
        LASDThr = 70;
    }
    else
    {
        LASDThr = 150;
    }
    MADI_REG32_ClearAndSet(MADI_LASD_THRES1 ,MADI_LASD_THR_MASK , MADI_LASD_THR_OFFSET, LASDThr);

    return FVDP_OK;
}
