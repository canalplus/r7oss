// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/eglhaisi/ozone_platform_eglhaisi.h"
#include "ui/ozone/platform/eglhaisi/eglhaisi_surface_factory.h"

#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_ime_controller.h"

#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"

#include "ui/events/ozone/events_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

#include "ui/aura/test/test_screen.h"

#include "base/logging.h"

#include <iostream>

namespace ui {

  namespace {

    class EglhaisiDesktopFactoryOzone :
        public views::DesktopFactoryOzone {

      public:
        EglhaisiDesktopFactoryOzone() {
          views::DesktopFactoryOzone::SetInstance(this);
        }

        ~EglhaisiDesktopFactoryOzone() {
          views::DesktopFactoryOzone::SetInstance(NULL);
        }

        views::DesktopWindowTreeHost* CreateWindowTreeHost(
          views::internal::NativeWidgetDelegate* native_widget_delegate,
          views::DesktopNativeWidgetAura* desktop_native_widget_aura) override {
          return views::DesktopWindowTreeHost::Create(native_widget_delegate, desktop_native_widget_aura);
        }

        display::Screen* CreateDesktopScreen() override {
          aura::TestScreen* screen = aura::TestScreen::Create(gfx::Size());
          display::Screen::SetScreenInstance(screen);
          return screen;
        }
    };

    class EglhaisiImeController :
        public PlatformImeController {

      public:
        EglhaisiImeController() {}
        ~EglhaisiImeController() {}

        void UpdateTextInputState(const TextInputState&) {
          // Nothing to do here
        }

        void SetImeVisibility(bool visible) {
          // Nothing to do here
        }
    };

    class EglhaisiWindow :
        public PlatformWindow,
        public PlatformEventDispatcher {
      public:
        static int current_id_;

        static gfx::AcceleratedWidget GetWindowId() {
          current_id_++;
          return (gfx::AcceleratedWidget)current_id_;
        };

        EglhaisiWindow(PlatformWindowDelegate* delegate,
                       EventFactoryEvdev* event_factory,
                       const gfx::Rect& bounds)
          : delegate_(delegate),
            event_factory_(event_factory),
            bounds_(bounds),
            window_id_(EglhaisiWindow::GetWindowId())
        {
          delegate_->OnAcceleratedWidgetAvailable(window_id_, 1.f);
          ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
          ime_controller_.reset(new EglhaisiImeController());
        }

        ~EglhaisiWindow() {};

        // PlatformWindow:
        gfx::Rect GetBounds() {
          return bounds_;
        }

        void SetBounds(const gfx::Rect& bounds) {
          bounds_ = bounds;
          delegate_->OnBoundsChanged(bounds);
        }

        void SetTitle(const base::string16& title) {}
        void Show() {}
        void Hide() {}
        void Close() {}
        void SetCapture() {}
        void ReleaseCapture() {}
        void ToggleFullscreen() {}
        void Maximize() {}
        void Minimize() {}
        void Restore() {}
        void SetCursor(PlatformCursor cursor) {}

        void MoveCursorTo(const gfx::Point& location) {
          event_factory_->WarpCursorTo(window_id_, gfx::PointF(location));
        }

        void ConfineCursorToBounds(const gfx::Rect& bounds) {}

        // PlatformEventDispatcher:
        bool CanDispatchEvent(const PlatformEvent& event) {
          return true;
        }

        uint32_t DispatchEvent(const PlatformEvent& native_event) {
          DispatchEventFromNativeUiEvent(
            native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                                     base::Unretained(delegate_)));

          return ui::POST_DISPATCH_STOP_PROPAGATION;
        }

        PlatformImeController* GetPlatformImeController() {
          return ime_controller_.get();
        }

      private:
        PlatformWindowDelegate* delegate_;
        EventFactoryEvdev* event_factory_;
        gfx::Rect bounds_;
        gfx::AcceleratedWidget window_id_;
        std::unique_ptr<EglhaisiImeController> ime_controller_;

        DISALLOW_COPY_AND_ASSIGN(EglhaisiWindow);
    };

    int EglhaisiWindow::current_id_ = 0;

    class OzonePlatformEglhaisi : public OzonePlatform {
      public:
        OzonePlatformEglhaisi() {
        }

        virtual ~OzonePlatformEglhaisi() {
        }

        // OzonePlatform:
        virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() {
          return surface_factory_ozone_.get();
        }

        OverlayManagerOzone* GetOverlayManager() {
          return overlay_manager_.get();
        }

        virtual CursorFactoryOzone* GetCursorFactoryOzone() {
          return cursor_factory_ozone_.get();
        }

        virtual GpuPlatformSupport* GetGpuPlatformSupport() {
          return gpu_platform_support_.get();
        }
        virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() {
          return gpu_platform_support_host_.get();
        }

        virtual std::unique_ptr<ui::NativeDisplayDelegate> CreateNativeDisplayDelegate() {
          return std::unique_ptr<ui::NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
        }

        base::ScopedFD OpenClientNativePixmapDevice() const {
          return base::ScopedFD();
        }

        virtual void InitializeUI() {
          device_manager_ = CreateDeviceManager();
          KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
            std::unique_ptr<KeyboardLayoutEngine>(new StubKeyboardLayoutEngine()));
          event_factory_ozone_.reset(
            new EventFactoryEvdev(NULL, device_manager_.get(), KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));
          overlay_manager_.reset(new StubOverlayManager());
          surface_factory_ozone_.reset(new SurfaceFactoryEglhaisi());
          cursor_factory_ozone_.reset(new CursorFactoryOzone());
          gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
        }

        virtual void InitializeGPU() {
          if(!surface_factory_ozone_)
            surface_factory_ozone_.reset(new SurfaceFactoryEglhaisi());
          gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
        }

        virtual ui::InputController* GetInputController() {
          return event_factory_ozone_->input_controller();
        }

        virtual std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() {
          return event_factory_ozone_->CreateSystemInputInjector();
        }

        virtual std::unique_ptr<PlatformWindow> CreatePlatformWindow(
          PlatformWindowDelegate* delegate,
          const gfx::Rect& bounds) {
          return std::unique_ptr<PlatformWindow>(new EglhaisiWindow(delegate, event_factory_ozone_.get(), bounds));
        }

      private:

        std::unique_ptr<DeviceManager> device_manager_;
        std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;
        std::unique_ptr<SurfaceFactoryEglhaisi> surface_factory_ozone_;
        std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
        std::unique_ptr<GpuPlatformSupport> gpu_platform_support_;
        std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
        std::unique_ptr<OverlayManagerOzone> overlay_manager_;
        EglhaisiDesktopFactoryOzone desktop_factory_;

        DISALLOW_COPY_AND_ASSIGN(OzonePlatformEglhaisi);
    };

  }  // namespace

  OzonePlatform* CreateOzonePlatformEglhaisi() {
    OzonePlatformEglhaisi* platform = new OzonePlatformEglhaisi;
    return platform;
  }

}  // namespace ui
