/*
 * Based on arch/arm/include/asm/hw_irq.h
 */
#ifndef _ASM_HW_IRQ_H
#define _ASM_HW_IRQ_H

void set_irq_flags(unsigned int irq, unsigned int flags);

#define IRQF_VALID	BIT(0)
#define IRQF_PROBE	BIT(1)
#define IRQF_NOAUTOEN	BIT(2)

#endif
