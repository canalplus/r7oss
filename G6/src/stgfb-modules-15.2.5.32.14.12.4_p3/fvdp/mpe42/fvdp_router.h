/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_router.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_ROUTER_H
#define FVDP_ROUTER_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "fvdp_private.h"
#include "fvdp_services.h"
//#include "fvdp_config.h"
#include "fvdp_mcc.h"
#include "fvdp_dfr.h"
/* Exported Constants ------------------------------------------------------- */

#define DATAPATH_CONFIG_POOL_SIZE   20

/* Only for initial bring up without VCPU */
#define USE_VCPU

#if 0
#ifndef USE_VCPU
#warning "VCPU disabled!"
#endif
#endif

#ifdef USE_VCPU
#define VCPU_MAIN
#define VCPU_PIP
#endif
/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    INIT_INPUT_STAGE = BIT0,
    INIT_OUTPUT_STAGE = BIT1,
} InitOptions_t;

typedef enum
{
    TERM_INPUT_STAGE = BIT0,
    TERM_OUTPUT_STAGE = BIT1,
} TermOptions_t;

typedef enum
{
    UPDATE_INPUT_STAGE,
    UPDATE_OUTPUT_STAGE,
    NUM_UPDATE_OPTIONS
} UpdateOptions_t;

typedef enum
{
    PRE_MODULE_ID = 0,
    IVP_MODULE_ID,
    HSCALER_FE_MODULE_ID,
    VSCALER_FE_MODULE_ID,
    FBM_IN_MODULE_ID,
    TNR_MODULE_ID,
    AFM_MODULE_ID,
    MOTDET_MODULE_ID,
    CCS_MODULE_ID,
    MADI_MODULE_ID,
    MVE_MODULE_ID,
    FBM_OUT_MODULE_ID,
    DEINT_MODULE_ID,
    SFSR_MODULE_ID,
    HSCALER_BE_MODULE_ID,
    VSCALER_BE_MODULE_ID,
    CROP_MODULE_ID,
    OVP_MODULE_ID,
    ACC_MODULE_ID,
    ACM_MODULE_ID,
    OCSC_MODULE_ID,
    POST_MODULE_ID,
    SCALER_RDMCC_MODULE_ID,
    SCALER_WRMCC_MODULE_ID,
    STAT_MODULE_ID,
    NUM_MODULE_IDS
} ModuleId_t;

typedef struct Video3DConfig_s
{
    uint16_t    Video3DHAct;
    uint16_t    Video3DVAct;
    uint16_t    Video3DHSpace;
    uint16_t    Video3DVSpace;
} Video3D_Config_t;

typedef struct InternalFlags_s
{
    uint32_t    hscaler_shrink : 1;
    uint32_t    vscaler_shrink : 1;
    uint32_t    hscaler_enable : 1;
    uint32_t    vscaler_enable : 1;
    uint32_t    hscaler_input_stage : 1;
    uint32_t    vscaler_input_stage : 1;
    uint32_t    hscaler_force_update_stage : 1;
    uint32_t    vscaler_force_update_stage : 1;

    uint32_t    ivp_awd_touched : 1;
    uint32_t    motdet_afm_touched : 1;
    uint32_t    hscaler_touched : 1;
    uint32_t    vscaler_touched : 1;
    uint32_t    hcrop_touched : 1;
    uint32_t    vcrop_touched : 1;
    uint32_t    config_update_all : 1;
    uint32_t    deint_bypass : 1;

    uint32_t    rgb30bitprocess : 1;
    uint32_t    madi_enable : 1;
    uint32_t    deint_interlaced_out : 1;

    uint32_t    fbm_reset : 1;

} InternalFlags_t;

typedef struct AFM_Measure_s
{
    uint32_t                FilmMode : 4;
    uint32_t                Phase : 3;
    uint32_t                StillImage : 1;
} AFM_Measure_t;

typedef struct MVE_Measure_s
{
    uint32_t                TBD;
} MVE_Measure_t;

typedef struct Measure_s
{
    AFM_Measure_t           AFM1;
    AFM_Measure_t           AFM2;
    MVE_Measure_t           MVE;
} Measure_t;

typedef struct DataPathConfig_s* DataPathConfig_p;
typedef struct DataPathConfig_s
{
    FVDP_VideoInfo_t        InputVideoInfo;
    FVDP_VideoInfo_t        OutputVideoInfo;
    FVDP_CropWindow_t       CropWindow;
//    uint8_t                 BufferIndex[NUM_BUFFER_IDX];
//    MCC_Config_t            MccConfig[NUM_MCC_CFG];
    Video3D_Config_t        Video3DConfig;
//    CONFIG_UpdateFlag_t     ConfigUpdateFlags;
//    CONFIG_PSIFeatureFlag_t ConfigUpdatePsiFlags;
    InternalFlags_t         InternalFlags;
    Measure_t               Measure;
    uint8_t                 InterpolationRatio;
    uint8_t                 FbmTagID;
    DataPathConfig_p        pPrev;
} DataPathConfig_t;

typedef struct DramPartitionConfig_s
{
    uint32_t        StartAddr;
    uint32_t        Size;
} DramPartitionConfig_t;

typedef struct DataPathModule_s* pDataPathModule_t;

typedef struct DataPathModule_s
{
    ModuleId_t          ModuleId;
    uint8_t             DebugLevel;

    pDataPathModule_t   pPrev, pNext;
    DataPathConfig_t*   pConfig;
    DramPartitionConfig_t DramConfig;

    void*               pPrivate;

    FVDP_Result_t       (*pInit)    (FVDP_CH_t CH, InitOptions_t options);
    FVDP_Result_t       (*pTerm)    (FVDP_CH_t CH, TermOptions_t options);
    FVDP_Result_t       (*pUpdate)  (FVDP_CH_t CH, UpdateOptions_t options);
    FVDP_Result_t       (*pAux)     (FVDP_CH_t CH, uint32_t aux_func_id,
                                     void* data, uint32_t size);
} DataPathModule_t;

/* Exported Variables ------------------------------------------------------- */

extern char ModuleNameList[NUM_MODULE_IDS][12];
extern DataPathModule_t* pThisModule;
extern FVDP_ProcessingType_t gProcessingType[NUM_FVDP_CH];
#if (0)
extern ObjectPool_t DataPathConfigPool[NUM_FVDP_CH];
extern DataPathConfig_t DataPathConfigPool_array[NUM_FVDP_CH][DATAPATH_CONFIG_POOL_SIZE];
extern uint8_t DataPathConfigPool_states[NUM_FVDP_CH][DATAPATH_CONFIG_POOL_SIZE];
#endif
extern uint32_t DataPathConfigIndex;

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

extern FVDP_Result_t ROUTER_Init (FVDP_CH_t CH, FVDP_ProcessingType_t Processing);
extern FVDP_Result_t ROUTER_Term (FVDP_CH_t CH);
extern FVDP_Result_t ROUTER_Update (FVDP_CH_t CH, UpdateOptions_t options);

extern FVDP_Result_t ROUTER_DebugPrintModules (FVDP_CH_t CH);
extern FVDP_Result_t ROUTER_SetDebugLevel     (FVDP_CH_t CH, uint32_t Modules,
                                               uint8_t DebugLevel);

extern void          UpdateInternalFlags (DataPathConfig_t* pConfig,
                                          DataPathConfig_t* pPrev);

extern FVDP_Result_t DFR_MUX_Select_FE(FVDP_CH_t CH, uint32_t input_port, uint32_t output_port);
extern FVDP_Result_t DFR_MUX_Select_UNIT(FVDP_CH_t CH, uint32_t input_port, uint32_t output_port);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_ROUTER_H */

