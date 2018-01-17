#include "network_change_notifier.h"

CefNetworkChangeNotifier *CefNetworkChangeNotifier::instance_ = NULL;

CefNetworkChangeNotifier *CefNetworkChangeNotifier::GetInstance()
{
  if (!instance_)
    instance_ = new CefNetworkChangeNotifier();
  return instance_;
}

CefNetworkChangeNotifier::CefNetworkChangeNotifier() {
}

CefNetworkChangeNotifier::~CefNetworkChangeNotifier() {
}

void CefNetworkChangeNotifier::ConnectionTypeChanged(CefConnectionType type) {
  current_type_ = type;
  net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

void CefNetworkChangeNotifier::IPAddressChanged() {
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
}

net::NetworkChangeNotifier::ConnectionType CefNetworkChangeNotifier::GetCurrentConnectionType() const {
  return static_cast<net::NetworkChangeNotifier::ConnectionType>(current_type_);
}

net::NetworkChangeNotifier* CefNetworkChangeNotifierFactory::CreateInstance() {
  return CefNetworkChangeNotifier::GetInstance();
}

void CefNetworkTypeChanged(CefConnectionType type) {
  CefNetworkChangeNotifier *notifier = CefNetworkChangeNotifier::GetInstance();
  if (notifier)
    notifier->ConnectionTypeChanged(type);
}

void CefNetworkIPAddressChanged() {
  CefNetworkChangeNotifier *notifier = CefNetworkChangeNotifier::GetInstance();
  if (notifier)
    notifier->IPAddressChanged();
}
