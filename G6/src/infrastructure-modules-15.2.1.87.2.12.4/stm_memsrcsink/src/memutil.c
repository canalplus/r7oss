#include "memutil.h"

int generate_block_list(char *buf, int32_t count,
			struct scatterlist **input_sg_list,
			struct page ***input_pages,
			struct stm_data_block **input_block_list,
			uint32_t *pages_mapped)
{
	struct page **pages;
	struct scatterlist *sg_list;
	struct stm_data_block *block_list;
	unsigned long uaddr = (unsigned long) buf;
	int32_t npages;
	uint32_t initial_offset;
	uint32_t i;
	int ret;

	initial_offset = uaddr & ~PAGE_MASK;
	npages = (initial_offset + count + PAGE_SIZE - 1) >> PAGE_SHIFT;

	pages = infra_os_malloc(npages * sizeof(*pages));
	if (!pages) {
		pr_err("Pages not allocated\n");
		goto error_pages;
	}

	ret = get_user_pages_fast(uaddr, npages, 1, pages);
	if (ret < 0)
		goto error_mapped;

	npages = ret;

	if (WARN_ON(npages == 0))
		goto error_mapped;

	sg_list = infra_os_malloc(npages * sizeof(*sg_list));
	if (!sg_list) {
		pr_err("SG list not allocated\n");
		goto error_list;
	}
	sg_init_table(sg_list, npages);

	block_list = infra_os_malloc(npages * sizeof(*block_list));
	if (!block_list) {
		pr_err("block_list not allocated\n");
		goto error_block;
	}

	for (i = 0; i < npages; i++) {
		int plen = min_t(int, count, PAGE_SIZE - initial_offset);

		sg_set_page(sg_list + i, pages[i], plen, initial_offset);

		block_list[i].data_addr = sg_virt(sg_list + i);
		block_list[i].len = plen;
		block_list[i].next = &(block_list[i+1]);

		initial_offset = 0;
		count -= plen;
		ret += plen;
	}
	block_list[i-1].next = NULL;
	*input_pages = pages;
	*input_sg_list = sg_list;
	*input_block_list = block_list;
	*pages_mapped = npages;

	return ret;

error_block:
	infra_os_free(sg_list);

error_list:
	while (--npages >= 0)
		page_cache_release(pages[npages]);

error_mapped:
	infra_os_free(pages);
error_pages:
	return -ENOMEM;
}

void free_block_list(struct page **pages,
		     struct scatterlist *sg_list,
		     struct stm_data_block *input_block_list,
		     uint32_t mapped,
		     uint32_t filled_blocks)
{
	int index;

	for (index = 0; index < mapped; index++) {
		flush_kernel_dcache_page(pages[index]);
		page_cache_release(pages[index]);
	}

	infra_os_free(input_block_list);
	infra_os_free(sg_list);
	infra_os_free(pages);
}

int generate_block_list_vmalloc(char *buf, int32_t count,
				struct scatterlist **input_sg_list,
				struct page ***input_pages,
				struct stm_data_block **input_block_list,
				uint32_t *pages_mapped)
{
	struct scatterlist *sglist;
	struct page **pages = NULL;
	struct stm_data_block *block_list;
	int32_t npages;
	uint32_t initial_offset;
	uint32_t i;
	int ret = 0;
	int plen = 0;

	initial_offset = (unsigned long) buf & ~PAGE_MASK;
	npages = (initial_offset + count + PAGE_SIZE - 1) >> PAGE_SHIFT;

	pages = infra_os_malloc(npages * sizeof(*pages));
	if (!pages) {
		pr_err("Pages not allocated\n");
		goto error_pages;
	}
	sglist = infra_os_malloc(npages * sizeof(*sglist));
	if (sglist == NULL) {
		pr_err("%s: fail in vzalloc sglist\n", __func__);
		goto error_sglist;
	}
	block_list = infra_os_malloc(npages * sizeof(*block_list));
	if (!block_list) {
		pr_err("%s: blocklist not allocated\n", __func__);
		goto error_block;
	}
	sg_init_table(sglist, npages);
	for (i = 0; i < npages; i++, buf += plen) {
		plen = min_t(int, count, PAGE_SIZE - initial_offset);
		pages[i] = vmalloc_to_page(buf);
		if (NULL == pages[i]) {
			pr_err("%s fail vmalloc_to_page\n", __func__);
			goto error_list;
		}
		BUG_ON(PageHighMem(pages[i]));
		sg_set_page(&sglist[i], pages[i], plen, initial_offset);

		block_list[i].data_addr = sg_virt(&sglist[i]);
		block_list[i].len = plen;
		block_list[i].next = &(block_list[i+1]);

		initial_offset = 0;
		count -= plen;
		ret += plen;
	}
	block_list[i-1].next = NULL;
	*input_pages = pages;
	*input_sg_list = sglist;
	*input_block_list = block_list;
	*pages_mapped = npages;

	return ret;

error_list:
	infra_os_free(block_list);
error_block:
	infra_os_free(sglist);

error_sglist:
	infra_os_free(pages);
error_pages:
	return -ENOMEM;
}

void free_block_list_vmalloc(struct page **pages,
			     struct scatterlist *sg_list,
			     struct stm_data_block *input_block_list,
			     uint32_t mapped,
			     uint32_t filled_blocks)
{
	int index;

	for (index = 0; index < mapped; index++)
		flush_kernel_dcache_page(pages[index]);

	infra_os_free(input_block_list);
	infra_os_free(sg_list);
	infra_os_free(pages);
}

static int __init stm_memsrcsink_init(void)
{
	stm_memsrc_init();
	stm_memsink_init();
	return 0;
}
module_init(stm_memsrcsink_init);

static void __exit stm_memsrcsink_term(void)
{
	stm_memsrc_term();
	stm_memsink_term();
}
module_exit(stm_memsrcsink_term);
