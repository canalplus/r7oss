#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA_H_
# define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA_H_

#include "libcef/browser/native/browser_platform_delegate_native.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/views/widget/widget.h"

class CefBrowserPlatformDelegateNativeAura : public CefBrowserPlatformDelegateNative
{
  public:
    explicit CefBrowserPlatformDelegateNativeAura(
      const CefWindowInfo& window_info);

    void BrowserDestroyed(CefBrowserHostImpl* browser) override;
    bool CreateHostWindow() override;
    void CloseHostWindow() override;
    CefWindowHandle GetHostWindowHandle() const override;
    views::Widget* GetWindowWidget() const override;
    void SendFocusEvent(bool setFocus) override;
    void NotifyMoveOrResizeStarted() override;
    void SizeTo(int width, int height) override;
    gfx::Point GetScreenPoint(const gfx::Point& view) const override;
    void ViewText(const std::string& text) override;
    void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
    void HandleExternalProtocol(const GURL& url) override;
    void TranslateKeyEvent(content::NativeWebKeyboardEvent& result,
                           const CefKeyEvent& key_event) const override;
    void TranslateClickEvent(blink::WebMouseEvent& result,
                             const CefMouseEvent& mouse_event,
                             CefBrowserHost::MouseButtonType type,
                             bool mouseUp, int clickCount) const override;
    void TranslateMoveEvent(blink::WebMouseEvent& result,
                            const CefMouseEvent& mouse_event,
                            bool mouseLeave) const override;
    void TranslateWheelEvent(blink::WebMouseWheelEvent& result,
                             const CefMouseEvent& mouse_event,
                             int deltaX, int deltaY) const override;
    CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
    std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;

  private:
    void TranslateMouseEvent(blink::WebMouseEvent& result,
                             const CefMouseEvent& mouse_event) const;

    void InitHostPlatform(gfx::Size& size);

    bool host_window_created_;

    //std::unique_ptr<views::Widget> window_widget_;
    aura::Window * window_;
    static std::unique_ptr<aura::WindowTreeHost> host_;
    static std::unique_ptr<aura::client::FocusClient> focus_client_;
    static std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
    static std::unique_ptr<aura::client::WindowTreeClient> window_tree_client_;
    //std::unique_ptr<ui::EventHandler> ime_filter_;
};


#endif /* !CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA.H */
