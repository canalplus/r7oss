#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

const char *progname;

static __noreturn usage(void)
{
	fprintf(stderr, "Usage: %s link...\n", progname);
	exit(1);
}

int main(int argc, char *argv[])
{
	const char *name;
	char link_name[PATH_MAX];
	int rv;
	int i;

	progname = *argv++;

	if (argc < 2)
		usage();

	while ((name = *argv++)) {
		rv = readlink(name, link_name, sizeof link_name - 1);
		if (rv < 0) {
			perror(name);
			exit(1);
		}
		link_name[rv] = '\n';
		_fwrite(link_name, rv+1, stdout);
	}

	return 0;
}
