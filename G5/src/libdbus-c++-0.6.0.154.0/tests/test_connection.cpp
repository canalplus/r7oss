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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <pthread.h>

#include "dbus-c++/dbus.h"
#include "test_helpers.h"

void *dispatch_routine(void *data)
{
    DBus::BusDispatcher *disp = (DBus::BusDispatcher *) data;
    disp->enter();
}

class testConnection : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(testConnection);
  CPPUNIT_TEST(test_connection);
  CPPUNIT_TEST(test_loop_connection);
  CPPUNIT_TEST_SUITE_END();

public:

    virtual void setUp()
    {
        DBus::default_dispatcher = &dbus_dispatcher;
        CPPUNIT_ASSERT(pthread_create(&thread, NULL, dispatch_routine, &dbus_dispatcher) == 0);
    }

    virtual void tearDown()
    {
        dbus_dispatcher.leave();
        CPPUNIT_ASSERT(pthread_join(thread, NULL) == 0);
    }

protected:
    DBus::BusDispatcher dbus_dispatcher;
    pthread_t thread;

    void test_connection()
    {
        std::cout << "Starting " << __FUNCTION__ << std::endl;

        std::string bus_id;

        {
            DBus::Connection conn = DBus::Connection::SystemBus();

            bus_id = conn.unique_name();
            CPPUNIT_ASSERT(conn.has_name(bus_id.c_str()));
        }

        DBus::Connection conn = DBus::Connection::SystemBus();
        CPPUNIT_ASSERT(!conn.has_name(bus_id.c_str()));
    }

    void test_loop_connection()
    {
        std::cout << "Starting " << __FUNCTION__ << std::endl;

        for (int i = 0; i < 1000; ++i)
        {
            DBus::Connection conn = DBus::Connection::SystemBus();
        }
        std::cout << "done " << std::endl;
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testConnection);

