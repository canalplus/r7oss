/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_eventctrl.c
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
#include "fvdp_eventctrl.h"


/* Private Struct Definitions ----------------------------------------------- */

typedef struct
{
    uint32_t CommCtrl_Event_Pos_Addr;
    uint32_t CommCtrl_Event_Pos_Mask;
    uint32_t CommCtrl_Event_Pos_Offset;
} CommClust_EventCfg_Register_t;

/* Private Variables (static)------------------------------------------------ */

CommClust_EventCfg_Register_t CommClust_SyncEvent_FE_Register[]=
{
    {COMMCTRL_FE_EVENT_POS_0,COMMCTRL_UPDATE_EVENT_POS_MASK,COMMCTRL_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_0,HFLITE_UPDATE_EVENT_POS_MASK,HFLITE_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_1,VFLITE_UPDATE_EVENT_POS_MASK,VFLITE_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_1,CCS_UPDATE_EVENT_POS_MASK,CCS_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_2,DNR_UPDATE_EVENT_POS_MASK,DNR_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_2,MCMADI_UPDATE_EVENT_POS_MASK,MCMADI_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_3,EVENT_POS_3_RSVD_A_MASK,EVENT_POS_3_RSVD_A_OFFSET},
    {COMMCTRL_FE_EVENT_POS_3,EVENT_POS_3_RSVD_B_MASK,EVENT_POS_3_RSVD_B_OFFSET},
    {COMMCTRL_FE_EVENT_POS_4,MCC_IP_UPDATE_EVENT_POS_MASK,MCC_IP_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_FE_EVENT_POS_4,MCC_OP_UPDATE_EVENT_POS_MASK,MCC_OP_UPDATE_EVENT_POS_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_FrameReset_FE_Register[]=
{
    {COMMCTRL_FE_FRST_POS_0,VIDEO1_IN_FRST_POS_MASK,VIDEO1_IN_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_0,VIDEO2_IN_FRST_POS_MASK,VIDEO2_IN_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_1,VIDEO3_IN_FRST_POS_MASK,VIDEO3_IN_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_1,VIDEO4_IN_FRST_POS_MASK,VIDEO4_IN_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_2,VIDEO1_OUT_FRST_POS_MASK,VIDEO1_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_2,VIDEO2_OUT_FRST_POS_MASK,VIDEO2_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_3,VIDEO3_OUT_FRST_POS_MASK,VIDEO3_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_3,VIDEO4_OUT_FRST_POS_MASK,VIDEO4_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_4,HFLITE_FRST_POS_MASK,HFLITE_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_4,VFLITE_FRST_POS_MASK,VFLITE_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_5,CCS_FRST_POS_MASK,CCS_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_5,DNR_FRST_POS_MASK,DNR_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_6,MCMADI_FRST_POS_MASK,MCMADI_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_6,ELAFIFO_FRST_POS_MASK,ELAFIFO_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_7,CHLSPLIT_FRST_POS_MASK,CHLSPLIT_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_7,MCC_WRITE_FRST_POS_MASK,MCC_WRITE_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_8,MCC_READ_FRST_POS_MASK,MCC_READ_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_8,INPUT_IRQ_FRST_POS_MASK,INPUT_IRQ_FRST_POS_OFFSET},
    {COMMCTRL_FE_FRST_POS_9,DISPLAY_IRQ_FRST_POS_MASK,DISPLAY_IRQ_FRST_POS_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_SyncEvent_BE_Register[]=
{
    {COMMCTRL_BE_EVENT_POS_0,COMMCTRL_UPDATE_EVENT_POS_MASK,COMMCTRL_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_0,HQSCALER_UPDATE_EVENT_POS_MASK,HQSCALER_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_1,CCTRL_UPDATE_EVENT_POS_MASK,CCTRL_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_1,BDR_UPDATE_EVENT_POS_MASK,BDR_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_2,R2DTO3D_EVENT_POS_MASK,R2DTO3D_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_2,EVENT_POS_2_RSVD_B_MASK,EVENT_POS_2_RSVD_B_OFFSET},
    {COMMCTRL_BE_EVENT_POS_3,SPARE_EVENT_POS_MASK,SPARE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_3,COMM_CLUST_BE_MCC_IP_UPDATE_EVENT_POS_MASK,COMM_CLUST_BE_MCC_IP_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_BE_EVENT_POS_4,COMM_CLUST_BE_MCC_OP_UPDATE_EVENT_POS_MASK,COMM_CLUST_BE_MCC_OP_UPDATE_EVENT_POS_OFFSET},

};

CommClust_EventCfg_Register_t CommClust_FrameReset_BE_Register[]=
{
    {COMMCTRL_BE_FRST_POS_0,COMM_CLUST_BE_VIDEO1_IN_FRST_POS_MASK,COMM_CLUST_BE_VIDEO1_IN_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_0,COMM_CLUST_BE_VIDEO2_IN_FRST_POS_MASK,COMM_CLUST_BE_VIDEO2_IN_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_1,COMM_CLUST_BE_VIDEO3_IN_FRST_POS_MASK,COMM_CLUST_BE_VIDEO3_IN_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_1,COMM_CLUST_BE_VIDEO4_IN_FRST_POS_MASK,COMM_CLUST_BE_VIDEO4_IN_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_2,COMM_CLUST_BE_VIDEO1_OUT_FRST_POS_MASK,COMM_CLUST_BE_VIDEO1_OUT_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_2,COMM_CLUST_BE_VIDEO2_OUT_FRST_POS_MASK,COMM_CLUST_BE_VIDEO2_OUT_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_3,COMM_CLUST_BE_VIDEO3_OUT_FRST_POS_MASK,COMM_CLUST_BE_VIDEO3_OUT_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_3,COMM_CLUST_BE_VIDEO4_OUT_FRST_POS_MASK,COMM_CLUST_BE_VIDEO4_OUT_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_4,HQSCALER_FRST_POS_MASK,HQSCALER_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_4,CCTRL_FRST_POS_MASK,CCTRL_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_5,BDR_FRST_POS_MASK,BDR_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_5,R2DTO3D_FRST_POS_MASK,DNR_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_6,COMM_CLUST_BE_ELAFIFO_FRST_POS_MASK,COMM_CLUST_BE_ELAFIFO_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_6,SPARE_FRST_POS_MASK,SPARE_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_7,COMM_CLUST_BE_MCC_WRITE_FRST_POS_MASK,COMM_CLUST_BE_MCC_WRITE_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_7,COMM_CLUST_BE_MCC_READ_FRST_POS_MASK,COMM_CLUST_BE_MCC_READ_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_8,COMM_CLUST_BE_DISPLAY_IRQ_FRST_POS_MASK,COMM_CLUST_BE_INPUT_IRQ_FRST_POS_OFFSET},
    {COMMCTRL_BE_FRST_POS_8,INPUT_IRQ_FRST_POS_MASK,COMM_CLUST_BE_DISPLAY_IRQ_FRST_POS_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_SyncEvent_LITE_Register[]=
{
    {COMMCTRL_EVENT_POS_0,COMM_CLUST_PIP_COMMCTRL_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_COMMCTRL_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_0,TNR_UPDATE_EVENT_POS_MASK,TNR_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_1,COMM_CLUST_PIP_HFLITE_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_HFLITE_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_1,COMM_CLUST_PIP_VFLITE_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_VFLITE_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_2,CCTRL_LITE_UPDATE_EVENT_POS_MASK,CCTRL_LITE_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_2,COMM_CLUST_PIP_BDR_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_BDR_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_3,ZOOMFIFO_UPDATE_EVENT_POS_MASK,ZOOMFIFO_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_3,COMM_CLUST_PIP_MCC_IP_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_MCC_IP_UPDATE_EVENT_POS_OFFSET},
    {COMMCTRL_EVENT_POS_4,COMM_CLUST_PIP_MCC_OP_UPDATE_EVENT_POS_MASK,COMM_CLUST_PIP_MCC_OP_UPDATE_EVENT_POS_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_FrameReset_LITE_Register[]=
{
    {COMMCTRL_FRST_POS_0,COMM_CLUST_PIP_VIDEO1_IN_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO1_IN_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_0,COMM_CLUST_PIP_VIDEO2_IN_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO2_IN_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_1,COMM_CLUST_PIP_VIDEO3_IN_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO3_IN_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_1,COMM_CLUST_PIP_VIDEO4_IN_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO4_IN_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_2,COMM_CLUST_PIP_VIDEO1_OUT_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO1_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_2,COMM_CLUST_PIP_VIDEO2_OUT_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO2_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_3,COMM_CLUST_PIP_VIDEO3_OUT_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO3_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_3,COMM_CLUST_PIP_VIDEO4_OUT_FRST_POS_MASK,COMM_CLUST_PIP_VIDEO4_OUT_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_4,TNR_FRST_POS_MASK,TNR_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_4,COMM_CLUST_PIP_HFLITE_FRST_POS_MASK,COMM_CLUST_PIP_HFLITE_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_5,COMM_CLUST_PIP_VFLITE_FRST_POS_MASK,COMM_CLUST_PIP_VFLITE_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_5,CCTRL_LITE_FRST_POS_MASK,CCTRL_LITE_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_6,COMM_CLUST_PIP_BDR_FRST_POS_MASK,COMM_CLUST_PIP_BDR_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_6,ZOOMFIFO_FRST_POS_MASK,ZOOMFIFO_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_7,COMM_CLUST_PIP_MCC_WRITE_FRST_POS_MASK,CHLSPLIT_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_7,COMM_CLUST_PIP_MCC_READ_FRST_POS_MASK,COMM_CLUST_PIP_MCC_READ_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_8,COMM_CLUST_PIP_INPUT_IRQ_FRST_POS_MASK,COMM_CLUST_PIP_INPUT_IRQ_FRST_POS_OFFSET},
    {COMMCTRL_FRST_POS_8,COMM_CLUST_PIP_DISPLAY_IRQ_FRST_POS_MASK,COMM_CLUST_PIP_DISPLAY_IRQ_FRST_POS_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_FID_Config_FE_Register[]=
{
    {COMMCTRL_FE_FID_CFG_0, VFLITE_IN_FID_ODD_MASK  , VFLITE_IN_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, VFLITE_OUT_FID_ODD_MASK , VFLITE_OUT_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, CCS_FID_ODD_MASK        , CCS_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, DNR_FID_ODD_MASK        , DNR_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, MCMADI_FID_ODD_MASK     , MCMADI_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, MCC_PROG_FID_ODD_MASK   , MCC_PROG_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, MCC_C_FID_ODD_MASK      , MCC_C_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_0, MCC_MCMADI_FID_ODD_MASK , MCC_MCMADI_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_1, MCC_DNR_FID_ODD_MASK    , MCC_DNR_FID_ODD_OFFSET},
    {COMMCTRL_FE_FID_CFG_1, MCC_CCS_FID_ODD_MASK    , MCC_CCS_FID_ODD_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_FID_Config_BE_Register[]=
{
    {COMMCTRL_BE_FID_CFG_0, HQSCALER_IN_FID_ODD_MASK  , HQSCALER_IN_FID_ODD_OFFSET},
    {COMMCTRL_BE_FID_CFG_0, HQSCALER_OUT_FID_ODD_MASK , HQSCALER_OUT_FID_ODD_OFFSET},
    {COMMCTRL_BE_FID_CFG_0, CCTRL_FID_ODD_MASK        , CCTRL_FID_ODD_OFFSET},
    {COMMCTRL_BE_FID_CFG_0, R2DTO3D_FID_ODD_MASK      , R2DTO3D_FID_ODD_OFFSET},
    {COMMCTRL_BE_FID_CFG_0, SPARE_FID_ODD_MASK        , SPARE_FID_ODD_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_FID_Config_LITE_Register[]=
{
    {COMMCTRL_FID_CFG_0, TNR_FID_ODD_MASK                      , TNR_FID_ODD_OFFSET},
    {COMMCTRL_FID_CFG_0, COMM_CLUST_PIP_VFLITE_IN_FID_ODD_MASK , COMM_CLUST_PIP_VFLITE_IN_FID_ODD_OFFSET},
    {COMMCTRL_FID_CFG_0, COMM_CLUST_PIP_VFLITE_OUT_FID_ODD_MASK, COMM_CLUST_PIP_VFLITE_OUT_FID_ODD_OFFSET},
    {COMMCTRL_FID_CFG_0, CCTRL_LITE_FID_ODD_MASK               , CCTRL_LITE_FID_ODD_OFFSET},
    {COMMCTRL_FID_CFG_0, MCC_WRITE_FID_ODD_MASK                , MCC_WRITE_FID_ODD_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_Module_Clock_Config_FE_Register[]=
{
    {COMMCTRL_FE_MODULE_CLK_EN, HFLITE_CLK_EN_MASK                , HFLITE_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, VFLITE_CLK_EN_MASK                , VFLITE_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, CCS_CLK_EN_MASK                   , CCS_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, DNR_CLK_EN_MASK                   , DNR_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, MCMADI_CLK_EN_MASK                , MCMADI_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, ELAFIFO_CLK_EN_MASK               , ELAFIFO_CLK_EN_OFFSET},
    {COMMCTRL_FE_MODULE_CLK_EN, STBUS_T3_CLK_EN_MASK              , STBUS_T3_CLK_EN_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_Module_Clock_Config_BE_Register[]=
{
    {COMMCTRL_BE_MODULE_CLK_EN, HQSCALER_CLK_EN_MASK               , HQSCALER_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, CCTRL_CLK_EN_MASK                  , CCTRL_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, BDR_CLK_EN_MASK                    , BDR_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, R2DTO3D_CLK_EN_MASK                , R2DTO3D_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, COMM_CLUST_BE_ELAFIFO_CLK_EN_MASK  , COMM_CLUST_BE_ELAFIFO_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, SPARE_CLK_EN_MASK                  , SPARE_CLK_EN_OFFSET},
    {COMMCTRL_BE_MODULE_CLK_EN, COMM_CLUST_BE_STBUS_T3_CLK_EN_MASK , COMM_CLUST_BE_STBUS_T3_CLK_EN_MASK},
};

CommClust_EventCfg_Register_t CommClust_Module_Clock_Config_LITE_Register[]=
{
    {COMMCTRL_MODULE_CLK_EN, TNR_CLK_EN_MASK                      , TNR_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, COMM_CLUST_PIP_HFLITE_CLK_EN_MASK    , COMM_CLUST_PIP_HFLITE_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, COMM_CLUST_PIP_VFLITE_CLK_EN_MASK    , COMM_CLUST_PIP_VFLITE_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, CCTRL_LITE_CLK_EN_MASK               , CCTRL_LITE_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, COMM_CLUST_PIP_BDR_CLK_EN_MASK       , COMM_CLUST_PIP_BDR_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, ZOOMFIFO_CLK_EN_MASK                 , ZOOMFIFO_CLK_EN_OFFSET},
    {COMMCTRL_MODULE_CLK_EN, COMM_CLUST_PIP_STBUS_T3_CLK_EN_MASK  , COMM_CLUST_PIP_STBUS_T3_CLK_EN_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_Hard_Reset_FE_Register[]=
{
    {COMMCTRL_FE_RESET, COMMCTRL_HARD_RESET_MASK              , COMMCTRL_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, HFLITE_HARD_RESET_MASK                , HFLITE_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, VFLITE_HARD_RESET_MASK                , VFLITE_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, CCS_HARD_RESET_MASK                   , CCS_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, DNR_HARD_RESET_MASK                   , DNR_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, MCMADI_HARD_RESET_MASK                , MCMADI_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, ELAFIFO_HARD_RESET_MASK               , ELAFIFO_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, CHLSPLIT_HARD_RESET_MASK              , CHLSPLIT_HARD_RESET_OFFSET},
    {COMMCTRL_FE_RESET, MCC_HARD_RESET_MASK                   , MCC_HARD_RESET_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_Hard_Reset_BE_Register[]=
{
    {COMMCTRL_BE_RESET, COMM_CLUST_BE_COMMCTRL_HARD_RESET_MASK , COMM_CLUST_BE_COMMCTRL_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, HQSCALER_HARD_RESET_MASK               , HQSCALER_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, CCTRL_HARD_RESET_MASK                  , CCTRL_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, BDR_HARD_RESET_MASK                    , BDR_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, R2DTO3D_HARD_RESET_MASK                , R2DTO3D_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, COMM_CLUST_BE_ELAFIFO_HARD_RESET_MASK  , COMM_CLUST_BE_ELAFIFO_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, SPARE_HARD_RESET_MASK                  , SPARE_HARD_RESET_OFFSET},
    {COMMCTRL_BE_RESET, COMM_CLUST_BE_MCC_HARD_RESET_MASK      , COMM_CLUST_BE_MCC_HARD_RESET_OFFSET},
};

CommClust_EventCfg_Register_t CommClust_Hard_Reset_LITE_Register[]=
{
    {COMMCTRL_RESET, COMM_CLUST_PIP_COMMCTRL_HARD_RESET_MASK  , COMM_CLUST_PIP_COMMCTRL_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, TNR_HARD_RESET_MASK                      , TNR_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, COMM_CLUST_PIP_HFLITE_HARD_RESET_MASK    , COMM_CLUST_PIP_HFLITE_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, COMM_CLUST_PIP_VFLITE_HARD_RESET_MASK    , COMM_CLUST_PIP_VFLITE_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, CCTRL_LITE_HARD_RESET_MASK               , CCTRL_LITE_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, COMM_CLUST_PIP_BDR_HARD_RESET_MASK       , COMM_CLUST_PIP_BDR_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, ZOOMFIFO_HARD_RESET_MASK                 , ZOOMFIFO_HARD_RESET_OFFSET},
    {COMMCTRL_RESET, COMM_CLUST_PIP_MCC_HARD_RESET_MASK       , COMM_CLUST_PIP_MCC_HARD_RESET_OFFSET},
};

/*
 =======================================================================================================================
    COMM register access Macros
 =======================================================================================================================
 */
#define COMM_CLUST_FE_REG32_Write(Addr,Val)              REG32_Write(Addr+COMM_CLUST_FE_BASE_ADDRESS,Val)
#define COMM_CLUST_FE_REG32_Read(Addr)                   REG32_Read(Addr+COMM_CLUST_FE_BASE_ADDRESS)
#define COMM_CLUST_FE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToSet)
#define COMM_CLUST_FE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


#define COMM_CLUST_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+COMM_CLUST_BE_BASE_ADDRESS,Val)
#define COMM_CLUST_BE_REG32_Read(Addr)                   REG32_Read(Addr+COMM_CLUST_BE_BASE_ADDRESS)
#define COMM_CLUST_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToSet)
#define COMM_CLUST_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


static const uint32_t COMM_CLUST_LITE_BASE_ADDR[] =
{
    0,
    COMM_CLUST_PIP_BASE_ADDRESS,
    COMM_CLUST_AUX_BASE_ADDRESS,
    COMM_CLUST_ENC_BASE_ADDRESS
};

#define COMM_CLUST_LITE_REG32_Write(Ch, Addr,Val)              REG32_Write(Addr+COMM_CLUST_LITE_BASE_ADDR[Ch],Val)
#define COMM_CLUST_LITE_REG32_Read(Ch, Addr)                   REG32_Read(Addr+COMM_CLUST_LITE_BASE_ADDR[Ch])
#define COMM_CLUST_LITE_REG32_Set(Ch, Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_LITE_BASE_ADDR[Ch],BitsToSet)
#define COMM_CLUST_LITE_REG32_Clear(Ch, Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_LITE_BASE_ADDR[Ch],BitsToClear)
#define COMM_CLUST_LITE_REG32_ClearAndSet(Ch, Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_LITE_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)

/*******************************************************************************
Name        : EventCtrl_Init
Description : FVDP_FE, FVDP_BE, FVDP_PIP,FVDP_AUX, FVDP_ENC
               Init Event control hard ware module
Parameters  : void
Assumptions :
Limitations :
Returns     : FVDP_ERROR
*******************************************************************************/
void EventCtrl_Init(FVDP_CH_t CH)
{
    if (CH == FVDP_MAIN)
    {
        // Initialize the Algo block clocks
        COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_MODULE_CLK_EN, 0xAAAA);
        COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_MODULE_CLK_EN, 0x220A);

        // HQScaler use even/odd signal from display
        COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FID_CFG_0,0x20);
        COMM_CLUST_FE_REG32_ClearAndSet(COMMCTRL_FE_FID_CFG_0,MCMADI_FID_ODD_MASK,MCMADI_FID_ODD_OFFSET,1);
    }
    else
    {
        COMM_CLUST_LITE_REG32_Write(CH, COMMCTRL_MODULE_CLK_EN, 0x2AAA);
        if (CH == FVDP_ENC)
        {
            COMM_CLUST_LITE_REG32_Write(CH, COMMCTRL_FR_MASK, 0x3FFFF);
        }
    }
}

/******************************************************************************

 FUNCTION       : EventCtrl_SetSyncEvent_FE

 USAGE          : Sets event parameters source for Front End module


 INPUT          : Event_ID_FE_t BlockId - block id of the event to be configured
                  uint32_t Position - vertical position
                  EventSource_t EventSource - source or event could be Input or Display
 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetSyncEvent_FE(Event_ID_FE_t EventId, uint32_t Position, EventSource_t EventSource)
{
    uint32_t Index;
    if (EventId > EVENT_ID_ALL_FE)
        return FVDP_ERROR;
    if (EventSource)
    {
        COMM_CLUST_FE_REG32_Set(COMMCTRL_FE_UPDATE_CFG, EventId);
    }
    else
    {
        COMM_CLUST_FE_REG32_Clear(COMMCTRL_FE_UPDATE_CFG, EventId);
    }
    for (Index = 0; (BIT0 << Index) < EVENT_ID_ALL_FE; Index ++)
    {
        if ((BIT0 << Index) & EventId)
        {
            COMM_CLUST_FE_REG32_ClearAndSet(CommClust_SyncEvent_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_SyncEvent_FE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_SyncEvent_FE_Register[Index].CommCtrl_Event_Pos_Offset,
                Position);
        }
    }
    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_EnableFwFrameReset_FE

 USAGE          : Enables software generation of frame reset, sets enable bit and mask
                  Software generated frame reset by using EventCtrl_SetFwFrameReset_FE

 INPUT          : FrameReset_Type_FE_t FrameType

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : COMMCTRL_FW_FR_EN, COMMCTRL_FR_MASK

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_EnableFwFrameReset_FE(FrameReset_Type_FE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_FE)
    {
        COMM_CLUST_FE_REG32_Set(COMMCTRL_FE_FW_FR_EN , FrameType);
//        COMM_CLUST_FE_REG32_Set(COMMCTRL_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
}
/******************************************************************************

 FUNCTION       : EventCtrl_DisableFwFrameReset_FE

 USAGE          : Disables software generation of frame reset, Clears enable bit and mask

 INPUT          : FrameReset_Type_FE_t FrameType

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : COMMCTRL_FW_FR_EN, COMMCTRL_FR_MASK

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_DisableFwFrameReset_FE(FrameReset_Type_FE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_FE)
    {
        COMM_CLUST_FE_REG32_Clear(COMMCTRL_FE_FW_FR_EN , FrameType);
//        COMM_CLUST_FE_REG32_Clear(COMMCTRL_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
}
/******************************************************************************

 FUNCTION       :EventCtrl_SetFwFrameReset_FE

 USAGE          :Generates - forces firmware frame reset for given type of frame
                 (if previously enabled by EventCtrl_EnableFwFrameReset_FE)

 INPUT          :FrameReset_Type_FE_t FrameType

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_SetFwFrameReset_FE(FrameReset_Type_FE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_FE)
    {
        COMM_CLUST_FE_REG32_Set(COMMCTRL_FE_FW_FR_CTRL  , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFrameReset_FE

 USAGE          : Sets up the position for frame reset and source of generating frame reset
                  - Input or Display, and disables fw frame reset.

 INPUT          : FrameReset_Type_FE_t FrameType
                  uint32_t FrameReset_Pos
                  EventSource_t EventSource

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_SetFrameReset_FE(FrameReset_Type_FE_t FrameType, uint32_t FrameReset_Pos, EventSource_t EventSource)
{
    uint32_t Index;
    if (FrameType > FRAME_RESET_ALL_FE)
        return  FVDP_ERROR;

    EventCtrl_DisableFwFrameReset_FE( FrameType);
    if (EventSource)
    {
        COMM_CLUST_FE_REG32_Set(COMMCTRL_FE_FRST_CFG, FrameType);
    }
    else
    {
        COMM_CLUST_FE_REG32_Clear(COMMCTRL_FE_FRST_CFG, FrameType);
    }
    for (Index = 0; (BIT0 << Index) < FRAME_RESET_ALL_FE; Index ++)
    {
        if ((BIT0 << Index) & FrameType)
        {
            COMM_CLUST_FE_REG32_ClearAndSet(CommClust_FrameReset_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_FrameReset_FE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_FrameReset_FE_Register[Index].CommCtrl_Event_Pos_Offset,
                FrameReset_Pos);
        }
    }
    return FVDP_OK;
}

/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineEdge_FE

 USAGE          : Selected Flagline is Level or Edge sensitive, one common selection
                  for all Input flaglines, and other for all Display flaglines

 INPUT          : EventSource_t EventSource - group of input or display flaglines
                  FlaglineType_t FlaglineType - the group is Level or Edge sensitive

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineEdge_FE(EventSource_t EventSource, FlaglineType_t FlaglineType)
{
    uint32_t Address;

    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_FE_SRC_FLAGLN_CFG ;
    else
        Address = COMMCTRL_FE_DISP_FLAGLN_CFG;

    COMM_CLUST_FE_REG32_ClearAndSet(Address, COMMCTRL_SRC_FLAGLN_EDGE_MASK,COMMCTRL_SRC_FLAGLN_EDGE_OFFSET, FlaglineType);

    return FVDP_OK;
}

/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineCfg_FE

 USAGE          : Sets up flagline parameters, Position, and Repeat interval


 INPUT          : FlaglineName_t Name - which flagline
                  uint32_t FlaglinePos - position or line number of event generation
                  uint8_t FlaglineRepeatInterval - how often flaglines are generated,
                    and 0 for no repeat

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineCfg_FE( FlaglineName_t Name,
                                                    EventSource_t EventSource,
                                                    uint32_t FlaglinePos,
                                                    uint8_t FlaglineRepeatInterval)
{
    uint32_t Address;
    if (Name > FLAGLINE_7)
        return  FVDP_ERROR;
    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_FE_SRC_FLAGLN_POS_0;
    else
        Address = COMMCTRL_FE_DISP_FLAGLN_POS_0;
    Address += Name * 4; // address difference between flaglines is 4
    COMM_CLUST_FE_REG32_ClearAndSet(Address  ,SRC_FLAGLN_0_POS_MASK,SRC_FLAGLN_0_POS_OFFSET,FlaglinePos);
    COMM_CLUST_FE_REG32_ClearAndSet(Address  ,SRC_FLAGLN_0_RPT_MASK,SRC_FLAGLN_0_RPT_OFFSET,FlaglineRepeatInterval);

    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : EventCtrl_GetCurrentFieldType_FE
 USAGE          : return requested field type (odd/even polarity) of
                    FE Communication Cluster
 INPUT          : EventSource_t EventSource: INPUT_SOURCE/DISPLAY_SOURCE
 OUTPUT         : FVDP_FieldType_t *FieldType: TOP_FIELD/BOTTOM_FIELD
 GLOBALS        : None
 PRE_CONDITION  :
 RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_GetCurrentFieldType_FE(EventSource_t EventSource, FVDP_FieldType_t *FieldType)
{
    uint32_t Fid_Odd_Mask;

    Fid_Odd_Mask = (EventSource == INPUT_SOURCE) ? SRC_FID_ODD_MASK : DISP_FID_ODD_MASK;

   //FID_ODD: Low == TOP_FIELD    High == BOTTOM_FIELD
    if(COMM_CLUST_FE_REG32_Read(COMMCTRL_FE_FID_STATUS) & Fid_Odd_Mask)
    {
        *FieldType = BOTTOM_FIELD;
    }
    else
    {
        *FieldType = TOP_FIELD;
    }

    return FVDP_OK;
}

/******************************************************************************
FUNCTION       : EventCtrl_SetOddSignal_FE
USAGE          : Set firmware field ID (odd/even polarity)
                    FE Communication Cluster
INPUT          : Field_ID_FE_t FieldId:
                 FVDP_FieldType_t FieldType: TOP_FIELD/BOTTOM_FIELD
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_SetOddSignal_FE (Field_ID_FE_t FieldId, FVDP_FieldType_t FieldType)
{
    if(FieldType == TOP_FIELD)
    {
        COMM_CLUST_FE_REG32_Set(COMMCTRL_FE_FW_FIELD_ID, FieldId);
    }
    else
    {
        COMM_CLUST_FE_REG32_Clear(COMMCTRL_FE_FW_FIELD_ID, FieldId);
    }

    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ConfigOddSignal_FE
USAGE          : Set field ID (odd/even polarity) source
                    FE Communication Cluster
INPUT          : Field_ID_FE_t FieldId:
                 FieldSource_t FieldSource: FW_FID\SOURCE_IN\DISPLAY_OUT
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_ConfigOddSignal_FE ( Field_ID_FE_t FieldId, FieldSource_t FieldSource)
{
    uint32_t Index;

    if (FieldId > FIELD_ID_ALL_FE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < FIELD_ID_ALL_FE; Index ++)
    {
        if ((BIT0 << Index) & FieldId)
        {
            COMM_CLUST_FE_REG32_ClearAndSet(CommClust_FID_Config_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_FID_Config_FE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_FID_Config_FE_Register[Index].CommCtrl_Event_Pos_Offset,
                FieldSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ClockCtrl_FE
USAGE          : Select Module clock of FE Communication Cluster
INPUT          : Module_Clk_FE_t ModuleClockId
                 ModuleClock_t ClockSource: T_CLK/GND/SYSTEM_CLOCL
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_ClockCtrl_FE (Module_Clk_FE_t ModuleClockId, ModuleClock_t ClockSource)
{
    uint32_t Index;

    if (ModuleClockId > MODULE_CLK_ALL_FE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < MODULE_CLK_ALL_FE; Index ++)
    {
        if ((BIT0 << Index) & ModuleClockId)
        {
            COMM_CLUST_FE_REG32_ClearAndSet(CommClust_Module_Clock_Config_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_Module_Clock_Config_FE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_Module_Clock_Config_FE_Register[Index].CommCtrl_Event_Pos_Offset,
                ClockSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_HardReset_FE
USAGE          : Control Hard reset of FE Communication Cluster
INPUT          : Hard_Reset_FE_t HardResetId : block to hard reset
                 bool State : TRUE/FALSE
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_HardReset_FE (Hard_Reset_FE_t HardResetId, bool State)
{
    uint32_t Index;

    if (HardResetId > HARD_RESET_ALL_FE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < HARD_RESET_ALL_FE; Index ++)
    {
        if ((BIT0 << Index) & HardResetId)
        {
            if(State == TRUE)
            {
                COMM_CLUST_FE_REG32_Set(CommClust_Hard_Reset_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_FE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
            else //if(State == FALSE)
            {
                COMM_CLUST_FE_REG32_Clear(CommClust_Hard_Reset_FE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_FE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
        }
    }
    return (FVDP_OK);
}

/******************************************************************************

 FUNCTION       : EventCtrl_SetSyncEvent_BE

 USAGE          : Sets event parameters source for Back End module


 INPUT          : Event_ID_BE_t BlockId - block id of the event to be configured
                  uint32_t Position - vertical position
                  EventSource_t EventSource - source or event could be Input or Display
 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetSyncEvent_BE(Event_ID_BE_t EventId, uint32_t Position, EventSource_t EventSource)
{
    uint32_t Index;
    if (EventId > EVENT_ID_ALL_BE)
        return FVDP_ERROR;
    if (EventSource)
    {
        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_UPDATE_CFG, EventId);
    }
    else
    {
        COMM_CLUST_BE_REG32_Clear(COMMCTRL_BE_UPDATE_CFG,EventId);
    }
    for (Index = 0; (BIT0 << Index) < EVENT_ID_ALL_BE; Index ++)
    {
        if ((BIT0 << Index) & EventId)
        {
            COMM_CLUST_BE_REG32_ClearAndSet(CommClust_SyncEvent_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_SyncEvent_BE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_SyncEvent_BE_Register[Index].CommCtrl_Event_Pos_Offset,
                Position);
        }
    }
	return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_EnableFwFrameReset_BE

 USAGE          : Enables software generation of frame reset, sets enable bit and mask
                  Software generated frame reset by using EventCtrl_SetFwFrameReset_FE

 INPUT          : FrameReset_Type_BE_t FrameType

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_EnableFwFrameReset_BE(FrameReset_Type_BE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_BE)
    {
        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_FW_FR_EN , FrameType);
//        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;

}
/******************************************************************************

 FUNCTION       : EventCtrl_DisableFwFrameReset_BE

 USAGE          : Disables software generation of frame reset, Clears enable bit and mask

 INPUT          : FrameReset_Type_BE_t FrameType

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_DisableFwFrameReset_BE(FrameReset_Type_BE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_BE)
    {
        COMM_CLUST_BE_REG32_Clear(COMMCTRL_BE_FW_FR_EN , FrameType);
//        COMM_CLUST_BE_REG32_Clear(COMMCTRL_BE_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFwFrameReset_BE

 USAGE          : Generates - forces firmware frame reset for given type of frame
                  (if previously enabled by EventCtrl_EnableFwFrameReset_FE)

 INPUT          : FrameReset_Type_BE_t FrameType

 OUTPUT         : None

 GLOBALS        : None


 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_SetFwFrameReset_BE(FrameReset_Type_BE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_BE)
    {
        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_FW_FR_CTRL  , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFrameReset_BE

 USAGE          : Sets up the position for frame reset and source of generating frame reset
                  - Input or Display, and disables fw frame reset.

 INPUT          : FrameReset_Type_BE_t FrameType
                  uint32_t FrameReset_Pos
                  EventSource_t EventSource

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/

FVDP_Result_t EventCtrl_SetFrameReset_BE(FrameReset_Type_BE_t FrameType, uint32_t FrameReset_Pos, EventSource_t EventSource)
{
    uint32_t Index;

    if (FrameType > FRAME_RESET_ALL_BE)
        return  FVDP_ERROR;

    EventCtrl_DisableFwFrameReset_BE( FrameType);
    if (EventSource)
    {
        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_FRST_CFG , FrameType);
    }
    else
    {
        COMM_CLUST_BE_REG32_Clear(COMMCTRL_BE_FRST_CFG ,FrameType);
    }

    for (Index = 0; (BIT0 << Index) < FRAME_RESET_ALL_BE; Index ++)
    {
        if ((BIT0 << Index) & FrameType)
        {
            COMM_CLUST_BE_REG32_ClearAndSet(CommClust_FrameReset_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_FrameReset_BE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_FrameReset_BE_Register[Index].CommCtrl_Event_Pos_Offset,
                FrameReset_Pos);
        }
    }
    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineEdge_BE

 USAGE          : Selected Flagline is Level or Edge sensitive, one common selection
                  for all Input flaglines, and other for all Display flaglines

 INPUT          : EventSource_t EventSource - group of input or display flaglines
                  FlaglineType_t FlaglineType - the group is Level or Edge sensitive

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineEdge_BE(EventSource_t EventSource, FlaglineType_t FlaglineType)
{
    uint32_t Address;

    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_BE_SRC_FLAGLN_CFG ;
    else
        Address = COMMCTRL_BE_DISP_FLAGLN_CFG;

    COMM_CLUST_BE_REG32_ClearAndSet(Address,
                                    COMM_CLUST_BE_COMMCTRL_SRC_FLAGLN_EDGE_MASK,
                                    COMM_CLUST_BE_COMMCTRL_SRC_FLAGLN_EDGE_OFFSET,
                                    FlaglineType);

    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineCfg_BE

 USAGE          : Sets up flagline parameters, Position, and Repeat interval

 INPUT          : FlaglineName_t Name - which flagline
                  uint32_t FlaglinePos - position or line number of event generation
                  uint8_t FlaglineRepeatInterval - how often flaglines are generated,
                    and 0 for no repeat

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineCfg_BE(  FlaglineName_t Name,
                                            EventSource_t EventSource,
                                            uint32_t FlaglinePos,
                                            uint8_t FlaglineRepeatInterval)
{

    uint32_t Address;
    if (Name > FLAGLINE_7)
        return  FVDP_ERROR;
    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_BE_SRC_FLAGLN_POS_0;
    else
        Address = COMMCTRL_BE_DISP_FLAGLN_POS_0;
    Address += Name * 4; // address difference between flaglines is 4
    COMM_CLUST_BE_REG32_ClearAndSet(Address  ,COMM_CLUST_BE_SRC_FLAGLN_0_POS_MASK,COMM_CLUST_BE_SRC_FLAGLN_0_POS_OFFSET,FlaglinePos);
    COMM_CLUST_BE_REG32_ClearAndSet(Address  ,COMM_CLUST_BE_SRC_FLAGLN_0_RPT_MASK,COMM_CLUST_BE_SRC_FLAGLN_0_RPT_OFFSET,FlaglineRepeatInterval);

    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : EventCtrl_GetCurrentFieldType_BE
 USAGE          : return requested field type (odd/even polarity) of
                    BE Communication Cluster
 INPUT          : EventSource_t EventSource: INPUT_SOURCE/DISPLAY_SOURCE
 OUTPUT         : FVDP_FieldType_t *FieldType: TOP_FIELD/BOTTOM_FIELD
 GLOBALS        : None
 PRE_CONDITION  :
 RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_GetCurrentFieldType_BE(EventSource_t EventSource, FVDP_FieldType_t *FieldType)
{
    uint32_t Fid_Odd_Mask;

    Fid_Odd_Mask = (EventSource == INPUT_SOURCE) ? COMM_CLUST_BE_SRC_FID_ODD_MASK : COMM_CLUST_BE_DISP_FID_ODD_MASK;

    //FID_ODD: Low == TOP_FIELD    High == BOTTOM_FIELD
    if(COMM_CLUST_BE_REG32_Read(COMMCTRL_BE_FID_STATUS) & Fid_Odd_Mask)
    {
        *FieldType = BOTTOM_FIELD;
    }
    else
    {
        *FieldType = TOP_FIELD;
    }
    return FVDP_OK;
}

/******************************************************************************
FUNCTION       : EventCtrl_SetOddSignal_BE
USAGE          : Set firmware field ID (odd/even polarity)
                    BE Communication Cluster
INPUT          : Field_ID_BE_t FieldId:
                 FVDP_FieldType_t FieldType: TOP_FIELD/BOTTOM_FIELD
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_SetOddSignal_BE ( Field_ID_BE_t FieldId, FVDP_FieldType_t FieldType)
{
    if(FieldType == TOP_FIELD)
    {
        COMM_CLUST_BE_REG32_Set(COMMCTRL_BE_FW_FIELD_ID, FieldId);
    }
    else
    {
        COMM_CLUST_BE_REG32_Clear(COMMCTRL_BE_FW_FIELD_ID, FieldId);
    }

    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ConfigOddSignal_BE
USAGE          : Set field ID (odd/even polarity) source
                    BE Communication Cluster
INPUT          : Field_ID_BE_t FieldId:
                FieldSource_t FieldSource: FW_FID\SOURCE_IN\DISPLAY_OUT
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_ConfigOddSignal_BE ( Field_ID_BE_t FieldId, FieldSource_t FieldSource)
{
    uint32_t Index;

    if (FieldId > FIELD_ID_ALL_BE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < FIELD_ID_ALL_BE; Index ++)
    {
        if ((BIT0 << Index) & FieldId)
        {
            COMM_CLUST_FE_REG32_ClearAndSet(CommClust_FID_Config_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_FID_Config_BE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_FID_Config_BE_Register[Index].CommCtrl_Event_Pos_Offset,
                FieldSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ClockCtrl_BE
USAGE          : Select Module clock of BE Communication Cluster
INPUT          : Module_Clk_BE_t ModuleClockId
                 ModuleClock_t ClockSource: T_CLK/GND/SYSTEM_CLOCL
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_ClockCtrl_BE (Module_Clk_BE_t ModuleClockId, ModuleClock_t ClockSource)
{
    uint32_t Index;

    if (ModuleClockId > MODULE_CLK_ALL_BE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < MODULE_CLK_ALL_BE; Index ++)
    {
        if ((BIT0 << Index) & ModuleClockId)
        {
            COMM_CLUST_BE_REG32_ClearAndSet(CommClust_Module_Clock_Config_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_Module_Clock_Config_BE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_Module_Clock_Config_BE_Register[Index].CommCtrl_Event_Pos_Offset,
                ClockSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_HardReset_BE
USAGE          : Control Hard reset of BE Communication Cluster
INPUT          : Hard_Reset_FE_t HardResetId : block to hard reset
                 bool State : TRUE/FALSE
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_HardReset_BE (Hard_Reset_BE_t HardResetId, bool State)
{
    uint32_t Index;

    if (HardResetId > HARD_RESET_ALL_BE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < HARD_RESET_ALL_BE; Index ++)
    {
        if ((BIT0 << Index) & HardResetId)
        {
            if(State == TRUE)
            {
                COMM_CLUST_BE_REG32_Set(CommClust_Hard_Reset_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_BE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
            else //if(State == FALSE)
            {
                COMM_CLUST_BE_REG32_Clear(CommClust_Hard_Reset_BE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_BE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
        }
    }
    return (FVDP_OK);
}

/******************************************************************************

 FUNCTION       : EventCtrl_SetSyncEvent_LITE

 USAGE          : Sets event parameters source for PIP AUX and ENC module


 INPUT          : FVDP_CH_t CH - channel, it could be Pip, Aux, or Enc
                  Event_ID_LITE_t BlockId - block id of the event to be configured
                  uint32_t Position - vertical position
                  EventSource_t EventSource - source or event could be Input or Display
 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetSyncEvent_LITE(FVDP_CH_t CH, Event_ID_LITE_t EventId, uint32_t Position, EventSource_t EventSource)
{
    uint32_t Index;
    if (EventId > EVENT_ID_ALL_LITE)
        return FVDP_ERROR;

    if (EventSource)
    {
        COMM_CLUST_LITE_REG32_Set(CH,COMMCTRL_UPDATE_CFG, EventId);
    }
    else
    {
        COMM_CLUST_LITE_REG32_Clear(CH,COMMCTRL_UPDATE_CFG,EventId);
    }

    for (Index = 0; (BIT0 << Index) < EVENT_ID_ALL_LITE; Index ++)
    {
        if ((BIT0 << Index) & EventId)
        {
            COMM_CLUST_LITE_REG32_ClearAndSet(CH, CommClust_SyncEvent_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_SyncEvent_LITE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_SyncEvent_LITE_Register[Index].CommCtrl_Event_Pos_Offset,
                Position);
        }
    }

    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_EnableFwFrameReset_LITE

 USAGE          : Enables software generation of frame reset, sets enable bit and mask
                  Software generated frame reset by using EventCtrl_SetFwFrameReset_FE

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  FrameReset_Type_LITE_t FrameType - which module/frame to trigger

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_EnableFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_LITE)
    {
        COMM_CLUST_LITE_REG32_Set(CH, COMMCTRL_FW_FR_EN, FrameType);
//        COMM_CLUST_LITE_REG32_Set(CH,COMMCTRL_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_DisableFwFrameReset_FE

 USAGE          : Disables software generation of frame reset, Clears enable bit and mask

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  FrameReset_Type_FE_t FrameType - which module/frame to disable

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_DisableFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_LITE)
    {
        COMM_CLUST_LITE_REG32_Clear(CH, COMMCTRL_FW_FR_EN, FrameType);
//        COMM_CLUST_LITE_REG32_Clear(CH,COMMCTRL_FR_MASK , FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFwFrameReset_LITE

 USAGE          : Generates - forces firmware frame reset for given type of frame
                  (if previously enabled by EventCtrl_EnableFwFrameReset_FE)

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  FrameReset_Type_LITE_t FrameType - which module/frame to set

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_SetFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType)
{
    if (FrameType <= FRAME_RESET_ALL_LITE)
    {
        COMM_CLUST_LITE_REG32_Set(CH, COMMCTRL_FW_FR_CTRL, FrameType);
        return FVDP_OK;
    }
    else
        return  FVDP_ERROR;
    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFrameReset_LITE

 USAGE          : Sets up the position for frame reset and source of generating frame reset
                  - Input or Display, and disables fw frame reset.

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  FrameReset_Type_LITE_t FrameType
                  uint32_t FrameReset_Pos
                  EventSource_t EventSource

 OUTPUT         : None

 GLOBALS        : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter
******************************************************************************/
FVDP_Result_t EventCtrl_SetFrameReset_LITE (FVDP_CH_t              CH,
                                            FrameReset_Type_LITE_t FrameType,
                                            uint32_t               FrameReset_Pos,
                                            EventSource_t          EventSource)
{
    uint32_t Index;

    if (FrameType > FRAME_RESET_ALL_LITE)
        return  FVDP_ERROR;

    EventCtrl_DisableFwFrameReset_LITE(CH, FrameType);

    if (EventSource)
    {
        COMM_CLUST_LITE_REG32_Set(CH, COMMCTRL_FRST_CFG , FrameType);
    }
    else
    {
        COMM_CLUST_LITE_REG32_Clear(CH, COMMCTRL_FRST_CFG ,FrameType);
    }

    for (Index = 0; (BIT0 << Index) < FRAME_RESET_ALL_LITE; Index ++)
    {
        if ((BIT0 << Index) & FrameType)
        {
            COMM_CLUST_LITE_REG32_ClearAndSet(CH,
                                              CommClust_FrameReset_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                                              CommClust_FrameReset_LITE_Register[Index].CommCtrl_Event_Pos_Mask,
                                              CommClust_FrameReset_LITE_Register[Index].CommCtrl_Event_Pos_Offset,
                                              FrameReset_Pos);
        }
    }

    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineEdge_LITE

 USAGE          : Selected Flagline is Level or Edge sensitive, one common selection
                  for all Input flaglines, and other for all Display flaglines

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  EventSource_t EventSource - group of input or display flaglines
                  FlaglineType_t FlaglineType - the group is Level or Edge sensitive

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineEdge_LITE(FVDP_CH_t CH, EventSource_t EventSource, FlaglineType_t FlaglineType)
{
    uint32_t Address;

    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_SRC_FLAGLN_CFG ;
    else
        Address = COMMCTRL_DISP_FLAGLN_CFG;

    COMM_CLUST_LITE_REG32_ClearAndSet(CH, Address, COMMCTRL_SRC_FLAGLN_EDGE_MASK,COMMCTRL_SRC_FLAGLN_EDGE_OFFSET, FlaglineType);

    return FVDP_OK;
}
/******************************************************************************

 FUNCTION       : EventCtrl_SetFlaglineCfg_LITE

 USAGE          : Sets up flagline parameters, Position, and Repeat interval

 INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                  FlaglineName_t Name - which flagline
                  uint32_t FlaglinePos - position or line number of event generation
                  uint8_t FlaglineRepeatInterval - how often flaglines are generated,
                    and 0 for no repeat

 OUTPUT         : None

 GLOBALS        : None

 USED_REGS      : None

 PRE_CONDITION  : Associated register should be set before calling this routine

 RETURN         : FVDP_Result_t FVDP_OK - input parameter was OK
                                FVDP_ERROR - incorrect input parameter

******************************************************************************/
FVDP_Result_t EventCtrl_SetFlaglineCfg_LITE(FVDP_CH_t CH,	FlaglineName_t Name,
                                                	EventSource_t EventSource,
                                                	uint32_t FlaglinePos,
                                                	uint8_t FlaglineRepeatInterval)
{
    uint32_t Address;
    if (Name > FLAGLINE_7)
        return  FVDP_ERROR;
    if (EventSource == INPUT_SOURCE)
        Address = COMMCTRL_SRC_FLAGLN_POS_0;
    else
        Address = COMMCTRL_DISP_FLAGLN_POS_0;
    Address += Name * 4; // address difference between flaglines is 4
    COMM_CLUST_LITE_REG32_ClearAndSet(CH,Address  ,COMM_CLUST_PIP_SRC_FLAGLN_0_POS_MASK,COMM_CLUST_PIP_SRC_FLAGLN_0_POS_OFFSET,FlaglinePos);
    COMM_CLUST_LITE_REG32_ClearAndSet(CH,Address  ,COMM_CLUST_PIP_SRC_FLAGLN_0_RPT_MASK,COMM_CLUST_PIP_SRC_FLAGLN_0_RPT_OFFSET,FlaglineRepeatInterval);

    return FVDP_OK;
}

/******************************************************************************
 FUNCTION       : EventCtrl_GetCurrentFieldType_LITE
 USAGE          : return requested field type (odd/even polarity) of
                    LITE Communication Cluster
 INPUT          : EventSource_t EventSource: INPUT_SOURCE/DISPLAY_SOURCE
 OUTPUT         : FVDP_FieldType_t *FieldType: TOP_FIELD/BOTTOM_FIELD
 GLOBALS        : None
 PRE_CONDITION  :
 RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_GetCurrentFieldType_LITE(FVDP_CH_t CH, EventSource_t EventSource, FVDP_FieldType_t *FieldType)
{
    uint32_t Fid_Odd_Mask;

    Fid_Odd_Mask = (EventSource == INPUT_SOURCE) ? COMM_CLUST_PIP_SRC_FID_ODD_MASK : COMM_CLUST_PIP_DISP_FID_ODD_MASK;

    //FID_ODD: Low == TOP_FIELD    High == BOTTOM_FIELD
    if(COMM_CLUST_LITE_REG32_Read(CH, COMMCTRL_FID_STATUS) & Fid_Odd_Mask)
    {
        *FieldType = BOTTOM_FIELD;
    }
    else
    {
        *FieldType = TOP_FIELD;
    }

    return FVDP_OK;
}

/******************************************************************************
FUNCTION       : EventCtrl_SetOddSignal_LITE
USAGE          : Set firmware field ID (odd/even polarity)
                    LITE Communication Cluster
INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                 Field_ID_LITE_t FieldId:
                 FVDP_FieldType_t FieldType: TOP_FIELD/BOTTOM_FIELD
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_SetOddSignal_LITE (FVDP_CH_t CH, Field_ID_LITE_t FieldId, FVDP_FieldType_t FieldType)
{
    if(FieldType == TOP_FIELD)
    {
        COMM_CLUST_LITE_REG32_Set(CH, COMMCTRL_FW_FIELD_ID,FieldId);
    }
    else
    {
        COMM_CLUST_LITE_REG32_Clear(CH,COMMCTRL_FW_FIELD_ID,FieldId);
    }

    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ConfigOddSignal_LITE
USAGE          : Set field ID (odd/even polarity) source
                    LITE Communication Cluster
INPUT          : FVDP_CH_t CH - Channel Pip, Aux or Enc
                 Field_ID_LITE_t FieldId:
                 FieldSource_t FieldSource: FW_FID\SOURCE_IN\DISPLAY_OUT
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
FVDP_Result_t EventCtrl_ConfigOddSignal_LITE (FVDP_CH_t CH, Field_ID_LITE_t FieldId, FieldSource_t FieldSource)
******************************************************************************/
FVDP_Result_t EventCtrl_ConfigOddSignal_LITE (FVDP_CH_t CH, Field_ID_LITE_t FieldId, FieldSource_t FieldSource)
{
    uint32_t Index;

    if (FieldId > FIELD_ID_ALL_LITE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < FIELD_ID_ALL_LITE; Index ++)
    {
        if ((BIT0 << Index) & FieldId)
        {
            COMM_CLUST_LITE_REG32_ClearAndSet(CH,
                CommClust_FID_Config_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_FID_Config_LITE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_FID_Config_LITE_Register[Index].CommCtrl_Event_Pos_Offset,
                FieldSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_ClockCtrl_LITE
USAGE          : Select Module clock of LITE Communication Cluster
INPUT          : Module_Clk_BE_t ModuleClockId
                 ModuleClock_t ClockSource: T_CLK/GND/SYSTEM_CLOCL
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_ClockCtrl_LITE (FVDP_CH_t CH, Module_Clk_LITE_t ModuleClockId, ModuleClock_t ClockSource)
{
    uint32_t Index;

    if (ModuleClockId > MODULE_CLK_ALL_LITE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < MODULE_CLK_ALL_LITE; Index ++)
    {
        if ((BIT0 << Index) & ModuleClockId)
        {
            COMM_CLUST_LITE_REG32_ClearAndSet(CH,
                CommClust_Module_Clock_Config_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                CommClust_Module_Clock_Config_LITE_Register[Index].CommCtrl_Event_Pos_Mask,
                CommClust_Module_Clock_Config_LITE_Register[Index].CommCtrl_Event_Pos_Offset,
                ClockSource);
        }
    }
    return (FVDP_OK);
}

/******************************************************************************
FUNCTION       : EventCtrl_HardReset_LITE
USAGE          : Control Hard reset of BE Communication Cluster
INPUT          : Hard_Reset_FE_t HardResetId : block to hard reset
                 bool State : TRUE/FALSE
OUTPUT         :
GLOBALS        : None
PRE_CONDITION  :
RETURN         : FVDP_Result_t FVDP_OK
******************************************************************************/
FVDP_Result_t EventCtrl_HardReset_LITE (FVDP_CH_t CH, Hard_Reset_LITE_t HardResetId, bool State)
{
    uint32_t Index;

    if (HardResetId > HARD_RESET_ALL_LITE)
        return  FVDP_ERROR;

    for (Index = 0; (BIT0 << Index) < HARD_RESET_ALL_LITE; Index ++)
    {
        if ((BIT0 << Index) & HardResetId)
        {
            if(State == TRUE)
            {
                COMM_CLUST_LITE_REG32_Set(CH,
                    CommClust_Hard_Reset_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_LITE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
            else //if(State == FALSE)
            {
                COMM_CLUST_LITE_REG32_Clear(CH,
                    CommClust_Hard_Reset_LITE_Register[Index].CommCtrl_Event_Pos_Addr,
                    CommClust_Hard_Reset_LITE_Register[Index].CommCtrl_Event_Pos_Mask);
            }
        }
    }
    return (FVDP_OK);
}
