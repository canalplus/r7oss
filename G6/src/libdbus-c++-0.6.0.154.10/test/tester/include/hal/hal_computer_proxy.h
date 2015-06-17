/*
 *
 *
 *
 *  Copyright (C) 2011 Wyplay
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

#ifndef __HAL_COMPUTER_PROXY_H__
#define __HAL_COMPUTER_PROXY_H__

#include "hal-proxy.h"

namespace DBus {
	class Connection;
}

/**
 *
 */
class hal_computer_proxy : public org::freedesktop::Hal::Device, public DBus::ObjectProxy
{
public:
	/**
	 *
	 */
	hal_computer_proxy(DBus::Connection & connection, const DBus::Path & path, const char * service) :
		DBus::ObjectProxy(connection, path, service)
	{}
};



#endif /* __HAL_COMPUTER_PROXY_H__ */
