/* Uncomment for debugging in release mode. */
//#define DCHECK_ALWAYS_ON 1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include <iomanip>

#include "base/files/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/guid.h"
#include "base/logging.h"
#include "media/cdm/ppapi/cdm_logging.h"

#include <drmmanager.h>
#include <drmbase64.h>
#include <drmerror.h>
#include <drmmathsafe.h>
#include <drmutf.h>

#include "playready_keysession.h"
#include "playready_datastore.h"
#include "playready_utils.h"

namespace media {

namespace playready {

static const char kPlayReadyDataStoreDir[] = "/etc/params/cef/";

static const DRM_CONST_STRING* kPlayReadyRights[1] = {
  &g_dstrWMDRM_RIGHT_PLAYBACK
};

// The standard PlayReady protection system ID.
static DRM_ID kPlayReadyProtectionSystemID = {
  0x79, 0xf0, 0x04, 0x9a,
  0x40, 0x98,
  0x86, 0x42,
  0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95
};

// The standard ID of the PSSH box wrapped inside of a UUID box.
static DRM_ID kPSSHBoxGUID = {
  0x18, 0x4f, 0x8a, 0xd0,
  0xf3, 0x10,
  0x82, 0x4a,
  0xb6, 0xc8, 0x32, 0xd8, 0xab, 0xa1, 0x83, 0xd3
};

static const char kPlayReadyKeyMessagePrefix[] =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<PlayReadyKeyMessage type=\"LicenseAcquisition\">"
"<LicenseAcquisition version=\"1.0\">"
"<Challenge encoding=\"base64encoded\">";
static const char kPlayReadyKeyMessageSuffix[] =
"</Challenge>"
"<HttpHeaders>"
"<HttpHeader>"
"<name>Content-Type</name>"
"<value>text/xml; charset=utf-8</value>"
"</HttpHeader>"
"<HttpHeader>"
"<name>SOAPAction</name>"
"<value>http://schemas.microsoft.com/DRM/2007/03/protocols/AcquireLicense</value>"
"</HttpHeader>"
"</HttpHeaders>"
"</LicenseAcquisition>"
"</PlayReadyKeyMessage>";

base::FilePath KeySession::temp_dir_;
base::FilePath KeySession::fifo_path_;
base::File KeySession::fifo_;

KeySession::KeySession(cdm::SessionType type)
  : initialized_(false),
    callable_(false),
    closed_(false),
    type_(type),
    contains_data_(false),
    remove_marker_(false),
    session_lock_fd_(-1),
    app_context_(NULL),
    decrypt_context_initialized_(false),
    commit_license_use_(false) {
  CDM_DLOG() << DH() << __FUNCTION__;

  DCHECK(IsSupportedSessionType(type));
}

KeySession::~KeySession() {
  CDM_DLOG() << DH() << __FUNCTION__;

  if (callable_ && !closed_)
    Close();
}

void KeySession::SetDocumentHost(const std::string& document_host) {
  document_host_ = document_host;
}

std::string KeySession::GetDataStoreFile(void) const {
  return data_store_file_;
}

DRM_APP_CONTEXT* KeySession::GetAppContext(void) {
  return app_context_;
}

cdm::SessionType KeySession::Type() const {
  return type_;
}

std::string KeySession::Id() const {
    return id_;
}

bool KeySession::Initialized() const {
  return initialized_;
}

bool KeySession::Callable() const {
  return callable_;
}

bool KeySession::GenerateRequest(const std::string& init_data_type,
    const uint8_t* init_data, size_t init_data_size,
    std::vector<uint8_t>& key_message, Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  Error local_error;
  size_t pro_offset, pro_size;
  std::vector<uint8_t> pro_data;
  std::vector<uint8_t> challenge;
  std::vector<uint8_t> encoded_challenge;
  std::vector<uint8_t> utf8_key_message;
  DRM_RESULT dr;
  DRM_DWORD size;

  /* Cannot initialize twice. */
  if (initialized_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "already initialized";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }
  initialized_ = true;

  /* Check initialization data type. */
  if (!IsSupportedInitDataType(init_data_type)) {
    local_error.error = cdm::kNotSupportedError;
    local_error.message = "unsupported init data type";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Check initialization data. */
  if (!init_data || init_data_size == 0) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "initialization data is empty";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Find and copy PlayReady object in initialization data. */
  if (!FindPlayReadyObject(init_data, init_data_size, pro_offset, pro_size)) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "invalid initialization data";
    CDM_DLOG() << DH() << "cannot get PlayReady object out of initialization "
      "data";
    goto l_error;
  }

  pro_data.reserve(pro_size);
  pro_data.insert(pro_data.begin(), init_data + pro_offset,
      init_data + pro_offset + pro_size);
  DCHECK(pro_size == pro_data.size());

  /* We need a session identifier now. */
  id_ = GenerateSessionId();

  /* Compute data store file path. */
  data_store_file_ = ComputeDataStoreFilePath(type_, id_);

  /* If it's a persistent key session, we enable the remove marker to indicate
   * it must be deleted by the purge mechanism in case we exit abruptly and
   * don't have the opportunity to do it. The marker file must be created prior
   * to creating the data store, just in case a crash occurs before the marker
   * is set. Temporary sessions are created in a temporary directory and will
   * be automatically wiped out when purging obsolete files. */
  if (type_ != cdm::kTemporary && !SetRemoveMarker(true))
    goto l_error;

  /* Prevent a session from being loaded multiple times.
   * A data store may already be opened in a different process. */
  if (type_ != cdm::kTemporary && !OpenLockFile(id_)) {
    local_error.error = cdm::kQuotaExceededError;
    local_error.message = "a non-closed session already exists for this sessionId";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Set up application context. */
  if (!SetUpContext(data_store_file_, &local_error))
    goto l_error;

  dr = Drm_Content_SetProperty(app_context_, DRM_CSP_AUTODETECT_HEADER,
      pro_data.data(), pro_data.size());
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot set content header: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  /* Generate licence acquisition challenge. */
  dr = Drm_LicenseAcq_GenerateChallenge(app_context_, kPlayReadyRights,
      NO_OF(kPlayReadyRights), NULL, NULL, 0, NULL, NULL, NULL, 0, NULL, &size);
  if (dr != DRM_E_BUFFERTOOSMALL) {
    CDM_DLOG() << DH() << "cannot determine size of challenge";
    goto l_error;
  }
  challenge.resize(size);

  dr = Drm_LicenseAcq_GenerateChallenge(app_context_, kPlayReadyRights,
      NO_OF(kPlayReadyRights), NULL, NULL, 0, NULL, NULL, NULL, 0,
      challenge.data(), &size);
  if (DRM_FAILED(dr)) {
    local_error.message = "cannot generate challenge";
    CDM_DLOG() << DH() << local_error.message << ": "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }
  /* The returned size may not be the same as in the first call. */
  challenge.resize(size);

  /* Encode challenge to base64. */
  size = CCH_BASE64_EQUIV(challenge.size());
  encoded_challenge.resize(size);
  dr = DRM_B64_EncodeA(challenge.data(), challenge.size(),
          (DRM_CHAR*)encoded_challenge.data(), &size, 0);
  if (DRM_FAILED(dr)) {
    local_error.message = "cannot encode challenge to base64";
    CDM_DLOG() << DH() << local_error.message << ": "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }
  /* The returned size may not be the same as the previously computed one. */
  encoded_challenge.resize(size);

  /* Construct UTF-8 key message. */
  utf8_key_message.insert(utf8_key_message.end(), kPlayReadyKeyMessagePrefix,
      kPlayReadyKeyMessagePrefix + sizeof(kPlayReadyKeyMessagePrefix) - 1);
  utf8_key_message.insert(utf8_key_message.end(), encoded_challenge.begin(),
      encoded_challenge.end());
  utf8_key_message.insert(utf8_key_message.end(), kPlayReadyKeyMessageSuffix,
      kPlayReadyKeyMessageSuffix + sizeof(kPlayReadyKeyMessageSuffix) - 1);

  /* Windows returns the key message in UTF-16 format. */
  size = 0;
  dr = DRM_STR_UTF8toUTF16((DRM_CHAR*)utf8_key_message.data(), 0,
      utf8_key_message.size(), NULL, &size);
  if (dr != DRM_E_BUFFERTOOSMALL) {
    CDM_DLOG() << DH() << "cannot determine key message size in UTF-16";
    goto l_error;
  }
  key_message.resize(sizeof(DRM_WCHAR) * size);

  dr = DRM_STR_UTF8toUTF16((DRM_CHAR*)utf8_key_message.data(), 0,
      utf8_key_message.size(), (DRM_WCHAR*)key_message.data(), &size);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot convert key message to UTF-16: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }
  /* The returned size may not be the same as in the first call. */
  key_message.resize(sizeof(DRM_WCHAR) * size);

  /* Strip the nil terminator. */
  DCHECK(key_message.size() > 2);
  DCHECK(key_message[key_message.size() - 2] == 0);
  DCHECK(key_message[key_message.size() - 1] == 0);
  key_message.resize(key_message.size() - 2);

  /* Finish the initialization. */
  callable_ = true;

  return true;

l_error:
  TearDownContext();
  SetRemoveMarker(false);
  if (type_ != cdm::kTemporary && session_lock_fd_ != -1) {
    DeleteLockFile(id_);
  }

  if (error)
    *error = local_error;

  return false;
}

bool KeySession::Load(const std::string& session_id, Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  bool success = false;
  Error local_error;
  struct stat file_status;

  /* Cannot initialize twice. */
  if (initialized_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "already initialized";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }
  initialized_ = true;

  /* Check session identifier. */
  if (session_id.empty()) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "empty session identifier";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Check session type. */
  if (type_ == cdm::kTemporary) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "not a persistent session";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Prevent a session from being loaded multiple times.
   * A data store may already be opened in a different process. */
  if (!OpenLockFile(session_id)) {
    local_error.error = cdm::kQuotaExceededError;
    local_error.message = "a non-closed session already exists for this sessionId";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  if (playready::KeySession::IsValidSessionId(session_id)) {
    /* Compute data store file path. */
    data_store_file_ = ComputeDataStoreFilePath(type_, session_id);
  }

  /* Check if data stored for this session. */
  if (data_store_file_.empty() ||
      lstat(data_store_file_.c_str(), &file_status) != 0 ||
      !S_ISREG(file_status.st_mode)) {
    CDM_DLOG() << DH() << "no data stored for session " << session_id;
    success = true;
    goto l_error;
  }

  /* Store the session identifier. */
  id_ = session_id;

  /* Set up application context. */
  if (!SetUpContext(data_store_file_, &local_error))
    goto l_error;

  /* TODO: is it the right time to call Drm_StoreMgmt_CleanupStore? */

  /* Finish the initialization. */
  callable_ = true;
  contains_data_ = true;

  return true;

l_error:
  TearDownContext();
  if (type_ != cdm::kTemporary) {
    DeleteLockFile(session_id);
  }

  if (!success && error)
    *error = local_error;

  return success;
}

bool KeySession::Update(const uint8_t* response, uint32_t response_size,
    Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  Error local_error;
  DRM_LICENSE_RESPONSE license_response = { eUnknownProtocol, 0 };
  DRM_RESULT dr;

  /* Check if callable. */
  if (!callable_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "not callable";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Check if response is empty. */
  if (!response || !response_size) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "empty response";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Let PlayReady process the response. */
  dr = Drm_LicenseAcq_ProcessResponse(app_context_,
      DRM_PROCESS_LIC_RESPONSE_SIGNATURE_NOT_REQUIRED, NULL, NULL,
      (DRM_BYTE*)response, response_size, &license_response);
  if (DRM_FAILED(dr)) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "invalid response";
    CDM_DLOG() << DH() << "cannot process response: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  /* Get rid of the remove marker if set. */
  if (!SetRemoveMarker(false))
    goto l_error;

  /* Now the session contains data. */
  contains_data_ = true;

  /* TODO: is it the right time to call Drm_StoreMgmt_CleanupStore? */

  CDM_DLOG() << DH() << "session updated";
  return true;

l_error:
  if (error)
    *error = local_error;

  return false;
}

bool KeySession::Close(Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  Error local_error;

  /* Check if callable. */
  if (!callable_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "not callable";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Already closed. */
  if (closed_) {
    CDM_DLOG() << DH() << "session already closed";
    return true;
  }

  DCHECK(!data_store_file_.empty());
  DCHECK(app_context_);

  /* Release the decrypt context. */
  StopDecrypt();

  /* Release the context. */
  TearDownContext(false, &local_error);

  /* Remove the lock if the session is persistent. */
  if (type_ != cdm::kTemporary) {
    DeleteLockFile(id_);
  }

  /* Get rid of the remove marker if set. */
  SetRemoveMarker(false);

  closed_ = true;

  return true;

l_error:
  if (error)
    *error = local_error;

  return false;
}

bool KeySession::Remove(Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  Error local_error;

  /* Check if callable. */
  if (!callable_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "not callable";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Can only be called on persistent sessions. */
  if (type_ == cdm::kTemporary) {
    local_error.error = cdm::kInvalidAccessError;
    local_error.message = "not a persistent session";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  /* Check if closed. */
  if (closed_) {
    local_error.error = cdm::kInvalidStateError;
    local_error.message = "session was closed";
    CDM_DLOG() << DH() << local_error.message;
    goto l_error;
  }

  DCHECK(!data_store_file_.empty());
  DCHECK(app_context_);

  /* Release the decrypt context. */
  StopDecrypt();

  /* The session's data has already been removed. */
  if (!contains_data_) {
    CDM_DLOG() << DH() << "no session data";
    return true;
  }

  /* Delete licenses. */
  /* TODO: we must enumerate all the key identifiers available and delete each
   * one. The key identifier enumeration function is to be implemented soon.
   * Presently, we assume the content was previously set and delete the
   * corresponding licenses. As this function fails if no content is set, we
   * test this case before and do nothing when it happens. This is typically
   * the case when the session has just been loaded. */
  if (!GetContentKeyId().empty()) {
    DRM_RESULT dr;

    dr = Drm_StoreMgmt_DeleteLicenses(app_context_, NULL, NULL);
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << DH() << "cannot delete licences: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
      goto l_error;
    }
  }

  /* Enable remove marker. */
  if (!SetRemoveMarker(true))
    goto  l_error;

  /* We have no content anymore. */
  contains_data_ = false;

  return true;

l_error:
  if (error)
    *error = local_error;

  return false;
}

std::vector<uint8_t> KeySession::GetContentKeyId() const {
  DRM_RESULT dr;

  if (!app_context_) {
    CDM_DLOG() << DH() << "no application context";
    return std::vector<uint8_t>();
  }

  DRM_BYTE key_id_data[CCH_BASE64_EQUIV(sizeof(DRM_GUID)) * sizeof(DRM_WCHAR)];
  DRM_DWORD key_id_size = sizeof(key_id_data);
  DRM_CONST_STRING key_id_str;
  DRM_KID key_id;

  dr = Drm_Content_GetProperty(app_context_, DRM_CGP_HEADER_KID,
      (DRM_BYTE*)&key_id_data, &key_id_size);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot get key identifier: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    return std::vector<uint8_t>();
  }

  DSTR_FROM_PB(&key_id_str, key_id_data, key_id_size);
  dr = DRM_UTL_DecodeKID(&key_id_str, &key_id);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot decode key identifier: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    return std::vector<uint8_t>();
  }

  return std::vector<uint8_t>((uint8_t*)&key_id, (uint8_t*)(&key_id + 1));
}

cdm::Status KeySession::StartDecrypt(const std::vector<uint8_t>& key_id) {
  DRM_RESULT dr;

  if (!callable_ || closed_)
    return cdm::kNoKey;

  if (decrypt_context_initialized_)
    return cdm::kSuccess;

  std::vector<uint8_t> content_key_id = GetContentKeyId();

  /* We assume a single key identifier per session. If a key identifier was
   * already set, we just check it matches the requested one. If no key
   * identifier is set, we will try to find a valid license for the requested
   * key identifier. If we find a license, we will stick to this key identifier
   * and will disallow the use of any other key identifier. Typically, the key
   * identifier is not set after a session has been loaded. But generally it is
   * after a call to GenerateRequest() as it's typically stored in the
   * PlayReady object. The PlayReady object can contain at most one key
   * identifier. */

  if (!content_key_id.empty() && content_key_id != key_id) {
    CDM_DLOG() << DH() << "key identifier does not match content's key "
      "identifier";
    return cdm::kNoKey;
  }

  /* Find a valid license. */
  dr = Drm_Reader_Bind(app_context_, kPlayReadyRights, NO_OF(kPlayReadyRights),
      NULL, NULL, &decrypt_context_);
  if (dr == DRM_E_HEADER_NOT_SET) {
    DRM_WCHAR encoded_key_id[CCH_BASE64_EQUIV(key_id.size())];
    DRM_DWORD encoded_key_id_size = sizeof(encoded_key_id) / sizeof(DRM_WCHAR);
    DRM_CSP_HEADER_COMPONENTS_DATA kid_data = { DRM_HEADER_VERSION_4,
      eDRM_AES_COUNTER_CIPHER, 0 };

    dr = DRM_B64_EncodeW((DRM_BYTE*)key_id.data(), key_id.size(),
        encoded_key_id, &encoded_key_id_size, 0);
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << DH() << "cannot encode key identifier to base64: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
      return cdm::kDecryptError;
    }

    kid_data.dstrKID.cchString = encoded_key_id_size;
    kid_data.dstrKID.pwszString = encoded_key_id;
    dr = Drm_Content_SetProperty(app_context_, DRM_CSP_HEADER_COMPONENTS,
       (DRM_BYTE*)&kid_data, sizeof(kid_data));
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << DH() << "cannot set content header: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
      return cdm::kDecryptError;
    }

    /* Find a valid license. */
    dr = Drm_Reader_Bind(app_context_, kPlayReadyRights,
        NO_OF(kPlayReadyRights), NULL, NULL, &decrypt_context_);
    if (DRM_FAILED(dr)) {
      DRM_RESULT dr2;

      /* This is the only way to unset the current content. If we don't, we
       * won't be able to call Drm_Content_SetProperty() again. And so we won't
       * be able to find a license for another key identifier. */
      dr2 = Drm_Reinitialize(app_context_);
      if (DRM_FAILED(dr2))
        CDM_DLOG() << DH() << "warning: cannot reinitialize application "
          "context:" << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    }
  }

  if (dr == DRM_E_LICENSE_NOT_FOUND || dr == DRM_E_LICENSE_EXPIRED)
    return cdm::kNoKey;
  else if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot bind to license: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    return cdm::kDecryptError;
  }

  /* Set up the decrypt context. */
  dr = Drm_Reader_InitDecrypt(&decrypt_context_, NULL, 0);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot initialize decrypt context: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    Drm_Reader_Close(&decrypt_context_);
    return cdm::kDecryptError;
  }

  decrypt_context_initialized_ = true;
  commit_license_use_ = true;
  return cdm::kSuccess;
}

cdm::Status KeySession::Decrypt(uint64_t iv, uint8_t* data,
    uint32_t data_size) {
  DRM_AES_COUNTER_MODE_CONTEXT aes_context;
  DRM_RESULT dr;

  if (!callable_ || closed_)
    return cdm::kNoKey;

  if (!decrypt_context_initialized_) {
    CDM_DLOG() << DH() << "decryption not started";
    return cdm::kDecryptError;
  }

  /* Set up AES context. */
  memset(&aes_context, 0, sizeof(aes_context));
  aes_context.qwInitializationVector = iv;

  dr = Drm_Reader_Decrypt(&decrypt_context_, &aes_context, data, data_size);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot decrypt payload: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    return cdm::kDecryptError;
  }

  /* Commit license use on first decrypted packet. */
  if (commit_license_use_) {
    CDM_DLOG() << DH() << "commit license use";
    dr = Drm_Reader_Commit(app_context_, NULL, NULL);
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << DH() << "cannot commit license use: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
      return cdm::kDecryptError;
    }
    commit_license_use_ = false;
  }

  return cdm::kSuccess;
}

void KeySession::StopDecrypt() {
  if (!decrypt_context_initialized_)
    return;

  Drm_Reader_Close(&decrypt_context_);
  decrypt_context_initialized_ = false;
  commit_license_use_ = false;
}

bool KeySession::IsSupportedSessionType(cdm::SessionType& session_type) {
  switch (session_type) {
    case cdm::kTemporary:
    case cdm::kPersistentLicense:
      return true;
    default:
      break;
  }
  return false;
}

bool KeySession::IsSupportedInitDataType(const std::string& session_type) {
  return (session_type == "cenc");
}

bool KeySession::IsValidSessionId(const std::string& session_id) {
  if (session_id.length() != 36 || session_id[8] != 'x' ||
      session_id[13] != 'x' || session_id[18] != 'x' ||
      session_id[23] != 'x')
    return false;

  std::string guid(session_id);
  guid[8] = guid[13] = guid[18] = guid[23] = '-';

  return base::IsValidGUID(guid);
}

bool KeySession::InitializeClass() {
  std::string guid;
  std::stringstream sstr;
  int fifo_fd;
  unsigned int attempts = 10;
  base::File::Error file_error;

  DCHECK(fifo_path_.empty());
  DCHECK(!fifo_.IsValid());

  guid = base::GenerateGUID();
  sstr << kPlayReadyDataStoreDir << guid << '/';
  temp_dir_ = base::FilePath(sstr.str());
  sstr.clear(); sstr.str("");
  sstr << kPlayReadyDataStoreDir << guid << ".fifo";
  fifo_path_ = base::FilePath(sstr.str());

  do {
    CDM_DLOG() << "create FIFO " << fifo_path_.value();
    if (mkfifo(fifo_path_.value().c_str(), 0600) != 0) {
      CDM_DLOG() << "cannot create FIFO " << fifo_path_.value() << ": "
        << strerror(errno);
      goto l_error;
    }

    CDM_DLOG() << "open FIFO " << fifo_path_.value();
    fifo_fd = open(fifo_path_.value().c_str(), O_RDONLY | O_NONBLOCK);
    if (fifo_fd >= 0) {
      fifo_ = std::move(base::File(fifo_fd));
      break;
    }

    /* As FIFO is not opened yet, it may be deleted by another process running
     * the clean up procedure. Run again the creation and open steps. If we are
     * still unable to complete them after a few attempts, bail out. */
    if (errno != ENOENT || attempts == 0) {
      CDM_DLOG() << "cannot open FIFO " << fifo_path_.value() << ": "
        << strerror(errno);
      goto l_error;
    }
    attempts--;

  } while (true);

  CDM_DLOG() << "create temporary directory " << temp_dir_.value();
  if (!base::CreateDirectoryAndGetError(temp_dir_, &file_error)) {
    CDM_DLOG() << "cannot create temporary directory " << temp_dir_.value()
      << ": " << base::File::ErrorToString(file_error);
    goto l_error;
  }

  DCHECK(!fifo_path_.empty());
  DCHECK(fifo_.IsValid());
  DCHECK(!temp_dir_.empty());
  DCHECK(base::DirectoryExists(temp_dir_));
  DCHECK(base::PathIsWritable(temp_dir_));

  /* It's time to delete obsolete files. */
  PurgeFiles();

  return true;

l_error:
  FinalizeClass();
  DCHECK(fifo_path_.empty());
  DCHECK(!fifo_.IsValid());
  DCHECK(temp_dir_.empty());

  return false;
}

void KeySession::FinalizeClass() {
  if (fifo_.IsValid()) {
    CDM_DLOG() << "close FIFO " << fifo_path_.value();
    fifo_.Close();
  }

  if (!fifo_path_.empty()) {
    CDM_DLOG() << "delete FIFO " << fifo_path_.value();
    if (!media::playready::DeleteFile(fifo_path_, false))
      CDM_DLOG() << "warning: cannot delete FIFO " << fifo_path_.value();
    fifo_path_.clear();
  }

  if (!temp_dir_.empty()) {
    CDM_DLOG() << "delete temporary directory " << temp_dir_.value();
    if (!media::playready::DeleteFile(fifo_path_, true))
      CDM_DLOG() << "warning: cannot delete temporary directory"
        << temp_dir_.value();
    temp_dir_.clear();
  }
}

std::string KeySession::ComputeRemoveMarkerFilePath(
    const std::string& session_id) const {
  std::stringstream sstr;

  DCHECK(type_ != cdm::kTemporary);
  DCHECK(IsValidSessionId(session_id));

  sstr << temp_dir_.value() << document_host_ << '_' << session_id << ".remove";

  return sstr.str();
}

bool KeySession::SetRemoveMarker(bool enabled) {
  /* No change. */
  if (!(enabled ^ remove_marker_))
    return true;

  base::FilePath path(ComputeRemoveMarkerFilePath(id_));

  if (enabled &&
      !base::CreateSymbolicLink(base::FilePath(data_store_file_), path)) {
    CDM_DLOG() << DH() << "cannot create remove marker " << path.value();
    return false;
  }
  else if (!enabled && !media::playready::DeleteFile(path, false)) {
    CDM_DLOG() << DH() << "cannot delete remove marker " << path.value();
    return false;
  }

  /* Flush parent directory to ensure marker creation/deletion is permanently
   * stored. If we fail, we don't report an error and expect the system won't
   * lose our changes if it crashes. */
  base::File dir(path.DirName(), base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!dir.IsValid()) {
    CDM_DLOG() << DH() << "warning: cannot open directory "
      << path.DirName().value();
  }
  else if (!dir.Flush()) {
    CDM_DLOG() << DH() << "warning: cannot flush directory "
      << path.DirName().value();
  }

  remove_marker_ = !remove_marker_;

  return true;
}

std::string KeySession::ComputeLockFilePath(const std::string& session_id) const {
  DCHECK(type_ != cdm::kTemporary);
  DCHECK(IsValidSessionId(session_id));

  base::FilePath temp_dir_path;
  if (!base::GetTempDir(&temp_dir_path)) {
    temp_dir_path = base::FilePath::FromUTF8Unsafe("/tmp");
  }
  std::stringstream sstr;
  sstr << temp_dir_path.value() << '/' << document_host_ << '_' << session_id << ".lock";

  return sstr.str();
}

bool KeySession::OpenLockFile(const std::string& session_id) {
  /* A session lock is only required for persistent sessions. */
  DCHECK(type_ != cdm::kTemporary);
  DCHECK(session_lock_fd_ == -1);

  /* A lock file is opened in /tmp and locked. */

  /* Compute lock path. */
  base::FilePath lock_path(ComputeLockFilePath(session_id));

  CDM_DLOG() << DH() << "opening lock file: " << lock_path.value();

try_again:

  /* Open lock file. */
  int lock_fd = open(lock_path.value().c_str(), O_RDONLY | O_CREAT, S_IRUSR);
  if (lock_fd == -1) { /* An error occured while opening lock path. */
    CDM_DLOG() << DH() << "error: cannot open lock file: " << strerror(errno);
    return false;
  }

  /* Acquire lock. */
  if (flock(lock_fd, LOCK_EX | LOCK_NB) == -1) {
    /* Locking failed. */
    CDM_DLOG() << DH() << "error: cannot acquire lock: " << strerror(errno);
    close(lock_fd);
    return false;
  }

  /* Ensure acquired lock file has not been deleted between open and flock by the previous owner. */
  struct stat lock_status;
  struct stat reopened_lock_status;
  if ((fstat(lock_fd, &lock_status) == -1)
      || (stat(lock_path.value().c_str(), &reopened_lock_status) == -1)) {
    if (errno == ENOENT) { /* stat failed. */
      /* Acquired lock file has been deleted by the previous owner.
       * We shall recreate a new one and reacquire it. */
      close(lock_fd);
      goto try_again;
    }
    else { /* fstat/stat failed. */
      CDM_DLOG() << DH() << "error: cannot check lock file: " << strerror(errno);
      close(lock_fd);
      return false;
    }
  }
  if (lock_status.st_ino != reopened_lock_status.st_ino) { /* Ensure inodes still match. */
    /* Acquired lock file has been deleted by the previous owner and recreated by another process.
     * We shall reacquire it. */
    close(lock_fd);
    goto try_again;
  }

  session_lock_fd_ = lock_fd;

  CDM_DLOG() << DH() << "lock acquired";
  return true;
}

void KeySession::DeleteLockFile(const std::string& session_id) {
  DCHECK(type_ != cdm::kTemporary);

  if (session_lock_fd_ != -1) {
    /* Compute paths. */
    base::FilePath lock_path(ComputeLockFilePath(session_id));

    CDM_DLOG() << DH() << "removing lock file: " << lock_path.value();

    /* Remove lock file. */
    if (unlink(lock_path.value().c_str()) == -1) {
      /* Unlocking failed. */
      CDM_DLOG() << DH() << "warning: cannot remove lock file, releasing lock anyway: " << strerror(errno);
    }

    /* Close lock file and release lock. */
    close(session_lock_fd_);

    CDM_DLOG() << DH() << "lock released";

    session_lock_fd_ = -1;
  }
}

void KeySession::PurgeFiles() {
  /* WARNING: this method may be executed from multiple renderer processes
   * simultaneously. As they all share the same data store directory, it must
   * be carefully written to avoid conflicts. */

  CDM_DLOG() << "purge obsolete files";

  /* Delete temporary directories whose corresponding process has exited. If we
   * cannot delete a file, we keep the FIFO and hope we have more chance in the
   * future. Otherwise, we would leak files forever. */
  base::FileEnumerator fifo_enum(base::FilePath(kPlayReadyDataStoreDir),
      false, base::FileEnumerator::FILES, "*.fifo");
  base::FilePath path;

  while (!(path = fifo_enum.Next()).empty()) {
    CDM_DLOG() << "check FIFO " << path.value();

    base::ScopedFD fd(open(path.value().c_str(), O_WRONLY | O_NONBLOCK));
    if (fd.is_valid()) {
      CDM_DLOG() << "FIFO " << path.value() << " still connected";
      continue;
    }
    if (errno != ENXIO) {
      CDM_DLOG() << "cannot open FIFO " << path.value() << ": "
        << strerror(errno);
      continue;
    }

    /* No process connected to read-end of FIFO. */
    CDM_DLOG() << "FIFO " << path.value() << " not used anymore";

    /* Delete persistent sessions marked for removal. */
    base::FilePath temp_dir(path.RemoveExtension());
    base::FileEnumerator remove_marker_enum(temp_dir, false,
        base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS,
        "*.remove");
    base::FilePath remove_marker_path;
    bool delete_fifo = true;

    while (!(remove_marker_path = remove_marker_enum.Next()).empty()) {
      CDM_DLOG() << "found session remove marker "
        << remove_marker_path.value();

      /* Find path of target session. */
      base::FilePath data_store_file;

      if (!ReadSymbolicLink(remove_marker_path, &data_store_file)) {
        CDM_DLOG() << "cannot resolve session remove marker "
          << remove_marker_path.value();
        continue;
      }

      /* Delete target session. */
      if (!media::playready::DeleteFile(data_store_file, false)) {
        CDM_DLOG() << "cannot delete session " << data_store_file.value();
        delete_fifo = false;
        continue;
      }
    }

    if (!media::playready::DeleteFile(temp_dir, true)) {
      CDM_DLOG() << "warning: cannot delete temporary directory"
        << path.RemoveExtension().value();
      delete_fifo = false;
    }

    if (delete_fifo && !media::playready::DeleteFile(path, false))
    CDM_DLOG() << "warning: cannot delete FIFO"
      << path.RemoveExtension().value();
  }
}

std::string KeySession::DH() const {
  std::stringstream sstr;

  sstr << "[" << this;
  if (!id_.empty())
    sstr << "/" << id_;
  sstr << "] ";

    return sstr.str();
}

std::string KeySession::GenerateSessionId() {
  std::string guid;

  guid = base::GenerateGUID();

  /* Chrome allows ASCII alphanumeric characters only. */
  DCHECK(guid[8] == '-');
  DCHECK(guid[13] == '-');
  DCHECK(guid[18] == '-');
  DCHECK(guid[23] == '-');
  guid[8] = guid[13] = guid[18] = guid[23] = 'x';

  return guid;
}

std::string KeySession::ComputeDataStoreFilePath(cdm::SessionType session_type,
    const std::string& session_id) const {
  std::stringstream sstr;

  DCHECK(IsSupportedSessionType(session_type));
  DCHECK(IsValidSessionId(session_id));

  if (session_type == cdm::kTemporary) sstr << temp_dir_.value();
  else sstr << kPlayReadyDataStoreDir;
  sstr << document_host_ << '_' << session_id << ".hds";

  return sstr.str();
}

bool KeySession::SetUpContext(const std::string& file, Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  bool success = false;
  Error local_error;

  DRM_RESULT dr;
  DRM_APP_CONTEXT* app_context = NULL;
  DRM_STRING data_store_file = EMPTY_DRM_STRING;
  DRM_BYTE* opaque_buffer = NULL;
  DRM_BYTE* revocation_buffer = NULL;
  bool app_context_initialized = false;

  DCHECK(!file.empty());

  if (app_context_) {
    CDM_DLOG() << DH() << "a context is already set up";
    goto l_error;
  }

  data_store_file.cchString = file.length();
  data_store_file.pwszString = (DRM_WCHAR *)Oem_MemAlloc(
      data_store_file.cchString * SIZEOF(DRM_WCHAR));
  if (!data_store_file.pwszString) {
    local_error.message = "not enough memory";
    CDM_DLOG() << local_error.message;
    goto l_error;
  }

  dr = DRM_STR_ASCIItoDSTR(file.data(), file.length(), &data_store_file);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot convert file path to UTF-16: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  app_context = (DRM_APP_CONTEXT*)Oem_MemAlloc(SIZEOF(DRM_APP_CONTEXT));
  if (!app_context) {
    local_error.message = "not enough memory";
    CDM_DLOG() << DH() << "cannot allocate application context";
    goto l_error;
  }
  memset(app_context, 0, sizeof(DRM_APP_CONTEXT));

  opaque_buffer = (DRM_BYTE*)Oem_MemAlloc(
      MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE);
  if (!opaque_buffer) {
    local_error.message = "not enough memory";
    CDM_DLOG() << DH() << "cannot allocate opaque context";
    goto l_error;
  }

  revocation_buffer = (DRM_BYTE*)Oem_MemAlloc(REVOCATION_BUFFER_SIZE);
  if (!revocation_buffer) {
    local_error.message = "not enough memory";
    CDM_DLOG() << DH() << "cannot allocate revocation buffer";
    goto l_error;
  }

  CDM_DLOG() << DH() << "create data store file " << file;
  dr = Drm_Initialize(app_context, NULL, opaque_buffer,
      MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE,
      (DRM_CONST_STRING*)&data_store_file);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot initialize app context: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }
  app_context_initialized = true;

  dr = Drm_Revocation_SetBuffer(app_context, revocation_buffer,
      REVOCATION_BUFFER_SIZE);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << DH() << "cannot assign revocation buffer: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  app_context_ = app_context;
  success = true;
  goto l_exit;

l_error:
  if (app_context_initialized)
    Drm_Uninitialize(app_context);
  if (app_context)
    Oem_MemFree(app_context);
  if (revocation_buffer)
    Oem_MemFree(revocation_buffer);
  if (opaque_buffer)
    Oem_MemFree(opaque_buffer);

l_exit:
  if (data_store_file.pwszString)
    Oem_MemFree(data_store_file.pwszString);

  if (!success && error)
    *error = local_error;

  return success;
}

void KeySession::TearDownContext(bool delete_data_store, Error* error) {
  CDM_DLOG() << DH() << __FUNCTION__;

  if (!app_context_)
    return;

  DRM_RESULT dr;
  DRM_BYTE* buffer;
  DRM_DWORD size;

  dr = Drm_GetOpaqueBuffer(app_context_, &buffer, &size);
  if (DRM_FAILED(dr))
    CDM_DLOG() << DH() << "warning: cannot get opaque buffer: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
  else if (buffer)
    Oem_MemFree(buffer);

  dr = Drm_Revocation_GetBuffer(app_context_, &buffer, &size);
  if (DRM_FAILED(dr))
    CDM_DLOG() << DH() << "warning: cannot get revocation buffer: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
  else if (buffer)
    Oem_MemFree(buffer);

  Drm_Uninitialize(app_context_);
  Oem_MemFree(app_context_);
  app_context_ = NULL;

  if (type_ == cdm::kTemporary || !contains_data_ || delete_data_store) {
    CDM_DLOG() << DH() << "delete data store file " << data_store_file_;
    if (unlink(data_store_file_.c_str()) != 0)
      CDM_DLOG() << DH() << "warning: cannot delete data store file "
        << data_store_file_;
  }
}

bool KeySession::FindPlayReadyObject(const void* init_data,
    size_t init_data_size, size_t& offset, size_t& size) {
  CDM_DLOG() << DH() << __FUNCTION__;

  const uint8_t* initdata = (const uint8_t*)init_data;
  DRM_DWORD initdatasize = init_data_size;
  DRM_DWORD currentpos = 0;
  DRM_BOOL proFound = FALSE;
  DRM_BOOL foundUUIDBox = FALSE;

  DRM_DWORD cbSystemSize = 0;
  DRM_DWORD cbSize = 0;
  DRM_DWORD dwType = 0;
  DRM_WORD wVersion = 0;
  DRM_WORD wFlags = 0;
  DRM_DWORD countMultiKeyIDs = 0;

  DRM_DWORD counterKeyIDs = 0;

  const DRM_DWORD uuidType = 0x75756964;
  const DRM_DWORD psshType = 0x70737368;
  DRM_GUID guidUUID = EMPTY_DRM_GUID;
  DRM_GUID guidSystemID = EMPTY_DRM_GUID;

  DRM_DWORD resinterm = 0;

  offset = 0;
  size = 0;

  while ((proFound == FALSE) && (currentpos < initdatasize)) {

    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_DWORD), &resinterm)) ) {
      CDM_DLOG() << DH() << "Impossible to add two numbers";
      return false;
    }
    if (resinterm > initdatasize) {
        CDM_DLOG() << DH() << "Index out of range";
        return false;
    }
    NETWORKBYTES_TO_DWORD(cbSize, initdata, currentpos);
    currentpos = resinterm;

    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_DWORD), &resinterm)) ) {
      CDM_DLOG() << DH() << "Impossible to add two numbers";
      return false;
    }
    if (resinterm > initdatasize) {
        CDM_DLOG() << DH() << "Index out of range";
        return false;
    }
    NETWORKBYTES_TO_DWORD(dwType, initdata, currentpos);
    currentpos = resinterm;

    if (dwType == uuidType) {
        if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_GUID), &resinterm)) ) {
          CDM_DLOG() << DH() << "Impossible to add two numbers";
          return false;
        }
        if (resinterm > initdatasize) {
            CDM_DLOG() << DH() << "Index out of range";
            return false;
        }
        if (DRM_FAILED(playready::DRM_UTL_ReadNetworkBytesToNativeGUID(
                initdata, initdatasize, currentpos, &guidUUID)) ) {
          CDM_DLOG() << DH() << "Impossible to convert network byte order to native byte order";
          return false;
        }
        currentpos = resinterm;

        if (0 != memcmp(&guidUUID, &kPSSHBoxGUID, SIZEOF(DRM_ID) )) {
          CDM_DLOG() << DH() << "It is not a PSSH box";
          return false;
        }
        foundUUIDBox = TRUE;
    }
    else {
        /* Let's at least hope we have the pssh */
        if (dwType != psshType) {
          CDM_DLOG() << DH() << "Neither UUID nor PSSH";
          return false;
        }
    }

    /* Version */
    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_WORD), &resinterm)) ) {
      CDM_DLOG() << DH() << "Impossible to add two numbers";
      return false;
    }
    NETWORKBYTES_TO_WORD(wVersion, initdata, currentpos);
    currentpos = resinterm;

    /* Flags */
    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_WORD), &resinterm)) ) {
      CDM_DLOG() << DH() << "Impossible to add two numbers";
      return false;
    }
    NETWORKBYTES_TO_WORD(wFlags, initdata, currentpos);
    currentpos = resinterm;

    /* SystemID */
    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_GUID), &resinterm)) ) {
      CDM_DLOG() << DH() << "Impossible to add two numbers";
      return false;
    }
    if (DRM_FAILED(DRM_UTL_ReadNetworkBytesToNativeGUID(initdata, initdatasize, 
                    currentpos, &guidSystemID)) ) {
      CDM_DLOG() << DH() << "Impossible to convert network byte order to native byte order";
      return false;
    }
    currentpos = resinterm;

    /* If we have multiple key IDs */
    if (wVersion > 0) {
        DRM_DWORD countKeyIDs = 0;
        if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_DWORD), &resinterm)) ) {
          CDM_DLOG() << DH() << "Impossible to add two numbers";
          return false;
        }
        NETWORKBYTES_TO_DWORD(countKeyIDs, initdata, currentpos);
        currentpos = resinterm;

        if (DRM_FAILED(DRM_DWordMult(counterKeyIDs, SIZEOF(DRM_GUID), &resinterm)) ) {
          CDM_DLOG() << DH() << "Impossible to multiply two numbers";
          return false;
        }
        if (DRM_FAILED(DRM_DWordAdd(currentpos, resinterm, &resinterm)) ) {
          CDM_DLOG() << DH() << "Impossible to add two numbers";
          return false;
        }
        currentpos = resinterm;

        countMultiKeyIDs = SIZEOF(DRM_DWORD) + (countKeyIDs * SIZEOF(DRM_GUID));
    }

    if (DRM_FAILED(DRM_DWordAdd(currentpos, SIZEOF(DRM_DWORD), &resinterm)) ) {
       CDM_DLOG() << DH() << "Impossible to add two numbers";
       return false;
    }

    NETWORKBYTES_TO_DWORD(cbSystemSize, initdata, currentpos);
    currentpos = resinterm;

    if (DRM_FAILED(DRM_DWordAdd(currentpos, cbSystemSize, &resinterm)) ) {
       CDM_DLOG() << DH() << "Impossible to add two numbers";
       return false;
    }

    /* Is it for playready ? */

    if (memcmp(&guidSystemID, &kPlayReadyProtectionSystemID, SIZEOF(DRM_GUID)) != 0) {
        /* continue looping */
        currentpos = resinterm;
    } else {
        /* found it ! */
        proFound = TRUE;
    }
  }

  if (proFound == FALSE) {
    CDM_DLOG() << DH() << "Impossible to find a PlayReady object";
    return false;
  }

  if (foundUUIDBox) {
    if (cbSystemSize + SIZEOF(cbSize) + SIZEOF(dwType) +
            SIZEOF(wVersion) + SIZEOF(wFlags) + SIZEOF(DRM_GUID) +
            SIZEOF(DRM_GUID)  + SIZEOF(cbSystemSize) != cbSize) {
        CDM_DLOG() << DH() << "Wrong size";
        return false;
    }
  } else {
    if (cbSystemSize + SIZEOF(cbSize) + SIZEOF(dwType) +
            SIZEOF(wVersion) + SIZEOF(wFlags) + SIZEOF(DRM_GUID) +
            SIZEOF(cbSystemSize) + countMultiKeyIDs != cbSize) {
        CDM_DLOG() << DH() << "Wrong size";
        return false;
    }
  }

  offset = currentpos;
  size = cbSystemSize;

  return true;
}

} /* namespace playready */

} /* namespace media */
