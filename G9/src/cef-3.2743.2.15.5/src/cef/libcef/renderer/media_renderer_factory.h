#ifndef CEF_LIBCEF_RENDERER_MEDIA_RENDERER_FACTORY_H_
# define CEF_LIBCEF_RENDERER_MEDIA_RENDERER_FACTORY_H_

#include "include/cef_base.h"
#include "include/cef_media_delegate.h"
#include "base/macros.h"
#include "media/base/renderer_factory.h"
#include "content/public/renderer/render_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/base/media_log.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/audio_renderer_sink.h"
#include "libcef/common/cef_display.h"

class CefRendererFactory : public media::RendererFactory
{
  public:
    CefRendererFactory(
      content::RenderFrame* render_frame,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const scoped_refptr<media::MediaLog>& media_log,
      const CefGetDisplayInfoCB& get_display_info_cb);
    ~CefRendererFactory();

    std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      const media::RequestSurfaceCB& request_surface_cb) final;

  private:

    content::RenderFrame* render_frame_;
    media::GpuVideoAcceleratorFactories* gpu_factories_;
    const scoped_refptr<media::MediaLog> media_log_;
    const CefGetDisplayInfoCB get_display_info_cb_;
    CefRefPtr<CefMediaDelegate>  delegate_;

    DISALLOW_COPY_AND_ASSIGN(CefRendererFactory);
};

#endif /* !CEF_LIBCEF_RENDERER_MEDIA_RENDERER_FACTORY.H */
