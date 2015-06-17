/*
 * (c) 2014 STMicroelectronics Limited
 *
 * Author: David McKay <david.mckay@st.com>
 * Author: Ajit Pal Singh <ajitpal.singh@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/st_amba_bridge.h>

struct st_amba_bridge {
	void __iomem *base;
	struct device *dev;
	struct dentry *dentry;
	const struct st_amba_bridge_config *config;
};

/*
 * Register definitions valid for AHB-STBus protocol converter,
 * this is the type1 interface.
 */
#define AHB2STBUS_STBUS_OPC	0x00
#define AHB2STBUS_MSGSIZE	0x04
#define AHB2STBUS_CHUNKSIZE	0x08
#define AHB2STBUS_REQ_TIMEOUT	0x0c
#define AHB2STBUS_SW_RESET	0x10
#define AHB2STBUS_STATUS	0x14


/* order_base_2() can't handle zero (it probably should) */
static u32 ob2(u32 val, u32 max_val)
{
	return min(max_val, (val) ? (u32) order_base_2(val) : 0);
}

static void
configure_amba_stbus_type1(struct st_amba_bridge *plug,
			   const struct st_amba_bridge_config *config)
{
	u32 chunks_in_msg = ob2(config->chunks_in_msg, 6);
	u32 packets_in_chunk = ob2(config->packets_in_chunk, 6);

	writel_relaxed(config->max_opcode | (config->write_posting << 4),
		       plug->base + AHB2STBUS_STBUS_OPC);
	writel_relaxed(chunks_in_msg, plug->base + AHB2STBUS_MSGSIZE);
	writel_relaxed(packets_in_chunk, plug->base + AHB2STBUS_CHUNKSIZE);
	writel_relaxed(config->type1.req_timeout,
		       plug->base + AHB2STBUS_REQ_TIMEOUT);
}

#define SD_CONFIG				0x00
#define SD_CONFIG_REQ_NOTIFY_SHIFT		0
#define SD_CONFIG_CONT_ON_ERROR_SHIFT		1
#define SD_CONFIG_BUSY_SHIFT			2
#define SD_CONFIG_STBUS_MESSAGE_MERGER_SHIFT	3

#define AD_CONFIG			0x04
#define AD_CONFIG_THRESHOLD_SHIFT	0
#define AD_CONFIG_CHUNKS_IN_MSG_SHIFT	4
#define AD_CONFIG_PCKS_IN_CHUNK_SHIFT	9
#define AD_CONFIG_TRIGGER_MODE_SHIFT	14
#define AD_CONFIG_POSTED_SHIFT		15
#define AD_CONFIG_OPCODE_SHIFT		16
#define AD_CONFIG_READ_AHEAD_SHIFT	21
#define AD_CONFIG_BUSY_SHIFT		31

static void
configure_amba_stbus_type2(struct st_amba_bridge *plug,
			   const struct st_amba_bridge_config *config)
{
	u32 chunks_in_msg = ob2(config->chunks_in_msg, 31);
	u32 packets_in_chunk = ob2(config->packets_in_chunk, 31);
	u32 threshold = ob2(config->type2.threshold, 15);
	u32 value;

	if (!plug->config->type2.sd_config_missing) {
		/* Set up SD_CONFIG */
		value = (config->type2.req_notify <<
				SD_CONFIG_REQ_NOTIFY_SHIFT) |
			(config->type2.cont_on_error <<
				SD_CONFIG_CONT_ON_ERROR_SHIFT) |
			(config->type2.msg_merge <<
				SD_CONFIG_STBUS_MESSAGE_MERGER_SHIFT);

		writel_relaxed(value, plug->base + SD_CONFIG);
	}

	value = (threshold << AD_CONFIG_THRESHOLD_SHIFT) |
		(chunks_in_msg << AD_CONFIG_CHUNKS_IN_MSG_SHIFT) |
		(packets_in_chunk << AD_CONFIG_PCKS_IN_CHUNK_SHIFT) |
		(config->type2.trigger_mode << AD_CONFIG_TRIGGER_MODE_SHIFT) |
		(config->write_posting << AD_CONFIG_POSTED_SHIFT) |
		((config->max_opcode + 2) << AD_CONFIG_OPCODE_SHIFT) |
		(config->type2.read_ahead << AD_CONFIG_READ_AHEAD_SHIFT);

	writel_relaxed(value, plug->base + AD_CONFIG);
}



void st_amba_bridge_init(struct st_amba_bridge *plug)
{
	switch (plug->config->type) {
	case st_amba_type1:
		configure_amba_stbus_type1(plug, plug->config);
		break;
	case st_amba_type2:
		configure_amba_stbus_type2(plug, plug->config);
		break;
	default:
		BUG(); /* Invalid Convertor type */
	}
}
EXPORT_SYMBOL(st_amba_bridge_init);

#ifdef CONFIG_DEBUG_FS

struct amba_print {
	const char *name; /* Name of bitfield */
	int reg_offset; /* Which register */
	int start;	/* Bitfield start */
	int width;	/* How many bits */
	unsigned int pow2:1; /* Print as ^2 */
	unsigned int zero_to_disable:1; /* 0 means this feature disabled */
	/* Descriptive strings to print out for binary fields */
	const char *value_str[2];
};

/* opcode must be first in the list as it is special cased */
static const struct amba_print type2_bitfields[] = {
	{ "status", SD_CONFIG, SD_CONFIG_BUSY_SHIFT, 1, 0, 0,
		{"idle", "busy"}
	},
	{"req notify", SD_CONFIG, SD_CONFIG_REQ_NOTIFY_SHIFT, 1, 0, 0,
		{"burst", "cell"}
	},
	{"cont on error", SD_CONFIG, SD_CONFIG_CONT_ON_ERROR_SHIFT, 1, 0, 0,
		{"complete", "abort"}
	},
	{"msg merger", SD_CONFIG, SD_CONFIG_STBUS_MESSAGE_MERGER_SHIFT, 1, 0, 0,
		{"disabled", "enabled"}
	},
	{"opcode", AD_CONFIG, AD_CONFIG_OPCODE_SHIFT, 4, 1, 0},
	{"write posting", AD_CONFIG, AD_CONFIG_POSTED_SHIFT, 1, 0, 0,
		{"disabled", "enabled"}
	},
	{"chunks in msg", AD_CONFIG, AD_CONFIG_CHUNKS_IN_MSG_SHIFT, 5, 1, 1},
	{"packets in chunk", AD_CONFIG, AD_CONFIG_PCKS_IN_CHUNK_SHIFT, 5, 1, 1},
	{"threshold", AD_CONFIG, AD_CONFIG_THRESHOLD_SHIFT, 4, 1, 0},
	{"trigger mode", AD_CONFIG, AD_CONFIG_TRIGGER_MODE_SHIFT, 1, 0, 0,
		{"cell", "threshold"}
	},
	{"read ahead", AD_CONFIG, AD_CONFIG_READ_AHEAD_SHIFT, 1, 0, 0,
		{"disabled", "enabled"}
	},
};

static const struct amba_print type1_bitfields[] = {
	{"opcode", AHB2STBUS_STBUS_OPC, 0, 3, 1, 0},
	{"write posting", AHB2STBUS_STBUS_OPC, 4, 1, 0, 0,
		{"disabled", "enabled"}
	},
	{"chunks in msg", AHB2STBUS_MSGSIZE, 0, 2, 1, 0},
	{"packets in chunk", AHB2STBUS_CHUNKSIZE, 0, 2, 1, 0},
	{"req time out", AHB2STBUS_REQ_TIMEOUT, 0, 32, 0, 0},
	{"status", AHB2STBUS_STATUS, 0, 1, 0, 0,
		{"busy", "idle"}
	},
};

static const char * const type1_reg_names[] = {
	"stbus_opc", "msgsize", "chunksize", "req_timeout", "status"
};

static const char * const type2_reg_names[] = {
	"sd_config", "ad_config"
};

static const struct {
	const char * const *reg_names;
	const struct amba_print *bitfields;
	int num_entries;
} amba_print_type[2] = {
	{type1_reg_names, type1_bitfields, ARRAY_SIZE(type1_bitfields)},
	{type2_reg_names, type2_bitfields, ARRAY_SIZE(type2_bitfields)},
};

static int st_amba_bridge_reg_show(struct seq_file *s, void *v)
{
	const char * const *reg_names;
	struct st_amba_bridge *plug = s->private;
	const struct amba_print *a;
	int num_entries;
	int i;
	const int type = plug->config->type;
	u32 val, regval = 0;
	char *field;
	int reg_offset = -1;

	a = amba_print_type[type].bitfields;
	num_entries = amba_print_type[type].num_entries;
	reg_names = amba_print_type[type].reg_names;

	/* If SD_CONFIG is missing we skip those elements in the table */
	if (type == st_amba_type2 && plug->config->type2.sd_config_missing) {
		a += 4;
		num_entries -= 4;
	}

	for (i = 0; i < num_entries; i++, a++) {
		if (reg_offset != a->reg_offset) {
			/*
			 * If we move onto a new register, then print it,
			 * this implies the fields have to be in register
			 * order
			 */
			reg_offset = a->reg_offset;
			regval = readl(plug->base + reg_offset);
			seq_printf(s, "%s : 0x%x\n",
				   reg_names[reg_offset / 4], regval);
		}
		val = regval >> a->start;
		val &=  (a->width == 32) ? ~0 : (1 << a->width) - 1;
		/* We have to special case the opcode */
		if (!strcmp("opcode", a->name)) {
			/* Not encoded the same way, hence the +2 */
			val = (type == st_amba_type1) ? val + 2
						      : max_t(u32, 2, val);
			field = "LD/ST";
		} else {
			field = "";
		}
		seq_printf(s, "  %-16s : ", a->name);

		if (a->pow2) {
			val = (a->zero_to_disable && val == 0) ? 0 : 1 << val;
			seq_printf(s, "%s%d", field, val);
		} else {
			/* Single bit entries */
			if (a->width == 1)
				seq_printf(s, "%s", a->value_str[val]);
			else
				seq_printf(s, "%d", val);
		}
		seq_puts(s, "\n");
	}

	return 0;
}

static int st_amba_bridge_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, st_amba_bridge_reg_show, inode->i_private);
}

static const struct file_operations st_amba_bridge_reg_read_fops = {
	.open = st_amba_bridge_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define ST_AMBA_BRIDGE_DEBUGFS_FOPS (&st_amba_bridge_reg_read_fops)
#else
#define ST_AMBA_BRIDGE_DEBUGFS_FOPS NULL
#endif /* CONFIG_DEBUG_FS */

/* Number of times st_amba_bridge_create called */
static atomic_t ref_count = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(dfs_lock);
static struct dentry *dfs_dir;

static void st_amba_bridge_devres_release(struct device *dev, void *res)
{
	struct st_amba_bridge *plug = res;

	debugfs_remove(plug->dentry);

	spin_lock(&dfs_lock);
	if (atomic_dec_and_test(&ref_count)) {
		debugfs_remove(dfs_dir);
		dfs_dir = NULL;
	}
	spin_unlock(&dfs_lock);
}

struct st_amba_bridge *
st_amba_bridge_create(struct device *dev,
		      void __iomem *base,
		      struct st_amba_bridge_config *bus_config)
{
	struct st_amba_bridge *plug;

	plug = devres_alloc(st_amba_bridge_devres_release,
			    sizeof(*plug), GFP_KERNEL);
	if (!plug)
		return ERR_PTR(-ENOMEM);
	devres_add(dev, plug);

	plug->base = base;
	plug->dev = dev;
	plug->config = bus_config;
	plug->dentry = NULL;

	spin_lock(&dfs_lock);
	if (!dfs_dir) {
		dfs_dir = debugfs_create_dir("amba-stbus-bridge", NULL);
		if (IS_ERR(dfs_dir)) {
			dfs_dir = NULL;
			goto out;
		}
	}
	plug->dentry = debugfs_create_file(dev_name(dev), S_IFREG | S_IRUGO,
					   dfs_dir, plug,
					   ST_AMBA_BRIDGE_DEBUGFS_FOPS);
	if (IS_ERR(plug->dentry))
		plug->dentry = NULL;
out:
	atomic_inc(&ref_count);
	spin_unlock(&dfs_lock);
	return plug;
}
EXPORT_SYMBOL(st_amba_bridge_create);

/*
 *	st_of_get_amba_config - parse amba config from a device pointer.
 *
 *	returns a pointer to newly allocated amba bridge config.
 *	User is responsible to freeing the pointer.
 *
 */
struct st_amba_bridge_config *st_of_get_amba_config(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *cn = of_parse_phandle(np, "st,amba-config", 0);
	struct st_amba_bridge_config *config;

	if (!cn)
		return ERR_PTR(-EINVAL);

	config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
	if (!config)
		return ERR_PTR(-ENOMEM);

	if (of_get_property(cn, "st,bridge_type2", NULL))
		config->type = st_amba_type2;
	else
		config->type = st_amba_type1;

	if (of_get_property(cn, "st,max_opcode_LD4_ST4", NULL))
		config->max_opcode = st_amba_opc_LD4_ST4;

	if (of_get_property(cn, "st,max_opcode_LD8_ST8", NULL))
		config->max_opcode = st_amba_opc_LD8_ST8;

	if (of_get_property(cn, "st,max_opcode_LD16_ST16", NULL))
		config->max_opcode = st_amba_opc_LD16_ST16;

	if (of_get_property(cn, "st,max_opcode_LD32_ST32", NULL))
		config->max_opcode = st_amba_opc_LD32_ST32;

	if (of_get_property(cn, "st,max_opcode_LD64_ST64", NULL))
		config->max_opcode = st_amba_opc_LD64_ST64;

	of_property_read_u32(cn, "st,chunks_in_msg", &config->chunks_in_msg);

	of_property_read_u32(cn, "st,packets_in_chunk",
			     &config->packets_in_chunk);

	if (of_get_property(cn, "st,write_posting", NULL))
		config->write_posting = st_amba_write_posting_enabled;
	else
		config->write_posting = st_amba_write_posting_disabled;

	of_property_read_u32(cn, "st,req_timeout", &config->type1.req_timeout);

	if (of_get_property(cn, "st,sd_config_missing", NULL))
		config->type2.sd_config_missing = 1;

	of_property_read_u32(cn, "st,threshold", &config->type2.threshold);

	if (of_get_property(cn, "st,req_notify_cell_based", NULL))
		config->type2.req_notify = st_amba_ahb_cell_based;
	else
		config->type2.req_notify = st_amba_ahb_burst_based;

	if (of_get_property(cn, "st,complete_on_error", NULL))
		config->type2.cont_on_error = st_amba_complete_transaction;
	else
		config->type2.cont_on_error = st_amba_abort_transaction;

	if (of_get_property(cn, "st,msg_merge", NULL))
		config->type2.msg_merge = st_amba_msg_merge_enabled;
	else
		config->type2.msg_merge = st_amba_msg_merge_disabled;

	if (of_get_property(cn, "st,cell_based_trigger", NULL))
		config->type2.trigger_mode = st_amba_stbus_cell_based;
	else
		config->type2.trigger_mode = st_amba_stbus_threshold_based;

	if (of_get_property(cn, "st,read_ahead", NULL))
		config->type2.read_ahead = st_amba_read_ahead_enabled;
	else
		config->type2.read_ahead = st_amba_read_ahead_disabled;

	return config;
}
EXPORT_SYMBOL(st_of_get_amba_config);
