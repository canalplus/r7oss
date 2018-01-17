#include "dbus-spy.h"

#include "dbus-protocol.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "strlcat.c"


#define DBUS_SPY_PORT       5759
#define DBUS_SPY_SERVER_ENV "DBUS_SPY_SERVER"
#define DBUS_SPY_FORMAT_ENV "DBUS_SPY_FORMAT"
#define DBUS_SPY_PRINTF_ENV "DBUS_SPY_PRINTF"
#define DBUS_SPY_DUMP_ARGS  "DBUS_SPY_DUMP_ARGS"

/* Size of the whole message dump buffer - Allocated on heap */
#define DBUS_SPY_BUFFER_LEN 32768

/* Size of each message element, including strings for paths, interfaces, ... - Allocated on heap */
#define DBUS_SPY_ELEM_MAX   8192

/* Size of each msg argument, except strings - Allocated on stack */
#define DBUS_SPY_ARG_MAX    128


typedef enum
{
  XML = 0,
  CSV = 1
} e_dbus_spy_format;


static pthread_mutex_t dbus_spy_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static int dbus_spy_fd = -1;
static int dbus_spy_printf = 0;
static struct sockaddr_in dbus_spy_server;
static e_dbus_spy_format dbus_spy_format = XML;
static int dbus_spy_dump_args = 1;  // Dump args by default



static void
_dbus_spy_message_iter_args_as_text (DBusMessageIter * iterator, char *buffer,
                                     int bufsize)
{
  int arg_type;
  char arg_buf[DBUS_SPY_ARG_MAX];

  /* Prepare first arg */
  arg_type = dbus_message_iter_get_arg_type (iterator);

  if (arg_type == DBUS_TYPE_INVALID)
    {
      return;                   /* No args here */
    }

  while (1)
    {
      switch (arg_type)
        {
        case DBUS_TYPE_BYTE:
          {
            unsigned char arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "byte %d", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
            break;
          }
          break;

        case DBUS_TYPE_BOOLEAN:
          {
            dbus_bool_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "boolean %s",
                      arg_value ? "true" : "false");
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
            break;
          }
          break;

        case DBUS_TYPE_INT16:
          {
            dbus_int16_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "int16 %d", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_UINT16:
          {
            dbus_uint16_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "uint16 %u", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_INT32:
          {
            dbus_int32_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "int32 %d", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_UINT32:
          {
            dbus_uint32_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "uint32 %u", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_INT64:
          {
            dbus_int64_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "int64 %lld", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_UINT64:
          {
            dbus_uint64_t arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "uint64 %llu",
                      arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_DOUBLE:
          {
            double arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            snprintf (arg_buf, DBUS_SPY_ARG_MAX - 1, "double %g", arg_value);
            arg_buf[DBUS_SPY_ARG_MAX - 1] = 0;
            strlcat (buffer, arg_buf, bufsize);
          }
          break;

        case DBUS_TYPE_STRING:
          {
            char *arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            strlcat (buffer, "string \"", bufsize);
            strlcat (buffer, arg_value, bufsize);
            strlcat (buffer, "\"", bufsize);
          }
          break;

        case DBUS_TYPE_OBJECT_PATH:
          {
            char *arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            strlcat (buffer, "object path \"", bufsize);
            strlcat (buffer, arg_value, bufsize);
            strlcat (buffer, "\"", bufsize);
          }
          break;

        case DBUS_TYPE_SIGNATURE:
          {
            char *arg_value;
            dbus_message_iter_get_basic (iterator, &arg_value);
            strlcat (buffer, "signature \"", bufsize);
            strlcat (buffer, arg_value, bufsize);
            strlcat (buffer, "\"", bufsize);
          }
          break;

        case DBUS_TYPE_ARRAY:
          {
            DBusMessageIter subiterator;

            dbus_message_iter_recurse (iterator, &subiterator);

            strlcat (buffer, "array [", bufsize);
            while (dbus_message_iter_get_arg_type (&subiterator) !=
                   DBUS_TYPE_INVALID)
              {
                _dbus_spy_message_iter_args_as_text (&subiterator, buffer,
                                                     bufsize);
                dbus_message_iter_next (&subiterator);
                if (dbus_message_iter_get_arg_type (&subiterator) !=
                    DBUS_TYPE_INVALID)
                  strlcat (buffer, ",", bufsize);
              }
            strlcat (buffer, "]", bufsize);
          }
          break;

        case DBUS_TYPE_VARIANT:
          {
            DBusMessageIter subiterator;

            dbus_message_iter_recurse (iterator, &subiterator);

            strlcat (buffer, "variant ", bufsize);
            _dbus_spy_message_iter_args_as_text (&subiterator, buffer,
                                                 bufsize);

          }
          break;

        case DBUS_TYPE_STRUCT:
          {
            DBusMessageIter subiterator;

            dbus_message_iter_recurse (iterator, &subiterator);

            strlcat (buffer, "struct (", bufsize);
            while (dbus_message_iter_get_arg_type (&subiterator) !=
                   DBUS_TYPE_INVALID)
              {
                _dbus_spy_message_iter_args_as_text (&subiterator, buffer,
                                                     bufsize);
                dbus_message_iter_next (&subiterator);
                if (dbus_message_iter_get_arg_type (&subiterator) !=
                    DBUS_TYPE_INVALID)
                  strlcat (buffer, ",", bufsize);
              }
            strlcat (buffer, ")", bufsize);
          }
          break;

        case DBUS_TYPE_DICT_ENTRY:
          {
            DBusMessageIter subiterator;

            dbus_message_iter_recurse (iterator, &subiterator);

            strlcat (buffer, "dict_entry (", bufsize);
            _dbus_spy_message_iter_args_as_text (&subiterator, buffer,
                                                 bufsize);

            dbus_message_iter_next (&subiterator);
            _dbus_spy_message_iter_args_as_text (&subiterator, buffer,
                                                 bufsize);

            strlcat (buffer, ")", bufsize);
          }
          break;

        default:
          strlcat (buffer, "unknown", bufsize);
          break;
        }

      /* Prepare for next arg */
      dbus_message_iter_next (iterator);

      arg_type = dbus_message_iter_get_arg_type (iterator);

      if (arg_type == DBUS_TYPE_INVALID)
        break;                  /* No more args */
      else
        strlcat (buffer, ",", bufsize); /* Add a comma before next arg */
    }
}

static void
_dbus_spy_message_args_as_text (DBusMessage * message, char *buffer,
                                int bufsize)
{
  DBusMessageIter iterator;

  dbus_message_iter_init (message, &iterator);

  _dbus_spy_message_iter_args_as_text (&iterator, buffer, bufsize);
}

const char *
_dbus_spy_message_type_as_text (DBusMessage * message)
{
  const char *type;

  switch (dbus_message_get_type (message))
    {
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
      type = "MethodCall";
      break;
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      type = "MethodReturn";
      break;
    case DBUS_MESSAGE_TYPE_ERROR:
      type = "Error";
      break;
    case DBUS_MESSAGE_TYPE_SIGNAL:
      type = "Signal";
      break;
    case DBUS_MESSAGE_TYPE_INVALID:
      type = "Invalid";
      break;
    default:
      type = "Unknown";
      break;
    }

  return type;
}

static void
_dbus_spy_parse_message_xml (char *dbus_spy_buffer, int spy_bufsize,
                             char *buffer, int bufsize, DBusMessage * message,
                             dbus_bool_t outgoing,
                             long long unsigned int timestamp)
{
  snprintf (dbus_spy_buffer, spy_bufsize - 1,
            "<message>\n"
            "  <process>%d</process>\n"
            "  <direction>%s</direction>\n"
            "  <serial>%u</serial>\n"
            "  <timestamp>%llu</timestamp>\n"
            "  <type>%s</type>\n",
            getpid (),
            (outgoing) ? "Out" : "In",
            dbus_message_get_serial (message),
            timestamp, _dbus_spy_message_type_as_text (message));

  dbus_spy_buffer[spy_bufsize - 1] = 0;

  if (dbus_message_get_sender (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <sender>%s</sender>\n", dbus_message_get_sender (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_destination (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <destination>%s</destination>\n",
                dbus_message_get_destination (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
      snprintf (buffer, bufsize - 1,
                "  <replySerial>%u</replySerial>\n",
                dbus_message_get_reply_serial (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_path (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <path>%s</path>\n", dbus_message_get_path (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_interface (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <interface>%s</interface>\n",
                dbus_message_get_interface (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_member (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <member>%s</member>\n", dbus_message_get_member (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  if (dbus_message_get_signature (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "  <signature>%s</signature>\n",
                dbus_message_get_signature (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  /* if it's an error, add its name */
  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_ERROR)
    {
      snprintf (buffer, bufsize - 1,
                "  <errorName>%s</errorName>\n",
                dbus_message_get_error_name (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  /* Dump message arguments */
  if (dbus_spy_dump_args)
    {
      strlcat (dbus_spy_buffer, "  <arguments>", spy_bufsize);
      _dbus_spy_message_args_as_text (message, dbus_spy_buffer, spy_bufsize);
      strlcat (dbus_spy_buffer, "</arguments>\n", spy_bufsize);
    }

  strlcat (dbus_spy_buffer, "\n", spy_bufsize);

  strlcat (dbus_spy_buffer, "</message>\n\n", spy_bufsize);


  dbus_spy_buffer[spy_bufsize - 1] = 0;
}

static void
_dbus_spy_parse_message_csv (char *dbus_spy_buffer, int spy_bufsize,
                             char *buffer, int bufsize, DBusMessage * message,
                             dbus_bool_t outgoing,
                             long long unsigned int timestamp)
{
  snprintf (dbus_spy_buffer, spy_bufsize - 1,
            "%d;%s;%u;%llu;%s;",
            getpid (),
            (outgoing) ? "Out" : "In",
            dbus_message_get_serial (message),
            timestamp, _dbus_spy_message_type_as_text (message));

  dbus_spy_buffer[spy_bufsize - 1] = 0;

  if (dbus_message_get_sender (message) != NULL)
    {
      snprintf (buffer, bufsize - 1, "%s", dbus_message_get_sender (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_destination (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "%s", dbus_message_get_destination (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
      snprintf (buffer, bufsize - 1,
                "%u", dbus_message_get_reply_serial (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_path (message) != NULL)
    {
      snprintf (buffer, bufsize - 1, ">%s", dbus_message_get_path (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_interface (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "%s", dbus_message_get_interface (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_member (message) != NULL)
    {
      snprintf (buffer, bufsize - 1, "%s", dbus_message_get_member (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  if (dbus_message_get_signature (message) != NULL)
    {
      snprintf (buffer, bufsize - 1,
                "%s", dbus_message_get_signature (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }
  strlcat (dbus_spy_buffer, ";", spy_bufsize);

  /* if it's an error, add its name */
  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_ERROR)
    {
      snprintf (buffer, bufsize - 1,
                "%s", dbus_message_get_error_name (message));
      buffer[bufsize - 1] = 0;
      strlcat (dbus_spy_buffer, buffer, spy_bufsize);
    }

  /* Dump message arguments */
  if (dbus_spy_dump_args)
    {
      strlcat (dbus_spy_buffer, ";", spy_bufsize);
      _dbus_spy_message_args_as_text (message, dbus_spy_buffer, spy_bufsize);
    }

  strlcat (dbus_spy_buffer, "\n", spy_bufsize);


  dbus_spy_buffer[spy_bufsize - 1] = 0;
}

void
_dbus_spy_dump_message (DBusMessage * message, dbus_bool_t outgoing)
{
  char *dbus_spy_buffer = NULL;
  char *buffer = NULL;
  char *dbus_spy_dump_args_string;
  static int trace_done = 0;

  struct timespec ts;
  long long unsigned int timestamp;

  dbus_spy_buffer = malloc (DBUS_SPY_BUFFER_LEN);
  if (!dbus_spy_buffer)
    {
      fprintf (stderr, "DBUS_SPY: Unable to allocate memory\n");
      return;
    }

  buffer = malloc (DBUS_SPY_ELEM_MAX);
  if (!buffer)
    {
      fprintf (stderr, "DBUS_SPY: Unable to allocate memory\n");
      return;
    }

  if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0)
    {
      fprintf (stderr, "DBUS_SPY: Failed to get time: %s\n",
               strerror (errno));
      timestamp = 0;
    }
  else
    {
      timestamp = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    }


  pthread_mutex_lock (&dbus_spy_fd_mutex);

  if (dbus_spy_fd < 0)
    {
      const char *dbus_spy_server_addr = NULL;
      const char *dbus_spy_format_string = NULL;

      dbus_spy_format_string = getenv (DBUS_SPY_FORMAT_ENV);
      if (dbus_spy_format_string && !strcmp (dbus_spy_format_string, "CSV"))
        dbus_spy_format = CSV;

      if (getenv (DBUS_SPY_PRINTF_ENV))
        dbus_spy_printf = 1;

      dbus_spy_dump_args_string = getenv (DBUS_SPY_DUMP_ARGS);
      if (dbus_spy_dump_args_string)
        dbus_spy_dump_args = atoi (dbus_spy_dump_args_string);

      dbus_spy_server_addr = getenv (DBUS_SPY_SERVER_ENV);

      if (dbus_spy_server_addr == NULL)
        {
          if (!trace_done)
            fprintf (stderr, "DBUS_SPY: No %s envvar defined\n",
                     DBUS_SPY_SERVER_ENV);
        }
      else
        {
          if (inet_aton (dbus_spy_server_addr, &(dbus_spy_server.sin_addr)) ==
              0)
            {
              fprintf (stderr,
                       "DBUS_SPY: Failed to resolve server address (%s)\n",
                       dbus_spy_server_addr);
            }

          dbus_spy_server.sin_family = AF_INET;
          dbus_spy_server.sin_port = htons (DBUS_SPY_PORT);
        }

      if ((dbus_spy_fd = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
        {
          fprintf (stderr, "DBUS_SPY: Failed to create socket: %s\n",
                   strerror (errno));
        }

      if (!trace_done)
        fprintf (stderr, "DBUS_SPY: socket create\n");
    }
  pthread_mutex_unlock (&dbus_spy_fd_mutex);

  if (dbus_spy_fd > 0)
    {
      switch (dbus_spy_format)
        {
        case CSV:
          _dbus_spy_parse_message_csv (dbus_spy_buffer, DBUS_SPY_BUFFER_LEN,
                                       buffer, DBUS_SPY_ELEM_MAX, message,
                                       outgoing, timestamp);
          break;
        default:
          _dbus_spy_parse_message_xml (dbus_spy_buffer, DBUS_SPY_BUFFER_LEN,
                                       buffer, DBUS_SPY_ELEM_MAX, message,
                                       outgoing, timestamp);
          break;
        }

      if (dbus_spy_printf)
        {
          printf (dbus_spy_buffer);
          fflush (NULL);
        }

      if (sendto (dbus_spy_fd, dbus_spy_buffer, strlen (dbus_spy_buffer), 0,
                  (struct sockaddr *) &dbus_spy_server,
                  sizeof (dbus_spy_server)) < 0)
        {
          if (!trace_done)
            fprintf (stderr, "DBUS_SPY: Failed to send message: %s\n",
                     strerror (errno));
          close (dbus_spy_fd);
          dbus_spy_fd = -1;
          trace_done = 1;
        }
    }

  free (dbus_spy_buffer);
  free (buffer);
}
