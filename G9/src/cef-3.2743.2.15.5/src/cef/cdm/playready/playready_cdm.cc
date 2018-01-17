/* Uncomment for debugging in release mode. */
//#define DCHECK_ALWAYS_ON 1

#define _BSD_SOURCE
#include <endian.h>
#include <stdlib.h>

#include <iomanip>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/key_systems.h"
#include "media/cdm/json_web_key.h"
#include "media/cdm/ppapi/cdm_logging.h"
#include "url/gurl.h"

#include <drmmanager.h>
#include <drmbase64.h>
#include <drmmathsafe.h>
#include <drmutf.h>

#include "playready_cdm.h"
#include "playready_utils.h"
#include "playready_datastore.h"

using namespace media;

static const char kPlayReadyCdmVersion[] = "0.0.1";
static const char kExternalPlayReadyKeySystem[] = "com.microsoft.playready";

static bool drm_platform_inited;
static bool cdm_inited;

void INITIALIZE_CDM_MODULE() {
  DRM_RESULT dr;

  dr = Drm_Platform_Initialize();
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "cannot initialize DRM platform";
    return;
  }
  drm_platform_inited = true;

  if (!playready::KeySession::InitializeClass()) {
    CDM_DLOG() << "cannot initialize key session management";
    return;
  }

  cdm_inited = true;
}

void DeinitializeCdmModule() {
  if (drm_platform_inited) {
    DRM_RESULT dr;

    playready::KeySession::FinalizeClass();
    cdm_inited = false;

    dr = Drm_Platform_Uninitialize();
    if (DRM_FAILED(dr))
      CDM_DLOG() << "cannot uninitialize DRM platform";
    else
      drm_platform_inited = false;
  }
}

void* CreateCdmInstance(int cdm_interface_version,
                        const char* key_system, uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data) {
  std::string key_system_string(key_system, key_system_size);

  if (key_system_string != kExternalPlayReadyKeySystem) {
    CDM_DLOG() << "unsupported key system:" << key_system_string;
    return NULL;
  }

  if (cdm_interface_version != media::PlayReadyCdm::kVersion)
    return NULL;

  if (!cdm_inited) {
    CDM_DLOG() << "CDM could not initialize";
    return NULL;
  }

  media::PlayReadyCdm::Host* host = static_cast<media::PlayReadyCdm::Host*>(
      get_cdm_host_func(media::PlayReadyCdm::Host::kVersion, user_data));
  if (!host)
    return NULL;

  return new media::PlayReadyCdm(host, key_system_string);
}

const char* GetCdmVersion() {
  return kPlayReadyCdmVersion;
}

namespace media {

PlayReadyCdm::PlayReadyCdm(PlayReadyCdm::Host* host,
                         const std::string& key_system)
    : host_(host),
      key_system_(key_system),
      decrypt_session_(NULL) {
  CDM_DLOG() << __FUNCTION__;
}

PlayReadyCdm::~PlayReadyCdm() {
  if (decrypt_session_)
    decrypt_session_->StopDecrypt();

  for (auto it = key_sessions_.begin(); it != key_sessions_.end(); ++it)
    delete it->second;
}

void PlayReadyCdm::CreateSessionAndGenerateRequest(
    uint32_t promise_id,
    cdm::SessionType session_type,
    const char* init_data_type,
    uint32_t init_data_type_size,
    const uint8_t* init_data,
    uint32_t init_data_size) {
  CDM_DLOG() << __FUNCTION__;

  std::string init_data_type_string(init_data_type, init_data_type_size);
  playready::Error error;
  scoped_ptr<playready::KeySession> key_session;
  std::vector<uint8_t> key_message;

  /* Check session type. */
  if (!playready::KeySession::IsSupportedSessionType(session_type)) {
    error.error = cdm::kNotSupportedError;
    error.message = "unsupported session type";
    CDM_DLOG() << error.message;
    goto l_error;
  }

  /* Create a key session. */
  key_session.reset(new playready::KeySession(session_type));
  DCHECK(key_session.get()); /* no exceptions in Chrome */

  /* Set the document host. */
  key_session->SetDocumentHost(host_->GetDocumentHost());

  /* Let the key session generate a challenge. */
  if (!key_session->GenerateRequest(init_data_type_string, init_data,
        init_data_size, key_message, &error))
    goto l_error;

  /* Trigger events. */
  host_->OnResolveNewSessionPromise(promise_id, key_session->Id().data(),
      key_session->Id().size());
  host_->OnSessionMessage(key_session->Id().data(), key_session->Id().size(),
      cdm::kLicenseRequest, (const char *)key_message.data(),
      key_message.size(), NULL, 0);

  /* Register the key session. */
  DCHECK(!key_sessions_.count(key_session->Id()));
  key_sessions_[key_session->Id()] = key_session.release();

  return;

l_error:
  host_->OnRejectPromise(promise_id, error.error, 0, error.message.data(),
      error.message.length());
}

void PlayReadyCdm::LoadSession(uint32_t promise_id,
                              cdm::SessionType session_type,
                              const char* session_id,
                              uint32_t session_id_length) {
  std::string session_id_string(session_id, session_id_length);
  playready::Error error;
  scoped_ptr<playready::KeySession> key_session;

  CDM_DLOG() << __FUNCTION__;

  /* Check session type. */
  if (!playready::KeySession::IsSupportedSessionType(session_type)) {
    error.error = cdm::kNotSupportedError;
    error.message = "unsupported session type";
    CDM_DLOG() << error.message;
    goto l_error;
  }

  /* Create a key session. */
  key_session.reset(new playready::KeySession(session_type));
  DCHECK(key_session.get()); /* no exceptions in Chrome */

  /* Set the document host. */
  key_session->SetDocumentHost(host_->GetDocumentHost());

  /* Let the key session load data. */
  if (!key_session->Load(session_id_string, &error))
    goto l_error;

  /* Data was stored for this session. */
  if (!key_session->Id().empty()) {
    /* Trigger event. */
    host_->OnResolveNewSessionPromise(promise_id, key_session->Id().data(),
        key_session->Id().size());

    /* TODO: update key statuses and expiration date. */
    TriggerUpdateOnLicenses(key_session.get());

    /* Register the key session. */
    DCHECK(!key_sessions_.count(key_session->Id()));
    key_sessions_[key_session->Id()] = key_session.release();
  }
  /* No data stored for this session. */
  else
    host_->OnResolveNewSessionPromise(promise_id, NULL, 0);

  return;

l_error:
  host_->OnRejectPromise(promise_id, error.error, 0, error.message.data(),
      error.message.length());
}

void PlayReadyCdm::UpdateSession(uint32_t promise_id,
                                const char* session_id,
                                uint32_t session_id_length,
                                const uint8_t* response,
                                uint32_t response_size) {
  CDM_DLOG() << __FUNCTION__;

  std::string session_id_string(session_id, session_id_length);
  playready::Error error;
  playready::KeySession* key_session;

  /* Check session identifier. */
  if (!playready::KeySession::IsValidSessionId(session_id_string)) {
    error.error = cdm::kInvalidAccessError;
    error.message = "invalid session identifier";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* Find corresponding session. */
  key_session = FindKeySessionById(session_id_string);
  if (!key_session) {
    error.error = cdm::kInvalidStateError;
    error.message = "unknown session";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* Let the session process the response. */
  if (!key_session->Update(response, response_size, &error))
    goto l_error;

  host_->OnResolvePromise(promise_id);

  /* TODO: update key statuses and expiration date. */
  TriggerUpdateOnLicenses(key_session);
  return;

l_error:
  host_->OnRejectPromise(promise_id, error.error, 0, error.message.data(),
      error.message.length());
}

void PlayReadyCdm::CloseSession(uint32_t promise_id,
                               const char* session_id,
                               uint32_t session_id_length) {
  CDM_DLOG() << __FUNCTION__;

  std::string session_id_string(session_id, session_id_length);
  playready::Error error;
  playready::KeySession* key_session;

  /* Check session identifier. */
  if (!playready::KeySession::IsValidSessionId(session_id_string)) {
    error.error = cdm::kInvalidAccessError;
    error.message = "invalid session identifier";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* Find corresponding session. */
  key_session = FindKeySessionById(session_id_string);
  if (!key_session) {
    error.error = cdm::kInvalidStateError;
    error.message = "unknown session";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* If used for decryption, reset decrypt session. */
  if (key_session == decrypt_session_) {
    decrypt_session_->StopDecrypt();
    decrypt_session_ = NULL;
  }

  /* Unregister the session. */
  key_sessions_.erase(key_session->Id());

  /* Close the session. */
  if (!key_session->Close(&error)) {
    delete key_session;
    goto l_error;
  }

  /* Delete the session. */
  delete key_session;

  host_->OnResolvePromise(promise_id);
  host_->OnSessionClosed(session_id, session_id_length);
  return;

l_error:
  host_->OnRejectPromise(promise_id, error.error, 0, error.message.data(),
      error.message.length());
}

void PlayReadyCdm::RemoveSession(uint32_t promise_id,
                                const char* session_id,
                                uint32_t session_id_length) {
  CDM_DLOG() << __FUNCTION__;

  std::string session_id_string(session_id, session_id_length);
  playready::Error error;
  playready::KeySession* key_session;

  /* Check session identifier. */
  if (!playready::KeySession::IsValidSessionId(session_id_string)) {
    error.error = cdm::kInvalidAccessError;
    error.message = "invalid session identifier";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* Find corresponding session. */
  key_session = FindKeySessionById(session_id_string);
  if (!key_session) {
    error.error = cdm::kInvalidStateError;
    error.message = "unknown session";
    CDM_DLOG() << error.message << ' ' << session_id_string;
    goto l_error;
  }

  /* If used for decryption, reset decrypt session. */
  if (key_session == decrypt_session_) {
    decrypt_session_->StopDecrypt();
    decrypt_session_ = NULL;
  }

  /* Remove the session's stored data. */
  if (!key_session->Remove(&error))
    goto l_error;

  /* TODO: update key statuses and expiration date. */

  host_->OnResolvePromise(promise_id);
  return;

l_error:
  host_->OnRejectPromise(promise_id, error.error, 0, error.message.data(),
      error.message.length());
}

void PlayReadyCdm::SetServerCertificate(uint32_t promise_id,
                                       const uint8_t* server_certificate_data,
                                       uint32_t server_certificate_data_size) {
  /* We don't use a certificate. */
  host_->OnResolvePromise(promise_id);
}

void PlayReadyCdm::TimerExpired(void* context) {
  /* Called only if you set a timer using host_->SetTimer(). */
}

cdm::Status PlayReadyCdm::Decrypt(const cdm::InputBuffer& encrypted_buffer,
                                 cdm::DecryptedBlock* decrypted_block) {
  DCHECK(encrypted_buffer.data);

  scoped_refptr<media::DecoderBuffer> buffer;

  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);
  if (status != cdm::kSuccess)
    return status;

  DCHECK(buffer->data());
  decrypted_block->SetDecryptedBuffer(
      host_->Allocate(buffer->data_size()));
  memcpy(reinterpret_cast<void*>(decrypted_block->DecryptedBuffer()->Data()),
         buffer->data(),
         buffer->data_size());
  decrypted_block->DecryptedBuffer()->SetSize(buffer->data_size());
  decrypted_block->SetTimestamp(buffer->timestamp().InMicroseconds());

  return cdm::kSuccess;
}

cdm::Status PlayReadyCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& audio_decoder_config) {
  /* We don't support audio decoding. The browser will call Decrypt() and
   * decode audio samples itself. */
  return cdm::kSessionError;
}

cdm::Status PlayReadyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& video_decoder_config) {
  /* We don't support video decoding. The browser will call Decrypt() and
   * decode video frames itself. */
  return cdm::kSessionError;
}

void PlayReadyCdm::ResetDecoder(cdm::StreamType decoder_type) {
  /* Nothing to do as we don't support decoding. */
}

void PlayReadyCdm::DeinitializeDecoder(cdm::StreamType decoder_type) {
  /* Nothing to do as we don't support decoding. */
}

cdm::Status PlayReadyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  /* Nothing to do as we don't support decoding. */
  return cdm::kDecodeError;
}

cdm::Status PlayReadyCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::AudioFrames* audio_frames) {
  /* Nothing to do as we don't support decoding. */
  return cdm::kDecodeError;
}

void PlayReadyCdm::Destroy() {
  CDM_DLOG() << __FUNCTION__;
  delete this;
}

cdm::Status PlayReadyCdm::DecryptBuffer(const std::vector<uint8_t>& key_id,
    uint64_t iv, uint8_t* data, uint32_t data_size) {
  cdm::Status cdm_status;

  DCHECK(!key_id.empty());

  if (decrypt_session_ && key_id != decrypt_session_->GetContentKeyId()) {
    decrypt_session_->StopDecrypt();
    decrypt_session_ = NULL;
  }

  if (!decrypt_session_) {
    cdm::Status final_cdm_status = cdm::kNoKey;

    for (auto it = key_sessions_.begin(); it != key_sessions_.end(); ++it) {
      playready::KeySession* key_session = it->second;

      cdm_status = key_session->StartDecrypt(key_id);
      if (cdm_status == cdm::kSuccess) {
        decrypt_session_ = key_session;
        break;
      }

      /* Report no license was found only if all key sessions returned so. */
      if (final_cdm_status != cdm::kNoKey && cdm_status != cdm::kNoKey)
        final_cdm_status = cdm_status;
    }

    if (!decrypt_session_) {
      CDM_DLOG() << "cannot find valid license";
      return final_cdm_status;
    }
  }

  return decrypt_session_->Decrypt(iv, data, data_size);
}

cdm::Status PlayReadyCdm::DecryptToMediaDecoderBuffer(
    const cdm::InputBuffer& encrypted_buffer,
    scoped_refptr<media::DecoderBuffer>* decrypted_buffer) {
  cdm::Status cdm_status;
  scoped_refptr<media::DecoderBuffer> buffer;
  DRM_RESULT dr;
  DRM_GUID key_id_guid;
  uint64_t iv;

  DCHECK(decrypted_buffer);

  if (encrypted_buffer.iv_size == 16) {
    /* The browser passes a 16-byte IV even if it was stored as a 8-byte array.
     * We ensure the last 8 bytes are set to 0.*/
    for (uint32_t i = 8; i < 16; ++i) {
      if (encrypted_buffer.iv[i] != 0) {
        CDM_DLOG() << "not a 8-byte IV";
        return cdm::kDecryptError;
      }
    }
  }
  else if (encrypted_buffer.iv_size != 8) {
    CDM_DLOG() << "unsupported IV size " << encrypted_buffer.iv_size;
    return cdm::kDecryptError;
  }

  DCHECK(sizeof(iv) == 8);
  memcpy(&iv, encrypted_buffer.iv, 8);
  iv = be64toh(iv);

  if (!encrypted_buffer.data_size) {
    DCHECK(!encrypted_buffer.data);
    *decrypted_buffer = media::DecoderBuffer::CreateEOSBuffer();
    return cdm::kSuccess;
  }
  DCHECK(encrypted_buffer.data);

  /* Read the key identifier. */
  dr = playready::DRM_UTL_ReadNetworkBytesToNativeGUID(
      encrypted_buffer.key_id, encrypted_buffer.key_id_size, 0, &key_id_guid);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "invalid key identifier";
    return cdm::kDecryptError;
  }

  std::vector<uint8_t> key_id((uint8_t*)&key_id_guid,
      (uint8_t*)((&key_id_guid) + 1));

  /* Prepare the decrypted buffer. */
  buffer = media::DecoderBuffer::CopyFrom(encrypted_buffer.data,
      encrypted_buffer.data_size);
  buffer->set_timestamp(base::TimeDelta::FromMicroseconds(
        encrypted_buffer.timestamp));
  if (!buffer.get()) {
    CDM_DLOG() << "cannot allocate decrypted buffer";
    return cdm::kDecryptError;
  }

  /* No subsamples. */
  if (encrypted_buffer.num_subsamples == 0) {
    /* Decrypt the buffer in-place. */
    cdm_status = DecryptBuffer(key_id, iv, buffer->writable_data(),
        buffer->data_size());
    if (cdm_status != cdm::kSuccess)
      return cdm_status;
  }
  /* We have subsamples. */
  else {
    uint32_t i;
    const uint8_t* src;
    uint8_t* dst;
    uint32_t size, total_size;

    DCHECK(encrypted_buffer.subsamples);

    /* Compute how much data to decrypt. */
    for (total_size = 0, i = 0; i < encrypted_buffer.num_subsamples; ++i)
      total_size += encrypted_buffer.subsamples[i].cipher_bytes;

    /* Allocate buffer to store the encrypted bytes of all subsamples. */
    scoped_refptr<media::DecoderBuffer> subsamples_buffer(new
      media::DecoderBuffer(total_size));
    if (!subsamples_buffer.get()) {
      CDM_DLOG() << "cannot allocate decoder buffer";
      return cdm::kDecryptError;
    }

    /* Concatenate encrypted bytes. */
    src = encrypted_buffer.data;
    dst = subsamples_buffer->writable_data();
    for (i = 0; i < encrypted_buffer.num_subsamples; ++i) {
      src += encrypted_buffer.subsamples[i].clear_bytes;
      size = encrypted_buffer.subsamples[i].cipher_bytes;
      memcpy(dst, src, size);
      src += size;
      dst += size;
    }
    DCHECK((uint32_t)(src - encrypted_buffer.data) ==
        encrypted_buffer.data_size);
    DCHECK((uint32_t)(dst - subsamples_buffer->data()) == total_size);

    /* Decrypt the buffer in-place. */
    cdm_status = DecryptBuffer(key_id, iv, subsamples_buffer->writable_data(),
        total_size);
    if (cdm_status != cdm::kSuccess)
      return cdm_status;

    /* Copy back decrypted bytes at the right position. */
    src = subsamples_buffer->data();
    dst = buffer->writable_data();
    for (i = 0; i < encrypted_buffer.num_subsamples; ++i) {
      dst += encrypted_buffer.subsamples[i].clear_bytes;
      size = encrypted_buffer.subsamples[i].cipher_bytes;
      memcpy(dst, src, size);
      src += size;
      dst += size;
    }
    DCHECK((uint32_t)(src - subsamples_buffer->data()) == total_size);
    DCHECK((dst - buffer->data()) == buffer->data_size());
  }

  *decrypted_buffer = buffer;
  return cdm::kSuccess;
}

void PlayReadyCdm::OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& response) {
  NOTIMPLEMENTED();
}

void PlayReadyCdm::OnQueryOutputProtectionStatus(
    cdm::QueryResult result,
    uint32_t link_mask,
    uint32_t output_protection_mask) {
  NOTIMPLEMENTED();
};

playready::KeySession* PlayReadyCdm::FindKeySessionById(
    const std::string& session_id) {
   auto it = key_sessions_.find(session_id);
   if (it == key_sessions_.end())
     return NULL;
   return it->second;
}

void PlayReadyCdm::TriggerUpdateOnLicenses(playready::KeySession* key_session) {
  std::vector<cdm::KeyInformation> key_informations;
  playready::DataStore datastore;

  datastore.GetLicenses(key_informations, key_session->GetAppContext(),
      key_session->GetDataStoreFile());

  host_->OnSessionKeysChange(key_session->Id().data(),
      key_session->Id().size(),
      true,
      key_informations.data(),
      key_informations.size());

  for (auto it = key_informations.begin();
      it != key_informations.end(); ++it) {
    delete [] (*it).key_id;
    (*it).key_id = NULL;
  }

};

} /* namespace media */
