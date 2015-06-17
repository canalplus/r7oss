
#ifndef STM_NAND_DT_H
#define STM_NAND_DT_H

#ifdef CONFIG_OF

struct device_node *stm_of_get_partitions_node(struct device_node *np,
				int bank_nr);
int  stm_of_get_nand_banks(struct device *dev, struct device_node *np,
			struct stm_nand_bank_data **banksp);

#else
static inline struct device_node *stm_of_get_partitions_node(
		struct device_node *np, int bank_nr)
{
	return NULL;
}

static inline int  stm_of_get_nand_banks(struct device *dev,
		struct device_node *np, struct stm_nand_bank_data **banksp)
{
	return 0;
}


#endif /* CONFIG_OF */
#endif /* STM_NAND_DT_H */
