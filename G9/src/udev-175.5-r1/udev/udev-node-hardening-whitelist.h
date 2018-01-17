/*************************************************
*   This file is overriden with a load_config.   *
*   Read this only as an example.                *
**************************************************/

#ifndef _UDEV_NODE_HARDENING_WHITELIST_
#define _UDEV_NODE_HARDENING_WHITELIST_

#include "udev-hardening-utils.h"

#ifndef END_NODE_HARDENING_WHITELIST_ELEMENT
# define END_NODE_HARDENING_WHITELIST_ELEMENT { NULL, 0000, 0, 0, 0, 0 }
#endif /* END_NODE_HARDENING_WHITELIST_ELEMENT */


struct node_hardening_whitelist_t {
	const char  *path;
	mode_t mode;
	int    major;
	int    minor;
	uid_t  uid;
	gid_t  gid;
};

/* is the device allowed or not
 *
 * return: 0 = not allowed
 * otherwise = allowed
 */
static int __hardening_is_allowed_device(const char *path, const int major, const int minor, const mode_t mode, const uid_t uid, const gid_t gid);

static const struct node_hardening_whitelist_t __hardening_node_whitelist[] = {
// 	{ "/dev/input/event0", S_IRUSR|S_IWUSR|S_IRGRP, 13, 64, 0, 0 },
	END_NODE_HARDENING_WHITELIST_ELEMENT
};

static int __hardening_is_allowed_device(const char *path, const int major, const int minor, const mode_t mode, const uid_t uid, const gid_t gid)
{
	int i;

	for(i = 0; __hardening_node_whitelist[i].path != NULL; ++i){
		const struct node_hardening_whitelist_t * const wl = __hardening_node_whitelist + i;
		if(major == wl->major && minor == wl->minor && mode == wl->mode && uid == wl->uid && gid == wl->gid && MATCH(path, wl->path)){
			return 1;
		}
	}
	return 0;
}

#endif /* _UDEV_NODE_HARDENING_WHITELIST_ */
