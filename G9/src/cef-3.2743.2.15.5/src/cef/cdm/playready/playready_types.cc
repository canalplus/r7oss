#include "playready_types.h"

namespace media {

namespace playready {

Error::Error(cdm::Error error, const std::string& message)
    : error(error), message(message) {
}

Error::Error(const std::string& message) {
}

} /* namespace playready */

} /* namespace media */
