
/*****************************************************************************/
/* COPYRIGHT (C) 2012 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 *  @File   mss_test_consumer.h
 *   @brief
 *   */

#ifndef _MSS_TEST_CONSUMER_H
#define _MSS_TEST_CONSUMER_H


int	mss_test_consumer_init(void);
void	mss_test_consumer_term(void);

int	mss_test_consumer_create_new(stm_object_h *consumer_h);
int	mss_test_consumer_delete(stm_object_h consumer_h);


#endif
