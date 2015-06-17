/*
 * MediaPlayerPrivateWYMediaPlayer.h
 *
 *  Created on: 24 mars 2011
 *      Author: sroyer
 */

#ifndef MEDIAPLAYERPRIVATEWYMEDIAPLAYER_H_
#define MEDIAPLAYERPRIVATEWYMEDIAPLAYER_H_

#include "config.h"

#if ENABLE(VIDEO)

#if USE(WYMEDIAPLAYER)

#include <wtf/Forward.h>
#include <wtf/Threading.h>
#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include "BitmapImage.h"

#ifdef ENABLE_DFB_SUPPORT
#include <directfb/directfb.h>
#endif // ENABLE_DFB_SUPPORT

#include <wymediaplayerhelper/wymediaplayerhelper.h>

#ifdef ENABLE_DFB_SUPPORT
#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
#include "TextureMapper.h"
#endif

#if PLATFORM(QT)
#include "QWYVideoItem.h"
#endif
#endif // ENABLE_DFB_SUPPORT

namespace WebCore {

#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
class WMPHLayer;
#endif

class MediaPlayerPrivateWYMediaPlayer :
    public MediaPlayerPrivateInterface,
    public IWebkitMediaPlayerEventSink
#ifdef ENABLE_DFB_SUPPORT
#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
        , public TextureMapperPlatformLayer
#endif
#endif // ENABLE_DFB_SUPPORT
{
public:
    MediaPlayerPrivateWYMediaPlayer(MediaPlayer* player);
    virtual ~MediaPlayerPrivateWYMediaPlayer();

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
    friend class QWYVideoItem;
private:
        void onRepaintAsked();
#endif
#endif // ENABLE_DFB_SUPPORT

private:
    MediaPlayer* m_webCorePlayer;

#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
#if USE(ACCELERATED_COMPOSITING)
    WMPHLayer *m_layer;
#endif
#endif // ENABLE_GLNEXUS_SUPPORT

    WYSmartPtr<IMediaPlayer>            m_spMediaPlayer;
    WYSmartPtr<IWebkitMediaPlayer>      m_spWebkitMediaPlayer;
#ifdef ENABLE_DFB_SUPPORT
    IDirectFB*                          m_pDirectFB;
#endif // ENABLE_DFB_SUPPORT

    bool    m_bNetworkStateChanged;
    bool    m_bReadyStateChanged;
    bool    m_bVolumeChanged;
    bool    m_bMuteChanged;
    bool    m_bTimeChanged;
    bool    m_bSizeChanged;
    bool    m_bRateChanged;
    bool    m_bPlaybackStateChanged;
    bool    m_bDurationChanged;

    ThreadIdentifier m_threadCreator;

    float   m_fChangedVolume;
    bool    m_bMutedValue;

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
    QWYVideoItem*   m_pVideoItem;
#endif
#endif // ENABLE_DFB_SUPPORT

protected:
    virtual bool init();
    virtual bool uninit();

#ifdef ENABLE_DFB_SUPPORT
            bool renderVideoFrame(GraphicsContext* c, const IntRect& r) const;
            RefPtr<BitmapImage> bitmapImageFromDirectFBSurface(IDirectFBSurface* p_pDirectFBSurface) const;
#else // ENABLE_DFB_SUPPORT
            void doRepaint();
#endif // ENABLE_DFB_SUPPORT

            void updateStates();

public:
            // player status change mng
            static void updateStatesCallback(void* thiz);
#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
            static void differedRepaint(void *thiz);
#endif // ENABLE_GLNEXUS_SUPPORT

public:
    static  PassOwnPtr<MediaPlayerPrivateInterface>    create(MediaPlayer* player);
    static  bool                            isAvailable();
    static  bool                            doWYMediaPlayerInit();
    static  void                            getSupportedTypes(HashSet<String>&);
    static  MediaPlayer::SupportsType       supportsType(const String& type, const String& codecs, const WebCore::KURL&);
    static  void                            registerMediaEngine(MediaEngineRegistrar);

public:
    virtual void load(const String& url);
    virtual void cancelLoad();

    virtual void prepareToPlay();
    virtual PlatformMedia platformMedia() const;
#if USE(ACCELERATED_COMPOSITING)
#ifdef ENABLE_DFB_SUPPORT
    virtual PlatformLayer* platformLayer() const { return 0; }
#if USE(TEXTURE_MAPPER)
    // Const-casting here is safe, since all of TextureMapperPlatformLayer's functions are const.g
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect& targetRect, const TransformationMatrix&, float opacity, BitmapTexture* mask) const;
#endif // USE(TEXTURE_MAPPER)
#else // ENABLE_DFB_SUPPORT
    virtual PlatformLayer* platformLayer() const;
#endif // ENABLE_DFB_SUPPORT
#endif

    virtual void play();
    virtual void pause();

    virtual bool supportsFullscreen() const;
    virtual bool supportsSave() const;

    virtual IntSize naturalSize() const;

    virtual bool hasVideo() const;
    virtual bool hasAudio() const;

    virtual void setVisible(bool);

    virtual float duration() const;

    virtual float currentTime() const;
    virtual void seek(float time);
    virtual bool seeking() const;

    virtual float startTime() const;

    virtual void setRate(float);
    virtual void setPreservesPitch(bool);

    virtual bool paused() const;

    virtual void setVolume(float);

    virtual bool supportsMuting() const;
    virtual void setMuted(bool);

    virtual bool hasClosedCaptions() const;
    virtual void setClosedCaptionsVisible(bool);

    virtual MediaPlayer::NetworkState networkState() const;
    virtual MediaPlayer::ReadyState readyState() const;

    virtual float maxTimeSeekable() const;
    virtual PassRefPtr<TimeRanges> buffered() const;

    virtual unsigned bytesLoaded() const;
    virtual unsigned totalBytes() const;

    virtual void setSize(const IntSize&);

    virtual void paint(GraphicsContext*, const IntRect&);

    virtual void paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r);

    virtual void setPreload(MediaPlayer::Preload);

    virtual bool hasAvailableVideoFrame() const;

    virtual bool canLoadPoster() const;
    virtual void setPoster(const String&);

#ifdef ENABLE_DFB_SUPPORT
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual void deliverNotification(MediaPlayerProxyNotificationType) = 0;
    virtual void setMediaPlayerProxy(WebMediaPlayerProxy*) = 0;
    virtual void setControls(bool) { }
    virtual void enterFullscreen() { }
    virtual void exitFullscreen() { }
#endif // ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#endif // ENABLE_DFB_SUPPORT

#if USE(ACCELERATED_COMPOSITING)
#ifdef ENABLE_DFB_SUPPORT

    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const { return false; }
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() { }

#else // ENABLE_DFB_SUPPORT

    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const;
     // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged();

#endif // ENABLE_DFB_SUPPORT
#endif // USE(ACCELERATED_COMPOSITING)

    virtual bool hasSingleSecurityOrigin() const;

    virtual MediaPlayer::MovieLoadType movieLoadType();

    virtual void prepareForRendering();

    // Time value in the movie's time scale. It is only necessary to override this if the media
    // engine uses rational numbers to represent media time.
    virtual float mediaTimeForTimeValue(float timeValue) const;

    // Overide this if it is safe for HTMLMediaElement to cache movie time and report
    // 'currentTime' as [cached time + elapsed wall time]. Returns the maximum wall time
    // it is OK to calculate movie time before refreshing the cached time.
    virtual double maximumDurationToCacheMediaTime() const;

    // IWebkitMediaPlayerEventSink interface
    virtual void networkStateChanged();
    virtual void readyStateChanged();
    virtual void volumeChanged(float p_fVolume);
    virtual void muteChanged(bool p_bMuted);
    virtual void timeChanged();
    virtual void sizeChanged();
    virtual void rateChanged();
    virtual void playbackStateChanged();
    virtual void durationChanged();
    virtual void repaint();
    virtual float rate();
    virtual bool  visible();
    bool didLoadingProgress() const { return 0; }
};

}

#endif // USE_WYMEDIAPLAYER

#endif // ENABLE(VIDEO)

#endif /* MEDIAPLAYERPRIVATEWYMEDIAPLAYER_H_ */
