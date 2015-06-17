/*
 * pcm_transcoder.h
 *
 * Copyright (C) STMicroelectronics Limited 2009. All rights reserved.
 *
 * Header file for the silence generating transformer.
 */


#ifndef PCM_TRANSCODER_H_
#define PCM_TRANSCODER_H_

#define PCM_MME_TRANSFORMER_NAME        "PCM_TRANSCODER"

#ifndef ENABLE_PCM_DEBUG
#define ENABLE_PCM_DEBUG                0
#endif

#define PCM_DEBUG(fmt, args...)         ((void) (ENABLE_PCM_DEBUG && \
                                                 (printk(KERN_INFO "%s: " fmt, __FUNCTION__,##args), 0)))

/* Output trace information off the critical path */
#define PCM_TRACE(fmt, args...)         (printk(KERN_NOTICE "%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define PCM_ERROR(fmt, args...)         (printk(KERN_CRIT "ERROR in %s: " fmt, __FUNCTION__, ##args))


MME_ERROR PcmTranscoder_RegisterTransformer(const char *);

#endif /*PCM_TRANSCODER_H_*/
