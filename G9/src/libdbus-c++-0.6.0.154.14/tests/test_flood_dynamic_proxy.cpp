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
#include <time.h>
#include <semaphore.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"
#include "test_flood.h"


#define SEM_NAME "dbuscxx_test"
#define DBUS_TEST_NUM 100


class DurationCheckThread : public Thread
{
public:
    void thread_routine() {
        time_t t1;
        time(&t1);
        m_continue = true;

        while (m_continue) {
            usleep(50000);
            time_t t2;
            time(&t2);
            if ((t2 - t1) > 10) {
                std::cout << "########### HERE IS THE BUG ##############################" << std::endl;
                std::cout << "########### 10 sec with no iteration .... #################" << std::endl;
                //((std::string *)(NULL))->c_str();
                CPPUNIT_ASSERT(false);
                while(true);
            }

        }
    }

    bool m_continue;
};



class testFloodDynamicProxy : public CppUnit::TestFixture
{
public:

  CPPUNIT_TEST_SUITE( testFloodDynamicProxy );
  CPPUNIT_TEST( test_dynamic_proxy );
  CPPUNIT_TEST_SUITE_END();

public:

    class MyFloodProxy : public floodProxy
    {
    public:
          MyFloodProxy(DBus::Connection &conn):floodProxy( conn) {
          }

          virtual ~MyFloodProxy() {

          }

          virtual void sign( const ::DBus::SignalMessage& sig ) {
              // std::cout << "A receive signal from B" << std::endl;
          }
    };

    virtual void setUp()
    {
        sem_t *sem;
        disp = NULL;
        dbus_connection = NULL;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid = fork();
        if (pid == 0) {
            ThreadedDispatcher dbus_dispatcher;
            DBus::default_dispatcher = & dbus_dispatcher;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();

            dbus_connection.request_name(DBUS_NAME);

            if (!dbus_connection.has_name(DBUS_NAME))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            floodAdaptor adapt(dbus_connection);

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }


            sem_close(sem);
            dbus_dispatcher.start();

            sleep(2);
            time_t t_start, t_current;
            time(&t_start);

            do {

                usleep(5000);
                //std::cout << "C send signal" << std::endl;
                adapt.sign(2);
                time(&t_current);
            } while ((t_current - t_start) < 600);

            dbus_dispatcher.join();

            _exit(0);
        } else {
            struct timespec ts;
            CPPUNIT_ASSERT_EQUAL(0, clock_gettime(CLOCK_REALTIME, &ts));
            ts.tv_sec += 3;
            CPPUNIT_ASSERT_EQUAL(0, sem_timedwait(sem, &ts));

            std::cout << "service D started" << std::endl;
            sem_close(sem);
        }

        disp = new ThreadedDispatcher();
        DBus::default_dispatcher = disp;

        dbus_connection = new DBus::Connection(DBus::Connection::SystemBus());
        dbus_connection->request_name(DBUS_NAME "proxy");

        if (!dbus_connection->has_name(DBUS_NAME "proxy"))
        {
            CPPUNIT_ASSERT("Unable to name DBUS connection");
        }

    }

    virtual void tearDown()
    {
        int status;
        if (pid == 0) {
            CPPUNIT_FAIL("The server might have raised an exception");
        }
        else {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }

        sleep(1);

        disp->leave();
        disp->join();

        delete disp;
        disp = NULL;
    }


protected:
    pid_t pid;
    ThreadedDispatcher *disp;
    DBus::Connection *dbus_connection;

    void test_dynamic_proxy() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;

        disp->start();

        MyFloodProxy * my_proxy = NULL;

        sleep(2);
        time_t t_start, t_current;
        time(&t_start);
        DurationCheckThread checkIterationDuration;

        do {
            checkIterationDuration.thread_start();
            my_proxy = new MyFloodProxy(*dbus_connection);
            //MyFloodProxy tmp_proxy(*dbus_connection);
            std::cout << "floodProxy created" << std::endl;
            usleep(500);
            // std::cout << "going to delete" << std::endl;
            delete my_proxy;
            my_proxy = NULL;
            //std::cout << "everything seems OK" << std::endl;
            checkIterationDuration.m_continue=false;
            checkIterationDuration.thread_join();
            std::cout << "loop" << std::endl;
            time(&t_current);
        } while ((t_current - t_start) < 20);

        std::cout << "THE END" << std::endl;
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION(testFloodDynamicProxy);

