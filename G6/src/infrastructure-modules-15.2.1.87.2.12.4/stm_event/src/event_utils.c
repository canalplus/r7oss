/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   event_utils.c
 @brief



*/

#include "event_utils.h"

#define EVT_MAX_BITS_IN_UINT32		32

int32_t evt_mng_get_bit_pos(uint32_t value)
{
	uint32_t bit_pos = 0;

	for (; bit_pos < EVT_MAX_BITS_IN_UINT32; bit_pos++) {
		if (value & (1 << bit_pos))
			return bit_pos;

	}

	return -1;
}
