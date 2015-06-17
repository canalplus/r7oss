/*
 * silencegen_transformer.c
 *
 * Copyright (C) STMicroelectronics Limited 2008. All rights reserved.
 *
 * Silence generating transformer.
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mempolicy.h>
#include <asm/cacheflush.h>
#else
#include <string.h>
#endif

#include <mme.h>
#include "silencegen.h"

static MME_ERROR SilenceGen_AbortCommand(void *ctx, MME_CommandId_t cmdId)
{
        /* This transformer supports neither aborting the currently executing 
         * transform nor does it utilize deferred transforms. For this reason
         * there is nothing useful it can do here.
         */

        return MME_INVALID_ARGUMENT;
}

static MME_ERROR SilenceGen_GetTransformerCapability(MME_TransformerCapability_t *capability)
{
        capability->Version = 0x010000;
        return MME_SUCCESS;
}

static MME_ERROR SilenceGen_InitTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **pctx)
{
        /* if we have a parameter structure ensure it is the correct size */
        if (paramsSize != 0) {
                return MME_INVALID_ARGUMENT;
        }

        /* supply the context pointer and return */
        *pctx = NULL;
        return MME_SUCCESS;
}

static MME_ERROR SilenceGen_Transform(void *ctx, MME_Command_t *cmd)
{
        int i, j;
        
        for (i=0; i<cmd->NumberOutputBuffers; i++) {
                MME_DataBuffer_t *dbuf = cmd->DataBuffers_p[cmd->NumberInputBuffers + i];
                
                for (j=0; j<dbuf->NumberOfScatterPages; j++) {
                        MME_ScatterPage_t *page = dbuf->ScatterPages_p + j;
                        memset(page->Page_p, 0, page->Size);
                }
        }
        
#ifdef __KERNEL__
        // TODO: most of the Linux cache flush functions "don't do what you think they do". We take a
        //       conservative approach here.
        flush_cache_all();
#endif
        
        return MME_SUCCESS;
}

static MME_ERROR SilenceGen_ProcessCommand(void *ctx, MME_Command_t *cmd)
{
        switch (cmd->CmdCode) {
        case MME_TRANSFORM:
                return SilenceGen_Transform(ctx, cmd);

        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        case MME_SEND_BUFFERS: /* not supported by this transformer */
        default:
                break;
        }

        return MME_INVALID_ARGUMENT;
}

static MME_ERROR SilenceGen_TermTransformer(void *ctx)
{
        return MME_SUCCESS;
}


MME_ERROR SilenceGen_RegisterTransformer(const char *name)
{
        return MME_RegisterTransformer(name,
                                       SilenceGen_AbortCommand,
                                       SilenceGen_GetTransformerCapability,
                                       SilenceGen_InitTransformer,
                                       SilenceGen_ProcessCommand,
                                       SilenceGen_TermTransformer);
}
