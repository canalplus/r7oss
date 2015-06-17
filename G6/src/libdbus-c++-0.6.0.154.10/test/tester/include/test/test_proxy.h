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
#ifndef __HAL_COMPUTER_PROXY_H__
#define __HAL_COMPUTER_PROXY_H__

#include "test-proxy.h"

#include <unistd.h>

namespace DBus {
	class Connection;
}

/**
 *
 */
class test_proxy : public com::wyplay::asynctest::Tests, public DBus::ObjectProxy
{
private:

    int signal_count;

public:
	/**
	 *
	 */
    test_proxy(DBus::Connection & connection, const DBus::Path & path, const char * service) :
		DBus::ObjectProxy(connection, path, service),
        signal_count(0)
	{}

    virtual void GetDownloadStatus_reply(bool & r1, uint32_t & r2, void * udata)
    {
        wl_info("test_proxy::GetDownloadStatus_reply r1(%d) r2(%d) udata(%p)", r1, r2, udata);
    }


    void STBReboot_reply(void * udata)
    {
        wl_info("test_proxy::STBReboot_reply: udata(%p)", udata);
    }

    void SetUsageID_reply(bool & res, void * udata)
    {
        wl_info("test_proxy::SetUsageID_reply res(%d) udata(%p)", res, udata);
    }

    virtual void ECMStatus(const int32_t& ecm_status, const ::DBus::UnixFD & fd)
    {
        wl_info("%s: ecm_status %d fd(%d)", __PRETTY_FUNCTION__, ecm_status, fd.get());
        {
            const char msg[] = "un mesaj scris din proxy intr-un fd primit din daemon\n";
            write(fd.get(), msg, sizeof(msg));
            close(fd.get());
        }
        signal_count++;
        if (0 == (signal_count%5)) {
            if (path() == "/com/wyplay/tdaemon/test" ) {
                change_path(DBus::Path("/com/wyplay/tdaemon/test/1"));
            }
            else {
                change_path(DBus::Path("/com/wyplay/tdaemon/test"));
            }
        }
    }

    virtual void ECMLinkStatus(const int32_t& ecm_link_status)
    {
        wl_info("%s: ecm_link_status %d", __PRETTY_FUNCTION__, ecm_link_status);
        signal_count++;
        if (0 == (signal_count%5)) {
            if (path() == "/com/wyplay/tdaemon/test" ) {
                change_path(DBus::Path("/com/wyplay/tdaemon/test/1"));
            }
            else {
                change_path(DBus::Path("/com/wyplay/tdaemon/test"));
            }
        }
    }
};



#endif /* __HAL_COMPUTER_PROXY_H__ */
