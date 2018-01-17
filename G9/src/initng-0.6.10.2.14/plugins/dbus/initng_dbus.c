/*
 * Initng, a next generation sysvinit replacement.
 */

#include <initng.h>

#include <stdio.h>
#include <stdlib.h> /* free() exit() */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>
#include <initng_event.h>
#include <initng_depend.h>

#include <glib.h>
#include <gio/gio.h>

/* initng service provided DBus */
#define DBUS_INITNG_SERVICE_NAME "daemon/dbus"

static GStaticMutex g_mutex = G_STATIC_MUTEX_INIT;
static int dbus_started = 0;

INITNG_PLUGIN_MACRO;

int dummy_stop(active_db_h * service);

stype_h DBUS_SERVICE = {"dbus", "This is a service dependency for dbus", TRUE, NULL, &dummy_stop, NULL };
s_entry WAIT_FOR_DBUS = { "wait_for_dbus", STRINGS, NULL,
	"Check so that this bus name are registered before launching."
}; 

a_state_h DBUS_DEP_UP = {"DBUS_DEP_UP", "Waiting for all services in this runlevel to start.", IS_UP,
        NULL, NULL, NULL};

static GHashTable* bus_hashtable = NULL;

typedef struct {
    guint watcher_id;
    gboolean present;
    gchar* service_name;
} bus_t;

int dummy_stop(active_db_h * service)
{
       service->current_state->is = IS_DOWN;
       return (TRUE);
}

static gpointer loop_thread(gpointer data)
{
    GMainLoop* loop;
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return NULL;
}

static active_db_h* dbus_serv_create(const char* name)
{
    active_db_h* dbus_serv = NULL;
    
    if (!(dbus_serv = initng_active_db_new(name)))
    {
	F_("Failed to create %s\n", name);
	return NULL;
    }
    /* set type */
    dbus_serv->type = &DBUS_SERVICE;
    
#ifdef SERVICE_CACHE
    dbus_serv->from_service = &NO_CACHE;
#endif
    
    /* register it */
    if (!initng_active_db_register(dbus_serv))
    {
	F_("Failed to register %s\n", dbus_serv->name);
	initng_active_db_free(dbus_serv);
	return (NULL);
    }
    return (dbus_serv);
}

static active_db_h* dbus_serv_find_or_create(const char* name)
{
    active_db_h* dbus_serv = NULL;
    
    dbus_serv = initng_active_db_find_by_exact_name(name);
    if (dbus_serv)
    {
	return dbus_serv;
    }
    dbus_serv = dbus_serv_create(name);
    if (dbus_serv) 
    {
	return dbus_serv;
    }
    return NULL;
}

static int dbus_set_up(const char* name)
{
    active_db_h* dbus_serv;
    char new_name[60] = "dbus/";
    strncat(new_name, name, 50);
    dbus_serv = dbus_serv_find_or_create(new_name);
    if (!dbus_serv)
    {
	return FALSE;
    }
    /* service is up */
    initng_common_mark_service(dbus_serv, &DBUS_DEP_UP);
    return TRUE;
}

static void on_name_appeared(GDBusConnection* connection,
			     const gchar *name,
			     const gchar* name_owner,
			     gpointer user_data)
{
    bus_t* tmp = NULL;
    active_db_h* service;
    s_event event;
    g_static_mutex_lock(&g_mutex);
    tmp = g_hash_table_lookup(bus_hashtable, name);
    if (NULL == tmp)
    {
	g_static_mutex_unlock(&g_mutex);
	return;
    }
    tmp->present = TRUE;
    g_static_mutex_unlock(&g_mutex);
    dbus_set_up(name);
}

static void on_name_vanished(GDBusConnection* connection,
			     const gchar* name,
			     gpointer user_data)
{
    bus_t* tmp = NULL;
    g_static_mutex_lock(&g_mutex);
    tmp = g_hash_table_lookup(bus_hashtable, name);
    if (NULL != tmp)
    {
	tmp->present = FALSE;
    }
    g_static_mutex_unlock(&g_mutex);
}

static void watch_all(gpointer key,
		     gpointer value,
		     gpointer user_data)
{
    g_bus_watch_name(G_BUS_TYPE_SYSTEM,
		     (gchar*)key,
		     G_BUS_NAME_WATCHER_FLAGS_NONE,
		     on_name_appeared,
		     on_name_vanished,
		     NULL,
		     NULL);
}

static int bus_name_parse(s_event* event)
{
    service_cache_h* s = NULL;
    const char* bus_name = NULL;
    s_data* itt = NULL;
    bus_t* tmp;
    
    s = event->data;
    while ((bus_name = get_next_string(&WAIT_FOR_DBUS, s, &itt)))
    {
	if (NULL == g_hash_table_lookup(bus_hashtable, bus_name))
	{
	    tmp = g_new0(bus_t, 1);
	    tmp->present = FALSE;
	    tmp->service_name = g_strdup(s->name);
	    D_("Service: %s\n", tmp->service_name);
	    g_hash_table_insert(bus_hashtable, g_strdup(bus_name), tmp);
	    if(dbus_started) {
		g_bus_watch_name(G_BUS_TYPE_SYSTEM,
				 bus_name,
				 G_BUS_NAME_WATCHER_FLAGS_NONE,
				 on_name_appeared,
				 on_name_vanished,
				 NULL,
				 NULL);
	    }
	}
    }
    return (TRUE);
}

/* we need to be sure that DBus is running before adding watchers */
static int service_change(s_event* event)
{
    active_db_h* service;
    assert(event->event_type == &EVENT_IS_CHANGE);
    assert(event->data);

    service = event->data;

    assert(service->name);
    if ((strlen(service->name) == strlen(DBUS_INITNG_SERVICE_NAME))
	&& (0 == strncmp(service->name, DBUS_INITNG_SERVICE_NAME, strlen(DBUS_INITNG_SERVICE_NAME)))
	)
    {
	D_("DBUS daemon state change\n");
	if (IS_UP(service))
	{
	    dbus_started = 1;
	    D_("DBUS daemon is running\n");
	    g_hash_table_foreach(bus_hashtable, watch_all, NULL);
	    g_thread_create(loop_thread, NULL, FALSE, NULL);
	}
    }
    return (TRUE);
}

static int check_bus_names_to_exist(s_event * event)
{
	active_db_h * service;
	const char *bus_name = NULL;
	s_data *itt = NULL;
	bus_t* tmp = NULL;
	
	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	while ((bus_name = get_next_string(&WAIT_FOR_DBUS, service, &itt)))
	{
	    D_("Service %s need bus name %s to exist\n", service->name, bus_name);
	    g_static_mutex_lock(&g_mutex);
	    tmp = g_hash_table_lookup(bus_hashtable, bus_name);
	    if ((NULL == tmp)
		|| (FALSE == tmp->present))
	    {
		D_("Bus name %s needed by %s doesn't exist.\n", bus_name, service->name);
		/* don't change status of service to START_DEP_MET */
		g_static_mutex_unlock(&g_mutex);
		return (FAIL);
	    }
	    g_static_mutex_unlock(&g_mutex);
	}
	return (TRUE);
}

static int my_main(s_event *event)
{
    /* restart main loop in 1 seconds */
    initng_global_set_sleep(1);
    return TRUE;
}


int module_init(int api_version)
{
	S_;
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&WAIT_FOR_DBUS);

	initng_active_state_register(&DBUS_DEP_UP);
	initng_service_type_register(&DBUS_SERVICE);

	initng_event_hook_register(&EVENT_ADDITIONAL_PARSE, &bus_name_parse);
	initng_event_hook_register(&EVENT_IS_CHANGE, &service_change);
	initng_event_hook_register(&EVENT_START_DEP_MET, &check_bus_names_to_exist);
	initng_event_hook_register(&EVENT_MAIN, &my_main);

	/* FIXME: added destroy functions */
	bus_hashtable = g_hash_table_new(g_str_hash, g_str_equal);
	g_type_init();
	return (TRUE);
}


void module_unload(void)
{
	S_;
	initng_service_data_type_unregister(&WAIT_FOR_DBUS);

	initng_active_state_unregister(&DBUS_DEP_UP);

	initng_event_hook_unregister(&EVENT_ADDITIONAL_PARSE, &bus_name_parse);
	initng_event_hook_unregister(&EVENT_IS_CHANGE, &service_change);
	initng_event_hook_unregister(&EVENT_START_DEP_MET, &check_bus_names_to_exist);
	initng_event_hook_unregister(&EVENT_MAIN, &my_main);

	initng_service_type_unregister(&DBUS_SERVICE);
	
	g_hash_table_destroy(bus_hashtable);
}
