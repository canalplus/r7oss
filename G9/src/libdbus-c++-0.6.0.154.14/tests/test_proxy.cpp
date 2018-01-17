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
#define DBUS_TEST_NUM 100

class testProxy : public CppUnit::TestFixture
{

  CPPUNIT_TEST_SUITE( testProxy );
  CPPUNIT_TEST( test_proxy );
  CPPUNIT_TEST_SUITE_END();

public:

    virtual void setUp()
    {
    }

    virtual void tearDown()
    {
    }

protected:

    void test_proxy() {
        std::cout << "Starting " << __FUNCTION__ << std::endl;
        std::vector< pid_t > pids(10);

        static DBus::BusDispatcher dbus_dispatcher;
        DBus::default_dispatcher = & dbus_dispatcher;
        DBus::Connection dbus_connection = DBus::Connection::SystemBus();
        floodProxy prox(dbus_connection);

        dbus_dispatcher.enter();
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testProxy);

