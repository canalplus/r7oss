/*
 * Copyright (C) 2009 Wyplay, All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Wyplay and is confidential in nature.
 * Under no circumstances is this software to be exposed to or placed
 * under an Open Source License of any type without the expressed written
 * permission of Wyplay.
*/


#include "Library.h"

#if USE(WYMEDIAPLAYER)

#include <dlfcn.h>

#include <unistd.h>

#include "debug.h"

CLibrary::CLibrary()
{
	m_hLibrary = NULL;
}


CLibrary::~CLibrary()
{
	unload();
}


bool CLibrary::load(const char* p_szLibraryFilePathName)
{
	unload();

	m_hLibrary = dlopen(p_szLibraryFilePathName, RTLD_NOW);

	if (m_hLibrary == NULL)
	{
	    WYTRACE_ERROR("Can not open library '%s' (%s)\n", p_szLibraryFilePathName, dlerror());
		return false;
	}

	return true;
}

bool CLibrary::unload()
{
	if (m_hLibrary != NULL)
	{
		dlclose(m_hLibrary);
		m_hLibrary = NULL;
	}

	return true;
}

void* CLibrary::loadSymbol(const char* p_szSymbolName)
{
	if (m_hLibrary == NULL)
	{
		printf("(m_hLibrary == NULL)\n");
		return NULL;
	}
	return dlsym(m_hLibrary, p_szSymbolName);
}

bool CLibrary::isLoaded()
{
    return (m_hLibrary != NULL);
}

#endif // USE(WYMEDIAPLAYER)
