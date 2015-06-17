/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
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
 @File   evt_test_subscriber.c
 @brief



*/
#ifndef _EVT_TEST_SUB_S2_H
#define _EVT_TEST_SUB_S2_H

#include "stm_event.h"
#include "infra_os_wrapper.h"
#include "evt_test_subscriber.h"

int evt_test_subscribe_init_S2(struct evt_test_subscriber_s *control_p);
int evt_test_set_subscription_S2(struct evt_test_subscriber_s *control_p);
int evt_test_modify_subscription_S2(struct evt_test_subscriber_s *control_p);
int evt_test_delete_subscription_S2(struct evt_test_subscriber_s *control_p);
int evt_test_subscriber_S2_set_interface(struct evt_test_subscriber_s *control_p);
#endif
