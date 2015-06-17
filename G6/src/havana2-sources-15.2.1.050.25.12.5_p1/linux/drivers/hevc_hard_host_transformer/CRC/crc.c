/*!
 * \file crc.c
 *
 * \brief CRC computation
 */
#include "crc.h"

#define CRC_POLYNOMIAL 0x04c11db7u //!< Polynomial for CRC computation

uint32_t
crc_update
(       uint32_t word,
        uint8_t  bits,
        uint32_t result)
{
        uint8_t i;
        uint32_t mask;

        word <<= 32-bits;
        for(i=0;i<bits;i++) {
                mask = (result^word)>>31;
                result <<= 1;
                word <<= 1;
                if(mask)
                        result ^= CRC_POLYNOMIAL;
        }
        return result;
}

uint32_t
crc_rot
(       uint8_t *base,
        uint32_t start,
        uint32_t stop,
        uint32_t end)
{
        uint32_t result = CRC_INIT;

        if(stop<start) {
                for(;start<end;start++)
                        result = crc_update(base[start],8,result);
                start = 0;
        }
        for(;start<stop;start++)
                result = crc_update(base[start],8,result);
        return result;
}

uint32_t
crc_rot_swap
(	uint8_t *base,
	uint32_t start,
	uint32_t stop,
	uint32_t end)
{
	uint32_t result = CRC_INIT;

	// Assumption: start/stop/end shall be a multiple of 8 bytes

	if(stop<start) {
		for(;start<end;start+=8) {
			result = crc_update(base[start+4],8,result);
			result = crc_update(base[start+5],8,result);
			result = crc_update(base[start+6],8,result);
			result = crc_update(base[start+7],8,result);
			result = crc_update(base[start+0],8,result);
			result = crc_update(base[start+1],8,result);
			result = crc_update(base[start+2],8,result);
			result = crc_update(base[start+3],8,result);
		}
		start = 0;
	}
	for(;start<stop;start+=8) {
		result = crc_update(base[start+4],8,result);
		result = crc_update(base[start+5],8,result);
		result = crc_update(base[start+6],8,result);
		result = crc_update(base[start+7],8,result);
		result = crc_update(base[start+0],8,result);
		result = crc_update(base[start+1],8,result);
		result = crc_update(base[start+2],8,result);
		result = crc_update(base[start+3],8,result);
	}
	return result;
}

uint32_t
crc_1d
(       uint8_t *buffer,
        uint32_t size)
{
        uint32_t i, result = CRC_INIT;
        uint8_t bits = 8*sizeof(uint8_t);

        for(i=0;i<size;i++)
                result = crc_update(buffer[i],bits,result);

        return result;
}

uint32_t
crc_2d
(       uint8_t *buffer,
        uint16_t stride,
        uint16_t h_size,
        uint16_t v_size)
{
        uint16_t i, j;
        uint32_t result = CRC_INIT;
        uint8_t bits = 8*sizeof(uint8_t);

        for(j=0;j<v_size;j++) {
                for(i=0;i<h_size;i++)
                        result = crc_update(buffer[i],bits,result);
                buffer += stride;
        }
        return result;
}

uint32_t
crc_3d
(       uint8_t *buffer1,
        uint8_t *buffer2,
        uint16_t stride,
        uint16_t h_size,
        uint16_t v_size)
{
        uint16_t i, j;
        uint32_t result = CRC_INIT;
        uint8_t bits = 8*sizeof(uint8_t);

        for(j=0;j<v_size;j++) {
                for(i=0;i<h_size;i++) {
                        result = crc_update(buffer1[i],bits,result);
                        result = crc_update(buffer2[i],bits,result);
                }
                buffer1 += stride;
                buffer2 += stride;
        }
        return result;
}
