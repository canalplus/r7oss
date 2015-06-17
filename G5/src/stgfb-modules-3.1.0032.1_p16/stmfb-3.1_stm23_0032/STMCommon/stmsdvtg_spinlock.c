/***********************************************************************
 *
 * File: STMCommon/stmsdvtg_spinlock.c
 * Copyright (c) 2013 MathEmbedded. All rights reserved
 *
 * Provide a spinlock capability for VTG reset, such that all VTGs can
 * be (re)started in sync
\***********************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

/* The following functions provide a mechanism for ensuring that the VTG
   clocks get (re)synced with a predictable offset.
*/   

/* Control access to VTG clock (re)sync function. */
static spinlock_t VtgLock = SPIN_LOCK_UNLOCKED;

/* Local storage for interrupts */
static unsigned long flags = 0;

void vtg_spin_lock_irqsave( void )
{
    unsigned long flags;

    spin_lock_irqsave( &VtgLock, flags );
}

void vtg_spin_unlock_irqrestore( void )
{
    spin_unlock_irqrestore( &VtgLock, flags );
}

/* Specify the delay to apply in the vtg_delay() function */
static int vtg_delay_in_us=5000;

/* Declare module parameters for VTG delay */
module_param(vtg_delay_in_us, int, S_IRUGO);
MODULE_PARM_DESC(vtg_delay_in_us,"delay in uS");

/* Delay function */
void vtg_delay( void )
{
    udelay( vtg_delay_in_us );
}
