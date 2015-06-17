#ifndef STM_NAND_BBT_H
#define STM_NAND_BBT_H

#define BCH_REMAP_NONE		0
#define BCH_REMAP_18BIT		1
#define BCH_REMAP_30BIT		2

int stmnand_examine_bbts(struct mtd_info *mtd, int bch_remap);

int stmnand_scan_bbt(struct mtd_info *mtd);

int stmnand_blocks_all_bad(struct mtd_info *mtd);


#endif /* STM_NAND_BBT_H */
