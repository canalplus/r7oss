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

#ifndef H_MIXER_REQUEST_CLASS
#define H_MIXER_REQUEST_CLASS

#undef TRACE_TAG
#define TRACE_TAG   "MixerRequest_c"

////////////////////////////////////////////////////////////////////////////////////
///
/// class MixerRequest_c:
///
/// (1) This class encapsulates ALL the different mixer request parameters.
/// It can be used by the mixer manager as an argument to the SendRequest()
/// method.
/// It would have been possible to create a generic MixerRequest_c class and
/// and derive from it one parameter class for each specific request. But it
/// would have meant that the the mixer manager should have stored the request
/// parameters as a pointer instead of a copied object which is less safer.
/// Hence the choice of having all the request parameters in one class.
/// This class provides thus setter and getter for the different parameters.
///
/// (2) This class also allows to specify the Mixer_Mme_c method to call when
/// managing the request. For this purpose it provides the
/// SetFunctionToManageTheRequest() and the GetFunctionToManageTheRequest().
///
/// (3) Example:
///
///     PlayerStatus_t status;
///     MixerRequest_c request;
///
///     request.SetInteractiveIdPter(InteractiveId);
///     request.SetFunctionToManageTheRequest(&Mixer_Mme_c::AllocInteractiveInput);
///
///     status = MixerRequestManager.SendRequest(request);
///     if(status != PlayerNoError)
///     {
///         SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
///     }
///
///     return status;
///
/// (4) To add a new request with a parameter of type TypeA for instance.
/// - define a private member:
///     TypeA mMyParameter;
/// - initialize this parameter in the MixerRequest_c constructor if necessary (pointer)
/// - define a getter and a setter for this parameter.
///
///
///
class MixerRequest_c
{
public:

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// A constructor.
    /// It simply initializes the data members.
    ///
    inline MixerRequest_c(void)
        : mIntResult(0)
        , mFunctionToManageTheRequest(0)
        , mAudioPlayer((Audio_Player_c *)(0))
        , mManifestor((Manifestor_AudioKsound_c *)(0))
        , mHavanaStream((HavanaStream_c *)(0))
        , mAudioGenerator((Audio_Generator_c *)(0))
        , mInputPort((stm_se_sink_input_port_t)(STM_SE_SINK_INPUT_PORT_PRIMARY))
    {}

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Define the pointer to the member function of Mixer_Mme_c.
    ///
    typedef PlayerStatus_t (Mixer_Mme_c::*FunctionToManageTheRequest)(MixerRequest_c &);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It sets the Mixer_Mme_c member function to be called to managed the
    /// the request.
    ///
    inline void SetFunctionToManageTheRequest(FunctionToManageTheRequest aFunctionToManageTheRequest)
    {
        mFunctionToManageTheRequest = aFunctionToManageTheRequest;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the Mixer_Mme_c member function to be called to managed the
    /// the request.
    ///
    inline FunctionToManageTheRequest GetFunctionToManageTheRequest(void)
    {
        return mFunctionToManageTheRequest;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It sets the audio player parameter to aAudioPlayer.
    ///
    inline void SetAudioPlayer(Audio_Player_c *aAudioPlayer)
    {
        mAudioPlayer = aAudioPlayer;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the audio player parameter.
    ///
    inline Audio_Player_c *GetAudioPlayer()
    {
        return mAudioPlayer;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It sets the manifestor parameter to aManifestor.
    ///
    inline void SetManifestor(Manifestor_AudioKsound_c *aManifestor)
    {
        mManifestor = aManifestor;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the the manifestor parameter.
    ///
    inline Manifestor_AudioKsound_c *GetManifestor()
    {
        return mManifestor;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It sets the havana stream parameter to aManifestor.
    ///
    inline void SetHavanaStream(HavanaStream_c *aHavanaStream)
    {
        mHavanaStream = aHavanaStream;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the the havana stream parameter.
    ///
    inline HavanaStream_c *GetHavanaStream()
    {
        return mHavanaStream;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It sets the AudioGenerator parameter to mAudioGenerator.
    ///
    inline void SetAudioGenerator(Audio_Generator_c *aGenerator)
    {
        mAudioGenerator = aGenerator;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the AudioGenerator parameter.
    ///
    inline Audio_Generator_c *GetAudioGenerator()
    {
        return mAudioGenerator;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the AudioGenerator parameter.
    ///
    inline void SetInputPort(stm_se_sink_input_port_t input_port)
    {
        mInputPort = input_port;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// It returns the AudioGenerator parameter.
    ///
    inline stm_se_sink_input_port_t GetInputPort()
    {
        return mInputPort;
    }


    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Genereric return integer value
    ///
    int mIntResult;

////////////////////////////////////////////////////////////////////////////////
///
///                              Private API
///
////////////////////////////////////////////////////////////////////////////////
private:

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The Mixer_Mme_c member function to be called to managed the the request.
    ///
    FunctionToManageTheRequest mFunctionToManageTheRequest;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The audio player parameter.
    ///
    Audio_Player_c *mAudioPlayer;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The manifestor parameter.
    ///
    Manifestor_AudioKsound_c *mManifestor;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The havana stream parameter.
    ///
    HavanaStream_c *mHavanaStream;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The AudioGenerator parameter.
    ///
    Audio_Generator_c *mAudioGenerator;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The input port parameter.
    ///
    stm_se_sink_input_port_t mInputPort;
};
#endif // H_MIXER_REQUEST_CLASS
