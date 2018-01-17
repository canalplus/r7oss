#ifndef DBUS_SPY_H
#define DBUS_SPY_H

#include "dbus-message.h"

const char *_dbus_spy_message_type_as_text (DBusMessage *message);
void _dbus_spy_dump_message (DBusMessage *message, dbus_bool_t outgoing);

#endif

