#ifndef __MEM_UTIL_H_
#define __MEM_UTIL_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/scatterlist.h>

#include "stm_data_interface.h"
#include "infra_os_wrapper.h"

#define INVALIDATE	1
#define NO_INVALIDATE	0

int __init stm_memsrc_init(void);
void __exit stm_memsrc_term(void);

int __init stm_memsink_init(void);
void __exit stm_memsink_term(void);

int generate_block_list(char *buf, int32_t count,
			struct scatterlist **input_sg_list,
			struct page ***input_pages,
			struct stm_data_block **input_block_list,
			uint32_t *pages_mapped);

void free_block_list(struct page **pages,
		     struct scatterlist *sg_list,
		     struct stm_data_block *input_block_list,
		     uint32_t mapped,
		     uint32_t filled_blocks);

int generate_block_list_vmalloc(char *buf, int32_t count,
				struct scatterlist **input_sg_list,
				struct page ***input_pages,
				struct stm_data_block **input_block_list,
				uint32_t *pages_mapped);

void free_block_list_vmalloc(struct page **pages,
			     struct scatterlist *sg_list,
			     struct stm_data_block *input_block_list,
			     uint32_t mapped,
			     uint32_t filled_blocks);

#endif /* __MEM_UTIL_H_ */
