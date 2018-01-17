#include "media/base/demuxer_stream_provider.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decryptor.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/renderer_client.h"
#include "base/bind.h"
#include "base/logging.h"

#include "libcef/renderer/media/cef_renderer.h"
#include "libcef/renderer/media/cef_device.h"

class CefMediaEventCallbackImpl : public CefMediaEventCallback {
  public:
    CefMediaEventCallbackImpl(base::WeakPtr<CefMediaRenderer> renderer,
                              const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : renderer_(renderer),
        task_runner_(task_runner) {
    }

    ~CefMediaEventCallbackImpl() {
    }

    void EndOfStream() override {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefMediaRenderer::OnEndOfStream, renderer_));
    }

    void ResolutionChanged(int width, int height) override {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefMediaRenderer::OnResolutionChanged, renderer_,
            width, height));
    }

    void VideoPTS(int64_t pts) override {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefMediaRenderer::OnVideoPTS, renderer_, pts));
    }

    void AudioPTS(int64_t pts) override {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefMediaRenderer::OnAudioPTS, renderer_, pts));
    }

    void HaveEnough() override {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefMediaRenderer::OnHaveEnough, renderer_));
    }

  private:

    base::WeakPtr<CefMediaRenderer> renderer_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    IMPLEMENT_REFCOUNTING(CefMediaEventCallbackImpl);
};

CefMediaRenderer::CefMediaRenderer(
  const scoped_refptr<media::MediaLog>& media_log,
  const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
  const scoped_refptr<base::TaskRunner>& worker_task_runner,
  media::VideoRendererSink* video_renderer_sink,
  CefRefPtr<CefMediaDelegate>& delegate,
  const CefGetDisplayInfoCB& get_display_info_cb)
  : media_log_(media_log),
    media_task_runner_(media_task_runner),
    delegate_(delegate),
    video_renderer_sink_(video_renderer_sink),
    get_display_info_cb_(get_display_info_cb),
    decryptor_(NULL),
    client_(NULL),
    state_(UNDEFINED),
    paused_(false),
    last_audio_pts_(-1),
    last_video_pts_(-1),
    x_(0),
    y_(0),
    width_(0),
    height_(0),
    volume_(1),
    rate_(1),
    initial_video_hole_created_(false),
    audio_renderer_(NULL),
    video_renderer_(NULL),
    weak_factory_(this)
{
  weak_this_ = weak_factory_.GetWeakPtr();
}

CefMediaRenderer::~CefMediaRenderer()
{
  Cleanup();
}

void CefMediaRenderer::Initialize(
  media::DemuxerStreamProvider* demuxer_stream_provider,
  media::RendererClient* client,
  const media::PipelineStatusCB& init_cb)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  media::DemuxerStream* video_stream = NULL;
  media::DemuxerStream* audio_stream = NULL;

  /* Save callbacks for future use */
  init_cb_ = media::BindToCurrentLoop(init_cb);
  client_ = client;
  stop_cb_ = media::BindToCurrentLoop(base::Bind(&CefMediaRenderer::OnStop, weak_this_));
  last_audio_pts_ = -1;
  last_video_pts_ = -1;

  video_stream = demuxer_stream_provider->GetStream(media::DemuxerStream::VIDEO);
  audio_stream = demuxer_stream_provider->GetStream(media::DemuxerStream::AUDIO);

  /* Check video stream configuration */
  if (!CheckVideoConfiguration(video_stream) || !CheckAudioConfiguration(audio_stream))
  {
    Cleanup();
    if (media_log_.get())
      media_log_->AddLogEvent(media::MediaLog::MEDIALOG_ERROR, "Unsupported audio or video format");
    init_cb.Run(media::PipelineStatus::DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  media_task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&CefMediaRenderer::InitializeTask, weak_this_, video_stream, audio_stream)
    );
}

void CefMediaRenderer::InitializeTask(media::DemuxerStream* video_stream,
                                      media::DemuxerStream* audio_stream)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  CefRefPtr<CefMediaEventCallbackImpl> callbackImpl =
    new CefMediaEventCallbackImpl(weak_this_, media_task_runner_);
  delegate_->SetEventCallback(callbackImpl);

  if (audio_stream)
  {
    audio_renderer_ = new CefMediaDevice(delegate_, media_task_runner_, media_log_);
   if (!audio_renderer_->Open(
      audio_stream,
      base::Bind(&CefMediaRenderer::OnStatistics, weak_this_, media::DemuxerStream::AUDIO),
      base::Bind(&CefMediaRenderer::OnLastPTS, weak_this_, media::DemuxerStream::AUDIO),
      client_))
      goto exit_error;
    if (decryptor_)
      audio_renderer_->SetDecryptor(decryptor_);
  }
  if (video_stream)
  {
    video_renderer_ = new CefMediaDevice(delegate_, media_task_runner_, media_log_);
    if (!video_renderer_->Open(
          video_stream,
          base::Bind(&CefMediaRenderer::OnStatistics, weak_this_, media::DemuxerStream::VIDEO),
          base::Bind(&CefMediaRenderer::OnLastPTS, weak_this_, media::DemuxerStream::VIDEO),
          client_))
      goto exit_error;
    if (decryptor_)
      video_renderer_->SetDecryptor(decryptor_);
  }

  if (decryptor_ && !cdm_attached_cb_.is_null() && (video_stream || audio_stream))
    cdm_attached_cb_.Run(true);

  state_ = INITIALIZED;
  init_cb_.Run(media::PipelineStatus::PIPELINE_OK);
  return;

  exit_error:
  init_cb_.Run(media::PipelineStatus::PIPELINE_ERROR_INITIALIZATION_FAILED);
}

void CefMediaRenderer::Cleanup()
{
  if (state_ != STOPPED)
    delegate_->Stop();
  if (HasAudio())
  {
    audio_renderer_->Close();
    delete audio_renderer_;
  }
  if (HasVideo())
  {
    video_renderer_->Close();
    delete video_renderer_;
  }
  delegate_->Cleanup();
  delegate_ = NULL;
  state_ = UNDEFINED;
  decryptor_ = NULL;
}

// Associates the |cdm_context| with this Renderer for decryption (and
// decoding) of media data, then fires |cdm_attached_cb| with the result.
void CefMediaRenderer::SetCdm(media::CdmContext* cdm_context,
                              const media::CdmAttachedCB& cdm_attached_cb)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  bool attached = false;

  decryptor_ = NULL;
  cdm_attached_cb_.Reset();

  if (!cdm_context)
    goto err;

  decryptor_ = cdm_context->GetDecryptor();
  if (!decryptor_)
    goto err;

  if (HasAudio())
  {
    audio_renderer_->SetDecryptor(decryptor_);
    attached = true;
  }
  if (HasVideo())
  {
    video_renderer_->SetDecryptor(decryptor_);
    attached = true;
  }

  if (attached)
    cdm_attached_cb.Run(true);
  else
    cdm_attached_cb_ = cdm_attached_cb;

  return;

  err:
  cdm_attached_cb.Run(false);
  return;
}

// Discards any buffered data, executing |flush_cb| when completed.
void CefMediaRenderer::Flush(const base::Closure& flush_cb)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ != PLAYING)
    return;
  if (HasAudio())
    audio_renderer_->StopFeeding();
  if (HasVideo())
    video_renderer_->StopFeeding();
  if (!paused_)
  {
    paused_ = true;
    delegate_->Pause();
  }
  delegate_->Reset();
  state_ = FLUSHING;
  flush_cb.Run();
}

void CefMediaRenderer::SendHaveEnough() {
  if (client_) {
    client_->OnBufferingStateChange(media::BUFFERING_HAVE_ENOUGH);
  }
}

// Starts rendering from |time|.
void CefMediaRenderer::StartPlayingFrom(base::TimeDelta time)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ == INITIALIZED || state_ == STOPPED)
    delegate_->Play();
  else if (state_ == PLAYING)
    delegate_->Reset();
  else if (state_ == FLUSHING)
    media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CefMediaRenderer::SendHaveEnough, weak_this_)
      );
  else
    return;

  state_ = PLAYING;
  start_time_ = time;
  current_time_ = time;
  if (!initial_video_hole_created_)
  {
    initial_video_hole_created_ = true;
    video_renderer_sink_->PaintSingleFrame(
      media::VideoFrame::CreateHoleFrame(gfx::Size()));
  }
  if (HasAudio())
    audio_renderer_->StartFeeding(start_time_);
  if (HasVideo())
    video_renderer_->StartFeeding(start_time_);
}

// Updates the current playback rate. The default playback rate should be 1.
void CefMediaRenderer::SetPlaybackRate(double playback_rate)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ != INITIALIZED && state_ != PLAYING)
    return;
  if (playback_rate == 0)
  {
    if (!paused_)
    {
      paused_ = true;
      delegate_->Pause();
    }
    return;
  }
  if (paused_)
  {
    paused_ = false;
    delegate_->Resume();
  }
  if (rate_ != playback_rate)
    delegate_->SetSpeed(playback_rate);
  rate_ = playback_rate;
}

// Sets the output volume. The default volume should be 1.
void CefMediaRenderer::SetVolume(float volume)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ != INITIALIZED && state_ != STOPPED && state_ != PLAYING)
    return;
  if (volume != volume_)
  {
    volume_ = volume;
    delegate_->SetVolume(volume * 100);
  }
}

// Returns the current media time.
base::TimeDelta CefMediaRenderer::GetMediaTime()
{
  if (!HasVideo() && !HasAudio())
    return media::kNoTimestamp();

  return current_time_;
}

// Returns whether |this| renders audio.
bool CefMediaRenderer::HasAudio()
{
  return audio_renderer_ != NULL;
}

// Returns whether |this| renders video.
bool CefMediaRenderer::HasVideo()
{
  return video_renderer_ != NULL;
}

// Signal the position and size of the video surface on screen
void CefMediaRenderer::UpdateVideoSurface(int x, int y, int width, int height)
{
  if (state_ == UNDEFINED)
    return;

  if (x != x_ || y != y_ || width != width_ || height != height_)
  {
    CefDisplayInfo display_info = get_display_info_cb_.Run();

    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;
    delegate_->SetVideoPlan(x, y, width, height, display_info.width, display_info.height);
  }
}

bool CefMediaRenderer::CheckVideoConfiguration(media::DemuxerStream* stream)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!stream)
    return true;
  return delegate_->CheckVideoCodec((cef_video_codec_t)stream->video_decoder_config().codec());
}

bool CefMediaRenderer::CheckAudioConfiguration(media::DemuxerStream* stream)
{
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!stream)
    return true;
  return delegate_->CheckAudioCodec((cef_audio_codec_t)stream->audio_decoder_config().codec());
}

void CefMediaRenderer::OnStatistics(media::DemuxerStream::Type type, int size)
{
  if (type == media::DemuxerStream::VIDEO)
  {
    stats_.video_bytes_decoded += size;
    stats_.video_frames_decoded++;
  }
  else if (type == media::DemuxerStream::AUDIO)
  {
    stats_.audio_bytes_decoded += size;
  }

  if (client_)
    client_->OnStatisticsUpdate(stats_);
}

void CefMediaRenderer::OnStop()
{
  delegate_->Stop();
}

void CefMediaRenderer::OnEndOfStream()
{
  if (state_ != FLUSHING && state_ != STOPPED)
  {
    if (client_)
      client_->OnEnded();
    state_ = STOPPED;
    stop_cb_.Run();
  }
}

void CefMediaRenderer::OnResolutionChanged(int width, int height)
{
  gfx::Size new_size(width, height);

  if (video_renderer_) {
    if (video_renderer_sink_) {
      video_renderer_sink_->PaintSingleFrame(
        media::VideoFrame::CreateHoleFrame(new_size));
    }

    if (client_) {
      client_->OnVideoNaturalSizeChange(new_size);
      client_->OnVideoOpacityChange(true);
    }
  }
}

void CefMediaRenderer::OnVideoPTS(int64_t pts)
{
  if (HasVideo())
  {
    video_renderer_->OnPTSEvent(pts);
    current_time_ = start_time_ + base::TimeDelta::FromMilliseconds(pts);
  }

  if (last_video_pts_ > 0 && last_video_pts_ <= pts)
    OnEndOfStream();
}

void CefMediaRenderer::OnAudioPTS(int64_t pts)
{
  if (HasAudio())
  {
    audio_renderer_->OnPTSEvent(pts);
    if (!HasVideo())
      current_time_ = start_time_ + base::TimeDelta::FromMilliseconds(pts);
  }

  if (last_audio_pts_ > 0 && last_audio_pts_ <= pts)
    OnEndOfStream();
}

void CefMediaRenderer::OnHaveEnough()
{
  media_task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&CefMediaRenderer::SendHaveEnough, weak_this_)
    );
}

void CefMediaRenderer::OnLastPTS(media::DemuxerStream::Type type, int64_t pts)
{
  switch (type)
  {
    case media::DemuxerStream::VIDEO:
      last_video_pts_ = pts;
      break;
    case media::DemuxerStream::AUDIO:
      last_audio_pts_ = pts;
      break;
    default:
      break;
  }
}
