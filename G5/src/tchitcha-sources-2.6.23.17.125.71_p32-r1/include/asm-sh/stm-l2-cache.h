/*
 * Copyright (C) 2009 STMicroelectronics
 * Written by Richard P. Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef __ASM_SH_STM_L2_CACHE_H
#define __ASM_SH_STM_L2_CACHE_H

void stm_l2_flush_wback_region(void *start, int size);
void stm_l2_flush_purge_region(void *start, int size);
void stm_l2_flush_invalidate_region(void *start, int size);

#endif /* __ASM_SH_STM_L2_CACHE_H */
