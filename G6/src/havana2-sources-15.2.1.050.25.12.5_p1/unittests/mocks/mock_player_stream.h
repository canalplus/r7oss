#ifndef _MOCK_PLAYER_STREAM_H_
#define _MOCK_PLAYER_STREAM_H_

#include "gmock/gmock.h"
#include "mock_collate_time_frame_parser.h"
#include "player_stream.h"

class MockPlayerStream_c : public PlayerStreamInterface_c {
  public:
    MockPlayerStream_c(Player_t player, PlayerPlayback_t playback, PlayerStreamType_t streamType);
    virtual ~MockPlayerStream_c();

    MOCK_METHOD0(GetPlayer,
                 Player_t());
    MOCK_METHOD0(GetPlayback,
                 PlayerPlayback_t());
    MOCK_METHOD0(GetCodedFrameBufferType,
                 BufferType_t());
    MOCK_METHOD0(GetHavanaStream,
                 HavanaStream_t());
    MOCK_METHOD0(GetDecodeBufferManager,
                 DecodeBufferManager_t());
    MOCK_METHOD0(GetCollator,
                 Collator_t());
    MOCK_METHOD0(GetFrameParser,
                 FrameParser_t());
    MOCK_METHOD0(GetCodec,
                 Codec_t());
    MOCK_METHOD0(GetManifestationCoordinator,
                 ManifestationCoordinator_t());
    MOCK_METHOD0(GetOutputTimer,
                 OutputTimer_t());
    MOCK_METHOD0(GetEsProcessor,
                 ES_Processor_c * ());
    MOCK_METHOD0(GetStreamType,
                 PlayerStreamType_t());
    MOCK_METHOD0(GetInstanceId,
                 unsigned int());
    MOCK_METHOD0(GetPolicyRecord,
                 PlayerPolicyState_t *());
    MOCK_METHOD0(GetControlsRecord,
                 PlayerControslStorageState_t *());
    MOCK_METHOD0(GetPresentationIntervalStartNormalizedTime,
                 unsigned long long());
    MOCK_METHOD0(GetPresentationIntervalEndNormalizedTime,
                 unsigned long long());
    MOCK_CONST_METHOD0(GetNumberOfDecodeBuffers,
                       unsigned int());
    MOCK_METHOD0(GetDecodeBufferPool,
                 BufferPool_t());
    MOCK_METHOD0(GetStatistics,
                 const PlayerStreamStatistics_t &());
    MOCK_METHOD0(ResetStatistics,
                 void());
    MOCK_METHOD0(Statistics,
                 PlayerStreamStatistics_t &());
    MOCK_METHOD0(GetAttributes,
                 const PlayerStreamAttributes_t &());
    MOCK_METHOD0(ResetAttributes,
                 void());
    MOCK_METHOD2(SetSpeed,
                 void(Rational_t Speed, PlayDirection_t Direction));
    MOCK_METHOD0(MarkUnPlayable,
                 PlayerStatus_t());
    MOCK_CONST_METHOD0(IsUnPlayable,
                       bool());
    MOCK_METHOD0(Terminate,
                 void());
    MOCK_CONST_METHOD0(IsTerminating,
                       bool());
    MOCK_METHOD1(GetElementaryBufferLevel,
                 PlayerStatus_t(stm_se_ctrl_play_stream_elementary_buffer_level_t *ElementaryBufferLevel));
    MOCK_METHOD0(EndOfStream,
                 PlayerStatus_t());
    MOCK_METHOD3(InputJump,
                 PlayerStatus_t(bool SurplusData, bool ContinuousReverseJump, bool FromDiscontinuityControl));
    MOCK_METHOD6(Drain,
                 PlayerStatus_t(bool NonBlocking, bool SignalEvent, void *EventUserData, PlayerPolicy_t PlayoutPolicy, bool ParseAllFrames, unsigned long long *pDrainSequenceNumber));
    MOCK_METHOD0(Step,
                 PlayerStatus_t());
    MOCK_METHOD4(Switch,
                 PlayerStatus_t(Collator_t Collator, FrameParser_t FrameParser, Codec_t Codec, bool NonBlocking));
    MOCK_METHOD3(SetAlarm,
                 PlayerStatus_t(stm_se_play_stream_alarm_t alarm, bool enable, void *value));
    MOCK_METHOD0(ResetDiscardTrigger,
                 PlayerStatus_t());
    MOCK_METHOD1(SetDiscardTrigger,
                 PlayerStatus_t(stm_se_play_stream_discard_trigger_t const &trigger));
    MOCK_METHOD1(GetCodedFrameBufferPool,
                 BufferPool_t(unsigned int *MaximumCodedFrameSize));
    MOCK_METHOD0(LowPowerEnter,
                 PlayerStatus_t());
    MOCK_METHOD0(LowPowerExit,
                 PlayerStatus_t());
    MOCK_METHOD0(IsLowPowerState,
                 bool());
    MOCK_METHOD0(SetLowPowerEnterEvent,
                 void());
    MOCK_METHOD0(WaitForLowPowerExitEvent,
                 void());
    MOCK_METHOD0(GetCollateTimeFrameParser,
                 CollateTimeFrameParserInterface_c * ());
    MOCK_METHOD1(SignalEvent,
                 PlayerStatus_t(struct PlayerEventRecord_s *PlayerEvent));
    MOCK_METHOD0(GetParseToDecodeEdge,
                 ActiveEdgeInterface_c * ());

    MockCollateTimeFrameParser_c    mParser;
    PlayerStreamStatistics_t        mStatistics;
    Player_t                        mPlayer;
    PlayerPlayback_t                mPlayback;
    PlayerStreamType_t              mStreamType;
};

#endif
