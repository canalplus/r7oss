/*
 * FB driver for the HX8347D LCD Controller
 *
 * Copyright (C) 2013 Christian Vogelgsang
 *
 * Based on driver code found here: https://github.com/watterott/r61505u-Adapter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_hx8347d"
#define WIDTH		320
#define HEIGHT		240

#if 1
#define DEFAULT_GAMMA	"0 0 0 0 0 0 0 0 0 0 0 0 0 0\n" \
			"0 0 0 0 0 0 0 0 0 0 0 0 0 0"
#else
#define DEFAULT_GAMMA	"01 07 07 1F 1C 3E 1B 6B 07 13 19 19 16\n" \
			"01 23 20 38 38 3E 14 64 09 06 06 06 18"
#endif

struct fbtft_par fbtft_locpar = {0};

/* Initial sequence from spec UMNH-8970MD-3T device from URT */
static int init_display(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);	
	memcpy(&fbtft_locpar,par,sizeof(struct fbtft_par));
	
	par->fbtftops.reset(par);

	/*start sequence*/
	write_reg(par, 0x18,0xAA);
	write_reg(par, 0x19,0x01);
	write_reg(par, 0x1A,0x04);
	/*power setting*/
	write_reg(par, 0x1F,0x88);
	mdelay(5);
	write_reg(par, 0x1F,0x80);
	mdelay(5);
	write_reg(par, 0x1F,0x90);
	mdelay(5);
	write_reg(par, 0x1F,0xD0);
	mdelay(5);
	/*Display ON*/
	write_reg(par, 0x28,0x38);
	mdelay(40);
	write_reg(par, 0x28,0x3C);
	write_reg(par, 0x36,0x01);//miroir
	write_reg(par, 0x17,0x05);
	/*Gamma 2.2*/
	write_reg(par, 0xEB,0x26);
	write_reg(par, 0x2F,0x11);
	write_reg(par, 0x29,0xFF);
	write_reg(par, 0x2B,0x02);
	write_reg(par, 0x2C,0x02);
	write_reg(par, 0x2D,0x04);
	write_reg(par, 0x2E,0x8D);
	write_reg(par, 0x18,0xAA);//NEW
	write_reg(par, 0x1A,0x01);
	write_reg(par, 0x1B,0x1E);
	write_reg(par, 0xE2,0x04);//NEW
	write_reg(par, 0xE3,0x04);//NEW
	write_reg(par, 0x24,0x2F);
	write_reg(par, 0x25,0x57);
	write_reg(par, 0x23,0xB3);

	write_reg(par, 0xE8,0x9C);
	write_reg(par, 0xEC,0x1C);
	write_reg(par, 0xED,0x1C);
	write_reg(par, 0xE4,0x01);
	write_reg(par, 0xE5,0x11);
	write_reg(par, 0xE6,0x11);
	write_reg(par, 0xE7,0x01);
	write_reg(par, 0xF3,0x00);
	write_reg(par, 0xF4,0x00);

	/*Color Enhancemant*/
	write_reg(par, 0xFF,0x01);
	write_reg(par, 0x71,0x00);
	write_reg(par, 0x72,0x06);
	write_reg(par, 0x73,0x04);
	write_reg(par, 0x74,0x02);
	write_reg(par, 0x75,0x00);
	write_reg(par, 0x76,0x01);/*Enable/Disable*/


	/*Full pump*/
	write_reg(par, 0xFF,0x02);
	write_reg(par, 0x0C,0x40);
	
	/*geometrie*/
	write_reg(par, 0xFF,0x00);
	write_reg(par, 0x16,0xE0);/*TLG: orientation 0xE0/0x20 pour double flip*/
	write_reg(par, 0x02,0x00);
	write_reg(par, 0x03,0x00);
	write_reg(par, 0x04,0x00);
	write_reg(par, 0x05,0xEF);
	write_reg(par, 0x06,0x00);
	write_reg(par, 0x07,0x00);
	write_reg(par, 0x08,0x01);
	write_reg(par, 0x09,0x3F);

	write_reg(par, 0x28,0x38);
	mdelay(40);
	write_reg(par, 0x28,0x3C);
	/*end of sequence*/
	
	/* gamma */
#if 0
	write_reg(par, 0x40 ,0x00);
	write_reg(par, 0x41 ,0x12);
	write_reg(par, 0x42 ,0x11);
	write_reg(par, 0x43 ,0x37);
	write_reg(par, 0x44 ,0x38);
	write_reg(par, 0x45 ,0x3C);
	write_reg(par, 0x46 ,0x0F);
	write_reg(par, 0x47 ,0x72);
	write_reg(par, 0x48 ,0x06);
	write_reg(par, 0x49 ,0x0C);
	write_reg(par, 0x4A ,0x10);
	write_reg(par, 0x4B ,0x13);
	write_reg(par, 0x4C ,0x1A);
	write_reg(par, 0x50 ,0x03);
	write_reg(par, 0x51 ,0x07);
	write_reg(par, 0x52 ,0x08);
	write_reg(par, 0x53 ,0x2E);
	write_reg(par, 0x54 ,0x2D);
	write_reg(par, 0x55 ,0x3E);
	write_reg(par, 0x56 ,0x0D);
	write_reg(par, 0x57 ,0x70);
	write_reg(par, 0x58 ,0x05);
	write_reg(par, 0x59 ,0x0C);
	write_reg(par, 0x5A ,0x0F);
	write_reg(par, 0x5B ,0x13);
	write_reg(par, 0x5C ,0x19);
	write_reg(par, 0x5D, 0xCC);
#endif

	return 0;
}


static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par,
		"%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);

	write_reg(par, 0x02, (xs >> 8) & 0xFF);
	write_reg(par, 0x03, xs & 0xFF);
	write_reg(par, 0x04, (xe >> 8) & 0xFF);
	write_reg(par, 0x05, xe & 0xFF);
	write_reg(par, 0x06, (ys >> 8) & 0xFF);
	write_reg(par, 0x07, ys & 0xFF);
	write_reg(par, 0x08, (ye >> 8) & 0xFF);
	write_reg(par, 0x09, ye & 0xFF);
	write_reg(par, 0x22);
}

/*
  Gamma string format:
    VRP0 VRP1 VRP2 VRP3 VRP4 VRP5 PRP0 PRP1 PKP0 PKP1 PKP2 PKP3 PKP4 CGM
    VRN0 VRN1 VRN2 VRN3 VRN4 VRN5 PRN0 PRN1 PKN0 PKN1 PKN2 PKN3 PKN4 CGM
*/
#define CURVE(num, idx)  curves[num*par->gamma.num_values + idx]
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	unsigned long mask[] = {
		0b111111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
		0b1111111, 0b1111111,
		0b11111, 0b11111, 0b11111, 0b11111, 0b11111,
		0b1111};
	int i, j;
	int acc = 0;

	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	/* apply mask */
	for (i = 0; i < par->gamma.num_curves; i++)
		for (j = 0; j < par->gamma.num_values; j++) {
			acc += CURVE(i, j);
			CURVE(i, j) &= mask[j];
		}

	if (acc == 0) /* skip if all values are zero */
		return 0;

	for (i = 0; i < par->gamma.num_curves; i++) {
		write_reg(par, 0x40 + (i * 0x10), CURVE(i, 0));
		write_reg(par, 0x41 + (i * 0x10), CURVE(i, 1));
		write_reg(par, 0x42 + (i * 0x10), CURVE(i, 2));
		write_reg(par, 0x43 + (i * 0x10), CURVE(i, 3));
		write_reg(par, 0x44 + (i * 0x10), CURVE(i, 4));
		write_reg(par, 0x45 + (i * 0x10), CURVE(i, 5));
		write_reg(par, 0x46 + (i * 0x10), CURVE(i, 6));
		write_reg(par, 0x47 + (i * 0x10), CURVE(i, 7));
		write_reg(par, 0x48 + (i * 0x10), CURVE(i, 8));
		write_reg(par, 0x49 + (i * 0x10), CURVE(i, 9));
		write_reg(par, 0x4A + (i * 0x10), CURVE(i, 10));
		write_reg(par, 0x4B + (i * 0x10), CURVE(i, 11));
		write_reg(par, 0x4C + (i * 0x10), CURVE(i, 12));
	}
	write_reg(par, 0x5D, (CURVE(1, 0) << 4) | CURVE(0, 0));
//	write_reg(par, 0x5D, 0x00);/*TLG*/
	return 0;
}
#undef CURVE


struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_gamma = set_gamma,
	},
};

/******************************PM****************************************/
void fbtft_get(struct fbtft_display *pdisplay,struct fbtft_par *ppar )
{
memcpy(pdisplay,&display,sizeof(struct fbtft_display));
memcpy(ppar,&fbtft_locpar,sizeof(struct fbtft_par));
}
/************************************************************************/

FBTFT_REGISTER_DRIVER(DRVNAME, &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);

MODULE_DESCRIPTION("FB driver for the HX8347D LCD Controller");
MODULE_AUTHOR("Christian Vogelgsang");
MODULE_LICENSE("GPL");
