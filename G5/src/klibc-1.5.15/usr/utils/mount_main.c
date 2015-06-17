/*
 * by rmk
 */
#include <sys/mount.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mount_opts.h"

char *progname;

static struct extra_opts extra;
static unsigned long rwflag;

static int
do_mount(char *dev, char *dir, char *type, unsigned long rwflag, void *data)
{
	char *s;
	int error = 0;

	while ((s = strsep(&type, ",")) != NULL) {
retry:
		if (mount(dev, dir, s, rwflag, data) == -1) {
			error = errno;
			/*
			 * If the filesystem is not found, or the
			 * superblock is invalid, try the next.
			 */
			if (error == ENODEV || error == EINVAL)
				continue;

			/*
			 * If we get EACCESS, and we're trying to
			 * mount readwrite and this isn't a remount,
			 * try read only.
			 */
			if (error == EACCES &&
			    (rwflag & (MS_REMOUNT | MS_RDONLY)) == 0) {
				rwflag |= MS_RDONLY;
				goto retry;
			}
		} else {
			error = 0;
		}
		break;
	}

	if (error) {
		errno = error;
		perror("mount");
		return 255;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *type = NULL;
	int c;

	progname = argv[0];
	rwflag = MS_VERBOSE;

	do {
		c = getopt(argc, argv, "no:rt:wfi");
		if (c == EOF)
			break;
		switch (c) {
		case 'n':
			/* no mtab writing */
			break;
		case 'o':
			rwflag = parse_mount_options(optarg, rwflag, &extra);
			break;
		case 'r':
			rwflag |= MS_RDONLY;
			break;
		case 't':
			type = optarg;
			break;
		case 'w':
			rwflag &= ~MS_RDONLY;
			break;
		case 'f':
			/* we can't edit /etc/mtab yet anyway; exit */
			exit(0);
		case 'i':
			/* ignore for now; no support for mount helpers */
			break;
		case '?':
			fprintf(stderr, "%s: invalid option -%c\n",
				progname, optopt);
			exit(1);
		}
	} while (1);

	/*
	 * If remount, bind or move was specified, then we don't
	 * have a "type" as such.  Use the dummy "none" type.
	 */
	if (rwflag & MS_TYPE)
		type = "none";

	if (optind + 2 != argc || type == NULL) {
		fprintf(stderr, "Usage: %s [-r] [-w] [-o options] [-t type] [-f] [-i] "
			"[-n] device directory\n", progname);
		exit(1);
	}

	return do_mount(argv[optind], argv[optind + 1], type, rwflag,
			extra.str);
}
