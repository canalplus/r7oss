/*
 *  stm_nand_bbt.c         STM NAND BBT Support
 *
 *  This file adds some extra functionality to the existing BBT support found
 *  in nand_bbt.c:
 *
 *     - STM_NAND_SAFE_MOUNT: A configurable option to help prevent corruption
 *	 that might otherwise result from the presence of 'alien' BBTs
 *	 (i.e. BBTs written by a driver different to that currently employed).
 *
 *     - Enable detection of STM NAND ECC tags when scanning for
 *       manufacturer-programmed bad block markers.
 *
 *  Copyright (c) 2012 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 */


#include "stm_nand_bbt.h"

#ifndef CONFIG_STM_NAND_SAFE_MOUNT
int stmnand_examine_bbts(struct mtd_info *mtd, int bch_remap)
{
	return 0;
}
EXPORT_SYMBOL(stmnand_examine_bbts);
#else /* CONFIG_STM_NAND_SAFE_MOUNT */

#define BCH_SECTOR_BYTES	1024
#define BCH18_ECC_BYTES		32
#define BCH30_ECC_BYTES		54

/* Undo the Page-OOB mapping performed by the stm-nand-bch driver */
static void __attribute__((unused)) stmnand_bch_unmap(uint8_t *page,
				uint8_t *oob, int page_size, int oob_size,
				int bch_remap)
{
	int n_sectors, s;
	int ecc_bytes_per_sector;
	int oob_bytes_remainder;
	uint8_t *data;
	uint8_t *dst_p, *dst_o, *src;

	if (bch_remap == BCH_REMAP_NONE)
		return;

	n_sectors = page_size / BCH_SECTOR_BYTES;
	if (!n_sectors)
		return;

	ecc_bytes_per_sector = (bch_remap == BCH_REMAP_18BIT) ?
		BCH18_ECC_BYTES : BCH30_ECC_BYTES;
	oob_bytes_remainder = oob_size - (n_sectors * ecc_bytes_per_sector);

	data = kmalloc(page_size * oob_size, GFP_KERNEL);
	memcpy(data, page, page_size);
	memcpy(data + page_size, oob, oob_size);

	memset(oob, 0xff, oob_size);
	src = data;
	dst_p = page;
	dst_o = oob;

	for (s = 0; s < n_sectors; s++) {
		memcpy(dst_p, src, BCH_SECTOR_BYTES);
		src += BCH_SECTOR_BYTES;
		dst_p += BCH_SECTOR_BYTES;

		memcpy(dst_o, src, ecc_bytes_per_sector);
		src += ecc_bytes_per_sector;
		dst_o += ecc_bytes_per_sector;
	}

	if (oob_bytes_remainder)
		memcpy(dst_o, src, oob_bytes_remainder);

	kfree(data);
}

/* Remap the Page-OOB data, as performed by the stm-nand-bch driver */
void stmnand_bch_remap(uint8_t *page, uint8_t *oob,
		       int page_size, int oob_size, int bch_remap)
{
	int n_sectors, s;
	int ecc_bytes_per_sector;
	int oob_bytes_remainder;
	uint8_t *data;
	uint8_t *dst, *src_p, *src_o;

	if (bch_remap == BCH_REMAP_NONE)
		return;

	n_sectors = page_size / BCH_SECTOR_BYTES;
	if (!n_sectors)
		return;

	ecc_bytes_per_sector =  (bch_remap == BCH_REMAP_18BIT) ?
		BCH18_ECC_BYTES : BCH30_ECC_BYTES;
	oob_bytes_remainder = oob_size - (n_sectors * ecc_bytes_per_sector);

	data = kmalloc(page_size * oob_size, GFP_KERNEL);
	memset(data, 0xff, page_size + oob_size);

	src_p = page;
	src_o = oob;
	dst = data;

	for (s = 0; s < n_sectors; s++) {
		memcpy(dst, src_p, BCH_SECTOR_BYTES);
		dst += BCH_SECTOR_BYTES;
		src_p += BCH_SECTOR_BYTES;

		memcpy(dst, src_o, ecc_bytes_per_sector);
		dst += ecc_bytes_per_sector;
		src_o += ecc_bytes_per_sector;
	}

	if (oob_bytes_remainder)
		memcpy(dst, src_o, oob_bytes_remainder);

	memcpy(page, data, page_size);
	memcpy(oob, data + page_size, oob_size);

	kfree(data);
}

static int stmnand_check_bbt_block(struct mtd_info *mtd, uint64_t offs,
				   uint8_t *buf, int bch_remap)
{
	struct nand_bbt_descr *bbt_descrs[] = {
		&bbt_main_descr,
		&bbt_mirror_descr,
		&bbt_main_descr_ode,
		&bbt_mirror_descr_ode,
	};

	uint8_t *ibbt_pats[] = {
		bbt_pattern,
		mirror_pattern,
	};

	int ret, retlen;
	int page_size = mtd->writesize;
	int oob_size = mtd->oobsize;
	uint8_t *oob = buf + mtd->writesize;
	int i, j, e;
	char *ecc_str;
	uint64_t offs_bch_sig;

	/* Try linux BBT signatures */
	for (i = 0; i < ARRAY_SIZE(bbt_descrs); i++) {
		ret = scan_read_oob(mtd, buf, offs, mtd->writesize);
		if (ret < 0)
			return ret;

		/* Check for empty page */
		e = 0;
		for (j = 0; j < (page_size + oob_size) && e <= 1; j++)
			e += hweight8(~buf[j]);
		if (e <= 1)
			return 0;

		if (bch_remap)
			stmnand_bch_remap(buf, oob, page_size, oob_size,
					  bch_remap);

		/* Check for BBT signature */
		if (check_pattern(buf, page_size + oob_size, page_size,
				  bbt_descrs[i]) == 0) {

			/* Check ECC */
			ret = mtd_read(mtd, offs, page_size, &retlen, buf);
			if (ret == 0 || ret  == -EUCLEAN)
				return 1;

			if (retlen != page_size)
				return ret;

			/* Uncorrectable ECC error */
			e = 0;
			for (j = 0; j < oob_size; j += 16) {
				e += hweight8(oob[j + 3] ^ 'A');
				e += hweight8(oob[j + 4] ^ 'F');
				e += hweight8(oob[j + 5] ^ 'M');
			}
			if (e <= 1)
				ecc_str = "AFM";
			else if (bbt_descrs[i] == &bbt_main_descr_ode ||
				 bbt_descrs[i] == &bbt_mirror_descr_ode)
				ecc_str = "Micron 'on-die'";
			else
				ecc_str = "FLEX/EMI";

			pr_info("nand_bbt: 0x%012llx: found 'alien' BBT (%s?)\n",
			       offs, ecc_str);
			return -EBADMSG;
		}
	}

	/* Try BCH 'inband' signatures */
	offs_bch_sig = offs + mtd->erasesize - mtd->writesize;
	for (i = 0; i < ARRAY_SIZE(ibbt_pats); i++) {
		ret = scan_read_oob(mtd, buf, offs_bch_sig, mtd->writesize);
		if (ret < 0)
			return ret;

		e = 0;
		/* Primary/Mirror Pattern */
		for (j = 0; j < 4 && e <= 4; j++)
			e += hweight8(buf[j] ^ ibbt_pats[i][j]);

		/* Base Schema */
		for (j = 0; j < 4 && e <= 4; j++)
			e += hweight8(buf[8 + j] ^ 0x10);
		/* IBBT BCH Schema */
		for (j = 0; j < 4 && e <= 4; j++)
			e += hweight8(buf[12 + j] ^ 0x10);

		if (e <= 2) {
			ret = mtd_read(mtd, offs_bch_sig, page_size, &retlen,
					buf);
			if (ret == 0 || ret  == -EUCLEAN)
				return 1;

			if (retlen != page_size)
				return ret;

			ecc_str = "";
			if (buf[16] == BCH18_ECC_BYTES)
				ecc_str = "18";
			else if (buf[16] == BCH30_ECC_BYTES)
				ecc_str = "30";

			/* Uncorrectable ECC Error */
			pr_info("nand_bbt: 0x%012llx: found 'alien' BBT (BCH%s %s)\n",
				offs, ecc_str, buf + 20);

			return -EBADMSG;
		}
	}

	return -EBADMSG;
}

/* Examine the BBT area */
int stmnand_examine_bbts(struct mtd_info *mtd, int bch_remap)
{
	struct nand_chip *this = mtd->priv;

	uint8_t *buf;
	int valid = 0;
	int alien = 0;
	int i;
	uint64_t offs;
	int bbt_len;
	int ret;

	buf = kmalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	offs = mtd->size - mtd->erasesize;
	for (i = 0; i < 4; i++) {
		ret = stmnand_check_bbt_block(mtd, offs, buf, bch_remap);

		if (ret == 1)
			valid++;
		else if (ret == -EBADMSG)
			alien++;
		else if (ret < 0)
			return ret;

		offs -= mtd->erasesize;
	}
	kfree(buf);

	if (this->bbt_options & NAND_BBT_USE_FLASH) {
		if (!valid && alien) {
			pr_warn("nand_bbt: only found 'alien' BBT(s)\n");
			goto recovery_mode;
		}
	} else {
		if (valid) {
			pr_warn("nand_bbt: found valid BBT but NAND_USE_FLASH_BBT not selected\n");
			goto recovery_mode;
		}
		if (alien) {
			pr_warn("nand_bbt: found 'alien' BBT(s)\n");
			goto recovery_mode;
		}
	}

	/* BBT status is consistent with current driver and configuration */
	return 0;

 recovery_mode:
	bbt_len = mtd->size >> (this->bbt_erase_shift + 2);
	this->bbt = kzalloc(bbt_len, GFP_KERNEL);
	if (!this->bbt) {
		pr_err("%s: Out of memory\n", __func__);
		return -ENOMEM;
	}
	/* Mark all blocks as bad */
	memset(this->bbt, 0xff, bbt_len);

	return 1;
}
EXPORT_SYMBOL(stmnand_examine_bbts);
#endif /* CONFIG_STM_NAND_SAFE_MOUNT */

int stmnand_scan_bbt(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	int ret;

	/* Check for 'alien' BBTs */
	ret = stmnand_examine_bbts(mtd, BCH_REMAP_NONE);
	if (ret != 0)
		return ret < 0 ? ret : 0;

	if (!this->badblock_pattern)
		nand_create_badblock_pattern(this);

	/* Enable detection of STM NAND ECC patterns */
	this->badblock_pattern->options |= (NAND_BBT_SCANSTMBOOTECC |
					    NAND_BBT_SCANSTMAFMECC);

	return nand_default_bbt(mtd);
}
EXPORT_SYMBOL(stmnand_scan_bbt);

int stmnand_blocks_all_bad(struct mtd_info *mtd)
{
	uint64_t offs;

	for (offs = 0; offs < mtd->size; offs += mtd->erasesize)
		if (mtd_block_isbad(mtd, offs) == 0)
			return 0;

	return 1;
}
EXPORT_SYMBOL(stmnand_blocks_all_bad);
