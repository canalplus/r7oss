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
#define DBUS_PROX_NUM 10
#define DBUS_TEST_NUM 100

static void sigHandler(int sig __attribute__ ((unused))) {
    if (DBus::default_dispatcher != NULL) {
        std::cout << "Stopping the dispatcher" << std::endl;
        DBus::default_dispatcher->leave();
    }
}

class testFloodMeth : public CppUnit::TestFixture
{

  CPPUNIT_TEST_SUITE( testFloodMeth );
  CPPUNIT_TEST( test_method );
  CPPUNIT_TEST_SUITE_END();

public:

    virtual void setUp()
    {
        sem_t *sem;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid = fork();
        if (pid == 0) {
            ThreadedDispatcher dbus_dispatcher;
            DBus::default_dispatcher = &dbus_dispatcher;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();

            dbus_dispatcher.start();

            errno = 0;
            nice(-19);
            CPPUNIT_ASSERT(errno == 0);
            dbus_connection.request_name(DBUS_NAME);

            if (!dbus_connection.has_name(DBUS_NAME))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);
            floodAdaptor adapt(dbus_connection);

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }
            sem_close(sem);

            dbus_dispatcher.join();

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

    void test_method() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        std::vector< pid_t > pids(DBUS_PROX_NUM);
        pid_t pid;
        unsigned int i;

        for (i = 0; i < DBUS_PROX_NUM; ++i) {
            if ((pid = fork()) == 0) {
                break;
            } else
                pids[i] = pid;
        }
        if (i != DBUS_PROX_NUM) {
            ThreadedDispatcher dbus_dispatcher;
            DBus::default_dispatcher = &dbus_dispatcher;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();
            floodProxy prox(dbus_connection);
            dbus_dispatcher.start();

            for (int i = 0; i < DBUS_TEST_NUM; ++i) {
                prox.meth(i);
            }

            dbus_dispatcher.leave();
            dbus_dispatcher.join();
            _exit(0);
        }
        else {
            for (i = 0; i < DBUS_PROX_NUM; ++i) {
                int status;
                waitpid(pids[i], &status, 0);
                CPPUNIT_ASSERT(WIFEXITED(status));
                CPPUNIT_ASSERT(WEXITSTATUS(status) == 0);
            }
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testFloodMeth);

