#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dbus/dbus.h>

/**
 * Command to do dbus-request-name --name=com.wyplay.wait.dbus --system in loop
 */

static int dbus_wait_dbus_loop(void);
static int dbus_wait_dbus_request(void);

int dbus_wait_dbus_loop(void)
{
  while (dbus_wait_dbus_request() != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    sleep(1);
  }

  return EXIT_SUCCESS;
}

int dbus_wait_dbus_request(void)
{
  const int bus = DBUS_BUS_SYSTEM;
  const char *name = "com.wyplay.wait.dbus";
  DBusConnection *cnx = NULL;
  int status = -1;

  cnx = dbus_bus_get_private(bus, NULL);

  if (cnx == NULL) {
    fprintf(stderr, "Could not get bus connection: name '%s'\n", name);
    return -1;
  }

  status = dbus_bus_request_name(cnx, name, 0, NULL);

  if (cnx) {
    dbus_bus_release_name(cnx, name, NULL);
    dbus_connection_close(cnx);
    dbus_connection_unref(cnx);
  }

  return status;
}

int main(int argc, char *argv[])
{
  return dbus_wait_dbus_loop();
}
