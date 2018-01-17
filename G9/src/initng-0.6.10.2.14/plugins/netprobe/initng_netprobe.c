/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <initng.h>


#include <stdio.h>
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

INITNG_PLUGIN_MACRO;

static int is_network(void);
static int check_START_DEP_MET(s_event * event);
static int check_STOP_DEP_MET(s_event * event);

int network_status = FALSE;
time_t last_check = 0;

static int is_network(void)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buffert[1024];
	int netsock = -1;


	netsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (netsock < 0)
	{
		F_("Unable to open a socket!\n");
		return (FALSE);
	}

	ifc.ifc_len = sizeof(buffert);
	ifc.ifc_buf = buffert;

	if (ioctl(netsock, SIOCGIFCONF, &ifc) < 0)
	{
		F_("error: SIOCGIFCONF\n");
		return (FALSE);
	}

	/*printf("len:%i\n", ifc.ifc_len); */


	ifr = ifc.ifc_req;

	{
		int i;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++)
		{
			/* Loopback is not network */
			if (strcmp("lo", ifr->ifr_name) == 0)
				continue;
			/*struct ifreq i; */
			D_("found up interface %s.\n", ifr->ifr_name);
			close(netsock);
			network_status = TRUE;
			return (TRUE);
		}

	}

	close(netsock);
	network_status = FALSE;
	return (FALSE);
}

s_entry REQUIRE_NETWORK = { "require_network", SET, NULL,
	"If set, initng will wait for an active Internet connection before starting this service."
};
s_entry NETWORK_PROVIDER = { "network_provider", SET, NULL,
	"Set this on services that provides network, and it will be stopped after daemons have quit."
};


static int check_STOP_DEP_MET(s_event * event)
{
	active_db_h * service;
	active_db_h * current = NULL;

	assert(event->event_type == &EVENT_STOP_DEP_MET);
	assert(event->data);

	service = event->data;

	/* The network provider should always be stoppable */
	if (is(&NETWORK_PROVIDER, service))
		return (TRUE);

	/*
	 * if there exits any service with NETWORK_PROVIDER set,
	 * and it is not down, continue waiting for STOP_DEP
	 */
	while_active_db(current)
	{
		if (is(&NETWORK_PROVIDER, service) && (!IS_DOWN(service)))
			return (FAIL);
	}

	return (TRUE);
}

static int check_START_DEP_MET(s_event * event)
{
	active_db_h * service;

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	/* only apply if service requires network */
	if (!is(&REQUIRE_NETWORK, service))
		return (TRUE);

	D_("Doing check because REQURE_NETWORK is set.\n");

	/*
	 * I believe that we are way better probing this in kernel,
	 * then waiting for the net provider service to be up.
	 */
#ifdef REQUIRE_NETWORK_PROVIDER_SERVICE_TO_BE_UP
	active_db_h *current = NULL;

	while_active_db(current)
	{
		if (is(&NETWORK_PROVIDER, current) && IS_UP(current))
			goto ok;
	}
	/* Don't set sleep 1, because initng will be interrupted when a
	 * service changes state anyway.
	 */
	/*initng_set_sleep(1); */
	return (FAIL);

  ok:
#endif

	/*
	 * Now when we now that there is a network provider that
	 * is really up, we do an check with kernel network subsystem
	 * so we now this is true.
	 */

	/* only do this check once every mainloop */
	if (last_check != g.now.tv_sec)
	{
		network_status = is_network();
		D_("Network status = %s\n",
		   network_status == TRUE ? "TRUE" : "FALSE");
	}
	last_check = g.now.tv_sec;

	if (network_status == TRUE)
		return (TRUE);

	/* Make sure mainloop will run within 1 second. */
	initng_global_set_sleep(1);

	return (FAIL);
}

int module_init(int api_version)
{
	S_;
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&REQUIRE_NETWORK);
	initng_service_data_type_register(&NETWORK_PROVIDER);
	initng_event_hook_register(&EVENT_START_DEP_MET, &check_START_DEP_MET);
	initng_event_hook_register(&EVENT_STOP_DEP_MET, &check_STOP_DEP_MET);

	return (TRUE);
}

void module_unload(void)
{
	S_;
	initng_service_data_type_unregister(&REQUIRE_NETWORK);
	initng_service_data_type_unregister(&NETWORK_PROVIDER);
	initng_event_hook_unregister(&EVENT_START_DEP_MET, &check_START_DEP_MET);
	initng_event_hook_unregister(&EVENT_STOP_DEP_MET, &check_STOP_DEP_MET);
}
