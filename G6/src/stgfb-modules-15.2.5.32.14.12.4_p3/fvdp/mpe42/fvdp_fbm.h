/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_FBM_H
#define FVDP_FBM_H

/* Includes ----------------------------------------------------------------- */
#include "fvdp_system.h"
#include "fvdp_router.h"
#include "fvdp_mbox.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


#ifndef PACKED
#define PACKED  __attribute__((packed))
#endif

/* Exported Definitions ----------------------------------------------------- */

#define MAX_FRAMES_TO_FREE_INPUT    6   // max frames to free on input update
#define MAX_FRAMES_TO_FREE_OUTPUT   6   // max frames to free on output update
#define MAX_INP_RET_FRAMES          4   // max frames to return for front-end
                                        // processing (eg. for CCS, TNR)
#define MAX_PRO_RET_FRAMES          2   // max proscan frames to return
#define MAX_FRAMES_TO_OUTPUT        1   // max frames to return for back-end
                                        // processing:
                                        // 1 - progressive/spatial
                                        // 2 - two fields to deinterlace, or
                                        //     two frames to interpolate
                                        // 4 - two pairs of fields to perform
                                        //     deinterlacing and interpolation

/* Exported Enumerations ---------------------------------------------------- */

typedef enum FBMStates_e
{
    FBM_STATE_STOP,
    FBM_STATE_PLAY,
    FBM_STATE_PAUSE
} FBMStates_t;

typedef enum FBMError_e
{
    FBM_ERROR_NONE                      = 0,

    FBM_ERROR_INVALID_PARAM             = BIT0,
    FBM_ERROR_FRAME_ALLOCATE_FAILED     = BIT1,
    FBM_ERROR_BUFFER_ALLOCATE_FAILED    = BIT2,
    FBM_ERROR_COMMAND_ALLOCATE_FAILED   = BIT3,
    FBM_ERROR_NULL_EXCEPTION            = BIT4,
    FBM_ERROR_OVERFLOW                  = BIT5,

    FBM_WARN_NOT_ACTIVE                 = BIT8,
    FBM_WARN_NO_OUTPUT_COMMAND          = BIT9,
    FBM_WARN_NO_FRAME_ALLOC             = BIT10,
    FBM_WARN_NO_PREV_PROCESS            = BIT11,
    FBM_WARN_NO_INPUT_FRAMES            = BIT12,
} FBMError_t;

typedef enum FBMChannel_e
{
    FBM_MAIN,
    FBM_PIP,
    FBM_AUX,
    NUM_FBM_CH
} FBMChannel_t;

typedef enum MVEClientUpdateType_e
{
    MVE_UPDATE_IP,
    MVE_UPDATE_CORE
} MVEClientUpdateType_t;

typedef enum FieldPolRecovery_e
{
    FPR_OPTION1 = BIT0, // repeat 2 fields: this is good when scaler is not
                        // available on the output-end to convert an odd/even-
                        // field to opposity polarity (eg. PIP/AUX, and MAIN
                        // depending on position of the scaler).  Because fields
                        // are spatially and temporally different, the side-
                        // effect is that repeated fields will go back-in-time.
    FPR_OPTION2 = BIT1, // repeat/drop 1 field: polarity inversion expected on
                        // output side.  Use this when scaler is in the backend
                        // and input-output frequency are (nearly) the same.
    FPR_OPTION3 = BIT2, // repeat same field twice: to avoid going backwards
                        // in time, same field is repeated twice.  One of the
                        // repeated fields will be converted to opposite field
                        // polarity if scaler is available in the output stage.
                        // Note that this is good for cases in which input and
                        // output field rates are the same.
    FPR_OPTION4 = BIT3, // repeat/drop 1 field: this is same as FPR_OPTION2,
                        // except any number of fields will be converted to
                        // that of opposite field polarity.
    FPR_OPTION5 = BIT4, // automatically discard field if polarity has not
                        // changed.
    FPR_OPTION6 = BIT5, // automatically discard repeated fields/frames
    MAX_FPR_OPTION
} FieldPolRecovery_t;   // (field polarity recovery method)

typedef enum FieldPolarity_e
{
    FIELD_POLARITY_TOP,     // lines 1,3,5,7,... (top/default)
    FIELD_POLARITY_BOTTOM   // lines 2,4,6,8,... (bottom)
} FieldPolarity_t;

typedef enum ThreeDPolarity_e
{
    THREED_POLARITY_LEFT,   // left/default
    THREED_POLARITY_RIGHT   // right
} ThreeDPolarity_t;

typedef enum DataPathComponent_e
{
    DATAPATH_MAIN_FE_LITE,
    DATAPATH_MAIN_BE,
} DataPathComponent_t;

/* Exported Data Structure Types -------------------------------------------- */

typedef struct PACKED BufferTable_s
{
    uint8_t  BufferType;
    uint32_t BufferStartAddr;
    uint32_t BufferSize;
    uint32_t NumOfBuffers;
} BufferTable_t;

typedef struct PACKED SendBufferTableCommand_s
{
    uint8_t       Channel;
    uint8_t       Rows;
    BufferTable_t Table[NUM_BUFFER_TYPES];
} SendBufferTableCommand_t;

typedef struct PACKED FbmReset_s
{
    uint8_t    Channel;
    uint8_t    SoftReset;
} FbmReset_t;

typedef struct PACKED FbmStateConfig_s
{
    uint8_t    Channel;
    uint8_t    State;
} FbmStateConfig_t;

typedef struct PACKED DataPathSelect_s
{
    uint8_t    Channel;
    uint8_t    Component;           // DataPathComponent_t
    uint8_t    DataPath;
} DataPathSelect_t;

typedef struct PACKED InputFrameData_s
{
    uint8_t Channel;                // FVDP_CH_t
    uint8_t ProcessingType;         // FVDP_ProcessingType_t
    uint8_t DataPathFE;             // DATAPATH_FE_t/DATAPATH_LITE_t
    uint8_t FieldPolRecovery;       // FieldPolRecovery_t

    uint8_t InputFrameID;           // note that the counter should roll over at 256
    uint8_t InputFrameIdx;          // Index for associated FVDP_Frame_t
    uint8_t InputScanType;          // FVDP_ScanType_t
    uint8_t InputFieldType;         // FVDP_FieldType_t
    uint8_t InputFrameRate;         // in Hz
    uint8_t Input3DFormat;          // FVDP_3DFormat_t
    uint8_t Input3DFlag;            // FVDP_3DFlag_t
    uint8_t IsPictureRepeated;      // TRUE if picture is repeated image of previous frame

    uint8_t ProscanFrameID;         // note that the counter should roll over at 256
    uint8_t ProscanFrameIdx;        // Index for associated FVDP_Frame_t

    uint8_t Output3DFormat;         // FVDP_3DFormat_t
    uint8_t OutputScanType;         // FVDP_ScanType_t
    uint8_t OutputFrameRate;        // in Hz

    uint8_t  TimingTransition;      // bool to indicate whether timing transition occurred
    uint16_t BufferAllocEnable;     // bit vector for enabling/disabling buffer types
    uint8_t  LatencyType;           // FVDP_LatencyType_t
} InputFrameData_t;

typedef struct PACKED FrameBufferIndices_s
{
    uint8_t InFrameIdx[MAX_INP_RET_FRAMES];          // frame indicies for front-end processing
                                                     // note: array Idx 0=C, 1=P, 2=P-1, etc...
    uint8_t ProscanFrameIdx[MAX_PRO_RET_FRAMES];     // index for the proscan frames
    uint8_t FreeFrameIdx[MAX_FRAMES_TO_FREE_INPUT];  // frame indices freed by FBM on input update
    uint8_t BufferIdx[NUM_BUFFER_TYPES];             // buffer indices allocated by FBM
    uint8_t FilmState;                               // film state associated with prev frame
} FrameBufferIndices_t;

typedef struct PACKED MVEBufferIndices_s
{
    uint8_t SS1Wr;
    uint8_t SS2Wr;
    uint8_t SS4Wr;
    uint8_t EDGEWr;
    uint8_t EDGERd;
    uint8_t DiffWr;
    uint8_t RPDWr;
    uint8_t SS1CRd;
    uint8_t SS2CRd;
    uint8_t SS4CRd;
    uint8_t SS1PRd;
    uint8_t SS2PRd;
    uint8_t SS4PRd;
    uint8_t DiffCRd;
    uint8_t DiffPRd;
    uint8_t RPDCRd;
    uint8_t RPDPRd;
    uint8_t MV2XWr;
    uint8_t MV2XRd;
    uint8_t FWMVRw;
    uint8_t BWMVRw;
} MVEBufferIndices_t;

typedef struct PACKED OutputFrameSelect_s
{
    uint8_t Channel;            // FVDP_CH_t
    uint8_t DataPathBE;         // DATAPATH_BE_t (FVDP_MAIN only)
    uint8_t FieldPolaritySel;   // requested output field polarity (FieldPolarity_t)
    uint8_t ThreeDPolaritySel;  // requested output 3D polarity (ThreeDPolarity_t)
} OutputFrameSelect_t;

typedef struct PACKED OutputFrameData_s
{
    uint8_t OutFrameIdx;
    uint8_t OutFrameID;
    uint8_t InterpolationRatio;   // 0: use previous frame,
                                  // 1: use current frame,
                                  // between 0-1: use MCTi to interpolate
    uint8_t ThreeDLRIdx;          // 0-Left, 1- Right
    uint8_t FreeFrameIdx[MAX_FRAMES_TO_FREE_OUTPUT];
    uint8_t FreeFrameID[MAX_FRAMES_TO_FREE_OUTPUT];
    uint8_t BdrBufferIdx;         // for BorderCrop buffer output
    uint8_t DispOutFrameIdx;      // output frame to display (2nd pass only)
} OutputFrameData_t;

typedef struct PACKED FBMDebugInputPrevData_s
{
    uint32_t RVal;
    uint32_t SVal;
    uint8_t  Afp1Mode;
    uint8_t  Afp1Phase;
    uint8_t  Afp2Mode;
    uint8_t  Afp2Phase;
} FBMDebugInputPrevData_t;

typedef struct PACKED FBMDebugData_s
{
    uint8_t  Data[29];       // WARNING: dep. on size of fbm_if structs!
    uint32_t TimeStamp:28;   // align to 32 bit
    uint32_t LogEvent:4;     // FBMLogEvents_t
} FBMDebugData_t;

#define FBM_WARN_MASK   (0                              \
                         | FBM_WARN_NOT_ACTIVE          \
                         | FBM_WARN_NO_OUTPUT_COMMAND   \
                         | FBM_WARN_NO_FRAME_ALLOC      \
                         | FBM_WARN_NO_PREV_PROCESS     \
                         | FBM_WARN_NO_INPUT_FRAMES)

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

#define INVALID_FRAME_ID        0xFF
#define INVALID_FRAME_INDEX     0xFF
#define INVALID_BUFFER_INDEX    0xFF

/* Exported Functions ------------------------------------------------------- */

extern FVDP_Result_t FBM_Reset     (FVDP_HandleContext_t* Context_p,
                                    bool HardReset);
extern FVDP_Result_t FBM_IN_Init   (FVDP_HandleContext_t* Context_p);
extern FVDP_Result_t FBM_IN_Term   (FVDP_HandleContext_t* Context_p);
extern FVDP_Result_t FBM_IN_Update (FVDP_HandleContext_t* Context_p);
extern void          FBM_IF_CreateBufferTable(const FVDP_FrameInfo_t* FrameInfo_p,
                                              BufferTable_t*          Table_p,
                                              uint8_t*                Rows_p);
extern FVDP_Result_t FBM_IF_SendBufferTable (SendBufferTableCommand_t* Command_p);
extern FVDP_Result_t FBM_IF_SendDataPath (FVDP_CH_t CH,
                                          DataPathComponent_t Component,
                                          uint8_t DataPath);
extern FVDP_Result_t FBM_OUT_Init   (FVDP_HandleContext_t* Context_p);
extern FVDP_Result_t FBM_OUT_Term   (FVDP_HandleContext_t* Context_p);
extern FVDP_Result_t FBM_OUT_Update (FVDP_HandleContext_t* Context_p);
extern FVDP_Result_t FBM_FreeFrames (FVDP_HandleContext_t* HandleContext_p, uint8_t *pIdx,
                                     uint8_t MaxFramesToFree);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_FBM_H */

