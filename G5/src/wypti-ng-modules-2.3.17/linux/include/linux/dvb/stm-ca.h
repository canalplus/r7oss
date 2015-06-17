/*
 * stm-ca.h
 *
 * Copyright (C) STMicroelectronics 2010 
 *
 * Author: Peter Bennett <peter.bennett@st.com>
 */

#ifndef _STMDVBCA_H_
#define _STMDVBCA_H_

//

typedef struct stm_ca_descr {
	unsigned int index;
	unsigned int parity;	/* 0 == even, 1 == odd */
	unsigned int mode;
	unsigned int valid;
	unsigned char cw[16];
	unsigned char iv[8];
} stm_ca_descr_t;

//

typedef struct stm_ca_multi2_system_key {
	unsigned char value[32];
} stm_ca_multi2_system_key_t;

//

#define STM_CA_MODE_AES				0x02
#define STM_CA_MODE_MULTI2			0x04
#define STM_CA_MODE_MULTI2_OFB_CHAINING 	0x24
#define STM_CA_MODE_MULTI2_CBC_CHAINING 	0x0c
#define STM_CA_MODE_MULTI2_CBC_DVS042_CHAINING 	0x1c

#define STM_CA_SET_DESCR     		 _IOW('o', 150, stm_ca_descr_t)
#define STM_CA_SET_MULTI2_SYSTEM_KEY     _IOW('o', 151, stm_ca_multi2_system_key_t)

#endif
