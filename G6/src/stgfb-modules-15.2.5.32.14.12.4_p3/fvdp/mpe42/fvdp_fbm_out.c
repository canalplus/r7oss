/***********************************************************************
 *
 * File: ip/fvdp/mpe42/fvdp_fbm_out.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_fbm.h"
#include "fvdp_mbox.h"
#include "fvdp_services.h"
#include "fvdp_eventctrl.h"
#include "fvdp_vcd.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */

//#define TrFBM(level,...)  TraceModuleOn(level,pThisModule->DebugLevel,__VA_ARGS__)
#define TrFBM(level,...)

// NOTE: FBM_IF_OUTPUTUPDATE() macro interfaces with FBM_Output_Process() in the
// FBM core of the VCPU via mailbox.
//
#define FBM_IF_OUTPUTUPDATE(ch,ofs,ofd)                                     \
        OUTBOX_SendWaitReply(MBOX_ID_VCPU, MBOX_FBM_OUTPUT_PROCESS,         \
                             MBOX_SUB_CMD_NONE,                             \
                             (void*) ofs, sizeof(OutputFrameSelect_t),      \
                             (void*) ofd, sizeof(OutputFrameData_t));

/* Global Function Prototypes ----------------------------------------------- */

FVDP_Result_t FBM_OUT_Init   (FVDP_HandleContext_t* Context_p);
FVDP_Result_t FBM_OUT_Term   (FVDP_HandleContext_t* Context_p);
FVDP_Result_t FBM_OUT_Update (FVDP_HandleContext_t* Context_p);

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */

static FVDP_FieldType_t fbm_GetNextOutputFieldType (FVDP_CH_t CH);

/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : FVDP_Result_t FBM_OUT_Init (FVDP_HandleContext_t* Context_p)
Description : Initialize video buffer queues (for PIP/AUX)
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
FVDP_Result_t FBM_OUT_Init (FVDP_HandleContext_t* Context_p)
{
    FVDP_Result_t result = FVDP_OK;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_OUT_Term (FVDP_HandleContext_t* Context_p)
Description : Terminates FBM module
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
*******************************************************************************/
FVDP_Result_t FBM_OUT_Term (FVDP_HandleContext_t* Context_p)
{
    FVDP_Result_t result = FVDP_OK;

    return result;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_OUT_Update (FVDP_HandleContext_t* Context_p)
Description : On output stage, request frame info to display from VCPU.
              Handles communications between host-side and VCPU for retrieval
              of frames to free and frames to display.
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_OK - if no issues have been encountered
              FVDP_QUEUE_UNDERFLOW_ERROR - if pop on empty queue
*******************************************************************************/
FVDP_Result_t FBM_OUT_Update (FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_FrameInfo_t*   FrameInfo_p = &HandleContext_p->FrameInfo;
    FVDP_CH_t           CH = HandleContext_p->CH;
    OutputFrameSelect_t ofs;
    OutputFrameData_t   ofd;
    FVDP_Result_t       result = FVDP_OK;
    FVDP_FieldType_t    OutputFieldType;

    // invoke FBM core in VCPU to get frame indicies to output and free.
    OutputFieldType = fbm_GetNextOutputFieldType(CH);

    if(OutputFieldType == BOTTOM_FIELD)
    {
        ofs.FieldPolaritySel = FIELD_POLARITY_BOTTOM;
    }
    else
    {
        ofs.FieldPolaritySel = FIELD_POLARITY_TOP;
    }
    ofs.DataPathBE       = HandleContext_p->DataPath.DatapathLite;
    ofs.Channel = CH;

#ifdef USE_VCPU
    result = FBM_IF_OUTPUTUPDATE(CH,&ofs,&ofd);
    CHECK(result == FVDP_OK);
#else
    ofd.FreeFrameIdx[0] = INVALID_FRAME_INDEX;
    ofd.FreeFrameIdx[1] = INVALID_FRAME_INDEX;
    ofd.FreeFrameIdx[2] = INVALID_FRAME_INDEX;
    ofd.FreeFrameIdx[3] = INVALID_FRAME_INDEX;
    ofd.FreeFrameIdx[4] = INVALID_FRAME_INDEX;
    ofd.FreeFrameIdx[5] = INVALID_FRAME_INDEX;

    if (HandleContext_p->FrameInfo.CurrentProScanFrame_p != NULL)
    {
        ofd.OutFrameIdx = HandleContext_p->FrameInfo.CurrentProScanFrame_p->FramePoolIndex;
    }
    else
    {
        ofd.OutFrameIdx = INVALID_FRAME_INDEX;
    }
#endif

    // log frames to free on output update
    {
        uint32_t i;
        for (i = 0; i < MAX_FRAMES_TO_FREE_OUTPUT; i++)
        {
            uint32_t FrameID = INVALID_FRAME_ID;
            if (ofd.FreeFrameIdx[i] != INVALID_FRAME_INDEX)
            {
                FVDP_Frame_t* Frame_p;
                Frame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, ofd.FreeFrameIdx[i]);
                if (Frame_p != NULL)
                    FrameID = Frame_p->FrameID;
            }
            VCD_ValueChange(CH, VCD_EVENT_FREE_FRAME_ID_1 + i, FrameID);
        }
    }

    // free unused frames
    FBM_FreeFrames(HandleContext_p, ofd.FreeFrameIdx, MAX_FRAMES_TO_FREE_OUTPUT);

    // check that the FBM core has something to output.  Under known and
    // controlled situations, the FBM core will have nothing to output (eg.
    // on startup, or after FBM_Reset()).
    if (ofd.OutFrameIdx == INVALID_FRAME_INDEX)
    {
        FrameInfo_p->CurrentOutputFrame_p = NULL;   // nothing to release
        result = FVDP_NOTHING_TO_RELEASE;
    }

    if (FrameInfo_p->CurrentOutputFrame_p != NULL)
        FrameInfo_p->PrevOutputFrame = *(FrameInfo_p->CurrentOutputFrame_p);
    FrameInfo_p->CurrentOutputFrame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, ofd.OutFrameIdx);

    if (FrameInfo_p->CurrentOutputFrame_p != NULL)
    {
        FVDP_Frame_t * Frame_p = FrameInfo_p->CurrentOutputFrame_p;
        Frame_p->OutputVideoInfo.FieldType = OutputFieldType;

        VCD_ValueChange(CH, VCD_EVENT_OUTPUT_SCAN_TYPE_REQ,  Frame_p->OutputVideoInfo.ScanType);
        VCD_ValueChange(CH, VCD_EVENT_OUTPUT_FIELD_TYPE_REQ, Frame_p->OutputVideoInfo.FieldType);
        VCD_ValueChange(CH, VCD_EVENT_OUTPUT_FRAME_ID,       Frame_p->FrameID);

        // VCD log activity for monitoring latency
        VCD_ValueChange(CH, VCD_EVENT_VIDEO_LATENCY, vibe_os_get_system_time() - FrameInfo_p->PrevOutputFrame.Timestamp);

        // VCD log activity for checking discontinuity in output frame IDs
        {
            static uint8_t DiffID[NUM_FVDP_CH] = {0xFF, 0xFF, 0xFF, 0xFF};

            uint8_t PrevFrameID = FrameInfo_p->PrevOutputFrame.FrameID;
            uint8_t CurrentDiff = Frame_p->FrameID - PrevFrameID;

            if ((    PrevFrameID + 1 == INVALID_FRAME_ID
                 ||  (uint8_t) (INVALID_FRAME_ID + 1) == Frame_p->FrameID)
                &&  CurrentDiff > 0 )
                CurrentDiff --;

            if (CurrentDiff > 0 && CurrentDiff != DiffID[CH])
            {
                VCD_ValueChange(CH, VCD_EVENT_OUTPUT_DISCONTINUITY, 1);
                DiffID[CH] = Frame_p->FrameID - PrevFrameID;
            }
        }
    }
    else
    {
        VCD_ValueChange(CH, VCD_EVENT_OUTPUT_FRAME_ID, INVALID_FRAME_ID);
    }

    return result;
}

/*******************************************************************************
Name        : FVDP_FieldType_t fbm_GetNextOutputFieldType (FVDP_CH_t CH)
Description : return requested output field type (odd/even polarity)
Parameters  : CH - FVDP_MAIN / FVDP_PIP / FVDP_AUX
Assumptions :
Limitations :
Returns     : FVDP_FieldType_t - TOP_FIELD/BOTTOM_FIELD
*******************************************************************************/
FVDP_FieldType_t fbm_GetNextOutputFieldType (FVDP_CH_t CH)
{
    FVDP_FieldType_t FieldType;

#ifdef FVDP_SIMULATION
    {
        static uint8_t pol = 0;
        pol ^= BIT0;
        return pol ? BOTTOM_FIELD : TOP_FIELD;
    }
#endif

    if (CH == FVDP_MAIN)
    {
        EventCtrl_GetCurrentFieldType_BE(DISPLAY_SOURCE, &FieldType);
    }
    else
    {
        EventCtrl_GetCurrentFieldType_LITE(CH, DISPLAY_SOURCE, &FieldType);
    }

    //  Need to convert the output field type for the NEXT output event, we must use the INVERSE of the FID_STATUS signal.
    if(FieldType == TOP_FIELD)
    {
        FieldType = BOTTOM_FIELD;
    }
    else
    {
        FieldType = TOP_FIELD;
    }

    return FieldType;
}

/*******************************************************************************
Name        : FVDP_Result_t FBM_FreeFrames (FVDP_HandleContext_t* HandleContext_p,
                uint8_t *pIdx, uint8_t MaxFramesToFree)
Description : free frames associated with array of indices
Parameters  : pIdx pointer to array of indices
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t FBM_FreeFrames(FVDP_HandleContext_t* HandleContext_p, uint8_t *pIdx, uint8_t MaxFramesToFree)
{
    uint8_t i;

    // match "indices to free" with their FVDP_Frame_t counterpart
    // in FVDP_FramePool and free the associated member.
    for (i = 0; i < MaxFramesToFree; i ++)
    {
        if (*(pIdx+i) != INVALID_FRAME_INDEX)
        {
            FVDP_Frame_t* Frame_p = SYSTEM_GetFrameFromIndex(HandleContext_p, *(pIdx+i));
            if (Frame_p)
            {
                TrFBM(DBG_INFO,"free FrameIdx %d (FrameID %d)\n",
                      *(pIdx+i), Frame_p->FrameID);
                SYSTEM_FreeFrame(HandleContext_p, Frame_p);
            }
        }
    }

    return FVDP_OK;
}


