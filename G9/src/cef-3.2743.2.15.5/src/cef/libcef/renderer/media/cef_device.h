#ifndef CEF_LIBCEF_RENDERER_MEDIA_CEF_DEVICE_H_
# define CEF_LIBCEF_RENDERER_MEDIA_CEF_DEVICE_H_

#include "media/base/demuxer_stream.h"
#include "media/base/decryptor.h"
#include "base/threading/thread.h"

#include "include/cef_base.h"
#include "include/cef_media_delegate.h"

#include "libcef/renderer/media/cef_renderer.h"

#include <queue>
#include <deque>

#define INTERNAL_BUFFER_SIZE 50

class CefMediaDevice :
  public base::RefCountedThreadSafe<CefMediaDevice> {
  public:

    CefMediaDevice(CefRefPtr<CefMediaDelegate>& delegate,
                   const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                   const scoped_refptr<media::MediaLog>& media_log);
    virtual ~CefMediaDevice();

    bool Open(media::DemuxerStream* stream,
              const CefMediaRenderer::StatisticsCB& stats_cb,
              const CefMediaRenderer::LastPTSCB& last_pts_cb,
              media::RendererClient* client);
    bool Close();
    void StartFeeding(base::TimeDelta start_time);
    void StopFeeding();
    void OnPTSEvent(int64_t pts);
    void SetDecryptor(media::Decryptor* decryptor);

  private:

    void Feed();
    void OnKeyAdded();
    void OnReadCb(media::DemuxerStream::Status status, const scoped_refptr<media::DecoderBuffer>& buffer);
    void OnDecryptCb(media::Decryptor::Status status, const scoped_refptr<media::DecoderBuffer>& buffer);
    void OnRead(media::DemuxerStream::Status status, const scoped_refptr<media::DecoderBuffer>& buffer);
    void OnDecrypt(media::Decryptor::Status status, const scoped_refptr<media::DecoderBuffer>& buffer);
    void DoRead();
    void DoReadTrampolined();
    void DecodeBuffer(const scoped_refptr<media::DecoderBuffer>& buffer);
    void FeedBuffer(const scoped_refptr<media::DecoderBuffer>& buffer);
    void EnqueueBuffer(const scoped_refptr<media::DecoderBuffer>& buffer);
    void EmptyQueuedBuffers();
    void FeedQueuedBuffer();
    bool HandleConfigChange();
    void EmptyPTSQueue();

    int SendInternal(uint8_t* buf, int size, int64_t pts);

    CefRefPtr<CefMediaDelegate> delegate_;
    scoped_refptr<media::MediaLog> media_log_;
    media::DemuxerStream* stream_;
    media::Decryptor* decryptor_;
    media::RendererClient* client_;

    bool reading_;
    bool feeding_;
    bool closed_;
    bool stop_feeding_;
    bool is_encrypted_;
    bool is_decrypting_;
    size_t max_frame_count_;

    base::TimeDelta start_time_;
    media::DemuxerStream::ReadCB read_cb_;
    CefMediaRenderer::StatisticsCB stats_cb_;
    CefMediaRenderer::LastPTSCB last_pts_cb_;
    media::Decryptor::DecryptCB decrypt_cb_;
    std::queue<scoped_refptr<media::DecoderBuffer> > queued_buffers_;
    std::deque<int64_t> pts_queue_;

    scoped_refptr<media::DecoderBuffer> waiting_buffer_;
    bool waiting_key_;

    base::WeakPtr<CefMediaDevice> weak_this_;
    base::WeakPtrFactory<CefMediaDevice> weak_factory_;
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

#endif /* !CEF_DEVICE.H */
