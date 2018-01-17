#ifndef PLAYREADY_TYPES_H_FE0AC160_E4D6_427D_87EF_0FDBB85A1071
#define PLAYREADY_TYPES_H_FE0AC160_E4D6_427D_87EF_0FDBB85A1071

#include <string>

#include "media/cdm/api/content_decryption_module.h"

namespace media {

namespace playready {

struct Error {

  cdm::Error error;
  std::string message;

  Error(cdm::Error error = cdm::kUnknownError,
      const std::string& message = "internal error");

  Error(const std::string& message);

}; /* Error */

} /* namespace playready */

} /* namespace media */

#endif /* file inclusion guard */
