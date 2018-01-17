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

#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"
#include "test_flood.h"

static void sigHandler(int sig __attribute__ ((unused))) {
}

class pipeDLDispatcher : public ThreadedDispatcher
{
public:
    pipeDLDispatcher()
    {}

    /*
     * The dispatcher clears the pipe by filling a 10 bytes buffer,
     * we're filling the pipe with exactly 10 bytes to provoque a read lock
     * (O_NONBLOCK read error)
     */
    void deadlock() {
        char buff[10];

        _running = false;

        ::memset(buff, 'a', 10);
        CPPUNIT_ASSERT_EQUAL(10, write(_fdunlock[1], buff, 10));
    }

    virtual void thread_routine() {
        ThreadedDispatcher::thread_routine();

        m_outcond.lock();
        m_outcond.broadcast();
        m_outcond.unlock();
    }

    bool thread_timedjoin(unsigned int msecs) {
        m_outcond.lock();
        Thread::State state = getState();

        if (state != Thread::FINISH)
            m_outcond.timedwait(msecs);

        state = getState();
        m_outcond.unlock();

        if (state == Thread::FINISH) {
            thread_join();
            return true;
        }
        else
            return false;
    }

protected:
    Cond m_outcond;
};

class testPipeDL : public CppUnit::TestFixture
{

  CPPUNIT_TEST_SUITE( testPipeDL );
  CPPUNIT_TEST( test );
  CPPUNIT_TEST_SUITE_END();

protected:

    void test() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        pipeDLDispatcher dbus_dispatcher;
        DBus::default_dispatcher = & dbus_dispatcher;
        DBus::Connection dbus_connection = DBus::Connection::SystemBus();

        struct sigaction act;
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = sigHandler;
        sigaction(SIGUSR1, &act, NULL);

        dbus_dispatcher.start();

        dbus_dispatcher.deadlock();
        bool timedjoin = dbus_dispatcher.thread_timedjoin(1000);

        if (!timedjoin) {
            dbus_dispatcher.kill(SIGUSR1);
            dbus_dispatcher.join();
            CPPUNIT_FAIL("The dispatcher deadlocks");
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testPipeDL);

