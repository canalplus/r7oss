#ifndef DSMCC_BIOP_TAP_H
#define DSMCC_BIOP_TAP_H

#include <stdint.h>

enum
{
	BIOP_DELIVERY_PARA_USE = 0x0016,
	BIOP_OBJECT_USE        = 0x0017
};

struct biop_tap
{
	uint16_t use;
	uint16_t assoc_tag;
	uint8_t  selector_length;
	uint8_t *selector_data;
};

int dsmcc_biop_parse_taps_keep_only_first(struct biop_tap **tap0, uint16_t tap0_use, uint8_t *data, int data_length);
const char *dsmcc_biop_get_tap_use_str(uint16_t use);
void dsmcc_biop_free_tap(struct biop_tap *tap);

#endif /* DSMCC_BIOP_TAP_H */
