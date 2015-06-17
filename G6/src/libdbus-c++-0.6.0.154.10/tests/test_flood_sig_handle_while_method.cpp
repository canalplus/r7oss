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

static void sigHandler(int sig __attribute__ ((unused))) {
    if (DBus::default_dispatcher != NULL) {
        std::cout << "Stopping the dispatcher" << std::endl;
        DBus::default_dispatcher->leave();
    }
}


#define DBUS_NAME_A "org.freedesktop.DBus.FloodA"
#define DBUS_NAME_B "org.freedesktop.DBus.FloodB"
#define DBUS_NAME_C "org.freedesktop.DBus.FloodC"


//#include <wytools/threading/Thread.h>


class simpleThread : public Thread
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



class testFloodSigHandleFromMethod : public CppUnit::TestFixture
{
public:

    class floodProxyA : public floodProxy
    {
    public:
            floodProxyA(DBus::Connection &conn, const char *name = DBUS_NAME_B, const char *path = DBUS_PATH):floodProxy( conn, name, path, DBUS_NAME_B) {
            }

            virtual ~floodProxyA() {

            }

            virtual void sign( const ::DBus::SignalMessage& sig ) {
                std::cout << "A receive signal from B" << std::endl;
            }
    };


    class floodAdaptorUsingProxyB :
        public floodAdaptor
    {
        class floodProxyBtoC : public floodProxy
        {
        public:
                floodProxyBtoC(floodAdaptorUsingProxyB * adaptor_B, DBus::Connection &conn, const char *name = DBUS_NAME_C, const char *path = DBUS_PATH):floodProxy( conn, name, path, DBUS_NAME) {
                    m_adaptor_B = adaptor_B;

                    signal_sent = false;
                }

                virtual ~floodProxyBtoC() {

                }

                virtual void sign( const ::DBus::SignalMessage& sig ) {
                    // std::cout << "B received signal from C" << std::endl;
                    m_adaptor_B->sign(2);
                    // std::cout << "B forward signal to A" << std::endl;
                    signal_sent = true;
                }

            bool signal_sent;
        private:
                floodAdaptorUsingProxyB * m_adaptor_B;

        };

    public:
        floodAdaptorUsingProxyB(DBus::Connection &conn, const char *name = DBUS_NAME_B, const char *path = DBUS_PATH):floodAdaptor(conn, name, path), m_conn(conn) {

        }


    private:
        virtual ::DBus::Message meth( const ::DBus::CallMessage& call ) {

                simpleThread checkIterationDuration;
                for (int i=0; i < 10; i++) {
                    std::cout << "B call C => need a proxy" << std::endl;
                    try {
                        checkIterationDuration.thread_start();
                        floodProxyBtoC proxyBToC(this, m_conn);
                        std::cout << "B call C proxy OK" << std::endl;
                        proxyBToC.meth(1);
                        std::cout << "call OK" << std::endl;
                        checkCreateProxy();
                        std::cout << "exit catch" << std::endl;

                    }
                    catch (...) {
                        std::cout << "#######################################################" << std::endl;
                        std::cout << "#######################################################" << std::endl;
                        std::cout << "#######################################################" << std::endl;
                        std::cout << "#######################################################" << std::endl;
                        CPPUNIT_ASSERT(false);
                    }

                    std::cout << "everything seems OK" << std::endl;
                    checkIterationDuration.m_continue=false;

                    //checkIterationDuration.kill(SIGABRT);
                    checkIterationDuration.thread_join();
                    std::cout << "loop" << std::endl;
                }
                std::cout << "return :DBus::Message meth" << std::endl;

            return floodAdaptor::meth(call);
        }

        void checkCreateProxy() {
            time_t t1, t2;
            time(&t1);
            std::cout << "going to build proxy:" << std::endl;
            floodProxyBtoC proxybis(this, m_conn);
            time (&t2);
            std::cout << "duration for creating proxy:" << t2- t1 << std::endl;
            CPPUNIT_ASSERT(t2- t1 < 2);
        }


        DBus::Connection & m_conn;
    };

  CPPUNIT_TEST_SUITE( testFloodSigHandleFromMethod );
  CPPUNIT_TEST( test_signal_from_method );
  CPPUNIT_TEST_SUITE_END();

public:

    virtual void setUp()
    {
        sem_t *sem;

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 0);
        CPPUNIT_ASSERT(sem != SEM_FAILED);

        pid_C = fork();
        if (pid_C == 0) {
            ThreadedDispatcher disp;
            DBus::default_dispatcher = & disp;
            DBus::Connection dbus_connection = DBus::Connection::SystemBus();

            disp.start();
            dbus_connection.request_name(DBUS_NAME_C);

            if (!dbus_connection.has_name(DBUS_NAME_C))
            {
                CPPUNIT_ASSERT("Unable to name DBUS connection");
            }

            CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);
            floodAdaptor adapt(dbus_connection);

            if (sem_post(sem) != 0) {
                CPPUNIT_FAIL("failed to post semaphore");
            }


            sem_close(sem);

            sleep(2);

            // for (int i=0; i<DBUS_TEST_NUM *400; i++)
            while(true){
                // std::cout << "C send signal" << std::endl;
                usleep(5000);
                try {
                    adapt.sign(3);
                }
                catch(...) {
                    CPPUNIT_FAIL("Fail to post signal");
                }

                //
            }

            disp.join();

            _exit(0);
        } else {
            struct timespec ts;
            CPPUNIT_ASSERT_EQUAL(0, clock_gettime(CLOCK_REALTIME, &ts));
            ts.tv_sec += 3;
            CPPUNIT_ASSERT_EQUAL(0, sem_timedwait(sem, &ts));

            std::cout << "service D started" << std::endl;
            sem_close(sem);

            sem = sem_open(SEM_NAME "BIS", O_CREAT | O_RDWR, 0666, 0);
            CPPUNIT_ASSERT(sem != SEM_FAILED);

            pid_B = fork();
            if (pid_B == 0) {
                ThreadedDispatcher disp;
                DBus::default_dispatcher = & disp;
                DBus::Connection dbus_connection = DBus::Connection::SystemBus();
                floodProxyA proxyA(dbus_connection);

                disp.start();

                if (sem_post(sem) != 0) {
                    CPPUNIT_FAIL("failed to post semaphore");
                }

                sem_close(sem);

                sleep(1);

                while(true) {
                    std::cout << "proxyA call B" << std::endl;
                    try {
                        proxyA.meth(1);
                    }
                    catch (...) {
                        CPPUNIT_FAIL("Fail to call method");
                    }

                    std::cout << "proxyA.meth(1) return: " << std::endl;
                }

                sleep(600);

                disp.join();

                _exit(0);
            } else {
                struct timespec ts;
                CPPUNIT_ASSERT_EQUAL(0, clock_gettime(CLOCK_REALTIME, &ts));
                ts.tv_sec += 3;
                CPPUNIT_ASSERT_EQUAL(0, sem_timedwait(sem, &ts));
                std::cout << "service B started" << std::endl;
                sem_close(sem);
            }
        }

    }

    virtual void tearDown()
    {
        int status;

        if (pid_B == 0) {
            CPPUNIT_FAIL("The server might have raised an exception");
        }
        else {
            kill(pid_B, SIGKILL);
            waitpid(pid_B, &status, 0);
        }

        if (pid_C == 0) {
            CPPUNIT_FAIL("The server might have raised an exception");
        }
        else {
            kill(pid_C, SIGKILL);
            waitpid(pid_C, &status, 0);
        }

    }

protected:
    pid_t pid_B, pid_C;

    void test_signal_from_method() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        ThreadedDispatcher disp;
        DBus::default_dispatcher = & disp;
        DBus::Connection dbus_connection = DBus::Connection::SystemBus();

        dbus_connection.request_name(DBUS_NAME_B);

        if (!dbus_connection.has_name(DBUS_NAME_B))
        {
            CPPUNIT_ASSERT("Unable to name DBUS connection");
        }

        CPPUNIT_ASSERT(signal(SIGTERM, sigHandler) != SIG_ERR);
        floodAdaptorUsingProxyB adaptorB(dbus_connection);

        disp.start();

        for (int i=0; i<300; i++) {
            std::cout << "B is spleeping" << std::endl;
            sleep(3);
        }


        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;
        std::cout << "EXIT TEST" << std::endl;


        disp.leave();
        disp.join();
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION(testFloodSigHandleFromMethod);


