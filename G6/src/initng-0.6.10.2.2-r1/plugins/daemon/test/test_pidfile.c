#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define PIDFILE "/run/test.pid"

static void set_pid_file(void)
{
	FILE *fd;

	fd = fopen(PIDFILE, "w+");
	if (!fd)
	{
		printf("Could not open " PIDFILE " for writing!\n");
		return;
	}
	fprintf(fd, "%i", getpid());
	fclose(fd);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	printf("This is a testing daemon pid %i, that will fork and leave pid in "
		   PIDFILE ". \n", getpid());


	if (fork() == 0)
	{
		printf("This is fork, pid %i\n", getpid());
		printf("Setting pid now!\n");
		set_pid_file();
		sleep(20);
		printf("Child will now die!\n");
		unlink(PIDFILE);
		_exit(1);
	}

	sleep(1);
	printf("Fork launched!\n");
	return (0);
}
