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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"

#define SEM_NAME "dbuscxx_test"
#define DBUS_NAME "org.freedesktop.DBus.Timeout"
#define DBUS_PATH "/org/freedesktop/DBus/Tests/Timeout"

static void sigHandler(int sig __attribute__ ((unused))) {
    if (DBus::default_dispatcher != NULL) {
        std::cout << "Stopping the dispatcher" << std::endl;
        DBus::default_dispatcher->leave();
    }
}

class timeoutProxy :
    public ::DBus::InterfaceProxy,
    public ::DBus::ObjectProxy
{
public:
        timeoutProxy(DBus::Connection &conn, const char *name = DBUS_NAME, const char *path = DBUS_PATH) :
            ::DBus::InterfaceProxy(name),
            ::DBus::ObjectProxy(conn, path, name)
        {

        }
        ~timeoutProxy()
        { }

        ::DBus::Int32 mymeth()
        {
            ::DBus::CallMessage call;

            call.member("mymeth");
            ::DBus::Message ret = invoke_method(call);
            ::DBus::MessageIter ri = ret.reader();

            ::DBus::Int32 argout;
            ri >> argout;
            return argout;
        }
};

class timeoutAdaptor :
    public ::DBus::InterfaceAdaptor,
    public ::DBus::ObjectAdaptor
{
public:
        timeoutAdaptor(DBus::Connection &conn, const char *name = DBUS_NAME, const char *path = DBUS_PATH) :
            ::DBus::InterfaceAdaptor(name),
            ::DBus::ObjectAdaptor(conn, path)
        {
            register_method(timeoutAdaptor, mymeth, mymeth);
        }
private:
        ::DBus::Message mymeth(const ::DBus::CallMessage &call)
        {
            ::DBus::MessageIter ri = call.reader();
            ::DBus::Int32 argout1 = 0;
            ::DBus::ReturnMessage reply(call);
            ::DBus::MessageIter wi = reply.writer();

            /* 2min sleep should be enough to get a timeout */
            sleep(120);

            wi << argout1;
            return reply;
        }

};

class testTimeout : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( testTimeout );
    CPPUNIT_TEST( test_timeout );
    CPPUNIT_TEST( test_timeout_value );
    CPPUNIT_TEST_SUITE_END();

public:
    virtual void setUp()
    {
        sem_t *sem;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid = fork();
        if (pid == 0) {
            ThreadedDispatcher disp;
            DBus::default_dispatcher = &disp;
            DBus::Connection dbus_connection =
                DBus::Connection::SystemBus(&disp);

            disp.start();
            dbus_connection.request_name(DBUS_NAME);

            if (!dbus_connection.has_name(DBUS_NAME))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);
            timeoutAdaptor adapt(dbus_connection);

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }
            sem_close(sem);

            disp.join();
            _exit(0);
        } else {
            struct timespec ts;
            CPPUNIT_ASSERT_EQUAL(0, clock_gettime(CLOCK_REALTIME, &ts));
            ts.tv_sec += 3;
            CPPUNIT_ASSERT_EQUAL(0, sem_timedwait(sem, &ts));
            sem_close(sem);
        }
    }

    virtual void tearDown()
    {
        int status;

        if (pid == 0) {
            CPPUNIT_FAIL("The server might have raised an exception");
        }
        else {
            kill(pid, SIGTERM);
            waitpid(pid, &status, 0);
            CPPUNIT_ASSERT_EQUAL(0, status);
        }
    }

protected:
    pid_t pid;

    void test_timeout() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;

        ThreadedDispatcher disp;
        DBus::default_dispatcher = &disp;
        DBus::Connection dbus_connection = DBus::Connection::SystemBus(&disp);
        timeoutProxy prox(dbus_connection);

        disp.start();

        try {
            prox.mymeth();
            CPPUNIT_FAIL("A timeout exception should have been raised");
        }
        catch (DBus:: Error &e) {
            std::cout << "Request timed out: " << e.name() << std::endl;
            std::cout << e.what() << std::endl;
        }

        disp.leave();
        disp.join();
    }

    void test_timeout_value() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;

        ThreadedDispatcher disp;
        DBus::default_dispatcher = &disp;
        DBus::Connection dbus_connection = DBus::Connection::SystemBus();
        timeoutProxy prox(dbus_connection);

        disp.start();
        prox.set_timeout(2000);
        Chrono c;

        c.start();
        try {
            prox.mymeth();
            CPPUNIT_FAIL("A timeout exception should have been raised");
        }
        catch (DBus:: Error &e) {
            std::cout << "Request timed out: " << e.name() << std::endl;
            std::cout << e.what() << std::endl;
        }
        c.stop();
        CPPUNIT_ASSERT(c.getMiliSecs() < 3000);

        disp.leave();
        disp.join();
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testTimeout);
