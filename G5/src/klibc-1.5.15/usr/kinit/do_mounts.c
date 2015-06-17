#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <inttypes.h>

#include "do_mounts.h"
#include "kinit.h"
#include "fstype.h"
#include "zlib.h"

/* Create the device node "name" */
int create_dev(const char *name, dev_t dev)
{
	unlink(name);
	return mknod(name, S_IFBLK | 0600, dev);
}

/* mount a filesystem, possibly trying a set of different types */
const char *mount_block(const char *source, const char *target,
			const char *type, unsigned long flags,
			const void *data)
{
	char *fslist, *p, *ep;
	const char *rp;
	ssize_t fsbytes;
	int fd;

	if (type) {
		DEBUG(("kinit: trying to mount %s on %s with type %s\n",
		       source, target, type));
		int rv = mount(source, target, type, flags, data);
		/* Mount readonly if necessary */
		if (rv == -1 && errno == EACCES && !(flags & MS_RDONLY))
			rv = mount(source, target, type, flags | MS_RDONLY,
				   data);
		return rv ? NULL : type;
	}

	/* If no type given, try to identify the type first; this
	   also takes care of specific ordering requirements, like
	   ext3 before ext2... */
	fd = open(source, O_RDONLY);
	if (fd >= 0) {
		int err = identify_fs(fd, &type, NULL, 0);
		close(fd);

		if (!err && type) {
			DEBUG(("kinit: %s appears to be a %s filesystem\n",
			       source, type));
			type = mount_block(source, target, type, flags, data);
			if (type)
				return type;
		}
	}

	DEBUG(("kinit: failed to identify filesystem %s, trying all\n",
	       source));

	fsbytes = readfile("/proc/filesystems", &fslist);

	errno = EINVAL;
	if (fsbytes < 0)
		return NULL;

	p = fslist;
	ep = fslist + fsbytes;

	rp = NULL;

	while (p < ep) {
		type = p;
		p = strchr(p, '\n');
		if (!p)
			break;
		*p++ = '\0';
		if (*type != '\t')	/* We can't mount a block device as a "nodev" fs */
			continue;

		type++;
		rp = mount_block(source, target, type, flags, data);
		if (rp)
			break;
		if (errno != EINVAL)
			break;
	}

	free(fslist);
	return rp;
}

/* mount the root filesystem from a block device */
static int
mount_block_root(int argc, char *argv[], dev_t root_dev,
		 const char *type, unsigned long flags)
{
	const char *data, *rp;

	data = get_arg(argc, argv, "rootflags=");
	create_dev("/dev/root", root_dev);

	errno = 0;

	if (type) {
		if ((rp = mount_block("/dev/root", "/root", type, flags, data)))
			goto ok;
		if (errno != EINVAL)
			goto bad;
	}

	if (!errno
	    && (rp = mount_block("/dev/root", "/root", NULL, flags, data)))
		goto ok;

      bad:
	if (errno != EINVAL) {
		/*
		 * Allow the user to distinguish between failed open
		 * and bad superblock on root device.
		 */
		fprintf(stderr, "%s: Cannot open root device %s\n",
			progname, bdevname(root_dev));
		return -errno;
	} else {
		fprintf(stderr, "%s: Unable to mount root fs on device %s\n",
			progname, bdevname(root_dev));
		return -ESRCH;
	}

      ok:
	printf("%s: Mounted root (%s filesystem)%s.\n",
	       progname, rp, (flags & MS_RDONLY) ? " readonly" : "");
	return 0;
}

int
mount_root(int argc, char *argv[], dev_t root_dev, const char *root_dev_name)
{
	unsigned long flags = MS_RDONLY | MS_VERBOSE;
	int ret;
	const char *type = get_arg(argc, argv, "rootfstype=");

	if (get_flag(argc, argv, "rw") > get_flag(argc, argv, "ro")) {
		DEBUG(("kinit: mounting root rw\n"));
		flags &= ~MS_RDONLY;
	}

	if (type) {
		if (!strcmp(type, "nfs"))
			root_dev = Root_NFS;
		else if (!strcmp(type, "jffs2") && !major(root_dev))
			root_dev = Root_MTD;
	}

	switch (root_dev) {
	case Root_NFS:
		ret = mount_nfs_root(argc, argv, flags);
		break;
	case Root_MTD:
		ret = mount_mtd_root(argc, argv, root_dev_name, type, flags);
		break;
	default:
		ret = mount_block_root(argc, argv, root_dev, type, flags);
		break;
	}

	if (!ret)
		chdir("/root");

	return ret;
}

int do_mounts(int argc, char *argv[])
{
	const char *root_dev_name = get_arg(argc, argv, "root=");
	const char *root_delay = get_arg(argc, argv, "rootdelay=");
	const char *load_ramdisk = get_arg(argc, argv, "load_ramdisk=");
	dev_t root_dev = 0;

	DEBUG(("kinit: do_mounts\n"));

	if (root_delay) {
		int delay = atoi(root_delay);
		fprintf(stderr, "Waiting %d s before mounting root device...\n",
			delay);
		sleep(delay);
	}

	md_run(argc, argv);

	if (root_dev_name) {
		root_dev = name_to_dev_t(root_dev_name);
	} else if (get_arg(argc, argv, "nfsroot=") ||
		   get_arg(argc, argv, "nfsaddrs=")) {
		root_dev = Root_NFS;
	} else {
		long rootdev;
		getintfile("/proc/sys/kernel/real-root-dev", &rootdev);
		root_dev = (dev_t) rootdev;
	}

	DEBUG(("kinit: root_dev = %s\n", bdevname(root_dev)));

	if (initrd_load(argc, argv, root_dev)) {
		DEBUG(("initrd loaded\n"));
		return 0;
	}

	if (load_ramdisk && atoi(load_ramdisk)) {
		if (ramdisk_load(argc, argv, root_dev))
			root_dev = Root_RAM0;
	}

	return mount_root(argc, argv, root_dev, root_dev_name);
}
