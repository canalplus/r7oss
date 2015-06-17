/*
 * QWYVideoItem.cpp
 *
 *  Created on: 2 déc. 2011
 *      Author: sroyer
 */
#include "config.h"
#include "QWYVideoItem.h"

#ifdef ENABLE_DFB_SUPPORT
#include "MediaPlayerPrivateWYMediaPlayer.h"
#include <stdio.h>

using namespace WebCore;

QWYVideoItem::QWYVideoItem(MediaPlayerPrivateWYMediaPlayer* p_pMediaPlayerPrivateWYMediaPlayer, QObject* p_pParent) : QObject(p_pParent)
    , m_pMediaPlayerPrivateWYMediaPlayer(p_pMediaPlayerPrivateWYMediaPlayer)
{
    QObject::connect(this, SIGNAL(onRepaint()), this, SLOT(onRepaintAsked())/*, Qt::QueuedConnection*/);
}

QWYVideoItem::~QWYVideoItem()
{
    // TODO Auto-generated destructor stub
}

void QWYVideoItem::notifyRepaint()
{
    emit onRepaint();
}

void QWYVideoItem::onRepaintAsked()
{
    if (m_pMediaPlayerPrivateWYMediaPlayer)
    {
        m_pMediaPlayerPrivateWYMediaPlayer->onRepaintAsked();
    }
}
#endif // ENABLE_GLNEXUS_SUPPORT
