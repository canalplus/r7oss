/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : sbag.c

Description: programmation of the TLM SBAG-lite

************************************************************************/

#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/ptrace.h>

#include "memtest.h"

#define BA_ENABLE 		0x000
#define BA_MODE   		0x004
#define WPSAT_ENABLE 		0x020
#define WPSAT_PROG_MODE  	0x024
#define WPSAT_PROG_CLEAR  	0x02C
#define WPSAT_ITS		0x060
#define WPSAT_ITS_CLR		0x064
#define WPSAT_ITM_CLR		0x070
#define WPSAT_ITM_SET  		0x074
#define MSG_WP_TRACE_CONTROL	0x41C
#define WPSAT_REGBASE(i)   	(0x200 + 0x14 * (i))
#define WPSAT_REG1	   	0x0
#define WPSAT_REG2	   	0x4
#define WPSAT_REG3	   	0x8
#define WPSAT_REG4	   	0xC

#define NB_SATS			16 // must match config.xml
#define MAX_WPS			100

typedef struct
{
	char     description[100];
	uint32_t address;
	uint32_t size;
	int      interior;
	uint32_t source_id;
	uint32_t source_mask;
} SBAG_WP_t;

static uint32_t SBAG_NCp = 0;
static int wp_index = 0;
static SBAG_WP_t wp_array[MAX_WPS];
static int wp_start;


static void SBAG_Init(uint32_t BaseAdress, uint32_t Length)
{
	if (SBAG_NCp == 0)
		SBAG_NCp = (unsigned int)ioremap_nocache(BaseAdress, Length);
	if (SBAG_NCp == 0)
		pr_err("Error: Hades: Memtest: SBAG: cannot map base address %p\n", (void*)BaseAdress);
}

void SBAG_SetWatchpoint(const char* description, uint32_t address, uint32_t size, int interior, uint32_t source_id, uint32_t source_mask)
{
	SBAG_WP_t* wp = wp_array + wp_index;

	if (wp_index == MAX_WPS)
		pr_err("Error: Hades Memtest: unexpected error while setting watchpoint %d\n", wp_index);

	strncpy(wp->description, description, sizeof(wp->description));
	wp->description[sizeof(wp->description) - 1] = '\0';

	wp->address = address;
	wp->size = size;
	wp->interior = interior;
	wp->source_id = source_id;
	wp->source_mask = source_mask;

	++wp_index;
}

static void SetWatchpoint(int satellite, SBAG_WP_t* wp)
{
	uint32_t sat_mask;
	uint32_t word;

	if (satellite >= NB_SATS)
	{
		pr_err("Error: Hades Memtest: internal error, out of satellites\n");
		return;
	}
	sat_mask = 1 << satellite;

	// pr_info("Hades Memtest: watching buffer %s, [%#08x, %#08x], with source %d..\n", wp->description, wp->address,  wp->address + ( wp->size - 1),  wp->source_id);

	writel(0, (volatile void *)( SBAG_NCp + BA_ENABLE ));

	word = readl((volatile void*)(SBAG_NCp + WPSAT_ENABLE));
	writel(word | sat_mask, (volatile void *)( SBAG_NCp + WPSAT_ENABLE ));

	// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; WP message  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	writel(sat_mask, (volatile void *)( SBAG_NCp + WPSAT_PROG_MODE ));

    // caution: "interior" semantics inverse of physical SBAG
	writel((wp->address & 0xFFFFFFFC) |  wp->interior, (volatile void *)( SBAG_NCp + WPSAT_REGBASE(satellite) + WPSAT_REG1 ));
	writel((wp->address + ( wp->size - 1)) & 0xFFFFFFFC, (volatile void *)( SBAG_NCp + WPSAT_REGBASE(satellite) + WPSAT_REG2 ));
	writel(wp->source_id << 8, (volatile void *)( SBAG_NCp + WPSAT_REGBASE(satellite) + WPSAT_REG3 )); // no opcode, no ropcode
    // mask all but source, result + request enabled, no inversion
	writel(0x0FFF00FF | ( wp->source_mask << 8), (volatile void *)( SBAG_NCp + WPSAT_REGBASE(satellite) + WPSAT_REG4 ));

	word = readl((volatile void*)(SBAG_NCp + WPSAT_ITM_SET));
	writel(word | sat_mask, (volatile void *)( SBAG_NCp + WPSAT_ITM_SET )); // unmask interrupt
	writel(0x1000103, (volatile void *)( SBAG_NCp + MSG_WP_TRACE_CONTROL )); // include some data in WP message, value taken from SBAG-lite doc

	writel(1, (volatile void *)( SBAG_NCp + WPSAT_PROG_CLEAR )); // clear WPSAT_PROG_MODE, WPSAT_PROG_MODE_STATUS and WPSAT_PROG_CLEAR
}

void SBAG_Start(int skip)
{
	int satellite;
	int wp;
	int satellites;

	if (wp_index == 0)
		pr_err("Error: Hades Memtest: unexpected error, no watchpoint\n");

	SBAG_Init(0x09D00000, 0x1000);

	wp_start = skip % wp_index;

	if (wp_index < NB_SATS)
		satellites = wp_index;
	else
		satellites = NB_SATS;

	for (satellite = 0, wp = wp_start; satellite < satellites; satellite++)
	{
		SetWatchpoint(satellite, wp_array + wp);
		wp = (wp + 1) % wp_index;
	}
	pr_info("Hades Memtest: programmed %d watchpoints out of %d, starting at %d\n", satellites, wp_index, skip);

	// Make sure that WPSAT_ITS is cleared
	writel(0xFFFF, (volatile void *)( SBAG_NCp + WPSAT_ITS_CLR ));

	writel(0, (volatile void *)( SBAG_NCp + BA_MODE )); // INTERRUPT mode
	writel(1, (volatile void *)( SBAG_NCp + BA_ENABLE ));
}

void SBAG_Result(const char* hardware)
{
	int sat;
	uint32_t status;

	status = readl((volatile void*)(SBAG_NCp + WPSAT_ITS)); // read interrupt status
	// pr_info("Hades Memtest: interrupt status is %#08x\n", status);
	writel(0xFFFF, (volatile void *)( SBAG_NCp + WPSAT_ITM_CLR )); // mask interrupts for all watchpoints

	// Note: WPSAT_ITM is not connected in the SBAG_lite, need to disable the SBAG and/or satellites to stop interrupts
	writel(0, (volatile void *)( SBAG_NCp + WPSAT_ENABLE ));
	writel(0, (volatile void *)( SBAG_NCp + BA_ENABLE ));

	writel(status, (volatile void *)( SBAG_NCp + WPSAT_ITS_CLR )); // clear interrupt status

	if (status == 0)
		MT_LogPass(wp_index); // report requested number of watchpoints
	else for (sat = 0; sat < NB_SATS; sat++)
	{
		if (status & (1 << sat))
		{
			uint32_t reg1, reg2;
			uint32_t source, address;
			char* description = wp_array[wp_start + sat % wp_index].description;

			reg1 = readl((volatile void*)(SBAG_NCp + WPSAT_REGBASE(sat) + WPSAT_REG1));
			reg2 = readl((volatile void*)(SBAG_NCp + WPSAT_REGBASE(sat) + WPSAT_REG2));
			source = reg1 & 0x3FF;
			address = reg2 & 0xFFFFFFFC;
			pr_info("Hades Memtest: watchpoint %d triggered, %s, %s, address %#08x, source %d\n", sat, hardware, description, address, source);
			MT_LogFail(description, source, address);
		}
	}
	writel(0, (volatile void *)( SBAG_NCp + WPSAT_ENABLE )); // all disabled
	writel(0, (volatile void *)( SBAG_NCp + BA_ENABLE ));
	wp_index = 0;
}

