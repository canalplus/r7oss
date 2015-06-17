#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/reboot.h>
#include <klibc/compiler.h>

static __noreturn usage(void)
{
	static char mesg[] = "Usage: {halt|reboot|poweroff} [-n]\n";
	write(2, mesg, sizeof(mesg) - 1);
	exit(1);
}

int main(int argc, char *argv[])
{
	int cmd = 0; /* initalize to shut gcc up */
	int do_sync = 1;
	char *ptr, *ptr2;

	/* Which action (program name)? */
	ptr2 = ptr = argv[0];
	while (*ptr2)
		if (*ptr2++ == '/')
			ptr = ptr2;
	if (*ptr == 'r')
		cmd = LINUX_REBOOT_CMD_RESTART;
	else if (*ptr == 'h')
		cmd = LINUX_REBOOT_CMD_HALT;
	else if (*ptr == 'p')
		cmd = LINUX_REBOOT_CMD_POWER_OFF;
	else
		usage();

	/* Walk options */
	while (*++argv && **argv == '-')
		switch (*++*argv) {
		case 'f':
			break; /* -f assumed */
		case 'n':
			do_sync = 0;
			break;
		default:
			usage();
		}
	if (*argv)
		usage(); /* any args == error */

	if (do_sync)
		sync();
	reboot(LINUX_REBOOT_CMD_CAD_OFF); /* Enable CTRL+ALT+DEL */
	if (!reboot(cmd)) {
		/* Success. Currently, CMD_HALT returns, so stop the world */
		/* kill(-1, SIGSTOP); */
		kill(getpid(), SIGSTOP);
	}
	write(2, "failed.\n", 8);
	return 1;
}
