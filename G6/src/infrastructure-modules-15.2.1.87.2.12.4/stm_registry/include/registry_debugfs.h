/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided “AS IS”, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   registry_debugfs.h
 @brief
*/



#ifndef _REGISTRY_DEBUGFS_H
#define _REGISTRY_DEBUGFS_H

#ifdef __cplusplus
extern "C" {
#endif

#if (defined SDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES)

int registry_create_debugfs(void);
void registry_remove_debugfs(void);

#endif

#ifdef __cplusplus
}
#endif

#endif	/* _REGISTRY_DEBUGFS_H */
