#include "base/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/filters/chunk_demuxer.h"
#include "libcef/renderer/media/cef_device.h"
#include "media/base/renderer_client.h"

#include <errno.h>
#include <string.h>
#include <iostream>

/* Code used for debugging and testing */
#if 0
#define DBG_PRINT(msg) do { \
  std::cerr << "CefMediaDevice("; \
  std::cerr << typeToString(stream_ ? \
      stream_->type() : media::DemuxerStream::UNKNOWN); \
  std::cerr << ")::" << __FUNCTION__ << ':' << __LINE__ << ": "; \
  std::cerr << msg << std::endl; \
} while (false)
#else
#define DBG_PRINT(msg) do {} while (false)
#endif

static inline const char *typeToString(media::DemuxerStream::Type type)
{
  if (type == media::DemuxerStream::AUDIO)
    return "AUDIO";
  else if (type == media::DemuxerStream::VIDEO)
    return "VIDEO";
  else
    return "OTHER";
}

/* CefMediaDevice implementation */
static media::Decryptor::StreamType DemuxerTypeToDecryptorType(media::DemuxerStream::Type type)
{
  if (type == media::DemuxerStream::AUDIO)
    return media::Decryptor::kAudio;
  else
    return media::Decryptor::kVideo;
}

CefMediaDevice::CefMediaDevice(CefRefPtr<CefMediaDelegate>& delegate, const scoped_refptr<base::SingleThreadTaskRunner>& task_runner, const scoped_refptr<media::MediaLog>& media_log)
  : delegate_(delegate),
    media_log_(media_log),
    stream_(NULL),
    decryptor_(NULL),
    client_(NULL),
    reading_(false),
    feeding_(false),
    closed_(true),
    stop_feeding_(false),
    is_encrypted_(false),
    is_decrypting_(false),
    max_frame_count_(0),
    waiting_buffer_(NULL),
    waiting_key_(false),
    weak_factory_(this),
    task_runner_(task_runner)
{
  weak_this_ = weak_factory_.GetWeakPtr();
}

CefMediaDevice::~CefMediaDevice()
{
  if (stream_ && !closed_)
    Close();
}

bool CefMediaDevice::Open(media::DemuxerStream* stream,
                          const CefMediaRenderer::StatisticsCB& stats_cb,
                          const CefMediaRenderer::LastPTSCB& last_pts_cb,
                          media::RendererClient* client)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  stream_ = stream;
  stats_cb_ = stats_cb;
  last_pts_cb_ = last_pts_cb;
  client_ = client;
  read_cb_ = media::BindToCurrentLoop(base::Bind(&CefMediaDevice::OnReadCb, weak_this_));
  decrypt_cb_ = media::BindToCurrentLoop(base::Bind(&CefMediaDevice::OnDecryptCb, weak_this_));

  stream_->EnableBitstreamConverter();

  switch (stream_->type())
  {
    case media::DemuxerStream::AUDIO:
      is_encrypted_ = stream_->audio_decoder_config().is_encrypted();
      max_frame_count_ = delegate_->MaxAudioSampleCount();
      closed_ = false;
      return delegate_->OpenAudio();
    case media::DemuxerStream::VIDEO:
      is_encrypted_ = stream_->video_decoder_config().is_encrypted();
      max_frame_count_ = delegate_->MaxVideoSampleCount();
      closed_ = false;
      return delegate_->OpenVideo();
    default:
      if (media_log_.get())
        media_log_->AddLogEvent(media::MediaLog::MEDIALOG_INFO, "Unexpected stream type");
      return false;
  }
}

void CefMediaDevice::OnReadCb(media::DemuxerStream::Status status, const scoped_refptr<media::DecoderBuffer>& buffer)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  reading_ = false;
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&CefMediaDevice::OnRead, weak_this_, status, buffer)
    );
}

void CefMediaDevice::OnDecryptCb(media::Decryptor::Status status, const scoped_refptr<media::DecoderBuffer>& buffer)
{
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&CefMediaDevice::OnDecrypt, weak_this_, status, buffer)
    );
}

void CefMediaDevice::DoReadTrampolined()
{
  stream_->Read(read_cb_);
}

void CefMediaDevice::DoRead()
{
  if (reading_)
    return;
  reading_ = true;
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&CefMediaDevice::DoReadTrampolined, weak_this_)
    );
}

bool CefMediaDevice::Close()
{
  StopFeeding();

  if (is_encrypted_ && is_decrypting_ && decryptor_ && stream_)
    decryptor_->CancelDecrypt(DemuxerTypeToDecryptorType(stream_->type()));

  switch (stream_->type())
  {
    case media::DemuxerStream::AUDIO:
      delegate_->CloseAudio();
      break;
    case media::DemuxerStream::VIDEO:
      delegate_->CloseVideo();
      break;
    default:
      return false;
  }

  if (waiting_buffer_.get())
    waiting_buffer_ = NULL;

  closed_ = true;
  return true;
}

void CefMediaDevice::StartFeeding(base::TimeDelta start_time)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!stream_)
    return;

  if (!feeding_ && !waiting_key_)
  {
    EmptyPTSQueue();
    task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CefMediaDevice::Feed, weak_this_)
      );
  }
  stop_feeding_ = false;
  start_time_ = start_time;

  if (waiting_buffer_.get())
    waiting_buffer_ = NULL;
}

void CefMediaDevice::StopFeeding()
{
  DCHECK(task_runner_->BelongsToCurrentThread());


  stop_feeding_ = true;
  feeding_ = false;
  waiting_key_ = false;
  if (waiting_buffer_.get())
    waiting_buffer_ = NULL;
  EmptyQueuedBuffers();
}

void CefMediaDevice::OnPTSEvent(int64_t pts)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool feed = true;

  if (pts_queue_.size() > 0)
  {
    while (pts_queue_.size() > 0 && pts_queue_.front() <= pts)
    {
      pts_queue_.pop_front();
    }
    feed = (pts_queue_.size() < max_frame_count_);
  }
  if (queued_buffers_.size() > 0) {
    if (feed) {
      task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CefMediaDevice::FeedQueuedBuffer, weak_this_)
        );
    }
  } else if (feed && !feeding_ && !stop_feeding_)
    task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CefMediaDevice::Feed, weak_this_)
      );
}

void CefMediaDevice::SetDecryptor(media::Decryptor* decryptor)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  decryptor_ = decryptor;
  decryptor_->RegisterNewKeyCB(
    DemuxerTypeToDecryptorType(stream_->type()),
    media::BindToCurrentLoop(base::Bind(&CefMediaDevice::OnKeyAdded, weak_this_))
    );
}

void CefMediaDevice::Feed()
{
  if (feeding_)
    return;
  feeding_ = true;
  stop_feeding_ = false;
  if (waiting_key_ && waiting_buffer_.get())
  {
    waiting_key_ = false;
    OnRead(media::DemuxerStream::kOk, waiting_buffer_);
  }
  else
    DoRead();
}

void CefMediaDevice::OnKeyAdded()
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (waiting_key_)
  {
    feeding_ = false;
    // Key was added we can start feeding again
    task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CefMediaDevice::Feed, weak_this_)
      );
  }
}

void CefMediaDevice::OnRead(media::DemuxerStream::Status status, const scoped_refptr<media::DecoderBuffer>& buffer)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  /* Configuration changed */
  if (status == media::DemuxerStream::kConfigChanged)
  {
    if (HandleConfigChange() && !stop_feeding_)
      DoRead();
    else
      StopFeeding();
    return;
  }
  /* Received an unreadable buffer */
  if (status != media::DemuxerStream::kOk)
  {
    StopFeeding();
    return;
  }
  /* EOS so stop feeding */
  if (buffer->end_of_stream())
  {
    StopFeeding();
    if (pts_queue_.size() < 4)
      last_pts_cb_.Run(pts_queue_.front());
    else
      last_pts_cb_.Run(pts_queue_[pts_queue_.size() - 4]);
    return;
  }
  /* Asked to stop */
  if (stop_feeding_)
  {
    feeding_ = false;
    return;
  }
  if (!is_encrypted_)
    FeedBuffer(buffer);
  else
    DecodeBuffer(buffer);
}

void CefMediaDevice::OnDecrypt(media::Decryptor::Status status, const scoped_refptr<media::DecoderBuffer>& buffer)
{
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_decrypting_ = false;
  if (status == media::Decryptor::kSuccess && buffer.get() != NULL)
    FeedBuffer(buffer);
  else if (status == media::Decryptor::kNoKey)
  {
    waiting_key_ = true;
    if (buffer.get() && buffer->data_size() > 0)
      waiting_buffer_ = buffer;
    if (client_)
      client_->OnWaitingForDecryptionKey();
  }
  else
  {
    feeding_ = false;
    if (media_log_.get())
      media_log_->AddLogEvent(media::MediaLog::MEDIALOG_ERROR, "Buffer decryption failed");
    if (client_)
      client_->OnError(media::PipelineStatus::PIPELINE_ERROR_DECODE);
  }
}

void CefMediaDevice::DecodeBuffer(const scoped_refptr<media::DecoderBuffer>& buffer)
{
  if (!decryptor_)
  {
    waiting_key_ = true;
    if (buffer.get() && buffer->data_size() > 0)
      waiting_buffer_ = buffer;
    if (media_log_.get())
      media_log_->AddLogEvent(media::MediaLog::MEDIALOG_INFO, "Trying to decode without a decryptor");
    return;
  }

  is_decrypting_ = true;
  decryptor_->Decrypt(DemuxerTypeToDecryptorType(stream_->type()),
                      buffer,
                      decrypt_cb_);
}

void CefMediaDevice::FeedBuffer(const scoped_refptr<media::DecoderBuffer>& buffer)
{
  int res;
  uint8_t *data = (uint8_t*)buffer->data();
  int size = buffer->data_size();
  int64_t pts = (buffer->timestamp() - start_time_).InMilliseconds();

  if (stop_feeding_)
  {
    feeding_ = false;
    return;
  }
  if (pts < 0)
  {
    DoRead();
    return;
  }

  bool processed = false;
  size_t pts_queue_size = pts_queue_.size();

  /* Try to send to decoder first if enough space and no queued buffer. */
  if (queued_buffers_.size() == 0 && pts_queue_size < max_frame_count_)
  {
    res = SendInternal(data, size, pts);
    stats_cb_.Run(size);
    /* Buffer sent successfully. */
    if (res == size) {
      pts_queue_.push_back(pts);
      DoRead();
      processed = true;
    }
    /* Buffer sending failed and decode buffer not full. */
    else if (res != -ENOSPC) {
      feeding_ = false;
      processed = true;
    }
  }

  /* Try to queue the buffer if it was not sent. */
  if (!processed && queued_buffers_.size() < INTERNAL_BUFFER_SIZE) {
    EnqueueBuffer(buffer);
    feeding_ = false;
    processed = true;
  }

  /* Packet not sent to decoder and not queued, or buffer queue is now full. */
  if (!processed || queued_buffers_.size() >= INTERNAL_BUFFER_SIZE) {
    /* Stop feeding. */
    feeding_ = false;
  }

  if (waiting_buffer_.get())
    waiting_buffer_ = NULL;
}

void CefMediaDevice::EmptyQueuedBuffers()
{
  while (!queued_buffers_.empty())
    queued_buffers_.pop();
}

void CefMediaDevice::EnqueueBuffer(const scoped_refptr<media::DecoderBuffer>& buffer)
{
  queued_buffers_.push(scoped_refptr<media::DecoderBuffer>(std::move(buffer)));
}

bool CefMediaDevice::HandleConfigChange()
{
  EmptyQueuedBuffers();

  switch (stream_->type())
  {
    case media::DemuxerStream::AUDIO:
      stream_->audio_decoder_config();
      break;
    case media::DemuxerStream::VIDEO:
      stream_->video_decoder_config();
      break;
    default:
      return false;
  }

  return true;
}

void CefMediaDevice::FeedQueuedBuffer()
{
  if (stop_feeding_)
  {
    feeding_ = false;
    return;
  }
  if (queued_buffers_.size() == 0)
  {
    DoRead();
    return;
  }

  while (!queued_buffers_.empty() && pts_queue_.size() < max_frame_count_) {

    const scoped_refptr<media::DecoderBuffer>& buffer = queued_buffers_.front();
    uint8_t *data = (uint8_t*)buffer->data();
    int size = buffer->data_size();
    int64_t pts = (buffer->timestamp() - start_time_).InMilliseconds();
    int res = SendInternal(data, size, pts);

    stats_cb_.Run(size);
    if (res == size) {
      pts_queue_.push_back(pts);
      queued_buffers_.pop();
    }
    /* Decode buffer full. */
    else if (res == -ENOSPC) {
      feeding_ = false;
      break;
    }
    /* Could not send to decoder. */
    else {
      queued_buffers_.pop();
      feeding_ = false;
      break;
    }
  }

  if (feeding_ && queued_buffers_.empty() &&
      pts_queue_.size() < max_frame_count_)
    DoRead();
  else
    feeding_ = false;
}

void CefMediaDevice::EmptyPTSQueue()
{
  while (!pts_queue_.empty())
    pts_queue_.pop_front();
}

int CefMediaDevice::SendInternal(uint8_t* buf, int size, int64_t pts)
{
  switch (stream_->type())
  {
    case media::DemuxerStream::AUDIO:
      return delegate_->SendAudioBuffer(reinterpret_cast<char *>(buf), size, pts);
    case media::DemuxerStream::VIDEO:
      return delegate_->SendVideoBuffer(reinterpret_cast<char *>(buf), size, pts);
    default:
      break;
  }
  return 0;
}
