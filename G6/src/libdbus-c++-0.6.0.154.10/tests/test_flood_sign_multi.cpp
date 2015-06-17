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
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"
#include "test_flood.h"


#define SEM_NAME "dbuscxx_test"
#define DBUS_TEST_NUM 10
#define DBUS_ADAPT_NUM 10

static void sigHandler(int sig __attribute__ ((unused))) {
    if (DBus::default_dispatcher != NULL) {
        std::cout << "Stopping the dispatcher" << std::endl;
        DBus::default_dispatcher->leave();
    }
}

class floodProxySigWatch : public floodProxy
{
public:
    floodProxySigWatch(DBus::Connection &conn, const char *name) :
        floodProxy(conn, name), m_count(0)
    {}

    virtual ~floodProxySigWatch()
    {}

    virtual void sign(const ::DBus::SignalMessage &sig)
    {
        std::cout << "sign called" << std::endl;
        m_cond.lock();
        m_count++;
        m_cond.broadcast();
        m_cond.unlock();
    }

    unsigned int sigCount()
    {
        return m_count;
    }

    bool waitSigCount(unsigned int count, unsigned int timeout) {
        m_cond.lock();
        int rc = 0;

        while (m_count != count && rc == 0)
            rc = m_cond.timedwait(timeout);

        m_cond.unlock();

        return m_count == count;
    }

protected:
    unsigned int m_count;
    Cond m_cond;
};

class testFloodSignMulti : public CppUnit::TestFixture
{

  CPPUNIT_TEST_SUITE( testFloodSignMulti );
  CPPUNIT_TEST( test_signal );
  CPPUNIT_TEST_SUITE_END();

public:

  /**
   * @details
   * Fork and launch DBUS_ADAPT_NUM proxies that are all waiting for
   * DBUS_TEST_NUM signals.
   */
    virtual void setUp()
    {
        sem_t *sem;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid = fork();
        if (pid == 0) {
            ThreadedDispatcher disp;
            DBus::default_dispatcher = & disp;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();

            dbus_connection.request_name(DBUS_NAME);

            if (!dbus_connection.has_name(DBUS_NAME))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);
            std::vector< floodProxySigWatch * > proxys(DBUS_ADAPT_NUM);
            for (unsigned int i = 0; i < DBUS_ADAPT_NUM; ++i) {
                std::ostringstream oss;
                oss << DBUS_NAME << i;
                proxys[i] = new floodProxySigWatch(dbus_connection, oss.str().c_str());
            }
            std::cout << DBUS_ADAPT_NUM << " proxies created" << std::endl;

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }
            sem_close(sem);

            disp.start();

            int ret = 0;
            for (unsigned int i = 0; i < DBUS_ADAPT_NUM; ++i) {
                if (!proxys[i]->waitSigCount(DBUS_TEST_NUM, 30000)) {
                    ret = 1;
                    std::cout << "wrong sigCount(" << proxys[i]->sigCount()
                        << ") on proxy " << i << std::endl;
                    break;
                }
                std::cout << "finished to wait for proxy " << i << std::endl;
            }

            disp.leave();
            disp.join();

            for (unsigned int i = 0; i < DBUS_ADAPT_NUM; ++i) {
                delete proxys[i];
            }

            _exit(ret);
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
            waitpid(pid, &status, 0);
            CPPUNIT_ASSERT_EQUAL(0, status);
        }
    }

protected:
    pid_t pid;

    /**
     * @brief create DBUS_TEST_NUM adaptors in child processes and send
     * DBUS_TEST_NUM signals to the waiting proxies.
     */
    void test_signal() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        std::vector< pid_t > pids(DBUS_ADAPT_NUM);
        pid_t pid;
        unsigned int i;

        for (i = 0; i < DBUS_ADAPT_NUM; ++i) {
            if ((pid = fork()) == 0) {
                break;
            } else
                pids[i] = pid;
        }
        if (i != DBUS_ADAPT_NUM) {
            errno = 0;
            nice(-19);
            CPPUNIT_ASSERT(errno == 0);
            ThreadedDispatcher disp;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus(&disp);
            std::ostringstream oss;

            oss << DBUS_NAME;
            oss << i;
            floodAdaptor adapt(dbus_connection, oss.str().c_str());

            disp.start();

            std::cout << "Sending signals on adaptor " << oss.str() << std::endl;
            for (int i = 0; i < DBUS_TEST_NUM; ++i) {
                adapt.sign(i);
            }

            std::cout << "Signals sent on adaptor " << oss.str() << std::endl;
            disp.leave();
            disp.join();
            std::cout << "Leaving adaptor " << oss.str() << std::endl;

            _exit(0);
        }
        else {
            for (i = 0; i < DBUS_ADAPT_NUM; ++i) {
                int status;
                waitpid(pids[i], &status, 0);
                CPPUNIT_ASSERT_EQUAL(0, status);
            }
        }
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION(testFloodSignMulti);

