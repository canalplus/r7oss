/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : helpers.c

Defines a number of helper functions for internal use in stm_ce
************************************************************************/

#include "stm_registry.h"
#include "helpers.h"

/**
 * foreach_child_object() - Calls a function for each child object of a given
 * registry object
 * @parent:	Parent registry object
 * @operator:	Function pointer to call for each child object. This function
 * must accept the child object as the first argument and return 0 on success.
 */
int foreach_child_object(stm_object_h parent, int (*operator)(stm_object_h))
{
	int err = 0;
	stm_registry_iterator_h iter = (stm_registry_iterator_h)NULL;
	char tag[STM_REGISTRY_MAX_TAG_SIZE];
	stm_registry_member_type_t type;
	stm_object_h child;

	/* Create an iterator object for the parent */
	err = stm_registry_new_iterator(parent,
			STM_REGISTRY_MEMBER_TYPE_OBJECT, &iter);
	if (0 != err) {
		printk(KERN_ALERT "Failed to get create iterator (%d)\n", err);
		return err;
	}

	/* For each object found, call the operator function */
	while (0 == stm_registry_iterator_get_next(iter, tag, &type) &&
			0 == err) {
		err = stm_registry_get_object(parent, tag, &child);
		if (0 == err)
			err = (*operator)(child);
	}

	/* Clean up */
	if ((stm_registry_iterator_h)NULL != iter) {
		err = stm_registry_delete_iterator(iter);
		if (0 != err) {
			printk(KERN_ALERT "Failed to delete iterator (%d)\n",
				err);
			return err;
		}
	}

	return err;
}

int get_dma_scatterlist(const stm_ce_buffer_t *kbuf, int direction,
		struct scatterlist **dma_sgl, uint32_t *n_elems)
{
	uint32_t max_pages = kbuf->size/PAGE_SIZE + 2;
	uint32_t offset;
	uint32_t index;
	struct scatterlist *sgl = NULL;

	/* Allocate a scatterlist big enough to hold max_pages */
	sgl = OS_calloc(sizeof(struct scatterlist) * max_pages);
	if (!sgl)
		return -ENOMEM;

	/* Split kernel virtual buffer by page boundaries, generating a
	 * scatterlist */
	offset = 0;
	index = 0;
	while (offset < kbuf->size) {
		unsigned long next_page =
			(unsigned long)(&kbuf->data[offset] + PAGE_SIZE) &
			PAGE_MASK;
		unsigned long remain = kbuf->size - offset;
		unsigned long len =
			min((next_page - (unsigned long)&kbuf->data[offset]),
					remain);

		sg_set_buf(&sgl[index], &kbuf->data[offset], len);
		/* TODO Rationalise cache management.
		   This function is not available for Arm.
		   Use instead one of, dma_sync_single_range... - variant.
		   dma_cache_sync(NULL, &kbuf->data[offset], len, direction); */
		index++;
		offset += len;
	}

	/* Manage cache coherency */
	dma_sync_sg_for_device(NULL, sgl, max_pages, direction);

	/* Map the scatterlist to physical addresses. */
	offset = dma_map_sg(NULL, sgl, max_pages, direction);

	*n_elems = max_pages;
	*dma_sgl = sgl;
	return 0;
}

int free_dma_scatterlist(struct scatterlist *dma_sgl, uint32_t n_elems,
		int direction)
{
	/* Manage cache coherency */
	dma_sync_sg_for_cpu(NULL, dma_sgl, n_elems, direction);

	dma_unmap_sg(NULL, dma_sgl, n_elems, direction);
	OS_free(dma_sgl);
	return 0;
}

int dma_scatterlist_to_buffers(struct scatterlist *sgl, uint32_t n_elems,
		stm_ce_buffer_t *buf, uint32_t *n_bufs)
{
	struct scatterlist *sg;
	int i;
	uint32_t buf_i = 0;
	void *last_address = NULL;

	for_each_sg(sgl, sg, n_elems, i) {
		/* Skip empty buffers */
		if (0 == sg_dma_len(sg))
			continue;

		/* Check if this buffer is contiguous with the previous. If
		 * so, extend the previous buffer size, otherwise make a new
		 * buffer entry */
		if ((void *)sg_dma_address(sg) == last_address) {
			buf[buf_i].size += sg_dma_len(sg);
		} else {
			if (NULL != last_address)
				buf_i++;
			buf[buf_i].data = (void *)sg_dma_address(sg);
			buf[buf_i].size = sg_dma_len(sg);
		}

		last_address = buf[buf_i].data + buf[buf_i].size;
	}

	*n_bufs = buf_i + 1;
	return 0;
}
