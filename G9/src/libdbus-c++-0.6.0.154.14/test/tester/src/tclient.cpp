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
#include <libwy/wylog.h>

#include "tdaemon.h"

#include "callback.hpp"
#include "test/test_proxy.h"

//#include "hal/hal_computer_proxy.h"

#include <dbus-c++/dbus.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <stdio.h>
#include <unistd.h>

#define LAUNCH_TCH_RESET_FILE "/etc/params/launch-tch-reset"

WyThreadedDispatcher global_dispatcher;

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
	mainConnection.request_name("com.wyplay.tclient");

    try {
        test_proxy asyncclient(mainConnection, "/com/wyplay/tdaemon/test/1/2/3", "com.wyplay.tdaemon");
        bool r1;
        uint32_t r2;
        if (asyncclient.is_legacy_mode()) {
            asyncclient.GetDownloadStatus(r1, r2);
            asyncclient.SetUsageID(5);
        }
        else {
            asyncclient.GetDownloadStatus(reinterpret_cast<void*>(0xFF33AA56), true);
            asyncclient.SetUsageID(5, reinterpret_cast<void*>(0xDD12AB99), false);
        }

        asyncclient.change_path(DBus::Path("/com/wyplay/tdaemon/test"));


        {
            int i=0;
            bool r1;
            uint32_t r2;

            /*for (i=0 ; i<1000 ; i++) {
	            DBus::Connection secConnection(DBus::Connection::SystemBus(&global_dispatcher.dispatcher()));
                test_proxy client(secConnection, "/com/wyplay/tdaemon/test/multiproxy", "com.wyplay.tdaemon");

                client.GetDownloadStatus(r1, r2);
            }*/
        }

//        std::map< int32_t, ::DBus::Variant > a1;
//        ::DBus::Variant v;
//        ::DBus::MessageIter it = v.writer();
//        it << "test de string";
        //::DBus::MessageIter it;
        //::DBus::MessageIter inner = it.new_variant("s");
        //it << "test de string";
        //wl_error("%s: message iter signature %s", __PRETTY_FUNCTION__, it.signature());
        //::DBus::Variant v(it);

        //wl_error("%s: variants signature %s", __PRETTY_FUNCTION__, v.signature().c_str());

        //it.close_container(inner);
//        a1[1] = v;
//        asyncclient.TestDict(a1);


	    wl_info("Main thread joining the dispatch thread");
	    global_dispatcher.join();
    }
    catch(DBus::Error & e) {
        wl_error("DBus exception while creating the proxy to ");
    }

	wl_info("Main thread exit");

    return 0;
}
