#include <config.h>
#include <stdio.h>
#include <string.h>
#include <dbus/dbus.h>

void usage(void)
{
  printf( "usage : dbus-try-request --name=<service> [--system|--session] [--verbose]\n" \
          "\n" \
          "Try to request a name on the given bus. This is usefull in order " \
          "to check that the dbus daemon is up and ready. It is mandatory " \
          "to give a name. By default the program will try to register on " \
          "the system bus. The program return 0 in case of success, -1 on" \
          "error\n" );
}

int main(int argc, char *argv[])
{
  /* Local vars */
  DBusConnection * cnx = NULL;
  int bus              = DBUS_BUS_SYSTEM;
  int i                = 1;
  char * name          = NULL;
  int result           = -1;
  int verbose          = 0;

  /* Parse args */
  while (i < argc) {
    if      (strncmp(argv[i], "--name=", 7) == 0 )  name=argv[i] + 7;
    else if (strcmp(argv[i],  "--system") == 0 )    bus = DBUS_BUS_SYSTEM;
    else if (strcmp(argv[i],  "--session") == 0 )   bus = DBUS_BUS_SESSION;
    else if (strcmp(argv[i],  "--verbose") == 0 )   verbose = 1;
    else                                            { usage();  return -1; }
    i += 1;
  }

  /* What is tried */
  if (verbose) {
    printf("Bus  = %s\n", bus == DBUS_BUS_SYSTEM ? "System" : "Session");
    printf("Name = %s\n", name == NULL ? "(null)" : name);
  }

  /* Name is mandatory */
  if (!name) {
    usage();
  }

  /* Get bus connection */
  else if ((cnx = dbus_bus_get_private(bus, NULL)) == NULL) {
    fprintf(stderr, "Could not get bus connection\n");
  }

  /* Request name */
  else if (dbus_bus_request_name(cnx, name, 0, NULL) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    fprintf(stderr, "Could not request name %s\n", name);
  }

  /* Success */
  else {
    result = 0;
  }

  if (cnx) {
    dbus_bus_release_name(cnx, name, NULL);
    dbus_connection_close(cnx);
    dbus_connection_unref(cnx);
  }

  if (verbose) {
    printf("Done: %s\n", result == 0 ? "Success" : "Failure");
  }

  /* Done */
  return result;
}
