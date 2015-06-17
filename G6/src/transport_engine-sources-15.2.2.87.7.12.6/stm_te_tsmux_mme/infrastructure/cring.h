/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

This may alternatively be licensed under a proprietary
license from ST.

Source file name : cring.h
Architects :	   Dan Thomson & Pete Steiglitz

Definition of the ring header file.

This if for a completely re-implemented c version of rings.
NOTE as this was all I needed at this time, these are nonblocking,
unprotected rings, that take a pointer as the insertion value.


Date	Modification				Architects
----	------------				----------
17-May-12   Created				 DT & PS

************************************************************************/

#ifndef H_CRING
#define H_CRING

#include <linux/slab.h>

/* Locally defined constants */
enum {
	CRingNoError = 0,
	CRingError,
	CRingFull,
	CRingEmpty
};

typedef unsigned int CRingStatus_t;

/* Structure associated with a ring */
typedef struct CRing_s {
	unsigned int Size;
	unsigned int NextInsert;
	unsigned int NextExtract;

	void **Array;
} CRing_t;

/* Functions */
CRingStatus_t CRingNew(CRing_t **Ring, unsigned int Capacity);

CRingStatus_t CRingDestroy(CRing_t *Ring);

CRingStatus_t CRingInsert(CRing_t *Ring, void *Value);

CRingStatus_t CRingExtract(CRing_t *Ring, void **Value);

bool CringNonEmpty(CRing_t *Ring);

#endif
