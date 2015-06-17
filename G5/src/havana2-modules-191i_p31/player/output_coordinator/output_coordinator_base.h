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

Source file name : output_coordinator_base.h
Author :           Nick

Basic instance of the output coordinator class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Mar-07   Created                                         Nick

************************************************************************/

#ifndef H_OUTPUT_COORDINATOR_BASE
#define H_OUTPUT_COORDINATOR_BASE

#include "player.h"
#include "least_squares.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

typedef enum
{
    OutputRateAdjustmentNotDetermined,
    OutputRateAdjustmentInputFollowing,
    OutputRateAdjustmentOutputDriven
} OutputRateAdjustmentType_t;

//

typedef struct OutputRateAdjustmentParameters_s
{
    unsigned int			  ClockDriftMinimumIntegrationFrames;
    unsigned int			  ClockDriftMaximumIntegrationFrames;
    unsigned int			  ClockDriftIgnoreBetweenIntegrations;
    unsigned int			  IntegrationThresholdForDriftCorrection;
    long long				  MaximumJitterDifference;
} OutputRateAdjustmentParameters_t;

//

struct OutputCoordinatorContext_s
{
    OutputCoordinatorContext_t	Next;

    PlayerStream_t			  Stream;
    PlayerStreamType_t			  StreamType;

    Manifestor_t			  Manifestor;
    unsigned long long	 		  ManifestorLatency;
    VideoOutputSurfaceDescriptor_t	 *VideoSurfaceDescriptor;

    bool				  InStartupDelay;
    bool				  InSynchronizeFn;
    unsigned long long			  SynchronizingAtPlaybackTime;

    OS_Event_t				  AbortPerformEntryIntoDecodeWindowWait;

    bool				  TimeMappingEstablished;
    bool				  BaseSystemTimeAdjusted;
    unsigned int			  BasedOnMasterMappingVersion;
    unsigned long long 			  BaseSystemTime;
    unsigned long long 			  BaseNormalizedPlaybackTime;

    long long				  AccumulatedPlaybackTimeJumpsSinceSynchronization;

    bool				  ClockMaster;
    bool				  ClockAdjustmentEstablished;
    Rational_t				  ClockAdjustment;

    OutputRateAdjustmentType_t		  OutputRateAdjustmentType;
    OutputRateAdjustmentParameters_t	  OutputRateAdjustmentParameters;

    bool				  AllowedToAdjustMappingBase;			// Specific flag, use to allow only one stream to adjust the mapping base,
											// in practice the first video stream in a playback.

    bool				  LastIntegrationWasRestarted;
    bool				  IntegratingClockDrift;
    unsigned int			  FramesToIntegrateOver;
    unsigned int			  IntegrationCount;
    LeastSquares_t			  LeastSquareFit;

    long long				  StreamOffset;

    long long				  CurrentErrorHistory[4];
};

// ---------------------------------------------------------------------
//
// Class definition
//

class OutputCoordinator_Base_c : public OutputCoordinator_c
{
private:

    // Data

    OS_Mutex_t			  Lock;
    OS_Event_t			  SynchronizeMayHaveCompleted;

    unsigned int		  StreamCount;
    OutputCoordinatorContext_t	  CoordinatedContexts;
    unsigned int		  StreamsInSynchronize;

    bool			  MasterTimeMappingEstablished;
    unsigned int		  MasterTimeMappingVersion;
    unsigned long long 		  MasterBaseSystemTime;
    unsigned long long 		  MasterBaseNormalizedPlaybackTime;

    bool			  GotAMasterClock;
    bool			  GotAnAlternateMasterClock;
    OutputCoordinatorContext_t	  AlternateMasterContext;

    bool			  SystemClockAdjustmentEstablished;
    Rational_t			  SystemClockAdjustment;

    bool			  GotAVideoStream;

    long long			  AccumulatedPlaybackTimeJumpsSinceSynchronization;
    unsigned long long		  JumpSeenAtPlaybackTime;

    Rational_t			  Speed;
    PlayDirection_t		  Direction;

    unsigned long long		  VideoFrameDuration;

    long long			  MinimumStreamOffset;

    unsigned int		  VsyncOffsetIntegrationCount;
    long long			  MinimumVsyncOffset;

    bool			  ClockRecoveryInitialized;
    PlayerTimeFormat_t		  ClockRecoverySourceTimeFormat;
    unsigned long long 		  ClockRecoveryLastPts;
    unsigned long long 		  ClockRecoveryPtsBaseLine;
    unsigned long long 		  ClockRecoveryBaseSourceClock;
    unsigned long long 		  ClockRecoveryBaseLocalClock;
    unsigned int		  ClockRecoveryAccumulatedPoints;
    unsigned long long		  ClockRecoveryIntegrationTime;
    LeastSquares_t		  ClockRecoveryLeastSquareFit;

    Rational_t			  ClockRecoveryEstablishedGradient;
    unsigned long long		  ClockRecoveryEstablishedBaseSource;
    unsigned long long		  ClockRecoveryEstablishedBaseLocal;

    // Functions

    unsigned long long	 RestartTime(			void );
    unsigned long long   SpeedScale(			unsigned long long 	T );
    unsigned long long   InverseSpeedScale(		unsigned long long 	T );

public:

    //
    // Constructor/Destructor methods
    //

    OutputCoordinator_Base_c(	void );
    ~OutputCoordinator_Base_c(	void );

    //
    // Overrides for component base class functions
    //

    OutputCoordinatorStatus_t   Halt(			void );
    OutputCoordinatorStatus_t   Reset(			void );

    //
    // OutputTimer class functions
    //

    OutputCoordinatorStatus_t   RegisterStream(		PlayerStream_t			  Stream,
							PlayerStreamType_t		  StreamType,
							OutputCoordinatorContext_t	 *Context );

    OutputCoordinatorStatus_t   DeRegisterStream(	OutputCoordinatorContext_t	  Context );

    OutputCoordinatorStatus_t   SetPlaybackSpeed(	OutputCoordinatorContext_t	  Context,
							Rational_t			  Speed,
							PlayDirection_t			  Direction );

    OutputCoordinatorStatus_t   ResetTimeMapping(	OutputCoordinatorContext_t	  Context );

    OutputCoordinatorStatus_t   EstablishTimeMapping(	OutputCoordinatorContext_t	  Context,
							unsigned long long		  NormalizedPlaybackTime,
							unsigned long long		  SystemTime = INVALID_TIME );

    OutputCoordinatorStatus_t   TranslatePlaybackTimeToSystem(
							OutputCoordinatorContext_t	  Context,
							unsigned long long		  NormalizedPlaybackTime,
							unsigned long long		 *SystemTime );

    OutputCoordinatorStatus_t   TranslateSystemTimeToPlayback(
							OutputCoordinatorContext_t	  Context,
							unsigned long long		  SystemTime,
							unsigned long long		 *NormalizedPlaybackTime );

    OutputCoordinatorStatus_t   SynchronizeStreams(	OutputCoordinatorContext_t	  Context,
							unsigned long long		  NormalizedPlaybackTime,
							unsigned long long		  NormalizedDecodeTime,
							unsigned long long		 *SystemTime );

    OutputCoordinatorStatus_t   PerformEntryIntoDecodeWindowWait(
							OutputCoordinatorContext_t	  Context,
							unsigned long long		  NormalizedDecodeTime,
							unsigned long long		  DecodeWindowPorch,
							unsigned long long		  MaximumAllowedSleepTime );

    OutputCoordinatorStatus_t   HandlePlaybackTimeDeltas(
							OutputCoordinatorContext_t	  Context,
							bool				  KnownJump,
							unsigned long long		  ExpectedPlaybackTime,
							unsigned long long		  ActualPlaybackTime );

    OutputCoordinatorStatus_t   CalculateOutputRateAdjustment(
							OutputCoordinatorContext_t	  Context,
							unsigned long long		  ExpectedDuration,
							unsigned long long		  ActualDuration,
							long long			  CurrentError,
							Rational_t			 *OutputRateAdjustment,
							Rational_t			 *SystemClockAdjustment );

    OutputCoordinatorStatus_t   RestartOutputRateIntegration( OutputCoordinatorContext_t  Context );

    OutputCoordinatorStatus_t   AdjustMappingBase( 	OutputCoordinatorContext_t	  Context,
							long long			  Adjustment );

    OutputCoordinatorStatus_t   MappingBaseAdjustmentApplied( OutputCoordinatorContext_t  Context );

    OutputCoordinatorStatus_t   GetStreamStartDelay(	OutputCoordinatorContext_t	  Context,
							unsigned long long		 *Delay );

    OutputCoordinatorStatus_t   MonitorVsyncOffset( 	OutputCoordinatorContext_t	  Context, 
							unsigned long long		  RequestedOutputTime, 
							unsigned long long 		  ActualOutputTime );

    OutputCoordinatorStatus_t   ClockRecoveryInitialize(PlayerTimeFormat_t		  SourceTimeFormat );

    OutputCoordinatorStatus_t   ClockRecoveryDataPoint(	unsigned long long		  SourceTime,
							unsigned long long		  LocalTime );

    OutputCoordinatorStatus_t   ClockRecoveryEstimate(	unsigned long long		 *SourceTime,
							unsigned long long		 *LocalTime );

};
#endif

