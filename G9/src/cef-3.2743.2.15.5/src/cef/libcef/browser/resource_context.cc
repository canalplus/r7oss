// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_context.h"

#include "libcef/browser/net/url_request_context_getter.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_platform_key.h"
#include "net/ssl/client_cert_store.h"

#if defined(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#endif

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif

#if defined(USE_OPENSSL_CERTS)
#include <openssl/digest.h>
#include <openssl/evp.h>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "crypto/scoped_openssl_types.h"
#include "net/base/net_errors.h"
#include "net/ssl/openssl_client_key_store.h"
#include "net/ssl/ssl_platform_key_task_runner.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"

#include "include/cef_app.h"
#include "libcef/common/content_client.h"
#include "include/cef_ssl_key_delegate.h"
#endif

CefResourceContext::CefResourceContext(
    bool is_off_the_record,
    extensions::InfoMap* extension_info_map,
    CefRefPtr<CefRequestContextHandler> handler)
    : is_off_the_record_(is_off_the_record),
      extension_info_map_(extension_info_map),
      handler_(handler) {
}

CefResourceContext::~CefResourceContext() {
  if (getter_.get()) {
    // When the parent object (ResourceContext) destructor executes all
    // associated URLRequests should be destroyed. If there are no other
    // references it should then be safe to destroy the URLRequestContextGetter
    // which owns the URLRequestContext.
    getter_->AddRef();
    CefURLRequestContextGetter* raw_getter = getter_.get();
    getter_ = NULL;
    content::BrowserThread::ReleaseSoon(
          content::BrowserThread::IO, FROM_HERE, raw_getter);
  }
}

net::HostResolver* CefResourceContext::GetHostResolver() {
  CHECK(getter_.get());
  return getter_->GetHostResolver();
}

net::URLRequestContext* CefResourceContext::GetRequestContext() {
  CHECK(getter_.get());
  return getter_->GetURLRequestContext();
}

std::unique_ptr<net::ClientCertStore> CefResourceContext::CreateClientCertStore() {
#if defined(USE_NSS_CERTS)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory()));
#elif defined(OS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MACOSX)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#elif defined(USE_OPENSSL_CERTS)
  // OpenSSL does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return std::unique_ptr<net::ClientCertStore>();
#else
#error Unknown platform.
#endif
}

void CefResourceContext::set_url_request_context_getter(
    scoped_refptr<CefURLRequestContextGetter> getter) {
  DCHECK(!getter_.get());
  getter_ = getter;
}

#if defined(USE_OPENSSL_CERTS)
static inline net::SSLPrivateKey::Type CefKeyTypeToChromiumKeyType(cef_key_type_t type)
{
  switch (type)
  {
    case CEF_RSA_KEY_TYPE:
      return net::SSLPrivateKey::Type::RSA;
    case CEF_ECDSA_KEY_TYPE:
      return net::SSLPrivateKey::Type::ECDSA;
  }

  return net::SSLPrivateKey::Type::RSA;
}

static inline cef_key_type_t ChromiumKeyTypeToCefKeyType(net::SSLPrivateKey::Type type)
{
  switch (type)
  {
    case net::SSLPrivateKey::Type::RSA:
      return CEF_RSA_KEY_TYPE;
    case net::SSLPrivateKey::Type::ECDSA:
      return CEF_ECDSA_KEY_TYPE;
  }

  return CEF_RSA_KEY_TYPE;
}

static inline net::SSLPrivateKey::Hash CefHashTypeToChromiumHashType(cef_hash_type_t type)
{
  switch (type)
  {
    case CEF_MD5_SHA1_HASH_TYPE:
      return net::SSLPrivateKey::Hash::MD5_SHA1;
    case CEF_SHA1_HASH_TYPE:
      return net::SSLPrivateKey::Hash::SHA1;
    case CEF_SHA256_HASH_TYPE:
      return net::SSLPrivateKey::Hash::SHA256;
    case CEF_SHA384_HASH_TYPE:
      return net::SSLPrivateKey::Hash::SHA384;
    case CEF_SHA512_HASH_TYPE:
      return net::SSLPrivateKey::Hash::SHA512;
  }

  return net::SSLPrivateKey::Hash::MD5_SHA1;
}

static inline cef_hash_type_t ChromiumHashTypeToCefHashType(net::SSLPrivateKey::Hash type)
{
  switch (type)
  {
    case net::SSLPrivateKey::Hash::MD5_SHA1:
      return CEF_MD5_SHA1_HASH_TYPE;
    case net::SSLPrivateKey::Hash::SHA1:
      return CEF_SHA1_HASH_TYPE;
    case net::SSLPrivateKey::Hash::SHA256:
      return CEF_SHA256_HASH_TYPE;
    case net::SSLPrivateKey::Hash::SHA384:
      return CEF_SHA384_HASH_TYPE;
    case net::SSLPrivateKey::Hash::SHA512:
      return CEF_SHA512_HASH_TYPE;
  }

  return CEF_MD5_SHA1_HASH_TYPE;
}

namespace {

  class CefOpenSSLPlatformKey : public net::ThreadedSSLPrivateKey::Delegate {
    public:
      CefOpenSSLPlatformKey(crypto::ScopedEVP_PKEY key, net::SSLPrivateKey::Type type, CefRefPtr<CefSSLKeyDelegate> delegate)
        : key_(std::move(key)), type_(type), delegate_(delegate) {}

      ~CefOpenSSLPlatformKey() override {}

      net::SSLPrivateKey::Type GetType() override { return type_; }

      std::vector<net::SSLPrivateKey::Hash> GetDigestPreferences() override {
        if (delegate_.get() && !key_.get())
        {
          // We have a delegate, let it handle this
          cef_hash_type_t *hashes = NULL;
          int count = 0;
          std::vector<net::SSLPrivateKey::Hash> res;

          delegate_->GetDigestPreferences(&hashes, &count);

          if (hashes != NULL && count > 0)
            for (int i = 0; i < count; i++)
              res.push_back(CefHashTypeToChromiumHashType(hashes[i]));

          return res;
        }

        /* Fallback on legacy behavior */
        static const net::SSLPrivateKey::Hash kHashes[] = {
          net::SSLPrivateKey::Hash::SHA512, net::SSLPrivateKey::Hash::SHA384,
          net::SSLPrivateKey::Hash::SHA256, net::SSLPrivateKey::Hash::SHA1
        };
        return std::vector<net::SSLPrivateKey::Hash>(kHashes,
                                                     kHashes + arraysize(kHashes));
      }

      size_t GetMaxSignatureLengthInBytes() override {
        if (delegate_.get() && !key_.get())
          // We have a delegate, let it handle this
          return delegate_->GetMaxSignatureLengthInBytes();

        /* Fallback on legacy behavior */
        return EVP_PKEY_size(key_.get());
      }

      net::Error SignDigest(net::SSLPrivateKey::Hash hash,
                            const base::StringPiece& input,
                            std::vector<uint8_t>* signature) override {
        const uint8_t* input_ptr = reinterpret_cast<const uint8_t*>(input.data());
        size_t input_len = input.size();

        if (delegate_.get() && !key_.get())
        {
          // We have a delegate and no key, let it handle this
          uint32_t sig_len = GetMaxSignatureLengthInBytes();
          uint8_t* sig_buf = new uint8_t[sig_len];

          if (delegate_->SignDigest(ChromiumKeyTypeToCefKeyType(type_),
                                   ChromiumHashTypeToCefHashType(hash),
                                   input_ptr,
                                   input_len,
                                   sig_buf,
                                   &sig_len) != 0)
            return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;

          signature->insert(signature->end(), sig_buf, sig_buf + sig_len);

          delete sig_buf;
        }
        else
        {
          /* Fallback on legacy behavior */
          crypto::ScopedEVP_PKEY_CTX ctx =
            crypto::ScopedEVP_PKEY_CTX(EVP_PKEY_CTX_new(key_.get(), NULL));

          if (!ctx)
            return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;

          if (!EVP_PKEY_sign_init(ctx.get()))
            return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;

          if (type_ == net::SSLPrivateKey::Type::RSA) {
            const EVP_MD* digest = nullptr;
            switch (hash) {
              case net::SSLPrivateKey::Hash::MD5_SHA1:
                digest = EVP_md5_sha1();
                break;
              case net::SSLPrivateKey::Hash::SHA1:
                digest = EVP_sha1();
                break;
              case net::SSLPrivateKey::Hash::SHA256:
                digest = EVP_sha256();
                break;
              case net::SSLPrivateKey::Hash::SHA384:
                digest = EVP_sha384();
                break;
              case net::SSLPrivateKey::Hash::SHA512:
                digest = EVP_sha512();
                break;
              default:
                return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
            }

            DCHECK(digest);

            if (!EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING))
              return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;

            if (!EVP_PKEY_CTX_set_signature_md(ctx.get(), digest))
              return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
          }

          size_t sig_len = 0;

          if (!EVP_PKEY_sign(ctx.get(), NULL, &sig_len, input_ptr, input_len))
            return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;

          signature->resize(sig_len);

          if (!EVP_PKEY_sign(ctx.get(), signature->data(), &sig_len, input_ptr, input_len))
            return net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
        }

        return net::OK;
      }

    private:
      crypto::ScopedEVP_PKEY key_;
      net::SSLPrivateKey::Type type_;
      CefRefPtr<CefSSLKeyDelegate> delegate_;

      DISALLOW_COPY_AND_ASSIGN(CefOpenSSLPlatformKey);
  };

  scoped_refptr<net::SSLPrivateKey> WrapOpenSSLPrivateKey(crypto::ScopedEVP_PKEY key) {
    net::SSLPrivateKey::Type type;
    if (key.get())
    {
      switch (EVP_PKEY_id(key.get())) {
        case EVP_PKEY_RSA:
          type = net::SSLPrivateKey::Type::RSA;
          break;
        case EVP_PKEY_EC:
          type = net::SSLPrivateKey::Type::ECDSA;
          break;
        default:
           LOG(ERROR) << "Unknown key type: " << EVP_PKEY_id(key.get());
          return nullptr;
      }
    }
    else
      type = net::SSLPrivateKey::Type::RSA;

    CefRefPtr<CefSSLKeyDelegate> delegate;
    CefRefPtr<CefApp> application = CefContentClient::Get()->application();

    if (application.get())
      delegate = application->GetSSLKeyDelegate();
    else if (!key.get())
      // No delegate and no key : nothing we can do
      return nullptr;

    return make_scoped_refptr(new net::ThreadedSSLPrivateKey(
                                std::unique_ptr<net::ThreadedSSLPrivateKey::Delegate>(new CefOpenSSLPlatformKey(std::move(key), type, delegate)),
                                net::GetSSLPlatformKeyTaskRunner()));
  }

}  // namespace

scoped_refptr<net::SSLPrivateKey> net::FetchClientCertPrivateKey(
    net::X509Certificate* certificate) {
  crypto::ScopedEVP_PKEY key =
      net::OpenSSLClientKeyStore::GetInstance()->FetchClientCertPrivateKey(
          certificate);
  return WrapOpenSSLPrivateKey(std::move(key));
}

#endif
