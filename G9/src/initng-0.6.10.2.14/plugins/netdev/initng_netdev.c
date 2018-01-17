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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_event_hook.h>
#include <initng_static_event_types.h>

INITNG_PLUGIN_MACRO;

/* this service type will a virtual provided get */
stype_h NETDEV = { "netdev", "This is a service dependency for network interfaces.", TRUE, NULL, NULL, NULL };

static void net_remove(const char *name);
static int net_set_up(const char *name);


/* THE UP STATE GOT */
a_state_h NIC_STARTING = { "NIC_STARTING", "This interface is found, but dont have an ip adress.", IS_STARTING, NULL, NULL, NULL };
a_state_h NIC_UP = { "NIC_UP", "This nic is up.", IS_UP, NULL, NULL, NULL };
a_state_h NIC_DOWN = { "NIC_DOWN", "This nic is down.", IS_DOWN, NULL, NULL, NULL };

typedef struct
{
	int status;
	char *dev;
} s_netdev;

/*
 * we need to have a list in memory, to notice when devices
 * are lost on the system.
 */
s_netdev devs[20];

static void devs_reset(void)
{
	int i;

	for (i = 0; i < 20; i++)
		devs[i].status = 0;
}

static void set_found(const char *name)
{
	int i;

	/* look for it */
	for (i = 0; i < 20; i++)
	{
		if (devs[i].dev)
		{
			if (strcmp(devs[i].dev, name) == 0)
			{
				devs[i].status = 1;
				return;
			}
			else
				continue;
		}
	}

	/* ok copy a new */
	for (i = 0; i < 20; i++)
	{
		if (!devs[i].dev)
		{
			devs[i].dev = i_strdup(name);
			devs[i].status = 1;
			return;
		}
	}
}

static void handle_devs(void)
{
	int i;

	for (i = 0; i < 20; i++)
	{
		if (!devs[i].dev)
			continue;

		if (devs[i].status == 1)
		{
			net_set_up(devs[i].dev);
		}
		else
		{
			net_remove(devs[i].dev);
			free(devs[i].dev);
			devs[i].dev = NULL;
		}
	}
}

/*
 * This function creates a new service nettype.
 */
static active_db_h *net_create(const char *name)
{
	active_db_h *netdev = NULL;

	/* create a new */
	if (!(netdev = initng_active_db_new(name)))
	{
		F_("Failed to create %s\n");
		return (NULL);
	}

	/* set type */
	netdev->type = &NETDEV;

#ifdef SERVICE_CACHE
	/* make sure it wont try to parse this entry */
	netdev->from_service = &NO_CACHE;
#endif

	/* register it */
	if (!initng_active_db_register(netdev))
	{
		F_("Failed to register %s\n", netdev->name);
		initng_active_db_free(netdev);
		return (NULL);
	}
	return (netdev);
}

static active_db_h *find_or_create(const char *name)
{
	active_db_h *netdev = NULL;

	/* first try find */
	netdev = initng_active_db_find_by_exact_name(name);
	if (netdev)
	{
		if (netdev->type == &NETDEV)
			return (netdev);
		else
			return (NULL);
	}

	/* else create it */
	netdev = net_create(name);
	if (netdev)
		return (netdev);

	/* else return failure */
	return (NULL);
}


/*
 * When an netdevice gets an ip-adress its set to UP
 */
static int net_set_up(const char *name)
{
	active_db_h *netdev;

	/* fix up the service name */
	char new_name[20] = "device/";

	strncat(new_name, name, 10);

	netdev = find_or_create(new_name);
	if (!netdev)
		return (FALSE);

	/* devices is up */
	initng_common_mark_service(netdev, &NIC_UP);
	return (TRUE);
}

#ifdef NOTYET
/*
 * When a netdevices is found, but wont have an ip, its set to STARTING.
 */
static int net_set_starting(const char *name)
{
	active_db_h *netdev;

	/* fix up the service name */
	char new_name[20] = "device/";

	strncat(new_name, name, 10);

	netdev = find_or_create(new_name);
	if (!netdev)
		return (FALSE);

	initng_common_mark_service(netdev, &NIC_STARTING);
	return (TRUE);
}
#endif

/*
 * When a nic is not found anymore, it will be removed.
 */
static void net_remove(const char *name)
{
	active_db_h *netdev;

	/* fix up the service name */
	char new_name[20] = "device/";

	strncat(new_name, name, 10);

	/* find that exact service */
	if (!(netdev = initng_active_db_find_by_exact_name(new_name)))
	{
		/* if not found, its down */
		return;
	}

	/* make sure this found is a provided type */
	if (netdev->type != &NETDEV)
	{
		F_("Netdev bad type.\n");
		return;
	}

	/* mark service down */
	initng_common_mark_service(netdev, &NIC_DOWN);
}


static int probe_network_devices(s_event *event)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buffert[1024];
	int netsock = -1;

	assert(event->event_type == &EVENT_MAIN);

	/* no monitoring on system_stopping */
	if (g.sys_state == STATE_STOPPING)
		return (TRUE);

	devs_reset();

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

	/* now add all nics */
	ifr = ifc.ifc_req;
	{
		int i;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++)
		{
			/*struct ifreq i; */
			/*printf("found up interface %s.\n", ifr->ifr_name); */
			set_found(ifr->ifr_name);
		}
	}

	handle_devs();

	close(netsock);

	/* Make sure mainloop will run within 2 mins. */
	initng_global_set_sleep(120);

	return (TRUE);
}


static int system_stopping(s_event * event)
{
	h_sys_state * state;
	active_db_h *current = NULL;

	assert(event->event_type == &EVENT_SYSTEM_CHANGE);
	assert(event->data);

	state = event->data;

	/* only do this if system is stopping */
	if (*state != STATE_STOPPING)
		return (TRUE);

	/* find all netdev types and stop them */
	while_active_db(current)
	{
		if (current->type == &NETDEV)
		{
			/* mark service down */
			initng_common_mark_service(current, &NIC_DOWN);
		}
	}

	return (TRUE);
}


int module_init(int api_version)
{
	int i;

	S_;
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	/* reset local db */
	for (i = 0; i < 20; i++)
	{
		devs[i].status = 0;
		devs[i].dev = NULL;
	}


	initng_active_state_register(&NIC_STARTING);
	initng_active_state_register(&NIC_UP);
	initng_active_state_register(&NIC_DOWN);
	initng_service_type_register(&NETDEV);
	initng_event_hook_register(&EVENT_MAIN, &probe_network_devices);
	initng_event_hook_register(&EVENT_SYSTEM_CHANGE, &system_stopping);
	return (TRUE);
}

void module_unload(void)
{
	int i;

	S_;

	for (i = 0; i < 20; i++)
	{
		if (devs[i].dev)
			free(devs[i].dev);
	}

	initng_active_state_unregister(&NIC_STARTING);
	initng_active_state_unregister(&NIC_UP);
	initng_active_state_unregister(&NIC_DOWN);
	initng_event_hook_unregister(&EVENT_MAIN, &probe_network_devices);
	initng_event_hook_unregister(&EVENT_SYSTEM_CHANGE, &system_stopping);
	initng_service_type_unregister(&NETDEV);
}
