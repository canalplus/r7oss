/*
 * pcm_transcoder_transformer.c
 *
 * Copyright (C) STMicroelectronics Limited 2009. All rights reserved.
 *
 * Linux kernel wrapper for the pcm transcoder transformer.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "mme.h"
#include "pcm_transcoder.h"

MODULE_DESCRIPTION("Pcm transcoding MME transformer.");
MODULE_AUTHOR("STMicroelectronics (R&D) Ltd.");
MODULE_LICENSE("GPL");

static char* TransformerName    = PCM_MME_TRANSFORMER_NAME;

module_param(TransformerName, charp, S_IRUGO);
MODULE_PARM_DESC(TransformerName, "Name to use for MME Transformer registration");

static int __init pcm_transcoder_init (void)
{
    MME_ERROR   Status  = MME_SUCCESS;

    Status              = PcmTranscoder_RegisterTransformer(TransformerName);
    if (Status != MME_SUCCESS)
    {
        printk ("Failed to register %s with MME\n", TransformerName);
        return -ENODEV;
    }
    printk ("%s registered with MME successfully\n", TransformerName);

    return 0;
}

static void __exit pcm_transcoder_exit (void)
{
    MME_ERROR   Status  = MME_SUCCESS;

    Status              = MME_DeregisterTransformer (TransformerName);
    if (Status != MME_SUCCESS)
        printk ("Failed to deregister %s with MME\n", TransformerName);
}

module_init(pcm_transcoder_init);
module_exit(pcm_transcoder_exit);
