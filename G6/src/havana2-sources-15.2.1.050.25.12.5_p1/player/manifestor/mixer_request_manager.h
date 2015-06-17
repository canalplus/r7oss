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

#ifndef H_MIXER_REQUEST_MANAGER_CLASS
#define H_MIXER_REQUEST_MANAGER_CLASS

#include "mixer.h"
#include "mixer_requests.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Request_Manager_c"

#define DEFAULT_MIXER_REQUEST_MANAGER_TIMEOUT 5000

////////////////////////////////////////////////////////////////////////////////////
///
/// class Mixer_Request_Manager_c:
///
/// (1) This class provides a way for mixer client to send synchronous requests to
/// the playback thread. It has been created to solve bug 22324 and in general to
/// avoid some of the concurrent accesses to the shared resources (client states)
/// between the client and the playback thread.
/// Note that this manager accept only one request at a time (no request queue)
/// and if a thread (a client thread typically) tries to send a request while
/// another thread is waiting for a request to complete, then it will block.
///
///
/// (2) The typical usage is:
/// - in the playback thread at the very beginning: Start()
/// - in the playback thread: regularly call ManageTheRequest() to manage
///   the pending request
/// - a client thread sends a request and blocks until the request has been
///   completed:
///
///     PlayerStatus_t Mixer_Mme_c::SendMyRequest(MyType aMyRequestParameter)
///     {
///         PlayerStatus_t status;
///         MixerRequest_c request;
///
///         request.SetMyRequestParameter(aMyRequestParameter);
///         request.SetFunctionToManageTheRequest(&Mixer_Mme_c::MyRequestManagementFunction);
///
///         status = MixerRequestManager.SendRequest(request);
///         if(status != PlayerNoError)
///         {
///             SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
///         }
///
///         return status;
///     }
/// - before deleting the manager needs to call Stop().
///
///
/// (3) CAUTION: SendRequest(), the method that sends the request to the receiver thread
/// typically the playback thread), is a blocking one that sleeps until
/// the request has been managed by receiver thread (typically the playback thread).
/// So it cannot be used in a spin locked client code.
///
///
class Mixer_Request_Manager_c
{
////////////////////////////////////////////////////////////////////////////////
///
///                              Public API
///
////////////////////////////////////////////////////////////////////////////////
public:
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// A constructor.
    /// It performs several initialization. In particular the mutexes and
    /// the events. An assert is done if some initialization went wrong.
    ///
    ///
    inline Mixer_Request_Manager_c(void)
        : mRtMutex()
        , mPendingRequestEvent()
        , mPendingTerminationEvent()
        , mRequestCompletionEvent()
        , mTimeOut(0)
        , mError(false)
        , mThreadState(Idle)
        , mMixerRequest()
    {
        SetTimeOut(DEFAULT_MIXER_REQUEST_MANAGER_TIMEOUT);

        SE_DEBUG(group_mixer, "this = %p\n", this);

        OS_InitializeRtMutex(&mRtMutex);
        OS_InitializeEvent(&mPendingRequestEvent);
        OS_InitializeEvent(&mPendingTerminationEvent);
        OS_InitializeEvent(&mRequestCompletionEvent);
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Destructor.
    ///
    ///
    inline ~Mixer_Request_Manager_c()
    {
        Stop();
        OS_TerminateRtMutex(&mRtMutex);
        OS_TerminateEvent(&mPendingRequestEvent);
        OS_TerminateEvent(&mRequestCompletionEvent);
        OS_TerminateEvent(&mPendingTerminationEvent);

        SE_DEBUG(group_mixer, "this = %p\n", this);
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it waits (with a time out value of aTimeOut) for a
    /// a request to be pending. the timeout is not considered as an error.
    /// It returns an error if something went wrong in waiting.
    ///
    inline PlayerStatus_t WaitForPendingRequest(OS_Timeout_t aTimeOut)
    {
        PlayerStatus_t returnStatus = PlayerNoError;

        switch (mThreadState)
        {
        case Started:
        {
            if (!(OS_TestEventSet(&mPendingRequestEvent)))
            {
                OS_WaitForEventAuto(&mPendingRequestEvent, aTimeOut);
            }
            break;
        }
        case Terminating:
        {
            // mPendingRequestEvent was set during termination phase to exit the idle state
            // but there is no actual request to process
            SE_DEBUG(group_mixer, "Terminating...\n");
            // The Event mPendingTerminationEvent is set, and to avoid the Playback thread
            // idle state handler to send us back here before termination is taken into account,
            // we sleep a little bit
            OS_SleepMilliSeconds(10);
            break;
        }
        default:
        {
            SE_ERROR("Trying to wait for pending request whereas the request manager is not started\n");
            returnStatus = PlayerError;
            break;
        }
        }

        return returnStatus;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it stops the manager so that it cannot send request
    /// nor wait for them.
    ///
    /// Implementation comment:
    /// This function sets the completion and pending request events so as to
    /// unblock the thread that could be waiting on these events.
    ///
    inline void Stop()
    {
        mThreadState = Terminating;
        OS_SetEvent(&mPendingTerminationEvent);
        OS_SetEvent(&mPendingRequestEvent);
        OS_SetEvent(&mRequestCompletionEvent);
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it starts the manager so as to make it able to send,
    /// wait for and manage requests. Typically it can be called when
    /// the receiver thread is ready to accept requests.
    ///
    inline void Start()
    {
        mThreadState = Started;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it sets to aTimeOut the time out value for waiting the
    /// request completion.
    ///
    inline void SetTimeOut(OS_Timeout_t aTimeOut)
    {
        mTimeOut = aTimeOut;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it manages the pending request if there is one.
    /// That is to say: it calls the Mixer_Mme_c method that has been specified
    /// in the mMixerRequest argument of the SendRequest() previously called.
    /// It returns an error status.
    ///
    inline PlayerStatus_t ManageTheRequest(Mixer_Mme_c &aMixer)
    {
        PlayerStatus_t returnStatus = PlayerNoError;

        if (
            !OS_TestEventSet(&mPendingTerminationEvent)
            && OS_TestEventSet(&mPendingRequestEvent)
        )
        {
            SE_DEBUG(group_mixer, "@: There is a pending request (this = %p)\n", this);
            MixerRequest_c::FunctionToManageTheRequest functionToManageTheRequest;
            functionToManageTheRequest = mMixerRequest.GetFunctionToManageTheRequest();

            if (functionToManageTheRequest == 0)
            {
                SE_ERROR("Unspecified Mixer_Mme_c method to manage the request\n");
                returnStatus = PlayerError;
            }
            else
            {
                returnStatus = (aMixer.*functionToManageTheRequest)(mMixerRequest);

                if (returnStatus != PlayerNoError)
                {
                    SetError();
                }
            }

            mMixerRequest.SetFunctionToManageTheRequest(0);
            SE_DEBUG(group_mixer, "@: Set the completion event to signal that the request has been completed (this = %p)\n", this);
            OS_ResetEvent(&mPendingRequestEvent);
            OS_SetEvent(&mRequestCompletionEvent);
        }

        return returnStatus;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it sends the aMixerRequest request to whoever is listening
    /// for request.
    /// It returns an error status.
    /// aMixerRequest: it should be initialized properly in particular the
    /// Mixer_Mme_c method to be called when managing the request must have been
    /// set.
    /// CAUTION: this function locks, sets event and waits for event. So be sure that
    /// the thread that is calling this function is in a state that allows that:
    /// DO NOT USED this function in a spin locked code for instance.
    ///
    /// Implementation comments:
    /// After having checked that the manager is started and that the mixer method
    /// to call has been specified all right:
    /// - we lock the manager
    /// - we reset the request completion event
    /// - we set the pending request event
    /// - we wait for the request completion event
    /// - we unlock the manager.
    ///
    inline PlayerStatus_t SendRequest(MixerRequest_c &aMixerRequest)
    {
        SE_VERBOSE(group_mixer, ">: this = %p\n", this);
        PlayerStatus_t status = PlayerNoError;

        if (mThreadState == Started)
        {
            Lock();

            if (aMixerRequest.GetFunctionToManageTheRequest() == 0)
            {
                SE_ERROR("Trying to send a request with unspecified Mixer_Mme_c method to managed the request\n");
                status = PlayerNoError;
            }
            else
            {
                mMixerRequest = aMixerRequest;
                SE_DEBUG(group_mixer, "@: sending a request\n");
                OS_ResetEvent(&mRequestCompletionEvent);
                OS_SetEvent(&mPendingRequestEvent);
                SE_VERBOSE(group_mixer, "@: waiting completion of the request\n");
                status = WaitRequestCompletion();
                SE_DEBUG(group_mixer, "@: the request has been completed\n");
            }

            Unlock();
        }
        else
        {
            SE_ERROR("Trying to send a request whereas the request manager is not yet started\n");
            status = PlayerError;
        }

        SE_VERBOSE(group_mixer, "<: this = %p\n", this);
        return status;
    }

////////////////////////////////////////////////////////////////////////////////
///
///                              Private API
///
////////////////////////////////////////////////////////////////////////////////
private:
    ////////////////////////////////////////////////////////////////////////////
    /// Internal mutex to prevent different threads to set an event in parallel.
    ///
    OS_RtMutex_t mRtMutex;

    ////////////////////////////////////////////////////////////////////////////
    /// Internal event that must be set to tell that a request is pending.
    ///
    OS_Event_t mPendingRequestEvent;

    ////////////////////////////////////////////////////////////////////////////
    /// Internal event that must be set to tell that a termination is requested.
    ///
    OS_Event_t mPendingTerminationEvent;

    ////////////////////////////////////////////////////////////////////////////
    /// Internal event that tells that the request has been completed
    ///
    OS_Event_t mRequestCompletionEvent;

    ////////////////////////////////////////////////////////////////////////////
    /// The time out value for waiting the request completion.
    ///
    ///
    OS_Timeout_t mTimeOut;

    ////////////////////////////////////////////////////////////////////////////
    /// Set to true if there is an error in the request management and false
    /// otherwise.
    ///
    ///
    bool mError;

    enum
    {
        Idle,
        Started,
        Terminating
    } mThreadState;



    ////////////////////////////////////////////////////////////////////////////
    /// The mixer request
    ///
    MixerRequest_c mMixerRequest;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specifications: it waits for the request to be completed.
    ///
    /// Implementation comments:
    ///
    inline PlayerStatus_t WaitRequestCompletion()
    {
        PlayerStatus_t returnStatus = PlayerNoError;
        SE_VERBOSE(group_mixer, ">: this = %p\n", this);

        if (mThreadState == Started)
        {
            returnStatus = PlayerNoError;
            OS_Status_t WaitStatus = OS_WaitForEventAuto(&mRequestCompletionEvent, mTimeOut);
            if (WaitStatus == OS_TIMED_OUT)
            {
                SE_ERROR("wait for request completion timedout\n");
                returnStatus = PlayerError;
            }

            OS_ResetEvent(&mRequestCompletionEvent);

            if (mError)
            {
                returnStatus = PlayerError;
            }

            mError = false;
        }

        SE_VERBOSE(group_mixer, "<: this = %p\n", this);
        return returnStatus;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it sets the error to true.
    ///
    inline void SetError()
    {
        mError = true;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: this function locks the manager
    ///
    inline void Lock()
    {
        SE_VERBOSE(group_mixer, ">: locking this = %p\n", this);
        OS_LockRtMutex(&mRtMutex);
        SE_VERBOSE(group_mixer, "<: this = %p is locked now\n", this);
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: this function unlocks the manager.
    ///
    inline void Unlock()
    {
        SE_VERBOSE(group_mixer, ">: Unlocking this = %p\n", this);
        OS_UnLockRtMutex(&mRtMutex);
        SE_VERBOSE(group_mixer, "<: this = %p is unlocked now\n", this);
    }

    DISALLOW_COPY_AND_ASSIGN(Mixer_Request_Manager_c);
};

#endif // H_MIXER_REQUEST_MANAGER_CLASS
