/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
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


#ifndef __DBUSXX_MESSAGE_H
#define __DBUSXX_MESSAGE_H

#include <string>
#include <map>

#include "api.h"
#include "util.h"

namespace DBus {

class Message;
class ErrorMessage;
class SignalMessage;
class ReturnMessage;
class Error;
class Connection;

/**
 * @brief Helper for simplified message arguments encoding/decoding.
 */
class DXXAPI MessageIter
{
public:

    /**
     * @brief Constructor.
     */
	MessageIter() {}

	int type();

	bool at_end();

	bool has_next();

	MessageIter &operator ++();

	MessageIter operator ++(int);

	bool append_byte(unsigned char byte);

	unsigned char get_byte();

	bool append_bool(bool b);

	bool get_bool();

	bool append_int16(signed short i);

	signed short get_int16();

	bool append_uint16(unsigned short u);

	unsigned short get_uint16();

	bool append_int32(signed int i);

	signed int get_int32();

	bool append_uint32(unsigned int u);

	unsigned int get_uint32();

	bool append_int64(signed long long i);

	signed long long get_int64();

	bool append_uint64(unsigned long long i);

	unsigned long long get_uint64();

	bool append_double(double d);

	double get_double();
	
	bool append_string(const char *chars);

	const char *get_string();

	bool append_path(const char *chars);

	const char *get_path();

	bool append_signature(const char *chars);

	const char *get_signature();

	char *signature() const; //returned string must be manually free()'d
	
	bool append_fd(signed int fd);

	signed int get_fd();

	MessageIter recurse();

	bool append_array(char type, const void *ptr, size_t length);

	int array_type();

	int get_array(void *ptr);

	bool is_array();

	bool is_dict();

	MessageIter new_array(const char *sig);

	MessageIter new_variant(const char *sig);

	MessageIter new_struct();

	MessageIter new_dict_entry();

	void close_container(MessageIter &container);

	void copy_data(MessageIter &to);

	Message &msg() const
	{
		return *_msg;
	}

private:

	DXXAPILOCAL MessageIter(Message &msg) : _msg(&msg) {}

	DXXAPILOCAL bool append_basic(int type_id, void *value);

	DXXAPILOCAL void get_basic(int type_id, void *ptr);

private:

	/* I'm sorry, but don't want to include dbus.h in the public api
	 */
	unsigned char _iter[sizeof(void *)*3+sizeof(int)*11];

	Message *_msg;

friend class Message;
};

/**
 * @brief Generic D-Bus message.
 */
class DXXAPI Message
{
public:
	
	struct Private;

	/**
	 * @name Initializaton/Destruction.
	 * @{
	 */
	Message(Private *, bool incref = true);

	Message(const Message &m);

	~Message();

	Message &operator = (const Message &m);
	/// @}

	/**
	 * @brief Creates a copy of the message.
	 *
	 * @return The copy.
	 */
	Message copy();

	/**
	 * @brief Gives the type of the message.
	 *
	 * The message type can be one of the following:
	 * DBUS_MESSAGE_TYPE_METHOD_CALL - 1
	 * DBUS_MESSAGE_TYPE_METHOD_RETURN -2
	 * DBUS_MESSAGE_TYPE_ERROR - 3
	 * DBUS_MESSAGE_TYPE_SIGNAL - 4
	 *
	 * @return The type coded as mentioned above
	 */
	int type() const;

	/**
	 * @brief Gives the serial if of the message if existing.
	 *
	 * @return The id or 0 if the id was not specified.
	 */
	int serial() const;

	/**
	 * @brief Returns the serial that the message is a reply to or 0 if none.
	 *
	 * @return The reply id.
	 */
	int reply_serial() const;

	/**
	 * @brief Sets the reply serial of a message (the serial of the message this is a reply to).
	 *
	 * @param[in] s The serial id to set.
	 *
	 * @return True is successful.
	 */
	bool reply_serial(int);

	/**
	 * @brief Gets the unique name of the connection which originated this message, or NULL if unknown or inapplicable.
	 *
	 * @return The sender's connection name.
	 */
	const char *sender() const;

	/**
	 * @brief Sets the message sender.
	 *
	 * @param[in] s The sender name.
	 */
	bool sender(const char *s);

	/**
	 * @brief Gets the destination of a message or NULL if there is none set.
	 *
	 * @return The destination name.
	 */
	const char *destination() const;

	/**
	 * @brief Sets the message's destination.
	 *
	 * param[in] s The Destination name.
	 *
	 * @return True if successful.
	 */
	bool destination(const char *s);

	/**
	 * @brief Checks if this is an error D-Bus message.
	 *
	 * @return True is this is an error message.
	 */
	bool is_error() const;

	/**
	 * @brief Checks whether the message is a signal with the given interface and member fields.
	 *
	 * @param[in] interface The name of the interface.
	 * @param[in] member The name of the signal.
	 *
	 * @return True is the message is a signal with the given interface and signal names.
	 */
	bool is_signal(const char *interface, const char *member) const;

	/**
	 * @brief Returns a helper object to be used for decoding the arguments of the message.
	 *
	 * @return The helper decoding object.
	 */
	MessageIter reader() const;

	/**
	 * @brief Returns a helper object to be used for encoding the arguments of the message.
	 *
	 * @return The helper encoding object.
	 */
	MessageIter writer();

	/**
	 * @brief Appends arguments to the message.
	 *
	 * @return True is successful.
	 */
	bool append(int first_type, ...);

	/**
	 * @brief Ends the message's list of arguments.
	 */
	void terminate();

protected:

	Message();

protected:

	RefPtrI<Private> _pvt;

/*	classes who need to read `_pvt` directly
*/
friend class ErrorMessage;
friend class ReturnMessage;
friend class MessageIter;
friend class Error;
friend class Connection;
};

/*
*/

/**
 * @brief Error D-Bus message.
 */
class DXXAPI ErrorMessage : public Message
{
public:
	
    /**
     * @name Initializing.
     * @{
     */
	ErrorMessage();

	ErrorMessage(const Message &, const char *name, const char *message);
	/// @}

	/**
	 * @brief Gets the error name.
	 *
	 * @return A string containing the error name if set, NULL otherwise.
	 */
	const char *name() const;

	/**
	 * @brief Sets the error name
	 *
	 * @param[in] n The error name to set.
	 *
	 * @return True is successful.
	 */
	bool name(const char *n);

	/**
	 * @brief Compares with another error message.
	 *
	 * @param[in] m The error message to compare to.
	 *
	 * @return True is the error message in the argument has the same name.
	 */
	bool operator == (const ErrorMessage & m) const;
};

/*
*/

/**
 * @brief Signal D-Bus message.
 */
class DXXAPI SignalMessage : public Message
{
public:

    /**
     * @name Initialization/Destruction.
     * @{
     */

    /**
     * @brief Creates a new signal message.
     *
     * @param[in] name The name of the signal.
     */
	SignalMessage(const char *name);


	/**
	 * @brief Creates a new signal message.
	 *
	 * @param[in] path The path of the D-Bus object sending the signal.
	 * @param[in] interface The name of the D-Bus interface where the signal is declared.
	 * @param[in] name The name of the signal.
	 */
	SignalMessage(const char *path, const char *interface, const char *name);
	/// @}

	/**
	 * @brief Gets the name of the interface where the signal is declared.
	 *
	 * @return A string containing the name of the interface.
	 */
	const char *interface() const;

	/**
	 * @brief Sets the name of the interface declaring the signal.
	 *
	 * @param[in] i The interface name to set.
	 *
	 * @return True if successful.
	 */
	bool interface(const char *i);

	/**
	 * @brief Gets the name of the signal.
	 *
	 * @return A string containing the name of the signal.
	 */
	const char *member() const;

	/**
	 * @brief Sets the name of the signal.
	 *
	 * @param[in] m The signal name.
	 */
	bool member(const char *m);

	/**
	 * @brief Gets the path of the D-Bus object sending the signal.
	 *
	 * @return A string containing the path.
	 */
	const char *path() const;

    /**
     * @brief Gets the decomposed path of the D-Bus object sending the signal.
     *
     * @return An array of strings containing the segments of the path.
     */
	char ** path_split() const;

	/**
	 * @brief Sets the path of the D-Bus object sending the signal.
	 *
	 * @param[in] p The path to set.
	 *
	 * @return True is successful.
	 */
	bool path(const char *p);

    /**
     * @brief Compares with another signal message.
     *
     * @param[in] m The signal message to compare to.
     *
     * @return True is the signal message in the argument has the same interface/name.
     */
	bool operator == (const SignalMessage & m) const;
};

/*
*/

/**
 * @brief Method-call D-Bus message.
 */
class DXXAPI CallMessage : public Message
{
public:
	
    /**
     * @name Initialization.
     * @{
     */

    /**
     * @brief Constructs a method call message.
     */
	CallMessage();

	/**
	 * @brief Constructs a method call message.
	 *
	 * @param[in] dest The connection name of the destination where to send the message.
	 * @param[in] path The path of the D-Bus object sending the message.
	 * @param[in] iface The name of the interface in whose scope the method is declared.
	 * @param[in] method The nme of the method.
	 */
	CallMessage(const char *dest, const char *path, const char *iface, const char *method);
	/// @}

    /**
     * @brief Gets the name of the interface in whose scope the method is declared.
     *
     * @return A string containing the name of the interface.
     */
	const char *interface() const;

    /**
     * @brief Sets the name of the interface declaring the method.
     *
     * @param[in] i The interface name to set.
     *
     * @return True if successful.
     */
	bool interface(const char *i);

    /**
     * @brief Gets the name of the method.
     *
     * @return A string containing the name of the method.
     */
	const char *member() const;

    /**
     * @brief Sets the name of the method.
     *
     * @param[in] m The method name.
     */
	bool member(const char *m);

    /**
     * @brief Gets the path of the D-Bus object targeted by the method call message.
     *
     * @return A string containing the path.
     */
	const char *path() const;

    /**
     * @brief Gets the decomposed path of the D-Bus object targeted by the method call message.
     *
     * @return An array of strings containing the segments of the path.
     */
	char ** path_split() const;

    /**
     * @brief Sets the path of the D-Bus object targeted by the method call message.
     *
     * @param[in] p The path to set.
     *
     * @return True is successful.
     */
	bool path(const char *p);

	/**
	 * @brief Gets the D-Bus arguments signature.
	 *
	 * @return A string containg the D-Bus arguments signature.
	 */
	const char *signature() const;

    /**
     * @brief Compares with another method call message.
     *
     * @param[in] m The method call message to compare to.
     *
     * @return True is the method call message in the argument has the same interface/name.
     */
	bool operator == (const CallMessage & m) const;
};

/*
*/

/**
 * @brief Method-call-reply D-Bus message.
 */
class DXXAPI ReturnMessage : public Message
{
public:
	
    /**
     * @brief Contructs a method return message.
     *
     * @param[in] callee The method call message whose reply to construct.
     */
	ReturnMessage(const CallMessage &callee);

    /**
     * @brief Gets the D-Bus arguments signature.
     *
     * @return A string containg the D-Bus arguments signature.
     */
	const char *signature() const;
};

} /* namespace DBus */

#endif//__DBUSXX_MESSAGE_H
