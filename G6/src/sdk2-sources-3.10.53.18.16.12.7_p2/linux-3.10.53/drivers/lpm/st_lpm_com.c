/*
 * This driver implements communication with Standby Controller
 * in some STMicroelectronics devices
 *
 * Copyright (C) 2014 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 * Author:Sudeep Biswas <sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/power/st_lpm.h>
#include <linux/power/st_lpm_def.h>


static struct st_lpm_ops *st_lpm_ops;
static void *st_lpm_private_data;

static DEFINE_MUTEX(st_lpm_mutex);

/*a Set of LPM callbacks and datas */
struct st_lpm_callback {
	int (*fn)(void *);
	void *data;
};
static struct st_lpm_callback st_lpm_callbacks[ST_LPM_MAX];

int st_lpm_get_command_data_size(unsigned char command_id)
{
	static const unsigned char size[] = {
	[LPM_MSG_NOP]		= LPM_MSG_NOP_SIZE,
	[LPM_MSG_VER]		= LPM_MSG_VER_SIZE,
	[LPM_MSG_READ_RTC]	= LPM_MSG_READ_RTC_SIZE,
	[LPM_MSG_SET_TRIM]	= LPM_MSG_SET_TRIM_SIZE,
	[LPM_MSG_ENTER_PASSIVE] = LPM_MSG_ENTER_PASSIVE_SIZE,
	[LPM_MSG_SET_WDT]	= LPM_MSG_SET_WDT_SIZE,
	[LPM_MSG_SET_RTC]	= LPM_MSG_SET_RTC_SIZE,
	[LPM_MSG_SET_FP]	= LPM_MSG_SET_FP_SIZE,
	[LPM_MSG_SET_TIMER]	= LPM_MSG_SET_TIMER_SIZE,
	[LPM_MSG_GET_STATUS]	= LPM_MSG_GET_STATUS_SIZE,
	[LPM_MSG_GEN_RESET]	= LPM_MSG_GEN_RESET_SIZE,
	[LPM_MSG_SET_WUD]	= LPM_MSG_SET_WUD_SIZE,
	[LPM_MSG_GET_WUD]	= LPM_MSG_GET_WUD_SIZE,
	[LPM_MSG_LGWR_OFFSET]	= LPM_MSG_LGWR_OFFSET_SIZE,
	[LPM_MSG_SET_PIO]	= LPM_MSG_SET_PIO_SIZE,
	[LPM_MSG_SET_ADV_FEA]	= LPM_MSG_SET_ADV_FEA_SIZE,
	[LPM_MSG_SET_KEY_SCAN]	= LPM_MSG_SET_KEY_SCAN_SIZE,
	[LPM_MSG_CEC_ADDR]	= LPM_MSG_CEC_ADDR_SIZE,
	[LPM_MSG_CEC_PARAMS]	= LPM_MSG_CEC_PARAMS_SIZE,
	[LPM_MSG_CEC_SET_OSD_NAME] = LPM_MSG_CEC_SET_OSD_NAME_SIZE,
	[LPM_MSG_SET_IR]	= LPM_MSG_SET_IR_SIZE,
	[LPM_MSG_GET_IRQ]	= LPM_MSG_GET_IRQ_SIZE,
	[LPM_MSG_TRACE_DATA]	= LPM_MSG_TRACE_DATA_SIZE,
	[LPM_MSG_BKBD_READ]	= LPM_MSG_BKBD_READ_SIZE,
	[LPM_MSG_BKBD_WRITE]	= LPM_MSG_BKBD_WRITE_SIZE,
	[LPM_MSG_REPLY]		= LPM_MSG_REPLY_SIZE,
	[LPM_MSG_ERR]		= LPM_MSG_ERR_SIZE,
	[LPM_MSG_EDID_INFO]	= LPM_MSG_EDID_INFO_SIZE,
	};

	if (command_id > LPM_MSG_ERR)
		return -EINVAL;
	return size[command_id];
}

static inline int st_lpm_ops_exchange_msg(
	struct lpm_message *command,
	struct lpm_message *response)
{
	if (!st_lpm_ops || !st_lpm_ops->exchange_msg)
		return -EINVAL;

	return st_lpm_ops->exchange_msg(command,
		response, st_lpm_private_data);
}

static inline int st_lpm_ops_write_bulk(u16 size, const char *msg)
{
	if (!st_lpm_ops || !st_lpm_ops->write_bulk)
		return -EINVAL;

	return st_lpm_ops->write_bulk(size, msg,
		st_lpm_private_data, NULL);
}

static inline int st_lpm_ops_read_bulk(u16 size,
	u16 offset, char *msg)
{
	if (!st_lpm_ops || !st_lpm_ops->read_bulk)
		return -EINVAL;
	return st_lpm_ops->read_bulk(size, offset, msg,
		st_lpm_private_data);
}

static inline int st_lpm_ops_config_reboot(enum st_lpm_config_reboot_type type)
{
	if (!st_lpm_ops || !st_lpm_ops->config_reboot)
		return -EINVAL;
	return st_lpm_ops->config_reboot(type, st_lpm_private_data);
}

int st_lpm_set_ops(struct st_lpm_ops *ops, void *private_data)
{
	if (!ops)
		return -EINVAL;
	mutex_lock(&st_lpm_mutex);
	if (st_lpm_ops) {
		mutex_unlock(&st_lpm_mutex);
		return -EBUSY;
	}
	st_lpm_ops = ops;
	st_lpm_private_data = private_data;
	mutex_unlock(&st_lpm_mutex);
	return 0;
}

/**
 * st_lpm_write_edid()	To write the EDID Info to SBC-DMEM
 * @data		byte array to write
 * @block_num		Block number to write
 *
 * Return - 0 on success
 * Return -  negative error code on failure.
 */
int st_lpm_write_edid(unsigned char *data, u8 block_num)
{
	if (!st_lpm_ops || !st_lpm_ops->write_edid || !data)
		return -EINVAL;

	return st_lpm_ops->write_edid(data, block_num, st_lpm_private_data);
}
EXPORT_SYMBOL(st_lpm_write_edid);

/**
 * st_lpm_read_edid()	To read EDID Info from SBC-DMEM
 * @data		byte array to write
 * @block_num		Block number to read
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_read_edid(unsigned char *data, u8 block_num)
{
	if (!st_lpm_ops || !st_lpm_ops->read_edid || !data)
		return -EINVAL;

	return st_lpm_ops->read_edid(data, block_num, st_lpm_private_data);
}
EXPORT_SYMBOL(st_lpm_read_edid);

/**
 * st_lpm_write_dmem()	To write to SBC-DMEM
 * @data		byte array to write
 * @size		number of bytes to write
 * @offset		offset in DMEM where to start write from
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_write_dmem(unsigned char *data, unsigned int size,
		      int offset)
{
	if (!st_lpm_ops || !st_lpm_ops->write_bulk || !data)
		return -EINVAL;

	if (offset < 0)
		return -EINVAL;

	return st_lpm_ops->write_bulk(size, data, st_lpm_private_data, &offset);
}
EXPORT_SYMBOL(st_lpm_write_dmem);

/**
 * st_lpm_read_dmem()	To read from SBC-DMEM
 * @data		byte array to read into
 * @size		number of bytes to read
 * @offset		offset in DMEM where to start read from
 *
 * Return - 0 on success
 * Return -  negative error code on failure.
 */
int st_lpm_read_dmem(unsigned char *data, unsigned int size,
		     int offset)
{
	if (!st_lpm_ops || !st_lpm_ops->read_bulk || !data)
		return -EINVAL;

	if (offset < 0)
		return -EINVAL;

	return st_lpm_ops->read_bulk(size, offset, data, st_lpm_private_data);
}
EXPORT_SYMBOL(st_lpm_read_dmem);

/**
 * st_lpm_get_dmem_offset()	To get offset inside SBC-DMEM
 * @offset_type			type of the offset that is wanted
 *
 * Return - 0 on success
 * Return -  negative error code on failure.
 */
int st_lpm_get_dmem_offset(enum st_lpm_sbc_dmem_offset_type offset_type)
{
	if (!st_lpm_ops || !st_lpm_ops->get_dmem_offset)
		return -EINVAL;

	return st_lpm_ops->get_dmem_offset(offset_type, st_lpm_private_data);
}
EXPORT_SYMBOL(st_lpm_get_dmem_offset);

/**
 * st_lpm_get_version() - To get version of driver and firmware
 * @driver_version:	driver version
 * @fw_version:		firmware version
 *
 * This function will return firmware and driver version in parameters.
 *
 * Return - 0 on success
 * Return -  negative error code on failure.
 */
int st_lpm_get_version(struct st_lpm_version *driver_version,
	struct st_lpm_version *fw_version)
{
	int ret = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_VER,
	};

	/* Check parameters */
	if (unlikely(!driver_version || !fw_version))
		return -EINVAL;

	ret = st_lpm_ops_exchange_msg(&command, &response);

	if (ret)
		return ret;

	/* Copy the firmware version in paramater */
	fw_version->major_comm_protocol = response.buf[0] >> 4;
	fw_version->minor_comm_protocol = response.buf[0] & 0x0F;
	fw_version->major_soft = response.buf[1] >> 4;
	fw_version->minor_soft = response.buf[1] & 0x0F;
	fw_version->patch_soft = response.buf[2] >> 4;
	fw_version->month = response.buf[2] & 0x0F;
	memcpy(&fw_version->day, &response.buf[3], 3);

	driver_version->major_comm_protocol = LPM_MAJOR_PROTO_VER;
	driver_version->minor_comm_protocol = LPM_MINOR_PROTO_VER;
	driver_version->major_soft = LPM_MAJOR_SOFT_VER;
	driver_version->minor_soft = LPM_MINOR_SOFT_VER;
	driver_version->patch_soft = LPM_PATCH_SOFT_VER;
	driver_version->month = LPM_BUILD_MONTH;
	driver_version->day = LPM_BUILD_DAY;
	driver_version->year = LPM_BUILD_YEAR;

	return ret;
}
EXPORT_SYMBOL(st_lpm_get_version);

/**
 * st_lpm_setup_ir() - To set ir key setup
 * @num_keys:		Number of IR keys
 * @ir_key_info:	Information of IR keys
 *
 * This function will configure IR information on SBC firmware.
 * User needs to pass on which IR keys wakeup is required and
 * the expected pattern for those keys.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
*/
int st_lpm_setup_ir(u8 num_keys, struct st_lpm_ir_keyinfo *ir_key_info)
{
	struct st_lpm_ir_keyinfo *this_key;
	u16 ir_size;
	char *buf, *org_buf;
	int count, i, err = 0;

	for (count = 0; count < num_keys; count++) {
		struct st_lpm_ir_key *key_info;
		this_key = ir_key_info;

		ir_key_info++;
		key_info = &this_key->ir_key;
		/* Check key crediantials */
		if (unlikely(this_key->time_period == 0 ||
			     key_info->num_patterns >= 64))
			return -EINVAL;

		ir_size = key_info->num_patterns*2 + 12;
		buf = kmalloc(ir_size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		org_buf = buf;

		/* Fill buffer */
		*buf++ = LPM_MSG_SET_IR;
		*buf++ = 0;
		*buf++ = this_key->ir_id & 0xF;
		*buf++ = ir_size;
		*buf++ = this_key->time_period & 0xFF;
		*buf++ = (this_key->time_period >> 8) & 0xFF;
		*buf++ = this_key->time_out & 0xFF;
		*buf++ = (this_key->time_out >> 8) & 0xFF;

		if (!this_key->tolerance)
			this_key->tolerance = 10;

		*buf++ = this_key->tolerance;
		*buf++ = key_info->key_index & 0xF;
		*buf++ = key_info->num_patterns;
		/* Now compress the actual data and copy */
		buf = org_buf + 12;

		for (i = 0; i < key_info->num_patterns; i++) {
			key_info->fifo[i].mark /= this_key->time_period;
			*buf++ = key_info->fifo[i].mark;
			key_info->fifo[i].symbol /=  this_key->time_period;
			*buf++ = key_info->fifo[i].symbol;
		}

		err = st_lpm_ops_write_bulk(ir_size, org_buf);
		kfree(org_buf);

		if (err < 0)
			break;
	}

	return err;
}
EXPORT_SYMBOL(st_lpm_setup_ir);

/**
 * lpm_get_trigger_data - to get which device woken up the system
 * @wakeup_device:      Which device did the wakeup
 * @size_max:           max size to read
 * @size_min:           min size
 * @data:               pointer to get data
 *
 * This function will return data associated with wakeup device
 * This is internal function
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */

int st_lpm_get_trigger_data(enum st_lpm_wakeup_devices wakeup_device,
		unsigned int size_max, char *data)
{
	int err = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_GET_IRQ,
	};

	command.buf[0] = wakeup_device & 0xFF;
	command.buf[1] = (wakeup_device >> 8) & 0xFF;
	command.buf[2] = size_max & 0xFF;
	command.buf[3] = (size_max >> 8) & 0xFF;

	err = st_lpm_ops_exchange_msg(&command, &response);

	if (!err)
		memcpy(data, &response.buf[2], 2);

	return err;
}
EXPORT_SYMBOL(st_lpm_get_trigger_data);

/**
 * st_lpm_get_wakeup_info() - To get additional info about wakeup device
 * @wakeupdevice:	wakeup device id
 * @validsize:		read valid size will be returned
 * @datasize:		data size to read
 * @data:		data pointer
 *
 * This API will return additional data for wakeup device if required.
 *
 * Return - 0 on success if data read from SBC is <= datasize
 * Return - 1 if data available with SBC is > datasize
 * Return - negative error on failure
*/
int st_lpm_get_wakeup_info(enum st_lpm_wakeup_devices *wakeupdevice,
			s16 *validsize, u16 datasize, char *data)
{
	int err = 0;
	unsigned short offset;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_GET_IRQ,
	};

	command.buf[0] = *wakeupdevice;
	command.buf[1] = (*wakeupdevice & 0xFF00) >> 8;

	/* Copy size requested */
	put_unaligned_le16(datasize, &command.buf[2]);
	err = st_lpm_ops_exchange_msg(&command, &response);
	if (unlikely(err < 0))
		goto exit;

	/* Two response are possible*/
	if (response.command_id == LPM_MSG_BKBD_READ) {
		/*
		 * If SBC replied to read response from its DMEM then
		 * get the offset to read from SBC memory.
		 */
		offset = get_unaligned_le32(&response.buf[2]);

		/* Get valid size from SBC */
		st_lpm_ops_read_bulk(2, offset + 2, (char *)validsize);

		/* Check if bit#15 is set */
		if (*validsize < 0) {
			pr_err("st lpm: Error data size not valid\n");
			err = -EINVAL;
			goto exit;
		}

		/*
		 * Below condition is not possible
		 * SBC have to provide data less than or equal to datasize
		 * Added below check, if some bug pops up in firmware
		 */
		if (unlikely(*validsize > datasize)) {
			pr_err("st lpm: more data than allowed\n");
			err = -EINVAL;
			goto exit;
		}
		st_lpm_ops_read_bulk(*validsize, offset + 4, data);

	} else {
		*validsize = get_unaligned_le16(response.buf);
		/* Check if bit#15 is set in mailbox */
		if (*validsize < 0) {
			pr_err("st lpm: Error data size not valid\n");
			err = -EINVAL;
			goto exit;
		}

		/*
		 * Below condition is not possible
		 * SBC have to provide data less than or equal to datasize
		 * Added below check, if some bug pops up in firmware
		 */
		if (unlikely(*validsize > datasize)) {
			pr_err("st lpm: more data than allowed\n");
			err = -EINVAL;
			goto exit;
		}
		/* Copy data to user */
		memcpy(data, &response.buf[2], *validsize);
	}
exit:
	return err;
}
EXPORT_SYMBOL(st_lpm_get_wakeup_info);

/**
 * st_lpm_configure_wdt - Set watchdog timeout for Standby Controller
 * @time_in_ms:	timeout in milli second
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_configure_wdt(u16 time_in_ms, u8 wdt_type)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_WDT,
	};

	if (!time_in_ms)
		return -EINVAL;

	put_unaligned_le16(time_in_ms, command.buf);
	command.buf[2] = wdt_type;

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_configure_wdt);

/**
 * st_lpm_get_fw_state - Get the SBC firmware status
 * @fw_state:	enum for firmware status
 *
 * Firmware status will be returned in passed parameter.
 *
 * Return - 0 on success
 * Return - negative  error code on failure.
 */
int st_lpm_get_fw_state(enum st_lpm_sbc_state *fw_state)
{
	int ret = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_GET_STATUS,
	};

	if (!fw_state)
		return -EINVAL;

	ret = st_lpm_ops_exchange_msg(&command, &response);

	if (likely(ret == 0))
		*fw_state = response.buf[0];

	return ret;
}
EXPORT_SYMBOL(st_lpm_get_fw_state);

/**
 * st_lpm_reset() - To reset part of full SOC
 * @reset_type:	type of reset
 *
 * Return - 0 on success
 * Return - negative error on failure
 */
int st_lpm_reset(enum st_lpm_reset_type reset_type)
{
	int ret = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_GEN_RESET,
	};

	command.buf[0] = reset_type;
	ret = st_lpm_ops_exchange_msg(&command, &response);

	if (!ret && reset_type == ST_LPM_SBC_RESET) {
		int i = 0;
		enum st_lpm_sbc_state fw_state;
		/* Wait till 1 second to get response from firmware */
		for (i = 0; i < 10; ++i) {
			ret = st_lpm_get_fw_state(&fw_state);
			if (!ret)
				break;
			mdelay(100);
		}
	}
	return ret;
}
EXPORT_SYMBOL(st_lpm_reset);

/**
 * st_lpm_set_wakeup_device - To set wakeup devices
 * @devices:	All enabled wakeup devices
 *
 * In older protocol version wakeup devices were limited to 8,
 * whereas new protocol version supports upto 10 wakeup devices.
 * Therefore two different message were used to set wakeup devices,
 * driver checks firmware version and send wakeup device accordingly.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_set_wakeup_device(u16 devices)
{
	struct st_lpm_adv_feature feature;
	feature.feature_name = ST_LPM_WU_TRIGGERS;
	put_unaligned_le16(devices, feature.params.set_params);

	return st_lpm_set_adv_feature(1, &feature);
}
EXPORT_SYMBOL(st_lpm_set_wakeup_device);

/**
 * st_lpm_set_wakeup_time - To set wakeup time
 * @timeout:	Timeout in seconds after which wakeup is required
 *
 * Wakeup will be done after current time + timeout
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_set_wakeup_time(u32 timeout)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_TIMER,
	};

	timeout = cpu_to_le32(timeout);
	/* Copy timeout into message */
	memcpy(command.buf, &timeout, 4);

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_set_wakeup_time);

/**
 * st_lpm_set_rtc - To set rtc time for standby controller
 * @new_rtc:	rtc value
 *
 * SBC can display RTC clock when in standby mode using this RTC value,
 * This RTC will act as base for RTC hardware of SBC.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_set_rtc(struct rtc_time *new_rtc)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_RTC,
	};

	/* Copy received values of rtc into message */
	if (new_rtc->tm_year >= MIN_RTC_YEAR &&
	    new_rtc->tm_year <= MAX_RTC_YEAR)
		command.buf[5] = new_rtc->tm_year - MIN_RTC_YEAR;
	else
		return  -EINVAL;

	command.buf[4] = new_rtc->tm_mon;
	command.buf[3] = new_rtc->tm_mday;
	command.buf[2] = new_rtc->tm_sec;
	command.buf[1] = new_rtc->tm_min;
	command.buf[0] = new_rtc->tm_hour;

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_set_rtc);

int st_lpm_cec_set_osd_name(struct st_lpm_cec_osd_msg *params)
{
	int len = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_CEC_SET_OSD_NAME,
	};

	if (!params)
		return -EINVAL;

	command.buf[0] = params->size;

	if (command.buf[0] > ST_LPM_CEC_MAX_OSD_NAME_LENGTH)
		return -EINVAL;

	if (command.buf[0] < ST_LPM_CEC_MAX_OSD_NAME_LENGTH)
		len = command.buf[0];
	else if (command.buf[0] == ST_LPM_CEC_MAX_OSD_NAME_LENGTH)
		len = command.buf[0]-1;

	memcpy(&command.buf[1], &params->name, len);
	if (command.buf[0] == ST_LPM_CEC_MAX_OSD_NAME_LENGTH)
		command.transaction_id =
			params->name[ST_LPM_CEC_MAX_OSD_NAME_LENGTH-1];

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_cec_set_osd_name);

int st_lpm_get_rtc(struct rtc_time *new_rtc)
{
	int err = 0, hours, t1;
	unsigned long time_in_sec;
	struct rtc_time time;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_READ_RTC,
	};

	if (!new_rtc)
		return -EINVAL;

	err = st_lpm_ops_exchange_msg(&command, &response);

	if (err >= 0 && (response.command_id & LPM_MSG_REPLY)) {
		hours = (response.buf[9] << 24) |
			(response.buf[8] << 16) |
			(response.buf[7] << 8)  |
			(response.buf[6]);

		t1 = (((hours * 60) + response.buf[1]) * 60 +
				response.buf[2]);

		time.tm_year = response.buf[12] + MIN_RTC_YEAR;
		time.tm_mon = response.buf[11];
		time.tm_mday = response.buf[10];
		time.tm_hour = 0;
		time.tm_min = 0;
		time.tm_sec = 0;
		time_in_sec = mktime(time.tm_year, time.tm_mon, time.tm_mday,
				time.tm_hour, time.tm_min, time.tm_sec);
		time_in_sec += t1;
		rtc_time_to_tm(time_in_sec , new_rtc);
	}

	return err;
}
EXPORT_SYMBOL(st_lpm_get_rtc);

int stm_lpm_get_standby_time(u32 *time)
{
	int err;
	struct st_lpm_adv_feature feature = {0};

	err = st_lpm_get_adv_feature(0, 0, &feature);
	if (likely(err == 0))
		memcpy((char *)time, &feature.params.get_params[6], 4);

	return err;
}
EXPORT_SYMBOL(stm_lpm_get_standby_time);

/**
 * st_lpm_get_wakeup_device - To get wake devices
 * @wakeup_device:	wakeup device
 *
 * This function will return wakeup device because of which SBC has
 * woken up the SOC.
 * In older protocol version wakeup devices were limited to 8,
 * whereas new Protocol version supports upto 10 wakeup devices.
 * Therefore two different message were used to get wakeup device.
 * Driver checks firmware version and send wakeup device accordingly.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_get_wakeup_device(enum st_lpm_wakeup_devices *wakeup_device)
{
	int err = 0;
	struct st_lpm_adv_feature  feature = {0};

	if (unlikely(wakeup_device == NULL))
		return -EINVAL;

	err = st_lpm_get_adv_feature(0, 0, &feature);

	if (!err) {
		*wakeup_device = feature.params.get_params[5] << 8;
		*wakeup_device |= feature.params.get_params[4];
	}

	return err;
}
EXPORT_SYMBOL(st_lpm_get_wakeup_device);

/**
 * st_lpm_setup_fp - To set front panel information for SBC
 * @fp_setting:	Front panel setting
 *
 * This function will set front panel setting.
 * By default host CPU is assumed to control the front panel.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_fp(struct st_lpm_fp_setting *fp_setting)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_FP,
	};

	if (unlikely(fp_setting == NULL))
		return -EINVAL;

	command.buf[0] = (fp_setting->owner & OWNER_MASK)
		| (fp_setting->am_pm & 1) << 2
		| (fp_setting->brightness & NIBBLE_MASK) << 4;

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_setup_fp);

/**
 * st_lpm_setup_pio - To inform SBC about PIO Use
 * @pio_setting:	pio_setting
 *
 * This function will inform SBC about PIO use,
 * driver running on Host must configure the PIO.
 * SBC will not do any configuration for PIO.
 * GPIO can be used as power control for board,
 * gpio interrupt, external interrupt, Phy WOL wakeup.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_pio(struct st_lpm_pio_setting *pio_setting)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_PIO,
	};

	if (unlikely(pio_setting == NULL ||
		(pio_setting->pio_direction &&
		 pio_setting->interrupt_enabled)))
		return -EINVAL;

	command.buf[0] = pio_setting->pio_bank;
	if (pio_setting->pio_use == ST_LPM_PIO_EXT_IT)
		command.buf[0] = 0xFF;

	pio_setting->pio_pin &= NIBBLE_MASK;
	command.buf[1] = pio_setting->pio_level << PIO_LEVEL_SHIFT |
		 pio_setting->interrupt_enabled <<  PIO_IT_SHIFT |
		 pio_setting->pio_direction << PIO_DIRECTION_SHIFT |
		 pio_setting->pio_pin;
	command.buf[2] = pio_setting->pio_use;

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_setup_pio);

/**
 * st_lpm_setup_keyscan - To inform SBC about wakeup key of Keyscan
 * @key_data:	Keyscan Key info
 *
 * This function will inform SBC about keyscan key
 * on which SBC will wakeup.
 * Driver running on Host must configure the Keyscan IP.
 * SBC will not do any configuration.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_keyscan(u16 key_data)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_KEY_SCAN,
	};

	memcpy(command.buf, &key_data, 2);

	return  st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_setup_keyscan);

/**
 * st_lpm_set_adv_feature - Set advance feature of SBC
 * @enabled:	If feature needs to enabled
 *		pass 1 for enabling, 0 disabling
 * @feature:	Feature type and its parameters.
 *		Features can be :
 *		SBC VCORE External without parameter.
 *		SBC Low voltage detect with value of voltage.
 *		SBC clock selection(external, AGC or 32K).
 *		SBC RTC source 32K_TCXO or 32K_OSC
 *		SBC Wakeup triggers.
 *
 * This function will enable/disable selected feature on SBC
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_set_adv_feature(u8 enabled, struct st_lpm_adv_feature *feature)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_ADV_FEA,
	};
	int ret;

	if (unlikely(feature == NULL))
		return -EINVAL;

	memcpy(&command.buf[2], feature->params.set_params, 12);
	command.buf[0] = feature->feature_name;
	command.buf[1] = enabled;

	/*
	 * No response expected from  SBC in case it is
	 * set to IDLE mode
	 */
	if (feature->feature_name == ST_LPM_SBC_IDLE)
		ret = st_lpm_ops_exchange_msg(&command, NULL);
	else
		ret = st_lpm_ops_exchange_msg(&command, &response);

	return ret;
}
EXPORT_SYMBOL(st_lpm_set_adv_feature);

/**
 * st_lpm_get_adv_feature - To get current/supported features of SBC
 * @all_features:	If required to get all features.
 *			pass 1 to get all supported features by SBC.
 *			pass 0 to get all current enabled feature of SBC.
 * @features:		structure to get features of SBC
 *
 * Supported or currently  enabled features will be returned in get_params
 * field of features argument.
 * get_params[0-3] are bit map for each feature. Bit map 0-3 is reserved
 * get_params[4-5] are bit map for wakeup triggers.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_get_adv_feature(bool all_features, bool custom_feature,
			struct st_lpm_adv_feature *features)
{
	int err = 0;
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_GET_ADV_FEA,
	};

	if (unlikely(!features))
		return -EINVAL;

	if (all_features == true)
		command.buf[0] = BIT(0);

	if (custom_feature)
		command.buf[0] |= BIT(7);

	err = st_lpm_ops_exchange_msg(&command, &response);
	if (likely(!err))
		memcpy(features->params.get_params, response.buf, 10);

	return err;
}
EXPORT_SYMBOL(st_lpm_get_adv_feature);

/**
 * st_lpm_setup_fp_pio - To setup frontpanel long press GPIO
 * @pio_setting:		PIO data
 * @long_press_delay:		Delay to detect long presss
 * @default_reset_delay:	Default delay to do SOC reset
 *
 * This function will inform SBC about long press GPIO and its delays
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_fp_pio(struct st_lpm_pio_setting *pio_setting,
			u32 long_press_delay, u32 default_reset_delay)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_SET_PIO,
	};

	if (!pio_setting ||
	    (pio_setting->pio_direction && pio_setting->interrupt_enabled) ||
	    pio_setting->pio_use != ST_LPM_PIO_FP_PIO)
		return -EINVAL;

	command.buf[0] = pio_setting->pio_bank;

	pio_setting->pio_pin &= NIBBLE_MASK;
	command.buf[1] = pio_setting->pio_level << PIO_LEVEL_SHIFT |
		pio_setting->interrupt_enabled <<  PIO_IT_SHIFT |
		pio_setting->pio_direction << PIO_DIRECTION_SHIFT |
		pio_setting->pio_pin;

	command.buf[2] = pio_setting->pio_use;

	/*msg[3,4,5] are reserved */
	memcpy(&command.buf[6], &long_press_delay, 4);
	memcpy(&command.buf[10], &default_reset_delay, 4);

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_setup_fp_pio);

/**
 * st_lpm_setup_power_on_delay - To setup delay on wakeup
 * @de_bounce_delay:	this is button de bounce delay.
 * @dc_stability_delay:	this is DC stability delay
 *
 * This function will inform SBC about delays on detecting valid wakeup
 * If this is not called, SBC will use default delay
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_power_on_delay(u16 de_bounce_delay,
				u16 dc_stable_delay)
{
	struct st_lpm_adv_feature feature;
	feature.feature_name = ST_LPM_DE_BOUNCE;

	put_unaligned_le16(de_bounce_delay, feature.params.set_params);
	st_lpm_set_adv_feature(1, &feature);
	feature.feature_name = ST_LPM_DC_STABLE;
	put_unaligned_le16(dc_stable_delay, feature.params.set_params);

	return st_lpm_set_adv_feature(1, &feature);
}
EXPORT_SYMBOL(st_lpm_setup_power_on_delay);

/**
 * st_lpm_setup_rtc_calibration_time -
 *	To setup the RTC calibration time in min
 * @cal_time:	Calibration time in minutes.
 *
 * This function will inform SBC about the RTC calibration time
 * If this is not called, SBC will use default time (5 min)
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_setup_rtc_calibration_time(u8 cal_time)
{
	struct st_lpm_adv_feature feature;
	feature.feature_name = ST_LPM_RTC_CALIBRATION_TIME;

	feature.params.set_params[0] = cal_time;

	return st_lpm_set_adv_feature(1, &feature);
}
EXPORT_SYMBOL(st_lpm_setup_rtc_calibration_time);

/**
 * st_lpm_set_cec_addr - to set CEC address for SBC
 * @addr:	CEC's physical and logical address
 *
 * This function will inform SBC about CEC phy and logical addresses
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_set_cec_addr(struct st_lpm_cec_address *addr)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_CEC_ADDR,
	};

	if (unlikely(!addr))
		return -EINVAL;

	put_unaligned_le16(addr->phy_addr, command.buf);
	put_unaligned_le16(addr->logical_addr, &command.buf[2]);

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_set_cec_addr);

/**
 * st_lpm_cec_config - configure SBC for CEC WU or custom message
 * @use:	WU reason or custom message
 * @params:	Data associated with use
 *
 * This function will inform SBC about CEC WU or custom message
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */
int st_lpm_cec_config(enum st_lpm_cec_select use,
			union st_lpm_cec_params *params)
{
	struct lpm_message response = {0};
	struct lpm_message command = {
		.command_id = LPM_MSG_CEC_PARAMS,
	};

	if (unlikely(NULL == params))
		return -EINVAL;

	command.buf[0] = use;
	if (use == ST_LPM_CONFIG_CEC_WU_REASON)
		command.buf[1] = params->cec_wu_reasons;
	else {
		command.buf[2] = params->cec_msg.size;
		memcpy(&command.buf[3],
			&params->cec_msg.opcode, command.buf[2]);
	}

	return st_lpm_ops_exchange_msg(&command, &response);
}
EXPORT_SYMBOL(st_lpm_cec_config);

int st_lpm_poweroff(void)
{
	struct lpm_message command = {
		.command_id = LPM_MSG_ENTER_PASSIVE,
	};

	return st_lpm_ops_exchange_msg(&command, NULL);
};
EXPORT_SYMBOL(st_lpm_poweroff);

int st_lpm_config_reboot(enum st_lpm_config_reboot_type type)
{
	switch (type) {
	case ST_LPM_REBOOT_WITH_DDR_SELF_REFRESH:
	case ST_LPM_REBOOT_WITH_DDR_OFF:
		return st_lpm_ops_config_reboot(type);
	break;
	default:
		pr_err("%s: configuration NOT supported!\n", __func__);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(st_lpm_config_reboot);

int st_lpm_sbc_ir_enable(bool enable)
{
	if (!st_lpm_ops || !st_lpm_ops->ir_enable)
		return -EINVAL;

	st_lpm_ops->ir_enable(enable, st_lpm_private_data);

	return 0;
}
EXPORT_SYMBOL(st_lpm_sbc_ir_enable);

/**
 * st_lpm_register_callback - to register user callback
 *
 * This function will configure user's callback for long press PIO
 */
int st_lpm_register_callback(enum st_lpm_callback_type type,
	int (*fnc)(void *), void *data)
{
	int ret = 0;
	if (type >= ST_LPM_MAX)
		return -EINVAL;

	mutex_lock(&st_lpm_mutex);

	if (!st_lpm_callbacks[type].fn) {
		st_lpm_callbacks[type].fn = fnc;
		st_lpm_callbacks[type].data = data;
	} else {
		ret = -EBUSY;
	}
	mutex_unlock(&st_lpm_mutex);

	return ret;
}
EXPORT_SYMBOL(st_lpm_register_callback);

int st_lpm_notify(enum st_lpm_callback_type type)
{
	struct st_lpm_callback *cb;

	if (type >= ST_LPM_MAX)
		return -EINVAL;

	cb = &st_lpm_callbacks[type];
	if (cb->fn)
		return cb->fn(cb->data);

	return 0;
}
EXPORT_SYMBOL(st_lpm_notify);

int st_lpm_reload_fw_prepare(void)
{
	if (!st_lpm_ops || !st_lpm_ops->reload_fw_prepare)
		return -EINVAL;

	st_lpm_ops->reload_fw_prepare(st_lpm_private_data);

	return 0;
}
EXPORT_SYMBOL(st_lpm_reload_fw_prepare);

int st_start_loaded_fw(void)
{
	if (!st_lpm_ops || !st_lpm_ops->start_loaded_fw)
		return -EINVAL;

	st_lpm_ops->start_loaded_fw(st_lpm_private_data);

	return 0;
}
EXPORT_SYMBOL(st_start_loaded_fw);

