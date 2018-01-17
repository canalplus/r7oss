#ifndef CEF_LIBCEF_RENDERER_MEDIA_CEF_RENDERER_H_
# define CEF_LIBCEF_RENDERER_MEDIA_CEF_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"

#include "media/base/renderer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/cdm_context.h"

#include "libcef/common/cef_display.h"

#include "include/cef_media_delegate.h"

class CefMediaDevice;

class CefMediaRenderer :
  public media::Renderer {
  public:

    typedef base::Callback<void(int)> StatisticsCB;
    typedef base::Callback<void(int64_t)> LastPTSCB;

    CefMediaRenderer(
      const scoped_refptr<media::MediaLog>& media_log,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::VideoRendererSink* video_renderer_sink,
      CefRefPtr<CefMediaDelegate>& delegate,
      const CefGetDisplayInfoCB& get_display_info_cb);

    virtual ~CefMediaRenderer();

    // Initializes the Renderer with |demuxer_stream_provider|, executing
    // |init_cb| upon completion. |demuxer_stream_provider| must be valid for
    // the lifetime of the Renderer object.  |init_cb| must only be run after this
    // method has returned.  Firing |init_cb| may result in the immediate
    // destruction of the caller, so it must be run only prior to returning.
    virtual void Initialize(media::DemuxerStreamProvider* demuxer_stream_provider,
                            media::RendererClient* client,
                            const media::PipelineStatusCB& init_cb) override;

    // Associates the |cdm_context| with this Renderer for decryption (and
    // decoding) of media data, then fires |cdm_attached_cb| with the result.
    virtual void SetCdm(media::CdmContext* cdm_context,
                        const media::CdmAttachedCB& cdm_attached_cb) override;

    // The following functions must be called after Initialize().

    // Discards any buffered data, executing |flush_cb| when completed.
    virtual void Flush(const base::Closure& flush_cb) override;

    // Starts rendering from |time|.
    virtual void StartPlayingFrom(base::TimeDelta time) override;

    // Updates the current playback rate. The default playback rate should be 1.
    virtual void SetPlaybackRate(double playback_rate) override;

    // Sets the output volume. The default volume should be 1.
    virtual void SetVolume(float volume) override;

    // Returns the current media time.
    virtual base::TimeDelta GetMediaTime() override;

    // Returns whether |this| renders audio.
    virtual bool HasAudio() override;

    // Returns whether |this| renders video.
    virtual bool HasVideo() override;

    // Signal the position and size of the video surface on screen
    void UpdateVideoSurface(int x, int y, int width, int height);

    void OnEndOfStream();
    void OnResolutionChanged(int width, int height);
    void OnVideoPTS(int64_t pts);
    void OnAudioPTS(int64_t pts);
    void OnHaveEnough();

  private:

    enum State {
      UNDEFINED,
      INITIALIZED,
      PLAYING,
      FLUSHING,
      STOPPING,
      STOPPED,
      ERROR,
    };

    bool CheckVideoConfiguration(media::DemuxerStream* stream);
    bool CheckAudioConfiguration(media::DemuxerStream* stream);

    void InitializeTask(media::DemuxerStream* video_stream,
                        media::DemuxerStream* audio_stream);
    void Cleanup();

    void OnStatistics(media::DemuxerStream::Type type, int size);
    void OnLastPTS(media::DemuxerStream::Type type, int64_t pts);
    void OnStop();

    void SendHaveEnough();

    const scoped_refptr<media::MediaLog>              media_log_;
    const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
    CefRefPtr<CefMediaDelegate>                       delegate_;
    media::VideoRendererSink*                         video_renderer_sink_;
    const CefGetDisplayInfoCB                         get_display_info_cb_;
    media::Decryptor*                                 decryptor_;
    media::RendererClient*                            client_;

    media::PipelineStatusCB init_cb_;
    base::Closure           stop_cb_;
    media::CdmAttachedCB    cdm_attached_cb_;

    State   state_;
    bool    paused_;
    int64_t last_audio_pts_;
    int64_t last_video_pts_;
    int     x_;
    int     y_;
    int     width_;
    int     height_;
    float   volume_;
    float   rate_;
    bool    initial_video_hole_created_;

    base::TimeDelta           start_time_;
    base::TimeDelta           current_time_;
    media::PipelineStatistics stats_;

    CefMediaDevice *audio_renderer_;
    CefMediaDevice *video_renderer_;

    base::WeakPtr<CefMediaRenderer> weak_this_;
    base::WeakPtrFactory<CefMediaRenderer> weak_factory_;

    DISALLOW_COPY_AND_ASSIGN(CefMediaRenderer);
};

#endif /* !CEF_RENDERER.H */
