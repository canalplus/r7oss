/*
 * videobuf2-bpa2-contig.c - bpa2 integration for videobuf2
 *
 * Copyright (C) 2012 STMicroelectronics Ltd.
 * Copyright (C) 2010 Samsung Electronics
 *
 * Based on code by:
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#include <linux/bpa2.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_conf {
	struct device *dev;
	struct bpa2_part *part;
};

struct vb2_dc_buf {
	struct vb2_dc_conf *conf;
	struct bpa2_part *part; /* only for userptr */
	void *vaddr;
	unsigned long paddr;
	unsigned long size;
	struct vm_area_struct *vma;
	atomic_t refcount;
	struct vb2_vmarea_handler handler;
};

static void vb2_bpa2_contig_put(void *buf_priv);

static void *vb2_bpa2_contig_alloc(void *alloc_ctx,
		unsigned long size, gfp_t flags)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct vb2_dc_buf *buf;

	unsigned long base;
	void *addr;
	int pages;

	BUG_ON(!conf);

	/* a valid bpa2 partition hasn't been configured */
	if (!conf->part)
		return ERR_PTR(-EINVAL);

	if (!size)
		return ERR_PTR(-EINVAL);

	pages = PAGE_ALIGN(size) / PAGE_SIZE;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	base = bpa2_alloc_pages(conf->part, pages, 1, GFP_KERNEL);
	if (!base) {
		kfree(buf);
		pr_err("bpa2: couldn't allocate pages!\n");
		return ERR_PTR(-ENOMEM);
	}

	if (bpa2_low_part(conf->part)) {
		addr = phys_to_virt(base);
	} else {
		addr = ioremap_nocache(base, pages * PAGE_SIZE);
		if (!addr) {
			bpa2_free_pages(conf->part, base);
			pr_err("bpa2: couldn't ioremap() region! (%lu bytes)\n",
					pages * PAGE_SIZE);
			kfree(buf);
			return ERR_PTR(-ENOMEM);
		}
	}

	buf->conf = conf;
	buf->size = size;
	buf->paddr = base;
	buf->vaddr = addr;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_bpa2_contig_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;
}

static void vb2_bpa2_contig_put(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!buf->conf->part)
		return;

	if (atomic_dec_and_test(&buf->refcount)) {
		if (!bpa2_low_part(buf->conf->part))
			iounmap(buf->vaddr);
		bpa2_free_pages(buf->conf->part, buf->paddr);
		kfree(buf);
	}
}

static void *vb2_bpa2_contig_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->paddr;
}

static void *vb2_bpa2_contig_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	if (!buf)
		return 0;

	return buf->vaddr;
}

static unsigned int vb2_bpa2_contig_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static int vb2_bpa2_contig_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;
	unsigned long size;
	int ret;

	if (!buf) {
		pr_err("bpa2: no buffer to map\n");
		return -EINVAL;
	}

	size = min_t(unsigned long, vma->vm_end - vma->vm_start, buf->size);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start, buf->paddr >> PAGE_SHIFT,
			size, vma->vm_page_prot);
	if (ret) {
		pr_err("Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vma->vm_flags |= VM_DONTEXPAND | VM_IO;
	vma->vm_private_data = &buf->handler;
	vma->vm_ops = &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	pr_debug("%s: mapped paddr 0x%08lx at 0x%08lx, size %ld\n",
			__func__, buf->paddr, vma->vm_start, size);

	return 0;
}

static void *vb2_bpa2_contig_get_userptr(void *alloc_ctx, unsigned long vaddr,
		unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	struct vm_area_struct *vma;

	struct bpa2_part *part;
	dma_addr_t base = 0;
	void *addr;

	int ret;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = vb2_get_contig_userptr(vaddr, size, &vma, &base);
	if (ret) {
		pr_err("bpa2: failed acquiring VMA for vaddr 0x%08lx\n", vaddr);
		kfree(buf);
		return ERR_PTR(ret);
	}

	part = bpa2_find_part_addr((unsigned long)base, size);
	if (!part) {
		pr_err("bpa2: failed acquiring bpa2 partition\n");
		kfree(buf);
		return ERR_PTR(-EINVAL);
	}

	if (bpa2_low_part(part))
		addr = phys_to_virt((unsigned long)base);
	else {
		addr = ioremap_nocache((unsigned long)base, size);
		if (!addr) {
			pr_err("bpa2: couldn't ioremap() region at 0x%08lx\n",
					(unsigned long)base);
			kfree(buf);
			return ERR_PTR(-ENOMEM);
		}
	}

	buf->size = size;
	buf->part = part;
	buf->paddr = (unsigned long)base;
	buf->vaddr = addr;
	buf->vma = vma;

	return buf;
}

static void vb2_bpa2_contig_put_userptr(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (!buf)
		return;

	BUG_ON(!buf->part);

	if (!bpa2_low_part(buf->part))
		iounmap(buf->vaddr);

	vb2_put_vma(buf->vma);
	kfree(buf);
}

const struct vb2_mem_ops vb2_bpa2_contig_memops = {
	.alloc		= vb2_bpa2_contig_alloc,
	.put		= vb2_bpa2_contig_put,
	.cookie		= vb2_bpa2_contig_cookie,
	.vaddr		= vb2_bpa2_contig_vaddr,
	.mmap		= vb2_bpa2_contig_mmap,
	.get_userptr	= vb2_bpa2_contig_get_userptr,
	.put_userptr	= vb2_bpa2_contig_put_userptr,
	.num_users	= vb2_bpa2_contig_num_users,
};
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_memops);

void *vb2_bpa2_contig_init_ctx(struct device *dev, struct bpa2_part *part)
{
	struct vb2_dc_conf *conf;

	conf = kzalloc(sizeof(*conf), GFP_KERNEL);
	if (!conf)
		return ERR_PTR(-ENOMEM);

	conf->dev = dev;
	/* if part is NULL then VB2_MMAP buffers won't be supported
	 * still we'd allow VB2_USERPTR buffers.
	 */
	conf->part = part;

	return conf;
}
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_init_ctx);

void vb2_bpa2_contig_cleanup_ctx(void *alloc_ctx)
{
	kfree(alloc_ctx);
}
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_cleanup_ctx);

MODULE_DESCRIPTION("bpa2 integration for videobuf2");
MODULE_AUTHOR("Ilyes Gouta <ilyes.gouta@st.com>");
MODULE_LICENSE("GPL");
