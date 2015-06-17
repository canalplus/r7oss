/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of common V4L2-MediaController registration functions
************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "stmedia.h"

struct stm_media {
	struct v4l2_device v4l2_dev;	/* V4L2 device */
	struct media_device media_dev;	/* Media controller device */
	struct device *dev;
	struct platform_device *pdev;	/* Platform device pointer */
};
static struct stm_media *g_stm_media;

struct platform_driver stm_platform_driver = {
	.driver = {
		   .name = "stm_media",
		   .owner = THIS_MODULE}
};

/*
 * This function is in charge of initializing the platform/v4l2/MC drivers
 */
static int __init stm_media_init(void)
{
	int ret;
	struct media_device *mdev;

	/* Allocate the stm_media structure */
	g_stm_media = kzalloc(sizeof(struct stm_media), GFP_KERNEL);
	if (!g_stm_media) {
		printk(KERN_ERR "%s: failed create stm_media\n", __func__);
		ret = -ENOMEM;
		goto failed_alloc_stm_media;
	}

	/* Initialize the platform device pointer */
	ret = platform_driver_register(&stm_platform_driver);
	if (ret) {
		printk(KERN_ERR "%s: platform driver register failed\n",
		       __func__);
		goto failed_platform_driver_register;
	}

	g_stm_media->pdev =
	    platform_device_register_simple("stm_media", -1, NULL, 0);
	if (IS_ERR(g_stm_media->pdev)) {
		printk(KERN_ERR "%s: register platform failed\n", __func__);
		ret = -EIO;
		goto failed_platform_device_register;
	}
	g_stm_media->dev = &g_stm_media->pdev->dev;
	platform_set_drvdata(g_stm_media->pdev, g_stm_media);

	/* Initialize the media controller device */
	mdev = &g_stm_media->media_dev;
	mdev->dev = g_stm_media->dev;
	strlcpy(mdev->model, "STM Media Controller", sizeof(mdev->model));

	ret = media_device_register(mdev);
	if (ret < 0) {
		printk(KERN_ERR "%s: media device registration failed (%d)\n",
		       __func__, ret);
		goto failed_media_device_register;
	}
	g_stm_media->v4l2_dev.mdev = mdev;

	/* Initialize the V4L2 device */
	ret = v4l2_device_register(g_stm_media->dev, &g_stm_media->v4l2_dev);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed register V4L2 dev (%d)\n", __func__,
		       ret);
		goto failed_v4l2_device_register;
	}

	return 0;

failed_v4l2_device_register:
	media_device_unregister(mdev);
failed_media_device_register:
	platform_device_unregister(g_stm_media->pdev);
failed_platform_device_register:
	platform_driver_unregister(&stm_platform_driver);
failed_platform_driver_register:
	kfree(g_stm_media);
	g_stm_media = NULL;
failed_alloc_stm_media:
	return ret;
}

module_init(stm_media_init);

/*
 * Perform the cleanup of the stm_media layer
 */
static void __exit stm_media_cleanup(void)
{
	if (!g_stm_media)
		return;		/* Nothing to do */

	v4l2_device_unregister(&g_stm_media->v4l2_dev);
	media_device_unregister(&g_stm_media->media_dev);
	platform_device_unregister(g_stm_media->pdev);
	platform_driver_unregister(&stm_platform_driver);
	kfree(g_stm_media);
	g_stm_media = NULL;
}

module_exit(stm_media_cleanup);

/*
 * Register a V4L2 subdevice to the stm_media layer V4L2/MC device
 */
int stm_media_register_v4l2_subdev(struct v4l2_subdev *sd)
{
	if (!g_stm_media)
		return -EINVAL;

	return v4l2_device_register_subdev(&g_stm_media->v4l2_dev, sd);
}
EXPORT_SYMBOL(stm_media_register_v4l2_subdev);

/**
 * stm_media_register_v4l2_subdev_nodes()
 * Create subdev nodes for all the subdevices registered with the
 * parent v4l2 device
 */
int stm_media_register_v4l2_subdev_nodes(void)
{
	if (!g_stm_media)
		return -EINVAL;

	return v4l2_device_register_subdev_nodes(&g_stm_media->v4l2_dev);
}
EXPORT_SYMBOL(stm_media_register_v4l2_subdev_nodes);

/*
 * Unregister a V4L2 subdevice attached to the stm_media layer
 * (actually not really needed here since it is simple v4l2_device call but
 * kept to be consistant with the register function)
 */
void stm_media_unregister_v4l2_subdev(struct v4l2_subdev *sd)
{
	v4l2_device_unregister_subdev(sd);
}
EXPORT_SYMBOL(stm_media_unregister_v4l2_subdev);

/*
 * Register a V4L2 video device to the stm_media layer V4L2/MC device
 */
int stm_media_register_v4l2_video_device(struct video_device *viddev, int type,
					 int nr)
{
	if (!g_stm_media)
		return -EINVAL;

	viddev->v4l2_dev = &g_stm_media->v4l2_dev;
	return video_register_device(viddev, type, nr);
}
EXPORT_SYMBOL(stm_media_register_v4l2_video_device);

/*
 * Unregister a V4L2 video device previously registered
 */
void stm_media_unregister_v4l2_video_device(struct video_device *viddev)
{
	video_unregister_device(viddev);
}
EXPORT_SYMBOL(stm_media_unregister_v4l2_video_device);

/*
 * Register a media entity to the stm_media layer V4L2/MC device
 */
int stm_media_register_entity(struct media_entity *entity)
{
	if (!g_stm_media)
		return -EINVAL;

	return media_device_register_entity(&g_stm_media->media_dev, entity);
}
EXPORT_SYMBOL(stm_media_register_entity);

/*
 * Unregister a media entity previously registered
 */
void stm_media_unregister_entity(struct media_entity *entity)
{
	media_device_unregister_entity(entity);
}
EXPORT_SYMBOL(stm_media_unregister_entity);

/*
 * Search for an entity of a specific type within the media device
 * stm_media_find_entity_with_type_first is called first
 * stm_media_find_entity_with_type_next then follow until it returns NULL
 */
struct media_entity *stm_media_find_entity_with_type_first(unsigned int type)
{
	struct media_entity *entity;
	unsigned int found = 0;

	if (!g_stm_media)
		return NULL;

	list_for_each_entry(entity, &g_stm_media->media_dev.entities, list) {
		if (entity->type == type) {
			found++;
			break;
		}
	}

	if (found)
		return entity;

	return NULL;
}
EXPORT_SYMBOL(stm_media_find_entity_with_type_first);

struct media_entity *stm_media_find_entity_with_type_next(struct media_entity
							  *cur,
							  unsigned int type)
{
	struct media_entity *entity;
	struct list_head *next;
	unsigned int found = 0;

	if (!cur || !g_stm_media)
		return NULL;

	list_for_each(next, &cur->list) {
		if (next == &g_stm_media->media_dev.entities)
			break;
		entity = list_entry(next, struct media_entity, list);
		if (entity->type == type) {
			found++;
			break;
		}
	}

	if (found)
		return entity;

	return NULL;
}
EXPORT_SYMBOL(stm_media_find_entity_with_type_next);

/*
 * stm_media_entity_find_remote_pad_with_type()
 * @pad        : The source pad whose remote to be discovered
 * @state      : MEDIA_LNK_*
 * @entity_type: remote pad entity type
 * @id         : from where to start searching
 * Finds out the remote pad of a paricular type connected with
 * the local media pad with a particular status.
 */
struct media_pad *stm_media_find_remote_pad_with_type
				(const struct media_pad *pad,
				unsigned int state,
				unsigned long entity_type,
				unsigned int *id)
{
	unsigned int i;

	for (i = *id; i < pad->entity->num_links; i++) {
		struct media_link *link = &pad->entity->links[i];

		if (state == MEDIA_LNK_FL_ENABLED) {
			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				continue;
		}

		*id = i + 1;

		if ((link->source == pad) &&
				(link->sink->entity->type == entity_type))
			return link->sink;

		if ((link->sink == pad) &&
				(link->source->entity->type == entity_type))
			return link->source;
	}

	return NULL;
}
EXPORT_SYMBOL(stm_media_find_remote_pad_with_type);

/*
 * Very similar to media_entity_remote_source except that it allows
 * to offset the search and then not only get the first enabled link
 */
struct media_pad *stm_media_find_remote_pad(const struct media_pad *pad,
					    unsigned int state,
					    unsigned int *id)
{
	unsigned int i;

	for (i = *id; i < pad->entity->num_links; i++) {
		struct media_link *link = &pad->entity->links[i];

		if (state == MEDIA_LNK_FL_ENABLED) {
			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				continue;
		}

		*id = i + 1;

		if (link->source == pad)
			return link->sink;

		if (link->sink == pad)
			return link->source;
	}

	return NULL;
}
EXPORT_SYMBOL(stm_media_find_remote_pad);

/*
 * Retrieve the remote v4l2_subdev of a specific pad
 */
struct v4l2_subdev *stm_media_remote_v4l2_subdev(const struct media_pad *local)
{
	struct media_pad *remote;
	unsigned int id = 0;

	if (!local)
		return NULL;

	remote = stm_media_find_remote_pad(local, MEDIA_LNK_FL_ENABLED, &id);

	if (remote == NULL)
		return NULL;

	return media_entity_to_v4l2_subdev(remote->entity);
}
EXPORT_SYMBOL(stm_media_remote_v4l2_subdev);

MODULE_DESCRIPTION("STLinuxTV stmedia driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
