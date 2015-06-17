#ifndef C8FVP3_FW_LLD_GLOBAL_H_
#define C8FVP3_FW_LLD_GLOBAL_H_

#include "c8fvp3_stddefs.h"
#include "c8fvp3_hqvdplite_api_struct.h"

/* error codes for driver */
#define HQVDP_NO_ERROR          (0)
#define HQVDP_VIEWPORT_ERROR    (1)
#define HQVDP_PHASE_ERROR       (2)
#define HQVDP_WARNING           (3)

#ifdef HQVDPLITE_API_FOR_STAPI
#include "hqvdp_firm_rev.h"

#define HQVDP_LITE_MBX_BASEADDR HQVDP_MBX_OFFSET

#define c8fvp3_hqvdplite_api_struct_t HQVDPLITE_CMD_t 

#else 
#include "hqr_csdi_mapping.h"

/* undef public defines to verify exported ones */
#undef HQVDP_STR64_RDPLUG_OFFSET
#undef HQVDP_STR64_WRPLUG_OFFSET
#undef HQVDP_MBX_OFFSET
#undef HQVDP_PMEM_OFFSET
#undef HQVDP_DMEM_OFFSET
#undef HQVDP_LITE_STR_READ_REGS_BASEADDR
#endif

/* exported defines to drivers */
#define HQVDP_STR64_RDPLUG_OFFSET         (0x0e0000)
#define HQVDP_STR64_WRPLUG_OFFSET         (0x0e2000)
#define HQVDP_MBX_OFFSET                  (0x0e4000)
#define HQVDP_PMEM_OFFSET                 (0x040000)
#define HQVDP_DMEM_OFFSET                 (0x000000)
#define HQVDP_LITE_STR_READ_REGS_BASEADDR (0x0b8000)

#endif
