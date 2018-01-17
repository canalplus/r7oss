/*
 *  plt_id.h - Driver that read Technicolor STB platform ID from GPIO
 *
 *  Author : Franck MARILLET <franck.marillet@technicolor.com>
 *           Cyril TOSTIVINT <cyril.tostivint@technicolor.com>
 *
 *  Copyright (C) 2011-2016 Technicolor
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define MAC_SIZE 	   6
#define NUM_HW_SUFFIX 16

struct tch_plt_id_desc {
	const char *name;
	u8 id;
	u8 multi_plt;
	struct kobject kobj;
	struct kobject kobj_perm;
	struct mtd_info *mtd;
};


#ifdef CONFIG_TECHNICOLOR_PLT_ID_PERMDATA
void plt_permdata_init(struct device *dev, struct tch_plt_id_desc *desc);
#else
inline void plt_permdata_init(struct device *dev, struct tch_plt_id_desc *desc)
{
	dev_info(dev, "Permdata sysfs disabled");
}
#endif
