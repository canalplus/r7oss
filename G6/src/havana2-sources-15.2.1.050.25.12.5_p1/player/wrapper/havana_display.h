/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_HAVANA_DISPLAY
#define H_HAVANA_DISPLAY

#undef TRACE_TAG
#define TRACE_TAG   "HavanaDisplay_c"

#define AUDIO_BUFFER_MEMORY                     0x00080000       // 1/2   mb
#define MAX_VIDEO_DECODE_BUFFERS                32

/// Display wrapper class responsible for managing manifestors.
class HavanaDisplay_c
{
public:
    HavanaDisplay_c(void);
    ~HavanaDisplay_c(void);

    HavanaStatus_t              GetManifestor(class HavanaPlayer_c   *HavanaPlayer,
                                              class HavanaStream_c   *Stream,
                                              unsigned int            Capability,
                                              stm_object_h            DisplayHandle,
                                              class Manifestor_c    **Manifestor,
                                              stm_se_sink_input_port_t input_port = STM_SE_SINK_INPUT_PORT_PRIMARY);

    bool                        Owns(stm_object_h            DisplayHandle,
                                     class HavanaStream_c   *Stream);

    class Manifestor_c         *ReferenceManifestor(void) {return Manifestor;};

private:
    class Manifestor_c         *Manifestor;
    class HavanaStream_c       *Stream;
    PlayerStreamType_t          PlayerStreamType;
    stm_object_h                DisplayHandle;

    DISALLOW_COPY_AND_ASSIGN(HavanaDisplay_c);
};

/*{{{  doxynote*/
/*! \class      HavanaDisplay_c
    \brief      Controls access to and initialisation of the manifestor

*/

/*! \fn class Manifestor_c* HavanaDisplay_c::GetManifestor( void );
\brief Create and initialise a manifestor.

    Create a manifestor and initialise it based on the input parameters.

\param HavanaPlayer     Parent class
\param Media            Video or Audio
\param Encoding         Content type - determines some layout characteristic
\param SurfaceId        Where we are going to display the stream
\param Manifestor       Created manifestor

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn class Manifestor_c* HavanaDisplay_c::ReferenceManifestor( void );
\brief Use the manifestor that this class holds.

    Use the current manifestor, without perturbing it (ie setting parameters encoding etc...).

\return a pointer to a manifestor class instance.
*/



/*}}}*/

#endif

