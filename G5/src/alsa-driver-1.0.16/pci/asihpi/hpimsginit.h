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

 Hardware Programming Interface (HPI) Utility functions

 (C) Copyright AudioScience Inc. 2007
*******************************************************************************/

void HPI_InitMessage(
	struct hpi_message *phm,
	u16 wObject,
	u16 wFunction
);
void HPI_InitResponse(
	struct hpi_response *phr,
	u16 wObject,
	u16 wFunction,
	u16 wError
);
