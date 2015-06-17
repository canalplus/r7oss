/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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
 * V4L2 dvp output device driver for ST SoC display subsystems header file
************************************************************************/
#ifndef STMDVPVIDEO_H_
#define STMDVPVIDEO_H_

// Include header from the standards directory that
// defines information encoded in an injected packet
#include "dvp.h"

#include "dvb_avr.h"

/******************************
 * DEFINES
 ******************************/
#define GAM_DVP_CTL    (0x00/4)
#define GAM_DVP_TFO    (0x04/4)
#define GAM_DVP_TFS    (0x08/4)
#define GAM_DVP_BFO    (0x0C/4)
#define GAM_DVP_BFS    (0x10/4)
#define GAM_DVP_VTP    (0x14/4)
#define GAM_DVP_VBP    (0x18/4)
#define GAM_DVP_VMP    (0x1C/4)
#define GAM_DVP_CVS    (0x20/4)
#define GAM_DVP_VSD    (0x24/4)
#define GAM_DVP_HSD    (0x28/4)
#define GAM_DVP_HLL    (0x2C/4)
#define GAM_DVP_HSRC   (0x30/4)
#define GAM_DVP_HFC(x) ((0x34/4)+x)
#define GAM_DVP_VSRC   (0x60/4)
#define GAM_DVP_VFC(x) ((0x64/4)+x)
#define GAM_DVP_ITM    (0x98/4)
#define GAM_DVP_ITS    (0x9C/4)
#define GAM_DVP_STA    (0xA0/4)
#define GAM_DVP_LNSTA  (0xA4/4)
#define GAM_DVP_HLFLN  (0xA8/4)
#define GAM_DVP_ABA    (0xAC/4)
#define GAM_DVP_AEA    (0xB0/4)
#define GAM_DVP_APS    (0xB4/4)
#define GAM_DVP_PKZ    (0xFC/4)

/******************************
 * Useful defines
 ******************************/

#define INVALID_TIME				0xfeedfacedeadbeefULL
#define DVP_MAXIMUM_FRAME_JITTER		16				// us

#define DVP_MAX_MISSED_FRAMES_BEFORE_POST_MORTEM 4
#define DVP_MAX_VIDEO_DECODE_BUFFER_COUNT	64
#define	DVP_VIDEO_DECODE_BUFFER_STACK_SIZE	(DVP_MAX_VIDEO_DECODE_BUFFER_COUNT+1)
#define DVP_MAX_SUPPORTED_PPM_CLOCK_ERROR	200
#define DVP_WARM_UP_TRIES			32
#define DVP_WARM_UP_VIDEO_FRAMES_30		2
#define DVP_LEAD_IN_VIDEO_FRAMES_30		(2 + DVP_WARM_UP_VIDEO_FRAMES_30)
#define DVP_WARM_UP_VIDEO_FRAMES_60		2
#define DVP_LEAD_IN_VIDEO_FRAMES_60		(4 + DVP_WARM_UP_VIDEO_FRAMES_60)

#define DVP_MAXIMUM_PLAYER_TRANSIT_TIME		150000				// raised to 150ms, it appears that under heavy use, it can take longer for a buffer to make the transition, in part because we have to wake the relatively low priority buffer injection process to initiate the injection
#define DVP_MINIMUM_TIME_INTEGRATION_FRAMES	32
#define DVP_MAXIMUM_TIME_INTEGRATION_FRAMES	1024				// This is the period between corrections for drift, we accumulate until we use the maximum data widths
#define DVP_MAX_RECORDED_INTEGRATIONS		256				// This is the maximum number of integration frames that can be used to calculate the clock correction factor.
#define DVP_CORRECTION_FIXED_POINT_BITS		30
#define DVP_CORRECTION_FACTOR_ONE		(1 << DVP_CORRECTION_FIXED_POINT_BITS)
#define DVP_ONE_PPM				(DVP_CORRECTION_FACTOR_ONE / 1000000)
#define DVP_VIDEO_THREAD_PRIORITY		40
#define DVP_SYNCHRONIZER_THREAD_PRIORITY	99				// Runs at highest possible priority

#define DVP_DEFAULT_ANCILLARY_INPUT_BUFFER_SIZE	(16 * 1024)			// Defaults for the ancillary input buffers, modifyable by controls
#define DVP_DEFAULT_ANCILLARY_PAGE_SIZE		256

#define DVP_ANCILLARY_PARTITION			"BPA2_Region0"
#define DVP_MIN_ANCILLARY_BUFFERS		4
#define DVP_MAX_ANCILLARY_BUFFERS		32
#define DVP_ANCILLARY_BUFFER_CHUNK_SIZE		16				// it deals with 128 bit words
#define DVP_MIN_ANCILLARY_BUFFER_SIZE		(2 * DVP_ANCILLARY_BUFFER_CHUNK_SIZE)
#define DVP_MAX_ANCILLARY_BUFFER_SIZE		4096				// Hardware restriction

#define DVP_SRC_OFF_VALUE			0x00000000

#define DVP_PIXEL_ASPECT_RATIO_CORRECTION_MIN_VALUE	0
#define DVP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE	100

//#define PIX_AR_NULL {0,0}
//#define PIX_AR_SQUARE {1,1}

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
} dvp_v4l2_video_mode_params_t;

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
} dvp_v4l2_video_timing_params_t;

/* 
 * Capture Interface version of stm_mode_line_t in the stmfb driver  
 */
typedef struct
{
    dvp_v4l2_video_mode_t		Mode;
    dvp_v4l2_video_mode_params_t 	ModeParams;
    dvp_v4l2_video_timing_params_t 	TimingParams;
} dvp_v4l2_video_mode_line_t;

typedef enum
{
    DvpInactive		= 0,
    DvpStarting,				// Enterred by user level, awaiting first interrupt
    DvpWarmingUp,				// Enterred by Interrupt, Got first interrupt, collecting data to establish timing baseline
    DvpStarted,					// Enterred by Interrupt, baseline established
    DvpMovingToRun,				// Enterred by user level, awaiting first collection
    DvpRunning,					// Enterred by Interrupt, running freely
    DvpMovingToInactive				// Enterred by user level, shutting down
} DvpState_t;

//

typedef struct DvpBufferStack_s
{
    buffer_handle_t		 Buffer;
    unsigned char		*Data;
    unsigned long long		 ExpectedFillTime;
    unsigned int		 Width;					// Recorded buffer width/height
    unsigned int		 Height;
    unsigned int		 RegisterCVS;				// Calculated captured window size register
    unsigned int		 RegisterVMP;				// Pitch (in a field)
    unsigned int		 RegisterVBPminusVTP;			// Offset borrom field from top field in memory
    unsigned int		 RegisterHSRC;
    unsigned int		 RegisterVSRC;
    DvpRectangle_t		 InputWindow;
    DvpRectangle_t		 OutputWindow;
} DvpBufferStack_t;

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

typedef struct dvp_v4l2_video_handle_s
{
    avr_v4l2_shared_handle_t	*SharedContext;
    struct DeviceContext_s	*DeviceContext;
    volatile int 		*DvpRegs;
    unsigned int 		 DvpIrq;
    unsigned long long		 DvpLatency;
    unsigned long long		 AppliedLatency;

    stm_display_mode_t		 inputmode;
    unsigned int		 BytesPerLine;				// Obtained when we get a buffer
    StreamInfo_t		 StreamInfo;				// Derived values supplied to player

    unsigned int		 RegisterTFO;				// Calculated top field offset register
    unsigned int		 RegisterTFS;				// Calculated top field stop register
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
    unsigned int		 NextRegisterCVS;
    unsigned int		 NextRegisterVMP;
    unsigned int		 NextRegisterVBPminusVTP;
    unsigned int		 NextRegisterHSRC;
    unsigned int		 NextRegisterVSRC;
    unsigned int		 LastRegisterHSRC;			// These are buffer specific, remembered so as not to reload the filter constants
    unsigned int		 LastRegisterVSRC;

    unsigned int		 DvpWarmUpVideoFrames;			// These vary with input mode to allow similar times
    unsigned int		 DvpLeadInVideoFrames;

    struct semaphore 		 DvpVideoInterruptSem;
    struct semaphore 		 DvpSynchronizerWakeSem;
    struct semaphore 		 DvpAncillaryBufferDoneSem;
    struct semaphore 		 DvpPreInjectBufferSem;
    struct semaphore 		 DvpScalingStateLock;

    bool			 VideoRunning;				// Indicates video process is running
    bool			 FastModeSwitch;			// Indicates that a fast mode switch is in progress

    bool			 SynchronizerRunning;			// Indicates synchronizer process is running
    bool			 SynchronizeEnabled;			// Indicates to synchronizer the state of the user control allowing vsync locking
    bool			 Synchronize;				// Indicates to synchronizer to synchronize

    DvpState_t			 DvpState;

    const dvp_v4l2_video_mode_line_t	*DvpCaptureMode;
    unsigned int		 ModeWidth;
    unsigned int		 ModeHeight;

    unsigned int		 DvpBuffersRequiredToInjectAhead;
    unsigned int		 DvpNextBufferToGet;
    unsigned int		 DvpNextBufferToInject;
    unsigned int		 DvpNextBufferToFill;
    int				 DvpFrameCount;

    unsigned int	         BufferBytesPerPixel;

    unsigned int		 DvpMissedFramesInARow;

    // SYSfs variables, those signalled from interrupt also need an associated boolean to inform the synchronizer that this one needs notifying
    struct class_device		*DvpSysfsClassDevice;
    bool			 DvpMicroSecondsPerFrameNotify; 
    atomic_t			 DvpMicroSecondsPerFrame;
    bool			 DvpFrameCaptureNotificationNotify; 
    atomic_t			 DvpFrameCaptureNotification;
    bool			 DvpFrameCountingNotificationNotify;
    atomic_t                     DvpFrameCountingNotification;
    atomic_t                     DvpOutputCropTargetReachedNotification;
    bool			 DvpPostMortemNotify;
    atomic_t                     DvpPostMortem;
 
    DvpBufferStack_t		 DvpBufferStack[DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    unsigned int		 DvpIntegrateForAtLeastNFrames;
    unsigned int		 DvpInterruptFrameCount;
    unsigned int		 DvpwarmUpSynchronizationAttempts;
    unsigned long long		 DvpTimeAtZeroInterruptFrameCount;
    unsigned long long		 DvpTimeOfLastFrameInterrupt;
    bool			 StandardFrameRate;

    bool			 DvpCalculatingFrameTime;			// Boolean set by process, to tell interrupt to avoid manipulating base times
    long long 			 DvpLastDriftCorrection;
    long long			 DvpLastFrameDriftError;
    long long			 DvpCurrentDriftError;
    unsigned int		 DvpDriftFrameCount;

    unsigned long long		 DvpBaseTime;
    unsigned long long		 DvpRunFromTime;
    unsigned long long		 DvpFrameDurationCorrection;			// Fixed point 2^DVP_CORRECTION_FIXED_POINT_BITS is one.
    unsigned long long		 DvpTotalElapsedCaptureTime;
    unsigned int		 DvpTotalCapturedFrameCount;
    unsigned int		 DvpNextIntegrationRecord;
    unsigned int		 DvpFirstIntegrationRecord;
    unsigned long long		 DvpElapsedCaptureTimeRecord[DVP_MAX_RECORDED_INTEGRATIONS];
    unsigned int		 DvpCapturedFrameCountRecord[DVP_MAX_RECORDED_INTEGRATIONS];

    bool			 AncillaryStreamOn;
    bool			 AncillaryCaptureInProgress;
    unsigned char		*AncillaryInputBufferUnCachedAddress;		// The actual hardware buffer
    unsigned char		*AncillaryInputBufferPhysicalAddress;
    unsigned char		*AncillaryInputBufferInputPointer;
    unsigned int		 AncillaryInputBufferSize;
    unsigned int		 AncillaryPageSizeSpecified;
    unsigned int		 AncillaryPageSize;

    unsigned int		 AncillaryBufferCount;				// Ancillary buffer information
    unsigned int		 AncillaryBufferSize;
    unsigned char		*AncillaryBufferUnCachedAddress;		// Base pointers rather than individual buffers
    unsigned char		*AncillaryBufferPhysicalAddress;
    unsigned int		 AncillaryBufferNextQueueIndex;
    unsigned int		 AncillaryBufferNextFillIndex;
    unsigned int		 AncillaryBufferNextDeQueueIndex;
    unsigned int		 AncillaryBufferQueue[DVP_MAX_ANCILLARY_BUFFERS];
    AncillaryBufferState_t	 AncillaryBufferState[DVP_MAX_ANCILLARY_BUFFERS];

    // The current values of all the controls

    int			 DvpControl16Bit;
    int			 DvpControlBigEndian;
    int			 DvpControlFullRange;
    int			 DvpControlIncompleteFirstPixel;
    int			 DvpControlOddPixelCount;
    int			 DvpControlVsyncBottomHalfLineEnable;
    int			 DvpControlExternalSync;
    int			 DvpControlExternalSyncPolarity;
    int			 DvpControlExternalSynchroOutOfPhase;
    int			 DvpControlExternalVRefOddEven;
    int			 DvpControlExternalVRefPolarityPositive;
    int			 DvpControlHRefPolarityPositive;
    int			 DvpControlActiveAreaAdjustHorizontalOffset;
    int			 DvpControlActiveAreaAdjustVerticalOffset;
    int			 DvpControlActiveAreaAdjustWidth;
    int			 DvpControlActiveAreaAdjustHeight;
    int			 DvpControlColourMode;				// 0 => Default, 1 => 601, 2 => 709
    int			 DvpControlVideoLatency;
    int			 DvpControlBlank;
    int			 DvpControlAncPageSizeSpecified;
    int			 DvpControlAncPageSize;
    int			 DvpControlAncInputBufferSize;
    int			 DvpControlHorizontalResizeEnable;
    int			 DvpControlVerticalResizeEnable;
    int			 DvpControlTopFieldFirst;
    int			 DvpControlVsyncLockEnable;
    int			 DvpControlOutputCropTransitionMode;
    int			 DvpControlOutputCropTransitionModeParameter0;

    int 		 DvpControlPixelAspectRatioCorrection;
    int 		 DvpControlPictureAspectRatio;

    int			 DvpControlDefault16Bit;
    int			 DvpControlDefaultBigEndian;
    int			 DvpControlDefaultFullRange;
    int			 DvpControlDefaultIncompleteFirstPixel;
    int			 DvpControlDefaultOddPixelCount;
    int			 DvpControlDefaultVsyncBottomHalfLineEnable;
    int			 DvpControlDefaultExternalSync;
    int			 DvpControlDefaultExternalSyncPolarity;
    int			 DvpControlDefaultExternalSynchroOutOfPhase;
    int			 DvpControlDefaultExternalVRefOddEven;
    int			 DvpControlDefaultExternalVRefPolarityPositive;
    int			 DvpControlDefaultHRefPolarityPositive;
    int			 DvpControlDefaultActiveAreaAdjustHorizontalOffset;
    int			 DvpControlDefaultActiveAreaAdjustVerticalOffset;
    int			 DvpControlDefaultActiveAreaAdjustWidth;
    int			 DvpControlDefaultActiveAreaAdjustHeight;
    int			 DvpControlDefaultColourMode;
    int			 DvpControlDefaultVideoLatency;
    int			 DvpControlDefaultBlank;
    int			 DvpControlDefaultAncPageSizeSpecified;
    int			 DvpControlDefaultAncPageSize;
    int			 DvpControlDefaultAncInputBufferSize;
    int			 DvpControlDefaultHorizontalResizeEnable;
    int			 DvpControlDefaultVerticalResizeEnable;
    int			 DvpControlDefaultTopFieldFirst;
    int			 DvpControlDefaultVsyncLockEnable;
    int			 DvpControlDefaultOutputCropTransitionMode;
    int			 DvpControlDefaultOutputCropTransitionModeParameter0;
    int 		 DvpControlDefaultPixelAspectRatioCorrection;
    int 		 DvpControlDefaultPictureAspectRatio;

} dvp_v4l2_video_handle_t;


/******************************
 * Function prototypes of
 * functions exported by the
 * video code.
 ******************************/

#if defined (CONFIG_KERNELVERSION)  // STLinux 2.3
int DvpInterrupt(int irq, void* data);
#else
int DvpInterrupt(int irq, void* data, struct pt_regs* pRegs);
#endif

int DvpVideoClose( 			dvp_v4l2_video_handle_t	*Context );

int DvpVideoThreadHandle( 		void 			*Data );

int DvpVideoIoctlSetFramebuffer(	dvp_v4l2_video_handle_t	*Context,
					unsigned int		 Width,
					unsigned int		 Height,
					unsigned int		 BytesPerLine,
					unsigned int		 Control );

int DvpVideoIoctlSetStandard(           dvp_v4l2_video_handle_t	*Context,
					v4l2_std_id		 Id );

int DvpVideoIoctlOverlayStart(  	dvp_v4l2_video_handle_t	*Context );

int DvpVideoIoctlCrop(			dvp_v4l2_video_handle_t	*Context,
					struct v4l2_crop	*Crop );

int DvpVideoIoctlSetControl( 		dvp_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		 Value );

int DvpVideoIoctlGetControl( 		dvp_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		*Value );

// ////////////////////////////

int DvpVideoIoctlAncillaryRequestBuffers(	dvp_v4l2_video_handle_t	*Context,
						unsigned int		 DesiredCount,
						unsigned int		 DesiredSize,
						unsigned int		*ActualCount,
						unsigned int		*ActualSize );

int DvpVideoIoctlAncillaryQueryBuffer(		dvp_v4l2_video_handle_t	 *Context,
						unsigned int		  Index,
						bool			 *Queued,
						bool			 *Done,
						unsigned char		**PhysicalAddress,
						unsigned char		**UnCachedAddress,
						unsigned long long	 *CaptureTime,
						unsigned int		 *Bytes,
						unsigned int		 *Size );

int DvpVideoIoctlAncillaryQueueBuffer(		dvp_v4l2_video_handle_t	*Context,
						unsigned int		 Index );

int DvpVideoIoctlAncillaryDeQueueBuffer(	dvp_v4l2_video_handle_t	*Context,
						unsigned int		*Index,
						bool			 Blocking );

int DvpVideoIoctlAncillaryStreamOn(		dvp_v4l2_video_handle_t	*Context );
int DvpVideoIoctlAncillaryStreamOff(		dvp_v4l2_video_handle_t	*Context );


#endif /*STMDVPVIDEO_H_*/

