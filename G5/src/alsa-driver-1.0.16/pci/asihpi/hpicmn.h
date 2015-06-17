/**

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

*/

struct hpi_adapter_obj {
	struct hpi_pci Pci;	/* PCI info - bus#,dev#,address etc */
	u16 wAdapterType;	/* ASI6701 etc */
	u16 wIndex;		/* */
	u16 wOpen;		/* =1 when adapter open */
	u16 wMixerOpen;

	struct hpios_spinlock dspLock;

	u16 wDspCrashed;
	u16 wHasControlCache;
	void *priv;
};

struct hpi_adapter_obj *HpiFindAdapter(
	u16 wAdapterIndex
);
u16 HpiAddAdapter(
	struct hpi_adapter_obj *pao
);

void HpiDeleteAdapter(
	struct hpi_adapter_obj *pao
);

short HpiCheckControlCache(
	volatile struct hpi_control_cache_single *pC,
	struct hpi_message *phm,
	struct hpi_response *phr
);
void HpiSyncControlCache(
	volatile struct hpi_control_cache_single *pC,
	struct hpi_message *phm,
	struct hpi_response *phr
);
u16 HpiValidateResponse(
	struct hpi_message *phm,
	struct hpi_response *phr
);
