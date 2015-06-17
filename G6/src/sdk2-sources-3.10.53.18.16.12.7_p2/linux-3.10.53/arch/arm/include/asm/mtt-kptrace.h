/*
 *  KPTrace - KProbes-based tracing
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2010
 */
#ifndef _MTT_ARCH_ARM_H_
#define _MTT_ARCH_ARM_H_

#define REG_EIP	ARM_pc
#define REG_SP		ARM_sp
#define REG_RET	uregs[0]
#define REG_ARG0	uregs[0]
#define REG_ARG1	uregs[1]
#define REG_ARG2	uregs[2]
#define REG_ARG3	uregs[3]

#endif /* _MTT_ARCH_ARM_H_ */
