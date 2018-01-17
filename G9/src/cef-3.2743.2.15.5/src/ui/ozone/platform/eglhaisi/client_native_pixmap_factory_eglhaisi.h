#ifndef UI_OZONE_PLATFORM_EGLHAISI_CLIENT_NATIVE_PIXMAP_FACTORY_EGLHAISI_H_
#define UI_OZONE_PLATFORM_EGLHAISI_CLIENT_NATIVE_PIXMAP_FACTORY_EGLHAISI_H_

namespace ui {

class ClientNativePixmapFactory;

// Constructor hook for use in constructor_list.cc
ClientNativePixmapFactory* CreateClientNativePixmapFactoryEglhaisi();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_EGLHAISI_CLIENT_NATIVE_PIXMAP_FACTORY_EGLHAISI_H_
