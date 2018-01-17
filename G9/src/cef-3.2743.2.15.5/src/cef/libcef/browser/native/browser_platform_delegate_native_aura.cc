#include "libcef/browser/native/browser_platform_delegate_native_aura.h"

// Include this first to avoid type conflict errors.
#include "base/tracked_objects.h"
#undef Status

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <iostream>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/window_delegate_view.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/renderer_preferences.h"

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "ui/gfx/font_render_params.h"
#include "ui/display/screen.h"

#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_window_tree_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"

#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"

#include "ui/wm/core/default_activation_client.h"

#include "include/cef_command_line.h"

#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720

namespace {

  bool GetFramebufferSize(const std::string & fbdev_path, gfx::Size& size) {
    struct fb_var_screeninfo fbinfo;
    int fbdev = open(fbdev_path.c_str(), O_RDWR);
    int res = 0;

    if (fbdev < 0) {
      LOG(ERROR) << "Cannot open framebuffer " << fbdev_path;
      return false;
    }
    res = ioctl(fbdev, FBIOGET_VSCREENINFO, &fbinfo);
    close(fbdev);
    if (res < 0) {
      LOG(ERROR) << "Cannot get virtual size of framebuffer " << fbdev_path;
      return false;
    }

    size.SetSize(fbinfo.xres, fbinfo.yres);

    return true;
  }

  long GetSystemUptime() {
    struct sysinfo info;
    if (sysinfo(&info) == 0)
      return info.uptime;
    return 0;
  }

  class FillLayout : public aura::LayoutManager {
    public:
      explicit FillLayout(aura::Window* root)
        : root_(root) {
      }

      virtual ~FillLayout() {}

    private:
      // aura::LayoutManager:
      virtual void OnWindowResized() {
      }

      virtual void OnWindowAddedToLayout(aura::Window* child) {
        child->SetBounds(root_->bounds());
      }

      virtual void OnWillRemoveWindowFromLayout(aura::Window* child) {
      }

      virtual void OnWindowRemovedFromLayout(aura::Window* child) {
      }

      virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                                  bool visible) {
      }

      virtual void SetChildBounds(aura::Window* child,
                                  const gfx::Rect& requested_bounds) {
        SetChildBoundsDirect(child, requested_bounds);
      }

      aura::Window* root_;

      DISALLOW_COPY_AND_ASSIGN(FillLayout);
  };

  /*class MinimalInputEventFilter : public ui::internal::InputMethodDelegate,
                                  public ui::EventHandler {
    public:
      explicit MinimalInputEventFilter(aura::WindowTreeHost* host)
        : host_(host),
          input_method_(host->GetInputMethod()) {
        host_->window()->AddPreTargetHandler(this);
        host_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                     input_method_.get());
      }

      virtual ~MinimalInputEventFilter() {
        host_->window()->RemovePreTargetHandler(this);
        host_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                     static_cast<ui::InputMethod*>(NULL));
      }

    private:
      // ui::EventHandler:
      virtual void OnKeyEvent(ui::KeyEvent* event) {
        // See the comment in InputMethodEventFilter::OnKeyEvent() for details.
        if (event->IsTranslated()) {
          event->SetTranslated(false);
        } else {
          if (input_method_->DispatchKeyEvent(*event))
            event->StopPropagation();
        }
      }

      // ui::internal::InputMethodDelegate:
      virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) {
        // See the comment in InputMethodEventFilter::DispatchKeyEventPostIME() for
        // details.
        ui::KeyEvent aura_event(event);
        aura_event.SetTranslated(true);
        ui::EventDispatchDetails details =
          host_->dispatcher()->OnEventFromSource(&aura_event);
        return aura_event.handled() || details.dispatcher_destroyed;
      }

      aura::WindowTreeHost* host_;
      std::unique_ptr<ui::InputMethod> input_method_;

      DISALLOW_COPY_AND_ASSIGN(MinimalInputEventFilter);
      };*/

}

std::unique_ptr<aura::WindowTreeHost> CefBrowserPlatformDelegateNativeAura::host_;
std::unique_ptr<aura::client::FocusClient> CefBrowserPlatformDelegateNativeAura::focus_client_;
std::unique_ptr<aura::client::DefaultCaptureClient> CefBrowserPlatformDelegateNativeAura::capture_client_;
std::unique_ptr<aura::client::WindowTreeClient> CefBrowserPlatformDelegateNativeAura::window_tree_client_;

CefBrowserPlatformDelegateNativeAura::CefBrowserPlatformDelegateNativeAura(
  const CefWindowInfo& window_info)
  : CefBrowserPlatformDelegateNative(window_info),
    host_window_created_(false),
    window_(NULL)
{
}

void CefBrowserPlatformDelegateNativeAura::BrowserDestroyed(CefBrowserHostImpl* browser)
{
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);

  if (host_window_created_)
    browser->Release();
}

void CefBrowserPlatformDelegateNativeAura::InitHostPlatform(gfx::Size& size)
{
  aura::TestScreen* screen = aura::TestScreen::Create(size);
  display::Screen::SetScreenInstance(screen);

  host_.reset(aura::WindowTreeHost::Create(gfx::Rect(size)));
  host_->InitHost();
  host_->window()->SetLayoutManager(new ::FillLayout(host_->window()));

  focus_client_.reset(new aura::test::TestFocusClient());
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  new wm::DefaultActivationClient(host_->window());
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));
  window_tree_client_.reset(
      new aura::test::TestWindowTreeClient(host_->window()));
  //ime_filter_.reset(new ::MinimalInputEventFilter(host_.get()));
}

bool CefBrowserPlatformDelegateNativeAura::CreateHostWindow()
{
  if (!host_.get()) {
    CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
    gfx::Size window_size(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

    if (command_line->HasSwitch(switches::kHostWindowSize)) {
      bool window_size_valid = true;
      std::string window_size_str = command_line->GetSwitchValue(
          switches::kHostWindowSize);

      /* fb:/dev/fbX */
      if (window_size_str.compare(0, 3, "fb:") == 0) {
        const std::string fbdev_path = window_size_str.substr(3);

        if (fbdev_path.empty()) {
          window_size_valid = false;
        } else {
          LOG(INFO) << "Using size of framebuffer " << fbdev_path;
          if (!::GetFramebufferSize(fbdev_path, window_size))
            return false;
        }
      }
      /* width,height */
      else {
        int width, height;

        if (sscanf(window_size_str.c_str(), "%d,%d", &width, &height) == 2 &&
            width > 0 && height > 0) {
          window_size.SetSize(width, height);
        } else {
          window_size_valid = false;
        }
      }

      if (!window_size_valid) {
        LOG(ERROR) << "Ignoring invalid window size " << window_size_str;
      }
    }

    LOG(INFO) << "Window size: " << window_size.width() << 'x'
        << window_size.height();

    InitHostPlatform(window_size);
  }

  gfx::Rect rect(host_->GetBounds());

  window_ = browser_->web_contents()->GetNativeView();

  // Set z index as user data
  window_->set_user_data(&window_info_);

  aura::Window* parent = host_->window();
  if (!parent->Contains(window_))
    parent->AddChild(window_);

  // Reorganize windows depending on z-depth
  aura::Window::Windows::const_iterator ite;
  for (ite=parent->children().begin(); ite!=parent->children().end(); ite++) {
      // For all other child window than the one we just added
      if ((*ite) != window_) {
          CefWindowInfo* winfo = (CefWindowInfo*)(*ite)->user_data();
          // Added window depth is lower than current one, add it below
          if (window_info_.z_index < winfo->z_index) {
              parent->StackChildBelow(window_, (*ite));
              break;
          }
      }
  }
  // Reached the end ? child hasn't been stacked, set it on top
  if (ite==parent->children().end()) {
      parent->StackChildAtTop(window_);
  }

  window_info_.window = reinterpret_cast<CefWindowHandle>(window_);
  window_info_.SetAsChild(reinterpret_cast<CefWindowHandle>(parent),
                          CefRect(rect.x(), rect.y(), rect.width(), rect.height()));
  host_window_created_ = true;

  browser_->AddRef();

  //const CefSettings& settings = CefContext::Get()->settings();
  //SkColor background_color = settings.background_color;

  // CefAuraWindowDelegateView* delegate_view =
  //   new CefAuraWindowDelegateView(background_color);

  // delegate_view->Init(window_info_.window,
  //                     browser_->web_contents(),
  //                     gfx::Rect(gfx::Point(), rect.size()));

  // window_widget_.reset(delegate_view->GetWidget());
  // window_widget_->Show();
  window_->Show();
  host_->Show();

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  content::RendererPreferences* prefs =
    browser_->web_contents()->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

  // Set font-related attributes.
  CR_DEFINE_STATIC_LOCAL(const gfx::FontRenderParams, params,
                         (gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), NULL)));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;

  browser_->web_contents()->GetRenderViewHost()->SyncRendererPrefs();

  return true;
}

void CefBrowserPlatformDelegateNativeAura::CloseHostWindow()
{
  /*
    FIXME : Maybe we should do something like :
    if (window_)
    window_->Close();
  */
  if (window_) {
    aura::Window* parent = host_->window();
    if (parent->Contains(window_)) {
        parent->RemoveChild(window_);
    }
  }
}

CefWindowHandle CefBrowserPlatformDelegateNativeAura::GetHostWindowHandle() const
{
  if (windowless_handler_)
    return windowless_handler_->GetParentWindowHandle();
  return window_info_.window;
}


views::Widget* CefBrowserPlatformDelegateNativeAura::GetWindowWidget() const
{
  return NULL;
}

void CefBrowserPlatformDelegateNativeAura::SendFocusEvent(bool focus)
{
  if (!focus)
    return;

  if (browser_->web_contents())
    browser_->web_contents()->Focus();
}

void CefBrowserPlatformDelegateNativeAura::NotifyMoveOrResizeStarted()
{
  CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted();
  content::RenderWidgetHostImpl::From(
    browser_->web_contents()->GetRenderViewHost()->GetWidget())->
    SendScreenRects();
}

void CefBrowserPlatformDelegateNativeAura::SizeTo(int width, int height)
{
  /* Nothing special to do here */
}

gfx::Point CefBrowserPlatformDelegateNativeAura::GetScreenPoint(
  const gfx::Point& view) const
{
  if (windowless_handler_)
    return windowless_handler_->GetParentScreenPoint(view);

  const gfx::Rect& bounds_in_screen = reinterpret_cast<aura::Window*>(window_info_.parent_window)->GetBoundsInScreen();
  return gfx::Point(bounds_in_screen.x() + view.x(),
                    bounds_in_screen.y() + view.y());
}

void CefBrowserPlatformDelegateNativeAura::ViewText(const std::string& text)
{
  CEF_REQUIRE_UIT();

  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1)
    return;

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput)
    return;

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0)
    return;

  std::string openCommand("xdg-open ");
  openCommand += newName;

  if (system(openCommand.c_str()) != 0)
    std::cerr << "Command '" << openCommand << "' failed !" << std::endl;
}

void CefBrowserPlatformDelegateNativeAura::HandleKeyboardEvent(
  const content::NativeWebKeyboardEvent& event)
{
  /* Probably nothing to do here */
}

void CefBrowserPlatformDelegateNativeAura::HandleExternalProtocol(const GURL& url)
{
  /* A lot of fun stuff could be done here ! */
}

void CefBrowserPlatformDelegateNativeAura::TranslateKeyEvent(
  content::NativeWebKeyboardEvent& result,
  const CefKeyEvent& key_event) const
{
  result.timeStampSeconds = ::GetSystemUptime();

  result.windowsKeyCode = key_event.windows_key_code;
  result.nativeKeyCode = key_event.native_key_code;
  result.isSystemKey = key_event.is_system_key ? 1 : 0;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      result.type = blink::WebInputEvent::RawKeyDown;
      break;
    case KEYEVENT_KEYUP:
      result.type = blink::WebInputEvent::KeyUp;
      break;
    case KEYEVENT_CHAR:
      result.type = blink::WebInputEvent::Char;
      break;
    default:
      NOTREACHED();
  }

  result.text[0] = key_event.character;
  result.unmodifiedText[0] = key_event.unmodified_character;

  result.setKeyIdentifierFromWindowsKeyCode();

  result.modifiers |= TranslateModifiers(key_event.modifiers);
}

void CefBrowserPlatformDelegateNativeAura::TranslateClickEvent(
  blink::WebMouseEvent& result,
  const CefMouseEvent& mouse_event,
  CefBrowserHost::MouseButtonType type,
  bool mouseUp, int clickCount) const
{
  TranslateMouseEvent(result, mouse_event);

  switch (type) {
    case MBT_LEFT:
      result.type = mouseUp ? blink::WebInputEvent::MouseUp :
      blink::WebInputEvent::MouseDown;
      result.button = blink::WebMouseEvent::ButtonLeft;
      break;
    case MBT_MIDDLE:
      result.type = mouseUp ? blink::WebInputEvent::MouseUp :
      blink::WebInputEvent::MouseDown;
      result.button = blink::WebMouseEvent::ButtonMiddle;
      break;
    case MBT_RIGHT:
      result.type = mouseUp ? blink::WebInputEvent::MouseUp :
      blink::WebInputEvent::MouseDown;
      result.button = blink::WebMouseEvent::ButtonRight;
      break;
    default:
      NOTREACHED();
  }

  result.clickCount = clickCount;
}

void CefBrowserPlatformDelegateNativeAura::TranslateMoveEvent(
  blink::WebMouseEvent& result,
  const CefMouseEvent& mouse_event,
  bool mouseLeave) const
{
  TranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.type = blink::WebInputEvent::MouseMove;
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonRight;
    else
      result.button = blink::WebMouseEvent::ButtonNone;
  } else {
    result.type = blink::WebInputEvent::MouseLeave;
    result.button = blink::WebMouseEvent::ButtonNone;
  }

  result.clickCount = 0;
}

void CefBrowserPlatformDelegateNativeAura::TranslateWheelEvent(
  blink::WebMouseWheelEvent& result,
  const CefMouseEvent& mouse_event,
  int deltaX, int deltaY) const
{
  result = blink::WebMouseWheelEvent();
  TranslateMouseEvent(result, mouse_event);

  result.type = blink::WebInputEvent::MouseWheel;

  static const double scrollbarPixelsPerGtkTick = 40.0;
  result.deltaX = deltaX;
  result.deltaY = deltaY;
  result.wheelTicksX = result.deltaX / scrollbarPixelsPerGtkTick;
  result.wheelTicksY = result.deltaY / scrollbarPixelsPerGtkTick;
  result.hasPreciseScrollingDeltas = true;

  // Unless the phase and momentumPhase are passed in as parameters to this
  // function, there is no way to know them
  result.phase = blink::WebMouseWheelEvent::PhaseNone;
  result.momentumPhase = blink::WebMouseWheelEvent::PhaseNone;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonRight;
  else
    result.button = blink::WebMouseEvent::ButtonNone;
}

void CefBrowserPlatformDelegateNativeAura::TranslateMouseEvent(
  blink::WebMouseEvent& result,
  const CefMouseEvent& mouse_event) const
{
  // position
  result.x = mouse_event.x;
  result.y = mouse_event.y;
  result.windowX = result.x;
  result.windowY = result.y;

  const gfx::Point& screen_pt = GetScreenPoint(gfx::Point(result.x, result.y));
  result.globalX = screen_pt.x();
  result.globalY = screen_pt.y();

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp
  result.timeStampSeconds = ::GetSystemUptime();
}

CefEventHandle CefBrowserPlatformDelegateNativeAura::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  if (!event.os_event)
    return NULL;
  return const_cast<CefEventHandle>(event.os_event->native_event());
}

std::unique_ptr<CefMenuRunner> CefBrowserPlatformDelegateNativeAura::CreateMenuRunner() {
  return NULL;
}
