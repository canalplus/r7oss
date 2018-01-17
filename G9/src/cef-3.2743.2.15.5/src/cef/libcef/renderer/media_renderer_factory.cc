#include "libcef/renderer/media_renderer_factory.h"
#include "libcef/renderer/media/cef_renderer.h"
#include "libcef/common/content_client.h"

#include "include/cef_media_delegate.h"
#include "include/cef_app.h"

CefRendererFactory::CefRendererFactory(
  content::RenderFrame* render_frame,
  media::GpuVideoAcceleratorFactories* gpu_factories,
  const scoped_refptr<media::MediaLog>& media_log,
  const CefGetDisplayInfoCB& get_display_info_cb)
  : render_frame_(render_frame),
    gpu_factories_(gpu_factories),
    media_log_(media_log),
    get_display_info_cb_(get_display_info_cb),
    delegate_(NULL)
{
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get())
    delegate_ = application->GetMediaDelegate();
}

CefRendererFactory::~CefRendererFactory()
{
}

std::unique_ptr<media::Renderer> CefRendererFactory::CreateRenderer(
  const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
  const scoped_refptr<base::TaskRunner>& worker_task_runner,
  media::AudioRendererSink* audio_renderer_sink,
  media::VideoRendererSink* video_renderer_sink,
  const media::RequestSurfaceCB& request_surface_cb)
{
  if (delegate_.get())
  {
    std::unique_ptr<media::Renderer> res(new CefMediaRenderer(
                                           media_log_,
                                           media_task_runner,
                                           worker_task_runner,
                                           video_renderer_sink,
                                           delegate_,
                                           get_display_info_cb_));
    return std::move(res);
  }
  return NULL;
}
