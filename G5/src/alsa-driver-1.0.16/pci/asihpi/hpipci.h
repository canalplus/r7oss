/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

HPI PCI interface function definitions

(C) Copyright AudioScience Inc. 1996,1997,2003
******************************************************************************/

#ifndef _HPIPCI_H_
#define _HPIPCI_H_

/* PCI config reg defines */
#define HPIPCI_CDID 0x0		/* Vendor/Device Id */
#define HPIPCI_CSTR 0x0004
#define HPIPCI_CCMR 0x0004
#define HPIPCI_CCCR 0x0008
#define HPIPCI_CLAT 0x000C
#define HPIPCI_CBMA 0x0010	/* base memory address BAR0 */
#define HPIPCI_CBMB 0x0014	/* base memory address BAR1 */
#define HPIPCI_CBMC 0x0018	/* base memory address BAR2 */
#define HPIPCI_CBMD 0x001c	/* base memory address BAR3 */
#define HPIPCI_CBME 0x0020	/* base memory address BAR4 */
#define HPIPCI_CSUB 0x002C	/* sub-system and sub-vendor ID */
#define HPIPCI_CILP 0x00FC

/* bits in command register */
#define HPIPCI_CCMR_MSE			0x00000002
#define HPIPCI_CCMR_BM			0x00000004
#define HPIPCI_CCMR_PERR		0x0000040

/* NOTE : HpiPci function defintions moved to hpios.h */
#endif				/* _HPIPCI_H_ */
