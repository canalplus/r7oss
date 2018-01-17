/* Uncomment for debugging in release mode. */
//#define DCHECK_ALWAYS_ON 1

#include "base/logging.h"
#include "media/cdm/ppapi/cdm_logging.h"

#include "playready_datastore.h"

#include <drmmanager.h>
#include <drmbase64.h>
#include <drmerror.h>
#include <drmutf.h>

namespace media {

namespace playready {

DataStore::DataStore()
  : datastore_inited_(false),
    datastore_opened_(false),
    licstore_opened_(false) {
}

DataStore::~DataStore() {
  Close();
}

void DataStore::GetLicenses(
    std::vector<cdm::KeyInformation>& key_informations,
    DRM_APP_CONTEXT* app_context, const std::string& data_store_file) {
  DRM_RESULT dr;
  scoped_ptr<DRM_LICSTOREENUM_CONTEXT> licstoreenum_ctx;
  bool licstoreenum_inited = false;

  CDM_DLOG() << __FUNCTION__;

  if (!Open(data_store_file))
    goto l_error;

  licstoreenum_ctx = make_scoped_ptr(new DRM_LICSTOREENUM_CONTEXT);
  ZEROMEM(licstoreenum_ctx.get(), SIZEOF(DRM_LICSTOREENUM_CONTEXT));

  dr = DRM_LST_InitEnum(licstore_ctx_.get(), NULL, FALSE,
      licstoreenum_ctx.get());
  if (DRM_FAILED(dr)) {
    CDM_DLOG() <<  "cannot initialize license enumeration context: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  while (true) {
    DRM_KID kid;
    DRM_LID lid;
    DRM_DST_SLOT_HINT slot_hint;
    DRM_DWORD license_size;
    DRM_LICENSE_STATE_CATEGORY category;
    DRM_STRING key_message_drm_string;
    DRM_DWORD size;
    cdm::KeyInformation ki;
    std::vector<uint8_t> utf8_key_message;
    std::vector<uint8_t> encoded_key_id;

    /* This is the important stuff : the key information that
     * will be returned to the javascript */

    dr = DRM_LST_EnumNext(licstoreenum_ctx.get(), &kid, &lid, &slot_hint,
        &license_size);
    if (dr == DRM_E_NOMORE)
      break;
    if (dr != DRM_SUCCESS) {
      CDM_DLOG() << "cannot get next license: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
      goto l_error;
    }

    size = CCH_BASE64_EQUIV(sizeof(kid));
    encoded_key_id.resize(size);
    dr = DRM_B64_EncodeA((const DRM_BYTE*)&kid, sizeof(kid),
          (DRM_CHAR*)encoded_key_id.data(), &size, 0);
    if (DRM_FAILED(dr)) {
       CDM_DLOG() << "cannot convert KID to base64: "
         << DRM_ERR_GetErrorNameFromCode(dr, NULL);
       goto l_error;
    }
    /* The returned size may not be the same as the previously computed one. */
    encoded_key_id.resize(size);

    if (!ConstructDrmLicenseStatusQuery(encoded_key_id,
          key_message_drm_string)) {
       CDM_DLOG() << "cannot create license status query";
       goto l_error;
    }

    dr = Drm_LicenseQuery_IsAllowed(app_context,
        (const DRM_CONST_STRING*)&key_message_drm_string, NULL, NULL,
        &category);
    delete key_message_drm_string.pwszString;
    if (DRM_FAILED(dr)) {
       CDM_DLOG() << DRM_ERR_GetErrorNameFromCode(dr, NULL);
       continue;
    }

    ki.status = GetLicenseValidityFromPlayReady(dr);
    ki.key_id = new uint8_t[sizeof(kid)];
    MEMCPY(ki.key_id, &kid, sizeof(kid));
    ki.key_id_size = sizeof(kid);
    ki.system_code = 0;

    key_informations.push_back(ki);
  }

l_error:
  if (licstoreenum_inited) {
    dr = DRM_LST_EnumDelete(licstoreenum_ctx.get());
    if (DRM_FAILED(dr))
      CDM_DLOG() << "cannot delete license enumeration context: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
  }

  Close();
  return;
}

bool DataStore::Open(const std::string& filename) {
  if (licstore_opened_)
      return false;

  DCHECK(!datastore_ctx_.get());
  DCHECK(!datastore_opened_);
  DCHECK(!licstore_ctx_.get());
  DCHECK(!licstore_opened_);

  DRM_RESULT dr;
  DRM_STRING datastore_file = EMPTY_DRM_STRING;
  std::vector<DRM_WCHAR> datastore_file_buf;

  datastore_ = { eDRM_DST_NONE, { 0 }, NULL };
  datastore_ctx_ = make_scoped_ptr(new DRM_DST_CONTEXT);
  licstore_ctx_ = make_scoped_ptr(new DRM_LICSTORE_CONTEXT);
  ZEROMEM(datastore_ctx_.get(), SIZEOF(*datastore_ctx_));
  ZEROMEM(licstore_ctx_.get(), SIZEOF(*licstore_ctx_));

  dr = DRM_DST_Init(eDRM_DST_HDS, datastore_ctx_.get(),
      SIZEOF(DRM_DST_CONTEXT), &datastore_);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "cannot initialize data store: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  datastore_inited_ = true;

  datastore_file.cchString = filename.length();
  datastore_file_buf.resize(filename.length());
  datastore_file.pwszString = datastore_file_buf.data();

  if (DRM_FAILED(DRM_STR_ASCIItoDSTR(filename.c_str(), filename.size(),
          &datastore_file))) {
    CDM_DLOG() << "cannot convert file path to UTF-16";
    goto l_error;
  }

  /* Add nil terminator. */
  datastore_file_buf.resize(datastore_file.cchString + 1);
  datastore_file_buf[datastore_file.cchString] = 0;

  dr = DRM_DST_OpenStore(NULL, (DRM_BYTE*)datastore_file_buf.data(),
          datastore_file_buf.size() * SIZEOF(DRM_WCHAR), 0, &datastore_);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "cannot open the data store file " << filename << ": "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  datastore_opened_ = true;

  dr = DRM_LST_Open(licstore_ctx_.get(), &datastore_, eDRM_LICENSE_STORE_XMR);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "cannot open the license store: "
      << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    goto l_error;
  }

  licstore_opened_ = true;

  return true;

l_error:
  Close();
  return false;
}

void DataStore::Close() {
  DRM_RESULT dr;

  if (licstore_opened_) {
    dr = DRM_LST_Close(licstore_ctx_.get());
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << "cannot close license store: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    }
    licstore_ctx_ = NULL;
    licstore_opened_ = false;
  }

  if (datastore_opened_) {
    dr = DRM_DST_CloseStore(&datastore_);
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << "cannot close data store: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    }
    datastore_opened_ = false;
  }

  if (datastore_inited_) {
    dr = DRM_DST_Uninit(&datastore_);
    if (DRM_FAILED(dr)) {
      CDM_DLOG() << "cannot uninitialize data store: "
        << DRM_ERR_GetErrorNameFromCode(dr, NULL);
    }
    datastore_ctx_ = NULL;
    datastore_inited_ = false;
  }
}

bool DataStore::ConstructDrmLicenseStatusQuery (
    const std::vector<uint8_t>& encoded_key_id,
    DRM_STRING& key_message_drm_string) {

  static const char kQueryKeyStatusPrefix[] =
    "<LICENSESTATE type = \"challenge\">"
    "<DATA>"
    "<ACTION>Play</ACTION>"
    "<KID>";

  static const char kQueryKeyStatusSuffix[] =
    "</KID>"
    "<CANBIND>1</CANBIND>"
    "</DATA>"
    "</LICENSESTATE>";

  std::vector<uint8_t> utf8_key_message;
  std::vector<uint8_t> utf16_key_message;
  DRM_RESULT dr;

  /* Construct UTF-8 key message. */
  utf8_key_message.insert(utf8_key_message.end(), kQueryKeyStatusPrefix,
    kQueryKeyStatusPrefix + sizeof(kQueryKeyStatusPrefix) - 1);
  utf8_key_message.insert(utf8_key_message.end(), encoded_key_id.begin(),
    encoded_key_id.end());
  utf8_key_message.insert(utf8_key_message.end(), kQueryKeyStatusSuffix,
    kQueryKeyStatusSuffix + sizeof(kQueryKeyStatusSuffix) - 1);

  std::string str_km(utf8_key_message.begin(), utf8_key_message.end());

  key_message_drm_string.cchString = str_km.length();
  key_message_drm_string.pwszString = new DRM_WCHAR[key_message_drm_string.cchString];

  dr = DRM_STR_ASCIItoDSTR(str_km.data(), str_km.length(),
      &key_message_drm_string);
  if (DRM_FAILED(dr)) {
    CDM_DLOG() << "cannot convert ASCII string to UTF-16";
    return false;
  }

  return true;
}

cdm::KeyStatus DataStore::GetLicenseValidityFromPlayReady(const DRM_RESULT dr) {
  switch(dr) {
    case DRM_SUCCESS:
      return cdm::kUsable;
    case DRM_E_RIGHTS_NOT_AVAILABLE:
    case DRM_E_LICENSE_EXPIRED:
    case DRM_E_INVALID_LICENSE:
    case DRM_E_LICENSE_NOT_FOUND :
      return cdm::kExpired;
    default:
      return cdm::kInternalError;
  }
}

} /* namespace playready */

} /* namespace media */
