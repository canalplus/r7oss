/* Taps */
/* see ETSI TR 101 202 Table 4.3 and 4.5 */

#include <stdlib.h>
#include <string.h>

#include "dsmcc-biop-tap.h"
#include "dsmcc-debug.h"
#include "dsmcc-util.h"

static int parse_tap(struct biop_tap *tap, uint8_t *data, int data_length)
{
	int off = 0;
	uint16_t id;

	if (!dsmcc_getshort(&id, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("ID = 0x%hx", id);

	if (!dsmcc_getshort(&tap->use, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("Use = 0x%hx (%s)", tap->use, dsmcc_biop_get_tap_use_str(tap->use));

	if (!dsmcc_getshort(&tap->assoc_tag, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("Assoc = 0x%hx", tap->assoc_tag);

	if (!dsmcc_getbyte(&tap->selector_length, data, off, data_length))
		return -1;
	off++;
	DSMCC_DEBUG("Selector Length = %hhd", tap->selector_length);
	if (!dsmcc_memdup(&tap->selector_data, tap->selector_length, data, off, data_length))
		return -1;
	off += tap->selector_length;

	return off;
}

int dsmcc_biop_parse_taps_keep_only_first(struct biop_tap **tap0, uint16_t tap0_use, uint8_t *data, int data_length)
{
	int off = 0, ret, i;
	uint8_t taps_count;

	if (!dsmcc_getbyte(&taps_count, data, off, data_length))
		return -1;
	off++;
	if (taps_count < 1)
	{
		DSMCC_ERROR("Invalid number of taps (got %hhd but expected at least 1)", taps_count);
		return -1;
	}
	DSMCC_DEBUG("Taps count = %hhd", taps_count);

	for (i = 0; i < taps_count; i++)
	{
		struct biop_tap *tap = calloc(1, sizeof(struct biop_tap));

		ret = parse_tap(tap, data + off, data_length - off);
		if (ret < 0)
		{
			dsmcc_biop_free_tap(tap);
			return -1;
		}
		off += ret;

		/* first tap should be tap0_use */
		if (i == 0)
		{
			if (tap->use != tap0_use)
			{
				DSMCC_ERROR("Expected a first tap with %s, but got Use 0x%hx (%s)", dsmcc_biop_get_tap_use_str(tap0_use), tap->use, dsmcc_biop_get_tap_use_str(tap->use));
				dsmcc_biop_free_tap(tap);
				return -1;
			}

			*tap0 = tap;
		}
		else
		{
			DSMCC_DEBUG("Skipping tap %d Use 0x%hx (%s)", i, tap->use, dsmcc_biop_get_tap_use_str(tap->use));
			dsmcc_biop_free_tap(tap);
		}
	}

	return off;
}

const char *dsmcc_biop_get_tap_use_str(uint16_t use)
{
	switch (use)
	{
		case BIOP_DELIVERY_PARA_USE:
			return "BIOP_DELIVERY_PARA_USE";
		case BIOP_OBJECT_USE:
			return "BIOP_OBJECT_USE";
		default:
			return "Unknown";
	}
}
void dsmcc_biop_free_tap(struct biop_tap *tap)
{
	if (tap == NULL)
		return;

	if (tap->selector_data != NULL)
	{
		free(tap->selector_data);
		tap->selector_data = NULL;
	}

	free(tap);
}
