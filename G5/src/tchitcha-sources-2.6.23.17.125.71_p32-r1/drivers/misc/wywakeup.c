#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/reboot.h>
#include <linux/wywakeup.h>

#define write32(addr, data32) (*(volatile unsigned int*)(addr)=(int)data32)
#define read32(addr) (*(volatile unsigned int*)(addr))
#define ST7105_PTI_BASE_ADDRESS 0xFE230000
#define PTI_OFFSET 0x9984
#define BOOT_TIME_ADJUSTMENT 30

extern unsigned int wokenup_by; /* from sh4/suspend.c */
extern struct timezone sys_tz; /* from kernel/time.c */

struct timespec wywakeup_wakeup_time = {0, 0};
unsigned long wywakeup_maxval = 0;
unsigned long wywakeup_wakeup = 0;
unsigned long wywakeup_suspend_mode = 0;


int wywakeup_get_wakeup_time(struct timespec *ts)
{
	if (ts == NULL || !wywakeup_wakeup)
		return -EAGAIN;

	ts->tv_sec = wywakeup_wakeup_time.tv_sec;
	ts->tv_nsec = wywakeup_wakeup_time.tv_nsec;

	return 0;
}
EXPORT_SYMBOL(wywakeup_get_wakeup_time);


void wywakeup_backup_to_pti_ram(void)
{
	struct timespec now;

	/* write magic word first
	 * offset starts at 8 because data is already written in the two previous
	 * bytes in suspend.c right before rebooting
	 */
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 8, ST7105_PTI_BASE_ADDRESS);

	/* write the suspend flag */
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 12, wywakeup_suspend_mode);
	printk("wywakeup: suspend flag written to pti ram = %ld\n", wywakeup_suspend_mode);

        if (!wywakeup_suspend_mode)
        	return;

	/* write time */
	getnstimeofday(&now);
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 16, now.tv_sec);
	printk("wywakeup: time written to pti ram = %ld\n", now.tv_sec);

	/* write timezone */
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 20, sys_tz.tz_minuteswest);
	printk("wywakeup: timezone written to pti ram = %d\n", sys_tz.tz_minuteswest);

	/* write wakeup wakeup time */
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 24, wywakeup_wakeup_time.tv_sec);
	write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 28, wywakeup_wakeup_time.tv_nsec);
	printk("wywakeup: wakeup time written to pti ram = %ld\n", wywakeup_wakeup_time.tv_sec);
}

void wywakeup_restore_from_pti_ram(void)
{
	int i;
	unsigned int magic_word;
	unsigned long time;
	int timezone;
	struct timespec ts;
	struct timezone tz;

	/* check magicword before reading value
	 * wakeup reason is written in suspend.c right before rebooting
	 */
	magic_word = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET);
	if (magic_word == ST7105_PTI_BASE_ADDRESS)
	{
		/* read wakeup source and set the global variable wokenup-by */
		wokenup_by = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 4);
		printk("wywakeup: wakeup source read from pti ram = %x\n", wokenup_by);
	}

	/* check magicword before reading value
	 * we check another magicword here because these values are written
	 * at a different time than the previous one
	 */
	magic_word = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 8);
	if (magic_word != ST7105_PTI_BASE_ADDRESS)
		goto exit;

	/* read the suspend flag */
	wywakeup_suspend_mode = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 12);
	printk("wywakeup: suspend flag read from pti ram = %ld\n", wywakeup_suspend_mode);
	if (!wywakeup_suspend_mode)
		goto exit;

        /* read time and set current system time */
	time = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 16);
	ts.tv_sec = time + BOOT_TIME_ADJUSTMENT;
	ts.tv_nsec = 0;
	printk("wywakeup: time read from pti ram = %ld\n", time);

	/* read timezone and set system timezone delay */
	timezone = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 20);
	tz.tz_minuteswest = timezone;
	printk("wywakeup: timezone read from pti ram = %d\n", timezone);

	/* set current time and timezone */
	do_sys_settimeofday(&ts, &tz);

	/* read wakeup time */
	wywakeup_wakeup_time.tv_sec = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 24);
	wywakeup_wakeup_time.tv_nsec = read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + 28);
	printk("wywakeup: wakeup time read from pti ram = %ld\n", wywakeup_wakeup_time.tv_sec);

exit:
	/* clean values */
	for (i = 0; i <= 7; i++)
		write32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + i*4, 0);
}

static ssize_t wywakeup_set_timeout(struct class *cls,const char *buf,size_t count)
{
        struct timespec now;
	unsigned long timeout;

        timeout = simple_strtoul(buf, NULL, 10);

	getnstimeofday(&now);
	wywakeup_wakeup_time.tv_sec = now.tv_sec + timeout;
	wywakeup_wakeup_time.tv_nsec = now.tv_nsec;

	return count;
}

static ssize_t wywakeup_get_timeout(struct class *cls,char *buf)
{
	int ret;
	struct timespec now;
	unsigned long timeout = 0;

	getnstimeofday(&now);
	if (now.tv_sec < wywakeup_wakeup_time.tv_sec)
                timeout = wywakeup_wakeup_time.tv_sec - now.tv_sec;
	ret = sprintf(buf, "%lu", timeout);
	return ret;
}

static ssize_t wywakeup_get_maxval(struct class *cls,char *buf)
{
	int ret;
	ret = sprintf(buf, "%lu", wywakeup_maxval);
	return ret;
}

static ssize_t wywakeup_set_wakeup(struct class *cls,const char *buf,size_t count)
{
	if(!strncmp(buf,"enabled",strlen("enabled")))
	{
		wywakeup_wakeup = 1;
	}
	else
	{
		wywakeup_wakeup = 0;
	}

	return count;

}

static ssize_t wywakeup_get_wakeup(struct class *cls,char *buf)
{
	int ret;
	ret = sprintf(buf, "%s", (wywakeup_wakeup ? "enabled" : "disabled"));
	return ret;

}

static ssize_t wywakeup_set_suspend_mode(struct class *cls, const char *buf, size_t count)
{
	if (!strncmp(buf, "enabled", strlen("enabled")))
	{
		wywakeup_suspend_mode = 1;
	}
	else
	{
		wywakeup_suspend_mode = 0;
	}

	wywakeup_backup_to_pti_ram();

	return count;
}

static ssize_t wywakeup_get_suspend_mode(struct class *cls, char *buf)
{
	int ret;
	ret = sprintf(buf, "%s", (wywakeup_suspend_mode ? "enabled" : "disabled"));
	return ret;
}

static struct class_attribute wywakeup_class_attrs[] = {
	__ATTR(timeout, S_IRUGO | S_IWUSR, wywakeup_get_timeout, wywakeup_set_timeout),
	__ATTR(maxval, S_IRUGO | S_IWUSR, wywakeup_get_maxval, NULL),
	__ATTR(wakeup, S_IRUGO | S_IWUSR, wywakeup_get_wakeup, wywakeup_set_wakeup),
	__ATTR(suspend, S_IRUGO | S_IWUSR, wywakeup_get_suspend_mode, wywakeup_set_suspend_mode),
	__ATTR_NULL
};


static struct class wywakeup_class = {
	.name = "wywakeup",
	.owner = THIS_MODULE,
	.class_attrs = wywakeup_class_attrs,
	.suspend = NULL,
	.resume = NULL,
};

static struct device wywakeup_device = {
	.bus_id = "wywakeup_bus",
	.class = &wywakeup_class,
};

static int __init wywakeup_init(void)
{
	int ret;

	printk("wywakeup_init\n");
	wywakeup_maxval = 23456248; /* (2^40)/46875; */
	printk("wywakeup: register class\n");
	ret = class_register(&wywakeup_class);
	if(ret < 0)
	{
		printk(KERN_ERR
				"%s: can't register class err: 0x%d\n",
				__FUNCTION__, ret);
		goto exit;

	}
	ret = device_register(&wywakeup_device);
	if(ret < 0)
	{
		printk(KERN_ERR
				"%s: can't register device err: 0x%d\n",
				__FUNCTION__, ret);
		goto device_error;

	}

	wywakeup_restore_from_pti_ram();

	return 0;

device_error:
	class_unregister(&wywakeup_class);
exit:
	return -EINVAL;
}

static void __exit wywakeup_exit(void)
{
	device_unregister(&wywakeup_device);
	class_unregister(&wywakeup_class);
}

module_init(wywakeup_init);
module_exit(wywakeup_exit);


MODULE_DESCRIPTION("get and set standby timeout ");
MODULE_AUTHOR("Nicolas FAURE <nfaure@wyplay.com>");
MODULE_LICENSE("GPL");

