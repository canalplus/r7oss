
#include <linux/module.h>

#include "lowpower_frontpanel.h"

#define __lpm_code __attribute__ ((section (".text.lpm")))

#define xstr(s) #s
#define str(s) xstr(s)


extern const unsigned char lpfp_bitmap[];

/* this array will be loaded in cache and used as a stack, except the first two
 * entries which contain start and end address of executable code */
unsigned long ____cacheline_aligned lpfp_data[LPFP_DATA_SIZE];


#define RegistreWrite32(addr, data)	(*(volatile unsigned int *)(addr) = (unsigned int)data)

#define ST7105_PIO3_BASE_ADDRESS	0xFD023000
#define ST7105_PIO11_BASE_ADDRESS	0xFE014000


#define setCLK()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO3] + 0x04, 0x10)
#define setDAT()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO3] + 0x04, 0x20)
#define setSTRB()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO11] + 0x04, 0x20)

#define clearCLK()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO3] + 0x08, 0x10)
#define clearDAT()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO3] + 0x08, 0x20)
#define clearSTRB()			RegistreWrite32(lpfp_data[LPFP_INDEX_PIO11] + 0x08, 0x20)



__asm__ (
".balign 32\n"
".global lpfp_display_time\n"
".section .text.lpm\n"
"lpfp_display_time:\n"
/* NO code MUST be before this */

/* change stack  */
"mov.l __addr_data, r0\n"
"mov.l r15, @-r0\n"
"mov r0, r15\n"

/* save registers */
"sts.l pr, @-r15\n"
"mov.l r1, @-r15\n"
"mov.l r2, @-r15\n"
"mov.l r3, @-r15\n"
"mov.l r4, @-r15\n"
"mov.l r5, @-r15\n"
"mov.l r6, @-r15\n"
"mov.l r7, @-r15\n"
"mov.l r8, @-r15\n"
"mov.l r9, @-r15\n"
"mov.l r10, @-r15\n"
"mov.l r11, @-r15\n"
"mov.l r12, @-r15\n"
"mov.l r13, @-r15\n"
"mov.l r14, @-r15\n"

/* call C function */
/* r4 and r5 contain the parameters, there are already set by the caller */
"bsr lpfp_set_time\n"
"nop\n"

/* restore registers & stack */
"mov.l @r15+, r14\n"
"mov.l @r15+, r13\n"
"mov.l @r15+, r12\n"
"mov.l @r15+, r11\n"
"mov.l @r15+, r10\n"
"mov.l @r15+, r9\n"
"mov.l @r15+, r8\n"
"mov.l @r15+, r7\n"
"mov.l @r15+, r6\n"
"mov.l @r15+, r5\n"
"mov.l @r15+, r4\n"
"mov.l @r15+, r3\n"
"mov.l @r15+, r2\n"
"mov.l @r15+, r1\n"
"lds.l @r15+, pr\n"
"mov.l @r15, r15\n"

/* return */
"rts\n"
"nop\n"

".align 2\n"
"__addr_data: .long (lpfp_data + " str(LPFP_DATA_SIZE) " * 4)\n"
);




static __lpm_code inline void delay(void)
{
	int i;

	for (i = 0; i < 10; i++)
		__asm__ __volatile__ ("nop");
}


static __lpm_code void start_cmd(void)
{
	setSTRB();
	delay();
	clearSTRB();
	delay();
}


static __lpm_code void stop_cmd(void)
{
	delay();
	setSTRB();
	delay();
}


static __lpm_code void send_byte(unsigned char data)
{
	unsigned char bit;

	delay();

	for (bit = 0; bit < 8; bit++) {
		clearCLK();
		delay();

		if (data & 1)
			setDAT();
		else
			clearDAT();
		delay();

		setCLK();
		delay();

		data >>= 1;
	}
}


/* lpfp_bitmap is read-only data, it can be included in .text section just as
regular executable code */
__asm(
".balign 32\n"
".section .text.lpm\n"
".global lpfp_bitmap\n"
"lpfp_bitmap: \n"
".byte 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F\n"
);


static __lpm_code void display_time(unsigned char hour, unsigned char min)
{
	start_cmd();
	/* write */
	send_byte(0x00);
	send_byte(lpfp_bitmap[hour / 10]);
	send_byte(lpfp_bitmap[hour % 10]);
	send_byte(lpfp_bitmap[min / 10]);
	send_byte(lpfp_bitmap[min % 10]);
	stop_cmd();

	start_cmd();
	/* display ":" */
	send_byte(0x24);
	send_byte(0x03);
	stop_cmd();

	start_cmd();
	/* low brightness */
	send_byte(0x30);
	send_byte(0x05 | 0x18);
	stop_cmd();

	start_cmd();
	/* display on */
	send_byte(0x2d);
	stop_cmd();
}


__attribute__((used)) static __lpm_code void lpfp_set_time(unsigned int timestamp)
{
	unsigned char hour, min;

	/* load default values for PIO register addresses */
	lpfp_data[LPFP_INDEX_PIO3] = ST7105_PIO3_BASE_ADDRESS;
	lpfp_data[LPFP_INDEX_PIO11] = ST7105_PIO11_BASE_ADDRESS;


	/* process timestamp decomposition */
	timestamp %= 86400;
	hour = timestamp / 3600;
	timestamp %= 3600;
	min = timestamp / 60;
	timestamp %= 60;

	/* the front panel cannot display seconds,
	so we round up to the next minute */
	if (timestamp > 30)
		min++;
	while (min >= 60) {
		min -= 60;
		hour++;
	}
	while (hour >= 24)
		hour -= 24;

	display_time(hour, min);
}
