/*
 * arch/sh/boards/st/custom002012/splash.h
 *
 * Copyright (C) 2012 Wyplay
 * Author: Vincent Penne (vpenne@wyplay.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

/*
 * this is a hack to display a very early logo at bootup on stx7105 socs via HDMI output in 720p
 *
 */


#define IS_SE

#ifndef IS_SE
# define fromSE(addr)							\
	(								\
		((addr) >= 0x40000000 && (addr) < 0x80000000)?		\
		((addr) + 0x0c000000 - 0x40000000):			\
		( ((addr) > 0x80000000 && (addr) < 0xc0000000)?	\
		  (addr) + 0x1c000000 - 0x80000000:			\
		  (addr) )						\
		)
#elif defined(ZREPLAY_IN_KERNEL)
# define fromSE(addr)							\
	(								\
		((addr) >= 0x40000000 && (addr) < 0x80000000)?		\
		((addr) + 0x80000000 - 0x40000000):			\
		(addr)							\
		)
#else
# define fromSE(addr) (addr)
#endif


static __initdata u32 zpokes[] = {
#include "7105_hdmi720p.h"
};

#include "draw_canal.h"

static void send_iframe(void)
{
	/* send HDMI info frame */
	if (!(1 & *((volatile u32 *)0xfd104230)))
		*((volatile u32 *)0xfd104230) = 1;
}

static void flush_fb(void)
{
	/* flush cache and wait a bit, on purpose */
	int i;
	for (i = 0; i < 1280*720; i++) {
		((volatile u32 *) fromSE(FBADDR))[i] = ((volatile u32 *) fromSE(FBADDR))[i];
	}
}

static void __init splash(void)
{
	u32 *src = zpokes;
	u32 *end = zpokes + sizeof(zpokes) / sizeof(zpokes[0]);

	while (src < end) {
		u32 opcode = *src++;
		volatile u32 *dst = (u32 *) *src++;
		dst = (u32 *) fromSE((u32)dst);
		
		if (opcode == 0) {
			u32 topoke = *src++;
			//topoke = fromSE(topoke);
			*dst = topoke;
		} else if (opcode == 1)
			*((char *) dst) = *src++;
		else if (opcode == 2) {
			u32 mask = *src++;
			u32 expected = *src++;
			while ( (mask & *dst) != expected )
				;
		} else if (opcode & 1) {
			// RLE
			while (opcode > 3) {
				u32 count = *src++;
				u32 topoke = *src++;
				//topoke = fromSE(topoke);
				while (count--) {
					opcode -= 4;
					*dst++ = topoke;
				}
			}
		} else {
			while (opcode > 3) {
				u32 topoke;
				opcode -= 4;
				topoke = *src++;
				//topoke = fromSE(topoke);
				*dst++ = topoke;
			}
		}
	}

	flush_fb();
	send_iframe();
	
	draw_canal();
	send_iframe();

	flush_fb();
	send_iframe();
}
