/*
 * MediaPlayerPrivateWYMediaPlayer.cpp
 *
 *  Created on: 24 mars 2011
 *      Author: sroyer
 */

#include "MediaPlayerPrivateWYMediaPlayer.h"

#if ENABLE(VIDEO)

#if USE(WYMEDIAPLAYER)

#include "WYMediaPlayerLibrary.h"
#include "debug.h"
#include "TimeRanges.h"
#include "GraphicsContext.h"
#include "../FrameView.h"

using namespace WebCore;

#ifdef ENABLE_DFB_SUPPORT
#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
#include "texmap/TextureMapper.h"
#endif
#endif //ENABLE_DFB_SUPPORT

//////////////////////////////////////////
// ImageWYMediaPlayer class
//////////////////////////////////////////

//#include "GOwnPtr.h"
#include <wtf/PassRefPtr.h>

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(CAIRO)
#include <cairo.h>
#include <cairo-directfb.h>
#endif
#endif // ENABLE_GLNEXUS_SUPPORT

#if PLATFORM(QT)
#include <QPixmap>
#endif

#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
#include <GLES2/gl2.h>
#include <QGLContext>
#endif // opengl + glnexus

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
#include <QImageReader>
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

#endif // ENABLE_DFB_SUPPORT


CWYMediaPlayerLibrary               g_libraryWYMediaPlayer;
WYSmartPtr<IFactory>                g_spWYMediaPlayerFactory;
WYSmartPtr<IMediaPlayersManager>    g_spMediaPlayersManager;
#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
bool                                g_bTraceEnabled = true;
#else
bool                                g_bTraceEnabled = false;
#endif // ENABLE_GLNEXUS_SUPPORT
bool                                g_bPreserveAspectRatio = true;

bool MediaPlayerPrivateWYMediaPlayer::doWYMediaPlayerInit()
{
    char*   l_pTraceEnabled = getenv("html5video_debug");
    g_bTraceEnabled = (l_pTraceEnabled != NULL);
    char*   l_pPreserveAspectRatio = getenv("html5video_preserveaspect");
    g_bPreserveAspectRatio = (l_pPreserveAspectRatio != NULL) ? std::string(l_pPreserveAspectRatio) != "false" : true;

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
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
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

MediaPlayer::SupportsType MediaPlayerPrivateWYMediaPlayer::supportsType(const String& type, const String& codecs, const WebCore::KURL&)
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
    , m_threadCreator(currentThread())
#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
    , m_pVideoItem(NULL)
#endif
#endif // ENABLE_DFB_SUPPORT
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    WYTRACE_DEBUG("this : %p\n", this);
    init();
}

MediaPlayerPrivateWYMediaPlayer::~MediaPlayerPrivateWYMediaPlayer()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    cancelCallOnMainThread(updateStatesCallback, this);
#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
    cancelCallOnMainThread(differedRepaint, this);
#endif // ENABLE_GLNEXUS_SUPPORT
    uninit();
}

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateWYMediaPlayer::create(MediaPlayer* player)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    return adoptPtr(new MediaPlayerPrivateWYMediaPlayer(player));
}

bool MediaPlayerPrivateWYMediaPlayer::init()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!doWYMediaPlayerInit())
    {
        WYTRACE_ERROR("(!doWYMediaPlayerInit())\n");
        return false;
    }
#ifdef ENABLE_DFB_SUPPORT
    DirectFBCreate(&m_pDirectFB);
    if (m_pDirectFB == NULL)
    {
        WYTRACE_ERROR("(m_pDirectFB == NULL)\n");
        return false;
    }
#endif // ENABLE_DFB_SUPPORT
    if (m_spMediaPlayer == NULL)
    {
        // Get First Player
        IMediaPlayer* l_pMediaPlayer;
        if (!g_spMediaPlayersManager->createPlayer(&l_pMediaPlayer))
        {
            WYTRACE_ERROR("(!g_spMediaPlayersManager->createPlayer(&m_spMediaPlayer))\n");
            return false;
        }
        m_spMediaPlayer = l_pMediaPlayer;
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

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
    m_pVideoItem = new QWYVideoItem(this);
#endif
#endif // ENABLE_DFB_SUPPORT

    return true;
}

bool MediaPlayerPrivateWYMediaPlayer::uninit()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);

#ifdef ENABLE_DFB_SUPPORT
#if PLATFORM(QT)
    if (m_pVideoItem)
    {
        delete m_pVideoItem;
        m_pVideoItem = NULL;
    }
#endif
#endif // ENABLE_DFB_SUPPORT

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

    if (m_spWebkitMediaPlayer)
    {
        m_spWebkitMediaPlayer->setEventSink(NULL);
    }
    m_spWebkitMediaPlayer.release();
    g_spMediaPlayersManager->deletePlayer(m_spMediaPlayer);
    m_spMediaPlayer->release();

#ifdef ENABLE_DFB_SUPPORT
    if (m_pDirectFB)
    {
        m_pDirectFB->Release(m_pDirectFB);
        m_pDirectFB = NULL;
    }
#endif // ENABLE_DFB_SUPPORT

    return true;
}

// Player interface
void MediaPlayerPrivateWYMediaPlayer::load(const String& url)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->load(url.utf8().data());
}

void MediaPlayerPrivateWYMediaPlayer::cancelLoad()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->cancelLoad();
}

void MediaPlayerPrivateWYMediaPlayer::prepareToPlay()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->prepareToPlay();
}

void MediaPlayerPrivateWYMediaPlayer::play()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->play();
}

void MediaPlayerPrivateWYMediaPlayer::pause()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
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
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setRate(p_fRate);
}

void MediaPlayerPrivateWYMediaPlayer::setPreservesPitch(bool p_bPreservePitch)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
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
    if (!m_spWebkitMediaPlayer) return;

    m_spWebkitMediaPlayer->setSize(p_sizeSize.width(), p_sizeSize.height());
}

void MediaPlayerPrivateWYMediaPlayer::setPreload(MediaPlayer::Preload p_ePreload)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
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
    PlatformMedia pm;
    pm.type = PlatformMedia::WYPLAYMediaPlayerType;
    pm.media.wyplayMediaPlayer = const_cast<MediaPlayerPrivateWYMediaPlayer*>(this);
    return pm;
}

void MediaPlayerPrivateWYMediaPlayer::paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r)
{
    paint(c, r);
}

#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
bool MediaPlayerPrivateWYMediaPlayer::supportsAcceleratedRendering() const
{
    return false;
}

void MediaPlayerPrivateWYMediaPlayer::acceleratedRenderingStateChanged()
{
}

PlatformLayer* MediaPlayerPrivateWYMediaPlayer::platformLayer() const
{
    return NULL;
}

#else // ENABLE_GLNEXUS_SUPPORT

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
void MediaPlayerPrivateWYMediaPlayer::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, BitmapTexture*) const
{
    //printf("%s:%s():%d : MediaPlayerPrivateWYMediaPlayer::paintToTextureMapper()\n", __FILE__, __FUNCTION__, __LINE__);
    GraphicsContext* context = textureMapper->graphicsContext();
    QPainter* painter = context->platformContext();
    painter->save();
    painter->setTransform(matrix);
    painter->setOpacity(opacity);
    IntRect r(targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height());
    renderVideoFrame(context, r);
    painter->restore();
}
#endif

RefPtr<BitmapImage> MediaPlayerPrivateWYMediaPlayer::bitmapImageFromDirectFBSurface(IDirectFBSurface* p_pDirectFBSurface) const
{
    RefPtr<BitmapImage> l_image;
    if (!p_pDirectFBSurface)
    {
        WYTRACE_ERROR("(!p_pDirectFBSurface)\n");
        return NULL;
    }
#if PLATFORM(CAIRO)
    cairo_surface_t*    l_pCairoSurface = NULL;
    l_pCairoSurface = cairo_directfb_surface_create(m_pDirectFB, p_pDirectFBSurface);
    if (!l_pCairoSurface)
    {
        WYTRACE_ERROR("(l_pCairoSurface == NULL)\n");
    }
    else
    {
        l_image = BitmapImage::create(l_pCairoSurface);
    }
#elif PLATFORM(QT)
    QPixmap* l_pPixmap = new QPixmap();

    (*l_pPixmap) = QPixmap::fromDirectFBSurface(p_pDirectFBSurface);

    l_image = BitmapImage::create(l_pPixmap);
#endif
    return l_image;
}

bool MediaPlayerPrivateWYMediaPlayer::renderVideoFrame(GraphicsContext* c, const IntRect& r) const
{
    IDirectFBSurface*   l_pDirectFBSurface = NULL;

    IntRect cTw(m_webCorePlayer->frameView()->contentsToWindow(r));

    if (m_spWebkitMediaPlayer->videoFrame(&l_pDirectFBSurface, cTw.x(), cTw.y(), cTw.width(), cTw.height()))
    {
        FloatRect rect(r.x(), r.y(), r.width(), r.height());
        c->clearRect(rect);
        if (l_pDirectFBSurface)
        {
            int l_nDirectFBSurfaceWidth;
            int l_nDirectFBSurfaceHeight;
            DFBResult   l_dfbResult;
            l_dfbResult = l_pDirectFBSurface->GetSize(l_pDirectFBSurface, &l_nDirectFBSurfaceWidth, &l_nDirectFBSurfaceHeight);
            if (l_dfbResult == DFB_OK)
            {
                RefPtr<BitmapImage> l_image = bitmapImageFromDirectFBSurface(l_pDirectFBSurface);
                if (l_image.get())
                {
                    IntRect l_rectDestinationArea = r;
                    IntRect  l_rectBB1;
                    IntRect  l_rectBB2;
                    bool    l_bAdaptToAspectRatio = g_bPreserveAspectRatio;
                    if (l_bAdaptToAspectRatio)
                    {
                        // Compute blit destination area
                        float   l_fGraphicsContextRatio = rect.width() / rect.height();
                        float   l_fDFBSurfaceRatio = ((float)l_nDirectFBSurfaceWidth) / ((float)l_nDirectFBSurfaceHeight);
                        if (l_fGraphicsContextRatio > l_fDFBSurfaceRatio)
                        {
                            l_rectDestinationArea.setWidth(l_fDFBSurfaceRatio * (float)r.height());
                            l_rectDestinationArea.setHeight(r.height());

                            l_rectBB1.setX(0);
                            l_rectBB1.setY(0);
                            l_rectBB1.setWidth((r.width() - l_rectDestinationArea.width()) / 2);
                            l_rectBB1.setHeight(l_rectDestinationArea.height());

                            l_rectDestinationArea.setX(l_rectBB1.x() + l_rectBB1.width());
                            l_rectDestinationArea.setY(0);

                            l_rectBB2.setX(l_rectDestinationArea.x() + l_rectDestinationArea.width());
                            l_rectBB2.setY(0);
                            l_rectBB2.setWidth(r.width() - (l_rectBB1.width() + l_rectDestinationArea.width()));
                            l_rectBB2.setHeight(l_rectDestinationArea.height());
                        }
                        else
                        {
                            l_rectDestinationArea.setWidth(r.width());
                            l_rectDestinationArea.setHeight((float)r.width() / l_fDFBSurfaceRatio);

                            l_rectBB1.setX(0);
                            l_rectBB1.setY(0);
                            l_rectBB1.setWidth(l_rectDestinationArea.width());
                            l_rectBB1.setHeight((r.height() - l_rectDestinationArea.height()) / 2);

                            l_rectDestinationArea.setX(0);
                            l_rectDestinationArea.setY(l_rectBB1.y() + l_rectBB1.height());

                            l_rectBB2.setX(0);
                            l_rectBB2.setY(l_rectDestinationArea.y() + l_rectDestinationArea.height());
                            l_rectBB2.setWidth(l_rectDestinationArea.width());
                            l_rectBB2.setHeight(r.height() - (l_rectBB1.height() + l_rectDestinationArea.height()));
                        }
                        l_rectBB1.setX(l_rectBB1.x() + r.x());
                        l_rectBB1.setY(l_rectBB1.y() + r.y());
                        l_rectDestinationArea.setX(l_rectDestinationArea.x() + r.x());
                        l_rectDestinationArea.setY(l_rectDestinationArea.y() + r.y());
                        l_rectBB2.setX(l_rectBB2.x() + r.x());
                        l_rectBB2.setY(l_rectBB2.y() + r.y());
                    }
                    c->fillRect(l_rectBB1, Color(1,0,0), ColorSpaceSRGB);
                    c->drawImage(reinterpret_cast<Image*>(l_image.get()), ColorSpaceSRGB, l_rectDestinationArea, CompositeCopy, false);
                    c->fillRect(l_rectBB2, Color(1,0,0), ColorSpaceSRGB);
                }
            }
            else
            {
                WYTRACE_ERROR("l_pDirectFBSurface->GetSize() FAILED (l_dfbResult = 0x%08X (%d))\n", l_dfbResult, l_dfbResult);
            }

            l_pDirectFBSurface->Release(l_pDirectFBSurface);
            l_pDirectFBSurface = NULL;
            return true;
        }
    }

    return false;
}

#endif // ENABLE_GLNEXUS_SUPPORT

void MediaPlayerPrivateWYMediaPlayer::paint(GraphicsContext* c, const IntRect& r)
{
    WYTRACE_DEBUG("Paint area : (%d, %d) - (%d x %d) disabled %i visible %i\n", r.x(), r.y(), r.width(), r.height(), c->paintingDisabled(), m_webCorePlayer->visible());
    if (c->paintingDisabled())
        return;

    if (!m_webCorePlayer->visible())
        return;

#ifdef ENABLE_DFB_SUPPORT
    renderVideoFrame(c, r);
#else // ENABLE_DFB_SUPPORT
    QPainter *l_pPainter = static_cast<QPainter*>(c->platformContext());

    if (m_spWebkitMediaPlayer != NULL)
    {
        GLuint textureId = 0;
        GLuint* pTex = &textureId;
        IntRect cTw(m_webCorePlayer->frameView()->contentsToWindow(r));
        bool result = m_spWebkitMediaPlayer->videoFrame((void**)&pTex, cTw.x(), cTw.y(), cTw.width(), cTw.height());
        if (result && pTex && *pTex)
        { // draw a captured frame
            l_pPainter->save();
            l_pPainter->scale(1.0, -1.0);
            QGLContext *l_pContext = const_cast<QGLContext*>(QGLContext::currentContext());
            l_pContext->drawTexture(QRectF(r.x(),-r.y()-r.height(), r.width(), r.height()), textureId, GL_TEXTURE_2D);
            l_pPainter->restore();
        }
        else  // draw black, either because we don't have any frame, or because of error
        {
            l_pPainter->save();
            l_pPainter->setCompositionMode (QPainter::CompositionMode_Clear);
            l_pPainter->fillRect ( QRectF(r.x(), r.y(), r.width(), r.height()), QColor(0,0,0,0));
            l_pPainter->restore();
        }
    }
    WYTRACE_DEBUG("OUT Paint area : (%d, %d) - (%d x %d) disabled %i visible %i\n", r.x(), r.y(), r.width(), r.height(), c->paintingDisabled(), m_webCorePlayer->visible());
#endif // ENABLE_DFB_SUPPORT
}

void MediaPlayerPrivateWYMediaPlayer::updateStatesCallback(void* p_thiz)
{
    MediaPlayerPrivateWYMediaPlayer* thiz = static_cast<MediaPlayerPrivateWYMediaPlayer*>(p_thiz);
    if (thiz)
        thiz->updateStates();
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
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
        callOnMainThread(updateStatesCallback, this);
    }
}

#if defined(ENABLE_GLNEXUS_SUPPORT) || defined(ENABLE_OPENGL_SUPPORT)
void MediaPlayerPrivateWYMediaPlayer::differedRepaint(void *p_thiz)
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    MediaPlayerPrivateWYMediaPlayer* thiz = static_cast<MediaPlayerPrivateWYMediaPlayer*>(p_thiz);
    if (thiz)
        thiz->doRepaint();
}

void MediaPlayerPrivateWYMediaPlayer::doRepaint()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
//  static bool ls_bFirstFrame = true;
//    if (ls_bFirstFrame)
//    {
//        if (m_webCorePlayer) m_webCorePlayer->firstVideoFrameAvailable();
//        ls_bFirstFrame = false;
//    }
//    else
//    {
        if (m_webCorePlayer) m_webCorePlayer->repaint();
//    }
}

void MediaPlayerPrivateWYMediaPlayer::repaint()
{
    WY_TRACK(MediaPlayerPrivateWYMediaPlayer);
    if (m_threadCreator == currentThread())
    {
        doRepaint();
    }
    else
    {
        callOnMainThread(differedRepaint, this);
    }
}
#else  // ENABLE_GLNEXUS_SUPPORT

#if PLATFORM(QT)
void MediaPlayerPrivateWYMediaPlayer::onRepaintAsked()
{
//    printf("%s:%s():%d : MediaPlayerPrivateWYMediaPlayer::onRepaintAsked()\n", __FILE__, __FUNCTION__, __LINE__);
    if (m_webCorePlayer) m_webCorePlayer->repaint();
}
void MediaPlayerPrivateWYMediaPlayer::repaint()
{
//    printf("%s:%s():%d : MediaPlayerPrivateWYMediaPlayer::repaint()\n", __FILE__, __FUNCTION__, __LINE__);
    if (m_pVideoItem) m_pVideoItem->notifyRepaint();
}
#else // PLATFORM(QT)
void MediaPlayerPrivateWYMediaPlayer::repaint()
{
//    printf("%s:%s():%d : MediaPlayerPrivateWYMediaPlayer::repaint()\n", __FILE__, __FUNCTION__, __LINE__);
    if (m_webCorePlayer) m_webCorePlayer->repaint();
}
#endif // PLATFORM(QT)
#endif // ENABLE_GLNEXUS_SUPPORT

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
