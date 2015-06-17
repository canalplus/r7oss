#ifndef _MOCK_PLAYER_H
#define _MOCK_PLAYER_H

#include "gmock/gmock.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "player.h"

class MockPlayer_c: public Player_c {
  public:
    MockPlayer_c();
    virtual ~MockPlayer_c();

    // Some member variables used to set policy values
    int             mPolicyLivePlaybackLatencyVariabilityMs;
    int             mPolicyMasterClock;
    int             mPolicyExternalTimeMapping;
    int             mPolicyExternalTimeMappingVsyncLocked;
    int             mPolicyRunningDevlog;
    int             mPolicyPtsForwardJumpDetectionThreshold;
    int             mPolicySymmetricJumpDetection;
    int             mPolicyLivePlayback;
    int             mPolicyVideoStartImmediate;
    int             mPolicyDiscardLateFrames;
    int             mPolicyStreamDiscardFrames;
    int             mPolicyVideoLastFrameBehaviour;
    int             mPolicyLimitInputInjectAhead;
    int             mPolicyReduceCollatedData;
    int             mPolicyControlDataDetection;
    int             mPolicyHdmiRxMode;


    MOCK_METHOD1(RegisterBufferManager,
                 PlayerStatus_t (BufferManager_t BufferManager));
    MOCK_METHOD4(SpecifySignalledEvents,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream,
                                 PlayerEventMask_t Events, void *UserData));
    MOCK_METHOD4(SetPolicy,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream,
                                 PlayerPolicy_t Policy, int PolicyValue));
    MOCK_METHOD4(CreatePlayback,
                 PlayerStatus_t (OutputCoordinator_t OutputCoordinator,
                                 PlayerPlayback_t *Playback,
                                 bool SignalEvent, void *EventUserData));
    MOCK_METHOD3(TerminatePlayback,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 bool SignalEvent, void *EventUserData));
#if 0
    MOCK_METHOD11(AddStream,
                  PlayerStatus_t (PlayerPlayback_t Playback,
                                  PlayerStreamInterface_c **Stream,
                                  PlayerStreamType_t StreamType,
                                  unsigned int              InstanceId,
                                  Collator_t Collator,
                                  FrameParser_t FrameParser, Codec_t Codec,
                                  OutputTimer_t OutputTimer,
                                  DecodeBufferManager_t DecodeBufferManager,
                                  ManifestationCoordinator_t
                                  ManifestationCoordinator,
                                  bool SignalEvent, void *EventUserData));
#endif
    virtual PlayerStatus_t   AddStream(PlayerPlayback_t          Playback,
                                       PlayerStreamInterface_c **Stream,
                                       PlayerStreamType_t        StreamType,
                                       unsigned int              InstanceId,
                                       Collator_t                Collator,
                                       FrameParser_t             FrameParser,
                                       Codec_t                   Codec,
                                       OutputTimer_t             OutputTimer,
                                       DecodeBufferManager_t     DecodeBufferManager,
                                       ManifestationCoordinator_t   ManifestationCoordinator,
                                       HavanaStream_t                 HavanaStream,
                                       UserDataSource_t               UserDataSender,
                                       bool                      SignalEvent           = false,
                                       void                     *EventUserData         = NULL)  { return PlayerNoError; }
    MOCK_METHOD3(RemoveStream,
                 PlayerStatus_t (PlayerStreamInterface_c *Stream, bool SignalEvent, void *EventUserData));
    MOCK_METHOD4(SetPresentationInterval,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream ,
                                 unsigned long long IntervalStartNativeTime,
                                 unsigned long long IntervalEndNativeTime));
    MOCK_METHOD3(SetNativePlaybackTime,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 unsigned long long NativeTime,
                                 unsigned long long SystemTime));
    MOCK_METHOD3(RetrieveNativePlaybackTime,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 unsigned long long *NativeTime,
                                 PlayerTimeFormat_t NativeTimeFormat));
    MOCK_METHOD3(TranslateNativePlaybackTime,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 unsigned long long NativeTime,
                                 unsigned long long *SystemTime));
    MOCK_METHOD2(ClockRecoveryInitialize,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerTimeFormat_t SourceTimeFormat));
    MOCK_METHOD3(ClockRecoveryDataPoint,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 unsigned long long SourceTime,
                                 unsigned long long LocalTime));
    MOCK_METHOD3(ClockRecoveryEstimate,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 unsigned long long *SourceTime,
                                 unsigned long long *LocalTime));
    MOCK_METHOD1(GetInjectBuffer, PlayerStatus_t (Buffer_t *Buffer));
    MOCK_METHOD2(InjectData,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 Buffer_t Buffer));
    MOCK_METHOD1(LowPowerEnter, PlayerStatus_t (__stm_se_low_power_mode_t   low_power_mode));
    MOCK_METHOD0(LowPowerExit, PlayerStatus_t (void));
    MOCK_METHOD0(GetLowPowerMode, __stm_se_low_power_mode_t (void));
    MOCK_METHOD1(ReleaseDecodeBufferReference,
                 PlayerStatus_t (PlayerEventRecord_t *Record));
    MOCK_METHOD4(SetModuleParameters,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream,
                                 unsigned int ParameterBlockSize,
                                 void *ParameterBlock));
    MOCK_METHOD1(GetBufferManager,
                 PlayerStatus_t (BufferManager_t *BufferManager));
    MOCK_METHOD6(GetClassList,
                 PlayerStatus_t (PlayerStreamInterface_c *Stream,
                                 Collator_t *Collator,
                                 FrameParser_t *FrameParser,
                                 Codec_t *Codec,
                                 OutputTimer_t *OutputTimer,
                                 ManifestationCoordinator_t *
                                 ManifestationCoordinator));
    MOCK_METHOD2(GetDecodeBufferPool,
                 PlayerStatus_t (PlayerStreamInterface_c *Stream,
                                 BufferPool_t *Pool));
    MOCK_METHOD0(GetControlStructurePool,
                 BufferPool_t ());
    MOCK_METHOD3(GetPlaybackSpeed,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 Rational_t *Speed,
                                 PlayDirection_t *Direction));
    MOCK_METHOD3(GetPresentationInterval,
                 PlayerStatus_t (PlayerStreamInterface_c *Stream,
                                 unsigned long long
                                 *IntervalStartNormalizedTime,
                                 unsigned long long
                                 *IntervalEndNormalizedTime));
    MOCK_METHOD3(PolicyValue,
                 int (PlayerPlayback_t Playback, PlayerStreamInterface_c *Stream,
                      PlayerPolicy_t Policy));

    MOCK_METHOD1(GetLastNativeTime,
                 unsigned long long(PlayerPlayback_t Playback));
    MOCK_METHOD2(SetLastNativeTime,
                 void (PlayerPlayback_t Playback, unsigned long long Time));
    MOCK_METHOD1(PlaybackLowPowerEnter,
                 PlayerStatus_t (PlayerPlayback_t Playback));
    MOCK_METHOD1(PlaybackLowPowerExit,
                 PlayerStatus_t (PlayerPlayback_t Playback));
    MOCK_METHOD1(StreamLowPowerEnter,
                 PlayerStatus_t (PlayerStreamInterface_c *Stream));
    MOCK_METHOD1(StreamLowPowerExit, PlayerStatus_t (PlayerStreamInterface_c *Stream));
    MOCK_METHOD1(GetStatistics,
                 PlayerStreamStatistics_t (PlayerStreamInterface_c *Stream));
    MOCK_METHOD1(ResetStatistics, void (PlayerStreamInterface_c *Stream));
    MOCK_METHOD1(GetStatistics,
                 PlayerPlaybackStatistics_t (PlayerPlayback_t Playback));
    MOCK_METHOD1(ResetStatistics, void (PlayerPlayback_t Playback));
    MOCK_METHOD4(SetControl,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream, stm_se_ctrl_t Ctrl,
                                 const void *Value));
    MOCK_METHOD4(GetControl,
                 PlayerStatus_t (PlayerPlayback_t Playback,
                                 PlayerStreamInterface_c *Stream, stm_se_ctrl_t Ctrl,
                                 void *Value));
    MOCK_METHOD1(GetAttributes,
                 PlayerStreamAttributes_t (PlayerStreamInterface_c *Stream));
    MOCK_METHOD1(ResetAttributes, void (PlayerStreamInterface_c *Stream));
};

#endif
