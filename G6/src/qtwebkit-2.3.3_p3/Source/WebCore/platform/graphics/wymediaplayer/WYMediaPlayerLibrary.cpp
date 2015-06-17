/*
 * WYMediaPlayerLibrary.cpp
 *
 *  Created on: 23 mars 2011
 *      Author: sroyer
 */


#include "WYMediaPlayerLibrary.h"

#if USE(WYMEDIAPLAYER)

#include <unistd.h>
#include <assert.h>
#include "debug.h"

CWYMediaPlayerLibrary::CWYMediaPlayerLibrary()
{
}

CWYMediaPlayerLibrary::~CWYMediaPlayerLibrary()
{
    // m_spFactory will be released in unload() (called by super class destructor)
}

bool CWYMediaPlayerLibrary::load(const char* p_szLibraryFilePathName)
{
    if (!CLibrary::load(p_szLibraryFilePathName))
    {
        WYTRACE_DEBUG("(!CLibrary::load('%s'))\n", p_szLibraryFilePathName);
        return false;
    }

    _proc_GetFactory l_GetFactory = (_proc_GetFactory)loadSymbol(WYMEDIAPLAYER_GET_FACTORY_FUNCTION_NAME);
    if (l_GetFactory == NULL)
    {
            WYTRACE_ERROR("Symbol '%s' function not found in '%s'\n", WYMEDIAPLAYER_GET_FACTORY_FUNCTION_NAME, p_szLibraryFilePathName);
            unload();
            return false;
    }
    assert(!m_spFactory);
    if (!l_GetFactory(&m_spFactory))
    {
            WYTRACE_ERROR("(!l_GetFactory(&m_spFactory))\n");
            unload();
            return false;
    }

    if (m_spFactory == NULL)
    {
        WYTRACE_ERROR("(m_spFactory == NULL)\n");
        unload();
        return false;
    }

    return true;
}

bool CWYMediaPlayerLibrary::unload()
{
    m_spFactory.release();

    return CLibrary::unload();
}

bool CWYMediaPlayerLibrary::factory(IFactory** p_ppFactory)
{
    if (p_ppFactory == NULL)
    {
        WYTRACE_ERROR("(p_ppFactory == NULL)\n");
        return false;
    }

    return m_spFactory.copyTo(p_ppFactory);
}

#endif // USE(WYMEDIAPLAYER)

