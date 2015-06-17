/*
 *
 *
 *
 *  Copyright (C) 2011 Wyplay
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "tdaemon.h"

#include "callback.hpp"
#include "test/test_object.h"

//#include "hal/hal_computer_proxy.h"

#include <dbus-c++/dbus.h>
#include <libwy/wylog.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <unistd.h>
#include <stdio.h>

#define LAUNCH_TCH_RESET_FILE "/etc/params/launch-tch-reset"

WyThreadedDispatcher global_dispatcher;

bool g_signalloop = true;

/**
 *
 */
void sig_hdler(int sig)
{
    wl_info("Caught signal %d", sig);
    switch(sig)
    {
	case SIGINT:
	    break;
	case SIGKILL:
	    break;
	case SIGSEGV:
	    break;
	default:
	    wl_debug("Unhandled signal %d", sig);
	    break;
    }
    //DBusMainLoop::instance(WYTCHMAIN_BUS_TYPE, WYTCHMAIN_BUS_NAME)->stop();
    global_dispatcher.dispatcher().leave();
    wl_info("DBUS loop stopped");

    g_signalloop = false;
}

/**
 *
 */
int main(int argc, char ** argv)
{
	wylog_init();
	{
		char args[100];
		int p=0;
		for(int i=0 ; i<argc && p<100 ; i++) {
			int ret = snprintf(args+p,100-p, "%s ", argv[i]);
			if (0<=ret) {
				p += ret;
			}
		}
		wl_info("START:%s", args);

	}


	unsigned int flagValue = 0;
	{
		int opt = -1;
		while ((opt = getopt(argc, argv, "hrwo:v:DIBFbf")) != -1 ) {
			switch(opt) {
			case 'h':
				printf("Options:\n");
				printf("\t-h                   : Help\n");
				printf("\t-r -o flag           : gets the value of TCH 'flag'; flag can be 0..3\n");
				printf("\t-w -o flag -v value  : sets the value of TCH 'flag'\n");
				printf("\t-D                   : run as a daemon\n");
				printf("\t-I                   : call TCH_SYS_Init\n");
				printf("\t-B                   : STB reboot\n");
				printf("\t-F                   : STB factory reset\n");
				printf("\t-b                   : eCM reboot\n");
				printf("\t-f                   : eCM factory reset\n");
				return 0;
				break;
			case 'r':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_GETFLAG:CMD_MODE_NONE;
				break;
			case 'w':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_SETFLAG:CMD_MODE_NONE;
				break;
			case 'o':
			{
				break;
			}
			case 'v':
			{
				break;
			}
			case 'D':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_DAEMON:CMD_MODE_NONE;
				break;
			case 'B':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_STB_REBOOT:CMD_MODE_NONE;
				break;
			case 'F':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_STB_FACTORYRESET:CMD_MODE_NONE;
				break;
			case 'b':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_ECM_REBOOT:CMD_MODE_NONE;
				break;
			case 'f':
				// mode = (CMD_MODE_NONE==mode)?CMD_MODE_ECM_FACTORYRESET:CMD_MODE_NONE;
				break;
			case '?':
				//printf("Command line error: invalid argument %s !!!\n", "");
				exit(-1);
				break;
			case ':':
				//printf("Command line error: -%s requires a value !!!\n", "");
				exit(-1);
				break;
			default:
				break;
			}
		}

	}

	signal(SIGINT, sig_hdler);

	global_dispatcher.start();

	DBus::default_dispatcher = &global_dispatcher.dispatcher();
	DBus::Connection mainConnection(DBus::Connection::SystemBus(&global_dispatcher.dispatcher()));
	mainConnection.request_name("com.wyplay.tdaemon");

	// Init the hosted DBus object
	test_object wytch_dbus_object(mainConnection, "/com/wyplay/tdaemon/test");
	test_object wytch_dbus_object1(mainConnection, "/com/wyplay/tdaemon/test/1");
	test_object wytch_dbus_object2(mainConnection, "/com/wyplay/tdaemon/test/1/2/3");
	test_object wytch_dbus_object3(mainConnection, "/com/wyplay/tdaemon/test/3/6/7");
	test_object wytch_dbus_object4(mainConnection, "/com/wyplay/tdaemon/test/alttest");
	test_object wytch_dbus_object5(mainConnection, "/com/wyplay/tdaemon/test/alttest/second");
	test_object wytch_dbus_object6(mainConnection, "/com/wyplay/tdaemon/test/first/second");
	test_object wytch_dbus_object7(mainConnection, "/com/wyplay/tdaemon/test/first/second/last");

	test_object wytch_dbus_object8(mainConnection, "/com/wyplay/tdaemon/test/multiproxy");


    int fd_test = open("/tmp/fd.test", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    //fd_test = -1;
    //close(fd_test);
	try {
		// Init the dbus hal object proxy
		//hal_computer_proxy hal(mainConnection, "/org/freedesktop/Hal/devices/computer", "org.freedesktop.Hal");

        int i=0;
        while (g_signalloop) {
            //wl_info("Sending some signals");
            wytch_dbus_object.ECMStatus(i, ::DBus::UnixFD(fd_test));
            wytch_dbus_object.ECMLinkStatus(i);

            wytch_dbus_object1.ECMStatus(1000-i, ::DBus::UnixFD(fd_test));
            wytch_dbus_object1.ECMLinkStatus(1000-i);
            i++;
            sleep(1);
        }
	}
	catch(DBus::Error & e) {
		wl_error("DBus exception while creating the proxy to /org/freedesktop/Hal/devices/computer");
	}

	wl_info("Main thread joining the dispatch thread");
	global_dispatcher.join();

    close(fd_test);

	wl_info("Main thread exit");

    return 0;
}
