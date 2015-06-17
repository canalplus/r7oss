/*
 * WYMediaPlayerLibrary.h
 *
 *  Created on: 23 mars 2011
 *      Author: sroyer
 */

#ifndef WYMEDIAPLAYERLIBRARY_H_
#define WYMEDIAPLAYERLIBRARY_H_

#include "config.h"

#if USE(WYMEDIAPLAYER)

#include "Library.h"
#include <wymediaplayerhelper/wymediaplayerhelper.h>


class CWYMediaPlayerLibrary : public CLibrary
{
public:
    CWYMediaPlayerLibrary();
    virtual ~CWYMediaPlayerLibrary();

private:
    WYSmartPtr<IFactory>   m_spFactory;
public:
    virtual bool    load(const char* p_szLibraryFilePathName);
    virtual bool    unload();
            bool    factory(IFactory** p_ppFactory);
};

#endif // USE(WYMEDIAPLAYER)

#endif /* WYMEDIAPLAYERLIBRARY_H_ */
