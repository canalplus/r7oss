/*
 * FS probe
 *
 * Part of LTTng
 *
 * Mathieu Desnoyers, March 2007
 *
 * Licensed under the GPLv2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/marker.h>
#include <linux/ltt-tracer.h>

#include <asm/uaccess.h>

/*
 * Expects va args : (int elem_num, const char __user *s)
 * Element size is implicit (sizeof(char)).
 */
static char *ltt_serialize_fs_data(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	int elem_size;
	int elem_num;
	const char __user  *s;
	unsigned long noncopy;

	elem_num = va_arg(*args, int);
	s = va_arg(*args, const char __user *);
	elem_size = sizeof(*s);

	if (align)
		str += ltt_align((long)str, sizeof(int));
	if (buffer)
		*(int*)str = elem_num;
	str += sizeof(int);

	if (elem_num > 0) {
		/* No alignment required for char */
		if (buffer) {
			noncopy = __copy_from_user_inatomic(str, s,
					elem_num*elem_size);
			memset(str+(elem_num*elem_size)-noncopy, 0, noncopy);
		}
		str += (elem_num*elem_size);
	}
	/* Following alignment for genevent compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}

static struct ltt_available_probe probe_array[] =
{
	{ "fs_read_data", "%u %zd %k",
		.probe_func = ltt_trace,
		.callbacks[0] = ltt_serialize_data,
		.callbacks[1] = ltt_serialize_fs_data },
	{ "fs_write_data", "%u %zd %k",
		.probe_func = ltt_trace,
		.callbacks[0] = ltt_serialize_data,
		.callbacks[1] = ltt_serialize_fs_data },
};

static int __init probe_init(void)
{
	int result, i;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++) {
		result = ltt_probe_register(&probe_array[i]);
		if (result)
			printk(KERN_INFO "LTT unable to register probe %s\n",
				probe_array[i].name);
	}
	return 0;
}

static void __exit probe_fini(void)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++) {
		err = ltt_probe_unregister(&probe_array[i]);
		BUG_ON(err);
	}
}

module_init(probe_init);
module_exit(probe_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("FS probe");
