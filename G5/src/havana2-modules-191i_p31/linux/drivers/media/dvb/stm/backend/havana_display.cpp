/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : havana_display.cpp
Author :           Julian

Implementation of the display module for havana.


Date        Modification                                    Name
----        ------------                                    --------
05-Sep-07   Created                                         Julian

************************************************************************/

#include "backend_ops.h"

#include "player.h"
#include "buffer_generic.h"
#include "manifestor_audio.h"
#include "manifestor_video.h"

#include "havana_player.h"
#include "havana_factory.h"
#include "havana_display.h"
#include "display.h"

//{{{  HavanaDisplay_c
HavanaDisplay_c::HavanaDisplay_c (void)
{
    //DISPLAY_DEBUG("\n");

    Manifestor          = NULL;
    DisplayDevice       = NULL;
    PlayerStreamType    = StreamTypeOther;
}
//}}}
//{{{  ~HavanaDisplay_c
HavanaDisplay_c::~HavanaDisplay_c (void)
{
    //DISPLAY_DEBUG("\n");

    if ((Manifestor != NULL) && (PlayerStreamType == StreamTypeVideo))
    {
        class Manifestor_Video_c*       VideoManifestor = (class Manifestor_Video_c*)Manifestor;
        VideoManifestor->CloseOutputSurface ();
    }

    if (Manifestor != NULL)
        delete Manifestor;

    Manifestor          = NULL;
    DisplayDevice       = NULL;
}
//}}}

//{{{  GetManifestor
//{{{  doxynote
/// \brief  access a manifestor - or create a new one if it doesn't exist
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaDisplay_c::GetManifestor  (class HavanaPlayer_c*           HavanaPlayer,
                                                char*                           Media,
                                                char*                           Encoding,
                                                unsigned int                    SurfaceId,
                                                class Manifestor_c**            Manifestor)
{
    HavanaStatus_t      Status  = HavanaNoError;

    //DISPLAY_DEBUG("%p\n", this->Manifestor);

    if (strcmp (Media, BACKEND_AUDIO_ID) == 0)
    {
        PlayerStreamType        = StreamTypeAudio;
        if (this->Manifestor == NULL)
            Status  = HavanaPlayer->CallFactory (Media, FACTORY_ANY_ID, StreamTypeAudio, ComponentManifestor, (void**)&this->Manifestor);
        if (Status == HavanaNoError)
        //{{{  Init audio manifestor
        {
            class Manifestor_Audio_c*               AudioManifestor = (class Manifestor_Audio_c*)this->Manifestor;
            const char*                             BackendMixer;
            ManifestorStatus_t                      ManifestorStatus;
            ManifestorParameterBlock_t              DecodeBufferParameters;
            ManifestorAudioParameterBlock_t         ManifestorParameters;
            class Mixer_Mme_c*                      AudioMixer;

            //         
            // find a mixer instance and register it with the manifestor
            //

        #ifndef CONFIG_DUAL_DISPLAY
            // we must unconditionally use mixer0 until SurfaceId has been split into a Stream and Substream
            // (e.g. /dev/dvb/adapter<Stream>/audio<Substream> )...
            BackendMixer = BACKEND_MIXER0_ID;
        #else
            // ... unless we are running with the (temporary) dual display hacks
            BackendMixer = (DISPLAY_ID_REMOTE != SurfaceId ? BACKEND_MIXER0_ID : BACKEND_MIXER1_ID);
        #endif
            Status  = HavanaPlayer->CallFactory (Media, BackendMixer, StreamTypeAudio, ComponentExternal, (void**)&AudioMixer);
            if (Status != HavanaNoError)
                return Status;

            ManifestorParameters.ParameterType                      = ManifestorAudioMixerConfiguration;
            ManifestorParameters.Mixer                              = AudioMixer;

            ManifestorStatus        = AudioManifestor->SetModuleParameters (sizeof(ManifestorParameters), &ManifestorParameters);
            if (ManifestorStatus != ManifestorNoError )
            {
                HAVANA_ERROR("Failed to set manifestor mixer parameters (%08x)\n", ManifestorStatus);
                return HavanaNoMemory;
            }

            //
            // provide the rest of the manifestors configuration
            //

            DecodeBufferParameters.ParameterType                                = ManifestorBufferConfiguration;
            DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat      = FormatAudio;
            DecodeBufferParameters.BufferConfiguration.MaxBufferCount           = 32;
            DecodeBufferParameters.BufferConfiguration.TotalBufferMemory        = AUDIO_BUFFER_MEMORY;

            strcpy( DecodeBufferParameters.BufferConfiguration.PartitionName, "BPA2_Region1" );

            ManifestorStatus        = AudioManifestor->SetModuleParameters (sizeof(DecodeBufferParameters), &DecodeBufferParameters);
            if (ManifestorStatus != ManifestorNoError )
            {
                HAVANA_ERROR("Failed to set manifestor parameters (%08x)\n", ManifestorStatus);
                return HavanaNoMemory;
            }

            ManifestorStatus        = AudioManifestor->OpenOutputSurface ();
            if (ManifestorStatus != ManifestorNoError)
            {
                HAVANA_ERROR("Failed to create audio output surface\n");
                return HavanaNoMemory;
            }
        }
        //}}}
    }
    else if (strcmp (Media, BACKEND_VIDEO_ID) == 0)
    {
        class Manifestor_Video_c*                VideoManifestor;
        ManifestorStatus_t                       ManifestorStatus;
        ManifestorParameterBlock_t               DecodeBufferParameters;
        BufferLocation_t                         BufferLocation;
        const char                              *Partition;
        unsigned int                             PlaneId;
        unsigned int                             OutputId;

        PlayerStreamType        = StreamTypeVideo;
        if (GetDisplayInfo (SurfaceId, &DisplayDevice, &PlaneId, &OutputId, &BufferLocation) != 0)
        {
            HAVANA_ERROR("Failed to access display info\n");
            return HavanaError;
        }

        if (this->Manifestor == NULL)
        {
            Status  = HavanaPlayer->CallFactory (Media, FACTORY_ANY_ID, StreamTypeVideo, ComponentManifestor, (void**)&this->Manifestor);
            VideoManifestor             = (class Manifestor_Video_c*)this->Manifestor;
            if (Status == HavanaNoError)
            //{{{  Open display surface
            {
                ManifestorStatus        = VideoManifestor->OpenOutputSurface (DisplayDevice, PlaneId, OutputId);
                if (ManifestorStatus != ManifestorNoError)
                {
                    HAVANA_ERROR("Failed to create video output surface\n");
                    return HavanaNoMemory;
                }
            }
            //}}}
            else
            {
                HAVANA_ERROR("Failed to create video Manifestor\n");
                return HavanaNoMemory;
            }
        }

        VideoManifestor         = (class Manifestor_Video_c*)this->Manifestor;
        //{{{  Init video manifestor

        //
        // Configure the manifestor buffers
        //

        DecodeBufferParameters.ParameterType                                    = ManifestorBufferConfiguration;
        DecodeBufferParameters.BufferConfiguration.MaxBufferCount               = MAX_VIDEO_DECODE_BUFFERS;

        if( strcmp(Encoding,"dvp") == 0 )
            DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat      = FormatVideo422_Raster;
        if( strcmp(Encoding,"cap") == 0 )
            DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat      = FormatVideo565_RGB;

        else if( strcmp (Encoding, "mpeg4p2") == 0 ) // Divx
            DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat      = FormatVideo420_PairedMacroBlock;
        else
            DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat      = FormatVideo420_MacroBlock;

        if (BufferLocation == BufferLocationVideoMemory)
            Partition   = "BPA2_Region1";
        else if (BufferLocation == BufferLocationSystemMemory)
            Partition   = "BPA2_Region0";
        else
            Partition   = (SurfaceId == DISPLAY_ID_MAIN) ? "BPA2_Region1" : "BPA2_Region0";

        DecodeBufferParameters.BufferConfiguration.TotalBufferMemory    = (SurfaceId == DISPLAY_ID_MAIN) ?
                                                                                PRIMARY_VIDEO_BUFFER_MEMORY : 
                                                                                SECONDARY_VIDEO_BUFFER_MEMORY;

        if( strcmp (Encoding, "dvp") == 0 ) 
        {
            DecodeBufferParameters.BufferConfiguration.TotalBufferMemory = AVR_VIDEO_BUFFER_MEMORY;
        }

        if ( strcmp (Encoding, "cap") == 0 )
        {
            Partition   = "BPA2_Region1";
            DecodeBufferParameters.BufferConfiguration.TotalBufferMemory = AVR_VIDEO_BUFFER_MEMORY;
        }

        strcpy( DecodeBufferParameters.BufferConfiguration.PartitionName, Partition );

        if( strcmp (Encoding, "mpeg4p2") == 0 ) // Divx
        {
            //
            // Nick fudge, because divx currently doubles up the buffers,
            // and allocates full size raster buffers, we halve the memory,
            // and restrict them to 12 buffers. This stays in effect until the 
            // manifestor handles their native format.

            DecodeBufferParameters.BufferConfiguration.MaxBufferCount           /= 2;
            DecodeBufferParameters.BufferConfiguration.TotalBufferMemory        /= 2;
        }

        HAVANA_DEBUG("Will allocate upto %d buffers of type %d out of %08x bytes from the %s partition\n",
                      DecodeBufferParameters.BufferConfiguration.MaxBufferCount,
                      DecodeBufferParameters.BufferConfiguration.DecodedBufferFormat,
                      DecodeBufferParameters.BufferConfiguration.TotalBufferMemory,
                      Partition );

        ManifestorStatus    = VideoManifestor->SetModuleParameters (sizeof(ManifestorParameterBlock_t), &DecodeBufferParameters);
        if (ManifestorStatus != ManifestorNoError)
        {
            HAVANA_ERROR("Failed to set manifestor parameters (%08x)\n", ManifestorStatus);
            return HavanaNoMemory;
        }

        ManifestorStatus    = VideoManifestor->Enable ();
        if (ManifestorStatus != ManifestorNoError)
        {
            HAVANA_ERROR("Failed to enable video output\n");
            return HavanaError;
        }
        //}}}

    }
    else
        PlayerStreamType        = StreamTypeOther;
        if (this->Manifestor == NULL)
            Status  = HavanaPlayer->CallFactory (Media, FACTORY_ANY_ID, StreamTypeOther, ComponentManifestor, (void**)&this->Manifestor);

    *Manifestor     = this->Manifestor;
    return Status;
}
//}}}

