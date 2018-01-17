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

#ifndef __CALLBACK_HPP
#define __CALLBACK_HPP

/**
 *
 */
class CallbackBase{
public:
	virtual void call(const char *) = 0;
};

/**
 *
 */
template<class T>
class Callback : public CallbackBase
{
public:
	typedef void (T::*METHOD)(const char *);

	/**
	 *
	 */
	Callback(T & anInstance, METHOD aMethod) : _instance(anInstance), _handle(aMethod)
	{
	}

	/**
	 *
	 */
	void call(const char * aStr)
	{
		(_instance.*_handle)(aStr);
	}

private:
	T & _instance;
	METHOD _handle;
};

#endif /* __CALLBACK_HPP */
