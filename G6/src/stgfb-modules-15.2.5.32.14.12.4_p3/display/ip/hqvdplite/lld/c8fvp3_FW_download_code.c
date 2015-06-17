#include "c8fvp3_FW_download_code.h"
#include "c8fvp3_strRegAccess.h"

/****************************************
  fct to download FW Code in PMEM and DMEM
******************************************/

void c8fvp3_download_code(gvh_u32_t c8fvp3_addr, gvh_bool_t regnotstreamer, gvh_u32_t src_addr, gvh_u32_t dest_addr, gvh_u32_t size) {

  gvh_u32_t offset;

  gvh_u32_t page;
  gvh_u32_t currsize;

  const gvh_u32_t pagesize= 0x1C00; /* 8192 max transfert */
  const gvh_u32_t DownloadCh=0;

  if (regnotstreamer) {
    /* write code in memory word by word */
    for (offset = 0; offset<size; offset+=4) {
      WriteRegister(c8fvp3_addr+dest_addr + offset, (*(gvh_u32_t*)(src_addr + offset)));
    }

  } else {
    /* write code in memory with streamer plug by pagesize byte page */
    for (page=0;page<size; page+=pagesize) {

      currsize = ((size-page)>pagesize) ? pagesize : (size-page);

      WriteBackSTBCh(
                     /* Streamer base address */
                     /* base_address */ c8fvp3_addr+HQVDP_LITE_STR_READ_REGS_BASEADDR,
                     /* Channel number */
                     /* chNb         */ DownloadCh,
                     /* Read channel (1) or write channel (0) */
                     /* rdNWr        */ 1,
                     /* External start address */
                     /* extsa        */ src_addr+page,
                     /* PLUG addressing mode */
                     /* plugam       */ STR_PLUGAM_RASTER,
                     /* STBUS Attribute Fields */
                     /* attr         */ 0,
                     /* External addressing mode */
                     /* extam        */ STR_EXTAM_LINEAR_BUFFER,
                     /* External two dimensional increment */
                     /* ext2d        */ 0,
                     /* External transfer size = size to transfer (in bytes) - 1 */
                     /* extsz        */ currsize - 1,
                     /* Endianness correction */
                     /* ec           */ STR_EC_BYTES_NOT_SWAPPED,
                     /* Data formatting */
                     /* df           */ STR_DF_NO_DEMUX,
                     /* Internal start address */
                     /* intsa        */ dest_addr+page, /*HQVDP_LITE_MEGACELL_OFFSET,*/
                     /* Internal reload address */
                     /* intra        */ 0,
                     /* Internal address increment = address increment to switch on the next circular buffer. */
                     /* intai        */ 0,
                     /* Internal decrement counter = remaining number of circular buffers to use */
                     /* intdc        */ 0,
                     /* Internal reload decrement counter = number of circular buffer to use - 1 */
                     /* intrc        */ 0,
                     /* Internal offset = offset between two consecutive sub-channels */
                     /* into         */ 0); /* Dummy value because we don't use channel 1*/

      WriteRegister(STR_ABS_BSF(c8fvp3_addr+HQVDP_LITE_STR_READ_REGS_BASEADDR), STR_BRSF_MASK(DownloadCh));
      while (ReadRegister(STR_ABS_BSF(c8fvp3_addr+HQVDP_LITE_STR_READ_REGS_BASEADDR)) & STR_BRSF_MASK(DownloadCh)) {
        yield();
      }
    }
  }
}
