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

 Array initializer for PCI card IDs

(C) Copyright AudioScience Inc. 1998-2003
*******************************************************************************/

/*NOTE: when adding new lines to this header file
  they MUST be grouped by HPI entry point.
*/

{
HPI_PCI_VENDOR_ID_TI, HPI_ADAPTER_DSP6205,
		HPI_PCI_VENDOR_ID_AUDIOSCIENCE, PCI_ANY_ID, 0, 0,
		(kernel_ulong_t) HPI_6205}
, {
HPI_PCI_VENDOR_ID_TI, HPI_ADAPTER_PCI2040,
		HPI_PCI_VENDOR_ID_AUDIOSCIENCE, PCI_ANY_ID, 0, 0,
		(kernel_ulong_t) HPI_6000}
, {
HPI_PCI_VENDOR_ID_MOTOROLA, HPI_ADAPTER_DSP56301,
		HPI_PCI_VENDOR_ID_AUDIOSCIENCE, PCI_ANY_ID, 0, 0,
		(kernel_ulong_t) HPI_4000}
,
	/* look for ASI cards that have 0x12cf sub-vendor ID,
	   like the 4300 and 4601 */
{
HPI_PCI_VENDOR_ID_MOTOROLA, HPI_ADAPTER_DSP56301, 0x12CF, PCI_ANY_ID,
		0, 0, (kernel_ulong_t) HPI_4000}
,
	/* look for ASI cards that have sub-vendor-ID = 0,
	   like the 4501, 4113 and 4215 revC and below */
{
HPI_PCI_VENDOR_ID_MOTOROLA, HPI_ADAPTER_DSP56301, 0, PCI_ANY_ID, 0, 0,
		(kernel_ulong_t) HPI_4000}
, {
0,}
