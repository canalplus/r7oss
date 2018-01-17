#ifndef LIBCEF_BROWSER_NETWORK_CHANGE_NOTIFIER_H_
# define LIBCEF_BROWSER_NETWORK_CHANGE_NOTIFIER_H_

#include "include/cef_base.h"
#include "include/cef_network_change_notification.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"

class CefNetworkChangeNotifier :
  public net::NetworkChangeNotifier,
  public CefBase {
  public:

    CefNetworkChangeNotifier();
    ~CefNetworkChangeNotifier();

    static CefNetworkChangeNotifier *GetInstance();

    virtual net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() const override;

    void ConnectionTypeChanged(CefConnectionType type);
    void IPAddressChanged();

  private :

    static CefNetworkChangeNotifier *instance_;

    CefConnectionType current_type_;

    IMPLEMENT_REFCOUNTING(CefNetworkChangeNotifier);
    DISALLOW_COPY_AND_ASSIGN(CefNetworkChangeNotifier);
};

class CefNetworkChangeNotifierFactory
  : public net::NetworkChangeNotifierFactory {
  public:
    CefNetworkChangeNotifierFactory() {}

    net::NetworkChangeNotifier* CreateInstance() override;
};

#endif /* !LIBCEF_BROWSER_NETWORK_CHANGE_NOTIFIER.H */
