/*
** Copyright (C) 2012 Fargier Sylvain <fargier.sylvain@free.fr>
**
** This software is provided 'as-is', without any express or implied
** warranty.  In no event will the authors be held liable for any damages
** arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose,
** including commercial applications, and to alter it and redistribute it
** freely, subject to the following restrictions:
**
** 1. The origin of this software must not be misrepresented; you must not
**    claim that you wrote the original software. If you use this software
**    in a product, an acknowledgment in the product documentation would be
**    appreciated but is not required.
** 2. Altered source versions must be plainly marked as such, and must not be
**    misrepresented as being the original software.
** 3. This notice may not be removed or altered from any source distribution.
**
** test_proxy_creation.cpp
**
**        Created on: Feb 04, 2012
**   Original Author: sfargier
**
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

/**
 * @details
 * only a simple proxy creation test
 */
class testProxyCreat : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(testProxyCreat);
    CPPUNIT_TEST(test);
    CPPUNIT_TEST_SUITE_END();

public:

protected:
    void test()
    {
        ThreadedDispatcher disp;
        DBus::Connection conn = DBus::Connection::SystemBus(&disp);
        disp.start();

        for (int i = 0; i < DBUS_TEST_NUM; ++i)
        {
            Chrono c;

            c.start();
            floodProxy prox(conn);
            c.stop();
            std::cout << "proxy(" << i << ") created in " << std::string(c)
                << std::endl;
            CPPUNIT_ASSERT(c.getMiliSecs() < 1000);
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(testProxyCreat);
