#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

char *progname;

static __noreturn usage(void)
{
	fprintf(stderr, "Usage: %s link\n", progname);
	exit(1);
}

int main(int argc, char *argv[])
{
	char *name, *link_name = NULL;
	size_t max_siz = 128;

	progname = *argv++;

	name = *argv++;
	if (!name)
		usage();

	link_name = malloc(max_siz);
	if (!link_name) {
		perror("malloc");
		exit(1);
	}

	if (readlink(name, link_name, max_siz) == -1) {
		perror("readlink");
		exit(1);
	}
	printf("%s\n", link_name);

	exit(0);
}
