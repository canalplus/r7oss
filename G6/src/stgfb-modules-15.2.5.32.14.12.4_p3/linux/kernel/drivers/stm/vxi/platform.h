/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/vxi/platform.h
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_PLATFORM_H
#define _STM_PLATFORM_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stm_vxi.h>
typedef struct stm_vxi_data_swap_s {
   unsigned int             data_swap1;
   unsigned int             data_swap2;
   unsigned int             data_swap3;
   unsigned int             data_swap4;
} stm_vxi_data_swap_t;

typedef struct stm_vxi_input_s {
   unsigned int             input_id;
   stm_vxi_interface_type_t interface_type;
   union {
   unsigned int             swap656;
   stm_vxi_data_swap_t      swap;
   };
   unsigned int             bits_per_component;

} stm_vxi_input_t;


typedef struct stm_vxi_platform_board_s {
   unsigned int     num_inputs;
   stm_vxi_input_t *inputs;
} stm_vxi_platform_board_t;

typedef struct stm_vxi_platform_soc_s {
   unsigned int  reg;
   unsigned int  size;
} stm_vxi_platform_soc_t;


typedef struct stm_vxi_platform_data_s {
   stm_vxi_platform_soc_t      soc;
   stm_vxi_platform_board_t   *board;     // platform data pertaining to board
} stm_vxi_platform_data_t;

int stmvxi_get_platform_data(stm_vxi_platform_data_t *data);
void stmvxi_get_vxi_platform_data_size(unsigned int *size);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_PLATFORM_H */
