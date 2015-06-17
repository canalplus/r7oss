/*
 * linux/arch/sh/kernel/cpu/irq/st40_ilc.h
 *
 * Copyright (C) 2003 STMicroelectronics Limited
 * Author: Henry Bell <henry.bell@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupt handling for ST40 Interrupt Level Controler (ILC).
 */

extern void __iomem *ilc_base;

#define _BIT(_int)		     (1 << (_int % 32))
#define _REG_OFF(_int)		     (sizeof(int) * (_int / 32))
#define _BANK(irq)		     ((irq) >> 5)

#define ILC_BASE_STATUS              0x200
#define ILC_BASE_ENABLE              0x400

#define ILC_INTERRUPT_REG(_int)      (ilc_base + 0x080 + _REG_OFF(_int))
#define ILC_STATUS_REG(_int)         (ilc_base + 0x200 + _REG_OFF(_int))
#define ILC_CLR_STATUS_REG(_int)     (ilc_base + 0x280 + _REG_OFF(_int))
#define ILC_ENABLE_REG(_int)         (ilc_base + 0x400 + _REG_OFF(_int))
#define ILC_CLR_ENABLE_REG(_int)     (ilc_base + 0x480 + _REG_OFF(_int))
#define ILC_SET_ENABLE_REG(_int)     (ilc_base + 0x500 + _REG_OFF(_int))

#define ILC_EXT_WAKEUP_EN(_int)	     (ilc_base + 0x600 + _REG_OFF(_int))
#define ILC_EXT_WAKPOL_EN(_int)	     (ilc_base + 0x680 + _REG_OFF(_int))

#define ILC_PRIORITY_REG(_int)       (ilc_base + 0x800 + (8 * _int))
#define ILC_TRIGMODE_REG(_int)       (ilc_base + 0x804 + (8 * _int))

/*
 * Macros to get/set/clear ILC registers
 */
#define ILC_SET_ENABLE(_int)     writel(_BIT(_int), ILC_SET_ENABLE_REG(_int))
#define ILC_CLR_ENABLE(_int)     writel(_BIT(_int), ILC_CLR_ENABLE_REG(_int))
#define ILC_GET_ENABLE(_int)     (readl(ILC_ENABLE_REG(_int)) & _BIT(_int))
#define ILC_CLR_STATUS(_int)     writel(_BIT(_int), ILC_CLR_STATUS_REG(_int))
#define ILC_GET_STATUS(_int)     (readl(ILC_STATUS_REG(_int)) & _BIT(_int))
#define ILC_SET_PRI(_int, _pri)  writel((_pri), ILC_PRIORITY_REG(_int))

#define ILC_SET_TRIGMODE(_int, _mod) writel((_mod), ILC_TRIGMODE_REG(_int))

#define ILC_WAKEUP_ENABLE(_int)	writel(readl(ILC_EXT_WAKEUP_EN(_int)) |	\
				_BIT(_int), ILC_EXT_WAKEUP_EN(_int))

#define ILC_WAKEUP_DISABLE(_int) writel(readl(ILC_EXT_WAKEUP_EN(_int)) & \
				~_BIT(_int), ILC_EXT_WAKEUP_EN(_int))

#define ILC_WAKEUP_HI(_int)	writel(readl(ILC_EXT_WAKPOL_EN(_int)) | \
				_BIT(_int), ILC_EXT_WAKPOL_EN(_int))

#define ILC_WAKEUP_LOW(_int)	writel(readl(ILC_EXT_WAKPOL_EN(_int)) & \
				~_BIT(_int), ILC_EXT_WAKPOL_EN(_int))

#define ILC_WAKEUP(_int, high)	((high) ? (ILC_WAKEUP_HI(_int)) : \
				(ILC_WAKEUP_LOW(_int)))
#define ILC_TRIGGERMODE_NONE	0
#define ILC_TRIGGERMODE_HIGH	1
#define ILC_TRIGGERMODE_LOW	2
#define ILC_TRIGGERMODE_RISING	3
#define ILC_TRIGGERMODE_FALLING	4
#define ILC_TRIGGERMODE_ANY	5
