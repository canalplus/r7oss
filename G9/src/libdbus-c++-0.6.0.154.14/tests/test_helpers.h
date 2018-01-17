/*
 *
 *
 *
 *  Copyright (C) 2010 Wyplay
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

#ifndef TEST_HELPERS_H_
#define TEST_HELPERS_H_

#include <pthread.h>
#include <iostream>
#include <string>
#include <time.h>
#include <vector>

#include "dbus-c++/eventloop-integration.h"

class Cond
{
public:
	Cond();
	~Cond();

	void lock();
	void unlock();
	void signal();
	void broadcast();
	int wait();
	int timedwait(unsigned int msecs);

private:
	pthread_mutex_t m_mutex;
	pthread_cond_t  m_cond;
};

class Thread
{
public:
	enum State
	{
		NOTSTARTED,
		RUNNING,
		FINISH,
		LAUNCHFAILED
	};

	Thread();

	virtual ~Thread();

	int thread_start();

	int thread_join();

	int kill(int sig);

	State getState();

protected:
	virtual void thread_routine() = 0;

private:
	static void* thread_routine_wrapper(void*);
	pthread_t m_thread_id;
	State m_thread_state;
};

class ThreadedDispatcher :
	public ::DBus::BusDispatcher,
	protected Thread
{
public:
	ThreadedDispatcher();
	virtual ~ThreadedDispatcher();

	/**
	 * @brief alias for start.
	 */
	void enter();

	int start();

	int join();

	bool is_running() const
	{
		return _running;
	}

	using Thread::kill;

protected:
	void thread_routine();
};

class Chrono
{
public:
	Chrono();

	inline void start() { clock_gettime(CLOCK_MONOTONIC, &m_times[0]); };
	inline void stop() { clock_gettime(CLOCK_MONOTONIC, &m_times[1]); };
	void clear();

	uint32_t getMiliSecs() const;
	operator std::string () const;
	void getTimespec(struct timespec &timespec) const;
	Chrono &operator += (const Chrono &);

protected:
	struct timespec m_times[2];
};

std::ostream &operator<< (std::ostream &, const Chrono &);

template <typename T>
std::ostream &operator << (std::ostream &oss, const std::vector<T> &vec)
{
	typename std::vector<T>::const_iterator it;
	bool first = true;

	oss << "[ ";
	for (it = vec.begin(); it != vec.end(); ++it)
	{
		if (first)
			first = false;
		else
			oss << ", ";
		oss << *it;
	}
	oss << " ]";
	return oss;
}

#endif /* TEST_HELPERS_H_ */
/* vim:set noexpandtab sw=4 ts=4 sts=4: */
