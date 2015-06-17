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
 @File   ASC_INTERNAL
 @brief
*/
#ifndef _ASC_HAL_H
#define _ASC_HAL_H

#include "stm_asc_scr.h"
#include "asc_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

ca_error_code_t asc_set_autoparity_rejection(asc_ctrl_t *asc_ctrl_p,
			uint8_t autoparityrejection);

ca_error_code_t asc_set_flow_control(asc_ctrl_t *asc_ctrl_p,
			uint8_t flowcontrol);
ca_error_code_t asc_set_nack(asc_ctrl_t *asc_ctrl_p,uint8_t NACK);

ca_error_code_t asc_set_stopbits(volatile asc_regs_t  *asc_regs_p,
			stm_asc_scr_stopbits_t stopbits);

ca_error_code_t asc_set_databits(volatile asc_regs_t *asc_regs_p,
			stm_asc_scr_databits_t databits);

ca_error_code_t asc_set_baudrate(asc_ctrl_t *asc_ctrl_p,int baud);

ca_error_code_t asc_set_protocol(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_protocol_t protocol);

#ifdef __cplusplus
}
#endif

#endif /**/
