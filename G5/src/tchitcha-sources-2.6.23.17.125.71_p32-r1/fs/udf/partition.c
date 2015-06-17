/*
 * partition.c
 *
 * PURPOSE
 *      Partition handling routines for the OSTA-UDF(tm) filesystem.
 *
 * COPYRIGHT
 *      This file is distributed under the terms of the GNU General Public
 *      License (GPL). Copies of the GPL can be obtained from:
 *              ftp://prep.ai.mit.edu/pub/gnu/GPL
 *      Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-2001 Ben Fennema
 *
 * HISTORY
 *
 * 12/06/98 blf  Created file.
 *
 */

#include "udfdecl.h"
#include "udf_sb.h"
#include "udf_i.h"

#include <linux/fs.h>
#include <linux/string.h>
#include <linux/udf_fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

inline uint32_t udf_get_pblock(struct super_block *sb, uint32_t block,
			       uint16_t partition, uint32_t offset)
{
	if (partition >= UDF_SB_NUMPARTS(sb)) {
		udf_debug("block=%d, partition=%d, offset=%d: invalid partition\n",
			  block, partition, offset);
		return 0xFFFFFFFF;
	}
	if (UDF_SB_PARTFUNC(sb, partition))
		return UDF_SB_PARTFUNC(sb, partition)(sb, block, partition, offset);
	else
		return UDF_SB_PARTROOT(sb, partition) + block + offset;
}

uint32_t udf_get_pblock_virt15(struct super_block *sb, uint32_t block,
			       uint16_t partition, uint32_t offset)
{
	struct buffer_head *bh = NULL;
	uint32_t newblock;
	uint32_t index;
	uint32_t loc;

	index = (sb->s_blocksize - UDF_SB_TYPEVIRT(sb,partition).s_start_offset) / sizeof(uint32_t);

	if (block > UDF_SB_TYPEVIRT(sb,partition).s_num_entries) {
		udf_debug("Trying to access block beyond end of VAT (%d max %d)\n",
			  block, UDF_SB_TYPEVIRT(sb,partition).s_num_entries);
		return 0xFFFFFFFF;
	}

	if (block >= index) {
		block -= index;
		newblock = 1 + (block / (sb->s_blocksize / sizeof(uint32_t)));
		index = block % (sb->s_blocksize / sizeof(uint32_t));
	} else {
		newblock = 0;
		index = UDF_SB_TYPEVIRT(sb,partition).s_start_offset / sizeof(uint32_t) + block;
	}

	loc = udf_block_map(UDF_SB_VAT(sb), newblock);

	if (!(bh = sb_bread(sb, loc))) {
		udf_debug("get_pblock(UDF_VIRTUAL_MAP:%p,%d,%d) VAT: %d[%d]\n",
			  sb, block, partition, loc, index);
		return 0xFFFFFFFF;
	}

	loc = le32_to_cpu(((__le32 *)bh->b_data)[index]);

	brelse(bh);

	if (UDF_I_LOCATION(UDF_SB_VAT(sb)).partitionReferenceNum == partition) {
		udf_debug("recursive call to udf_get_pblock!\n");
		return 0xFFFFFFFF;
	}

	return udf_get_pblock(sb, loc,
			      UDF_I_LOCATION(UDF_SB_VAT(sb)).partitionReferenceNum,
			      offset);
}

inline uint32_t udf_get_pblock_virt20(struct super_block * sb, uint32_t block,
				      uint16_t partition, uint32_t offset)
{
	return udf_get_pblock_virt15(sb, block, partition, offset);
}

uint32_t udf_get_pblock_spar15(struct super_block * sb, uint32_t block,
			       uint16_t partition, uint32_t offset)
{
	int i;
	struct sparingTable *st = NULL;
	uint32_t packet = (block + offset) & ~(UDF_SB_TYPESPAR(sb,partition).s_packet_len - 1);

	for (i = 0; i < 4; i++) {
		if (UDF_SB_TYPESPAR(sb,partition).s_spar_map[i] != NULL) {
			st = (struct sparingTable *)UDF_SB_TYPESPAR(sb,partition).s_spar_map[i]->b_data;
			break;
		}
	}

	if (st) {
		for (i = 0; i < le16_to_cpu(st->reallocationTableLen); i++) {
			if (le32_to_cpu(st->mapEntry[i].origLocation) >= 0xFFFFFFF0) {
				break;
			} else if (le32_to_cpu(st->mapEntry[i].origLocation) == packet) {
				return le32_to_cpu(st->mapEntry[i].mappedLocation) +
					((block + offset) & (UDF_SB_TYPESPAR(sb,partition).s_packet_len - 1));
			} else if (le32_to_cpu(st->mapEntry[i].origLocation) > packet) {
				break;
			}
		}
	}

	return UDF_SB_PARTROOT(sb,partition) + block + offset;
}

int udf_relocate_blocks(struct super_block *sb, long old_block, long *new_block)
{
	struct udf_sparing_data *sdata;
	struct sparingTable *st = NULL;
	struct sparingEntry mapEntry;
	uint32_t packet;
	int i, j, k, l;

	for (i = 0; i < UDF_SB_NUMPARTS(sb); i++) {
		if (old_block > UDF_SB_PARTROOT(sb,i) &&
		    old_block < UDF_SB_PARTROOT(sb,i) + UDF_SB_PARTLEN(sb,i)) {
			sdata = &UDF_SB_TYPESPAR(sb,i);
			packet = (old_block - UDF_SB_PARTROOT(sb,i)) & ~(sdata->s_packet_len - 1);

			for (j = 0; j < 4; j++) {
				if (UDF_SB_TYPESPAR(sb,i).s_spar_map[j] != NULL) {
					st = (struct sparingTable *)sdata->s_spar_map[j]->b_data;
					break;
				}
			}

			if (!st)
				return 1;

			for (k = 0; k < le16_to_cpu(st->reallocationTableLen); k++) {
				if (le32_to_cpu(st->mapEntry[k].origLocation) == 0xFFFFFFFF) {
					for (; j < 4; j++) {
						if (sdata->s_spar_map[j]) {
							st = (struct sparingTable *)sdata->s_spar_map[j]->b_data;
							st->mapEntry[k].origLocation = cpu_to_le32(packet);
							udf_update_tag((char *)st, sizeof(struct sparingTable) + le16_to_cpu(st->reallocationTableLen) * sizeof(struct sparingEntry));
							mark_buffer_dirty(sdata->s_spar_map[j]);
						}
					}
					*new_block = le32_to_cpu(st->mapEntry[k].mappedLocation) +
						((old_block - UDF_SB_PARTROOT(sb,i)) & (sdata->s_packet_len - 1));
					return 0;
				} else if (le32_to_cpu(st->mapEntry[k].origLocation) == packet) {
					*new_block = le32_to_cpu(st->mapEntry[k].mappedLocation) +
						((old_block - UDF_SB_PARTROOT(sb,i)) & (sdata->s_packet_len - 1));
					return 0;
				} else if (le32_to_cpu(st->mapEntry[k].origLocation) > packet) {
					break;
				}
			}

			for (l = k; l < le16_to_cpu(st->reallocationTableLen); l++) {
				if (le32_to_cpu(st->mapEntry[l].origLocation) == 0xFFFFFFFF) {
					for (; j < 4; j++) {
						if (sdata->s_spar_map[j]) {
							st = (struct sparingTable *)sdata->s_spar_map[j]->b_data;
							mapEntry = st->mapEntry[l];
							mapEntry.origLocation = cpu_to_le32(packet);
							memmove(&st->mapEntry[k + 1], &st->mapEntry[k], (l - k) * sizeof(struct sparingEntry));
							st->mapEntry[k] = mapEntry;
							udf_update_tag((char *)st, sizeof(struct sparingTable) + le16_to_cpu(st->reallocationTableLen) * sizeof(struct sparingEntry));
							mark_buffer_dirty(sdata->s_spar_map[j]);
						}
					}
					*new_block = le32_to_cpu(st->mapEntry[k].mappedLocation) +
						((old_block - UDF_SB_PARTROOT(sb,i)) & (sdata->s_packet_len - 1));
					return 0;
				}
			}

			return 1;
		} /* if old_block */
	}

	if (i == UDF_SB_NUMPARTS(sb)) {
		/* outside of partitions */
		/* for now, fail =) */
		return 1;
	}

	return 0;
}

static uint32_t udf_get_addr_from_short_ad(struct super_block *sb, short_ad *p, int desc_count, int block)
{
	int i;
	int crt_blocks=0;

	if (p == NULL)
		return 0xFFFFFFFF;

	udf_debug("SHORT AD SEARCHING BLOCK %d\n",block);

	for (i=0; i<desc_count; i++) {
		uint32_t extent_len;
		int blocks_in_extent;

		extent_len = le32_to_cpu(p[i].extLength);
		/* keep least 30 sign bits (ecma 167 14.14.1.1) */
		extent_len &= ((1<<30) - 1);
		blocks_in_extent = extent_len>>sb->s_blocksize_bits;
		if ((crt_blocks + blocks_in_extent) > block)
			break;
		crt_blocks += blocks_in_extent;
	}

	/* not found */
	if (i == desc_count) {
		udf_debug("block %d not found in allocation desc\n",block);
		return 0xFFFFFFFF;
	}

	/* block offset in current extent */
	block -= crt_blocks;

	block = le32_to_cpu(p[i].extPosition) + block;
	return block;
}

static uint32_t udf_try_read_meta(struct super_block *sb, uint32_t block, uint16_t partition, uint32_t offset, struct inode* inode)
{
	uint32_t metad_blk;
	uint32_t phy_blk;
	struct buffer_head *bh = NULL;
	short_ad *sa;
	int len;

	metad_blk = 0;

	switch(UDF_I_ALLOCTYPE(inode)) {
		case ICBTAG_FLAG_AD_SHORT:
			udf_debug("ICB flag is ICBTAG_FLAG_AD_SHORT\n");
			len = UDF_I_LENALLOC(inode)/sizeof(short_ad);
			if (len == 0) {
				udf_error(sb, __FUNCTION__, "Inode has 0 alloc\n");
				return 0xFFFFFFFF;
			}
			sa = (short_ad*)(UDF_I_DATA(inode) + UDF_I_LENEATTR(inode));
			if (sa == NULL) {
				udf_error(sb, __FUNCTION__, "Inode has null alloc desc\n");
				return 0xFFFFFFFF;
			}
			metad_blk = udf_get_addr_from_short_ad(sb, sa, len, block);
			break;
		case ICBTAG_FLAG_AD_LONG:
			udf_debug("ICB flag is ICBTAG_FLAG_AD_LONG\n");
			return 0xFFFFFFFF;
			break;
		case ICBTAG_FLAG_AD_IN_ICB:
			udf_debug("ICB flag is ICBTAG_FLAG_AD_IN_ICB\n");
			break;
		case ICBTAG_FLAG_AD_EXTENDED:
			udf_debug("ICB flag is ICBTAG_FLAG_AD_EXTENDED !!!!!!!\n");
			return 0xFFFFFFFF;
			break;
	}


	/* map to sparable/physical partition desc */
	phy_blk = udf_get_pblock(sb, metad_blk, UDF_SB_PARTNUM(sb, partition), offset);

	udf_debug("block=%d partition=%d realblk=%d physical=%d\n",block, partition, metad_blk, phy_blk);

	/* try to read from the physical location */
	bh = udf_tread(sb, phy_blk);

	if (bh) {
		udf_debug("udf_try_read_meta SUCCEEDED\n");
		brelse(bh);
		return phy_blk;
	} else {
		udf_debug("udf_try_read_meta FAILED\n");
		return 0xFFFFFFFF;
	}
}

uint32_t udf_get_pblock_meta25(struct super_block *sb, uint32_t block, uint16_t partition, uint32_t offset)
{
	uint32_t retblk;
	struct inode *inode;

	udf_debug("READING from METADATA\n");

	inode = UDF_SB_TYPEMETA(sb,partition).s_metadata_fe;

	if (inode) {
		retblk = udf_try_read_meta(sb, block, partition, offset, inode);
		if(retblk == 0xFFFFFFFF) {
			udf_warning(sb, __FUNCTION__, "OOOOPS ... error reading from METADATA, trying to read from MIRROR");
			inode = UDF_SB_TYPEMETA(sb,partition).s_mirror_fe;
			if (inode == NULL) {
				udf_error(sb, __FUNCTION__, "mirror inode is null");
				return 0xFFFFFFFF;
			}
			retblk = udf_try_read_meta(sb, block, partition, offset, inode);
		}
	}
	else 	/* metadata inode is NULL */
	{
		udf_warning(sb, __FUNCTION__, "metadata inode is null. hmmm, will try reading from mirror file");
		inode = UDF_SB_TYPEMETA(sb,partition).s_mirror_fe;
		if (inode == NULL) {
			udf_error(sb, __FUNCTION__, "mirror inode null too??? bad, bad, bad! how did we get here???");
			return 0xFFFFFFFF;
		}
		retblk = udf_try_read_meta(sb, block, partition, offset, inode);
	}

	return retblk;
}
