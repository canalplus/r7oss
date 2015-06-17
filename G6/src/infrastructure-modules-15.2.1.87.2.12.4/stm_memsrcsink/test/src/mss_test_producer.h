#ifndef _MSS_TEST_PRODUCER_H
#define _MSS_TEST_PRODUCER_H


int	  mss_test_producer_create_new(stm_object_h *producer_h);
int	  mss_test_producer_delete(stm_object_h producer_h);

uint32_t mss_test_producer_attach(stm_object_h consumer_p, stm_memsink_h snk_p);
uint32_t mss_test_producer_detach(stm_object_h consumer_p, stm_memsink_h snk_p);

#endif
