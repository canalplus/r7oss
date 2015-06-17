#ifndef C8FVP3_DOWNLOAD_CODE_H_
#define C8FVP3_DOWNLOAD_CODE_H_
#include "c8fvp3_FW_lld_global.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef HQVDPLITE_API_FOR_STAPI
#define vsoc_way (0)
#endif

#define c8fvp3_DOWNLOAD_PMEMCODE(base_address) c8fvp3_download_code(base_address,vsoc_way,PmemAddr,HQVDP_LITE_IMEM_BASEADDR,PmemSize)
#define c8fvp3_DOWNLOAD_DMEMCODE(base_address) c8fvp3_download_code(base_address,vsoc_way,DmemAddr,HQVDP_LITE_DMEM_BASEADDR,DmemSize)

/*!
 * \brief Download code into the PMEM or DMEM by using classic writeRegister or using the streamer way.
 *
 * \param[in] c8fvp3_addr    Base address of the IP
 * \param[in] regnotstreamer Way to load code. if set, writeregister of 4 bytes othewise streamer way
 * \param[in] src_addr       Define the address in external memory where the code is stored.
 * \param[in] dest_addr      Internal address where the code has to be stored.
 * \param[in] size           Size of the code to load.
 */
void c8fvp3_download_code(gvh_u32_t c8fvp3_addr, gvh_bool_t regnotstreamer, gvh_u32_t src_addr, gvh_u32_t dest_addr, gvh_u32_t size);

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif
