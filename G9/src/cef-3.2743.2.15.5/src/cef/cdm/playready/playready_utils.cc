/* Uncomment for debugging in release mode. */
//#define DCHECK_ALWAYS_ON 1

#include <errno.h>
#include <unistd.h>

#include <stack>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"

#include <drmbytemanip.h>
#include <drmerr.h>
#include <drmmathsafe.h>
#include <drmutilitieslite.h>

#include "playready_utils.h"

namespace media {

namespace playready {

/* Helper function copied from PlayReady as not publicly available */
DRM_RESULT DRM_UTL_ReadNetworkBytesToNativeGUID(
   __in_bcount( cbData ) const DRM_BYTE  *pbData,
   __in                  const DRM_DWORD  cbData,
   __in                        DRM_DWORD  ibGuidOffset,
   __out                       DRM_GUID  *pDrmGuid )
{
    DRM_RESULT dr       = DRM_SUCCESS;
    DRM_DWORD  dwResult = 0;

    ChkArg( pbData != NULL );
    ChkArg( pDrmGuid != NULL );
    ChkOverflow( cbData, ibGuidOffset );
    ChkDR( DRM_DWordSub( cbData, ibGuidOffset, &dwResult ) );
    ChkBOOL( dwResult >= DRM_GUID_LEN, DRM_E_BUFFERTOOSMALL );

    // Convert field by field.
    NETWORKBYTES_TO_DWORD( pDrmGuid->Data1, pbData, ibGuidOffset );
    ChkDR( DRM_DWordAdd( ibGuidOffset, SIZEOF( DRM_DWORD ), &ibGuidOffset ) );

    NETWORKBYTES_TO_WORD( pDrmGuid->Data2,  pbData, ibGuidOffset );
    ChkDR( DRM_DWordAdd( ibGuidOffset, SIZEOF( DRM_WORD ), &ibGuidOffset ) );

    NETWORKBYTES_TO_WORD( pDrmGuid->Data3, pbData, ibGuidOffset );
    ChkDR( DRM_DWordAdd( ibGuidOffset, SIZEOF( DRM_WORD ), &ibGuidOffset ) );

    // Copy last 8 bytes.
    DRM_BYT_CopyBytes( pDrmGuid->Data4, 0, pbData, ibGuidOffset, 8 );

ErrorExit :
    return dr;
}

bool DeleteFile(const base::FilePath& path, bool recursive) {
  /* This is an adaptation of Chrome's base::DeleteFile() that can deal with
   * another thread or process removing the same file at the same time.
   * Besides, it does not stop on the first error but tries to delete as many
   * files as possible. */
  if (!base::DirectoryExists(path))
    return (unlink(path.value().c_str()) == 0 || errno == ENOENT);
  if (!recursive)
    return (rmdir(path.value().c_str()) == 0 || errno == ENOENT);

  bool success = true;
  std::stack<std::string> directories;

  base::FileEnumerator traversal(path, true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
      base::FileEnumerator::SHOW_SYM_LINKS);
  directories.push(path.value());

  for (
      base::FilePath current = traversal.Next();
      !current.empty();
      current = traversal.Next()
  ) {
    if (traversal.GetInfo().IsDirectory())
      directories.push(current.value());
    else
      if (unlink(current.value().c_str()) != 0 && errno != ENOENT)
        success = false;
  }

  while (!directories.empty()) {
    base::FilePath dir(directories.top());
    directories.pop();
    if (rmdir(dir.value().c_str()) != 0 && errno != ENOENT)
      success = true;
  }

  return success;
}

} /* namespace playready */

} /* namespace media */
