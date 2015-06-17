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


#ifndef __DBUSXX_ERROR_H
#define __DBUSXX_ERROR_H

#include "api.h"
#include "util.h"

#include <exception>

namespace DBus {

class Message;
class InternalError;

/**
 * @brief Base exception class for all the D-Bus related exceptions.
 *
 * The handling of the D-Bus errors is implemented using standard C++ exceptions. To that purpose,
 * there is a C++ exception class declared for each major D-Bus error type.
 */
class DXXAPI Error : public std::exception
{
public:

    /**
     * @brief Constructor.
     */
	Error();

	/// @cond INTERNAL_API
	Error(InternalError &);

	Error(const char *name, const char *message = 0);
	
	Error(Message &);
    /// @endcond

	/**
	 * @brief Destructor.
	 */
	~Error() throw();

	/**
	 * @brief Returns the a message text associated with a D-Bus error.
	 *
	 * @return Associated message.
	 */
	const char *what() const throw();

    /**
     * @brief Returns the textual name of the D-Bus error.
     *
     * @return Error textual name.
     */
	const char *name() const;

    /**
     * @brief Returns the a message text associated with a D-Bus error.
     *
     * @return Associated message.
     */
	const char *message() const;

	/**
	 * @brief Initializes the data required by a D-Bus error.
	 *
	 * @param[in] D-Bus error name.
	 * @param[in] Message text associated with the D-Bus error.
	 */
	void set(const char *name, const char *message);
	// parameters MUST be static strings

	bool is_set() const;

	operator bool() const
	{
		return is_set();
	}

private:

	RefPtrI<InternalError> _int;
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Failed".
 */
struct DXXAPI ErrorFailed : public Error
{
	ErrorFailed(const char *message)
	: Error("org.freedesktop.DBus.Error.Failed", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NoMemory".
 */
struct DXXAPI ErrorNoMemory : public Error
{
	ErrorNoMemory(const char *message)
	: Error("org.freedesktop.DBus.Error.NoMemory", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.ServiceUnknown".
 */
struct DXXAPI ErrorServiceUnknown : public Error
{
	ErrorServiceUnknown(const char *message)
	: Error("org.freedesktop.DBus.Error.ServiceUnknown", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NameHasNoOwner".
 */
struct DXXAPI ErrorNameHasNoOwner : public Error
{
	ErrorNameHasNoOwner(const char *message)
	: Error("org.freedesktop.DBus.Error.NameHasNoOwner", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NoReply".
 */
struct DXXAPI ErrorNoReply : public Error
{
	ErrorNoReply(const char *message)
	: Error("org.freedesktop.DBus.Error.NoReply", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.IOError".
 */
struct DXXAPI ErrorIOError : public Error
{
	ErrorIOError(const char *message)
	: Error("org.freedesktop.DBus.Error.IOError", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.BadAddress".
 */
struct DXXAPI ErrorBadAddress : public Error
{
	ErrorBadAddress(const char *message)
	: Error("org.freedesktop.DBus.Error.BadAddress", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NotSupported".
 */
struct DXXAPI ErrorNotSupported : public Error
{
	ErrorNotSupported(const char *message)
	: Error("org.freedesktop.DBus.Error.NotSupported", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.LimitsExceeded".
 */
struct DXXAPI ErrorLimitsExceeded : public Error
{
	ErrorLimitsExceeded(const char *message)
	: Error("org.freedesktop.DBus.Error.LimitsExceeded", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.AccessDenied".
 */
struct DXXAPI ErrorAccessDenied : public Error
{
	ErrorAccessDenied(const char *message)
	: Error("org.freedesktop.DBus.Error.AccessDenied", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.AuthFailed".
 */
struct DXXAPI ErrorAuthFailed : public Error
{
	ErrorAuthFailed(const char *message)
	: Error("org.freedesktop.DBus.Error.AuthFailed", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NoServer".
 */
struct DXXAPI ErrorNoServer : public Error
{
	ErrorNoServer(const char *message)
	: Error("org.freedesktop.DBus.Error.NoServer", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Timeout".
 */
struct DXXAPI ErrorTimeout : public Error
{
	ErrorTimeout(const char *message)
	: Error("org.freedesktop.DBus.Error.Timeout", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.NoNetwork".
 */
struct DXXAPI ErrorNoNetwork : public Error
{
	ErrorNoNetwork(const char *message)
	: Error("org.freedesktop.DBus.Error.NoNetwork", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.AddressInUse".
 */
struct DXXAPI ErrorAddressInUse : public Error
{
	ErrorAddressInUse(const char *message)
	: Error("org.freedesktop.DBus.Error.AddressInUse", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Disconnected".
 */
struct DXXAPI ErrorDisconnected : public Error
{
	ErrorDisconnected(const char *message)
	: Error("org.freedesktop.DBus.Error.Disconnected", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.InvalidArgs".
 */
struct DXXAPI ErrorInvalidArgs : public Error
{
	ErrorInvalidArgs(const char *message)
	: Error("org.freedesktop.DBus.Error.InvalidArgs", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.FileNotFound".
 */
struct DXXAPI ErrorFileNotFound : public Error
{
	ErrorFileNotFound(const char *message)
	: Error("org.freedesktop.DBus.Error.FileNotFound", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.UnknownMethod".
 */
struct DXXAPI ErrorUnknownMethod : public Error
{
	ErrorUnknownMethod(const char *message)
	: Error("org.freedesktop.DBus.Error.UnknownMethod", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.TimedOut".
 */
struct DXXAPI ErrorTimedOut : public Error
{
	ErrorTimedOut(const char *message)
	: Error("org.freedesktop.DBus.Error.TimedOut", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.MatchRuleNotFound".
 */
struct DXXAPI ErrorMatchRuleNotFound : public Error
{
	ErrorMatchRuleNotFound(const char *message)
	: Error("org.freedesktop.DBus.Error.MatchRuleNotFound", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.MatchRuleInvalid".
 */
struct DXXAPI ErrorMatchRuleInvalid : public Error
{
	ErrorMatchRuleInvalid(const char *message)
	: Error("org.freedesktop.DBus.Error.MatchRuleInvalid", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Spawn.ExecFailed".
 */
struct DXXAPI ErrorSpawnExecFailed : public Error
{
	ErrorSpawnExecFailed(const char *message)
	: Error("org.freedesktop.DBus.Error.Spawn.ExecFailed", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Spawn.ForkFailed".
 */
struct DXXAPI ErrorSpawnForkFailed : public Error
{
	ErrorSpawnForkFailed(const char *message)
	: Error("org.freedesktop.DBus.Error.Spawn.ForkFailed", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Spawn.ChildExited".
 */
struct DXXAPI ErrorSpawnChildExited : public Error
{
	ErrorSpawnChildExited(const char *message)
	: Error("org.freedesktop.DBus.Error.Spawn.ChildExited", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Spawn.ChildSignaled".
 */
struct DXXAPI ErrorSpawnChildSignaled : public Error
{
	ErrorSpawnChildSignaled(const char *message)
	: Error("org.freedesktop.DBus.Error.Spawn.ChildSignaled", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.Spawn.Failed".
 */
struct DXXAPI ErrorSpawnFailed : public Error
{
	ErrorSpawnFailed(const char *message)
	: Error("org.freedesktop.DBus.Error.Spawn.Failed", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.InvalidSignature".
 */
struct DXXAPI ErrorInvalidSignature : public Error
{
	ErrorInvalidSignature(const char *message)
	: Error("org.freedesktop.DBus.Error.InvalidSignature", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.UnixProcessIdUnknown".
 */
struct DXXAPI ErrorUnixProcessIdUnknown : public Error
{
	ErrorUnixProcessIdUnknown(const char *message)
	: Error("org.freedesktop.DBus.Error.UnixProcessIdUnknown", message)
	{}
};

/**
 * @brief C++ exception to signal the D-Bus error "org.freedesktop.DBus.Error.SELinuxSecurityContextUnknown".
 */
struct DXXAPI ErrorSELinuxSecurityContextUnknown : public Error
{
	ErrorSELinuxSecurityContextUnknown(const char *message)
	: Error("org.freedesktop.DBus.Error.SELinuxSecurityContextUnknown", message)
	{}
};

} /* namespace DBus */

#endif//__DBUSXX_ERROR_H
