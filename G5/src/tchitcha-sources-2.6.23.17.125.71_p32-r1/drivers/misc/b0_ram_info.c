
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>


#define B0_HEADER_OFFSET	0x1FFC0
#define IMAGE_ID_OFFSET		0x14


static unsigned int image_id;


static ssize_t image_id_show(struct class *cls, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%08X\n", image_id);
}


static struct class_attribute b0_class_attrs[] = {
	__ATTR(image_id, 0444, image_id_show, NULL),
	__ATTR_NULL
};


static struct class b0_class = {
	.name = "b0",
	.owner = THIS_MODULE,
	.class_attrs = b0_class_attrs
};


static int __init image_id_init(void)
{
	image_id = *(unsigned int *)(CONFIG_PAGE_OFFSET +
			CONFIG_BOOT_LINK_OFFSET + B0_HEADER_OFFSET +
			IMAGE_ID_OFFSET);

	class_register(&b0_class);

	return 0;
}


static void __exit image_id_exit(void)
{
	class_unregister(&b0_class);
}


module_init(image_id_init);
module_exit(image_id_exit);


MODULE_DESCRIPTION("get B0 information from the RAM");
MODULE_AUTHOR("Hervé Boisse <hboisse@wyplay.com>");
MODULE_LICENSE("GPL");

