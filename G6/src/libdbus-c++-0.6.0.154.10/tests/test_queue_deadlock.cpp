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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"
#include "test_flood.h"


#define SEM_NAME "dbuscxx_test"
#define DBUS_TEST_NUM 100
#define DBUS_PROXYS_NUM 20

static void sigHandler(int sig __attribute__ ((unused))) {
    if (DBus::default_dispatcher != NULL) {
        std::cout << "Stopping the dispatcher" << std::endl;
        DBus::default_dispatcher->leave();
    }
}

class floodQueueDLAdaptor :
    public ::DBus::InterfaceAdaptor,
    public ::DBus::ObjectAdaptor
{
public:
    floodQueueDLAdaptor(DBus::Connection &conn, const char *name = DBUS_NAME, const char *path = DBUS_PATH) :
        ::DBus::InterfaceAdaptor(name),
        ::DBus::ObjectAdaptor(conn, path)
    {
        register_method(floodQueueDLAdaptor, meth, meth);
    }

private:
    ::DBus::Message meth( const ::DBus::CallMessage& call )
    {
        ::DBus::MessageIter ri = call.reader();
        ::DBus::Int32 argin1; ri >> argin1;
        ::DBus::Int32 argout1 = 42; //m_proxy.meth(42);

        floodProxy tempProxy(conn(), DBUS_NAME, DBUS_PATH);
        ::DBus::ReturnMessage reply(call);
        ::DBus::MessageIter wi = reply.writer();
        wi << argout1;
        return reply;
    }

    void sign( const ::DBus::Int32& arg1 )
    {
        ::DBus::SignalMessage sig("sign");
        ::DBus::MessageIter wi = sig.writer();
        wi << arg1;
        emit_signal(sig);
    }

};

/**
 * @brief
 * An adaptor is flooded by many proxies calling a method.
 * The adaptor instantiate a proxy in it's method.
 *
 * This tests fails du to a bug in proxy creation (dbus_bus_add_match) that
 * timeouts internally in DBus.
 */
class testQueueDL : public CppUnit::TestFixture
{

  CPPUNIT_TEST_SUITE( testQueueDL );
  CPPUNIT_TEST( test );
  CPPUNIT_TEST_SUITE_END();

public:

    virtual void setUp()
    {
        sem_t *sem;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid = fork();
        if (pid == 0) {
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGTERM);
            pthread_sigmask(SIG_BLOCK, &mask, NULL);

            ThreadedDispatcher disp;
            DBus::default_dispatcher = & disp;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();
            disp.start();

            nice(-19);
            dbus_connection.request_name(DBUS_NAME);

            if (!dbus_connection.has_name(DBUS_NAME))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            floodQueueDLAdaptor radapt(dbus_connection, DBUS_NAME, DBUS_PATH);

            pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
            CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }
            sem_close(sem);

            disp.join();
            std::cout << "dispatcher died" << std::endl;

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
            CPPUNIT_ASSERT(WIFEXITED(status));
            CPPUNIT_ASSERT_EQUAL(0, WEXITSTATUS(status));
        }
    }

protected:
    pid_t pid;

    void test() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        std::vector< pid_t > pids(DBUS_PROXYS_NUM);
        pid_t pid;
        unsigned int i;

        for (i = 0; i < DBUS_PROXYS_NUM; ++i) {
            if ((pid = fork()) == 0) {
                break;
            } else
                pids[i] = pid;
        }
        if (i != DBUS_PROXYS_NUM) {
            try {
                ThreadedDispatcher disp;
                DBus::default_dispatcher = & disp;
                DBus::Connection dbus_connection = DBus::Connection::SystemBus();
                disp.start();

                floodProxy prox(dbus_connection);
                int j = i;
                for (int i = 0; i < DBUS_TEST_NUM; ++i) {
                    try {
                        prox.meth(j);
                    }
                    catch (DBus::Error &e) {
                        std::cerr << "Proxy failure " << j << ": " <<
                            e.what() << std::endl;
                        disp.leave();
                        disp.join();
                        _exit(-1);
                    }
                    if (!(i % 10))
                        std::cout << "process " << j << ": " << i << std::endl;
                }

                disp.leave();
                disp.join();
                _exit(0);
            }catch (std::exception &e) {
                std::cerr << "Unkown exception in proxy: " << e.what() << std::endl;
                _exit(-1);
            }
        }
        else {
            for (i = 0; i < DBUS_PROXYS_NUM; ++i) {
                int status;
                waitpid(pids[i], &status, 0);
                CPPUNIT_ASSERT(WIFEXITED(status));
                CPPUNIT_ASSERT_EQUAL(0, WEXITSTATUS(status));
            }
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testQueueDL);

