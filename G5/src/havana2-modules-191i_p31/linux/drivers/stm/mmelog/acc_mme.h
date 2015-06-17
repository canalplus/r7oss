// This file abstracts Multicom/mme.h for debugging of ACC_Multicom Transformers

#ifndef _ACC_MME_H_
#define _ACC_MME_H_

#define	_ACC_WRAPP_MME_	0

#if _ACC_WRAPP_MME_ 

#ifndef _ACC_MME_WRAPPER_C_

// Abstract multicom calls 

#define MME_InitTransformer			acc_MME_InitTransformer
#define MME_SendCommand				acc_MME_SendCommand    
#define MME_AbortCommand			acc_MME_AbortCommand    
#define MME_TermTransformer			acc_MME_TermTransformer
#define MME_GetTransformerCapability		acc_MME_GetTransformerCapability

#endif // _ACC_MME_WRAPPER_C_

#endif // _ACC_WRAPP_MME_


#include "mme.h"

#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else
#define _EXTERN_C_ extern
#endif

_EXTERN_C_ MME_ERROR acc_MME_InitTransformer(const char *Name, MME_TransformerInitParams_t * Params_p, MME_TransformerHandle_t * Handle_p);
_EXTERN_C_ MME_ERROR acc_MME_GetTransformerCapability(const char * TransformerName, MME_TransformerCapability_t *TransformerInfo_p);
_EXTERN_C_ MME_ERROR acc_MME_SendCommand(MME_TransformerHandle_t Handle, MME_Command_t * CmdInfo_p);
_EXTERN_C_ MME_ERROR acc_MME_TermTransformer(MME_TransformerHandle_t handle);



#endif // _ACC_MME_H_


