/*
 * c8jpg-macros.h, various macros for registers access
 *
 * Copyright (C) 2012, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __STM_C8JPG_MACROS_H
#define __STM_C8JPG_MACROS_H

#include <linux/types.h>
#include <linux/slab.h>

#include "c8jpg-regs.h"

static inline
uint32_t _generic_read(void __iomem *base, uint32_t addr)
{
	return readl(base + addr);
}

static inline
void _generic_write(void __iomem *base, uint32_t addr, uint32_t data)
{
	writel(data, base + addr);
}

#define _GENERIC_REG(reg) \
	(reg##_OFFSET)
#define _GENERIC_MASK(reg, field) \
	(reg##_##field##_MASK)
#define _GENERIC_OFFSET(reg, field) \
	(reg##_##field##_OFFSET)

#define READ_REG(base, reg) \
	(_generic_read(base, _GENERIC_REG(reg)))

#define WRITE_REG(base, reg, value) \
	(_generic_write(base, _GENERIC_REG(reg), (uint32_t)(value)))

#define REG_OFFSET(reg) \
	(_GENERIC_REG(reg))

#define WRITE_ADDR(base, addr, value) \
	(_generic_write(base, addr, (uint32_t)(value)))

#define SET_FIELD(reg, field, value) \
	(((uint32_t)(value) << _GENERIC_OFFSET(reg, field)) \
		& _GENERIC_MASK(reg, field))

#endif /* __STM_C8JPG_MACROS_H */
