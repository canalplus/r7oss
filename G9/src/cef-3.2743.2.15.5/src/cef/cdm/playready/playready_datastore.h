#ifndef PLAYREADY_DATASTORE_H_23DFE00C_9CEE_4ED7_A19B_EC795BF38400
#define PLAYREADY_DATASTORE_H_23DFE00C_9CEE_4ED7_A19B_EC795BF38400

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_key_information.h"
#include "media/cdm/api/content_decryption_module.h"

#include <drmdatastoretypes.h>
#include <drmlicstore.h>
#include <drmmanagertypes.h>

namespace media {

namespace playready {

class DataStore {
  public:
    DataStore();
    ~DataStore();

    /* List licences in a datastore and send them back in key_informations */
    void GetLicenses(std::vector<cdm::KeyInformation>& key_informations,
        DRM_APP_CONTEXT* app_context, const std::string& data_store_file);

  private:
    DRM_DST datastore_;
    scoped_ptr<DRM_DST_CONTEXT> datastore_ctx_;
    bool datastore_inited_;
    bool datastore_opened_;

    scoped_ptr<DRM_LICSTORE_CONTEXT> licstore_ctx_;
    bool licstore_opened_;

    bool Open(const std::string& filename);
    void Close();

    bool ConstructDrmLicenseStatusQuery(
        const std::vector<uint8_t>& encoded_key_id,
        DRM_STRING& key_message_drm_string);
    cdm::KeyStatus GetLicenseValidityFromPlayReady(const DRM_RESULT dr);
};

} /* namespace playready */

} /* namespace media */

#endif /* file inclusion guard */
