#ifndef __CRC_H
#define __CRC_H
#include <linux/types.h>
#define CRC_BLOCK_SIZE   256
#define CRC_VERSION_SIZE 12
#define CRC_DATE_SIZE    16

typedef struct crc_header
{
    uint32_t magic_no;
    uint32_t crc;
    char version[CRC_VERSION_SIZE];
    char date[CRC_DATE_SIZE];
    uint32_t size;
} CRC_HEADER;
#define CRC_HEADER_LEN     sizeof(CRC_HEADER)

extern uint32_t crc32(uint32_t crc, const char *buf, uint32_t len);

#endif /* __CRC_H */
