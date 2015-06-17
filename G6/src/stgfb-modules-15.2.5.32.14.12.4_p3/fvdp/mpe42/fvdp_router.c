/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_router.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_private.h"
#include "fvdp_system.h"
#include "fvdp_update.h"
#include "../fvdp_revision.h"
#include "fvdp_router.h"
#include "fvdp_prepost.h"
#include "fvdp_ivp.h"
#include "fvdp_services.h"

#include "fvdp_common.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */

#define TrROUTER(level,...) //TraceModuleOn(level,RouterDebugLevel,__VA_ARGS__)


#define xUSE_PIP
#define xUSE_AUX

/* Private Variables (static)------------------------------------------------ */

extern uint8_t MccDebugLevel;

static uint8_t RouterDebugLevel = 2;

static const DataPathModule_t* DataPath_FE[] =
{
    &PRE_MODULE[FVDP_MAIN],
    //&IVP_MODULE[FVDP_MAIN],
    //&HSCALER_FE_MODULE[FVDP_MAIN],
    //&VSCALER_FE_MODULE[FVDP_MAIN],
    //&FBM_MODULE[FVDP_MAIN],
    //&MOTDET_MODULE,
    //&STAT_MODULE[FVDP_MAIN],
    NULL,
};

static const DataPathModule_t* DataPath_BE[] =
{
    //&FBM_MODULE[FVDP_MAIN],
    //&DEINT_MODULE,  // required
    //&SCALER_RDMCC_MODULE[FVDP_MAIN],
    //&CROP_MODULE[FVDP_MAIN],
    //&VSCALER_BE_MODULE[FVDP_MAIN],
    //&HSCALER_BE_MODULE[FVDP_MAIN],
    //&OVP_MODULE[FVDP_MAIN],
    //&ACC_MODULE,
    //&ACM_MODULE,
    //&OCSC_MODULE[FVDP_MAIN],
    &POST_MODULE[FVDP_MAIN],
    NULL
};

#ifdef USE_PIP
static const DataPathModule_t* DataPath_PIP[] =
{
    &PRE_MODULE[FVDP_PIP],
    &IVP_MODULE[FVDP_PIP],
    &HSCALER_FE_MODULE[FVDP_PIP],
    &VSCALER_FE_MODULE[FVDP_PIP],
    &FBM_IN_MODULE[FVDP_PIP],
    &SCALER_WRMCC_MODULE[FVDP_PIP],
    NULL
};
#endif

#ifdef USE_AUX
static const DataPathModule_t* DataPath_AUX[] =
{
    &PRE_MODULE[FVDP_AUX],
    &IVP_MODULE[FVDP_AUX],
    &HSCALER_FE_MODULE[FVDP_AUX],
    &VSCALER_FE_MODULE[FVDP_AUX],
    &FBM_IN_MODULE[FVDP_AUX],
    &SCALER_WRMCC_MODULE[FVDP_AUX],
    NULL
};
#endif

#ifdef USE_ENC
static const DataPathModule_t* DataPath_ENC[] =
{
    &PRE_MODULE[FVDP_AUX],
    &IVP_MODULE[FVDP_AUX],
    &HSCALER_FE_MODULE[FVDP_AUX],
    &VSCALER_FE_MODULE[FVDP_AUX],
    &FBM_IN_MODULE[FVDP_AUX],
    &SCALER_WRMCC_MODULE[FVDP_AUX],
    NULL
};
#endif  // #ifdef USE_ENC

static const DataPathModule_t** pDataPath[NUM_FVDP_CH][NUM_PROCESSING_TYPES][2] =
{
    {
        // FVDP_MAIN - FVDP_SPATIAL
        { DataPath_FE, DataPath_BE },

        // FVDP_MAIN - FVDP_TEMPORAL
        { DataPath_FE, DataPath_BE },

        // FVDP_MAIN - FVDP_TEMPORAL_FRC
        { DataPath_FE, DataPath_BE },
    },
    {
        // FVDP_PIP - FVDP_SPATIAL
        { NULL, NULL },

        // FVDP_PIP - FVDP_TEMPORAL
        { NULL, NULL },

        // FVDP_PIP - FVDP_TEMPORAL_FRC
        { NULL, NULL },
    },
    {
        // FVDP_AUX - FVDP_SPATIAL
        { NULL, NULL },

        // FVDP_AUX - FVDP_TEMPORAL
        { NULL, NULL },

        // FVDP_AUX - FVDP_TEMPORAL_FRC
        { NULL, NULL },
    }
};

#define GET_INPUT_DATAPATH(CH,PROC)    \
    ((DataPathModule_t**) pDataPath[CH][PROC][0])

#define GET_OUTPUT_DATAPATH(CH,PROC)    \
    ((DataPathModule_t**) pDataPath[CH][PROC][1])

static DataPathConfig_t DataPathConfigParams[NUM_FVDP_CH];
static bool DataPathInitialized[NUM_FVDP_CH] = {FALSE, FALSE, FALSE};

static uint8_t UpdateInterrupted[NUM_FVDP_CH][NUM_UPDATE_OPTIONS];

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */

// NOTE: ModuleNameList[][] must follow enumeration in ModuleId_t
char ModuleNameList[NUM_MODULE_IDS][12] =
{
    "PRE",
    "IVP",
    "HSCALER_FE",
    "VSCALER_FE",
    "FBM_IN",
    "TNR",
    "AFM",
    "MOTDET",
    "CCS",
    "MADI",
    "MVE",
    "FBM_OUT",
    "DEINT",
    "SFSR",
    "HSCALER_BE",
    "VSCALER_BE",
    "CROP",
    "OVP",
    "ACC",
    "ACM",
    "OCSC",
    "POST",
    "SCALER_RMCC",
    "SCALER_WMCC",
    "STAT"
};

DataPathModule_t* pThisModule = NULL;
DataPathModule_t* pOrigModule = NULL;
FVDP_ProcessingType_t gProcessingType[NUM_FVDP_CH] =
    {FVDP_SPATIAL, FVDP_SPATIAL, FVDP_SPATIAL};

bool FirstTimeInit = FALSE;

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : FVDP_Result_t FVDP_Init (void)
Description : Initializes FVDP hardware blocks to software defaults,
              Loads and verifies VCPU code via streamer.
Parameters  : none
Assumptions :
Limitations :
Returns     : FVDP_OK if initialization encountered no issues, otherwise
              FVDP_ERROR is returned (indicating problem with VCPU code-
              loading, or communications, etc...)
*******************************************************************************/
FVDP_Result_t FVDP_Init (void)
{
    FVDP_Result_t result = FVDP_OK;

    // Initialize registers to expected initial values that differ
    // from the power-on default values
    // MCTI_HWInit (FVDP_MAIN);

#if 0   //def USE_VCPU
    // Initialize mailbox system
    MBOX_Init();

    // Display FVDP version
    TRC( TRC_ID_MAIN_INFO, "FVDP VERSION: %s",FVDP_ReleaseID );

    {
        extern const uint32_t vcpu_checksum_exp;
        uint32_t chksum;

        // Initialize VCPU and load initial data and code
        if ((result = VCPU_Init(vcpu_dram_data, vcpu_iram_code)) != FVDP_OK)
        {
            TrROUTER(DBG_ERROR, "VCPU code load error %d, ABORTED\n",
                     result);
            return result;
        }

        // Verify that data and code has been loaded correctly
        result = VCPU_CheckLoading(vcpu_dram_data, vcpu_iram_code, &chksum);
        if (result != FVDP_OK)
        {
            TrROUTER(DBG_ERROR, "VCPU verify error %d, ABORTED\n",
                     result);
            return result;
        }

        // Calculate and display VCPU checksum
        if (vcpu_checksum_exp == chksum)
        {
            TRC( TRC_ID_UNCLASSIFIED, "VCPU CHCKSUM: 0x%08x (OK)", chksum );
        }
        else
        {
            TrROUTER(DBG_ERROR,
                "VCPU CHCKSUM: 0x%08x (WARNING: should be 0x%08x!!)\n",
                chksum, vcpu_checksum_exp);
        }
    }
#endif


    return result;
}

/*******************************************************************************
Name        : void LoadActiveConfig (FVDP_CH_t CH, DataPathConfig_t* pConfig)
Description : This routine loads active configuration data from
              ActiveUpdateConfig_p structure to shared memory and synchronizes
              the VCPU data accordingly.
              Note that pConfig->ConfigUpdateFlags is overridden to 0 as the
              flags contained therewithin are updated automatically an the
              beginning of both input and output datapath updates.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pConfig - instance of type DataPathConfig_t
Assumptions : Channel is already open
Limitations :
Returns     : none
*******************************************************************************/
static void LoadActiveConfig (FVDP_CH_t CH, DataPathConfig_t* pConfig)
{
#if (0)
    if (ActiveUpdateConfig_p[CH] == NULL)
    {
        TrROUTER(DBG_ERROR, "(%d): no active config data!\n",
            CH);
    }

    pConfig->InputVideoInfo         = ActiveUpdateConfig_p[CH]->InputVideoInfo;
    pConfig->OutputVideoInfo        = ActiveUpdateConfig_p[CH]->OutputVideoInfo;
    pConfig->ConfigUpdateFlags      = 0;    // updated automatically elsewhere
    ZERO_MEMORY(&pConfig->InternalFlags, sizeof(InternalFlags_t));
    pConfig->ConfigUpdatePsiFlags   = ActiveUpdateConfig_p[CH]->VQDataUpdateFlag;
    pConfig->CropWindow             = ActiveUpdateConfig_p[CH]->CropWindow;
#endif
}

/*******************************************************************************
Name        : FVDP_Result_t traverse_datapath
                            (DataPathModule_t** ppList1,
                             DataPathModule_t** ppList2,
                             FVDP_Result_t (*func)(FVDP_CH_t CH,
                                                   DataPathModule_t* pModule,
                                                   void* param),
                             FVDP_CH_t CH, void* param)
Description : This generic routine applies function func to each data path
              module in lists ppList1 and ppList2, for specific channel CH
              and void parameter param.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              ppList1 - data path config list (input data path)
              ppList2 - data path config list (output data path)
              func - function to be invoked for each module in succession
              param - generic parameter of type void*
Assumptions :
Limitations :
Returns     : FVDP_Result_t - return will vary depending on module and function
*******************************************************************************/
static FVDP_Result_t traverse_datapath
                            (DataPathModule_t** ppList1,
                             DataPathModule_t** ppList2,
                             FVDP_Result_t (*func)(FVDP_CH_t CH,
                                                   DataPathModule_t* pModule,
                                                   void* param),
                             FVDP_CH_t CH, void* param)
{
    DataPathModule_t** ppModuleList = ppList1;
    DataPathModule_t* pPrev = NULL;
    int index = 0;
    FVDP_Result_t result = FVDP_OK;

    // begin iterating through modules in ppList1
    pThisModule = *ppModuleList;
    pThisModule->pPrev = NULL;

    while (pThisModule)
    {
        // if there was a previous module, link it to this current module
        if (pPrev)
        {
            pThisModule->pPrev = pPrev;
            pThisModule->pConfig = pPrev->pConfig;
            pPrev->pNext = pThisModule;
        }

        // call the specified function func for this module
        if (func)
        {
            result = func(CH,pThisModule,param);
            if (result != FVDP_OK)
                return result;
        }

        // advance to next module in the list.  if we reached the end,
        // advance to the next list ppList2 if not NULL, otherwise task
        // is complete.
        ppModuleList++;
        pPrev = pThisModule;
        pThisModule = *ppModuleList;
        // pThisModule->pPrev = pPrev;
        if (!pThisModule && index == 0 && ppList2)
        {
            index++;
            ppModuleList = ppList2;
            pThisModule = *ppModuleList;
        }
    }

    //DFR_MUX_Select_FE(CH, DFR_UNIT_VIDEO1_IN_Y_G_IN_ID, DFR_FE_VIDEO1_OUT_Y_G_MUX_SEL);
    //DFR_MUX_Enable_FE(CH, PRE_MODULE_ID, (FVDP_ColorSampling_t) 1);

    //DFR_MUX_Select_UNIT(CH, DFR_UNIT_VIDEO1_IN_Y_G_IN_ID, DFR_FE_VIDEO1_OUT_Y_G_MUX_SEL);

    if (pPrev)
        pPrev->pNext = NULL;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t router_module_init(FVDP_CH_t CH,
                                               DataPathModule_t* pModule,
                                               void* param)
Description : This is a helper routine intentionally used in-conjunction with
              traverse_datapath to initialize individual modules in the
              datapath.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pModule - module context in which this function applies
              param - void parameter (containing info of type InitOptions_t)
Assumptions : Channel is already open
Limitations :
Returns     : FVDP_OK - no errors encountered
              FVDP_MEM_INIT_ERROR - memory range exceeded
              otherwise, return is that of the pInit member
*******************************************************************************/
static FVDP_Result_t router_module_init(FVDP_CH_t CH,
                                        DataPathModule_t* pModule,
                                        void* param)
{
    FVDP_Result_t result;

    if (!pModule->pInit)
        return FVDP_OK;

    TrROUTER(DBG_INFO, "(%d): initializing %s...\n",
        CH,ModuleNameList[pModule->ModuleId]);

    result = pModule->pInit(CH, (InitOptions_t) param);
    if (result != FVDP_OK)
        return result;

    //result = MCC_CheckMemoryRange(CH,
    //                    pModule->DramConfig.StartAddr +
    //                    pModule->DramConfig.Size);
    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t router_module_term(FVDP_CH_t CH,
                                               DataPathModule_t* pModule,
                                               void* param)
Description : This is a helper routine intentionally used in-conjunction with
              traverse_datapath to terminate individual modules in the
              datapath.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pModule - module context in which this function applies
              param - void parameter (containing info of type TermOptions_t)
Assumptions : Channel is already open
Limitations :
Returns     : FVDP_OK - no errors encountered
              otherwise, return is that of the pTerm member
*******************************************************************************/
static FVDP_Result_t router_module_term(FVDP_CH_t CH,
                                        DataPathModule_t* pModule,
                                        void* param)
{
    if (!pModule->pUpdate)
        return FVDP_OK;

    TrROUTER(DBG_INFO, "(%d): terminating %s...\n",
        CH,ModuleNameList[pModule->ModuleId]);

    return pModule->pTerm(CH, (TermOptions_t) param);
}

/*******************************************************************************
Name        : FVDP_Result_t router_module_update(FVDP_CH_t CH,
                                                 DataPathModule_t* pModule,
                                                 void* param)
Description : This is a helper routine intentionally used in-conjunction with
              traverse_datapath to update individual modules in the
              datapath for input or output.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pModule - module context in which this function applies
              param - void parameter (containing info of type UpdateOptions_t)
Assumptions : Channel is already open
Limitations :
Returns     : FVDP_OK - no errors encountered
              otherwise, return is that of the pUpdate member
*******************************************************************************/
static FVDP_Result_t router_module_update(FVDP_CH_t CH,
                                          DataPathModule_t* pModule,
                                          void* param)
{
    if (!pModule->pUpdate)
        return FVDP_OK;

    TrROUTER(DBG_INFO, "(%d): updating %s...\n",
        CH,ModuleNameList[pModule->ModuleId]);

    return pModule->pUpdate(CH, (UpdateOptions_t) param);
}

/*******************************************************************************
Name        : FVDP_Result_t ROUTER_Init (FVDP_CH_t CH,
                                         FVDP_ProcessingType_t Processing)
Description : This routine invokes traverse_datapath with reference to
              router_module_init to update individual modules for both
              input and output datapaths.
              In addition, SOC_Init() is called if this is the first time
              a channel is being opened.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              Processing - type of processing to open channel with
Assumptions :
Limitations :
Returns     : FVDP_OK - no errors encountered
              FVDP_ERROR - invalid channel specified
              otherwise, return is that of the pInit member
*******************************************************************************/
FVDP_Result_t ROUTER_Init (FVDP_CH_t CH,
                           FVDP_ProcessingType_t Processing)
{
    DataPathModule_t** ppList1;
    DataPathModule_t** ppList2;
    DataPathConfig_t InitConfig;
    FVDP_Result_t result = FVDP_OK;

    // initialize the FVDP driver if not done so already
    if (FirstTimeInit == FALSE)
    {
        FVDP_Init();
        FirstTimeInit = TRUE;
    }

    gProcessingType[CH] = Processing;

    DataPathInitialized[CH] = FALSE;
    ZERO_MEMORY(&UpdateInterrupted[CH],sizeof(uint8_t) * NUM_UPDATE_OPTIONS);

    // retrieve both the input and output lists associated with this
    // processing type.
    ppList1 = GET_INPUT_DATAPATH(CH,Processing);
    ppList2 = GET_OUTPUT_DATAPATH(CH,Processing);
    if (!ppList1 || !ppList2)
        return FVDP_INFO_NOT_AVAILABLE;

    // first start with modules in the input data path
    pThisModule = *ppList1;    // first module

    // setup the first module to point to the initial configuration for setup
    ZERO_MEMORY(&InitConfig,sizeof(DataPathConfig_t));
    LoadActiveConfig (CH, &InitConfig);
    pThisModule->pConfig = &InitConfig;

    //pThisModule->DramConfig.StartAddr =
    //    ALIGNED64BYTE(ActiveUpdateConfig_p[CH]->ChannelConfig.MemBaseAddress);

    // call router_module_init() for all modules in the input and output path
    result = traverse_datapath(ppList1, ppList2, &router_module_init, CH, (void*) 0);

    if (result == FVDP_OK)
        DataPathInitialized[CH] = TRUE;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t ROUTER_Term (FVDP_CH_t CH)
Description : This routine invokes traverse_datapath with reference to
              router_module_term to terminate individual modules for both
              input and output datapaths.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - no errors encountered
              FVDP_ERROR - invalid option specified, or channel not opened
              otherwise, return is that of the pTerm member
*******************************************************************************/
FVDP_Result_t ROUTER_Term (FVDP_CH_t CH)
{
    DataPathModule_t** ppList1;
    DataPathModule_t** ppList2;
    FVDP_Result_t result = FVDP_OK;

    if (DataPathInitialized[CH] == FALSE)
        return FVDP_ERROR;

    // join the input and output data path for termination purposes
    ppList1 = GET_INPUT_DATAPATH(CH,gProcessingType[CH]);
    ppList2 = GET_OUTPUT_DATAPATH(CH,gProcessingType[CH]);
    if (!ppList1 || !ppList2)
        return FVDP_INFO_NOT_AVAILABLE;

    // call router_module_term() for all modules in the input and output path
    result = traverse_datapath(ppList1, ppList2, &router_module_term, CH, (void*) 0);

#if 0//def USE_VCPU
    // TODO: differentiate closing of datapath and shutdown of VCPU
    if (result == FVDP_OK)
    {
        DataPathInitialized[CH] = FALSE;

        // only when all channels are uninitialized can we terminate the VCPU
        if (VCPU_Initialized() == TRUE &&
            DataPathInitialized[FVDP_MAIN] == FALSE &&
            DataPathInitialized[FVDP_PIP ] == FALSE &&
            DataPathInitialized[FVDP_AUX ] == FALSE)
        {
            result = VCPU_Term();
            FirstTimeInit = FALSE;
        }
    }
#endif
    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t ROUTER_Update (FVDP_CH_t CH, UpdateOptions_t options)
Description : This routine invokes traverse_datapath with reference to
              router_module_update to update individual modules in the
              datapath for either input or output.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - no errors encountered
              otherwise, return is that of the module pUpdate member
              FVDP_ERROR - invalid option specified
*******************************************************************************/
FVDP_Result_t ROUTER_Update (FVDP_CH_t CH, UpdateOptions_t options)
{
    DataPathModule_t** ppList;
    FVDP_Result_t result = FVDP_OK;

#if 0//def USE_VCPU
    MBOX_Process();
#endif

     if (DataPathInitialized[CH] == FALSE)
        return FVDP_ERROR;

    // retrieve the input or output list of datapath modules
    if (options == UPDATE_INPUT_STAGE)
        ppList = GET_INPUT_DATAPATH(CH,gProcessingType[CH]);
    else if (options == UPDATE_OUTPUT_STAGE)
        ppList = GET_OUTPUT_DATAPATH(CH,gProcessingType[CH]);
    else
        return FVDP_ERROR;

    pThisModule = *ppList;
    pThisModule->pConfig = &DataPathConfigParams[CH];

    // load active configuration data to the first module in the list
    LoadActiveConfig (CH, pThisModule->pConfig);

    // if update was previously interrupted for some reason, raise the
    // config_update_all so that internal settings and HW registers are
    // updated in this iteration.
    if (UpdateInterrupted[CH][options] != 0)
        pThisModule->pConfig->InternalFlags.config_update_all = TRUE;

//    if(DEINT_BYPASS_SEL)
//        pThisModule->pConfig->InternalFlags.deint_bypass = TRUE;

    // call router_module_update() for every module in the input or output path
    result = traverse_datapath(ppList, NULL, &router_module_update, CH, (void*) options);

    // keep track if update was interrupted
    UpdateInterrupted[CH][options] = (result != FVDP_OK);

    return result;
}

/*******************************************************************************
Name        : void UpdateInternalFlags (
                    DataPathConfig_t* pConfig, DataPathConfig_t* pPrev)

Description : Raise flags to indicate need for updating hardware register
              settings and internal state, based on the comparison of the
              current frame data and the previous frame data.  This is
              called once on input update and once on output update.
              For input update, this is used to monitor change in properties
              on a frame-by-frame basis from CHD.
              For output update, this is used to monitor change in properties
              on a frame-by-frame basis from the FBM.  The reason for this is
              that update flags determined in the input stage could become
              discarded by the FBM under various FRC conditions.
Parameters  : pConfig - current configuration
              pPrev - previous configuration
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
void UpdateInternalFlags (DataPathConfig_t* pConfig, DataPathConfig_t* pPrev)
{
    // note that these assignments are here for short-hand only
    FVDP_VideoInfo_t*   pCurrIn     = &pConfig->InputVideoInfo;
    FVDP_VideoInfo_t*   pCurrOut    = &pConfig->OutputVideoInfo;
    FVDP_CropWindow_t*  pCurrCrop   = &pConfig->CropWindow;
    InternalFlags_t*    pCurrFlags  = &pConfig->InternalFlags;
    Video3D_Config_t*   pCurr3D     = &pConfig->Video3DConfig;
    FVDP_VideoInfo_t*   pPrevIn = NULL;
    FVDP_VideoInfo_t*   pPrevOut = NULL;
    FVDP_CropWindow_t*  pPrevCrop = NULL;
    InternalFlags_t*    pPrevFlags = NULL;
    Video3D_Config_t*   pPrev3D = NULL;

    // if pPrev is NULL (ie. no previous frame), request all to be updated.
    if (!pPrev)
    {
        pConfig->InternalFlags.config_update_all = TRUE;
    }
    else
    {
        // more short-hand mapping
        pPrevIn    = &pPrev->InputVideoInfo;
        pPrevOut   = &pPrev->OutputVideoInfo;
        pPrevCrop  = &pPrev->CropWindow;
        pPrevFlags = &pPrev->InternalFlags;
        pPrev3D    = &pPrev->Video3DConfig;
    }

    if (   pConfig->InternalFlags.config_update_all == TRUE
        // check for change in input frame characteristics
        || pCurrIn->FrameRate               != pPrevIn->FrameRate
        || pCurrIn->Width                   != pPrevIn->Width
        || pCurrIn->Height                  != pPrevIn->Height
        || pCurrIn->ScanType                != pPrevIn->ScanType
        || pCurrIn->ColorSpace              != pPrevIn->ColorSpace
        || pCurrIn->ColorSampling           != pPrevIn->ColorSampling
        // check for change in output frame characteristics
        || pCurrOut->FrameRate              != pPrevOut->FrameRate
        || pCurrOut->Width                  != pPrevOut->Width
        || pCurrOut->Height                 != pPrevOut->Height
        || pCurrOut->ScanType               != pPrevOut->ScanType
        || pCurrOut->ColorSpace             != pPrevOut->ColorSpace
        || pCurrOut->ColorSampling          != pPrevOut->ColorSampling
        // check for change in crop parameters
        || pCurrCrop->HCropWidth            != pPrevCrop->HCropWidth
        || pCurrCrop->HCropStart            != pPrevCrop->HCropStart
        || pCurrCrop->HCropStartOffset      != pPrevCrop->HCropStartOffset
        || pCurrCrop->VCropHeight           != pPrevCrop->VCropHeight
        || pCurrCrop->VCropStart            != pPrevCrop->VCropStart
        || pCurrCrop->VCropStartOffset      != pPrevCrop->VCropStartOffset
        // check for change in 3D configuration
        || pCurr3D->Video3DHAct             != pPrev3D->Video3DHAct
        || pCurr3D->Video3DHSpace           != pPrev3D->Video3DHSpace
        || pCurr3D->Video3DVAct             != pPrev3D->Video3DVAct
        || pCurr3D->Video3DVSpace           != pPrev3D->Video3DVSpace
        || pCurrIn->FVDP3DFormat            != pPrevIn->FVDP3DFormat
        || pCurrIn->FVDP3DSubsampleMode     != pPrevIn->FVDP3DSubsampleMode
        || pCurrOut->FVDP3DFormat           != pPrevOut->FVDP3DFormat
        || pCurrOut->FVDP3DSubsampleMode    != pPrevOut->FVDP3DSubsampleMode
        // check for change in special internal flags
        || pCurrFlags->rgb30bitprocess      != pPrevFlags->rgb30bitprocess
        || pCurrFlags->deint_bypass         != pPrevFlags->deint_bypass
        || pCurrFlags->deint_interlaced_out != pPrevFlags->deint_interlaced_out
        // check for change in other flag status originating from outside FVDP
        || (pCurrIn->FVDPDisplayModeFlag &
            FVDP_FLAGS_PICTURE_CHARACTERISTICS_CHANGED) != 0
        || (pCurrOut->FVDPDisplayModeFlag &
            FVDP_FLAGS_PICTURE_CHARACTERISTICS_CHANGED) != 0
       )
    {
        // due to complex interdependencies between cropping and scaling,
        // and between horizontal and vertical sizes for certain h/w blocks,
        // raise flags to request update from all modules.
        pConfig->InternalFlags.hscaler_touched = TRUE;
        pConfig->InternalFlags.vscaler_touched = TRUE;
        pConfig->InternalFlags.hcrop_touched   = TRUE;
        pConfig->InternalFlags.vcrop_touched   = TRUE;
//        pConfig->ConfigUpdateFlags |= CONFIG_UPDATE_INPUT_VIDEO_INFO;
//        pConfig->ConfigUpdateFlags |= CONFIG_UPDATE_OUTPUT_VIDEO_INFO;
    }

    if ((( pPrevIn != NULL ) &&
         (( pCurrIn->FrameRate != pPrevIn->FrameRate ) ||
          ( pCurrIn->ScanType != pPrevIn->ScanType ))) ||
        (( pPrevOut != NULL ) &&
         (( pCurrOut->FrameRate != pPrevOut->FrameRate ) ||
          ( pCurrOut->ScanType != pPrevOut->ScanType ))))
    {
        pConfig->InternalFlags.fbm_reset = TRUE;
    }
}

/*******************************************************************************
Name        : FVDP_Result_t router_module_dbg_print(FVDP_CH_t CH,
                                                    DataPathModule_t* pModule,
                                                    void* param)
Description : This is a helper routine intentionally used in-conjunction with
              traverse_datapath to print the debug level of individual modules
              in the datapath.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pModule - module context in which this function applies
              param - void parameter (not used)
Assumptions : Channel is already open
Limitations :
Returns     : FVDP_OK - no errors encountered
*******************************************************************************/
static FVDP_Result_t router_module_dbg_print(FVDP_CH_t CH,
                                             DataPathModule_t* pModule,
                                             void* param)
{
    param = param;  // suppress warning

    if (!pModule->pUpdate)
        return FVDP_OK;

    TRC( TRC_ID_MAIN_INFO, "  0x%08X    %-16s    %-11d", (1<<(pModule->ModuleId+2)), ModuleNameList[pModule->ModuleId], pModule->DebugLevel );

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_Result_t ROUTER_DebugPrintModules (FVDP_CH_t CH)
Description : This routine invokes traverse_datapath with reference to
              router_module_dbg_print to print the debug level of all modules
              in the selected datapath.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
Assumptions : channel was opened prior
Limitations :
Returns     : FVDP_OK - no errors encountered
              FVDP_INFO_NOT_AVAILABLE - no information found on datapath
*******************************************************************************/
FVDP_Result_t ROUTER_DebugPrintModules (FVDP_CH_t CH)
{
    DataPathModule_t** ppList1;
    DataPathModule_t** ppList2;

    if (DataPathInitialized[CH] == FALSE)
        return FVDP_ERROR;

    // join the input and output data path for termination purposes
    ppList1 = GET_INPUT_DATAPATH(CH,gProcessingType[CH]);
    ppList2 = GET_OUTPUT_DATAPATH(CH,gProcessingType[CH]);
    if (!ppList1 || !ppList2)
        return FVDP_INFO_NOT_AVAILABLE;

    TRC( TRC_ID_MAIN_INFO, "  %-10s    %-16s    %-11s\n",  "MASK ID","MODULE", "DEBUG LEVEL" );
    TRC( TRC_ID_MAIN_INFO, "  %-10s    %-16s    %-11s\n",  "-------","----------------", "-----------" );
    TRC( TRC_ID_MAIN_INFO, "  0x%08X    %-16s    %-11d\n", BIT0,"ROUTER", RouterDebugLevel );
    //TRC( TRC_ID_MAIN_INFO, "  0x%08X    %-16s    %-11d\n", BIT1,"MCC", MccDebugLevel );

    // call router_module_dbg_print() for all modules in the input and output path
    return traverse_datapath(ppList1, ppList2, &router_module_dbg_print, CH, (void*) 0);
}

typedef struct {
    uint32_t mask;
    uint8_t level;
} dbg_level_t ;

/*******************************************************************************
Name        : FVDP_Result_t router_module_dbg_level(FVDP_CH_t CH,
                                                    DataPathModule_t* pModule,
                                                    void* param)
Description : This is a helper routine intentionally used in-conjunction with
              traverse_datapath to change the debug level of individual modules
              in the datapath.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
              pModule - module context in which this function applies
              param - void* parameter (pointing to info of type dbg_level_t*)
Assumptions : Channel is already open
Limitations :
Returns     : FVDP_OK - no errors encountered
*******************************************************************************/
static FVDP_Result_t router_module_dbg_level(FVDP_CH_t CH,
                                             DataPathModule_t* pModule,
                                             void* param)
{
    dbg_level_t* dbglev = (dbg_level_t*) param;

    if (!pModule->pUpdate)
        return FVDP_OK;

    if ((1 << (pModule->ModuleId + 2)) & dbglev->mask)
    {
        pThisModule->DebugLevel = dbglev->level;

        TRC( TRC_ID_MAIN_INFO, "  debug level of %s set to %d for CH%d...",  ModuleNameList[pModule->ModuleId], dbglev->level, CH );
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_Result_t ROUTER_SetDebugLevel (
                    FVDP_CH_t CH, uint32_t Modules, uint8_t DebugLevel)
Description : This routine invokes traverse_datapath with reference to
              router_module_dbg_level to set the debug level of ALL modules.
              The associated channel CH does not need to be opened first in
              this case.
Parameters  : CH - FVDP_MAIN/FVDP_PIP/FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - no errors encountered
              FVDP_INFO_NOT_AVAILABLE - no datapath information found
*******************************************************************************/
FVDP_Result_t ROUTER_SetDebugLevel (FVDP_CH_t CH, uint32_t Modules, uint8_t DebugLevel)
{
    DataPathModule_t** ppList1;
    DataPathModule_t** ppList2;
    dbg_level_t dbglev;
    FVDP_ProcessingType_t proc;

    if (CH >= NUM_FVDP_CH)
        return FVDP_ERROR;

    // start with datapath with greatest number of modules
    // assumption: "highest datapath" is superset
    proc = NUM_PROCESSING_TYPES;
    do
    {
        proc --;
        ppList1 = GET_INPUT_DATAPATH(CH,proc);
        ppList2 = GET_OUTPUT_DATAPATH(CH,proc);
    } while ((!ppList1 || !ppList2) && (proc > 0));

    if (!ppList1 || !ppList2)
        return FVDP_INFO_NOT_AVAILABLE;

    if (Modules & BIT0)
    {
        TRC( TRC_ID_MAIN_INFO, "  setting debug level of %s to %d in CH = %d...",  "ROUTER", DebugLevel, CH );
        RouterDebugLevel = DebugLevel;
    }

    if (Modules & BIT1)
    {
        //TRC( TRC_ID_MAIN_INFO, "  setting debug level of %s to %d in CH = %d...",  //    "MCC", DebugLevel, CH );
        //MccDebugLevel = DebugLevel;
    }

    dbglev.level = DebugLevel;
    dbglev.mask = Modules;

    // call router_module_dbg_level() for all modules in the input and output path
    return traverse_datapath(ppList1, ppList2, &router_module_dbg_level, CH, (void*) &dbglev);
}

