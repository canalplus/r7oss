#include <linux/proc_fs.h>
#include <linux/module.h>

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_debugfs.h"
#include "asc_internal.h"

#ifdef CONFIG_PROC_FS
static void scr_dump_atr_parsed_data(char *buf, scr_atr_parsed_data_t data);
static int scr_retries_read(struct file *file,
			char __user *buffer,
			size_t	len,
			loff_t	*offset)
{
	scr_ctrl_t	*scr_handle_p = file->private_data;
	uint32_t	scr_retries = 0;
	char		_retries[32];

	(void) stm_asc_scr_get_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES, &scr_retries);

	/* convert to ASCII before passing to user */
	sprintf(_retries, "%d\n", scr_retries);
	return simple_read_from_buffer(buffer, len, offset, /* to */
				_retries, strlen(_retries));
}

static int scr_retries_write(struct file *file,
			const char __user *buffer,
			size_t len,
			loff_t	*offset)
{
	scr_ctrl_t	*scr_handle_p = file->private_data;
	unsigned long	scr_retries = 0;
	int		ret;

	ret = kstrtoul_from_user(buffer, len, 10, &scr_retries);
	if (ret)
		return ret;
	/* would be better to check scr_retries boundaries ? */
	stm_asc_scr_set_control(scr_handle_p->asc_handle,
				STM_ASC_SCR_TX_RETRIES, scr_retries);
	return len;
}

static int scr_open(struct inode *inode, struct file *file)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	file->private_data = (void *) PDE_DATA(inode);
#else
	file->private_data = (void *) PDE(inode)->data;
#endif
	printk(KERN_ALERT "%s (user data %p)\n", __func__, file->private_data);
	/*file->private_data = inode->i_private; */
	if (!file->private_data)
		return -EINVAL;
	return 0;
}

/*
 * close: do nothing!
 */
static int scr_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations scr_retries_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_retries_read,
	.write		= scr_retries_write,
	.llseek		= default_llseek,
	.release		= scr_close,
};

static int scr_atr_show(struct file *file,
			char __user *buffer, size_t len, loff_t *offset)

{
	uint8_t buf[800];
	scr_ctrl_t	*scr_handle_p = file->private_data;
	scr_dump_atr_parsed_data(buf, scr_handle_p->atr_response.parsed_data);
	/* note that should return the total length of write */
	return simple_read_from_buffer(buffer, len, offset,
					buf, strlen(buf));
}

static const struct file_operations scr_atr_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_atr_show,
	.write		= NULL, /* not allowed */
	.llseek		= default_llseek,
	.release		= scr_close,
};

static int scr_inverse_detect_read(struct file *file,
			char __user *buffer,
			size_t len,
			loff_t *offset)
{
	scr_ctrl_t	*scr_handle_p = file->private_data;
	char _polarity[32];

	/* convert to ASCII before passing to user */
	sprintf(_polarity, "%d\n", scr_handle_p->detect_inverted);
	return simple_read_from_buffer(buffer, len, offset,
					_polarity, strlen(_polarity));
}

static int scr_inverse_detect_write(struct file *file,
				const char __user *buffer,
				size_t len,
				loff_t	*offset)
{
	unsigned long inverse_detect = 0;
	scr_ctrl_t	*scr_handle_p = file->private_data;
	int	ret;

	ret = kstrtoul_from_user(buffer, len, 10, &inverse_detect);
	if (ret)
		return ret;

	scr_handle_p->detect_inverted = (uint32_t)inverse_detect;
	return len;
}

static const struct file_operations scr_inverse_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_inverse_detect_read,
	.write		= scr_inverse_detect_write,
	.llseek		= default_llseek,
	.release		= scr_close,
};

static int scr_fifo_size_read(struct file *file,
			char __user *buffer,
			size_t len,
			loff_t	*offset)
{
	char _fifo_size[32];
	scr_ctrl_t	*scr_handle_p = file->private_data;
	asc_ctrl_t	*asc_ctrl_p = (asc_ctrl_t *)(scr_handle_p->asc_handle);

	/* convert to ASCII before passing to user */
	sprintf(_fifo_size, "%d\n", asc_ctrl_p->device.hwfifosize);
	return simple_read_from_buffer(buffer, len, offset,
					_fifo_size, strlen(_fifo_size));
}

static int scr_fifo_size_write(struct file	*file,
			const char __user *buffer,
			size_t	len,
			loff_t *offset)

{
	int	ret;
	unsigned long fifo_size = 0;
	scr_ctrl_t	*scr_handle_p = file->private_data;
	asc_ctrl_t	*asc_ctrl_p = (asc_ctrl_t *)(scr_handle_p->asc_handle);

	ret = kstrtoul_from_user(buffer, len, 10, &fifo_size);

	if (ret)
		return ret;
	asc_ctrl_p->device.hwfifosize = (uint32_t)fifo_size;
	return len;
}

static const struct file_operations scr_fifo_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_fifo_size_read,
	.write		= scr_fifo_size_write,
	.llseek		= default_llseek,
	.release		= scr_close,
};

void scr_dump_atr(scr_atr_parsed_data_t data)
{
	#if SCR_DEBUG
	uint8_t buf[1800];
	scr_dump_atr_parsed_data(buf, data);
	scr_error_print("%s", buf);
	#endif
	return;
}

int scr_asc_register_dump_get(struct file *file,
			char __user *buffer,
			size_t	len,
			loff_t *offset)
{
	asc_ctrl_t *asc_ctrl_p;
	scr_ctrl_t *scr_handle_p = file->private_data;
	asc_ctrl_p = (asc_ctrl_t *)(scr_handle_p->asc_handle);

	dumpreg(asc_ctrl_p);

	return 0;
}

static const struct file_operations scr_register_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_asc_register_dump_get,
	.write		= NULL, /* not allowed */
	.llseek		= default_llseek,
	.release		= scr_close,
};

int scr_asc_interrupt_stats_get(struct file *file,
			char __user *buffer,
			size_t len,
			loff_t *offset)
{
	uint8_t buf[800];
	asc_ctrl_t *asc_ctrl_p;
	struct asc_statistics_t Stats_p;
	scr_ctrl_t	*scr_handle_p = file->private_data;
	asc_ctrl_p = (asc_ctrl_t *)(scr_handle_p->asc_handle);
	Stats_p = asc_ctrl_p->statistics;

	/* convert to ASCII before passing to user */
	sprintf(buf,
	" time: %ld | IT: %i | THRES_INT: %i | NKD: %i | TX_FULL: %i |"
	" RX_HALFFULL: %i | TOE: %i | TONE: %i |"
	" OVERRUN_ERR: %i | FRAME_ERR: %i | PARITY_ERR: %i |"
	" TX_HALFEMPTY: %i | TX_EMPTY: %i | RX_BUFFULL: %i |"
	" Bytes Received: %i | Bytes Transmitted %i |"
	" Residual: %i | Reserved %i |\n",
			jiffies * 1000 / HZ,
			Stats_p.ITCount,
			Stats_p.KStats[11],
			Stats_p.KStats[10],
			Stats_p.KStats[9],
			Stats_p.KStats[8],
			Stats_p.KStats[7],
			Stats_p.KStats[6],
			Stats_p.KStats[5],
			Stats_p.KStats[4],
			Stats_p.KStats[3],
			Stats_p.KStats[2],
			Stats_p.KStats[1],
			Stats_p.KStats[0],
			Stats_p.NumberBytesReceived,
			Stats_p.NumberBytesTransmitted,
			Stats_p.KStats[12] + Stats_p.KStats[13] +
			Stats_p.KStats[14] + Stats_p.KStats[15] +
			Stats_p.KStats[16] + Stats_p.KStats[17] +
			Stats_p.KStats[18] + Stats_p.KStats[19] +
			Stats_p.KStats[20] + Stats_p.KStats[21],
			Stats_p.KStats[22] + Stats_p.KStats[23] +
			Stats_p.KStats[24] + Stats_p.KStats[25] +
			Stats_p.KStats[26] + Stats_p.KStats[27] +
			Stats_p.KStats[28] + Stats_p.KStats[29] +
			Stats_p.KStats[30] + Stats_p.KStats[31]);
	return simple_read_from_buffer(buffer, len, offset,
				buf, strlen(buf));
}

static const struct file_operations scr_int_status_fops = {
	.owner		= THIS_MODULE,
	.open		= scr_open,
	.read		= scr_asc_interrupt_stats_get,
	.write		= NULL, /* not allowed */
	.llseek		= default_llseek,
	.release		= scr_close,
};

static void scr_dump_atr_parsed_data(char *buf, scr_atr_parsed_data_t data)
{
	/* Doesn't seems to be correct!
	* o is available only whit the DEBUG enabled while the other
	* proc entry are always available
	* o takes advantage of /proc mechanism to be activate, but prints
	* on system console (printk) only !?
	* Should be reworked and an easy way to manage such type of
	* "show" is to use sequential file support (see "seq_printf")
	*/
	sprintf(buf, "Bit convention            %d\n", data.bit_convention);
	sprintf(buf, "f_int                     %d\n", data.f_int);
	sprintf(buf, "d_int                    %d\n", data.d_int);
	sprintf(buf, "wi                        %d\n", data.wi);
	sprintf(buf, "N                         %d\n", data.N);
	sprintf(buf, "specific_mode             %d\n", data.specific_mode);
	sprintf(buf, "specific_protocol_type    %d\n",
			data.specific_protocol_type);
	sprintf(buf, "negotiable_mode           %d\n",
			data.negotiable_mode);
	sprintf(buf, "parameters_to_use         %d\n",
			data.parameters_to_use);
	sprintf(buf, "first_protocol            %d\n", data.first_protocol);
	sprintf(buf, "ifsc                      %d\n", data.ifsc);
	sprintf(buf, "clock_stop_indicator      %d\n",
			data.clock_stop_indicator);
	sprintf(buf, "class_supported           %d\n",
			data.class_supported);
	sprintf(buf, "spu                       %d\n", data.spu);
	sprintf(buf, "rc                        %d\n", data.rc);
	sprintf(buf, "character_waiting_index   %d\n",
			data.character_waiting_index);
	sprintf(buf, "block_waiting_index       %d\n",
			data.block_waiting_index);
	sprintf(buf, "supported_protocol        %d\n",
			data.supported_protocol);
	sprintf(buf, "%c", '\0');
}

int scr_create_procfs(uint32_t scr_num, scr_ctrl_t *scr_handle_p)
{
	char scr_dir_temp[20];
	struct proc_dir_entry *lpde;
	char	*lname;

	/* create proc directory for Master card */
	sprintf(scr_dir_temp, "%s-%d", "stm_scr", scr_num);
	scr_handle_p->scr_dir = proc_mkdir(scr_dir_temp, NULL);
	if (!scr_handle_p->scr_dir)
	{
		scr_error_print("Failed to create stm_scr directory folder!\n");
		return -1;
	}

	scr_debug_print("%s (user data %p)\n", __func__, scr_handle_p);

	lname = "scr_retries";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_retries_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	/* show ATR (Answer To Reset) parameters */
	lname = "scr_atr_dump";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_atr_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	/* inverse detect */
	lname = "scr_inverse_detect";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_inverse_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	/* fifo size */
	lname = "scr_asc_fifo_size";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_fifo_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	/*asc register dump */
	lname = "scr_asc_register_dump";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_register_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	/* asc interrupt status */
	lname = "scr_asc_interrupt_stats";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				scr_handle_p->scr_dir, &scr_int_status_fops,
				(void *) scr_handle_p);
	if (!lpde)
		goto _proc_create_failure;

	return 0;

_proc_create_failure:
	scr_error_print("failed to create proc entry for %s\n", lname);
	return -1;

}

void scr_delete_procfs(uint32_t scr_num, scr_ctrl_t *scr_handle_p)
{
	/* Proc support a sub-tree removal, likely easier */
	uint8_t scr_dir_temp[20];
	sprintf(scr_dir_temp, "%s-%d", "stm_scr", scr_num);
	remove_proc_entry("scr_asc_fifo_size", scr_handle_p->scr_dir);
	remove_proc_entry("scr_inverse_detect", scr_handle_p->scr_dir);
	remove_proc_entry("scr_atr_dump", scr_handle_p->scr_dir);
	remove_proc_entry("scr_retries", scr_handle_p->scr_dir);
	remove_proc_entry("scr_asc_register_dump", scr_handle_p->scr_dir);
	remove_proc_entry("scr_asc_interrupt_stats", scr_handle_p->scr_dir);
	remove_proc_entry(scr_dir_temp, NULL);
}
#endif
