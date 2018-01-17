#include "ui/ozone/platform/eglhaisi/client_native_pixmap_factory_eglhaisi.h"

#include "ui/ozone/common/stub_client_native_pixmap_factory.h"

namespace ui {

ClientNativePixmapFactory* CreateClientNativePixmapFactoryEglhaisi() {
  return CreateStubClientNativePixmapFactory();
}

}  // namespace ui
