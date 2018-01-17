#ifndef PLAYREADY_CDM_H_12649572_B303_4460_87C7_C925BCA0B360
#define PLAYREADY_CDM_H_12649572_B303_4460_87C7_C925BCA0B360

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/cdm/api/content_decryption_module.h"

#include "playready_keysession.h"

namespace media {
class DecoderBuffer;

class PlayReadyCdm : public cdm::ContentDecryptionModule_7 {
 public:
  PlayReadyCdm(Host* host, const std::string& key_system);
  ~PlayReadyCdm() override;

  void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                       cdm::SessionType session_type,
                                       const char * init_data_type,
                                       uint32_t init_data_type_size,
                                       const uint8_t* init_data,
                                       uint32_t init_data_size) override;
  void LoadSession(uint32_t promise_id,
                   cdm::SessionType session_type,
                   const char* session_id,
                   uint32_t session_id_length) override;
  void UpdateSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length,
                     const uint8_t* response,
                     uint32_t response_size) override;
  void CloseSession(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length) override;
  void RemoveSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length) override;
  void SetServerCertificate(uint32_t promise_id,
                            const uint8_t* server_certificate_data,
                            uint32_t server_certificate_data_size) override;
  void TimerExpired(void* context) override;
  cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                      cdm::DecryptedBlock* decrypted_block) override;
  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) override;
  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) override;
  void DeinitializeDecoder(cdm::StreamType decoder_type) override;
  void ResetDecoder(cdm::StreamType decoder_type) override;
  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer& encrypted_buffer,
                                    cdm::VideoFrame* video_frame) override;
  cdm::Status DecryptAndDecodeSamples(const cdm::InputBuffer& encrypted_buffer,
                                      cdm::AudioFrames* audio_frames) override;
  void Destroy() override;
  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& response) override;
  void OnQueryOutputProtectionStatus(cdm::QueryResult result,
                                     uint32_t link_mask,
                                     uint32_t output_protection_mask) override;

 private:
  PlayReadyCdm::Host* host_;
  const std::string key_system_;
  std::map<std::string, playready::KeySession*> key_sessions_;
  playready::KeySession* decrypt_session_;

  /* Find a key session and decrypt a buffer. */
  cdm::Status DecryptBuffer(const std::vector<uint8_t>& key_id, uint64_t iv,
      uint8_t* data, uint32_t data_size);

  // Decrypts the |encrypted_buffer| and puts the result in |decrypted_buffer|.
  // Returns cdm::kSuccess if decryption succeeded. The decrypted result is
  // put in |decrypted_buffer|. If |encrypted_buffer| is empty, the
  // |decrypted_buffer| is set to an empty (EOS) buffer.
  // Returns cdm::kNoKey if no decryption key was available. In this case
  // |decrypted_buffer| should be ignored by the caller.
  // Returns cdm::kDecryptError if any decryption error occurred. In this case
  // |decrypted_buffer| should be ignored by the caller.
  cdm::Status DecryptToMediaDecoderBuffer(
      const cdm::InputBuffer& encrypted_buffer,
      scoped_refptr<DecoderBuffer>* decrypted_buffer);

  playready::KeySession* FindKeySessionById(const std::string& session_id);
  void TriggerUpdateOnLicenses(playready::KeySession* key_session);

  DISALLOW_COPY_AND_ASSIGN(PlayReadyCdm);

}; /* PlayReadyCdm */

} /* namespace media */

#endif /* file inclusion guard */
