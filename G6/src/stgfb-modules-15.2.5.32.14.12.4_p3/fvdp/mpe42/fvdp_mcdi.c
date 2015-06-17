/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_mcdi.c
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

#include "fvdp_mcdi.h"

#define PROGRAM_MCDI_RW_REGS_IN_VCPU            TRUE

/* Private Macros ----------------------------------------------------------- */
#define MCDI_REG32_Write(Addr,Val)              REG32_Write(Addr+MCDI_FE_BASE_ADDRESS,Val)
#define MCDI_REG32_Read(Addr)                   REG32_Read(Addr+MCDI_FE_BASE_ADDRESS)
#define MCDI_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+MCDI_FE_BASE_ADDRESS,BitsToSet)
#define MCDI_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+MCDI_FE_BASE_ADDRESS,BitsToClear)
#define MCDI_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+MCDI_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define DEINT_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DEINT_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define DEINT_REG32_Write(Addr,Val)             REG32_Write(Addr+DEINT_FE_BASE_ADDRESS,Val)


#define MCMADI_REG32_Write(Addr,Val)            REG32_Write(Addr+MCMADI_FE_BASE_ADDRESS,Val)
#define MCMADI_REG32_Read(Addr)                 REG32_Read(Addr+MCMADI_FE_BASE_ADDRESS)
#define MCMADI_REG32_Set(Addr,BitsToSet)        REG32_Set(Addr+MCMADI_FE_BASE_ADDRESS,BitsToSet)
#define MCMADI_REG32_Clear(Addr,BitsToClear)    REG32_Clear(Addr+MCMADI_FE_BASE_ADDRESS,BitsToClear)
#define MCMADI_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+MCMADI_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


/* Functions ---------------------------------------------------------------- */

/******************************************************************************
 FUNCTION       : McMadiEnableDatapath
 USAGE          : Enables mcmadi blocks, so data can go through
 INPUT          : bool Enable, if true mcmadi blocks datapath is enabled (tnr, madi, deint, mcdi),
                            Mcmadi features are enabled and disabled by other functions
                  FVDP_ScanType_t InputScanType - Is input progressive or interlaced
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void McMadiEnableDatapath(bool Enable, FVDP_ScanType_t InputScanType)
{
    MCMADI_REG32_Clear(MCMADI_CTRL, DEINT_EN_MASK|MADI_EN_MASK|MCDI_EN_MASK|TNR_EN_MASK|TNR_DUPLICATE_MASK|PROG_INTLCN_MASK|DEINT_TEMP_EN_MASK|SD_HDN_SEL_MASK);

    if (Enable)
    {
         if (InputScanType == INTERLACED)
         {
            MCMADI_REG32_Set(MCMADI_CTRL, DEINT_EN_MASK | MADI_EN_MASK | MCDI_EN_MASK | TNR_EN_MASK | DEINT_TEMP_EN_MASK);
         }
         else
         {
            MCMADI_REG32_Set(MCMADI_CTRL, TNR_EN_MASK | TNR_DUPLICATE_MASK | PROG_INTLCN_MASK | TNR_TEMP_SEL_MASK);
         }
    }
}

/******************************************************************************
 FUNCTION       : Mcdi_Init
 USAGE          : Initialization function for tnr, it should be called after power up
 INPUT          :  void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Mcdi_Init (void)
{

    MCDI_REG32_Write(MCDI_CTRL1,0);

    MCDI_REG32_Clear(LDME_CTRL1,LDME_HME_ONLY_MASK);

    MCDI_REG32_Clear(LDME_CTRL1,LDME_FORCE_GMOVE_MASK);
    //MCDI_LDME_USER_VMV 4
    MCDI_REG32_ClearAndSet(LDME_CTRL10,LDME_USER_VMV_MASK,LDME_USER_VMV_OFFSET,4);
    //MCDI_LDME_USER_HMV 5
    MCDI_REG32_ClearAndSet(LDME_CTRL10,LDME_USER_HMV_MASK,LDME_USER_HMV_OFFSET,5);

    //MCDI_LDME_GSTILL_USER_K 156
    MCDI_REG32_ClearAndSet(LDME_CTRL10,LDME_GSTILL_USER_K_MASK,LDME_GSTILL_USER_K_OFFSET,155);

    //MCDI_LDME_STILL_USER_K 75
    MCDI_REG32_ClearAndSet(LDME_CTRL10,LDME_STILL_USER_K_MASK,LDME_STILL_USER_K_OFFSET,75);

    //MCDI_LDME_SF_USER_K	155
    MCDI_REG32_ClearAndSet(LDME_CTRL11, LDME_SF_USER_K_MASK, LDME_SF_USER_K_OFFSET,155);
    //MCDI_SDI_USER_K	5
    MCDI_REG32_ClearAndSet(SDI_CTRL4, SDI_USER_K_MASK, SDI_USER_K_OFFSET,0);


    //MCDI_LDME_MODE_SF_EN
    MCDI_REG32_ClearAndSet(LDME_CTRL12, LDME_MODE_SF_EN_MASK, LDME_MODE_SF_EN_OFFSET,0);

    //MCDI_LMVD_HLG_USER_K
    MCDI_REG32_ClearAndSet(LMVD_CTRL10, LMVD_HLG_USER_K_MASK, LMVD_HLG_USER_K_OFFSET,0);

    //MCDI_LDME_DME_DISABLE
    MCDI_REG32_ClearAndSet(LDME_CTRL20, LDME_DME_DISABLE_MASK, LDME_DME_DISABLE_OFFSET,0);

    //MCDI_DL_ST	11
    MCDI_REG32_ClearAndSet(TDI_CTRL5, DL_ST_MASK, DL_ST_OFFSET,11);

    //MCDI_DL_SP
    MCDI_REG32_ClearAndSet(TDI_CTRL5, DL_SP_MASK, DL_SP_OFFSET, 708);

    //MCDI_DL_VST	0
    MCDI_REG32_ClearAndSet(TDI_CTRL6, DL_VST_MASK, DL_VST_OFFSET,0);

    //MCDI_DL_VSP
    MCDI_REG32_ClearAndSet(TDI_CTRL6, DL_VSP_MASK, DL_VSP_OFFSET, 479);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);

}
/******************************************************************************
 FUNCTION       : Deint_Init
 USAGE          : Initialization function for deint, it should be called after power up
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Deint_Init (void)
{

    DEINT_REG32_ClearAndSet(DEINTC_CTRL,DEINTC_INT_COEF_EN_MASK,DEINTC_INT_COEF_EN_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_CTRL,DEINTC_SPATIAL_EN_MASK,DEINTC_SPATIAL_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_CTRL,DEINTC_DITHER_EN_MASK,DEINTC_DITHER_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_CTRL,DEINTC_AFM_VT_MADI_SEL_MASK,DEINTC_AFM_VT_MADI_SEL_OFFSET,3);

    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_RING_CLAMP_EN_MASK,DEINTC_RING_CLAMP_EN_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_RING_THRESH_MASK,DEINTC_RING_THRESH_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_MV_0_SEL_MASK,DEINTC_MV_0_SEL_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_MV_1_SEL_MASK,DEINTC_MV_1_SEL_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_MV_2_SEL_MASK,DEINTC_MV_2_SEL_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_RING_CLAMP,DEINTC_MV_3_SEL_MASK,DEINTC_MV_3_SEL_OFFSET,0);

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL, DEINTC_DCDI_ENABLE_MASK,DEINTC_DCDI_ENABLE_OFFSET,1);

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_CLAMP_REP_THRSH_MASK,DEINTC_CLAMP_REP_THRSH_OFFSET,8);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_EDGE_REP_EN_MASK,DEINTC_EDGE_REP_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_VAR_DEP_EN_MASK,DEINTC_VAR_DEP_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_VAR_DEP_ORDER_MASK,DEINTC_VAR_DEP_ORDER_OFFSET,3);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_PRO_ANGLE_EN_MASK,DEINTC_PRO_ANGLE_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_CS_ALG_SWITCH_MASK,DEINTC_CS_ALG_SWITCH_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_PS_QUAD_INTP_EN_MASK,DEINTC_PS_QUAD_INTP_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_SP_COEFF_SEL_MASK,DEINTC_SP_COEFF_SEL_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_PELCORR_EN_MASK,DEINTC_PELCORR_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_ALPHA_LIMIT_MASK,DEINTC_ALPHA_LIMIT_OFFSET,0x3f);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_DIS_MV_ANG_CALC_MASK,DEINTC_DIS_MV_ANG_CALC_OFFSET,0);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_MF_ALPHA_DEP_EN_MASK,DEINTC_MF_ALPHA_DEP_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_ANGLE_HOR_MF_EN_MASK,DEINTC_ANGLE_HOR_MF_EN_OFFSET,1);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL2,DEINTC_DCDI_MV_NL_LUT_EN_MASK,DEINTC_DCDI_MV_NL_LUT_EN_OFFSET,1);

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL3,DEINTC_DCDI_PELCORR_TH_MASK,DEINTC_DCDI_PELCORR_TH_OFFSET,0xb);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL3,DEINTC_DCDI_VAR_THRSH_MASK,DEINTC_DCDI_VAR_THRSH_OFFSET,3);
    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL3,DEINTC_DCDI_MV_NL_LUT_MASK,DEINTC_DCDI_MV_NL_LUT_OFFSET,0xFA50);

    DEINT_REG32_ClearAndSet(DEINTC_SV, DEINTC_SV_MASK,DEINTC_SV_OFFSET,0x40000);

    DEINT_REG32_ClearAndSet(DEINTC_OFFSET0, DEINTC_OFFSET0_MASK,DEINTC_OFFSET0_OFFSET,0x800);

    DEINT_REG32_ClearAndSet(DEINTC_OFFSET1, DEINTC_OFFSET1_MASK,DEINTC_OFFSET1_OFFSET,0x20800);


    MCMADI_REG32_ClearAndSet(MCMADI_CTRL,DEINT_CHROMA_EXPAND_MASK,DEINT_CHROMA_EXPAND_OFFSET,0);
    MCMADI_REG32_ClearAndSet(MCMADI_CTRL,DEINT_TEMP_EN_MASK,DEINT_TEMP_EN_OFFSET,0);
    MCMADI_REG32_ClearAndSet(MCMADI_CTRL,DEINT_TEMP_SEL_MASK,DEINT_TEMP_SEL_OFFSET,0);

    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);

}
/******************************************************************************
 FUNCTION       : Deint_Enable
 USAGE          : Enable and configure Deint/Dcdi related hardware
 INPUT          : Deint_Mode_t Mode - Select Mcdi mode or spatial
                 uint32_t In_Hsize - Input horizontal size
                 uint32_t In_Vsize - Input vertical size
                 uint32_t Out_Vsize -Output vertical size
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Deint_Enable (Deint_Mode_t Mode , uint32_t In_Hsize , uint32_t In_Vsize, uint32_t Out_Vsize)
{
#if (PROGRAM_MCDI_RW_REGS_IN_VCPU == FALSE)
    uint32_t temp;
#endif

    if (Mode == DEINT_MODE_MCDI || Mode == DEINT_MODE_SPATIAL)
    {
        DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_SPATIAL_EN_MASK, DEINTC_SPATIAL_EN_OFFSET, 1);
        DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_AFM_VT_MADI_SEL_MASK, DEINTC_AFM_VT_MADI_SEL_OFFSET, 3);
        DEINT_REG32_ClearAndSet(DEINTC_SV, DEINTC_SV_MASK,DEINTC_SV_OFFSET,0x40000);
    }
    else
    {
        DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_SPATIAL_EN_MASK, DEINTC_SPATIAL_EN_OFFSET, 0);
        DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_AFM_VT_MADI_SEL_MASK, DEINTC_AFM_VT_MADI_SEL_OFFSET, 1);
        DEINT_REG32_ClearAndSet(DEINTC_SV, DEINTC_SV_MASK,DEINTC_SV_OFFSET,0x20000);
    }
    //set SD_DH flag for SD input
    if (In_Hsize < 1024)
    {
        MCMADI_REG32_Set(MCMADI_CTRL,SD_HDN_SEL_MASK);
    }
    else
    {
        MCMADI_REG32_Clear(MCMADI_CTRL,SD_HDN_SEL_MASK);
    }

    DEINT_REG32_ClearAndSet(DEINT_IP_SIZE, DEINT_HACTIVE_MASK, DEINT_HACTIVE_OFFSET, In_Hsize);
    DEINT_REG32_ClearAndSet(DEINT_IP_SIZE2, DEINT_IP_VACTIVE_HEIGHT_MASK, DEINT_IP_VACTIVE_HEIGHT_OFFSET, In_Vsize);
    DEINT_REG32_ClearAndSet(DEINT_OP_SIZE, DEINT_OP_VACTIVE_HEIGHT_MASK, DEINT_OP_VACTIVE_HEIGHT_OFFSET, Out_Vsize);

    DEINT_REG32_ClearAndSet(DEINT_TOP_CTRL, DEINT_ENABLE_MASK, DEINT_ENABLE_OFFSET, DEINT_ENABLE_MASK);
    DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_INT_COEF_EN_MASK, DEINTC_INT_COEF_EN_OFFSET, DEINTC_INT_COEF_EN_MASK);
    MCMADI_REG32_Set(MCMADI_CTRL,DEINT_TEMP_EN_MASK);

#if (PROGRAM_MCDI_RW_REGS_IN_VCPU == TRUE)
    // (note: registers will be programmed in VCPU on flagline interrupt
    //  based on dimensions preprogrammed in DEINT_IP_SIZE and DEINT_IP_SIZE2)
#else
    MCDI_REG32_ClearAndSet(MCDI_CTRL2, HACTIVE_MASK, HACTIVE_OFFSET, In_Hsize);
    // Size after deinterlacing which is double the input
    MCDI_REG32_ClearAndSet(MCDI_CTRL2, VACTIVE_MASK, VACTIVE_OFFSET, 2*In_Vsize);

    // Calculate parameters based on input size
    temp = (MCDI_REG32_Read(LDME_CTRL3) & LDME_BST_MASK) >> LDME_BST_OFFSET;
    MCDI_REG32_ClearAndSet(LDME_CTRL3,    LDME_BSP_MASK, LDME_BSP_OFFSET, In_Hsize - temp -1);

    temp = (MCDI_REG32_Read(TDI_CTRL1) & TDI_HBST_MASK) >> TDI_HBST_OFFSET;
    MCDI_REG32_ClearAndSet(TDI_CTRL1,     TDI_HBSP_MASK, TDI_HBSP_OFFSET, In_Hsize - temp -1);

    temp = (MCDI_REG32_Read(TDI_CTRL1) & TDI_VBST_MASK) >> TDI_VBST_OFFSET;
    MCDI_REG32_ClearAndSet(TDI_CTRL2,    TDI_VBSP_MASK, TDI_VBSP_OFFSET,  2*In_Vsize -temp -1 );

    temp = (MCDI_REG32_Read(TDI_CTRL5) & DL_ST_MASK) >> DL_ST_OFFSET;
    MCDI_REG32_ClearAndSet(TDI_CTRL5,    DL_SP_MASK, DL_SP_OFFSET, In_Hsize -temp -1 );

    temp = (MCDI_REG32_Read(TDI_CTRL6) & DL_VST_MASK) >> DL_VST_OFFSET;
    MCDI_REG32_ClearAndSet(TDI_CTRL6,    DL_VSP_MASK, DL_VSP_OFFSET, 2*In_Vsize -temp -1);
#endif

    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Deint_Disable
 USAGE          : Disabling Deintarlacer, putting it back to spatial.
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Deint_Disable (void)
{
    DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_SPATIAL_EN_MASK, DEINTC_SPATIAL_EN_OFFSET, 1);
    DEINT_REG32_ClearAndSet(DEINTC_CTRL, DEINTC_AFM_VT_MADI_SEL_MASK, DEINTC_AFM_VT_MADI_SEL_OFFSET, 3);
    DEINT_REG32_ClearAndSet(DEINTC_SV, DEINTC_SV_MASK,DEINTC_SV_OFFSET,0x20000);
    MCMADI_REG32_Clear(MCMADI_CTRL,DEINT_TEMP_EN_MASK);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Mcdi_Enable
 USAGE          : Enable Mcdi_Enable, after initialization of changing vq data
 INPUT          : Mcdi_Mode_t Mode selectin madi only mcdi only or both
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Mcdi_Enable(Mcdi_Mode_t Mode)
{
    DEINT_REG32_ClearAndSet(MCDI_BLENDER_CTRL,MCDI_MODE_MASK,MCDI_MODE_OFFSET,Mode);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Mcdi_Disable
 USAGE          : Disables or suspends mcdi operation, bypassing mcdi block.
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Mcdi_Disable(void)
{

    DEINT_REG32_ClearAndSet(MCDI_BLENDER_CTRL,MCDI_MODE_MASK,MCDI_MODE_OFFSET,0);
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
}
/******************************************************************************
 FUNCTION       : Mcdi_SetVqData
 USAGE          : Disables software generation of frame reset, Clears enable bit and mask
 INPUT          : VQ_TNR_Adaptive_Parameters_t *vq_data_p - pointer to vqdata
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
FVDP_Result_t Mcdi_SetVqData(const VQ_Mcdi_Parameters_t *vq_data_p)
{
    if (vq_data_p == 0)
        return FVDP_ERROR;

    DEINT_REG32_ClearAndSet(MCDI_BLENDER_LUT ,MCDI_KLUT_0_MASK , MCDI_KLUT_0_OFFSET, vq_data_p-> MCMADI_Blender_KLUT_0);//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_0
    DEINT_REG32_ClearAndSet(MCDI_BLENDER_LUT ,MCDI_KLUT_1_MASK , MCDI_KLUT_1_OFFSET, vq_data_p-> MCMADI_Blender_KLUT_1);//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_1
    DEINT_REG32_ClearAndSet(MCDI_BLENDER_LUT ,MCDI_KLUT_2_MASK , MCDI_KLUT_2_OFFSET, vq_data_p-> MCMADI_Blender_KLUT_2);//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_2
    DEINT_REG32_ClearAndSet(MCDI_BLENDER_CTRL ,MCDI_KLUT_3_MASK , MCDI_KLUT_3_OFFSET, vq_data_p->    MCMADI_Blender_KLUT_3);//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_3
    MCDI_REG32_ClearAndSet(LDME_CTRL1 ,LDME_VTAP_SEL_MASK , LDME_VTAP_SEL_OFFSET, vq_data_p->    MCDI_LDME_VTAP_SEL);//	1 byte	0	3	1	Select one of these: 0 = no filter; 1 = 2 Tap filter; 2 = 5 Tap filter*/
    MCDI_REG32_ClearAndSet(LDME_CTRL2 ,LDME_BOFFSET_MASK , LDME_BOFFSET_OFFSET, vq_data_p->    MCDI_LDME_BOFFSET);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL2 ,LDME_HME_THR_MASK , LDME_HME_THR_OFFSET, vq_data_p->    MCDI_LDME_HME_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL2 ,LDME_VME_THR_MASK , LDME_VME_THR_OFFSET, vq_data_p->    MCDI_LDME_VME_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL2 ,LDME_DME_THR_MASK , LDME_DME_THR_OFFSET, vq_data_p->    MCDI_LDME_DME_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL3 ,LDME_THR_MASK , LDME_THR_OFFSET, vq_data_p->    MCDI_LDME_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL3 ,LDME_BST_MASK , LDME_BST_OFFSET, vq_data_p->    MCDI_LDME_BST);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(LDME_CTRL3 ,LDME_BSP_MASK , LDME_BSP_OFFSET, vq_data_p->    MCDI_LDME_BSP);//	2 bytes	0	4095	1
    MCDI_REG32_ClearAndSet(LDME_CTRL4 ,LDME_HBM_OST0_MASK , LDME_HBM_OST0_OFFSET, vq_data_p->    MCDI_LDME_HBM_OST0);//	2 bytes	-10	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on X axis. Typical value is: -5 -5 -4 -4 -3 -3 -2 -2 -1 -1 0
    MCDI_REG32_ClearAndSet(LDME_CTRL4 ,LDME_HBM_OST1_MASK , LDME_HBM_OST1_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST1);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL4 ,LDME_HBM_OST2_MASK , LDME_HBM_OST2_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST2);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL5 ,LDME_HBM_OST3_MASK , LDME_HBM_OST3_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST3);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL5 ,LDME_HBM_OST4_MASK , LDME_HBM_OST4_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST4);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL5 ,LDME_HBM_OST5_MASK , LDME_HBM_OST5_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST5);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL6 ,LDME_HBM_OST6_MASK , LDME_HBM_OST6_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST6);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL6 ,LDME_HBM_OST7_MASK , LDME_HBM_OST7_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST7);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL6 ,LDME_HBM_OST8_MASK , LDME_HBM_OST8_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST8);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL7 ,LDME_HBM_OST9_MASK , LDME_HBM_OST9_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST9);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL7 ,LDME_HBM_OST10_MASK , LDME_HBM_OST10_OFFSET, vq_data_p->  MCDI_LDME_HBM_OST10);//	2 bytes	-10	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL8 ,LDME_CNST_THR_LO_MASK , LDME_CNST_THR_LO_OFFSET, vq_data_p->  MCDI_LDME_CNST_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL8 ,LDME_CNST_THR_HI_MASK , LDME_CNST_THR_HI_OFFSET, vq_data_p->  MCDI_LDME_CNST_THR_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL8 ,LDME_HSTILL_THR_MASK , LDME_HSTILL_THR_OFFSET, vq_data_p->  MCDI_LDME_HSTILL_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL8 ,LDME_CNST_GAIN_MASK , LDME_CNST_GAIN_OFFSET, vq_data_p->  MCDI_LDME_CNST_GAIN);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL8 ,LDME_ST_MASK , LDME_ST_OFFSET, vq_data_p->  MCDI_LDME_ST);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(LDME_CTRL9 ,LDME_HME_BM_GAIN_MASK , LDME_HME_BM_GAIN_OFFSET, vq_data_p->  MCDI_LDME_HME_BM_GAIN);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL9 ,LDME_VME_BM_GAIN_MASK , LDME_VME_BM_GAIN_OFFSET, vq_data_p->  MCDI_LDME_VME_BM_GAIN);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL9 ,LDME_DME_BM_GAIN_MASK , LDME_DME_BM_GAIN_OFFSET, vq_data_p->  MCDI_LDME_DME_BM_GAIN);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL10 ,LDME_HME_NOISE_MASK , LDME_HME_NOISE_OFFSET, vq_data_p->  MCDI_LDME_HME_NOISE);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL11 ,LDME_VME_NOISE_MASK , LDME_VME_NOISE_OFFSET, vq_data_p->  MCDI_LDME_VME_NOISE);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL11 ,LDME_VCNST_NKL_MASK , LDME_VCNST_NKL_OFFSET, vq_data_p->  MCDI_LDME_VCNST_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL12 ,LDME_SF_THR_MASK , LDME_SF_THR_OFFSET, vq_data_p->  MCDI_LDME_SF_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL12 ,LDME_SF_THR_LO_MASK , LDME_SF_THR_LO_OFFSET, vq_data_p->  MCDI_LDME_SF_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL12 ,LDME_SF_THR_HI_MASK , LDME_SF_THR_HI_OFFSET, vq_data_p->  MCDI_LDME_SF_THR_HI);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(LDME_CTRL13 ,LDME_VBM_OST0_MASK , LDME_VBM_OST0_OFFSET, vq_data_p->  MCDI_LDME_VBM_OST0);//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost4 is at the center. There are 9 entries on y axis.Typical value is: -16 -8 -2 -1 0
    MCDI_REG32_ClearAndSet(LDME_CTRL13 ,LDME_VBM_OST1_MASK ,LDME_VBM_OST1_OFFSET, vq_data_p->  MCDI_LDME_VBM_OST1);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL13 ,LDME_VBM_OST2_MASK , LDME_VBM_OST2_OFFSET, vq_data_p->  MCDI_LDME_VBM_OST2);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL14 ,LDME_VBM_OST3_MASK , LDME_VBM_OST3_OFFSET, vq_data_p->  MCDI_LDME_VBM_OST3);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL14 ,LDME_VBM_OST4_MASK , LDME_VBM_OST4_OFFSET, vq_data_p->  MCDI_LDME_VBM_OST4);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_STILL_NKL_MASK , LDME_STILL_NKL_OFFSET, vq_data_p->  MCDI_LDME_STILL_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_STILL_THR_MASK , LDME_STILL_THR_OFFSET, vq_data_p->  MCDI_LDME_STILL_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_HME_DIFF_THR_MASK , LDME_HME_DIFF_THR_OFFSET, vq_data_p->  MCDI_LDME_HME_DIFF_THR);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_VME_DIFF_THR_MASK ,LDME_VME_DIFF_THR_OFFSET, vq_data_p->  MCDI_LDME_VME_DIFF_THR);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_DME_HME_DIFF_THR_MASK , LDME_DME_HME_DIFF_THR_OFFSET, vq_data_p->  MCDI_LDME_DME_HME_DIFF_THR);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(LDME_CTRL15 ,LDME_DME_VME_DIFF_THR_MASK , LDME_DME_VME_DIFF_THR_OFFSET, vq_data_p->  MCDI_LDME_DME_VME_DIFF_THR);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_DME_ST_MASK , LDME_DME_ST_OFFSET, vq_data_p->  MCDI_LDME_DME_ST);//	1 byte	0	15	1	LDME_DME_ST
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_VME_ST_MASK , LDME_VME_ST_OFFSET, vq_data_p->  MCDI_LDME_VME_ST);//	1 byte	0	15	1	LDME_VME_ST
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_SF_HST_MASK , LDME_SF_HST_OFFSET, vq_data_p->  MCDI_LDME_SF_HST);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_SF_VST_MASK , LDME_SF_VST_OFFSET, vq_data_p->  MCDI_LDME_SF_VST);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_GSTILL_THR_MASK , LDME_GSTILL_THR_OFFSET, vq_data_p->  MCDI_LDME_GSTILL_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL16 ,LDME_BSTILL_THR_MASK , LDME_BSTILL_THR_OFFSET, vq_data_p->  MCDI_LDME_BSTILL_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_HME_G3_EN_MASK , LDME_HME_G3_EN_OFFSET, vq_data_p->  MCDI_LDME_HME_G3_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_VME_G3_EN_MASK , LDME_VME_G3_EN_OFFSET, vq_data_p->  MCDI_LDME_VME_G3_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_DME_G3_EN_MASK , LDME_DME_G3_EN_OFFSET, vq_data_p->  MCDI_LDME_DME_G3_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_GSTILL_ENABLE_MASK , LDME_GSTILL_ENABLE_OFFSET, vq_data_p->  MCDI_LDME_GSTILL_ENABLE);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_FILM_ENABLE_MASK , LDME_FILM_ENABLE_OFFSET, vq_data_p->  MCDI_LDME_FILM_ENABLE);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_GHF_EN_MASK , LDME_GHF_EN_OFFSET, vq_data_p->  MCDI_LDME_GHF_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_GMOVE_EN_MASK , LDME_GMOVE_EN_OFFSET, vq_data_p->  MCDI_LDME_GMOVE_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL17 ,LDME_SCENE_EN_MASK , LDME_SCENE_EN_OFFSET, vq_data_p->  MCDI_LDME_SCENE_EN);//	bool
    MCDI_REG32_ClearAndSet(LDME_CTRL18 ,LDME_GMOVE_HACC_GAIN_HI_MASK , LDME_GMOVE_HACC_GAIN_HI_OFFSET, vq_data_p->  MCDI_LDME_GMOVE_HACC_GAIN_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL18 ,LDME_GMOVE_HACC_GAIN_LO_MASK , LDME_GMOVE_HACC_GAIN_LO_OFFSET, vq_data_p->  MCDI_LDME_GMOVE_HACC_GAIN_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL18 ,LDME_GMOVE_VACC_GAIN_HI_MASK , LDME_GMOVE_VACC_GAIN_HI_OFFSET, vq_data_p->  MCDI_LDME_GMOVE_VACC_GAIN_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL18 ,LDME_GMOVE_VACC_GAIN_LO_MASK , LDME_GMOVE_VACC_GAIN_LO_OFFSET, vq_data_p->  MCDI_LDME_GMOVE_VACC_GAIN_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL19 ,LDME_ACC_THR_HI_MASK , LDME_ACC_THR_HI_OFFSET, vq_data_p->  MCDI_LDME_ACC_THR_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL19 ,LDME_ACC_THR_LO_MASK , LDME_ACC_THR_LO_OFFSET, vq_data_p->  MCDI_LDME_ACC_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL19 ,LDME_GHF_ACC_THR_MASK , LDME_GHF_ACC_THR_OFFSET, vq_data_p->  MCDI_LDME_GHF_ACC_THR);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(LDME_CTRL20 ,LDME_LGM_THR_MASK , LDME_LGM_THR_OFFSET, vq_data_p->  MCDI_LDME_LGM_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL20 ,LDME_LGM_OFF_MASK , LDME_LGM_OFF_OFFSET, vq_data_p->  MCDI_LDME_LGM_OFF);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LDME_CTRL20 ,LDME_FIX_MIX_FILM_EN_MASK , LDME_FIX_MIX_FILM_EN_OFFSET, vq_data_p->  MCDI_LDME_FIX_MIX_FILM_EN);//	bool			1	Mix_film_en; the register is 1 byte but only use bit0
    MCDI_REG32_ClearAndSet(LMVD_CTRL1 ,LMVD_HLG_BTHR_MASK , LMVD_HLG_BTHR_OFFSET, vq_data_p->  MCDI_LMVD_HLG_BTHR);//	1 byte	0	127	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL1 ,LDME_HLG_BTHR_MASK , LDME_HLG_BTHR_OFFSET, vq_data_p->  MCDI_LDME_HLG_BTHR);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL1 ,LMVD_HLG_LTHR_MASK , LMVD_HLG_LTHR_OFFSET, vq_data_p->  MCDI_LMVD_HLG_LTHR);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL1 ,LMVD_HLG_GTHR_MASK , LMVD_HLG_GTHR_OFFSET, vq_data_p->  MCDI_LMVD_HLG_GTHR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL2 ,LMVD_HLG_OST0_MASK , LMVD_HLG_OST0_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST0);//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and os10 is at the center. There are 21 entries on x axis.Typical value is:-10 -9 -8 -7 -6 -5 -4 -3 -2 -1 0
    MCDI_REG32_ClearAndSet(LMVD_CTRL2 ,LMVD_HLG_OST1_MASK , LMVD_HLG_OST1_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST1);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL2 ,LMVD_HLG_OST2_MASK , LMVD_HLG_OST2_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST2);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL3 ,LMVD_HLG_OST3_MASK , LMVD_HLG_OST3_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST3);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL3 ,LMVD_HLG_OST4_MASK , LMVD_HLG_OST4_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST4);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL3 ,LMVD_HLG_OST5_MASK , LMVD_HLG_OST5_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST5);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL4 ,LMVD_HLG_OST6_MASK , LMVD_HLG_OST6_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST6);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL4 ,LMVD_HLG_OST7_MASK , LMVD_HLG_OST7_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST7);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL4 ,LMVD_HLG_OST8_MASK , LMVD_HLG_OST8_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST8);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL5 ,LMVD_HLG_OST9_MASK , LMVD_HLG_OST9_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST9);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL5 ,LMVD_HLG_OST10_MASK , LMVD_HLG_OST10_OFFSET, vq_data_p->  MCDI_LMVD_HLG_OST10);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL6 ,LDME_HLG_OST0_MASK , LDME_HLG_OST0_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST0);//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on x axis.Typical value is:-20 -20 -18 -16 -14 -12 -10 -8 -2 -1 0
    MCDI_REG32_ClearAndSet(LMVD_CTRL6 ,LDME_HLG_OST1_MASK , LDME_HLG_OST1_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST1);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL6 ,LDME_HLG_OST2_MASK , LDME_HLG_OST2_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST2);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL7 ,LDME_HLG_OST3_MASK , LDME_HLG_OST3_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST3);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL7 ,LDME_HLG_OST4_MASK , LDME_HLG_OST4_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST4);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL7 ,LDME_HLG_OST5_MASK , LDME_HLG_OST5_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST5);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL8 ,LDME_HLG_OST6_MASK , LDME_HLG_OST6_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST6);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL8 ,LDME_HLG_OST7_MASK , LDME_HLG_OST7_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST7);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL8 ,LDME_HLG_OST8_MASK , LDME_HLG_OST8_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST8);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL9 ,LDME_HLG_OST9_MASK , LDME_HLG_OST9_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST9);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL9 ,LDME_HLG_OST10_MASK , LDME_HLG_OST10_OFFSET, vq_data_p->  MCDI_LDME_HLG_OST10);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL9 ,LDME_HLG_NKL_MASK , LDME_HLG_NKL_OFFSET, vq_data_p->  MCDI_LDME_HLG_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL10 ,LMVD_HLG_NOISE_MASK , LMVD_HLG_NOISE_OFFSET, vq_data_p->  MCDI_LMVD_HLG_NOISE);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL10 ,LMVD_HLG_THR_LO_MASK , LMVD_HLG_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_HLG_THR_LO);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL10 ,LMVD_HLG_THR_HI_MASK , LMVD_HLG_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_HLG_THR_HI);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL10 ,LMVD_HLG_NKL_MASK , LMVD_HLG_NKL_OFFSET, vq_data_p->  MCDI_LMVD_HLG_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL10 ,LMVD_HLG_CNST_NKL_MASK , LMVD_HLG_CNST_NKL_OFFSET, vq_data_p->  MCDI_LMVD_HLG_CNST_NKL);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 , LMVD_HLG_STILL_NKL_MASK, LMVD_HLG_STILL_NKL_OFFSET, vq_data_p->  MCDI_LMVD_HLG_STILL_NKL);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 ,LMVD_LMD_LV_ENABLE_MASK , LMVD_LMD_LV_ENABLE_OFFSET, vq_data_p->  MCDI_LMVD_LMD_LV_ENABLE);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 ,LMVD_LMD_HLV_THR_LO_MASK , LMVD_LMD_HLV_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_LMD_HLV_THR_LO);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 ,LMVD_LMD_HLV_THR_HI_MASK , LMVD_LMD_HLV_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_LMD_HLV_THR_HI);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 ,LMVD_LMD_VLV_THR_LO_MASK , LMVD_LMD_VLV_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_LMD_VLV_THR_LO);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL11 ,LMVD_LMD_VLV_THR_HI_MASK , LMVD_LMD_VLV_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_LMD_VLV_THR_HI);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL12 ,LMVD_VLVD_GAIN_HI_MASK ,LMVD_VLVD_GAIN_HI_OFFSET, vq_data_p->  MCDI_LMVD_VLVD_GAIN_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL12,LMVD_VLVD_GAIN_LO_MASK , LMVD_VLVD_GAIN_LO_OFFSET, vq_data_p->  MCDI_LMVD_VLVD_GAIN_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL12 ,LMVD_HLVD_GAIN_HI_MASK , LMVD_HLVD_GAIN_HI_OFFSET, vq_data_p->  MCDI_LMVD_HLVD_GAIN_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL12 ,LMVD_HLVD_GAIN_LO_MASK , LMVD_HLVD_GAIN_LO_OFFSET, vq_data_p->  MCDI_LMVD_HLVD_GAIN_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_HLVD_OFFSET_MASK , LMVD_HLVD_OFFSET_OFFSET, vq_data_p->  MCDI_LMVD_HLVD_OFFSET);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_VLVD_OFFSET_MASK , LMVD_VLVD_OFFSET_OFFSET, vq_data_p->  MCDI_LMVD_VLVD_OFFSET);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_VMV_THR_LO_MASK , LMVD_VMV_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_VMV_THR_LO);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_VMV_THR_HI_MASK , LMVD_VMV_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_VMV_THR_HI);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_HMV_THR_LO_MASK , LMVD_HMV_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_HMV_THR_LO);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_HMV_THR_HI_MASK , LMVD_HMV_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_HMV_THR_HI);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL13 ,LMVD_LHMV_EN_MASK , LMVD_LHMV_EN_OFFSET, vq_data_p->  MCDI_LMVD_LHMV_EN);//	bool
    MCDI_REG32_ClearAndSet(LMVD_CTRL14 ,LMVD_HST_MASK , LMVD_HST_OFFSET, vq_data_p->  MCDI_LMVD_HST);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL14 ,LMVD_VST_MASK ,LMVD_VST_OFFSET, vq_data_p->  MCDI_LMVD_VST);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL14 ,LMVD_GSTILL_THR_MASK , LMVD_GSTILL_THR_OFFSET, vq_data_p->  MCDI_LMVD_GSTILL_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL14 ,LMVD_GSTILL_THR_LO_MASK , LMVD_GSTILL_THR_LO_OFFSET, vq_data_p->  MCDI_LMVD_GSTILL_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,LMVD_GSTILL_THR_HI_MASK , LMVD_GSTILL_THR_HI_OFFSET, vq_data_p->  MCDI_LMVD_GSTILL_THR_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,LMVD_GSTILL_GAIN_LO_MASK , LMVD_GSTILL_GAIN_LO_OFFSET, vq_data_p->  MCDI_LMVD_GSTILL_GAIN_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,LMVD_GSTILL_GAIN_HI_MASK , LMVD_GSTILL_GAIN_HI_OFFSET, vq_data_p->  MCDI_LMVD_GSTILL_GAIN_HI);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,MVD_GSTIL_COUNT_CTRL_MASK , MVD_GSTIL_COUNT_CTRL_OFFSET, vq_data_p->  MCDI_MVD_GSTIL_COUNT_CTRL);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,LMVD_DIV_MORE_MASK , LMVD_DIV_MORE_OFFSET, vq_data_p->  MCDI_LMVD_DIV_MORE);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(LMVD_CTRL15 ,LDME_DIV_MORE_MASK , LDME_DIV_MORE_OFFSET, vq_data_p->  MCDI_LDME_DIV_MORE);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(SDI_CTRL1 ,SDI_NOISE_MASK , SDI_NOISE_OFFSET, vq_data_p->  MCDI_SDI_NOISE);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(SDI_CTRL1 ,SDI_THR_MASK , SDI_THR_OFFSET, vq_data_p->  MCDI_SDI_THR);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(SDI_CTRL1 ,SDI_HI_THR_MASK , SDI_HI_THR_OFFSET, vq_data_p->  MCDI_SDI_HI_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(SDI_CTRL1 ,SDI_CNST_THR_MASK , SDI_CNST_THR_OFFSET, vq_data_p->  MCDI_SDI_CNST_THR);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(SDI_CTRL2 ,SDI_OST0_MASK , SDI_OST0_OFFSET, vq_data_p->  MCDI_SDI_OST0);//	2 bytes	0	32	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost5 is at the center. There are 9 entries on x axis.Typical value is:0 0 0 0 0 25
    MCDI_REG32_ClearAndSet(SDI_CTRL2 ,SDI_OST1_MASK , SDI_OST1_OFFSET, vq_data_p->  MCDI_SDI_OST1);//	2 bytes	0	32	1
    MCDI_REG32_ClearAndSet(SDI_CTRL2 ,SDI_OST2_MASK , SDI_OST2_OFFSET, vq_data_p->  MCDI_SDI_OST2);//	2 bytes	0	32	1
    MCDI_REG32_ClearAndSet(SDI_CTRL3 ,SDI_OST3_MASK , SDI_OST3_OFFSET, vq_data_p->  MCDI_SDI_OST3);//	2 bytes	0	32	1
    MCDI_REG32_ClearAndSet(SDI_CTRL3 ,SDI_OST4_MASK , SDI_OST4_OFFSET, vq_data_p->  MCDI_SDI_OST4);//	2 bytes	0	32	1
    MCDI_REG32_ClearAndSet(SDI_CTRL3 ,SDI_OST5_MASK , SDI_OST5_OFFSET, vq_data_p->  MCDI_SDI_OST5);//	2 bytes	0	32	1
    MCDI_REG32_ClearAndSet(SDI_CTRL4 ,SDI_OFFSET_MASK , SDI_OFFSET_OFFSET, vq_data_p->  MCDI_SDI_OFFSET);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(SDI_CTRL4 ,SDI_CNST_GAIN_MASK , SDI_CNST_GAIN_OFFSET, vq_data_p->  MCDI_SDI_CNST_GAIN);//	1 byte	0	255	1	Link to the register
    MCDI_REG32_ClearAndSet(SDI_CTRL4 ,SDI_HI_GAIN_MASK , SDI_HI_GAIN_OFFSET, vq_data_p->  MCDI_SDI_HI_GAIN);//	1 byte	0	255	1	Link to the register
    MCDI_REG32_ClearAndSet(MCDI_CTRL1 ,HSDME_FRAME_BYPASS_MASK , HSDME_FRAME_BYPASS_OFFSET, vq_data_p->  MCDI_HSDME_Frame_Bypass);//	bool				SDME_HOR_EN button
    MCDI_REG32_ClearAndSet(MCDI_CTRL1 ,HSDME_FIELD_BYPASS_MASK , HSDME_FIELD_BYPASS_OFFSET, vq_data_p->  MCDI_HSDME_Field_Bypass);//	bool				SDME_VER_EN button

    MCDI_REG32_ClearAndSet(SDME_CTRL2 ,HSDME_FRAME_NOISE_MASK , HSDME_FRAME_NOISE_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_NOISE);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(SDME_CTRL2 ,HSDME_FIELD_NOISE_MASK , HSDME_FIELD_NOISE_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_NOISE);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(SDME_CTRL2 ,HSDME_ST_MASK , HSDME_ST_OFFSET, vq_data_p->  MCDI_HSDME_ST);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(SDME_CTRL2 ,HSDME_STILL_NOISE_MASK , HSDME_STILL_NOISE_OFFSET, vq_data_p->  MCDI_HSDME_STILL_NOISE);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(SDME_CTRL2 ,VSDME_FRAME_NOISE_MASK , VSDME_FRAME_NOISE_OFFSET, vq_data_p->  MCDI_VSDME_FRAME_NOISE);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(MCDI_CTRL1 ,VSDME_FRAME_BYPASS_MASK , VSDME_FRAME_BYPASS_OFFSET, vq_data_p->  MCDI_VSDME_Bypass);//	bool				VSDME_EN button
    MCDI_REG32_ClearAndSet(SDME_CTRL3 ,HSDME_FRAME_OST0_MASK , HSDME_FRAME_OST0_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST0);//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center. There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    MCDI_REG32_ClearAndSet(SDME_CTRL3 ,HSDME_FRAME_OST1_MASK , HSDME_FRAME_OST1_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST1);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL3 ,HSDME_FRAME_OST2_MASK , HSDME_FRAME_OST2_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST2);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL4 ,HSDME_FRAME_OST3_MASK , HSDME_FRAME_OST3_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST3);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL4 ,HSDME_FRAME_OST4_MASK , HSDME_FRAME_OST4_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST4);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL4 ,HSDME_FRAME_OST5_MASK , HSDME_FRAME_OST5_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST5);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL5 ,HSDME_FRAME_OST6_MASK , HSDME_FRAME_OST6_OFFSET, vq_data_p->  MCDI_HSDME_FRAME_OST6);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL6 ,HSDME_FIELD_OST0_MASK , HSDME_FIELD_OST0_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST0);//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center. There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    MCDI_REG32_ClearAndSet(SDME_CTRL6 ,HSDME_FIELD_OST1_MASK , HSDME_FIELD_OST1_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST1);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL6 ,HSDME_FIELD_OST2_MASK , HSDME_FIELD_OST2_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST2);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL7 ,HSDME_FIELD_OST3_MASK , HSDME_FIELD_OST3_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST3);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL7 ,HSDME_FIELD_OST4_MASK , HSDME_FIELD_OST4_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST4);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL7 ,HSDME_FIELD_OST5_MASK , HSDME_FIELD_OST5_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST5);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL8 ,HSDME_FIELD_OST6_MASK , HSDME_FIELD_OST6_OFFSET, vq_data_p->  MCDI_HSDME_FIELD_OST6);//	2 bytes	-32	0	1
    MCDI_REG32_ClearAndSet(SDME_CTRL9 ,VSDME_FRAME_OST0_MASK , VSDME_FRAME_OST0_OFFSET, vq_data_p->  MCDI_VSDME_FRAME_OST0);//	2 bytes	0	255	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost2 is at the center. There are 5 entries on y axis.Typical value is:-16 -8 0
    MCDI_REG32_ClearAndSet(SDME_CTRL9 ,VSDME_FRAME_OST1_MASK , VSDME_FRAME_OST1_OFFSET, vq_data_p->  MCDI_VSDME_FRAME_OST1);//	2 bytes	0	255	1
    MCDI_REG32_ClearAndSet(SDME_CTRL9 ,VSDME_FRAME_OST2_MASK , VSDME_FRAME_OST2_OFFSET, vq_data_p->  MCDI_VSDME_FRAME_OST2);//	2 bytes	0	255	1

    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_TNR_BYPASS_MASK , TDI_TNR_BYPASS_OFFSET, vq_data_p->  MCDI_TDI_TNR_BYPASS);//	bool
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_INT_TNR_BYPASS_MASK , TDI_INT_TNR_BYPASS_OFFSET, vq_data_p->  MCDI_TDI_INT_TNR_BYPASS);//	bool
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_SNR_BYPASS_MASK , TDI_SNR_BYPASS_OFFSET, vq_data_p->  MCDI_TDI_SNR_BYPASS);//	bool
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_OFFSET_MASK , TDI_OFFSET_OFFSET, vq_data_p->  MCDI_TDI_OFFSET);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_HBST_MASK , TDI_HBST_OFFSET, vq_data_p->  MCDI_TDI_HBST);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_HBSP_MASK , TDI_HBSP_OFFSET, vq_data_p->  MCDI_TDI_HBSP);//	2 bytes	0	4095	1
    MCDI_REG32_ClearAndSet(TDI_CTRL1 ,TDI_VBST_MASK , TDI_VBST_OFFSET, vq_data_p->  MCDI_TDI_VBST);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL2 ,TDI_VBSP_MASK , TDI_VBSP_OFFSET, vq_data_p->  MCDI_TDI_VBSP);//	2 bytes	0	4095	1
    MCDI_REG32_ClearAndSet(TDI_CTRL2 ,TDI_IMPULSE_NKL_MASK , TDI_IMPULSE_NKL_OFFSET, vq_data_p->  MCDI_TDI_IMPULSE_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL2 ,TDI_IMPULSE_THR_MASK , TDI_IMPULSE_THR_OFFSET, vq_data_p->  MCDI_TDI_IMPULSE_THR);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(TDI_CTRL3 ,TDI_KLUT0_MASK , TDI_KLUT0_OFFSET, vq_data_p->  MCDI_TDI_KLUT0);//	1 byte	0	255	1	Map to graphics. Typical value: 255 200 150 100
    MCDI_REG32_ClearAndSet(TDI_CTRL3 ,TDI_KLUT1_MASK , TDI_KLUT1_OFFSET, vq_data_p->  MCDI_TDI_KLUT1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL3 ,TDI_KLUT2_MASK , TDI_KLUT2_OFFSET, vq_data_p->  MCDI_TDI_KLUT2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL3 ,TDI_KLUT3_MASK , TDI_KLUT3_OFFSET, vq_data_p->  MCDI_TDI_KLUT3);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL4 ,TDI_SNR_KLUT0_MASK , TDI_SNR_KLUT0_OFFSET, vq_data_p->  MCDI_TDI_SNR_KLUT0);//	1 byte	0	255	1	Map to graphics. Typical value: 255 128 0 0
    MCDI_REG32_ClearAndSet(TDI_CTRL4 ,TDI_SNR_KLUT1_MASK , TDI_SNR_KLUT1_OFFSET, vq_data_p->  MCDI_TDI_SNR_KLUT1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL4 ,TDI_SNR_KLUT2_MASK , TDI_SNR_KLUT2_OFFSET, vq_data_p->  MCDI_TDI_SNR_KLUT2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(TDI_CTRL4 ,TDI_SNR_KLUT3_MASK , TDI_SNR_KLUT3_OFFSET, vq_data_p->  MCDI_TDI_SNR_KLUT3);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL1 ,CL_MDET_THR0_MASK , CL_MDET_THR0_OFFSET, vq_data_p->  MCDI_CL_MDET_THR0);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL1 ,CL_MDET_THR1_MASK , CL_MDET_THR1_OFFSET, vq_data_p->  MCDI_CL_MDET_THR1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL1 ,CL_MDET_THR2_MASK , CL_MDET_THR2_OFFSET, vq_data_p->  MCDI_CL_MDET_THR2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL2 ,CL_VMV_GAIN_MASK , CL_VMV_GAIN_OFFSET, vq_data_p->  MCDI_CL_VMV_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL2 ,CL_HMV_GAIN_MASK , CL_HMV_GAIN_OFFSET, vq_data_p->  MCDI_CL_HMV_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL2 ,CL_NMV_GAIN_MASK , CL_NMV_GAIN_OFFSET, vq_data_p->  MCDI_CL_NMV_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL2 ,CL_SMALL_GAIN_MASK , CL_SMALL_GAIN_OFFSET, vq_data_p->  MCDI_CL_SMALL_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL3 ,CL_SMALL_GAIN_HI_MASK , CL_SMALL_GAIN_HI_OFFSET, vq_data_p->  MCDI_CL_SMALL_GAIN_H);//I	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL3 ,CL_SMALL_GAIN_LO_MASK , CL_SMALL_GAIN_LO_OFFSET, vq_data_p->  MCDI_CL_SMALL_GAIN_L);//O	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL3 ,CL_LARGE_GAIN_MASK , CL_LARGE_GAIN_OFFSET, vq_data_p->  MCDI_CL_LARGE_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL3 ,CL_DEFAULT_GAIN_MASK , CL_DEFAULT_GAIN_OFFSET, vq_data_p->  MCDI_CL_DEFAULT_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL4 ,CL_CNST_NKL_MASK , CL_CNST_NKL_OFFSET, vq_data_p->  MCDI_CL_CNST_NKL);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL4 ,CL_CNST_GAIN_MASK , CL_CNST_GAIN_OFFSET, vq_data_p->  MCDI_CL_CNST_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL4 ,CL_CNST_OFF_MASK , CL_CNST_OFF_OFFSET, vq_data_p->  MCDI_CL_CNST_OFF);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL4 ,CL_LMD_OFF_MASK , CL_LMD_OFF_OFFSET, vq_data_p->  MCDI_CL_LMD_OFF);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL4 ,CL_ANG_OFF_MASK , CL_ANG_OFF_OFFSET, vq_data_p->  MCDI_CL_ANG_OFF);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_MAX_ANG_OFF_MASK , CL_MAX_ANG_OFF_OFFSET, vq_data_p->  MCDI_CL_MAX_ANG_OFF);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_SMALL_OFF_MASK , CL_SMALL_OFF_OFFSET, vq_data_p->  MCDI_CL_SMALL_OFF);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_SMALL_OFF_LO_MASK , CL_SMALL_OFF_LO_OFFSET, vq_data_p->  MCDI_CL_SMALL_OFF_LO);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_SMALL_OFF_HI_MASK , CL_SMALL_OFF_HI_OFFSET, vq_data_p->  MCDI_CL_SMALL_OFF_HI);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_HTRAN_OFF_MASK , CL_HTRAN_OFF_OFFSET, vq_data_p->  MCDI_CL_HTRAN_OFF);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL5 ,CL_VTRAN_OFF_MASK , CL_VTRAN_OFF_OFFSET, vq_data_p->  MCDI_CL_VTRAN_OFF);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL6 ,CL_GSTILL_OFF_MASK , CL_GSTILL_OFF_OFFSET, vq_data_p->  MCDI_CL_GSTILL_OFF);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL6 ,CL_LAMD_NKL_MASK , CL_LAMD_NKL_OFFSET, vq_data_p->  MCDI_CL_LAMD_NKL);//1 byte	0	63	1
    MCDI_REG32_ClearAndSet(AR_CTRL6 ,CL_LAMD_SMALL_NKL_MASK , CL_LAMD_SMALL_NKL_OFFSET, vq_data_p->  MCDI_CL_LAMD_SMALL_NKL);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL6 ,CL_LAMD_BIG_NKL_MASK , CL_LAMD_BIG_NKL_OFFSET, vq_data_p->  MCDI_CL_LAMD_BIG_NKL);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(AR_CTRL7 ,CL_LAMD_HTHR_MASK , CL_LAMD_HTHR_OFFSET, vq_data_p->  MCDI_CL_LAMD_HTHR);//	1 byte	0	31	1
    MCDI_REG32_ClearAndSet(AR_CTRL7 ,CL_LAMD_GAIN_MASK , CL_LAMD_GAIN_OFFSET, vq_data_p->  MCDI_CL_LAMD_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL7 ,CL_DIAG_GAIN_MASK , CL_DIAG_GAIN_OFFSET, vq_data_p->  MCDI_CL_DIAG_GAIN);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL7 ,CL_DIAG_GAIN1_MASK , CL_DIAG_GAIN1_OFFSET, vq_data_p->  MCDI_CL_DIAG_GAIN1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL8 ,CL_DIAG_GAIN2_MASK , CL_DIAG_GAIN2_OFFSET, vq_data_p->  MCDI_CL_DIAG_GAIN2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL8 ,CL_HTRAN_THR_LO_MASK , CL_HTRAN_THR_LO_OFFSET, vq_data_p->  MCDI_CL_HTRAN_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL8 ,CL_HTRAN_THR_HI_MASK , CL_HTRAN_THR_HI_OFFSET, vq_data_p->  MCDI_CL_HTRAN_THR_HI);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(AR_CTRL9 ,CL_VTRAN_THR_LO_MASK , CL_VTRAN_THR_LO_OFFSET, vq_data_p->  MCDI_CL_VTRAN_THR_LO);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL9 ,CL_VTRAN_THR_HI_MASK , CL_VTRAN_THR_HI_OFFSET, vq_data_p->  MCDI_CL_VTRAN_THR_HI);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(AR_CTRL9 ,CL_HTRAN_GAIN_MASK , CL_HTRAN_GAIN_OFFSET, vq_data_p->  MCDI_CL_HTRAN_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL10 ,CL_HTRAN_GAIN1_MASK , CL_HTRAN_GAIN1_OFFSET, vq_data_p->  MCDI_CL_HTRAN_GAIN1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL10 ,CL_VTRAN_GAIN_MASK , CL_VTRAN_GAIN_OFFSET, vq_data_p->  MCDI_CL_VTRAN_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL10 ,CL_VTRAN_GAIN1_MASK , CL_VTRAN_GAIN1_OFFSET, vq_data_p->  MCDI_CL_VTRAN_GAIN1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL10 ,CL_GSTILL_GAIN_MASK , CL_GSTILL_GAIN_OFFSET, vq_data_p->  MCDI_CL_GSTILL_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL11 ,CL_GHF_GAIN_MASK , CL_GHF_GAIN_OFFSET, vq_data_p->  MCDI_CL_GHF_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL12 ,CL_GSTILL_GAIN0AND1_MASK , CL_GSTILL_GAIN0AND1_OFFSET, vq_data_p->  MCDI_CL_GSTILL_GAIN0AND1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL12 ,CL_GSTILL_GAIN0OR1_MASK , CL_GSTILL_GAIN0OR1_OFFSET, vq_data_p->  MCDI_CL_GSTILL_GAIN0OR1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL12 ,CL_ANG_FILM_GAIN_MASK , CL_ANG_FILM_GAIN_OFFSET, vq_data_p->  MCDI_CL_ANG_FILM_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL12 ,CL_FILM_GAIN3AND5_MASK , CL_FILM_GAIN3AND5_OFFSET, vq_data_p->  MCDI_CL_FILM_GAIN3AND5);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL13 ,CL_LASD_NKL_MASK , CL_LASD_NKL_OFFSET, vq_data_p->  MCDI_CL_LASD_NKL);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(AR_CTRL13 ,CL_LASD_THR_MASK , CL_LASD_THR_OFFSET, vq_data_p->  MCDI_CL_LASD_THR);//	1 byte	0	63	1
    MCDI_REG32_ClearAndSet(AR_CTRL13 ,CL_LASD_GAIN_MASK , CL_LASD_GAIN_OFFSET, vq_data_p->  MCDI_CL_LASD_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL13 ,CL_DMV_GAIN_MASK , CL_DMV_GAIN_OFFSET, vq_data_p->  MCDI_CL_DMV_GAIN);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_STILL_ANG_OFF_MASK , CL_STILL_ANG_OFF_OFFSET, vq_data_p->  MCDI_CL_STILL_ANG_OFF);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_STILL_ANG_EN_MASK , CL_STILL_ANG_EN_OFFSET, vq_data_p->  MCDI_CL_STILL_ANG_EN);//	bool
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_STILL_ANG_GAIN_MASK , CL_STILL_ANG_GAIN_OFFSET, vq_data_p->  MCDI_CL_STILL_ANG_GAIN);//	2 bytes	0	1023	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_DIAG_HTHR_MASK  , CL_DIAG_HTHR_OFFSET, vq_data_p->  MCDI_CL_DIAG_HTHR);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_DIAG_VTHR_MASK , CL_DIAG_VTHR_OFFSET, vq_data_p->  MCDI_CL_DIAG_VTHR);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_DIAG_HTHR_HI_MASK , CL_DIAG_HTHR_HI_OFFSET, vq_data_p->  MCDI_CL_DIAG_HTHR_HI);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(AR_CTRL14 ,CL_DIAG_VTHR_LO_MASK , CL_DIAG_VTHR_LO_OFFSET, vq_data_p->  MCDI_CL_DIAG_VTHR_LO);//	1 byte	0	7	1
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_GSTILL_THR_HI_MASK , CL_GSTILL_THR_HI_OFFSET, vq_data_p->  MCDI_CL_GSTILL_THR_HI);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_GSTILL_THR_LO_MASK , CL_GSTILL_THR_LO_OFFSET, vq_data_p->  MCDI_CL_GSTILL_THR_LO);//	1 byte	0	15	1
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_DIAG_FILM_EN_MASK , CL_DIAG_FILM_EN_OFFSET, vq_data_p->  MCDI_CL_DIAG_FILM_EN);//	bool
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_OK_PATCH_EN_MASK , CL_OK_PATCH_EN_OFFSET, vq_data_p->  MCDI_CL_OK_PATCH_EN);//	bool
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_DIAG_STILL_EN_MASK , CL_DIAG_STILL_EN_OFFSET, vq_data_p->  MCDI_CL_DIAG_STILL_EN);//	bool
    MCDI_REG32_ClearAndSet(AR_CTRL15 ,CL_GHF_EN_MASK , CL_GHF_EN_OFFSET, vq_data_p->  MCDI_CL_GHF_EN);//	bool
    MCDI_REG32_ClearAndSet(AR_CTRL16 ,CL_MVCL_THR0_MASK , CL_MVCL_THR0_OFFSET, vq_data_p->  MCDI_CL_MVCL_THR0);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL16 ,CL_MVCL_THR1_MASK , CL_MVCL_THR1_OFFSET, vq_data_p->  MCDI_CL_MVCL_THR1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL16 ,CL_MVCL_THR2_MASK , CL_MVCL_THR2_OFFSET, vq_data_p->  MCDI_CL_MVCL_THR2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT0_MASK , CL_MVCL_LUT0_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT0);//	1 byte	0	3	1	AR_CTRL18-> CL_MVCL_LUT0
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT1_MASK , CL_MVCL_LUT1_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT1);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT2_MASK , CL_MVCL_LUT2_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT2);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT3_MASK , CL_MVCL_LUT3_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT3);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT4_MASK , CL_MVCL_LUT4_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT4);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT5_MASK , CL_MVCL_LUT5_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT5);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT6_MASK , CL_MVCL_LUT6_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT6);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT7_MASK , CL_MVCL_LUT7_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT7);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT8_MASK , CL_MVCL_LUT8_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT8);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT9_MASK , CL_MVCL_LUT9_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT9);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT10_MASK , CL_MVCL_LUT10_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT10);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT11_MASK , CL_MVCL_LUT11_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT11);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT12_MASK , CL_MVCL_LUT12_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT12);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT13_MASK , CL_MVCL_LUT13_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT13);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT14_MASK , CL_MVCL_LUT14_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT14);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL17 ,CL_MVCL_LUT15_MASK , CL_MVCL_LUT15_OFFSET, vq_data_p->  MCDI_CL_MVCL_LUT15);//	1 byte	0	3	1
    MCDI_REG32_ClearAndSet(AR_CTRL18 ,CL_LMD_LUT0_MASK , CL_LMD_LUT0_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT0);//	1 byte	0	255	1	AR_CTRL18-> CL_LMD_LUT0
    MCDI_REG32_ClearAndSet(AR_CTRL18 ,CL_LMD_LUT1_MASK , CL_LMD_LUT1_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT1);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL18 ,CL_LMD_LUT2_MASK , CL_LMD_LUT2_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT2);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL18 ,CL_LMD_LUT3_MASK , CL_LMD_LUT3_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT3);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL19 ,CL_LMD_LUT4_MASK , CL_LMD_LUT4_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT4);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL19 ,CL_LMD_LUT5_MASK , CL_LMD_LUT5_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT5);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL19 ,CL_LMD_LUT6_MASK , CL_LMD_LUT6_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT6);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL19 ,CL_LMD_LUT7_MASK , CL_LMD_LUT7_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT7);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL20 ,CL_LMD_LUT8_MASK , CL_LMD_LUT8_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT8);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL20 ,CL_LMD_LUT9_MASK , CL_LMD_LUT9_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT9);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL20 ,CL_LMD_LUT10_MASK , CL_LMD_LUT10_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT10);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL20 ,CL_LMD_LUT11_MASK , CL_LMD_LUT11_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT11);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL21 ,CL_LMD_LUT12_MASK , CL_LMD_LUT12_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT12);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL21 ,CL_LMD_LUT13_MASK , CL_LMD_LUT13_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT13);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL21 ,CL_LMD_LUT14_MASK , CL_LMD_LUT14_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT14);//	1 byte	0	255	1
    MCDI_REG32_ClearAndSet(AR_CTRL21 ,CL_LMD_LUT15_MASK , CL_LMD_LUT15_OFFSET, vq_data_p->  MCDI_CL_LMD_LUT15);//	1 byte	0	255	1
    HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCMADI, SYNC_UPDATE);
    return FVDP_OK;
}
#pragma GCC diagnostic pop

/******************************************************************************
 FUNCTION       : Mcdi_LoadDefaultVqData
 USAGE          : Loads default vq data to the hardware registers
 INPUT          : void
 OUTPUT         : None
 GLOBALS        : None
 RETURN         : None
******************************************************************************/
void Mcdi_LoadDefaultVqData(void)
{
    VQ_Mcdi_Parameters_t McdiDefault;
    McdiDefault.MCMADI_Blender_KLUT_0 = 0;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_0
    McdiDefault.MCMADI_Blender_KLUT_1 = 64;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_1
    McdiDefault.MCMADI_Blender_KLUT_2 = 128;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_2
    McdiDefault.MCMADI_Blender_KLUT_3 = 255;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_3
    McdiDefault.MCDI_LDME_VTAP_SEL = 0;//	1 byte	0	3	1	Select one of these: 0 = no filter; 1 = 2 Tap filter; 2 = 5 Tap filter*/
    McdiDefault.MCDI_LDME_BOFFSET = 4;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_HME_THR = 200;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_VME_THR = 160;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_DME_THR = 0;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_THR = 250;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_BST = 22;//	1 byte	0	31	1
    McdiDefault.MCDI_LDME_BSP = 697;//	2 bytes	0	4095	1
    McdiDefault.MCDI_LDME_HBM_OST0 = -8;//	2 bytes	-10	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center.There are 21 entries on X axis.Typical value is: -5 -5 -4 -4 -3 -3 -2 -2 -1 -1 0
    McdiDefault.MCDI_LDME_HBM_OST1 = -6;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST2 = -4;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST3 = -4;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST4 = -3;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST5 = -3;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST6 = -2;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST7 = -2;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST8 = -1;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST9 = -1;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_HBM_OST10 = 0;//	2 bytes	-10	0	1
    McdiDefault.MCDI_LDME_CNST_THR_LO = 40;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_CNST_THR_HI = 20;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_HSTILL_THR = 55;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_CNST_GAIN = 12;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_ST = 0;//	1 byte	0	7	1
    McdiDefault.MCDI_LDME_HME_BM_GAIN = 6;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_VME_BM_GAIN = 5;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_DME_BM_GAIN = 6;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_HME_NOISE = 28;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_VME_NOISE = 12;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_VCNST_NKL = 25;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_SF_THR = 26;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_SF_THR_LO = 32;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_SF_THR_HI = 250;//	2 bytes	0	1023	1
    McdiDefault.MCDI_LDME_VBM_OST0 = -16;//	2 bytes	-32	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost4 is at the center.There are 9 entries on y axis.Typical value is: -16 -8 -2 -1 0
    McdiDefault.MCDI_LDME_VBM_OST1 = -8;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_VBM_OST2 = -2;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_VBM_OST3 = -1;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_VBM_OST4 = 0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_STILL_NKL = 20;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_STILL_THR = 36;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_HME_DIFF_THR = 2;//	1 byte	0	3	1
    McdiDefault.MCDI_LDME_VME_DIFF_THR = 2;//	1 byte	0	3	1
    McdiDefault.MCDI_LDME_DME_HME_DIFF_THR = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_LDME_DME_VME_DIFF_THR = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_LDME_DME_ST = 0;//	1 byte	0	15	1	LDME_DME_ST
    McdiDefault.MCDI_LDME_VME_ST = 0;//	1 byte	0	15	1	LDME_VME_ST
    McdiDefault.MCDI_LDME_SF_HST = 0;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_SF_VST = 0;//	1 byte	0	15	1
    McdiDefault.MCDI_LDME_GSTILL_THR = 84;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_BSTILL_THR = 6;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_HME_G3_EN = 1;//	bool
    McdiDefault.MCDI_LDME_VME_G3_EN = 1;//	bool
    McdiDefault.MCDI_LDME_DME_G3_EN = 0;//	bool
    McdiDefault.MCDI_LDME_GSTILL_ENABLE = 1;//	bool
    McdiDefault.MCDI_LDME_FILM_ENABLE = 1;//	bool
    McdiDefault.MCDI_LDME_GHF_EN = 1;//	bool
    McdiDefault.MCDI_LDME_GMOVE_EN = 1;//	bool
    McdiDefault.MCDI_LDME_SCENE_EN = 1;//	bool
    McdiDefault.MCDI_LDME_GMOVE_HACC_GAIN_HI = 40;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_GMOVE_HACC_GAIN_LO = 13;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_GMOVE_VACC_GAIN_HI = 36;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_GMOVE_VACC_GAIN_LO = 12;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_ACC_THR_HI = 56;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_ACC_THR_LO = 15;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_GHF_ACC_THR = 500;//	2 bytes	0	1023	1
    McdiDefault.MCDI_LDME_LGM_THR = 248;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_LGM_OFF = 70;//	1 byte	0	255	1
    McdiDefault.MCDI_LDME_FIX_MIX_FILM_EN = 1;//	bool			1	Mix_film_en; the register is 1 byte but only use bit0
    McdiDefault.MCDI_LMVD_HLG_BTHR = 100;//	1 byte	0	127	1
    McdiDefault.MCDI_LDME_HLG_BTHR = 20;//	1 byte	0	63	1
    McdiDefault.MCDI_LMVD_HLG_LTHR = 500;//	2 bytes	0	1023	1
    McdiDefault.MCDI_LMVD_HLG_GTHR = 64;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLG_OST0 = -16;//	2 bytes	-32	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and os10 is at the center.There are 21 entries on x axis.Typical value is:-10 -9 -8 -7 -6 -5 -4 -3 -2 -1 0
    McdiDefault.MCDI_LMVD_HLG_OST1 = -14;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST2 = -12;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST3 = -10;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST4 = -8;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST5 = -6;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST6 = -4;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST7 = -3;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST8 = -2;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST9 = -1;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LMVD_HLG_OST10 = 0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST0 = 255;//	2 bytes	-32	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center.There are 21 entries on x axis.Typical value is:-20 -20 -18 -16 -14 -12 -10 -8 -2 -1 0
    McdiDefault.MCDI_LDME_HLG_OST1 = 255;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST2 = 255;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST3 = 255;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST4 = 0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST5 = -16;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST6 = -8;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST7 = -4;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST8 = -2;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST9 = -1;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_OST10 = 0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_LDME_HLG_NKL = 250;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLG_NOISE = 6;//	1 byte	0	31	1
    McdiDefault.MCDI_LMVD_HLG_THR_LO = 5;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_HLG_THR_HI = 3;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_HLG_NKL = 200;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLG_CNST_NKL = 34;//	1 byte	0	63	1
    McdiDefault.MCDI_LMVD_HLG_STILL_NKL = 28;//	1 byte	0	63	1
    McdiDefault.MCDI_LMVD_LMD_LV_ENABLE = 13;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_LMD_HLV_THR_LO = 3;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_LMD_HLV_THR_HI = 8;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_LMD_VLV_THR_LO = 6;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_LMD_VLV_THR_HI = 9;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_VLVD_GAIN_HI = 0;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_VLVD_GAIN_LO = 12;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLVD_GAIN_HI = 23;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLVD_GAIN_LO = 2;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_HLVD_OFFSET = 100;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_VLVD_OFFSET = 100;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_VMV_THR_LO = 0;//	1 byte	0	7	1
    McdiDefault.MCDI_LMVD_VMV_THR_HI = 5;//	1 byte	0	7	1
    McdiDefault.MCDI_LMVD_HMV_THR_LO = 0;//	1 byte	0	7	1
    McdiDefault.MCDI_LMVD_HMV_THR_HI = 6;//	1 byte	0	7	1
    McdiDefault.MCDI_LMVD_LHMV_EN = 1;//	bool
    McdiDefault.MCDI_LMVD_HST = 48;//	1 byte	0	63	1
    McdiDefault.MCDI_LMVD_VST = 28;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_GSTILL_THR = 10;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_GSTILL_THR_LO = 131;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_GSTILL_THR_HI = 118;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_GSTILL_GAIN_LO = 25;//	1 byte	0	255	1
    McdiDefault.MCDI_LMVD_GSTILL_GAIN_HI = 6;//	1 byte	0	255	1
    McdiDefault.MCDI_MVD_GSTIL_COUNT_CTRL = 5;//	1 byte	0	15	1
    McdiDefault.MCDI_LMVD_DIV_MORE = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_LDME_DIV_MORE = 2;//	1 byte	0	3	1
    McdiDefault.MCDI_SDI_NOISE = 31;//	1 byte	0	63	1
    McdiDefault.MCDI_SDI_THR = 180;//	1 byte	0	63	1
    McdiDefault.MCDI_SDI_HI_THR = 120;//	1 byte	0	255	1
    McdiDefault.MCDI_SDI_CNST_THR = 15;//	1 byte	0	255	1
    McdiDefault.MCDI_SDI_OST0 = 0;//	2 bytes	0	32	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost5 is at the center.There are 9 entries on x axis.Typical value is:0 0 0 0 0 25
    McdiDefault.MCDI_SDI_OST1 = 0;//	2 bytes	0	32	1
    McdiDefault.MCDI_SDI_OST2 = 0;//	2 bytes	0	32	1
    McdiDefault.MCDI_SDI_OST3 = 0;//	2 bytes	0	32	1
    McdiDefault.MCDI_SDI_OST4 = 0;//	2 bytes	0	32	1
    McdiDefault.MCDI_SDI_OST5 = 25;//	2 bytes	0	32	1
    McdiDefault.MCDI_SDI_OFFSET = 64;//	1 byte	0	255	1
    McdiDefault.MCDI_SDI_CNST_GAIN = 15;//	1 byte	0	255	1	Link to the register
    McdiDefault.MCDI_SDI_HI_GAIN = 0;//	1 byte	0	255	1	Link to the register
    McdiDefault.MCDI_HSDME_Frame_Bypass = 0;//	bool				SDME_HOR_EN button
    McdiDefault.MCDI_HSDME_Field_Bypass = 0;//	bool				SDME_VER_EN button
    McdiDefault.MCDI_HSDME_FRAME_NOISE = 16;//	1 byte	0	31	1
    McdiDefault.MCDI_HSDME_FIELD_NOISE = 16;//	1 byte	0	31	1
    McdiDefault.MCDI_HSDME_ST = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_HSDME_STILL_NOISE = 8;//	1 byte	0	63	1
    McdiDefault.MCDI_VSDME_FRAME_NOISE = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_VSDME_Bypass = 0;//	bool				VSDME_EN button
    McdiDefault.MCDI_HSDME_FRAME_OST0 = -16;//	2 bytes	-32	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center.There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    McdiDefault.MCDI_HSDME_FRAME_OST1 = -16;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FRAME_OST2 = -14;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FRAME_OST3 = -12;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FRAME_OST4 = -10;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FRAME_OST5 = -8;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FRAME_OST6 = -0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST0 = -16;//	2 bytes	-32	0	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center.There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    McdiDefault.MCDI_HSDME_FIELD_OST1 = -16;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST2 = -14;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST3 = -12;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST4 = -10;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST5 = -8;//	2 bytes	-32	0	1
    McdiDefault.MCDI_HSDME_FIELD_OST6 = 0;//	2 bytes	-32	0	1
    McdiDefault.MCDI_VSDME_FRAME_OST0 = -16;//	2 bytes	0	255	1	Map to the graphics.The curve is symmetric: ost0 is at the beginning/end and ost2 is at the center.There are 5 entries on y axis.Typical value is:-16 -8 0
    McdiDefault.MCDI_VSDME_FRAME_OST1 = -8;//	2 bytes	0	255	1
    McdiDefault.MCDI_VSDME_FRAME_OST2 = 0;//	2 bytes	0	255	1
    McdiDefault.MCDI_TDI_TNR_BYPASS = 1;//	bool
    McdiDefault.MCDI_TDI_INT_TNR_BYPASS = 1;//	bool
    McdiDefault.MCDI_TDI_SNR_BYPASS = 1;//	bool
    McdiDefault.MCDI_TDI_OFFSET = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_TDI_HBST = 11;//	1 byte	0	31	1
    McdiDefault.MCDI_TDI_HBSP = 708;//	2 bytes	0	4095	1
    McdiDefault.MCDI_TDI_VBST = 4;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_VBSP = 475;//	2 bytes	0	4095	1
    McdiDefault.MCDI_TDI_IMPULSE_NKL = 150;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_IMPULSE_THR =  256;//	2 bytes	0	1023	1
    McdiDefault.MCDI_TDI_KLUT0 = 255;//	1 byte	0	255	1	Map to graphics.Typical value: 255 200 150 100
    McdiDefault.MCDI_TDI_KLUT1 = 200;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_KLUT2 = 150;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_KLUT3 = 100;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_SNR_KLUT0 = 255;//	1 byte	0	255	1	Map to graphics.Typical value: 255 128 0 0
    McdiDefault.MCDI_TDI_SNR_KLUT1 = 128;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_SNR_KLUT2 = 0;//	1 byte	0	255	1
    McdiDefault.MCDI_TDI_SNR_KLUT3 = 0;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MDET_THR0 = 15;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MDET_THR1 = 24;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MDET_THR2 = 36;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_VMV_GAIN = 8;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_HMV_GAIN = 8;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_NMV_GAIN = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_SMALL_GAIN = 5;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_SMALL_GAIN_H = 64;//I	1 byte	0	255	1
    McdiDefault.MCDI_CL_SMALL_GAIN_L = 1;//O	1 byte	0	255	1
    McdiDefault.MCDI_CL_LARGE_GAIN = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_DEFAULT_GAIN = 5;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_CNST_NKL = 32;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_CNST_GAIN = 18;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_CNST_OFF = 2;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_OFF = 5;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_ANG_OFF = 4;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_MAX_ANG_OFF = 1;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_SMALL_OFF = 2;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_SMALL_OFF_LO = 4;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_SMALL_OFF_HI = 5;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_HTRAN_OFF = 3;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_VTRAN_OFF = 3;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_GSTILL_OFF = 15;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LAMD_NKL = 15; //   1 byte	0	63	1
    McdiDefault.MCDI_CL_LAMD_SMALL_NKL = 8;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_LAMD_BIG_NKL = 20;//	1 byte	0	63	1
    McdiDefault.MCDI_CL_LAMD_HTHR = 8;//	1 byte	0	31	1
    McdiDefault.MCDI_CL_LAMD_GAIN = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_DIAG_GAIN = 8;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_DIAG_GAIN1 = 12;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_DIAG_GAIN2 = 8;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_HTRAN_THR_LO = 48;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_HTRAN_THR_HI = 70;//	2 bytes	0	1023	1
    McdiDefault.MCDI_CL_VTRAN_THR_LO = 48;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_VTRAN_THR_HI = 200;//	2 bytes	0	1023	1
    McdiDefault.MCDI_CL_HTRAN_GAIN = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_HTRAN_GAIN1 = 4;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_VTRAN_GAIN = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_VTRAN_GAIN1 = 64;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_GSTILL_GAIN = 80;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_GHF_GAIN = 6;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_GSTILL_GAIN0AND1 = 32;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_GSTILL_GAIN0OR1 = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_ANG_FILM_GAIN = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_FILM_GAIN3AND5 = 80;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LASD_NKL = 8;//	1 byte	0	63	1
    McdiDefault.MCDI_CL_LASD_THR = 48;//	1 byte	0	63	1
    McdiDefault.MCDI_CL_LASD_GAIN = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_DMV_GAIN = 128;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_STILL_ANG_OFF = -1;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_STILL_ANG_EN = 1;//	bool
    McdiDefault.MCDI_CL_STILL_ANG_GAIN = 1023;//	2 bytes	0	1023	1
    McdiDefault.MCDI_CL_DIAG_HTHR = 4;//	1 byte	0	7	1
    McdiDefault.MCDI_CL_DIAG_VTHR = 0;//	1 byte	0	7	1
    McdiDefault.MCDI_CL_DIAG_HTHR_HI = 5;//	1 byte	0	7	1
    McdiDefault.MCDI_CL_DIAG_VTHR_LO = 0;//	1 byte	0	7	1
    McdiDefault.MCDI_CL_GSTILL_THR_HI = 8;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_GSTILL_THR_LO = 3;//	1 byte	0	15	1
    McdiDefault.MCDI_CL_DIAG_FILM_EN = 0;//	bool
    McdiDefault.MCDI_CL_OK_PATCH_EN = 1;//	bool
    McdiDefault.MCDI_CL_DIAG_STILL_EN = 0;//	bool
    McdiDefault.MCDI_CL_GHF_EN = 1;//	bool
    McdiDefault.MCDI_CL_MVCL_THR0 = 12;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MVCL_THR1 = 16;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MVCL_THR2 = 20;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_MVCL_LUT0 = 0;//	1 byte	0	3	1	AR_CTRL18-> CL_MVCL_LUT0
    McdiDefault.MCDI_CL_MVCL_LUT1 = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT2 = 2;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT3 = 3;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT4 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT5 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT6 = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT7 = 2;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT8 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT9 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT10 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT11 = 1;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT12 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT13 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT14 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_MVCL_LUT15 = 0;//	1 byte	0	3	1
    McdiDefault.MCDI_CL_LMD_LUT0 = 1;//	1 byte	0	255	1	AR_CTRL18-> CL_LMD_LUT0
    McdiDefault.MCDI_CL_LMD_LUT1 = 1;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT2 = 3;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT3 = 6;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT4 = 10;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT5 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT6 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT7 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT8 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT9 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT10 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT11 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT12 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT13 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT14 = 255;//	1 byte	0	255	1
    McdiDefault.MCDI_CL_LMD_LUT15 = 255;//	1 byte	0	255	1

    Mcdi_SetVqData (&McdiDefault);
}


/*******************************************************************************
Name        : Deint_DcdiInit
Description : Initialize HQScaler DCDi related registers
Parameters  : void
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value:
                  FVDP_OK no error
*******************************************************************************/
FVDP_Result_t Deint_DcdiInit (void)
{
    DEINT_REG32_Write(DEINTC_DCDI_CTRL, 0x0); /*disable DCDi*/

    /*
    DEINTC_DCDI_CTRL2 initialization: 0x01dfcef8

    DEINTC_CLAMP_REP_THRSH  : 0x8
    DEINTC_EDGE_REP_EN      : 0x1
    DEINTC_VAR_DEP_EN       : 0x1
    DEINTC_VAR_DEP_ORDER    : 0x3
    DEINTC_PRO_ANGLE_EN     : 0x1
    DEINTC_CS_ALG_SWITCH    : 0x1
    DEINTC_PS_QUAD_INTP_EN  : 0x1
    DEINTC_SP_COEFF_SEL     : 0x0
    DEINTC_PELCORR_EN       : 0x1
    DEINTC_ALPHA_LIMIT      : 0x3F
    DEINTC_DIS_MV_ANG_CALC  : 0x0
    DEINTC_MF_ALPHA_DEP_EN  : 0x1
    DEINTC_ANGLE_HOR_MF_EN  : 0x1
    DEINTC_DCDI_MV_NL_LUT_EN: 0x1
    */
    DEINT_REG32_Write(DEINTC_DCDI_CTRL2, 0x01dfcef8);

    /*
    DEINTC_DCDI_CTRL3 initialization: 0xfa50030b

    DEINTC_DCDI_PELCORR_TH : 0xB
    DEINTC_DCDI_VAR_THRSH : 0x3
    DEINTC_DCDI_MV_NL_LUT : 0xFA50
    */
    DEINT_REG32_Write(DEINTC_DCDI_CTRL3, 0xfa50030b);

    return (FVDP_OK);
}

/*******************************************************************************
Name        : Deint_DcdiUpdate
Description : Set up HQScaler DCDi Status and Strength
Parameters  : void
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value:
              FVDP_OK no error
*******************************************************************************/
FVDP_Result_t Deint_DcdiUpdate ( bool Enable, uint8_t Strength)
{
    /* Check the maximum DCDi Strength */
    if (Strength > DEINTC_DCDI_PELCORR_TH_MASK)
    {
        Strength = DEINTC_DCDI_PELCORR_TH_MASK;
    }

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL3, DEINTC_DCDI_PELCORR_TH_MASK,
                  DEINTC_DCDI_PELCORR_TH_OFFSET,Strength);

    DEINT_REG32_ClearAndSet(DEINTC_DCDI_CTRL, DEINTC_DCDI_ENABLE_MASK, DEINTC_DCDI_ENABLE_OFFSET, Enable);

    return (FVDP_OK);
}
