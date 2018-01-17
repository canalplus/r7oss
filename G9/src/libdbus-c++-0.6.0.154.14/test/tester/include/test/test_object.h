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

#ifndef __TEST_OBJECT_H__
#define __TEST_OBJECT_H__

#include <test-adaptor.h>

#include <libwy/wylog.h>

namespace DBus {
	class Connection;
}

/**
 *
 */
class test_object 	 : public com::wyplay::asynctest::Tests,
						public DBus::IntrospectableAdaptor,
						public DBus::PropertiesAdaptor,
                        public DBus::ObjectManagerAdaptor,
						public DBus::ObjectAdaptor
{
public:
	/**
	 *
	 */
	test_object(DBus::Connection & aConnection, const char * path) :
		DBus::ObjectAdaptor(aConnection, path)
	{

	}

	/**
	 *
	 */
    void STBReboot()
    {
        wl_info("test_object::STBReboot");
    }

    /**
     *
     */
    void STBFactoryReset()
    {
        wl_info("test_object::STBFactoryReset");
    }

    /**
     *
     */
    void ECMReboot()
    {
        wl_info("test_object::ECMReboot");
    }

    /**
     *
     */
    void ECMFactoryReset()
    {
        wl_info("test_object::ECMFactoryReset");
    }

	/**
	 *
	 */
	bool SetUsageID(const int32_t& id)
	{
        wl_info("test_object::SetUsageId");

        //throw DBus::Error(com::wyplay::tdaemon::Error::SA_MA_FUT, "care este");
        return true;
	}

    /**
     *
     */
    virtual bool SetDownstreamFrequency(const int32_t& frequency)
    {
        wl_info("test_object::SetDownstreamFrequency");
        return false;
    }

    /**
     *
     */
    virtual void GetDownloadStatus(bool& result, uint32_t & statusValue)
    {
        wl_info("test_object::GetDownloadStatus");
        result = false;
        statusValue = 543;
    }

	/**
	 *
	 */
    std::vector< ::DBus::Struct< std::string, std::string > > GetProperties()
    {
        wl_info("test_object::GetProperties");
    	return std::vector< ::DBus::Struct< std::string, std::string > >();
    }

    /**
     *
     */
    std::vector< ::DBus::Struct< std::string, std::string > > GetDocsisProperties()
	{
        wl_info("test_objectLLGetDocsisProperties");
    	return std::vector< ::DBus::Struct< std::string, std::string > >();
	}

    /**
     *
     */
    virtual void TestSignals()
    {
        wl_info("test_object::TestSignals");
    }


    /**
     *
     */
    virtual void TestDict(const std::map< int32_t, ::DBus::Variant >& a1)
    {
        wl_info("%s: ....", __PRETTY_FUNCTION__);
    }


    /**
     *
     */
    virtual void on_get_property(DBus::InterfaceAdaptor & iface, const std::string & prop, DBus::Variant & v)
    {
        wl_info("%s: getting the property %s.%s", __PRETTY_FUNCTION__, iface.name().c_str(), prop.c_str());
    }

    /**
     *
     */
    virtual void on_set_property(DBus::InterfaceAdaptor & iface, const std::string & prop, const DBus::Variant & v)
    {
        wl_info("%s: setting the property %s.%s", __PRETTY_FUNCTION__, iface.name().c_str(), prop.c_str());
    }

private:
};

#endif /* __TEST_OBJECT_H__ */
