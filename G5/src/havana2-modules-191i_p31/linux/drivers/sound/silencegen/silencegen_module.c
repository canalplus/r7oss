/*
 * silencegen_transformer.c
 *
 * Copyright (C) STMicroelectronics Limited 2008. All rights reserved.
 *
 * Linux kernel wrapper for the silence generating transformer.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "mme.h"
#include "silencegen.h"

MODULE_DESCRIPTION("Trivial silence generating MME transformer.");
MODULE_AUTHOR("STMicroelectronics (R&D) Ltd.");
MODULE_LICENSE("GPL");

static char *name = MME_SILENCE_GENERATOR;

module_param( name , charp, S_IRUGO);
MODULE_PARM_DESC(name, "Name to use for MME Transformer registration");

static int __init silencegen_init (void)
{
        MME_ERROR status = MME_SUCCESS;
        
        status = SilenceGen_RegisterTransformer(name);
        if (MME_SUCCESS != status)
                printk("Failed to register %s with MME\n", name);
        
        return (0 == status ? 0 : -ENODEV);
}

static void __exit silencegen_exit (void)
{
        MME_ERROR status;
        
        status = MME_DeregisterTransformer(name);
        if (MME_SUCCESS != status)
                printk("Failed to deregister %s with MME\n", name);
}

module_init(silencegen_init);
module_exit(silencegen_exit);
