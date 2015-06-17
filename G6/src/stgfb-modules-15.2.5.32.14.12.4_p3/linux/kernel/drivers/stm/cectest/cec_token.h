/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cectest/cec_token.h
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*
   @file     cec_token.h
   @brief

 */
#ifndef CEC_TOKEN_H_
#define CEC_TOKEN_H_

typedef enum{
	UNKNOWN = 0,
	STRING = 1,
	INTEGER = 2,
	HEX = 3
}cec_type_t;

char *get_tc_name(char *buf);

int check_for_string (char *buf, void *data_p, uint8_t size);

int check_for_integer (char *buf, void *data_p);

int get_arg(void **data_p,cec_type_t type , void *argument, uint8_t arg_size);

int get_arg_size(char *data_p);

int get_number_of_tokens(char *data_p);

#endif
