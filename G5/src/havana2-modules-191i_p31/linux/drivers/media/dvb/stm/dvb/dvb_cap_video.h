/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.
 * V4L2 cap device driver for ST SoC display subsystems header file
************************************************************************/
#ifndef STMCAPVIDEO_H_
#define STMCAPVIDEO_H_

// Include header from the standards directory that
// defines information encoded in an injected packet
#include "dvp.h"

#include "dvb_cap.h"

/******************************
 * DEFINES
 ******************************/
#define GAM_CAP_CTL    (0x00/4)
#define GAM_CAP_CWO    (0x04/4)
#define GAM_CAP_CWS    (0x08/4)
#define GAM_CAP_VTP    (0x14/4)
#define GAM_CAP_VBP    (0x18/4)
#define GAM_CAP_PMP    (0x1C/4)
#define GAM_CAP_CMW    (0x20/4)
#define GAM_CAP_HSRC   (0x30/4)
#define GAM_CAP_HFC(x) ((0x34/4)+x)
#define GAM_CAP_VSRC   (0x5C/4)
#define GAM_CAP_VFC(x) ((0x60/4)+x)
#define GAM_CAP_PKZ    (0xFC/4)

/******************************
 * Useful defines
 ******************************/

#define INVALID_TIME				0xfeedfacedeadbeefULL
#define CAP_MAXIMUM_FRAME_JITTER		16				// us

#define CAP_MAX_MISSED_FRAMES_BEFORE_POST_MORTEM 4
#define CAP_MAX_VIDEO_DECODE_BUFFER_COUNT	64
#define	CAP_VIDEO_DECODE_BUFFER_STACK_SIZE	(CAP_MAX_VIDEO_DECODE_BUFFER_COUNT+1)
#define CAP_MAX_SUPPORTED_PPM_CLOCK_ERROR	200
#define CAP_WARM_UP_TRIES			32
#define CAP_WARM_UP_VIDEO_FRAMES_30		2
#define CAP_LEAD_IN_VIDEO_FRAMES_30		(2 + CAP_WARM_UP_VIDEO_FRAMES_30)
#define CAP_WARM_UP_VIDEO_FRAMES_60		2
#define CAP_LEAD_IN_VIDEO_FRAMES_60		(4 + CAP_WARM_UP_VIDEO_FRAMES_60)

#define CAP_MAXIMUM_PLAYER_TRANSIT_TIME		60000				// Guessing at 60 ms
#define CAP_MINIMUM_TIME_INTEGRATION_FRAMES	32
#define CAP_MAXIMUM_TIME_INTEGRATION_FRAMES	32768				// at 60fps must be at least 8192 to use the whole 28 bits of corrective muiltiplier
#define CAP_CORRECTION_FIXED_POINT_BITS		28
#define CAP_ONE_PPM				(1LL << (CAP_CORRECTION_FIXED_POINT_BITS - 20))
#define CAP_VIDEO_THREAD_PRIORITY		40
#define CAP_SYNCHRONIZER_THREAD_PRIORITY	99				// Runs at highest possible priority

#define CAP_DEFAULT_ANCILLARY_INPUT_BUFFER_SIZE	(16 * 1024)			// Defaults for the ancillary input buffers, modifyable by controls
#define CAP_DEFAULT_ANCILLARY_PAGE_SIZE		256

#define CAP_ANCILLARY_PARTITION			"BPA2_Region0"
#define CAP_MIN_ANCILLARY_BUFFERS		4
#define CAP_MAX_ANCILLARY_BUFFERS		32
#define CAP_ANCILLARY_BUFFER_CHUNK_SIZE		16				// it deals with 128 bit words
#define CAP_MIN_ANCILLARY_BUFFER_SIZE		(2 * CAP_ANCILLARY_BUFFER_CHUNK_SIZE)
#define CAP_MAX_ANCILLARY_BUFFER_SIZE		4096				// Hardware restriction

#define CAP_SRC_OFF_VALUE			0x00000000

#define CAP_PIXEL_ASPECT_RATIO_CORRECTION_MIN_VALUE	0
#define CAP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE	100

/******************************
 * Local types
 ******************************/

/* 
 * Capture Interface version of stm_mode_params_t in the stmfb video driver
 */
typedef struct
{
    ULONG           FrameRate;
    stm_scan_type_t ScanType;
    ULONG           ActiveAreaWidth;
    ULONG           ActiveAreaHeight;
    ULONG           ActiveAreaXStart;
    ULONG           FullVBIHeight;
//  ULONG           OutputStandards;
//  BOOL            SquarePixel;
//  ULONG           HDMIVideoCodes[HDMI_CODE_COUNT];
} cap_v4l2_video_mode_params_t;

/* 
 * Capture Interface version of stm_timing_params_t in the stmfb video driver
 */
typedef struct
{
    BOOL           HSyncPolarity;
    BOOL           VSyncPolarity;
//  ULONG          PixelsPerLine;
//  ULONG          LinesByFrame;
//  ULONG          ulPixelClock;
//  ULONG          HSyncPulseWidth;
//  ULONG          VSyncPulseWidth;
} cap_v4l2_video_timing_params_t;

/* 
 * Capture Interface version of stm_mode_line_t in the stmfb driver  
 */
typedef struct
{
    cap_v4l2_video_mode_t		Mode;
    cap_v4l2_video_mode_params_t 	ModeParams;
    cap_v4l2_video_timing_params_t 	TimingParams;
} cap_v4l2_video_mode_line_t;

typedef enum
{
    CapInactive		= 0,
    CapStarting,				// Enterred by user level, awaiting first interrupt
    CapWarmingUp,				// Enterred by Interrupt, Got first interrupt, collecting data to establish timing baseline
    CapStarted,					// Enterred by Interrupt, baseline established
    CapMovingToRun,				// Enterred by user level, awaiting first collection
    CapRunning,					// Enterred by Interrupt, running freely
    CapMovingToInactive				// Enterred by user level, shutting down
} CapState_t;

//

typedef struct CapBufferStack_s
{
    buffer_handle_t		 Buffer;
    unsigned char		*Data;
    unsigned long long		 ExpectedFillTime;
    unsigned int		 RegisterCMW;				// Calculated captured window size register
    unsigned int		 RegisterVMP;				// Pitch (in a field)
    unsigned int		 RegisterVBPminusVTP;			// Offset borrom field from top field in memory
    unsigned int		 RegisterHSRC;
    unsigned int		 RegisterVSRC;
    DvpRectangle_t		 InputWindow;
    DvpRectangle_t		 OutputWindow;
} CapBufferStack_t;

//

typedef struct AncillaryBufferState_s
{
    unsigned char	*PhysicalAddress;
    unsigned char	*UnCachedAddress;
    bool		 Queued;
    bool		 Done;
    unsigned int	 Bytes;
    unsigned long long	 FillTime;

    bool		 Mapped;		// External V4L2 flag
} AncillaryBufferState_t;

//

typedef struct cap_v4l2_video_handle_s
{
    cap_v4l2_shared_handle_t	*SharedContext;
    struct DeviceContext_s	*DeviceContext;
    volatile int 		*CapRegs;
    volatile int 		*VtgRegs;
    unsigned int 		 CapIrq;
    unsigned int 		 CapIrq2;
    unsigned long long		 CapLatency;
    unsigned long long		 AppliedLatency;

    stm_display_mode_t		 inputmode;
    unsigned int		 BytesPerLine;				// Obtained when we get a buffer
    StreamInfo_t		 StreamInfo;				// Derived values supplied to player

    unsigned int		 RegisterCWO;				// Calculated top field offset register
    unsigned int		 RegisterCWS;				// Calculated top field stop register
    unsigned int		 RegisterBFO;
    unsigned int		 RegisterBFS;
    unsigned int		 RegisterHLL;				// Half line length
    unsigned int		 RegisterCTL;				// Calculated control register value

    DvpRectangle_t		 InputCrop;				// Cropping data
    DvpRectangle_t		 ScaledInputCrop;
    DvpRectangle_t		 OutputCropStart;
    DvpRectangle_t		 OutputCropTarget;
    DvpRectangle_t		 OutputCrop;
    bool			 OutputCropStepping;			// Smooth change of output scaling parameters
    bool			 OutputCropTargetReached;
    unsigned int		 OutputCropSteps;
    unsigned int		 OutputCropCurrentStep;

    unsigned int		 NextWidth;				// These are buffer specific, these will be used with future buffers
    unsigned int		 NextHeight;
    DvpRectangle_t		 NextInputWindow;
    DvpRectangle_t		 NextOutputWindow;
    unsigned int		 NextRegisterCMW;
    unsigned int		 NextRegisterVMP;
    unsigned int		 NextRegisterVBPminusVTP;
    unsigned int		 NextRegisterHSRC;
    unsigned int		 NextRegisterVSRC;
    unsigned int		 LastRegisterHSRC;			// These are buffer specific, remembered so as not to reload the filter constants
    unsigned int		 LastRegisterVSRC;

    unsigned int		 CapWarmUpVideoFrames;			// These vary with input mode to allow similar times
    unsigned int		 CapLeadInVideoFrames;

    struct semaphore 		 CapVideoInterruptSem;
    struct semaphore 		 CapSynchronizerWakeSem;
    struct semaphore 		 CapAncillaryBufferDoneSem;
    struct semaphore 		 CapPreInjectBufferSem;
    struct semaphore 		 CapScalingStateLock;

    bool			 VideoRunning;				// Indicates video process is running
    bool			 FastModeSwitch;			// Indicates that a fast mode switch is in progress

    bool			 SynchronizerRunning;			// Indicates synchronizer process is running
    bool			 SynchronizeEnabled;			// Indicates to synchronizer the state of the user control allowing vsync locking
    bool			 Synchronize;				// Indicates to synchronizer to synchronize

    CapState_t			 CapState;

    const stm_mode_line_t	*CapCaptureMode;
    unsigned int		 ModeWidth;
    unsigned int		 ModeHeight;

    unsigned int                 BufferBytesPerPixel;
    unsigned int		 CapMissedFramesInARow;
   
    unsigned int		 CapBuffersRequiredToInjectAhead;
    unsigned int		 CapNextBufferToGet;
    unsigned int		 CapNextBufferToInject;
    unsigned int		 CapNextBufferToFill;
    int				 CapFrameCount;

// SYSfs variables, those signalled from interrupt also need an associated boolean to inform the synchronizer that this one needs notifying
    struct class_device		*CapSysfsClassDevice;
    bool			 CapFrameCaptureNotificationNotify; 
    atomic_t			 CapFrameCaptureNotification;
    bool			 CapFrameCountingNotificationNotify;
    atomic_t                     CapFrameCountingNotification;
    atomic_t                     CapOutputCropTargetReachedNotification;
    bool			 CapPostMortemNotify;
    atomic_t                     CapPostMortem;

    CapBufferStack_t		 CapBufferStack[CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    unsigned int		 CapIntegrateForAtLeastNFrames;
    unsigned int		 CapInterruptFrameCount;
    unsigned int		 CapwarmUpSynchronizationAttempts;
    unsigned long long		 CapTimeAtZeroInterruptFrameCount;
    unsigned long long		 CapTimeOfLastFrameInterrupt;
    bool			 StandardFrameRate;

    bool			 CapCalculatingFrameTime;			// Boolean set by process, to tell interrupt to avoid manipulating base times
    long long 			 CapLastDriftCorrection;
    long long			 CapLastFrameDriftError;
    long long			 CapCurrentDriftError;
    unsigned int		 CapDriftFrameCount;

    unsigned long long		 CapBaseTime;
    unsigned long long		 CapRunFromTime;
    unsigned long long		 CapFrameDurationCorrection;			// Fixed point 2^CAP_CORRECTION_FIXED_POINT_BITS is one.

    // The current values of all the controls

    int                  CapControlCSignOut;
    int                  CapControlCSignIn;
    int                  CapControlBF709Not601;
    int                  CapControlYCbCr2RGB;
    int                  CapControlBigNotLittle;
    int                  CapControlCapFormat;
    int                  CapControlEnHsRc;
    int                  CapControlEnVsRc;
    int                  CapControlSSCap;
    int                  CapControlTFCap;
    int                  CapControlBFCap;
    int                  CapControlVTGSel;
    int                  CapControlSource;   
   
    int			 CapControlBigEndian;
    int			 CapControlFullRange;
    int			 CapControlIncompleteFirstPixel;
    int			 CapControlOddPixelCount;
    int			 CapControlVsyncBottomHalfLineEnable;
    int			 CapControlExternalSync;
    int			 CapControlExternalSyncPolarity;
    int			 CapControlExternalSynchroOutOfPhase;
    int			 CapControlExternalVRefOddEven;
    int			 CapControlExternalVRefPolarityPositive;
    int			 CapControlHRefPolarityPositive;
    int			 CapControlActiveAreaAdjustHorizontalOffset;
    int			 CapControlActiveAreaAdjustVerticalOffset;
    int			 CapControlActiveAreaAdjustWidth;
    int			 CapControlActiveAreaAdjustHeight;
    int			 CapControlColourMode;				// 0 => Default, 1 => 601, 2 => 709
    int			 CapControlVideoLatency;
    int			 CapControlBlank;
    int			 CapControlTopFieldFirst;
    int			 CapControlVsyncLockEnable;
    int			 CapControlOutputCropTransitionMode;
    int			 CapControlOutputCropTransitionModeParameter0;
    int 		 CapControlPixelAspectRatioCorrection;
	int 		 CapControlPictureAspectRatio;
   
   
    int                  CapControlDefaultCSignOut;
    int                  CapControlDefaultCSignIn;
    int                  CapControlDefaultBF709Not601;
    int                  CapControlDefaultYCbCr2RGB;
    int                  CapControlDefaultBigNotLittle;
    int                  CapControlDefaultCapFormat;
    int                  CapControlDefaultEnHsRc;
    int                  CapControlDefaultEnVsRc;
    int                  CapControlDefaultSSCap;
    int                  CapControlDefaultTFCap;
    int                  CapControlDefaultBFCap;
    int                  CapControlDefaultVTGSel;
    int                  CapControlDefaultSource;      
   
    int			 CapControlDefaultBigEndian;
    int			 CapControlDefaultFullRange;
    int			 CapControlDefaultIncompleteFirstPixel;
    int			 CapControlDefaultOddPixelCount;
    int			 CapControlDefaultVsyncBottomHalfLineEnable;
    int			 CapControlDefaultExternalSync;
    int			 CapControlDefaultExternalSyncPolarity;
    int			 CapControlDefaultExternalSynchroOutOfPhase;
    int			 CapControlDefaultExternalVRefOddEven;
    int			 CapControlDefaultExternalVRefPolarityPositive;
    int			 CapControlDefaultHRefPolarityPositive;
    int			 CapControlDefaultActiveAreaAdjustHorizontalOffset;
    int			 CapControlDefaultActiveAreaAdjustVerticalOffset;
    int			 CapControlDefaultActiveAreaAdjustWidth;
    int			 CapControlDefaultActiveAreaAdjustHeight;
    int			 CapControlDefaultColourMode;
    int			 CapControlDefaultVideoLatency;
    int			 CapControlDefaultBlank;
    int			 CapControlDefaultHorizontalResizeEnable;
    int			 CapControlDefaultVerticalResizeEnable;
    int			 CapControlDefaultTopFieldFirst;
    int			 CapControlDefaultVsyncLockEnable;
    int			 CapControlDefaultOutputCropTransitionMode;
    int			 CapControlDefaultOutputCropTransitionModeParameter0;
    int 		 CapControlDefaultPixelAspectRatioCorrection;

} cap_v4l2_video_handle_t;


/******************************
 * Function prototypes of
 * functions exported by the
 * video code.
 ******************************/

void CapInterrupt(void* data, stm_field_t vsync);

int CapVideoClose( 			cap_v4l2_video_handle_t	*Context );

int CapVideoThreadHandle( 		void 			*Data );

int CapVideoIoctlSetFramebuffer(	cap_v4l2_video_handle_t	*Context,
					unsigned int		 Width,
					unsigned int		 Height,
					unsigned int		 BytesPerLine,
					unsigned int		 Control );

int CapVideoIoctlSetStandard(           cap_v4l2_video_handle_t	*Context,
					v4l2_std_id		 Id );

int CapVideoIoctlOverlayStart(  	cap_v4l2_video_handle_t	*Context );

int CapVideoIoctlCrop(			cap_v4l2_video_handle_t	*Context,
					struct v4l2_crop	*Crop );

int CapVideoIoctlSetControl( 		cap_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		 Value );

int CapVideoIoctlGetControl( 		cap_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		*Value );

#endif /*STMCAPVIDEO_H_*/

