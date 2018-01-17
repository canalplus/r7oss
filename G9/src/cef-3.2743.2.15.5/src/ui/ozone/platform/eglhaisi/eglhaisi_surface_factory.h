// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_EGLHAISI_EGLHAISI_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_EGLHAISI_EGLHAISI_SURFACE_FACTORY_H_

#include "ui/ozone/public/surface_factory_ozone.h"

namespace gfx {
class SurfaceOzone;
}

namespace ui {

class SurfaceFactoryEglhaisi : public ui::SurfaceFactoryOzone {
 public:
  SurfaceFactoryEglhaisi();
  virtual ~SurfaceFactoryEglhaisi();

  // SurfaceFactoryOzone:
  virtual intptr_t GetNativeDisplay();
  virtual std::unique_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget);
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback setprocaddress);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_EGLHAISI_EGLHAISI_SURFACE_FACTORY_H_
