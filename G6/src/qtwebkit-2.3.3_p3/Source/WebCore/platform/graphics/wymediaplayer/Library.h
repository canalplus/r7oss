/*
 * Copyright (C) 2009 Wyplay, All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Wyplay and is confidential in nature.
 * Under no circumstances is this software to be exposed to or placed
 * under an Open Source License of any type without the expressed written
 * permission of Wyplay.
*/

#ifndef LIBRARY_H
#define LIBRARY_H

#include "config.h"

#if USE(WYMEDIAPLAYER)

#include <string>

/**
    @author Sébastien Royer <sroyer@ubuntu-goulkreek>
*/
class CLibrary
{
public:
    CLibrary();

    virtual ~CLibrary();

private:
    std::string	m_strFilePathName;
    void*	m_hLibrary;

public:
    virtual bool    load(const char* p_szLibraryFilePathName);
    virtual bool    unload();
	    void*   loadSymbol(const char* p_szSymbolName);
	    bool    isLoaded();
};

#endif // USE(WYMEDIAPLAYER)

#endif
