#ifndef CEF_MEDIA_DELEGATE_H_
# define CEF_MEDIA_DELEGATE_H_

#include "include/cef_base.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    UnknownVideoCodec = 0,
    CodecH264,
    CodecVC1,
    CodecMPEG2,
    CodecMPEG4,
    CodecTheora,
    CodecVP8,
    CodecVP9,
    CodecHEVC,
  } cef_video_codec_t;

  typedef enum {
    UnknownAudioCodec = 0,
    CodecAAC = 1,
    CodecMP3 = 2,
    CodecPCM = 3,
    CodecVorbis = 4,
    CodecFLAC = 5,
    CodecAMR_NB = 6,
    CodecAMR_WB = 7,
    CodecPCM_MULAW = 8,
    CodecGSM_MS = 9,
    CodecPCM_S16BE = 10,
    CodecPCM_S24BE = 11,
    CodecOpus = 12,
    CodecPCM_ALAW = 14,
    CodecALAC = 15,
  } cef_audio_codec_t;

#ifdef __cplusplus
}
#endif

///
// Callback class used by the client to signal media events
///
/*--cef(source=library)--*/
class CefMediaEventCallback : public virtual CefBase {
  public:
    ///
    // Signal an end of stream
    ///
    /*--cef(capi_name=end_of_stream)--*/
    virtual void  EndOfStream() = 0;

    ///
    // Signal a resolution change
    ///
    /*--cef(capi_name=resolution_changed)--*/
    virtual void  ResolutionChanged(int width, int height) = 0;

    ///
    // Signal a video PTS
    ///
    /*--cef(capi_name=video_pts)--*/
    virtual void  VideoPTS(int64 pts) = 0;

    ///
    // Signal an audio PTS
    ///
    /*--cef(capi_name=audio_pts)--*/
    virtual void  AudioPTS(int64 pts) = 0;

    ///
    // Signal that the underlying stack has enough data to play
    ///
    /*--cef(capi_name=have_enough)--*/
    virtual void  HaveEnough() = 0;
};

///
// Delegate interface implemented by the client to handle media
///
/*--cef(source=client)--*/
class CefMediaDelegate : public virtual CefBase {
  public:

    ///
    // Used to set the event callback class from the library that the client
    // should use
    ///
    /*--cef()--*/
    virtual void SetEventCallback(CefRefPtr<CefMediaEventCallback> event) = 0;
    ///
    // Called when the client should stop current playback
    ///
    /*--cef()--*/
    virtual void Stop() = 0;
    ///
    // Called when the client should cleanup before being destroyed
    ///
    /*--cef()--*/
    virtual void Cleanup() = 0;
    ///
    // Called when the client should pause current playback
    ///
    /*--cef()--*/
    virtual void Pause() = 0;
    ///
    // Called when the client should reset itself
    ///
    /*--cef()--*/
    virtual void Reset() = 0;
    ///
    // Called when the client should start playback
    ///
    /*--cef()--*/
    virtual void Play() = 0;
    ///
    // Called when the client should resume playback
    ///
    /*--cef()--*/
    virtual void Resume() = 0;
    ///
    // Called to set the client playback speed rate
    ///
    /*--cef()--*/
    virtual void SetSpeed(double rate) = 0;
    ///
    // Called to set the client playback audio volume
    ///
    /*--cef()--*/
    virtual void SetVolume(float volume) = 0;
    ///
    // Called to set the client video plan
    ///
    /*--cef()--*/
    virtual void SetVideoPlan(int x, int y, int width, int height,
        unsigned int display_width, unsigned int display_height) = 0;
    ///
    // Called to check that the client supports a specific video codec
    ///
    /*--cef()--*/
    virtual bool CheckVideoCodec(cef_video_codec_t codec) = 0;
    ///
    // Called to check that the client supports a specific audio codec
    ///
    /*--cef()--*/
    virtual bool CheckAudioCodec(cef_audio_codec_t codec) = 0;
    ///
    // Called when the client should flush
    ///
    /*--cef()--*/
    virtual void Flush() = 0;
    ///
    // Called to get the maximum number of audio samples that can be sent
    ///
    /*--cef()--*/
    virtual int MaxAudioSampleCount() = 0;
    ///
    // Called to get the maximum number of video samples that can be sent
    ///
    /*--cef()--*/
    virtual int MaxVideoSampleCount() = 0;
    ///
    // Called when the client should open its audio layer
    ///
    /*--cef()--*/
    virtual bool OpenAudio() = 0;
    ///
    // Called when the client should open its video layer
    ///
    /*--cef()--*/
    virtual bool OpenVideo() = 0;
    ///
    // Called when the client should close its audio layer
    ///
    /*--cef()--*/
    virtual void CloseAudio() = 0;
    ///
    // Called when the client should close its video layer
    ///
    /*--cef()--*/
    virtual void CloseVideo() = 0;
    ///
    // Called to send an audio buffer
    ///
    /*--cef(default_retval=-1)--*/
    virtual int SendAudioBuffer(char* buf, int size, int64 pts) = 0;
    ///
    // Called to send a video buffer
    ///
    /*--cef(default_retval=-1)--*/
    virtual int SendVideoBuffer(char* buf, int size, int64 pts) = 0;
};

#endif /* !CEF_MEDIA_DELEGATE.H */
