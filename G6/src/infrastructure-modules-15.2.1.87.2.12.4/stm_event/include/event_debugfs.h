/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/*
   @file     event_debugfs.h
   @brief

 */

#ifndef _EVENT_DEBUGFS_H
#define _EVENT_DEBUGFS_H

#if (defined SDK2_EVENT_ENABLE_DEBUGFS_STATISTICS)

#ifdef __cplusplus
extern "C" {
#endif

struct io_descriptor {
	size_t size;
	size_t size_allocated;
	void *data;
};

int event_create_debugfs(void);
void event_remove_debugfs(void);

#ifdef __cplusplus
}
#endif

#endif
#endif	/* _EVENT_DEBUGFS_H */
