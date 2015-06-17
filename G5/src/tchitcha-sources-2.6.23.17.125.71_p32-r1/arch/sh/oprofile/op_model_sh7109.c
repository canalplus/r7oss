/*
 * arch/sh/oprofile/op_model_sh7109.c
 *
 * Copyright (C) 2007 Dave Peverley
 *
 * PWM timer code shamelessly pilfered from :
 *   drivers/hwmon/stm-pwm.c
 *
 * Note that the "PWM" name is a bit of a red-herring as we're not really
 * using the PWM part of the peripheral, but the "compare" part of the
 * capture-compare side.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/smp.h>
#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/init.h>
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <asm/io.h>

#include "op_sh_model.h"



/*
 * Hard-wire the profiler sample frequency (in Hz)
 */
#define PROFILER_FREQ       500


/*
 * The capture-compare block uses the cpu clock @ 100MHz
 */
#define CC_CLK_FREQ         100000000

/* TODO : This should be defined elsewhere? */
#define SH7109_PWM_IRQ      126



struct stm_pwm_counter {
        unsigned long compare_increment;
};



static struct stm_pwm_counter results;

/*
 * PWM Registers
 */

#define SH7109_PWM_TIMER_BASE_ADDRESS   0x18010000

#define PWM0_VAL_REG                    0x00
#define PWM1_VAL_REG                    0x04
#define PWM0_CPT_VAL_REG                0x10
#define PWM1_CPT_VAL_REG                0x14
#define PWM0_CMP_VAL_REG                0x20
#define PWM1_CMP_VAL_REG                0x24
#define PWM0_CPT_EDGE_REG               0x30
#define PWM1_CPT_EDGE_REG               0x34
#define PWM0_CMP_OUT_VAL_REG            0x40
#define PWM1_CMP_OUT_VAL_REG            0x44
#define PWM_CTRL_REG                    0x50
#define PWM_INT_EN_REG                  0x54
#define PWM_INT_STA_REG                 0x58
#define PWM_INT_ACK_REG                 0x5C
#define PWM_CNT_REG                     0x60
#define PWM_CPT_CMP_CNT_REG             0x64

/* Bit PWM_CTRL Definitions. */
#define PWM_CTRL_PWM_CLK_VAL4_SHIFT     11
#define PWM_CTRL_PWM_CLK_VAL4_MASK      0x0f
#define PWM_CTRL_CPT_EN                 (1 << 10)
#define PWM_CTRL_PWM_EN                 (1 << 9)
#define PWM_CTRL_CPT_CLK_VAL_SHIFT      4
#define PWM_CTRL_CPT_CLK_VAL_MASK       0x1f
#define PWM_CTRL_PWM_CLK_VAL0_SHIFT     0
#define PWM_CTRL_PWM_CLK_VAL0_MASK      0x0f

/*
 * Bit Definitions for :
 *   PWM_INT_EN
 *   PWM_INT_STA
 *   PWM_INT_ACK
 * These are the same bits in all three registers.
 */
#define CMP1_INT_EN                     (1 << 6)
#define CMP0_INT_EN                     (1 << 5)
#define CPT1_INT_EN                     (1 << 2)
#define CPT0_INT_EN                     (1 << 1)
#define PWM_INT_EN                      (1 << 0)




/*
 * PWM Timer support
 *
 * The PWM peripheral has two identical programmable timers. We're going
 * to use PWM1, the second channel, exclusively for the profiler irq
 * generation.
 */

struct stm_pwm {
        struct resource *mem;
        unsigned long base;
};

struct stm_pwm *pwm;



/***********************************************************************
 * pwm_init()
 *
 * Initialise the PWM1 counter registers to a known state ready for
 * use.
 */
void pwm_init(void)
{
        u32 reg = 0;


        /* Disable PWM if currently running */
        reg = ctrl_inl(pwm->base + PWM_CTRL_REG);
        reg &= ~(PWM_CTRL_PWM_EN);
        reg &= ~(PWM_CTRL_CPT_EN);
        ctrl_outl(reg, pwm->base + PWM_CTRL_REG);

        /* Disable all PWM related interrupts */
        ctrl_outl(0, pwm->base + PWM_INT_EN_REG);

        /* Initial reload value for PWM1 counter. */
        ctrl_outl(0, pwm->base + PWM1_VAL_REG);

        return;
}



/***********************************************************************
 * pwm_set_frequency()
 *
 * Set the interrupt generation of the PWM1 counter. The frequency is
 * specified in HZ.
 * use.
 */
void pwm_set_frequency(unsigned int freq_hz)
{
        u32 reg = 0;
        u32 psc;


        printk(KERN_INFO "oprofile: Setting profiler frequency to %d Hz\n", freq_hz);

        /*
         * The input clock top the capture-compare is the CPU Clock which is 100MHz.
         * We can use this as-is for profiling.
         */


        /* Set initial capture counter clock prescale value to x1. */
        psc = 0x00;

        reg &= ~(PWM_CTRL_CPT_CLK_VAL_MASK << PWM_CTRL_CPT_CLK_VAL_SHIFT);
        reg |= (psc & PWM_CTRL_CPT_CLK_VAL_MASK) << PWM_CTRL_CPT_CLK_VAL_SHIFT;

        ctrl_outl(reg, pwm->base + PWM_CTRL_REG);


        /* PWM1 compare interrupt on value 0. */
        results.compare_increment = CC_CLK_FREQ / freq_hz;

        reg = (u32)results.compare_increment;
        ctrl_outl(reg, pwm->base + PWM1_CMP_VAL_REG);

        return;
}



/***********************************************************************
 * pwm_irq_enable()
 *
 * Enable interrupt generation by the PWM counter.
 */
void pwm_irq_enable(void)
{
        u32 reg = 0;


        /*
         * TODO : Just enable & ack all the sources for now!
         */

        reg = ctrl_inl(pwm->base + PWM_INT_ACK_REG);
        reg |= CMP1_INT_EN;
        //        reg |= CMP0_INT_EN;
        reg |= CPT1_INT_EN;
        //        reg |= CPT0_INT_EN;
        //        reg |= PWM_INT_EN;
        ctrl_outl(reg, pwm->base + PWM_INT_ACK_REG);

        reg = ctrl_inl(pwm->base + PWM_INT_EN_REG);
        reg |= CMP1_INT_EN;
        //        reg |= CMP0_INT_EN;
        reg |= CPT1_INT_EN;
        //        reg |= CPT0_INT_EN;
        //        reg |= PWM_INT_EN;
        ctrl_outl(reg, pwm->base + PWM_INT_EN_REG);

        return;
}



/***********************************************************************
 * pwm_irq_disable()
 *
 * Disable interrupt generation by the PWM counter.
 */
void pwm_irq_disable(void)
{
        u32 reg = 0;


        /* Disable the PWM1 interrupt source. */

        reg = ctrl_inl(pwm->base + PWM_INT_EN_REG);
        reg &= ~(CMP1_INT_EN);
        ctrl_outl(reg, pwm->base + PWM_INT_EN_REG);

        return;
}



/***********************************************************************
 * pwm_counter_start()
 *
 * Start the PWM1 counter
 */
void pwm_counter_start(void)
{
        volatile u32 reg = 0;


        /* Reset counters. */
        ctrl_outl(0, pwm->base + PWM_CNT_REG);
        ctrl_outl(0, pwm->base + PWM_CPT_CMP_CNT_REG);

        /* Enable the pwm counter. */
        reg = ctrl_inl(pwm->base + PWM_CTRL_REG);
        reg |= PWM_CTRL_PWM_EN;
        reg |= PWM_CTRL_CPT_EN;
        ctrl_outl(reg, pwm->base + PWM_CTRL_REG);

        return;
}



/***********************************************************************
 * pwm_counter_stop()
 *
 * Stop the PWM1 counter
 */
void pwm_counter_stop(void)
{
        u32 reg = 0;


        reg = ctrl_inl(pwm->base + PWM_CTRL_REG);
        reg |= PWM_CTRL_PWM_EN;
        ctrl_outl(reg, pwm->base + PWM_CTRL_REG);

        return;
}





/*
 * Hooks for oprofile
 */


/***********************************************************************
 * sh7109_setup_ctrs()
 *
 *
 */
static int sh7109_setup_ctrs(void)
{
        pwm_set_frequency(PROFILER_FREQ);

        return 0;
}



/***********************************************************************
 * sh7109_pwm_interrupt()
 *
 *
 */
static irqreturn_t sh7109_pwm_interrupt(int irq, void *dev_id)
{
        u32 reg = 0;
        struct pt_regs *regs = get_irq_regs();

        /* Give the sample to oprofile. */
        oprofile_add_sample(regs, 0);

        /* Update the compare value. */
        reg = ctrl_inl(pwm->base + PWM1_CMP_VAL_REG);
        reg += results.compare_increment;
        ctrl_outl(reg, pwm->base + PWM1_CMP_VAL_REG);

        /* Ack active irq sources. */
        reg = ctrl_inl(pwm->base + PWM_INT_STA_REG);
        ctrl_outl(reg, pwm->base + PWM_INT_ACK_REG);


        return IRQ_HANDLED;
}



/***********************************************************************
 * sh7109_stop()
 *
 *
 */
static void sh7109_stop(void)
{
        pwm_irq_disable();
        pwm_counter_stop();

        free_irq(SH7109_PWM_IRQ, &results);
}



/***********************************************************************
 * sh7109_start()
 *
 *
 */
static int sh7109_start(void)
{
        int ret;

        ret = request_irq(SH7109_PWM_IRQ, sh7109_pwm_interrupt, SA_INTERRUPT,
                        "SH7109 PWM", (void *)&results);

        if (ret < 0) {
                printk(KERN_ERR "oprofile: unable to request IRQ%d for SH7109 PWM\n",
                        SH7109_PWM_IRQ);
                return ret;
        }

        pwm_irq_enable();
        pwm_counter_start();

        return 0;
}



/***********************************************************************
 * sh7109_init()
 *
 *
 */
static int sh7109_init(void)
{
        int err = 0;


        /*
         * Allocate and map memory for SH7109 PWM timer registers.
         */

        pwm = kmalloc(sizeof(struct stm_pwm), GFP_KERNEL);
        if (pwm == NULL) {
                err = -ENOMEM;
                goto cleanup;
        }
        memset(pwm, 0, sizeof(*pwm));

        pwm->mem = request_mem_region(SH7109_PWM_TIMER_BASE_ADDRESS, 0x64, "oprofile pwm timer");
        if (pwm->mem == NULL) {
                printk(KERN_ERR "sh7109_init: failed to claim memory region\n");
                kfree(pwm);
                err = -EBUSY;
                goto cleanup;
        }

        pwm->base = (unsigned long)ioremap(SH7109_PWM_TIMER_BASE_ADDRESS, 0x64);
        if (pwm->base == (unsigned long)NULL) {
                printk(KERN_ERR "sh7109_init: failed ioremap");
                kfree(pwm);
                err = -EINVAL;
                goto cleanup;
        }

        /* Initialise the SH7109 PWM timer peripheral. */
        pwm_init();


 cleanup:
        return err;
}



/*
 * Hooks for the common oprofile support for the SH architecture.
 */
struct op_sh_model_spec op_sh7109_spec = {
        .init           = sh7109_init,
	.num_counters   = 0,  // TODO
	.setup_ctrs     = sh7109_setup_ctrs,
        .start          = sh7109_start,
        .stop           = sh7109_stop,
//        .name           = "sh/STb710x",
        .name           = "timer",  // TODO : Shouldn't this be STb710x or similar?
};
