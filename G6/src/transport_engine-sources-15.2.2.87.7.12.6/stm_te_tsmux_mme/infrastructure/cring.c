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

Source file name : cring.c
Architects :	   Dan Thomson & Pete Steiglitz

Implementation of the C nonblocking, unprotected ring.

Date	Modification				Architects
----	------------				----------
17-May-12   Created				 DT & PS

************************************************************************/
#include <stm_te_dbg.h>
#include "cring.h"

/* Useful macro*/
#define IndexIncrement(v)	(((v) + 1) % Ring->Size)

/* Implementation of get a new ring */
CRingStatus_t CRingNew(CRing_t **Ring, unsigned int Capacity)
{

	/* Get me the ring itself */
	*Ring = kzalloc(sizeof(CRing_t), GFP_KERNEL);
	if (*Ring == NULL)
		return CRingError;

	memset(*Ring, 0x00, sizeof(CRing_t));

	/* Get me the storage array for the ring */
	(*Ring)->Size = (Capacity + 1);
	(*Ring)->Array = kzalloc((Capacity + 1) * sizeof(void *), GFP_KERNEL);
	if ((*Ring)->Array == NULL) {
		kfree(*Ring);
		*Ring = NULL;
		return CRingError;
	}

	memset((*Ring)->Array, 0x00, (Capacity + 1) * sizeof(void *));

	return CRingNoError;
}

/* Implementation of destroy a ring */
CRingStatus_t CRingDestroy(CRing_t *Ring)
{
	/* Helpful warning */
	if (Ring->NextExtract != Ring->NextInsert)
		pr_debug("CRingDestroy - Destroying a non-empty ring.\n");

	/* Free the array, followed by the ring itself */
	kfree(Ring->Array);
	kfree(Ring);

	return CRingNoError;
}

/* Implementation of insert onto ring */
CRingStatus_t CRingInsert(CRing_t *Ring, void *Value)
{
	if (IndexIncrement(Ring->NextInsert) == Ring->NextExtract)
		return CRingFull;

	Ring->Array[Ring->NextInsert] = Value;
	Ring->NextInsert = IndexIncrement(Ring->NextInsert);

	return CRingNoError;
}

/* Implementation of extract from ring */
CRingStatus_t CRingExtract(CRing_t *Ring, void **Value)
{
	if (Ring->NextExtract == Ring->NextInsert)
		return CRingEmpty;

	*Value = Ring->Array[Ring->NextExtract];
	Ring->NextExtract = IndexIncrement(Ring->NextExtract);

	return CRingNoError;
}

/* Implementation of non empty for ring */
bool CringNonEmpty(CRing_t *Ring)
{
	return (Ring->NextExtract != Ring->NextInsert);
}
