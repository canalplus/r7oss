#ifndef __STM_BLITTER_CRC_UTILS_H__
#define __STM_BLITTER_CRC_UTILS_H__

int init_crc(char *filename , int generate);
int check_crc(void * buffer, size_t size );
int term_crc(void);

#endif /*__STM_BLITTER_CRC_UTILS_H__*/
