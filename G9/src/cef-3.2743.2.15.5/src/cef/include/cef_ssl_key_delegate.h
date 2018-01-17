#ifndef CEF_SSL_KEY_DELEGATE_H_
# define CEF_SSL_KEY_DELEGATE_H_

#include "include/cef_base.h"

#ifdef __cplusplus
extern "C" {
#endif

 typedef enum {
    CEF_MD5_SHA1_HASH_TYPE,
    CEF_SHA1_HASH_TYPE,
    CEF_SHA256_HASH_TYPE,
    CEF_SHA384_HASH_TYPE,
    CEF_SHA512_HASH_TYPE,
  } cef_hash_type_t;

  typedef enum Type {
    CEF_RSA_KEY_TYPE,
    CEF_ECDSA_KEY_TYPE,
  } cef_key_type_t;

#ifdef __cplusplus
}
#endif

///
// Class used to delegate to the client SSL key manipulation
///
/*--cef(source=client)--*/
class CefSSLKeyDelegate : public virtual CefBase {
  public:

    ///
    // Return the hash types supported by the client
    ///
    /*--cef()--*/
    virtual void GetDigestPreferences(cef_hash_type_t **supported, int *count) = 0;

    ///
    // Return the maximum length of the signature in bytes
    ///
    /*--cef()--*/
    virtual size_t GetMaxSignatureLengthInBytes() = 0;

    ///
    // Used by the library to sign a digest
    ///
    /*--cef()--*/
    virtual int SignDigest(cef_key_type_t key_type, cef_hash_type_t hash_type, const uint8* digest, const uint32 digest_len, uint8 *sig, uint32 *sig_len) = 0;
};

#endif /* !CEF_SSL_KEY_DELEGATE.H */
