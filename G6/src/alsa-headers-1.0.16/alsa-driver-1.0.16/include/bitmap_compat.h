#ifndef __SOUND_BITMAP_COMPAT_H
#define __SOUND_BITMAP_COMPAT_H

#include <linux/types.h>

#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(bits) (((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#endif

#define BITMAP_LAST_WORD_MASK(nbits)					\
(									\
	((nbits) % BITS_PER_LONG) ?					\
		(1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL		\
)

static inline void bitmap_zero(unsigned long *dst, int nbits)
{
	if (nbits <= BITS_PER_LONG)
		*dst = 0UL;
	else {
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset(dst, 0, len);
	}
}

static inline void bitmap_fill(unsigned long *dst, int nbits)
{
	size_t nlongs = BITS_TO_LONGS(nbits);
	if (nlongs > 1) {
		int len = (nlongs - 1) * sizeof(unsigned long);
		memset(dst, 0xff,  len);
	}
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}

static inline void bitmap_copy(unsigned long *dst, const unsigned long *src,
			       int nbits)
{
	if (nbits <= BITS_PER_LONG)
		*dst = *src;
	else {
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memcpy(dst, src, len);
	}
}

static inline void bitmap_and(unsigned long *dst, const unsigned long *src1,
			      const unsigned long *src2, int nbits)
{
	if (nbits <= BITS_PER_LONG)
		*dst = *src1 & *src2;
	else {
		int k;
		int nr = BITS_TO_LONGS(nbits);

		for (k = 0; k < nr; k++)
			dst[k] = src1[k] & src2[k];
	}
}

#endif /* __SOUND_BITMAP_COMPAT_H */
