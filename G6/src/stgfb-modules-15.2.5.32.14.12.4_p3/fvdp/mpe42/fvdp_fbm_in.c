/***********************************************************************
 *
 * File: ip/fvdp/mpe42/fvdp_fbm_in.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_system.h"
#include "fvdp_fbm.h"
#include "fvdp_mbox.h"
#include "fvdp_vcpu.h"
#include "fvdp_services.h"
#include "fvdp_update.h"
#include "fvdp_vcd.h"
#include "fvdp_eventctrl.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

//#define DEBUG_FBM_IN_TESTING

#ifdef DEBUG_FBM_IN_TESTING
#warning "debug option DEBUG_FBM_IN_TESTING enabled"
#endif

#define FRAMERESETPOS           5
#define FLAGLINEOFFSET          1

/* Private Macros ----------------------------------------------------------- */

//#define TrFBM(level,...)  TraceModuleOn(level,pThisModule->DebugLevel,__VA_ARGS__)
#define TrFBM(level,...)

// NOTE: FBM_IF_Reset() macro interfaces with FBM_Common_Reset() in the FBM core
// of the VCPU via mailbox.
//
#define FBM_IF_RESET(param)                                                 \
        OUTBOX_SendWaitReply(MBOX_ID_VCPU, MBOX_FBM_RESET,                  \
                             MBOX_SUB_CMD_NONE,                             \
                             (void*)param, sizeof(FbmReset_t), NULL, 0)

// NOTE: FBM_IF_INPUTUPDATE() macro interfaces with FBM_Input_Process() in the
// FBM core of the VCPU via mailbox.
//
#define FBM_IF_INPUTUPDATE(ch,ifd,fbi)                                      \
        OUTBOX_SendWaitReply(MBOX_ID_VCPU, MBOX_FBM_INPUT_PROCESS,          \
                             MBOX_SUB_CMD_NONE,                             \
                             (void*) ifd, sizeof(InputFrameData_t),         \
                             (void*) fbi, sizeof(FrameBufferIndices_t))

#define ALLOC_FRAME(pool,idx) \
    (FVDP_Frame_t*) ObjectPool_Alloc_Object(&(pool)->FramePool,1,idx)

/* Private Variables (static)------------------------------------------------ */

#ifndef USE_VCPU
static uint32_t ProBufferIndex[NUM_FVDP_CH] = {0};
#endif

static uint16_t FlagLinePos[NUM_FVDP_CH] = { 0, 0, 0, 0 };

/* Global Function Prototypes ----------------------------------------------- */

FVDP_Result_t FBM_IN_Init   (FVDP_HandleContext_t* Context_p);
FVDP_Result_t FBM_IN_Term   (FVDP_HandleContext_t* Context_p);
FVDP_Result_t FBM_IN_Update (FVDP_HandleContext_t* Context_p);
FVDP_Result_t FBM_IF_SendBufferTable (SendBufferTableCommand_t* Command_p);

/* Private Function Prototypes ---------------------------------------------- */

static void FBM_IF_SetInputFrameData(FVDP_HandleContext_t * HandleContext_p,
                                     InputFrameData_t     * ifd,
                                     FVDP_Frame_t         * CurrentInputFrame_p,
                                     FVDP_Frame_t         * CurrentProScanFrame_p);

FVDP_Result_t SetFbmSynchroRegisters (FVDP_CH_t CH, uint16_t VStart,
                                      uint16_t VActive, FVDP_Input_t InputSource);

static uint8_t GetNewFrameID(FVDP_CH_t CH, FVDP_Frame_t* Frame_p);

/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : FVDP_Result_t FBM_IN_Init (FVDP_HandleContext_t* Context_p)
Description : Initialize video buffer queues (for PIP/AUX)
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
FVDP_Result_t FBM_IN_Init (FVDP_HandleContext_t* Context_p)
{
    FVDP_Result_t result = FVDP_OK;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t SetFbmSynchroRegisters (FVDP_CH_t CH)
Description : Automatically set flagline position for frame ready, FE sync-
              update and FE frame reset based on the programmed input active
              window and raster timing.
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
FVDP_Result_t SetFbmSynchroRegisters (FVDP_CH_t CH, uint16_t VStart, uint16_t VActive, FVDP_Input_t InputSource)
{
    FVDP_Result_t result   = FVDP_OK;

    static const uint32_t IVP_BASE_ADDR[] = {
        MIVP_BASE_ADDRESS, PIVP_BASE_ADDRESS, VIVP_BASE_ADDRESS, EIVP_BASE_ADDRESS};

    uint16_t NextFlagLinePos;

    /*MDTP1   FVDP_VMUX_INPUT5
      MDTP2   FVDP_VMUX_INPUT7
      MDTP0   FVDP_VMUX_INPUT6*/
    if((InputSource == FVDP_VMUX_INPUT5)
        || (InputSource == FVDP_VMUX_INPUT6)
        || (InputSource == FVDP_VMUX_INPUT7))
    {
        NextFlagLinePos = VStart + VActive + FLAGLINEOFFSET;
    }
    else
    {
        NextFlagLinePos = REG32_Read(IVP_BASE_ADDR[CH] + IVP_V_ACT_START_ODD) +
                          REG32_Read(IVP_BASE_ADDR[CH] + IVP_V_ACT_LENGTH) + 1;
    }

    if (FlagLinePos[CH] == 0)
        FlagLinePos[CH] = NextFlagLinePos;

    if (CH == FVDP_MAIN)
    {
        result |= EventCtrl_SetFlaglineCfg_FE(FLAGLINE_0, INPUT_SOURCE, FlagLinePos[CH], 0);
        result |= EventCtrl_SetFrameReset_FE(FRAME_RESET_ALL_FE, FRAMERESETPOS, INPUT_SOURCE);
        result |= EventCtrl_SetSyncEvent_FE(EVENT_ID_ALL_FE, FRAMERESETPOS - 1, INPUT_SOURCE);
    }
    else
    {
        result |= EventCtrl_SetFlaglineCfg_LITE(CH, FLAGLINE_0, INPUT_SOURCE, FlagLinePos[CH], 0);
    }

    VCD_ValueChange(CH, VCD_EVENT_HEARTBIT_COUNT, REG32_Read(MISC_FE_BASE_ADDRESS+MISC_FE_SW_SPARE_5));
    VCD_ValueChange(CH, VCD_EVENT_TEST_VALUE, FlagLinePos[CH]);

    FlagLinePos[CH] = NextFlagLinePos;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_IN_Term (FVDP_HandleContext_t* Context_p)
Description : Terminates FBM module
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
FVDP_Result_t FBM_IN_Term (FVDP_HandleContext_t* Context_p)
{
    FVDP_Result_t result = FVDP_OK;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_IN_Update (FVDP_HandleContext_t* Context_p)
Description : On input stage, send frame info to store to VCPU.
              Handles communications between host-side and VCPU for passing of
              new input frame data, and retrieving frame buffer indices.
              NOTES: currently only FVDP_MAIN supported in VCPU
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
              FVDP_QUEUE_UNDERFLOW_ERROR - if pop on empty queue
*******************************************************************************/
FVDP_Result_t FBM_IN_Update (FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_FrameInfo_t*     FrameInfo_p = &HandleContext_p->FrameInfo;
    FVDP_Frame_t*         CurrentInputFrame_p;
    FVDP_Frame_t*         CurrentProScanFrame_p;
    FVDP_Frame_t*         PrevProScanFrame_p;
    FVDP_Frame_t*         Prev1InputFrame_p;
    BufferType_t          BufferType = 0;
    FVDP_CH_t             CH = HandleContext_p->CH;
    InputFrameData_t      ifd;
    FrameBufferIndices_t  fbi;
    FVDP_Result_t         result = FVDP_OK;
    uint16_t              VStart;
    uint16_t              VActive;


    // Get the Current Input Frame
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    if (CurrentInputFrame_p == NULL)
    {
        VCD_ValueChange(CH, VCD_EVENT_INPUT_FRAME_ID, INVALID_FRAME_ID);
        return FVDP_ERROR;
    }

    VStart = HandleContext_p->InputRasterInfo.VStart;
    /*DynamicPixClk used in mdtp so active area will be equal to frame
     * height*/
    VActive = CurrentInputFrame_p->InputVideoInfo.Height;
    SetFbmSynchroRegisters(CH, VStart, VActive, HandleContext_p->InputSource);

    if (CurrentInputFrame_p->InputVideoInfo.FVDPDisplayModeFlag & FVDP_FLAGS_PICTURE_REPEATED)
    {
        VCD_ValueChange(CH, VCD_EVENT_PICTURE_REPEATED, 1);
    }

    // Get a new FrameID and timestamp for the Current Input Frame
    CurrentInputFrame_p->FrameID = GetNewFrameID(CH, CurrentInputFrame_p);

    // Log current frame properties here
    if (SYSTEM_IsFrameValid(HandleContext_p, CurrentInputFrame_p) == TRUE)
    {
        VCD_ValueChange(CH, VCD_EVENT_INPUT_FRAME_ID,    CurrentInputFrame_p->FrameID);
        VCD_ValueChange(CH, VCD_EVENT_INPUT_SCAN_TYPE,   CurrentInputFrame_p->InputVideoInfo.ScanType);
        VCD_ValueChange(CH, VCD_EVENT_INPUT_FIELD_TYPE,  CurrentInputFrame_p->InputVideoInfo.FieldType);
        VCD_ValueChange(CH, VCD_EVENT_INPUT_FRAME_RATE,  CurrentInputFrame_p->InputVideoInfo.FrameRate);
        VCD_ValueChange(CH, VCD_EVENT_OUTPUT_FRAME_RATE, CurrentInputFrame_p->OutputVideoInfo.FrameRate);
    }
    else
    {
        VCD_ValueChange(CH, VCD_EVENT_INPUT_FRAME_ID,INVALID_FRAME_ID);
    }

    if (    HandleContext_p->DataPath.DatapathFE >= DATAPATH_FE_INTERLACED
        &&  CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED
        &&  SYSTEM_IsFrameValid(HandleContext_p,FrameInfo_p->Prev2InputFrame_p) )
    {
        // Allocate (clone) Pro Scan Frame using the Prev2 Input Frame as the source
        CurrentProScanFrame_p = SYSTEM_CloneFrame(HandleContext_p, FrameInfo_p->Prev2InputFrame_p);
    }
    else
    {
        // Allocate (clone) Pro Scan Frame using the Current Input Frame as the source
        CurrentProScanFrame_p = SYSTEM_CloneFrame(HandleContext_p, CurrentInputFrame_p);
    }

    if (CurrentProScanFrame_p == NULL)
    {
        return FVDP_ERROR;
    }
    SYSTEM_SetCurrentProScanFrame(HandleContext_p, CurrentProScanFrame_p);

    // Overwrite the Pro Scan frame ScanType to PROGRESSIVE
    if(HandleContext_p->ProcessingType != FVDP_SPATIAL)
    {
        CurrentProScanFrame_p->InputVideoInfo.ScanType = PROGRESSIVE;
    }

    // Get a new FrameID for the Current ProScan Frame
    CurrentProScanFrame_p->FrameID = GetNewFrameID(CH, CurrentProScanFrame_p);

    // Populate input frame data to be passed to VCPU
    FBM_IF_SetInputFrameData(HandleContext_p, &ifd, CurrentInputFrame_p, CurrentProScanFrame_p);

    TrFBM(DBG_INFO,"sending FrameID %d\n",ifd.InputFrameID);

#ifdef USE_VCPU
    // Send input update information to FBM core in VCPU, and in return
    // retrieve buffer indices for further front-end processing.
    result = FBM_IF_INPUTUPDATE(CH,&ifd,&fbi);
    if (result != FVDP_OK)
    {
        return result;
    }
#else

    fbi.ProscanFrameIdx[0]  = CurrentProScanFrame_p->FramePoolIndex;
    fbi.ProscanFrameIdx[1]  = INVALID_FRAME_INDEX;
    fbi.InFrameIdx[0]       = INVALID_FRAME_INDEX;
    fbi.InFrameIdx[1]       = INVALID_FRAME_INDEX;
    fbi.InFrameIdx[2]       = INVALID_FRAME_INDEX;
    fbi.InFrameIdx[3]       = INVALID_FRAME_INDEX;

    fbi.FreeFrameIdx[0] = CurrentInputFrame_p->FramePoolIndex;
    fbi.FreeFrameIdx[1] = INVALID_FRAME_INDEX;
    fbi.FreeFrameIdx[2] = INVALID_FRAME_INDEX;
    fbi.FreeFrameIdx[3] = INVALID_FRAME_INDEX;
    fbi.FreeFrameIdx[4] = INVALID_FRAME_INDEX;
    fbi.FreeFrameIdx[5] = INVALID_FRAME_INDEX;
    fbi.BufferIdx[0]    = INVALID_BUFFER_INDEX;
    fbi.BufferIdx[1]    = INVALID_BUFFER_INDEX;
    fbi.BufferIdx[2]    = INVALID_BUFFER_INDEX;
    fbi.BufferIdx[3]    = INVALID_BUFFER_INDEX;

    fbi.BufferIdx[4]    = 0;
    fbi.BufferIdx[5]    = 0;
//    fbi.BufferIdx[4]    = ProBufferIndex[CH];
//    fbi.BufferIdx[5]    = ProBufferIndex[CH];

    if (ProBufferIndex[CH] == 0)
        ProBufferIndex[CH] = 1;
    else
        ProBufferIndex[CH] = 0;

#endif

    TrFBM(DBG_INFO,"got InFrameIdx (%d %d %d %d) ProscanFrameIdx (%d %d)\n",
        fbi.InFrameIdx[0],
        fbi.InFrameIdx[1],
        fbi.InFrameIdx[2],
        fbi.InFrameIdx[3],
        fbi.ProscanFrameIdx[0],
        fbi.ProscanFrameIdx[1]);
    TrFBM(DBG_INFO,"got FreeFrameIdx (%d %d %d %d)\n",
        fbi.FreeFrameIdx[0],
        fbi.FreeFrameIdx[1],
        fbi.FreeFrameIdx[2],
        fbi.FreeFrameIdx[3]);
    TrFBM(DBG_INFO,"got BufferIdx (%d %d %d %d %d %d)\n",
        fbi.BufferIdx[0],
        fbi.BufferIdx[1],
        fbi.BufferIdx[2],
        fbi.BufferIdx[3],
        fbi.BufferIdx[4],
        fbi.BufferIdx[5]);

    // log frames to free on input update
    {
        uint32_t i;
        for (i = 0; i < MAX_FRAMES_TO_FREE_INPUT; i++)
        {
            uint32_t FrameID = INVALID_FRAME_ID;
            if (fbi.FreeFrameIdx[i] != INVALID_FRAME_INDEX)
            {
                FVDP_Frame_t* Frame_p;
                Frame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, fbi.FreeFrameIdx[i]);
                if (Frame_p != NULL)
                    FrameID = Frame_p->FrameID;
            }
            VCD_ValueChange(CH, VCD_EVENT_FREE_FRAME_ID_1 + i, FrameID);
        }
    }

    // Free frames on input side (previous P3 frames and dropped ProScan frames)
    FBM_FreeFrames(HandleContext_p, fbi.FreeFrameIdx, MAX_FRAMES_TO_FREE_INPUT);

    // Check that the Current Pro Scan Frame index is the same as we assigned before -- the FBM should not change it.
    if (fbi.ProscanFrameIdx[0] != INVALID_FRAME_INDEX)
    {
        if (fbi.ProscanFrameIdx[0] != CurrentProScanFrame_p->FramePoolIndex)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  Current Pro Scan Frame Index is different than was assigned." );
            return FVDP_ERROR;
        }
    }

    // Check that the Previous Pro Scan Frame index is the same as we assigned before -- the FBM should not change it.
    if (fbi.ProscanFrameIdx[1] != INVALID_FRAME_INDEX)
    {
        FrameInfo_p->PrevProScanFrame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, fbi.ProscanFrameIdx[1]);
        if (FrameInfo_p->PrevProScanFrame_p == NULL)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  FBM returned Prev Pro Scan Frame Index [%d] that is incorrect.", fbi.ProscanFrameIdx[1] );
        }
    }
    else
    {
        FrameInfo_p->PrevProScanFrame_p = NULL;
    }

    // Check that the Current Input Frame index is the same as we assigned before -- the FBM should not change it.
    if (fbi.InFrameIdx[0] != INVALID_FRAME_INDEX)
    {
        if (fbi.InFrameIdx[0] != CurrentInputFrame_p->FramePoolIndex)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  Current Input Frame Index is different than was assigned." );
            return FVDP_ERROR;
        }
    }

    // Assign Prev1 Frame pointer based on Frame Index provided by FBM
    if (fbi.InFrameIdx[1] != INVALID_FRAME_INDEX)
    {
        FrameInfo_p->Prev1InputFrame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, fbi.InFrameIdx[1]);
        if (FrameInfo_p->Prev1InputFrame_p == NULL)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  FBM returned Prev1 Frame index [%d] that is incorrect.", fbi.InFrameIdx[1] );
        }
    }
    else
    {
        FrameInfo_p->Prev1InputFrame_p = NULL;
    }

    // Assign Prev2 Frame pointer based on Frame Index provided by FBM
    if (fbi.InFrameIdx[2] != INVALID_FRAME_INDEX)
    {
        FrameInfo_p->Prev2InputFrame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, fbi.InFrameIdx[2]);
        if (FrameInfo_p->Prev2InputFrame_p == NULL)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  FBM returned Prev2 Frame index [%d] that is incorrect.", fbi.InFrameIdx[2] );
        }
    }
    else
    {
        FrameInfo_p->Prev2InputFrame_p = NULL;
    }

    // Assign Prev3 Frame pointer based on Frame Index provided by FBM
    if (fbi.InFrameIdx[3] != INVALID_FRAME_INDEX)
    {
        FrameInfo_p->Prev3InputFrame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, fbi.InFrameIdx[3]);
        if (FrameInfo_p->Prev3InputFrame_p == NULL)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  FBM returned Prev3 Frame index [%d] that is incorrect.", fbi.InFrameIdx[3] );
        }
    }
    else
    {
        FrameInfo_p->Prev3InputFrame_p = NULL;
    }

    VCD_ValueChange(CH, VCD_EVENT_INPUT_PREV1_FRAME_ID,
                    SYSTEM_IsFrameValid(HandleContext_p, FrameInfo_p->Prev1InputFrame_p) == TRUE ?
                    FrameInfo_p->Prev1InputFrame_p->FrameID : INVALID_FRAME_ID);

    VCD_ValueChange(CH, VCD_EVENT_INPUT_PREV2_FRAME_ID,
                    SYSTEM_IsFrameValid(HandleContext_p, FrameInfo_p->Prev2InputFrame_p) == TRUE ?
                    FrameInfo_p->Prev2InputFrame_p->FrameID : INVALID_FRAME_ID);

    VCD_ValueChange(CH, VCD_EVENT_INPUT_PREV3_FRAME_ID,
                    SYSTEM_IsFrameValid(HandleContext_p, FrameInfo_p->Prev3InputFrame_p) == TRUE ?
                    FrameInfo_p->Prev3InputFrame_p->FrameID : INVALID_FRAME_ID);

    VCD_ValueChange(CH, VCD_EVENT_PROSCAN_FRAME_ID,
                    SYSTEM_IsFrameValid(HandleContext_p, CurrentProScanFrame_p) == TRUE ?
                    CurrentProScanFrame_p->FrameID : INVALID_FRAME_ID);

    VCD_ValueChange(CH, VCD_EVENT_PREV_PROSCAN_FRAME_ID,
                    SYSTEM_IsFrameValid(HandleContext_p, FrameInfo_p->PrevProScanFrame_p) == TRUE ?
                    FrameInfo_p->PrevProScanFrame_p->FrameID : INVALID_FRAME_ID);

    // Note:  The Frame Buffer Indexes (FrameBufferIndices_t.BufferIdx[]) returned by the VCPU FBM
    //        are not for the Current frame, but rather for the Previous frame.
    //        This is because the VCPU will only allocate the buffers (for write MCC clients) during
    //        the FrameReady process (not during the InputUpdate process), so that we do not need
    //        to allocate the buffer 'too early' and have to use more buffers than actually required.
#ifdef USE_VCPU
    Prev1InputFrame_p = SYSTEM_GetPrev1InputFrame(HandleContext_p);
    PrevProScanFrame_p = SYSTEM_GetPrevProScanFrame(HandleContext_p);
#else
    Prev1InputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    PrevProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);
#endif

    if (PrevProScanFrame_p != NULL) {
        PrevProScanFrame_p->Timestamp = vibe_os_get_system_time();
    }

    // For each of the buffer types
    for (BufferType = 0; BufferType < NUM_BUFFER_TYPES; BufferType++)
    {
        FVDP_Frame_t* PrevFrame;

        if ((BufferType == BUFFER_PRO_Y_G) || (BufferType == BUFFER_PRO_U_UV_B)
            ||(HandleContext_p->ProcessingType == FVDP_SPATIAL)
            ||(HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC))
            PrevFrame = PrevProScanFrame_p;
        else
            PrevFrame = Prev1InputFrame_p;

        // Check if the Prev frame is NULL -- it could be NULL for the first frame in the video stream.
        if (PrevFrame != NULL)
        {
            // Check if the buffer index is valid.  Invalid buffer index means MCC should not be enabled.
            if (fbi.BufferIdx[BufferType] != INVALID_BUFFER_INDEX)
            {
                // Allocate the buffer based on the buffer index provided by FBM
                PrevFrame->FrameBuffer[BufferType] = SYSTEM_AllocateBufferFromIndex(HandleContext_p,
                                                                                    BufferType,
                                                                                    fbi.BufferIdx[BufferType],
                                                                                    PrevFrame->InputVideoInfo.ScanType);
                // Check that the buffer allocation didn't fail
                if (PrevFrame->FrameBuffer[BufferType] == NULL)
                {
                    TRC( TRC_ID_MAIN_INFO, "ERROR:  - Could not allocate BufferType [%d] with Index [%d].", BufferType, fbi.BufferIdx[BufferType] );
                    TRC( TRC_ID_MAIN_INFO, "        - For Frame [%d].", PrevFrame->FramePoolIndex );
                    return FVDP_ERROR;
                }

                // Copy the MCC configuration into the allocated buffer objects
                PrevFrame->FrameBuffer[BufferType]->MccConfig = FrameInfo_p->CurrentMccConfig[BufferType];
            }
        }
    }

    return result;
}

/*******************************************************************************
Name        : void FBM_IF_SetInputFrameData (FVDP_HandleContext_t * HandleContext_p,
                                             InputFrameData_t     * ifd,
                                             FVDP_Frame_t         * CurrentInputFrame_p,
                                             FVDP_Frame_t         * CurrentProScanFrame_p)
Description : Populate InputFrameData_t structure pointed by ifd using
              information in FVDP_Frame_t structure pointed by Frame_p.
              This function is used to generate interface data for FBM core
              in the VCPU.
Parameters  : HandleContext_p - pointer to handle context
              ifd - structure to fill
              CurrentInputFrame_p - pointer to interlaced input field
              CurrentProScanFrame_p - pointer to input proscan field
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
static void FBM_IF_SetInputFrameData(FVDP_HandleContext_t * HandleContext_p,
                                     InputFrameData_t     * ifd,
                                     FVDP_Frame_t         * CurrentInputFrame_p,
                                     FVDP_Frame_t         * CurrentProScanFrame_p)
{
    uint32_t ifr, ofr;

    // populate input frame data
    ifd->Channel         =  HandleContext_p->CH;
    ifd->InputFrameID    =  CurrentInputFrame_p->FrameID;
    ifd->InputFrameIdx   =  (uint8_t)(CurrentInputFrame_p->FramePoolIndex);
    ifd->InputScanType   =  CurrentInputFrame_p->InputVideoInfo.ScanType;
    ifd->InputFieldType  =  CurrentInputFrame_p->InputVideoInfo.FieldType;
    ifr                  =  CurrentInputFrame_p->InputVideoInfo.FrameRate;
    ifd->Input3DFormat   =  CurrentInputFrame_p->InputVideoInfo.FVDP3DFormat;
    ifd->Input3DFlag     =  CurrentInputFrame_p->InputVideoInfo.FVDP3DFlag;
    ifd->Output3DFormat  =  CurrentInputFrame_p->OutputVideoInfo.FVDP3DFormat;
    ifd->OutputScanType  =  CurrentInputFrame_p->OutputVideoInfo.ScanType;
    ofr                  =  CurrentInputFrame_p->OutputVideoInfo.FrameRate;

    ifd->ProscanFrameID  =  CurrentProScanFrame_p->FrameID;
    ifd->ProscanFrameIdx =  (uint8_t)(CurrentProScanFrame_p->FramePoolIndex);

    if (CurrentInputFrame_p->InputVideoInfo.FVDPDisplayModeFlag & FVDP_FLAGS_PICTURE_REPEATED)
        ifd->IsPictureRepeated = TRUE;
    else
        ifd->IsPictureRepeated = FALSE;

    // notes: input/output VS rate is doubled for frame rate.
    // result is divided and rounded-up for units in Hz.
    if (ifd->InputScanType  == INTERLACED)
        ifr *= 2;
    if (ifd->OutputScanType == INTERLACED)
        ofr *= 2;
    ifd->InputFrameRate  = (ifr + 500) / 1000;
    ifd->OutputFrameRate = (ofr + 500) / 1000;

    if(HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC)
    {
        ifd->ProcessingType = FVDP_TEMPORAL;
    }
    else
    {
        ifd->ProcessingType = HandleContext_p->ProcessingType;
    }

    ifd->LatencyType = HandleContext_p->LatencyType;

    // when deinterlacer is available, use the specified processing type
    // and standard response to field frame repeat/drop.
    if (ifd->ProcessingType != FVDP_SPATIAL)
    {
        ifd->FieldPolRecovery = FPR_OPTION2;
    }
    else
    {
        // if interlaced-to-interlaced case...
        if (ifd->InputScanType == INTERLACED &&
            ifd->OutputScanType == INTERLACED)
        {
            // check if input/output rates are the same and use the
            // preferred option
            if (ifd->InputFrameRate == ifd->OutputFrameRate)
            {
                ifd->FieldPolRecovery = FPR_OPTION2 | FPR_OPTION3;
            }
            else
            {
                ifd->FieldPolRecovery = FPR_OPTION4 | FPR_OPTION3;
            }
        }
        else
            ifd->FieldPolRecovery = FPR_OPTION2;
    }

    //if (ifd->InputScanType == INTERLACED)
    {
        //ifd->FieldPolRecovery |= FPR_OPTION5;
        ifd->FieldPolRecovery |= FPR_OPTION6;
    }
    if (HandleContext_p->CH == FVDP_MAIN)
        ifd->DataPathFE   = HandleContext_p->DataPath.DatapathFE;
    else
        ifd->DataPathFE   = HandleContext_p->DataPath.DatapathLite;

  #ifdef DEBUG_FBM_IN_TESTING
    {
        uint32_t recov_override = REG32_Read(SCALERS_SW_SPARE_1);
        uint32_t recov_setting  = REG32_Read(SCALERS_SW_SPARE_2);
        if (recov_override & BIT0)
            ifd->FieldPolRecovery = recov_setting;
    }
  #endif
    ifd->TimingTransition  =  0;    // use timing transition detection in VCPU

    if(HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC)
    {
       ifd->BufferAllocEnable = 0x2f;    // for now
    }
    else
    {
       ifd->BufferAllocEnable = ~0;    // for now
    }
}

/*******************************************************************************
Name        : void FBM_IF_CreateBufferTable (const FVDP_FrameInfo_t* FrameInfo_p,
                                             BufferTable_t* Table_p,
                                             uint8_t* Rows)
Description : Create table containing buffer start addresses and size of each
              buffer, used for intefacing FBM core.
              This function is used to generate interface data for FBM core
              in the VCPU.
Parameters  : FrameInfo_p - The frame information structure that holds the buffer tables
              Table_p - table to fill
Assumptions : Buffer Pools are already created for this handle
Limitations :
Returns     : void
*******************************************************************************/
void FBM_IF_CreateBufferTable(const FVDP_FrameInfo_t* FrameInfo_p,
                              BufferTable_t*          Table_p,
                              uint8_t*                Rows_p)
{
    const FVDP_BufferPool_t* BufferPool_p;
    BufferTable_t*           Row_p = Table_p;
    BufferType_t             BufferType = 0;
    uint8_t                  RowCount = 0;

    ZERO_MEMORY (Table_p, sizeof(BufferTable_t) * NUM_BUFFER_TYPES);
    *Rows_p = 0;

    // Go though all the buffer types and build the buffer table.
    for (BufferType = 0; BufferType < NUM_BUFFER_TYPES; BufferType++)
    {
        // Set the Buffer Pool pointer based on buffer type
        BufferPool_p = &(FrameInfo_p->BufferPool[BufferType]);

        // Only include buffer types that are valid for this case
        if (BufferPool_p->NumberOfBuffers != 0)
        {
            Row_p->BufferType      = BufferType;
            Row_p->BufferStartAddr = BufferPool_p->BufferStartAddr;
            Row_p->BufferSize      = BufferPool_p->BufferSize;
            Row_p->NumOfBuffers    = BufferPool_p->NumberOfBuffers;
            Row_p ++;
            RowCount ++;
        }
    }

    *Rows_p = RowCount;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_IF_SendBufferTable (
                                SendBufferTableCommand_t* Command_p)
Description : Send buffer table to VCPU
Parameters  : Command_p - command to send
Assumptions :
Limitations :
Returns     : FVDP_OK - if there are no issues
*******************************************************************************/
#ifdef USE_VCPU
FVDP_Result_t FBM_IF_SendBufferTable (SendBufferTableCommand_t* Command_p)
{
    // note: size computed based on actual number of rows in the table
    uint32_t size = 0;
    if (Command_p->Rows<NUM_BUFFER_TYPES)
        size = offsetof(SendBufferTableCommand_t,Table[Command_p->Rows]);
    else
        size = sizeof(SendBufferTableCommand_t);

    return OUTBOX_SendWaitReply(MBOX_ID_VCPU, MBOX_SEND_BUFFER_TABLE,
                                MBOX_SUB_CMD_NONE, Command_p, size, NULL, 0);
}
#endif

/*******************************************************************************
Name        : FVDP_Result_t FBM_Reset (FVDP_HandleContext_t* Context_p)
Description : Reset the both the FBM frame information and the datapath
              config pool.
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
              HardReset - set to TRUE to not discard current output frame
Assumptions :
Limitations :
Returns     : FVDP_OK - if there are no issues
*******************************************************************************/
#ifdef USE_VCPU
FVDP_Result_t FBM_Reset (FVDP_HandleContext_t* Context_p, bool HardReset)
{
    FbmReset_t fbm_reset;
    fbm_reset.Channel = Context_p->CH;
    fbm_reset.SoftReset = !HardReset;
    return FBM_IF_RESET(&fbm_reset);
}
#endif

/*******************************************************************************
Name        : FVDP_Result_t FBM_IF_SendDataPath (FVDP_CH_t CH,
                                                 DataPathComponent_t Component,
                                                 uint8_t DataPath)
Description : Send datapath to VCPU
Parameters  : CH - channel to adjust datapath (on FrameReady)
              Component - datapath component (main FE/BE, lite)
              DataPath - datapath index
Assumptions :
Limitations :
Returns     : FVDP_OK - if there are no issues
*******************************************************************************/
#ifdef USE_VCPU
FVDP_Result_t FBM_IF_SendDataPath (FVDP_CH_t CH,
                                   DataPathComponent_t Component,
                                   uint8_t DataPath)
{
    DataPathSelect_t Select;
    Select.Channel   = CH;
    Select.Component = Component;
    Select.DataPath  = DataPath;

    return OUTBOX_SendWaitReply(MBOX_ID_VCPU, MBOX_SET_DATAPATH,
                                MBOX_SUB_CMD_NONE,
                                (void*)&Select, sizeof(DataPathSelect_t), NULL, 0);
}
#endif

/*******************************************************************************
Name        : uint8_t GetNewFrameID (FVDP_HandleContext_t* Context_p, FVDP_Frame_t* Frame_p)
Description : Determine next field/frame tag ID
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if there are no issues
*******************************************************************************/
uint8_t GetNewFrameID (FVDP_CH_t CH, FVDP_Frame_t* Frame_p)
{
    static uint8_t fbm_tag_id[NUM_FVDP_CH] = {0,0,0,0};

    // increment tag ID
    if (++ fbm_tag_id[CH] == INVALID_FRAME_ID)
        ++ fbm_tag_id[CH];

    return fbm_tag_id[CH];
}

/* end of file */
