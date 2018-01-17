#ifndef PLAYREADY_UTILS_H_E9F8CA26_2CA4_44E8_A62D_FAAD67908C37
#define PLAYREADY_UTILS_H_E9F8CA26_2CA4_44E8_A62D_FAAD67908C37

#include "base/files/file_path.h"

#include <drmtypes.h>

namespace media {

namespace playready {

/* Helper function copied from PlayReady as not publicly available */
DRM_RESULT DRM_UTL_ReadNetworkBytesToNativeGUID(
   __in_bcount( cbData ) const DRM_BYTE  *pbData,
   __in                  const DRM_DWORD  cbData,
   __in                        DRM_DWORD  ibGuidOffset,
   __out                       DRM_GUID  *pDrmGuid);

/* Delete a file or a directory. */
bool DeleteFile(const base::FilePath& path, bool recursive);

} /* namespace playready */

} /* namespace media */

#endif /* file inclusion guard */
