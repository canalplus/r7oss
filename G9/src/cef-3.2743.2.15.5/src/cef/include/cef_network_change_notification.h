#ifndef CEF_INCLUDE_NETWORK_CHANGE_NOTIFIFICATION_H_
# define CEF_INCLUDE_NETWORK_CHANGE_NOTIFIFICATION_H_

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    CONNECTION_UNKNOWN = 0,
    CONNECTION_ETHERNET = 1,
    CONNECTION_WIFI = 2,
    CONNECTION_2G = 3,
    CONNECTION_3G = 4,
    CONNECTION_4G = 5,
    CONNECTION_NONE = 6,
    CONNECTION_BLUETOOTH = 7,
    CONNECTION_LAST = CONNECTION_BLUETOOTH
  } CefConnectionType;

#ifdef __cplusplus
}
#endif

typedef CefConnectionType cef_connection_type_t;

///
// Called by the client to signal that the network type has changed
///
/*--cef()--*/
void CefNetworkTypeChanged(CefConnectionType type);

///
// Called by the client to signal that the IP address has changed
///
/*--cef()--*/
void CefNetworkIPAddressChanged();

#endif /* !CEF_INCLUDE_NETWORK_CHANGE_NOTIFIFICATION.H */
