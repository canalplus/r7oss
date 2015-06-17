/*
 * MediaPlayerPrivateWYMediaPlayer.cpp
 *
 *  Created on: 24 mars 2011
 *      Author: sroyer
 */

#include "config.h"
#include "MediaPlayerPrivateWYMediaPlayer.h"

#if ENABLE(VIDEO)

#if USE(WYMEDIAPLAYER)

#include "WYMediaPlayerLibrary.h"
#include "debug.h"
#include "TimeRanges.h"
#include "GraphicsContext.h"

using namespace WebCore;


//////////////////////////////////////////
// ImageWYMediaPlayer class
//////////////////////////////////////////
#include "GOwnPtr.h"
#include "BitmapImage.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(CAIRO)
#include <cairo.h>
#include <cairo-directfb.h>
#endif

class ImageWYMediaPlayer : public RefCounted<ImageWYMediaPlayer>
{
    public:
        static PassRefPtr<ImageWYMediaPlayer> createImage(void* p_pBuffer, int p_nWidth, int p_nHeight, eMediaPlayerPixelFormat p_eMediaPlayerPixelFormat);
        ~ImageWYMediaPlayer();

        PassRefPtr<BitmapImage> image()
        {
            ASSERT(m_image);
            return m_image.get();
        }

    private:
        RefPtr<BitmapImage> m_image;

#if PLATFORM(CAIRO)
        ImageWYMediaPlayer(void* p_pBuffer, int p_nWidth, int p_nHeight, cairo_format_t& cairoFormat);
#endif

};

PassRefPtr<ImageWYMediaPlayer> ImageWYMediaPlayer::createImage(void* p_pBuffer, int p_nWidth, int p_nHeight, eMediaPlayerPixelFormat p_eMediaPlayerPixelFormat)
{
    if (p_eMediaPlayerPixelFormat != eMPPF_RGB32)
    {
        WYTRACE_ERROR("(p_eMediaPlayerPixelFormat(%d) != eMPPF_RGB32)\n", (unsigned int)p_eMediaPlayerPixelFormat);
        return NULL;
    }

#if PLATFORM(CAIRO)
    cairo_format_t cairoFormat;
    cairoFormat = CAIRO_FORMAT_ARGB32;

    return adoptRef(new ImageWYMediaPlayer(p_pBuffer, p_nWidth, p_nHeight, cairoFormat));
#else
    return NULL;
#endif
}

#if PLATFORM(CAIRO)
ImageWYMediaPlayer::ImageWYMediaPlayer(void* p_pBuffer, int p_nWidth, int p_nHeight, cairo_format_t& cairoFormat)
    : m_image(0)
{
    cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)p_pBuffer, cairoFormat,
                                    p_nWidth, p_nHeight, cairo_format_stride_for_width(cairoFormat, p_nWidth));
    ASSERT(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS);
    m_image = BitmapImage::create(surface);
}
#endif

ImageWYMediaPlayer::~ImageWYMediaPlayer()
{
    if (m_image)
        m_image.clear();

    m_image = 0;
}
// ImageWYMediaPlayer class
//////////////////////////////////////////


CWYMediaPlayerLibrary               g_libraryWYMediaPlayer;
WYSmartPtr<IFactory>                g_spWYMediaPlayerFactory;
WYSmartPtr<IMediaPlayersManager>    g_spMediaPlayersManager;

bool MediaPlayerPrivateWYMediaPlayer::doWYMediaPlayerInit()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!g_libraryWYMediaPlayer.isLoaded())
    {
        std::string l_strLibraryFileName = "libwymediaplayerhelper.so";

        if (!g_libraryWYMediaPlayer.load(l_strLibraryFileName.c_str()))
        {
            WYTRACE_ERROR("Can not load library '%s'\n", l_strLibraryFileName.c_str());
            return false;
        }
    }
    if (g_spWYMediaPlayerFactory == NULL)
    {
        if (!g_libraryWYMediaPlayer.factory(&g_spWYMediaPlayerFactory))
        {
            WYTRACE_ERROR("(!g_libraryWYMediaPlayer.factory(&g_spWYMediaPlayerFactory))\n");
            return false;
        }
        if (g_spWYMediaPlayerFactory == NULL)
        {
            WYTRACE_ERROR("(g_spWYMediaPlayerFactory == NULL)\n");
            return false;
        }
    }
    if (g_spMediaPlayersManager == NULL)
    {
        if (!g_spWYMediaPlayerFactory->mediaPlayersManager(&g_spMediaPlayersManager))
        {
            WYTRACE_ERROR("(!g_spWYMediaPlayerFactory->mediaPlayersManager(&g_spMediaPlayersManager))\n");
            return false;
        }
        if (g_spMediaPlayersManager == NULL)
        {
            WYTRACE_ERROR("(g_spMediaPlayersManager == NULL)\n");
            return false;
        }
    }
    return true;
}

bool MediaPlayerPrivateWYMediaPlayer::isAvailable()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    return doWYMediaPlayerInit();
}

void MediaPlayerPrivateWYMediaPlayer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType);
}

void MediaPlayerPrivateWYMediaPlayer::getSupportedTypes(HashSet<String>& types)
{
    static  HashSet<String> l_setSupportedTypes;
    static  bool            l_bInitialized = false;

    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (g_spMediaPlayersManager == NULL)
    {
        WYTRACE_ERROR("(g_spMediaPlayersManager == NULL)\n");
        types = l_setSupportedTypes;
        return;
    }

    int l_nSupportedTypesCount = g_spMediaPlayersManager->supportedTypesCount();

    for (int i = 0 ; i < l_nSupportedTypesCount ; i++)
    {
        types.add(String(g_spMediaPlayersManager->supportedType(i)));
    }
    l_bInitialized = true;

    types = l_setSupportedTypes;
}

MediaPlayer::SupportsType MediaPlayerPrivateWYMediaPlayer::supportsType(const String& type, const String& codecs)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (g_spMediaPlayersManager == NULL)
    {
        WYTRACE_ERROR("(g_spMediaPlayersManager == NULL)\n");
        return MediaPlayer::IsNotSupported;
    }

    eMediaPlayerTypeSupport     l_eMediaPlayerTypeSupport = eMPTP_IsNotSupported;
    MediaPlayer::SupportsType   l_eSupportType = MediaPlayer::IsNotSupported;

    l_eMediaPlayerTypeSupport = g_spMediaPlayersManager->typeSupport(type.utf8().data(), codecs.utf8().data());

    switch (l_eMediaPlayerTypeSupport)
    {
    case eMPTP_IsNotSupported:
        l_eSupportType = MediaPlayer::IsNotSupported;
        break;
    case eMPTP_MayBeSupported:
        l_eSupportType = MediaPlayer::MayBeSupported;
        break;
    case eMPTP_IsSupported:
        l_eSupportType = MediaPlayer::IsSupported;
        break;
    default:
        WYTRACE_ERROR("%s:%s():%d : Unknown readyState value : l_eMediaPlayerTypeSupport = %d (converted to MediaPlayer::NotSupported;)\n", __FILE__, __FUNCTION__, __LINE__, (int)l_eMediaPlayerTypeSupport);
        l_eSupportType = MediaPlayer::IsNotSupported;
        break;
    }
    return l_eSupportType;
}

//
MediaPlayerPrivateWYMediaPlayer::MediaPlayerPrivateWYMediaPlayer(MediaPlayer* player)
    : m_webCorePlayer(player)
    , m_fillTimer(this, &MediaPlayerPrivateWYMediaPlayer::fillTimerFired)
    , m_threadCreator(currentThread())
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    init();
}

MediaPlayerPrivateWYMediaPlayer::~MediaPlayerPrivateWYMediaPlayer()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    uninit();
}

MediaPlayerPrivateInterface* MediaPlayerPrivateWYMediaPlayer::create(MediaPlayer* player)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    return new MediaPlayerPrivateWYMediaPlayer(player);
}

bool MediaPlayerPrivateWYMediaPlayer::init()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!doWYMediaPlayerInit())
    {
        WYTRACE_ERROR("(!doWYMediaPlayerInit())\n");
        return false;
    }
    DirectFBCreate(&m_pDirectFB);
    if (m_pDirectFB == NULL)
    {
        WYTRACE_ERROR("(m_pDirectFB == NULL)\n");
        return false;
    }
    if (m_spMediaPlayer == NULL)
    {
        // Get First Player
        if (!g_spMediaPlayersManager->createPlayer(&m_spMediaPlayer))
        {
            WYTRACE_ERROR("(!g_spMediaPlayersManager->createPlayer(&m_spMediaPlayer))\n");
            return false;
        }
        if (m_spMediaPlayer == NULL)
        {
            WYTRACE_ERROR("(m_spMediaPlayer == NULL)\n");
            return false;
        }
    }
    if (m_spWebkitMediaPlayer == NULL)
    {
        if (!m_spMediaPlayer->webkitInterface(&m_spWebkitMediaPlayer))
        {
            WYTRACE_ERROR("(!m_spMediaPlayer->webkitInterface(&m_spWebkitMediaPlayer))\n");
            return false;
        }
        if (m_spWebkitMediaPlayer == NULL)
        {
            WYTRACE_ERROR("(m_spWebkitMediaPlayer == NULL)\n");
            return false;
        }
        m_spWebkitMediaPlayer->setEventSink(this);
    }
    m_bNetworkStateChanged = false;
    m_bReadyStateChanged = false;
    m_bVolumeChanged = false;
    m_bMuteChanged = false;
    m_bTimeChanged = false;
    m_bSizeChanged = false;
    m_bRateChanged = false;
    m_bPlaybackStateChanged = false;
    m_bDurationChanged = false;

    m_fChangedVolume = 0.0f;
    m_bMutedValue = false;

    return true;
}

bool MediaPlayerPrivateWYMediaPlayer::uninit()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);

    m_fChangedVolume = 0.0f;
    m_bMutedValue = false;

    m_bNetworkStateChanged = false;
    m_bReadyStateChanged = false;
    m_bVolumeChanged = false;
    m_bMuteChanged = false;
    m_bTimeChanged = false;
    m_bSizeChanged = false;
    m_bRateChanged = false;
    m_bPlaybackStateChanged = false;
    m_bDurationChanged = false;

    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    if (m_spWebkitMediaPlayer)
    {
        m_spWebkitMediaPlayer->setEventSink(NULL);
    }
    m_spWebkitMediaPlayer.release();
    m_spMediaPlayer.release();

    if (m_pDirectFB)
    {
        m_pDirectFB->Release(m_pDirectFB);
        m_pDirectFB = NULL;
    }

    return true;
}

void MediaPlayerPrivateWYMediaPlayer::fillTimerFired(Timer<MediaPlayerPrivateWYMediaPlayer>*)
{
    if (!m_spWebkitMediaPlayer) return;

#if 0
    bool    l_bLoadingComplete = false;
    updateStates(l_bLoadingComplete);

    if (l_bLoadingComplete)
    {
        m_fillTimer.stop();
    }
#endif
}

// Player interface
void MediaPlayerPrivateWYMediaPlayer::load(const String& url)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->load(url.utf8().data());

    if (m_fillTimer.isActive())
        m_fillTimer.stop();
    m_fillTimer.startRepeating(0.2);
}

void MediaPlayerPrivateWYMediaPlayer::cancelLoad()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    m_spWebkitMediaPlayer->cancelLoad();
}

void MediaPlayerPrivateWYMediaPlayer::prepareToPlay()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->prepareToPlay();
}

void MediaPlayerPrivateWYMediaPlayer::play()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->play();
}

void MediaPlayerPrivateWYMediaPlayer::pause()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->pause();
}

bool MediaPlayerPrivateWYMediaPlayer::supportsFullscreen() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->supportsFullscreen();
}

bool MediaPlayerPrivateWYMediaPlayer::supportsSave() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->supportsSave();
}

IntSize MediaPlayerPrivateWYMediaPlayer::naturalSize() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return IntSize();

    IntSize l_sizeSize;

    int l_nWidth = 0;
    int l_nHeight = 0;
    if (m_spWebkitMediaPlayer->naturalSize(&l_nWidth, &l_nHeight))
    {
        l_sizeSize.setWidth(l_nWidth);
        l_sizeSize.setHeight(l_nHeight);
    }

    return l_sizeSize;
}

bool MediaPlayerPrivateWYMediaPlayer::hasVideo() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->hasVideo();
}

bool MediaPlayerPrivateWYMediaPlayer::hasAudio() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->hasAudio();
}

void MediaPlayerPrivateWYMediaPlayer::setVisible(bool p_bVisible)
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setVisible(p_bVisible);
}

float MediaPlayerPrivateWYMediaPlayer::duration() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0f;

    return m_spWebkitMediaPlayer->duration();
}

float MediaPlayerPrivateWYMediaPlayer::currentTime() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0f;

    return m_spWebkitMediaPlayer->currentTime();
}

void MediaPlayerPrivateWYMediaPlayer::seek(float time)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->seek(time);
}

bool MediaPlayerPrivateWYMediaPlayer::seeking() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->seeking();
}

float MediaPlayerPrivateWYMediaPlayer::startTime() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0f;

    return m_spWebkitMediaPlayer->startTime();
}

void MediaPlayerPrivateWYMediaPlayer::setRate(float p_fRate)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setRate(p_fRate);
}

void MediaPlayerPrivateWYMediaPlayer::setPreservesPitch(bool p_bPreservePitch)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setPreservesPitch(p_bPreservePitch);
}

bool MediaPlayerPrivateWYMediaPlayer::paused() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->paused();
}

void MediaPlayerPrivateWYMediaPlayer::setVolume(float p_fVolume)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setVolume(p_fVolume);
}

bool MediaPlayerPrivateWYMediaPlayer::supportsMuting() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->supportsMuting();
}

void MediaPlayerPrivateWYMediaPlayer::setMuted(bool p_bMuted)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setMuted(p_bMuted);
}

bool MediaPlayerPrivateWYMediaPlayer::hasClosedCaptions() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->hasClosedCaptions();
}

void MediaPlayerPrivateWYMediaPlayer::setClosedCaptionsVisible(bool p_bClosedCaptionsVisible)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setClosedCaptionsVisible(p_bClosedCaptionsVisible);
}

MediaPlayer::NetworkState MediaPlayerPrivateWYMediaPlayer::networkState() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return MediaPlayer::Empty;

    eMediaPlayerNetworkState    l_eMediaPlayerNetworkState = eMPNS_Empty;
    MediaPlayer::NetworkState   l_eNetworkState = MediaPlayer::Empty;

    l_eMediaPlayerNetworkState = m_spWebkitMediaPlayer->networkState();

    switch (l_eMediaPlayerNetworkState)
    {
    case eMPNS_Empty:
        l_eNetworkState = MediaPlayer::Empty;
        break;
    case eMPNS_Idle:
        l_eNetworkState = MediaPlayer::Idle;
        break;
    case eMPNS_Loading:
        l_eNetworkState = MediaPlayer::Loading;
        break;
    case eMPNS_Loaded:
        l_eNetworkState = MediaPlayer::Loaded;
        break;
    case eMPNS_FormatError:
        l_eNetworkState = MediaPlayer::FormatError;
        break;
    case eMPNS_NetworkError:
        l_eNetworkState = MediaPlayer::NetworkError;
        break;
    case eMPNS_DecodeError:
        l_eNetworkState = MediaPlayer::DecodeError;
        break;
    default:
        WYTRACE_ERROR("%s:%s():%d : Unknown networkState value : l_eMediaPlayerNetworkState = %d (converted to MediaPlayer::Empty;)\n", __FILE__, __FUNCTION__, __LINE__, (int)l_eMediaPlayerNetworkState);
        l_eNetworkState = MediaPlayer::Empty;
        break;
    }
    return l_eNetworkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateWYMediaPlayer::readyState() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return MediaPlayer::HaveNothing;

    eMediaPlayerReadyState  l_eMediaPlayerReadyState = eMPRS_HaveNothing;
    MediaPlayer::ReadyState l_eReadyState = MediaPlayer::HaveNothing;

    l_eMediaPlayerReadyState = m_spWebkitMediaPlayer->readyState();

    switch (l_eMediaPlayerReadyState)
    {
    case eMPRS_HaveNothing:
        l_eReadyState = MediaPlayer::HaveNothing;
        break;
    case eMPRS_HaveMetadata:
        l_eReadyState = MediaPlayer::HaveMetadata;
        break;
    case eMPRS_HaveCurrentData:
        l_eReadyState = MediaPlayer::HaveCurrentData;
        break;
    case eMPRS_HaveFutureData:
        l_eReadyState = MediaPlayer::HaveFutureData;
        break;
    case eMPRS_HaveEnoughData:
        l_eReadyState = MediaPlayer::HaveEnoughData;
        break;
    default:
        WYTRACE_ERROR("%s:%s():%d : Unknown readyState value : l_eMediaPlayerReadyState = %d (converted to MediaPlayer::HaveNothing;)\n", __FILE__, __FUNCTION__, __LINE__, (int)l_eMediaPlayerReadyState);
        l_eReadyState = MediaPlayer::HaveNothing;
        break;
    }
    return l_eReadyState;
}

float MediaPlayerPrivateWYMediaPlayer::maxTimeSeekable() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0f;

    return m_spWebkitMediaPlayer->maxTimeSeekable();
}

PassRefPtr<TimeRanges> MediaPlayerPrivateWYMediaPlayer::buffered() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    RefPtr<TimeRanges> timeRanges;

    timeRanges = TimeRanges::create();
    if (m_spWebkitMediaPlayer)
    {

        WYSmartPtr<ITimeRanges> l_spTimeRanges;
        if (m_spWebkitMediaPlayer->buffered(&l_spTimeRanges))
        {
            if (l_spTimeRanges == NULL)
            {
                WYTRACE_ERROR("(l_spTimeRanges == NULL)\n");
            }
            else
            {
                int     l_nTimeRangesCount = l_spTimeRanges->count();
                float   l_fStart = 0.0f;
                float   l_fEnd = 0.0f;
                for (int i = 0 ; i < l_nTimeRangesCount ; i++)
                {
                    if (!l_spTimeRanges->timeRange(i, &l_fStart, &l_fEnd))
                    {
                        WYTRACE_ERROR("(!l_spTimeRanges->timeRange(i(%d), &l_fStart, &l_fEnd))\n", i);
                    }
                    else
                    {
                        timeRanges->add(l_fStart, l_fEnd);
                    }
                }
            }
        }
        l_spTimeRanges.release();
    }

    return timeRanges.release();
}

unsigned MediaPlayerPrivateWYMediaPlayer::bytesLoaded() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);

    if (!m_spWebkitMediaPlayer) return 0;

    return m_spWebkitMediaPlayer->bytesLoaded();
}

unsigned MediaPlayerPrivateWYMediaPlayer::totalBytes() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);

    if (!m_spWebkitMediaPlayer) return 0;

    return m_spWebkitMediaPlayer->totalBytes();
}

void MediaPlayerPrivateWYMediaPlayer::setSize(const IntSize& p_sizeSize)
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();

    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setSize(p_sizeSize.width(), p_sizeSize.height());
}

void MediaPlayerPrivateWYMediaPlayer::setPreload(MediaPlayer::Preload p_ePreload)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    eMediaPlayerPreload l_eMediaPlayerPreload = eMPPL_None;
    switch (p_ePreload)
    {
    case MediaPlayer::None:
        l_eMediaPlayerPreload = eMPPL_None;
        break;
    case MediaPlayer::MetaData:
        l_eMediaPlayerPreload = eMPPL_MetaData;
        break;
    case MediaPlayer::Auto:
        l_eMediaPlayerPreload = eMPPL_Auto;
        break;
    default:
        WYTRACE_ERROR("%s:%s():%d : Unknown Preload value : p_ePreload = %d (converted to eMPPL_Auto)\n", __FILE__, __FUNCTION__, __LINE__, (int)p_ePreload);
        l_eMediaPlayerPreload = eMPPL_Auto;
        break;
    }
    m_spWebkitMediaPlayer->setPreload(l_eMediaPlayerPreload);
}

bool MediaPlayerPrivateWYMediaPlayer::hasAvailableVideoFrame() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->hasAvailableVideoFrame();
}

bool MediaPlayerPrivateWYMediaPlayer::canLoadPoster() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->canLoadPoster();
}

void MediaPlayerPrivateWYMediaPlayer::setPoster(const String& p_strPoster)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setPoster(p_strPoster.utf8().data());
}

bool MediaPlayerPrivateWYMediaPlayer::hasSingleSecurityOrigin() const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return false;

    return m_spWebkitMediaPlayer->hasSingleSecurityOrigin();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateWYMediaPlayer::movieLoadType()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return MediaPlayer::Unknown;

    eMediaPlayerMovieLoadType   l_eMediaPlayerMovieLoadType = eMPMLT_Unknown;
    MediaPlayer::MovieLoadType  l_eMovieLoadType = MediaPlayer::Unknown;

    l_eMediaPlayerMovieLoadType = m_spWebkitMediaPlayer->movieLoadType();

    switch (l_eMediaPlayerMovieLoadType)
    {
    case eMPMLT_Unknown:
        l_eMovieLoadType = MediaPlayer::Unknown;
        break;
    case eMPMLT_Download:
        l_eMovieLoadType = MediaPlayer::Download;
        break;
    case eMPMLT_StoredStream:
        l_eMovieLoadType = MediaPlayer::StoredStream;
        break;
    case eMPMLT_LiveStream:
        l_eMovieLoadType = MediaPlayer::LiveStream;
        break;
    default:
        WYTRACE_ERROR("%s:%s():%d : Unknown movieLoadType value : l_eMediaPlayerMovieLoadType = %d (converted to MediaPlayer::Unknown;)\n", __FILE__, __FUNCTION__, __LINE__, (int)l_eMediaPlayerMovieLoadType);
        l_eMovieLoadType = MediaPlayer::Unknown;
        break;
    }
    return l_eMovieLoadType;
}

void MediaPlayerPrivateWYMediaPlayer::prepareForRendering()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    updateStates();
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->prepareForRendering();
}

float MediaPlayerPrivateWYMediaPlayer::mediaTimeForTimeValue(float timeValue) const
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0f;

    return m_spWebkitMediaPlayer->mediaTimeForTimeValue(timeValue);
}

double MediaPlayerPrivateWYMediaPlayer::maximumDurationToCacheMediaTime() const
{
//    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return 0.0;

    return m_spWebkitMediaPlayer->maximumDurationToCacheMediaTime();
}

// Webkit specific
PlatformMedia MediaPlayerPrivateWYMediaPlayer::platformMedia() const
{
    return NoPlatformMedia;
}

void MediaPlayerPrivateWYMediaPlayer::paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r)
{
    paint(c, r);
}









void MediaPlayerPrivateWYMediaPlayer::paint(GraphicsContext* c, const IntRect& r)
{
    updateStates();
    if (c->paintingDisabled())
        return;

    if (!m_webCorePlayer->visible())
        return;
#if 1
    m_spWebkitMediaPlayer->beginPaint();
    void*   l_pBuffer = NULL;
    int     l_nFrameWidth = 0;
    int     l_nFrameHeight = 0;
    eMediaPlayerPixelFormat l_eMediaPlayerPixelFormat;

#if 1

#if PLATFORM(CAIRO)
#if 1
    IDirectFBSurface*   l_pDirectFBSurface = NULL;
    if (m_spWebkitMediaPlayer->frame(&l_pDirectFBSurface))
    {
        if (l_pDirectFBSurface)
        {
            cairo_surface_t*    l_pCairoSurface = NULL;
            l_pCairoSurface = cairo_directfb_surface_create(m_pDirectFB, l_pDirectFBSurface);
            if (l_pCairoSurface)
            {
                RefPtr<BitmapImage> l_image;
                l_image = BitmapImage::create(l_pCairoSurface);
                FloatRect rect(r.x(), r.y(), r.width(), r.height());
                c->clearRect(rect);
                c->drawImage(reinterpret_cast<Image*>(l_image.get()), ColorSpaceSRGB, r, CompositeCopy, false);
            }
            else
            {
                WYTRACE_ERROR("(l_pCairoSurface == NULL)\n");
            }

            l_pDirectFBSurface->Release(l_pDirectFBSurface);
            l_pDirectFBSurface = NULL;
        }
        else
        {
            WYTRACE_ERROR("(l_pDirectFBSurface == NULL)\n");
        }
    }
    else
    {
        // Draw a black rect (not transparent)
        FloatRect rect(r.x(), r.y(), r.width(), r.height());
        c->fillRect(r, Color(0, 0, 0), ColorSpaceSRGB);
    }

#else // PLATFORM(CAIRO)
    cairo_surface_t*    l_pCairoSurface = NULL;
    if (m_spWebkitMediaPlayer->frame(&l_pCairoSurface))
    {
        RefPtr<BitmapImage> l_image;
        if (l_pCairoSurface)
        {
            l_image = BitmapImage::create(l_pCairoSurface);
            FloatRect rect(r.x(), r.y(), r.width(), r.height());
            c->clearRect(rect);
            c->drawImage(reinterpret_cast<Image*>(l_image.get()), ColorSpaceSRGB, r, CompositeCopy, false);
        }
        else
        {
            WYTRACE_ERROR("(l_pCairoSurface == NULL)\n");
        }
    }
#endif // #if 1
#endif // PLATFORM(CAIRO)
#else
    if (m_spWebkitMediaPlayer->frame(&l_pBuffer, &l_nFrameWidth, &l_nFrameHeight, &l_eMediaPlayerPixelFormat))
    {
        // Create an image with this buffer
        RefPtr<ImageWYMediaPlayer> l_spImage = ImageWYMediaPlayer::createImage(l_pBuffer, l_nFrameWidth, l_nFrameHeight, l_eMediaPlayerPixelFormat);
        if (l_spImage)
        {
            c->drawImage(reinterpret_cast<Image*>(l_spImage->image().get()), ColorSpaceSRGB, r, CompositeCopy, false);
        }
    }
    else
    {
        // Draw a black rect (transparent)
        // Overlay mode
        FloatRect rect(r.x(), r.y(), r.width(), r.height());
        c->clearRect(rect);
    }
#endif // #if 1

    m_spWebkitMediaPlayer->endPaint();

#else
    // Overlay mode
    FloatRect rect(r.x(), r.y(), r.width(), r.height());
    c->clearRect(rect);

    // Convert position from frame coordinate space to screen coordinate space
    Frame* frame = mp->m_player->frameView() ? mp->m_player->frameView()->frame() : 0;
    if (frame)
    {
        frame->convertToRenderer
    }

    m_spWebkitMediaPlayer->paint((void*)c , r.x(), r.y(), r.width(), r.height());
#endif
}

void MediaPlayerPrivateWYMediaPlayer::updateStates()
{
    if (m_bNetworkStateChanged)
    {
        m_bNetworkStateChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->networkStateChanged();
    }
    if (m_bReadyStateChanged)
    {
        m_bReadyStateChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->readyStateChanged();
    }
    if (m_bVolumeChanged)
    {
        m_bVolumeChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->volumeChanged(m_fChangedVolume);
    }
    if (m_bMuteChanged)
    {
        m_bMuteChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->muteChanged(m_bMutedValue);
    }
    if (m_bTimeChanged)
    {
        m_bTimeChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->timeChanged();
    }
    if (m_bSizeChanged)
    {
        m_bSizeChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->sizeChanged();
    }
    if (m_bRateChanged)
    {
        m_bRateChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->rateChanged();
    }
    if (m_bPlaybackStateChanged)
    {
        m_bPlaybackStateChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->playbackStateChanged();
    }
    if (m_bDurationChanged)
    {
        m_bDurationChanged = false;
        WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
        if (m_webCorePlayer) m_webCorePlayer->durationChanged();
    }
}

// IWebkitMediaPlayerEventSink interface
void MediaPlayerPrivateWYMediaPlayer::networkStateChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->networkStateChanged();
    }
    else
    {
        m_bNetworkStateChanged = true;
    }

}

void MediaPlayerPrivateWYMediaPlayer::readyStateChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->readyStateChanged();
    }
    else
    {
        m_bReadyStateChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::volumeChanged(float p_fVolume)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    m_fChangedVolume = p_fVolume;
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->volumeChanged(m_bMutedValue);
    }
    else
    {
        m_bVolumeChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::muteChanged(bool p_bMuted)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    m_bMutedValue = p_bMuted;
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->muteChanged(m_bMutedValue);
    }
    else
    {
        m_bMuteChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::timeChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->timeChanged();
    }
    else
    {
        m_bTimeChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::sizeChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->sizeChanged();
    }
    else
    {
        m_bSizeChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::rateChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->rateChanged();
    }
    else
    {
        m_bRateChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::playbackStateChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->playbackStateChanged();
    }
    else
    {
        m_bPlaybackStateChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::durationChanged()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        if (m_webCorePlayer) m_webCorePlayer->durationChanged();
    }
    else
    {
        m_bDurationChanged = true;
    }
}

void MediaPlayerPrivateWYMediaPlayer::repaint()
{
    if (m_webCorePlayer) m_webCorePlayer->repaint();
}

float MediaPlayerPrivateWYMediaPlayer::rate()
{
    if (m_webCorePlayer) return m_webCorePlayer->rate();
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    WYTRACE_ERROR("(m_webCorePlayer == NULL) !!\n");
    return 1.0;
}

bool  MediaPlayerPrivateWYMediaPlayer::visible()
{
    if (m_webCorePlayer) return m_webCorePlayer->visible();
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    WYTRACE_ERROR("(m_webCorePlayer == NULL) !!\n");
    return false;
}

#endif // USE_WYMEDIAPLAYER

#endif // ENABLE(VIDEO)
