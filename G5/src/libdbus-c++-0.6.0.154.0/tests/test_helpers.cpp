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

#include <sstream>
#include <iomanip>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "dbus-c++/dbus.h"
#include "test_helpers.h"

Cond::Cond()
{
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);
}

Cond::~Cond()
{
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

void Cond::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void Cond::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

void Cond::signal()
{
	pthread_cond_signal(&m_cond);
}

void Cond::broadcast()
{
	pthread_cond_broadcast(&m_cond);
}

int Cond::wait()
{
	return pthread_cond_wait(&m_cond, &m_mutex);
}

int Cond::timedwait(unsigned int msecs)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	uint32_t nsec = ts.tv_nsec + (msecs % 1000) * 1000000;

	ts.tv_nsec = nsec % 1000000000;
	ts.tv_sec += msecs / 1000 + ((nsec >= 1000000000)? 1 : 0);
	return pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
}

Thread::Thread() : m_thread_state(NOTSTARTED)
{
}

Thread::~Thread()
{
	int rc;
	if (m_thread_state==RUNNING)
	{
		rc = pthread_cancel(m_thread_id);
		if (rc != 0)
		{
			std::cerr << "pthread_cancel failed, rc:" << rc;
			std::cerr << " (" <<strerror(rc) << ")" << std::endl;
		}
	}
}

ThreadedDispatcher::ThreadedDispatcher()
{
}

ThreadedDispatcher::~ThreadedDispatcher()
{
	if (getState() == Thread::RUNNING)
	{
		leave();
		join();
	}
}

void ThreadedDispatcher::enter()
{
	start();
}

int ThreadedDispatcher::start()
{
	if (_running)
		return -1;
	_running = true;
	return thread_start();
}

int ThreadedDispatcher::join()
{
	return thread_join();
}

void ThreadedDispatcher::thread_routine()
{
	while (_running)
	{
		do_iteration();
	}
}

/**
 * Create a new thread and run the thread_routine() virtual method.
 */
int Thread::thread_start()
{
	if (m_thread_state==RUNNING)
	{
		std::cerr << "thread already started" << std::endl;
		return -1;
	}
	int rc;

	rc = pthread_create(&m_thread_id, NULL, thread_routine_wrapper, static_cast<void*>(this));
	if (rc)
	{
		m_thread_state = LAUNCHFAILED;
		std::cerr << "pthread_create sigchld_thread failed, rc:";
		std::cerr << rc << " (" << strerror(rc) << ")" << std::endl;
		return -1;
	}
	m_thread_state = RUNNING;
	return 0;
}

/**
 * Wait for the previously started thread to exit.
 */
int Thread::thread_join()
{
	int rc;
	void* value_ptr;
	rc = pthread_join(m_thread_id, &value_ptr);
	if (rc)
	{
		std::cerr << "pthread_join failed, rc:";
		std::cerr << rc << " (" << strerror(rc) << ")" << std::endl;
		return -1;
	}
	return 0;
}

int Thread::kill(int sig)
{
	return pthread_kill(m_thread_id, sig);
}

/**
 * Wrapper calling the thread_routing virtual method.
 *
 * \param data instance of the Thread class asking to start a new thread
 * \return NULL
 */
void* Thread::thread_routine_wrapper(void* data)
{
	Thread* thread = static_cast<Thread*>(data);

	thread->thread_routine();
	thread->m_thread_state = FINISH;
	pthread_exit(0);
}

Thread::State Thread::getState()
{
	return m_thread_state;
}

Chrono::Chrono()
{
	clear();
}

void Chrono::clear()
{
	memset(m_times, 0, sizeof (struct timespec) * 2);
}

uint32_t Chrono::getMiliSecs() const {
	struct timespec ts;
	getTimespec(ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

Chrono::operator std::string () const {
	std::ostringstream oss;
	struct timespec ts;
	getTimespec(ts);

	oss.fill('0');
	oss << ts.tv_sec << "." << std::setw(9) << ts.tv_nsec;
	return oss.str();
}

std::ostream &operator<< (std::ostream &os, const Chrono &c) {
	return os << c.operator std::string();
}

Chrono &Chrono::operator += (const Chrono &c) {
	getTimespec(m_times[0]); // getTimespec must work on itself
	c.getTimespec(m_times[1]);
	uint32_t i = m_times[0].tv_nsec + m_times[1].tv_nsec;
	m_times[1].tv_nsec = i % 1000000000;
	m_times[1].tv_sec += m_times[0].tv_sec + ((i >= 1000000000) ? 1 : 0);

	memset(m_times, 0, sizeof(struct timespec));
	return *this;
}

void Chrono::getTimespec(struct timespec &out) const {
	if (m_times[1].tv_nsec < m_times[0].tv_nsec) {
		out.tv_sec = m_times[1].tv_sec - m_times[0].tv_sec - 1;
		out.tv_nsec = 1000000000 + m_times[1].tv_nsec - m_times[0].tv_nsec;
	}
	else {
		out.tv_sec = m_times[1].tv_sec - m_times[0].tv_sec;
		out.tv_nsec = m_times[1].tv_nsec - m_times[0].tv_nsec;
	}
}

void __attribute__ ((destructor)) dbusxx_valgrind_unload(void)
{
    DBus::shutdown();
}

/* vim:set noexpandtab sw=4 ts=4 sts=4: */
