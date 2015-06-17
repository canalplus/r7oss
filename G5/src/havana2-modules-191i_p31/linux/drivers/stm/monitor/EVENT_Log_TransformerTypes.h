/*
**  @file     : EVENT_Log_TransformerTypes.h
**
**  @brief    : Event logging transformer
**
**  @date     : August 6th 2008
**
**  &copy; 2008 ST Microelectronics. All Rights Reserved.
*/


#ifndef __EVENT_LOG_TRANSFORMER_TYPES_H
#define __EVENT_LOG_TRANSFORMER_TYPES_H

#include "stddefs.h"

#include "mme.h"

#define EVENT_LOG_MME_TRANSFORMER_NAME    "EVENT_LOG"
#define EVENT_LOG_MAX_MESSAGE_TEXT        128

/*
** EVENT_LOG_SetGlobalParamSequence_t :
*/
typedef struct {
	U32                  StructSize;                     /* Size of the structure in bytes */
} EVENT_LOG_SetGlobalParamSequence_t;

/*
** EVENT_LOG_TransformParam_t :
*/
typedef struct {
	U32                          StructSize;                         /* Size of the structure in bytes */
} EVENT_LOG_TransformParam_t;



/*
** EVENT_LOG_InitTransformerParam_t:
*/
typedef struct {
	U32                          StructSize;                         /* Size of the structure in bytes */
	U32                          TimeCodeMemoryAddress;              /* Physical Memory address of the time code */
} EVENT_LOG_InitTransformerParam_t;

/*
** EVENT_LOG_CommandStatus_t:
*/
typedef struct {
	U32                  StructSize;                       /* Size of the structure in bytes */
} EVENT_LOG_TransformerCapability_t;

typedef struct {
	U32 StructSize;  /* Size of the structure in bytes */

	U32 EventID;     /* 0 = No Event */
	U32 TimeCode;
	U32 Parameters[4];
	unsigned char Message[EVENT_LOG_MAX_MESSAGE_TEXT];

	U32 Discarded;
} EVENT_LOG_CommandStatus_t;

#endif 
