#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "initng_dbusevent.h"

/**
 * Listens for signals on the bus
 */
int main(int argc, char **argv)
{
	DBusMessage *msg;
	DBusMessageIter args;
	DBusConnection *conn;
	DBusError err;
	int ret;

	printf("Listening for initng-events\n");

	/* initialise the errors */
	dbus_error_init(&err);

	/* connect to the bus and check for errors */
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err))
	{
		fprintf(stderr, "Connection Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (NULL == conn)
	{
		exit(1);
	}

	/* request our name on the bus and check for errors */
	ret = dbus_bus_request_name(conn, SINK_REQUEST,
								DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (dbus_error_is_set(&err))
	{
		fprintf(stderr, "Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret)
	{
		exit(1);
	}

	/* add a rule for which messages we want to see */
	dbus_bus_add_match(conn, "type='signal',interface='" INTERFACE "'", &err);	/* see signals from the given interface */
	dbus_connection_flush(conn);
	if (dbus_error_is_set(&err))
	{
		fprintf(stderr, "Match Error (%s)\n", err.message);
		exit(1);
	}

	/* loop listening for signals being emmitted */
	while (true)
	{

		/* non blocking read of the next available message */
		dbus_connection_read_write(conn, 0);
		msg = dbus_connection_pop_message(conn);

		/* loop again if we haven't read a message */
		if (NULL == msg)
		{
			sleep(1);
			continue;
		}


		if (dbus_message_is_signal(msg, INTERFACE, "astatus_change"))
		{
			char *service = NULL;
			int is = 0;
			char *state = NULL;

			/* read the parameters */
			if (!dbus_message_iter_init(msg, &args))
				fprintf(stderr, "Message Has No Parameters\n");

			/* First interator is a string with service name */
			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &service);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			/* Second value is an int */
			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)
				exit(1);

			dbus_message_iter_get_basic(&args, &is);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			/* Third arg is a string, with state name */
			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &state);


			printf(" astatus_change service: \"%s\" is: \"%i\" state: \"%s\"\n", service, is, state);
		}

		if (dbus_message_is_signal(msg, INTERFACE, "system_state_change"))
		{
			int sys_state = 0;

			/* read the parameters */
			if (!dbus_message_iter_init(msg, &args))
				fprintf(stderr, "Message Has No Parameters\n");

			/* Second value is an int */
			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)
				exit(1);


			printf(" system_state_change state: %i\n", sys_state);
		}

		if (dbus_message_is_signal(msg, INTERFACE, "system_output"))
		{
			char *service = NULL;
			char *process = NULL;
			char *output = NULL;

			/* read the parameters */
			if (!dbus_message_iter_init(msg, &args))
				fprintf(stderr, "Message Has No Parameters\n");

			/* First interator is a string with service name */
			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &service);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &process);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &output);


			printf(" system_output service: %s process: %s \n%s\n", service,
				   process, output);
		}

		if (dbus_message_is_signal(msg, INTERFACE, "print_error"))
		{
			int mt = 0;
			char *file = NULL;
			char *func = NULL;
			int line = 0;
			char *message = NULL;

			/* read the parameters */
			if (!dbus_message_iter_init(msg, &args))
				fprintf(stderr, "Message Has No Parameters\n");

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)
				exit(1);

			dbus_message_iter_get_basic(&args, &mt);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &file);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &func);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)
				exit(1);

			dbus_message_iter_get_basic(&args, &line);

			/* go to next iter */
			if (!dbus_message_iter_next(&args))
				exit(1);

			if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
				exit(1);

			dbus_message_iter_get_basic(&args, &message);

			printf(" system_output service: mt: %i file: %s func: %s line: %i\n%s\n", mt, file, func, line, message);
		}


		/* free the message */
		dbus_message_unref(msg);
	}

	/* close the connection */
	dbus_connection_close(conn);
}
