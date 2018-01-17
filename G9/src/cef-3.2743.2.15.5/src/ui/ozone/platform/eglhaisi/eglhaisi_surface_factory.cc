#include "base/files/file_path.h"
#include "base/native_library.h"
#include "ui/ozone/platform/eglhaisi/eglhaisi_surface_factory.h"
#include "ui/ozone/public/surface_ozone_egl.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/gfx/vsync_provider.h"



#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/gfx/skia_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

#include <EGL/egl.h>

#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
#include <directfb.h>
#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
#include <mi_common.h>
#endif
#elif defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
#include <nexus_display.h>
#include <nexus_platform.h>
#include <default_nexus.h>
#define EGLHAISI_WINDOW_WIDTH 1280
#define EGLHAISI_WINDOW_HEIGHT 720
#if !defined(OZONE_PLATFORM_EGLHAISI_NEXUS_ZORDER)
#define OZONE_PLATFORM_EGLHAISI_NEXUS_ZORDER (0)
#endif
#else
#error unknown backend
#endif

namespace ui {

namespace {

#if defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
static NXPL_PlatformHandle nxpl_handle = 0;
#endif // OZONE_PLATFORM_EGLHAISI_NEXUS

class SurfaceOzoneEglhaisi : public SurfaceOzoneEGL {
 public:
  SurfaceOzoneEglhaisi(gfx::AcceleratedWidget window_id) {
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
    DFBResult result;
    IDirectFB *dfb = NULL;

    layer_ = NULL;
    native_window_ = NULL;

#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
    result = DirectFBInit(0, NULL);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot init DirectFB: "
          << DirectFBErrorString(result);
      goto l_exit;
    }
#endif

    result = DirectFBCreate(&dfb);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot create DirectFB super interface: "
          << DirectFBErrorString(result);
      goto l_exit;
    }

    result = dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer_);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot get primary layer: "
          << DirectFBErrorString(result);
      goto l_exit;
    }

#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
    // Setup layer
    layer_->SetCooperativeLevel(layer_, DLSCL_ADMINISTRATIVE);
    layer_->SetOpacity(layer_, 0xff);
    layer_->SetBackgroundMode(layer_, DLBM_COLOR);
    layer_->SetBackgroundColor(layer_, 0x00, 0x00, 0x00, 0x00);

    DFBDisplayLayerConfig dlc;
    layer_->GetConfiguration(layer_, &dlc);
    dlc.flags = (DFBDisplayLayerConfigFlags)(DLCONF_OPTIONS|DLCONF_BUFFERMODE);
    dlc.options = (DFBDisplayLayerOptions)(DLOP_ALPHACHANNEL);
    dlc.buffermode = DLBM_BACKVIDEO;
    layer_->SetConfiguration( layer_, &dlc );

    // Fill the window description.
    DFBWindowDescription  desc;
    desc.flags        = (DFBWindowDescriptionFlags)(DWDESC_CAPS | DWDESC_SURFACE_CAPS);
    desc.caps         = (DFBWindowCapabilities)(DWCAPS_ALPHACHANNEL| DWCAPS_DOUBLEBUFFER);
    desc.surface_caps = DSCAPS_VIDEOONLY;

    // Create the window.
    window_ = NULL;
    result = layer_->CreateWindow(layer_, &desc, &window_);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot create window: "
          << DirectFBErrorString(result);
      goto l_exit;
    }

    window_->SetOpacity( window_, 0xff );
    window_->SetOptions(window_, (DFBWindowOptions)(DWOP_ALPHACHANNEL));
    window_->SetStackingClass(window_, DWSC_MIDDLE);
    window_->RaiseToTop(window_);

    result = window_->GetSurface(window_, &native_window_);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot get surface of window: "
          << DirectFBErrorString(result);
      goto l_exit;
    }
#else
    result = layer_->GetSurface(layer_, &native_window_);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot get surface of primary layer: "
          << DirectFBErrorString(result);
      goto l_exit;
    }
#endif

l_exit:
    if (result != DFB_OK) {
      if (native_window_) {
        native_window_->Release(native_window_);
        native_window_ = NULL;
      }
      if (layer_) {
        layer_->Release(layer_);
        layer_ = NULL;
      }
#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
      if (window_) {
        window_->Release(window_);
      }
#endif
    }
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB)
    if (dfb) {
      dfb->Release(dfb);
    }
#endif

#elif defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
    NXPL_NativeWindowInfoEXT window_info;

    NXPL_GetDefaultNativeWindowInfoEXT(&window_info);

    window_info.x = window_info.y = 0;
    window_info.width = EGLHAISI_WINDOW_WIDTH;
    window_info.height = EGLHAISI_WINDOW_HEIGHT;
    window_info.stretch = true;
    window_info.clientID = 1;
    window_info.zOrder = OZONE_PLATFORM_EGLHAISI_NEXUS_ZORDER;

    native_window_ = NXPL_CreateNativeWindowEXT(&window_info);
    if (!native_window_) {
      LOG(ERROR) << "failed to create Nexus native window";
      return;
    }
#endif
  }

  virtual ~SurfaceOzoneEglhaisi() {
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
    if (native_window_) {
      native_window_->Release(native_window_);
    }
    if (layer_) {
      layer_->Release(layer_);
    }
#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
    if (window_) {
      window_->Release(window_);
    }
#endif
#elif defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
    if (native_window_) {
      NXPL_DestroyNativeWindow(native_window_);
    }
#endif
  }

  virtual intptr_t GetNativeWindow()
  {
    return (intptr_t)native_window_;
  }

  virtual bool OnSwapBuffers()
  {
    return true;
  }

  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) {
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined (OZONE_PLATFORM_EGLHAISI_MSTAR)
    DFBResult result, result2;

    result = layer_->SetCooperativeLevel(layer_, DLSCL_ADMINISTRATIVE);
    if (result != DFB_OK) {
      LOG(ERROR) << "cannot change cooperative level of layer: "
          << DirectFBErrorString(result);
      return false;
    }

#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB)
    result = layer_->SetScreenRectangle(layer_, 0, 0,
        viewport_size.width(), viewport_size.height());
    if (result == DFB_UNSUPPORTED) {
      LOG(WARNING) << "cannot change dimensions of layer as operation is not "
          << "supported ";
    } else if (result != DFB_OK) {
      LOG(ERROR) << "cannot change dimensions of layer: "
          << DirectFBErrorString(result);
    }

#elif defined (OZONE_PLATFORM_EGLHAISI_MSTAR)

    // Overwrite layer configuration
    // as mstar drivers doesn't support IDirectFBDisplayLayer::SetScreenRectangle
    DFBDisplayLayerConfig config;
    result = layer_->GetConfiguration(layer_, &config);
    if (result != DFB_OK) {
        LOG(ERROR) << "cannot get configuration of layer: "
            << DirectFBErrorString(result);
    }

    // Set proper width/height
    config.width = viewport_size.width();
    config.height = viewport_size.height();

    result = layer_->SetConfiguration(layer_, &config);
    if (result != DFB_OK) {
        LOG(ERROR) << "cannot set configuration of layer: "
            << DirectFBErrorString(result);
    }

    // Also update window bounds
    result = window_->SetBounds(window_, 0, 0,
        viewport_size.width(), viewport_size.height());
    if (result == DFB_UNSUPPORTED) {
        LOG(WARNING) << "cannot change dimensions of window as operation is not "
            << "supported ";
    } else if (result != DFB_OK) {
        LOG(ERROR) << "cannot change dimensions of window: "
            << DirectFBErrorString(result);
    }
#endif

    result2 = layer_->SetCooperativeLevel(layer_, DLSCL_SHARED);
    if (result2 != DFB_OK) {
      LOG(ERROR) << "cannot restore cooperative level of layer: "
          << DirectFBErrorString(result2);
    }

    return (result == DFB_OK);
#elif defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
    if (native_window_) {
      NXPL_NativeWindowInfoEXT window_info;

      NXPL_GetDefaultNativeWindowInfoEXT(&window_info);

      window_info.x = window_info.y = 0;
      window_info.width = viewport_size.width();
      window_info.height = viewport_size.height();
      window_info.stretch = true;
      window_info.clientID = 1;
      window_info.zOrder = 0;

      NXPL_UpdateNativeWindowEXT(native_window_, &window_info);
    }
    return true;
 #else
    return true;
#endif
  }

  virtual std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() {
    return nullptr;
  }

  virtual void OnSwapBuffersAsync(const SwapCompletionCallback& callback) {
  }

  virtual void* /* EGLConfig */ GetEGLSurfaceConfig(const EglConfigCallbacks& egl) {
    return nullptr;
  }

 private:
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
  IDirectFBDisplayLayer *layer_;
#if defined(OZONE_PLATFORM_EGLHAISI_MSTAR)
  // Use window for MSTAR
  IDirectFBWindow *window_;
#endif
#endif
  /* We use NativeWindowType instead of IDirectFBSurface on purpose as this
   * will raise type-cast errors if EGL does not use DirectFB surfaces. */
  NativeWindowType native_window_;
};

}  // namespace

SurfaceFactoryEglhaisi::SurfaceFactoryEglhaisi()
{
#if defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
  NXPL_RegisterNexusDisplayPlatform(&nxpl_handle, EGL_DEFAULT_DISPLAY);
#endif // OZONE_PLATFORM_EGLHAISI_NEXUS
}

SurfaceFactoryEglhaisi::~SurfaceFactoryEglhaisi()
{
#if defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
  if (nxpl_handle) {
    NXPL_UnregisterNexusDisplayPlatform(nxpl_handle);
    nxpl_handle = NULL;
  }
#endif // OZONE_PLATFORM_EGLHAISI_NEXUS
}

intptr_t SurfaceFactoryEglhaisi::GetNativeDisplay() {
  return (intptr_t)EGL_DEFAULT_DISPLAY;
}

std::unique_ptr<SurfaceOzoneEGL> SurfaceFactoryEglhaisi::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return std::unique_ptr<SurfaceOzoneEGL>(new SurfaceOzoneEglhaisi(widget));
}

bool SurfaceFactoryEglhaisi::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback setprocaddress) {
  base::NativeLibraryLoadError error;
#if defined(OZONE_PLATFORM_EGLHAISI_DIRECTFB) || defined (OZONE_PLATFORM_EGLHAISI_MSTAR)
  base::NativeLibrary gles_library = base::LoadNativeLibrary(
    base::FilePath("libGLESv2.so.2"), &error);

  if (!gles_library) {
    LOG(WARNING) << "Failed to load GLES library: " << error.ToString();
    return false;
  }

  base::NativeLibrary egl_library = base::LoadNativeLibrary(
    base::FilePath("libEGL.so.1"), &error);

  if (!egl_library) {
    LOG(WARNING) << "Failed to load EGL library: " << error.ToString();
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(
              egl_library, "eglGetProcAddress"));

  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  add_gl_library.Run(egl_library);
  add_gl_library.Run(gles_library);
#elif defined(OZONE_PLATFORM_EGLHAISI_NEXUS)
  base::NativeLibrary library = base::LoadNativeLibrary(
    base::FilePath("libv3ddriver.so"), &error);

  if (!library) {
    LOG(WARNING) << "Failed to load v3ddriver library: " << error.ToString();
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(
              library, "eglGetProcAddress"));

  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(library);
    return false;
  }

  add_gl_library.Run(library);
#endif

  setprocaddress.Run(get_proc_address);
  return true;
}

}  // namespace ui
