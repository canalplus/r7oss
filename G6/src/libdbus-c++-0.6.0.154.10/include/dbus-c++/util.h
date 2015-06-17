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


#ifndef __DBUSXX_UTIL_H
#define __DBUSXX_UTIL_H

#include <pthread.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cassert>

#include "api.h"
#include "debug.h"

namespace DBus {

/**
 * @cond INTERNAL_API
 */

/*
 *   Very simple reference counting
 */
class DXXAPI RefCnt
{
public:

	RefCnt()
	{
		__ref = new int;
		(*__ref) = 1;
	}

	RefCnt(const RefCnt &rc)
	{
		__ref = rc.__ref;
		ref();
	}

	virtual ~RefCnt()
	{
		unref();
	}

	RefCnt &operator = (const RefCnt &ref)
	{
		ref.ref();
		unref();
		__ref = ref.__ref;
		return *this;
	}

	bool noref() const
	{
		return (*__ref) == 0;
	}

	bool one() const
	{
		return (*__ref) == 1;
	}

private:

	DXXAPILOCAL void ref() const
	{
		++ (*__ref);
	}
	DXXAPILOCAL void unref() const
	{
		-- (*__ref);

		if ((*__ref) < 0)
		{
			debug_log("%p: refcount dropped below zero!", __ref);
		}

		if (noref())
		{
			delete __ref;
		}
	}

private:

	int *__ref;
};

/*
 *   Reference counting pointers (emulate boost::shared_ptr)
 */

template <class T>
class RefPtrI		// RefPtr to incomplete type
{
public:

	RefPtrI(T *ptr = 0);

	~RefPtrI();

	RefPtrI &operator = (const RefPtrI &ref)
	{
		if (this != &ref)
		{
			if (__cnt.one()) delete __ptr;

			__ptr = ref.__ptr;
			__cnt = ref.__cnt;
		}
		return *this;
	}

	T &operator *() const
	{
		return *__ptr;
	}

	T *operator ->() const
	{
		if (__cnt.noref()) return 0;
		
		return __ptr;
	}

	T *get() const
	{
		if (__cnt.noref()) return 0;
		
		return __ptr;
	}

private:

	T *__ptr;
	RefCnt __cnt;
};

template <class T>
class RefPtr
{
public:

	RefPtr(T *ptr = 0)
	: __ptr(ptr)
	{}

	~RefPtr()
	{
		if (__cnt.one()) delete __ptr;
	}

	RefPtr &operator = (const RefPtr &ref)
	{
		if (this != &ref)
		{
			if (__cnt.one()) delete __ptr;

			__ptr = ref.__ptr;
			__cnt = ref.__cnt;
		}
		return *this;
	}

	T &operator *() const
	{
		return *__ptr;
	}

	T *operator ->() const
	{
		if (__cnt.noref()) return 0;
		
		return __ptr;
	}

	T *get() const
	{
		if (__cnt.noref()) return 0;
		
		return __ptr;
	}

private:

	T *__ptr;
	RefCnt __cnt;
};
/**
 * @endcond
 */

/**
 * @class Callback_Base
 *
 * @brief Parameterizable two-argument callback wrapper interface.
 */

/*
 *   Typed callback template
 */

template <class R, class P, class U=void>
class Callback_Base
{
public:

    /**
     * @brief Declares how to call the callback.
     *
     * @param[in] param1 The first argument which is passed to the callback.
     * @param[in] param2 The second argument which is passed to the callback.
     */
	virtual R call(P param1, U param2) const = 0;

	/**
	 * @brief Destructor.
	 */
	virtual ~Callback_Base()
	{}
};

/**
 * @brief Parameterizable one-argument callback wrapper interface.
 */
template <class R, class P>
class Callback_Base<R, P, void>
{
public:

    /**
     * @brief Declares how to call the callback.
     *
     * @param[in] param The first argument which is passed to the callback.
     */
    virtual R call(P param) const = 0;

    /**
     * @brief Destructor.
     */
    virtual ~Callback_Base()
    {}
};

/**
 * @class Slot.
 *
 * @brief Helper class which facilitates the call of a parameterized two arguments callback.
 *
 * This class facilitates the call of a callback by offering the means to call a callback using
 * a standard function call syntax. It works in conjunction with a parameterized two arguments Callback.
 */
template <class R, class P, class U=void>
class Slot
{
public:

    /**
     * @name Initialize by assignment.
     *
     * The initialization works by assigning another Slot object, or a Callback_Base object.
     */
    /// @{
	Slot & operator= (Slot & s)
	{
		_cb = s._cb;
	}

	Slot & operator = (Callback_Base<R,P,U> * s)
	{
	    _cb = s;

	    return *this;
	}
	/// @}

	/**
	 * @name Callback call.
	 *
	 * There are two ways of calling the callback, using the function call syntax, or calling
	 * the call method. Both ways will require the two arguments the client intends to pass
	 * to the callback
	 */
	/// @{
	R operator()(P param1, U param2)
	{
	    if (0 != _cb.get())
	        return _cb->call(param1, param2);
	}

	R call(P param1, U param2)
	{
	    if (0 != _cb.get())
	        return _cb->call(param1, param2);
	}
	/// @}

	/**
	 * @brief Returns true if the Slot was initialized.
	 */
	bool empty() const
	{
		return (_cb.get() == 0);
	}

private:

	RefPtr< Callback_Base<R, P, U> > _cb;
};

/**
 * @brief Helper class which facilitates the call of a parameterized one argument callback.
 *
 * This class facilitates the call of a callback by offering the means to call a callback using
 * a standard function call syntax. It works in conjunction with a parameterized one argument Callback.
 */
template <class R, class P>
class Slot<R, P, void>
{
public:

    /**
     * @name Initialize by assignment.
     *
     * The initialization works by assigning another Slot object, or a Callback_Base object.
     */
    /// @{
    Slot & operator= (Slot & s)
    {
        _cb = s._cb;
    }

    Slot & operator = (Callback_Base<R,P> * s)
    {
        _cb = s;

        return *this;
    }
    /// @}

    /**
     * @name Callback call.
     *
     * There are two ways of calling the callback, using the function call syntax, or calling
     * the call method. Both ways will require the argument the client intends to pass
     * to the callback
     */
    /// @{
    R operator()(P param)
    {
        if (0 != _cb.get())
            return _cb->call(param);
    }

    R call(P param)
    {
        if (0 != _cb.get())
            return _cb->call(param);
    }
    /// @}

    /**
     * @brief Returns true if the Slot initialized.
     */
    bool empty() const
    {
        return (_cb.get() == 0);
    }

private:

    RefPtr< Callback_Base<R, P> > _cb;
};

/**
 * @class Callback
 *
 * @brief Two-argument callback wrapper implementation.
 */
template <class C, class R, class P, class U=void>
class Callback : public Callback_Base<R, P, U>
{
public:

    /**
     * @brief The prototype of the method accepted as a callback.
     */
    typedef R (C::*M)(P, U);

    /*
     * @brief Constructor.
     *
     * @param[in] c Pointer to the context object associated with the callback method.
     * @param[in] m Pointer to the method to be called by the callback.
     */
    Callback(C * c, M m) : _c(c), _m(m)
    {}

    /**
     * @brief Handles the callback call.
     *
     * @param[in] param1 First argument provided to the callback.
     * @param[in] param2 Second argument provided to the callback.
     */
    R call(P param1, U param2) const
    {
        return (_c->*_m)(param1, param2);
    }

private:
    C *_c;
    M _m;
};

/**
 * @brief One-argument callback wrapper implementation.
 */
template <class C, class R, class P>
class Callback<C, R, P, void> : public Callback_Base<R, P>
{
public:

    /**
     * @brief The prototype of the method accepted as a callback.
     */
    typedef R (C::*M)(P);

    /*
     * @brief Constructor.
     *
     * @param[in] c Pointer to the context object associated with the callback method.
     * @param[in] m Pointer to the method to be called by the callback.
     */
    Callback(C * c, M m) : _c(c), _m(m)
    {}

    /**
     * @brief Handles the callback call.
     *
     * @param[in] param First argument provided to the callback.
     */
    R call(P param) const
    {
        return (_c->*_m)(param);
    }

private:
    C *_c;
    M _m;
};

/**
 * @cond INTERNAL_API
 */
/// create std::string from any number
template <typename T>
std::string toString (const T &thing, int w = 0, int p = 0)
{
	std::ostringstream os;
	os << std::setw(w) << std::setprecision(p) << thing;
	return os.str();
}

class DXXAPI DefaultMutex
{
public:

  /*!
   * Constructor for non recursive Mutex
   */
  DefaultMutex();

  /*!
   * Constructor
   * \param recursive Set if Mutex should be recursive or not.
   */
    DefaultMutex(bool recursive);

    ~DefaultMutex();

    void lock();

    void unlock();

private:

    pthread_mutex_t _mutex;
};

class DXXAPI DefaultMutexLock
{
public:
  /*!
   * Constructor fo rmutex locker
   */
  DefaultMutexLock(DefaultMutex &);

  ~DefaultMutexLock();

private:
  DefaultMutex &_mutex;
};
/**
 * @endcond
 */

} /* namespace DBus */

#endif//__DBUSXX_UTIL_H
