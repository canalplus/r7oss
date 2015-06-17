#include "blitter_os.h"

#include "bdisp2/bdispII_aq_state.h"
#include "bdisp2/bdispII_debug.h"

const char *
bdisp2_get_palette_type_name(enum bdisp2_palette_type palette)
{
#define CMP(pal) case pal: return #pal
	switch (palette) {
	CMP(SG2C_NORMAL);
	CMP(SG2C_COLORALPHA);
	CMP(SG2C_INVCOLORALPHA);
	CMP(SG2C_ALPHA_MUL_COLORALPHA);
	CMP(SG2C_INV_ALPHA_MUL_COLORALPHA);
	CMP(SG2C_ONEALPHA_RGB);
	CMP(SG2C_INVALPHA_ZERORGB);
	CMP(SG2C_ALPHA_ZERORGB);
	CMP(SG2C_ZEROALPHA_RGB);
	CMP(SG2C_ZEROALPHA_ZERORGB);
	CMP(SG2C_ONEALPHA_ZERORGB);

	case SG2C_COUNT:
	default:
		break;
	}

	return "unknown";
#undef CMP
};
