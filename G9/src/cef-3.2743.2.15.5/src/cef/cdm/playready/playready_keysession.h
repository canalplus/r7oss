#ifndef PLAYREADY_KEYSESSION_H_CBC305DD_C7CF_4F5F_8F56_478151BC65FF
#define PLAYREADY_KEYSESSION_H_CBC305DD_C7CF_4F5F_8F56_478151BC65FF

#include <string>
#include <vector>

#include <drmmanagertypes.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "media/cdm/api/content_decryption_module.h"

#include "playready_types.h"

namespace media {

namespace playready {

class KeySession {
  public:
    KeySession(cdm::SessionType session_type);
    ~KeySession();

    /* Set the document host. */
    void SetDocumentHost(const std::string& document_host);

    /* Get the current PlayReady session's data store file. */
    std::string GetDataStoreFile(void) const;

    /* Get the current PlayReady session's app context. */
    DRM_APP_CONTEXT* GetAppContext(void);

    cdm::SessionType Type() const;
    std::string Id() const;
    bool Initialized() const;
    bool Callable() const;

    bool GenerateRequest(const std::string& init_data_type,
        const uint8_t* init_data, size_t init_data_size,
        std::vector<uint8_t>& key_message, Error* error = NULL);
    bool Load(const std::string& session_id, Error* error = NULL);
    bool Update(const uint8_t* response, uint32_t response_size,
        Error* error = NULL);
    bool Close(Error* error = NULL);
    bool Remove(Error* error = NULL);

    /* Get the current content's key identifier. */
    std::vector<uint8_t> GetContentKeyId() const;

    /* Get the current decrypt key identifier. */
    std::vector<uint8_t> GetDecryptKeyId() const;

    /* Start decryption. */
    cdm::Status StartDecrypt(const std::vector<uint8_t>& key_id);

    /* Decrypt a buffer. */
    cdm::Status Decrypt(uint64_t iv, uint8_t* data, uint32_t data_size);

    /* Stop decryption. */
    void StopDecrypt();

    /* Test if a session type is supported. */
    static bool IsSupportedSessionType(cdm::SessionType& session_type);

    /* Test if a initialization data type is supported. */
    static bool IsSupportedInitDataType(const std::string& init_data_type);

    /* Test if a session identifier is valid. */
    static bool IsValidSessionId(const std::string& session_id);

    /* Initialize key session management. */
    static bool InitializeClass();

    /* Finalize key session management. */
    static void FinalizeClass();

  private:
    /* Session has been initialized. */
    bool initialized_;

    /* Session is callable. */
    bool callable_;

    /* Session is closed. */
    bool closed_;

    /* Session type. */
    cdm::SessionType type_;

    /* Session identifier. */
    std::string id_;

    /* Path of the PlayReady data store file. */
    std::string data_store_file_;

    /* Document Host which serves as "origin" */
    std::string document_host_;

    /* Session contains data. */
    bool contains_data_;

    /* Remove marker state. */
    bool remove_marker_;

    /* Session lock file descriptor. */
    int session_lock_fd_;

    /* Playready application context. */
    DRM_APP_CONTEXT* app_context_;

    /* Decrypt context. */
    DRM_DECRYPT_CONTEXT decrypt_context_;

    /* Indicate whether the decrypt context is initialized. */
    bool decrypt_context_initialized_;

    /* Indicate whether we must commit license use. */
    bool commit_license_use_;

    /* Per-process temporary directory. */
    static base::FilePath temp_dir_;

    /* Path of per-process FIFO file. */
    static base::FilePath fifo_path_;

    /* Per-process FIFO file. */
    static base::File fifo_;

    /* Header for debug traces. */
    std::string DH() const;

    /* Generate a globally unique session identifier. */
    static std::string GenerateSessionId();

    /* Compute the lock file path of a persistent session. */
    std::string ComputeLockFilePath(const std::string& session_id) const;

    /* Open the lock file of a persistent session. */
    bool OpenLockFile(const std::string& session_id);

    /* Delete the lock file of a persistent session. */
    void DeleteLockFile(const std::string& session_id);

    /* Compute the PlayReady HDS file path. */
    std::string ComputeDataStoreFilePath(cdm::SessionType session_type,
        const std::string& session_id) const;

    /* Set up application context. */
    bool SetUpContext(const std::string& file, Error* error = NULL);

    /* Tear down application context. */
    void TearDownContext(bool delete_data_store = true, Error* error = NULL);

    /* Find PlayReady object in initialization data. */
    bool FindPlayReadyObject(const void* init_data, size_t init_data_size,
        size_t& offset, size_t& size);

    /* Compute the remove marker file path. */
    std::string ComputeRemoveMarkerFilePath(const std::string& session_id)
      const;

    /* Enable or disable remove marker. */
    bool SetRemoveMarker(bool enabled);

    /* Purge obsolete files. */
    static void PurgeFiles();

  DISALLOW_COPY_AND_ASSIGN(KeySession);

}; /* KeySession */

} /* namespace playready */

} /* namespace media */

#endif /* file inclusion guard */
