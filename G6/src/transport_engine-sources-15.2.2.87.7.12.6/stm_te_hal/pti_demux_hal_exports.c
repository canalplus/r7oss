#include <linux/init.h>		/* Initiliasation support */
#include <linux/module.h>	/* Module support */
#include <linux/kernel.h>	/* Kernel support */
#include <linux/version.h>	/* Kernel version */

#define EXPORT_SYMTAB

#include "objman/pti_object.h"
#include "pti_debug.h"
#include "pti_driver.h"

/* Debug facilities */
EXPORT_SYMBOL(stptiSUPPORT_ReturnPrintfBuffer);
EXPORT_SYMBOL(stptiSUPPORT_printf_fn);

/* HAL Driver */
EXPORT_SYMBOL(stptiAPI_DriverGetNumberOfpDevices);
EXPORT_SYMBOL(STPTI_Driver);
EXPORT_SYMBOL( STPTIHAL_entry );
EXPORT_SYMBOL( STPTIHAL_exit );
EXPORT_SYMBOL(STPTIHAL_PowerState);
EXPORT_SYMBOL(TSHAL_PowerState);

/* Object Manager */
EXPORT_SYMBOL(stptiOBJMAN_ReturnHALFunctionPool);
EXPORT_SYMBOL(stptiOBJMAN_ReturnTSHALFunctionPool);
EXPORT_SYMBOL(stptiOBJMAN_pDeviceObjectHandle);
EXPORT_SYMBOL(stptiOBJMAN_CheckHandle);
EXPORT_SYMBOL(stptiOBJMAN_ConvertFullObjectHandleToFullvDeviceHandle);
EXPORT_SYMBOL(stptiOBJMAN_ConvertFullObjectHandleToFullSessionHandle);
EXPORT_SYMBOL(stptiOBJMAN_HandleToObjectPointerWithChecks);
EXPORT_SYMBOL(stptiOBJMAN_HandleToObjectPointerWithoutChecks);
EXPORT_SYMBOL(stptiOBJMAN_ReturnAssociatedObjects);
EXPORT_SYMBOL(stptiOBJMAN_ReturnChildObjects);
EXPORT_SYMBOL(stptiOBJMAN_AllocateObjectAuxiliaryData);
EXPORT_SYMBOL(stptiOBJMAN_DeallocateObjectAuxiliaryData);
EXPORT_SYMBOL(stptiOBJMAN_RegisterObjectType);
EXPORT_SYMBOL(stptiOBJMAN_AllocateObject);
EXPORT_SYMBOL(stptiOBJMAN_AssociateObjects);
EXPORT_SYMBOL(stptiOBJMAN_DisassociateObjects);
EXPORT_SYMBOL(stptiOBJMAN_DeallocateObject);
EXPORT_SYMBOL(stptiOBJMAN_SerialiseObject);
EXPORT_SYMBOL(stptiOBJMAN_DeserialiseObject);
EXPORT_SYMBOL(stptiOBJMAN_IsAssociated);

/* Need to look to remove these */
EXPORT_SYMBOL(stptiOBJMAN_AllocateList);
EXPORT_SYMBOL(stptiOBJMAN_AllocateSizedList);
EXPORT_SYMBOL(stptiOBJMAN_ResizeList);
EXPORT_SYMBOL(stptiOBJMAN_AddToList);
EXPORT_SYMBOL(stptiOBJMAN_RemoveFromList);
EXPORT_SYMBOL(stptiOBJMAN_DefragmentList);
EXPORT_SYMBOL(stptiOBJMAN_FirstInList);
EXPORT_SYMBOL(stptiOBJMAN_NextInList);
EXPORT_SYMBOL(stptiOBJMAN_SearchListForItem);
EXPORT_SYMBOL(stptiOBJMAN_DeallocateList);
