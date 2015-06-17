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
 @File   stm_gearbox.h
 @brief
*/
#ifndef STM_GEARBOX_H
#define STM_GEARBOX_H

typedef struct {
	uint32_t size;
	uint8_t* address;
} stm_gearbox_param_t;

typedef struct {
	uint32_t address;
	uint32_t value;
	uint8_t size;
} stm_gearbox_reg_t;

int stm_gearbox_register_read(stm_gearbox_reg_t*);
int stm_gearbox_register_write(stm_gearbox_reg_t*);
int stm_gearbox_handle_tuning(stm_gearbox_param_t*);

#endif
