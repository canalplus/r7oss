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

#ifndef H_BASE_COMPONENT_CLASS
#define H_BASE_COMPONENT_CLASS

#include "player_types.h"
#include "encoder_types.h"

typedef enum
{
    ComponentReset      = 1,
    ComponentRunning,
    ComponentHalted,
    ComponentInError
} EngineComponentState_t;

#define AssertComponentState(s)            \
    if( !TestComponentState(s) )                \
    {                                           \
    if( ComponentRunning != s )                                                                         \
    {                                                                                                   \
    SE_WARNING("Component not in required state:%d, current:%d\n", s, ComponentState); \
    }                                                                                                   \
    else                                                                                                \
    {                                                                                                   \
    SE_WARNING("Component not in running state: current:%d\n", ComponentState);       \
    }                                                                                                   \
    return PlayerError;                     \
    }

#undef TRACE_TAG
#define TRACE_TAG "BaseComponentClass_c"

class BaseComponentClass_c
{
public:
    PlayerStatus_t  InitializationStatus;

    BaseComponentClass_c(void)
        : InitializationStatus(PlayerNoError)
        , ComponentState(ComponentReset)
        , Player(NULL)
        , Playback(PlayerAllPlaybacks)
        , Stream(PlayerAllStreams)
        , EventMask(0)
        , EventUserData(NULL)
        , ComponentLockMutex()
        , ComponentLockOwnerId(0)
        , ComponentLockCount(0)
        , mGroupTrace(0)
        , Encoder()
    {
        OS_InitializeMutex(&ComponentLockMutex);
    }

    virtual ~BaseComponentClass_c(void)
    {
        OS_TerminateMutex(&ComponentLockMutex);
    }

    virtual PlayerStatus_t      Halt(void)
    {
        SetComponentState(ComponentHalted);
        return  PlayerNoError;
    }

    // for reseting states for stream switch
    virtual PlayerStatus_t      Reset(void)
    {
        Player                      = NULL;
        Playback                    = PlayerAllPlaybacks;
        Stream                      = PlayerAllStreams;
        EventMask                   = 0;
        EventUserData               = NULL;
        Encoder.Encoder             = NULL;
        Encoder.Encode              = NULL;
        Encoder.EncodeStream        = NULL;
        Encoder.Preproc             = NULL;
        Encoder.Coder               = NULL;
        Encoder.Transporter         = NULL;
        Encoder.EncodeCoordinator   = NULL;
        SetComponentState(ComponentReset);
        return  PlayerNoError;
    }

    virtual PlayerStatus_t RegisterPlayer(
        Player_t             Player,
        PlayerPlayback_t     Playback,
        PlayerStream_t       Stream)
    {
        //
        // In order to provide this data for early use, there is an assumption
        // that this registratory function does no real work, and cannot fail.
        //
        this->Player    = Player;
        this->Playback  = Playback;
        this->Stream    = Stream;

        return  PlayerNoError;
    }

    virtual PlayerStatus_t  RegisterEncoder(
        Encoder_t           Encoder,
        Encode_t            Encode              = NULL,
        EncodeStream_t      EncodeStream        = NULL,
        Preproc_t           Preproc             = NULL,
        Coder_t             Coder               = NULL,
        Transporter_t       Transporter         = NULL,
        EncodeCoordinator_t EncodeCoordinator   = NULL)
    {
        //
        // In order to provide this data for early use, there is an assumption
        // that this registratory function does no real work, and cannot fail.
        //
        if (Encoder != NULL)
        {
            this->Encoder.Encoder     = Encoder;
        }

        if (Encode != NULL)
        {
            this->Encoder.Encode      = Encode;
        }

        if (EncodeStream != NULL)
        {
            this->Encoder.EncodeStream        = EncodeStream;
        }

        if (Preproc != NULL)
        {
            this->Encoder.Preproc     = Preproc;
        }

        if (Coder != NULL)
        {
            this->Encoder.Coder           = Coder;
        }

        if (Transporter != NULL)
        {
            this->Encoder.Transporter     = Transporter;
        }

        if (EncodeCoordinator != NULL)
        {
            this->Encoder.EncodeCoordinator   = EncodeCoordinator;
        }

        return  PlayerNoError;
    }

    virtual PlayerStatus_t      SpecifySignalledEvents(
        PlayerEventMask_t        EventMask,
        void                    *EventUserData)
    {
        this->EventMask         = EventMask;
        this->EventUserData     = EventUserData;
        return  PlayerNoError;
    }

    virtual PlayerStatus_t      SetModuleParameters(
        unsigned int             ParameterBlockSize,
        void                    *ParameterBlock)
    {
        return  PlayerNotSupported;
    }

    virtual PlayerStatus_t      CreateAttributeEvents(void)
    {
        return  PlayerNoError;
    }

    virtual PlayerStatus_t      GetAttribute(
        const char                      *Attribute,
        PlayerAttributeDescriptor_t     *Value)
    {
        return  PlayerNoError;
    }

    virtual PlayerStatus_t      SetAttribute(
        const char                      *Attribute,
        PlayerAttributeDescriptor_t     *Value)
    {
        return  PlayerNoError;
    }

    PlayerStatus_t Lock()
    {
        int threadid = OS_ThreadID();

        if (ComponentLockOwnerId != threadid)
        {
            OS_LockMutex(&ComponentLockMutex);
            ComponentLockOwnerId = threadid;
            ComponentLockCount = 1;
        }
        else
        {
            ComponentLockCount++;
        }

        return PlayerNoError;
    }

    void UnLock()
    {
        ComponentLockCount--;

        if (0 == ComponentLockCount)
        {
            ComponentLockOwnerId = 0;
            OS_UnLockMutex(&ComponentLockMutex);
        };
    }

    inline int GetGroupTrace() const { return mGroupTrace; }

    // This variable can be set through debugfs to switch the streaming-engine from
    // Frambase (frame parsing done on host) to StreamBase (frame parsing done on
    // co-processor).
    static unsigned int volatile EnableCoprocessorParsing;

protected:
    EngineComponentState_t   ComponentState;

    // Player Specifics

    Player_t                 Player;
    PlayerPlayback_t         Playback;
    PlayerStream_t           Stream;

    PlayerEventMask_t        EventMask;
    void                    *EventUserData;

    OS_Mutex_t               ComponentLockMutex;
    int                      ComponentLockOwnerId;
    unsigned int             ComponentLockCount;

    int                      mGroupTrace;

    //
    // Encoder Specifics are stored in a struct
    // This means in the future, we can move the player
    // specific members to a struct as well, and then
    // the two structs can be stored in an anonymous
    // union
    //

    struct
    {
        Encoder_t           Encoder;
        Encode_t            Encode;
        EncodeStream_t      EncodeStream;
        Preproc_t           Preproc;
        Coder_t             Coder;
        Transporter_t       Transporter;
        EncodeCoordinator_t EncodeCoordinator;
    } Encoder;

    inline bool TestComponentState(EngineComponentState_t s)
    {
        bool ret = (ComponentState == s);
        // This memory barrier must occur after Reading Component State.
        // The ordering is very specific to ensure that ComponentState
        // is checked before any operation depending on it occurs
        OS_Smp_Mb();
        return ret;
    }

    inline void SetComponentState(EngineComponentState_t s)
    {
        OS_Smp_Mb();
        // This memory barrier must occur before Setting Component State.
        // The ordering is very specific to ensure that ComponentState
        // Cannot be changed until all operations preceeding are complete
        ComponentState = s;
    }

    inline void SetGroupTrace(int group) { mGroupTrace = group; }

private:
    DISALLOW_COPY_AND_ASSIGN(BaseComponentClass_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class BaseComponentClass_c
\brief Base class from which all component classes are derived.

All of the component classes, (apart from the buffer classes) have
certain common components in terms of globally visible data,
entrypoints, and usage of player class support functions. Here we
detail those common components that all these classes <b>must</b>
have.
*/

/*! \var BaseComponentClass_c::InitializationStatus
\brief Status flag indicating how the initialization of the class went.

In order to fit well into the Linux kernel environment the player is
compiled with exceptions and RTTI disabled. In C++ constructors cannot
return an error code since all errors should be reported using
exceptions. With exceptions disabled we need to use this alternative
means to communicate that the constructed object instance is not valid.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::Halt()
\brief Prepare for destruction or reset.

This function is used in preparation for destruction or a reset, it
halts processing of all data, and de-registers any resources registered
to the class. Halt should be called prior to destruction, or reset, of
a component class.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::Reset()
\brief Completely reset a class.

This function is used to completely reset a class, it should be used
when a class is to be re-used after channel or stream change. This
function is called by the class constructor during class initialization.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::RegisterPlayer(Player_t Player, PlayerPlayback_t Playback, PlayerStream_t Stream )
\brief Register to player instantiation to which the class 'belongs'.

Takes as parameter a pointer to a Player class instance, a playback identifier, and a stream identifier. This function is used to register the Player instantiation with the component class instance, this is used when the component needs to access a player entrypoint.

\param Player Pointer to a Player_c instance
\param Playback Playback identifier
\param Stream Stream identifier

\return Player status code, PlayerNoError indicates success.
*/


/*! \fn PlayerStatus_t BaseComponentClass_c::RegisterEncoder(Encoder_t Encoder )
\brief Register to an encoder instantiation to which the class 'belongs'.

Takes as parameter a pointer to an Encoder class instance, a Preproc identifier,
a Coder identifier, and an EncodeCoordinator identifier. This function is used to
register the Encoder instantiation with the component class instance, this is used
when the component needs to access an encoder entrypoint.

\param Encoder Pointer to an Encoder_c instance
\param Preproc Preprocessor identifier
\param Coder Coder class identifier
\param EncodeCoordinator EncodeCoordinator class instance

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::SpecifySignalledEvents(PlayerEventMask_t EventMask, void *EventUserData)
\brief Enable/disable ongoing event signalling.

\param EventMask A mask indicating which occurrences should raise an event record
\param EventUserData An optional void * value to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::SetModuleParameters(unsigned int ParameterBlockSize, void *ParameterBlock)
\brief Specify the module parameters.

\param ParameterBlockSize Size of the parameter block (in bytes)
\param ParameterBlock Pointer to the parameter block

\return Player status code, PlayerNoError indicates success.
*/

// ---------------------------------------------------------------------
//
// Utilities
//

#undef TRACE_TAG
#define TRACE_TAG "BaseComponentAutoLock_c"

/*!
 * Automatically release the component lock when a (stack) frame is destroyed.
 *
 * This class has a pure inline implementation (allowing the optimizer to remove
 * things like the boolean where possible). It is intended to make it easy
 * to use BaseComponentClass_c::Lock() and BaseComponentClass_c::UnLock() without
 * risking leaving the method without releasing the lock.
 *
 * Usage:
 * <pre>
 * PlayerStatus_t Component::Action() {
 *     BaseComponentAutoLock_c autoLock(this);
 *     PlayerStatus_t Status = autoLock->Lock();
 *     if (PlayerNoError != Status) {
 *         return Status;
 *     }
 *
 *     // component activity goes here
 *     ...
 *     if (cond) {
 *         ...
 *         return PlayerError; // automatic unlock during return
 *     }
 *     ...
 *
 *     return PlayerNoError; // automatic unlock during return
 * }
 * </pre>
 */
class BaseComponentAutoLock_c
{
private:
    bool mLocked;
    BaseComponentClass_c *mComponent;

    DISALLOW_COPY_AND_ASSIGN(BaseComponentAutoLock_c);

public:
    BaseComponentAutoLock_c(BaseComponentClass_c *Component) :
        mLocked(false), mComponent(Component) {}

    ~BaseComponentAutoLock_c()
    {
        if (mLocked)
        {
            mComponent->UnLock();
        }
    }

    PlayerStatus_t Lock()
    {
        if (mLocked)
        {
            SE_ERROR("already locked\n");
            return PlayerError;
        }

        PlayerStatus_t Status = mComponent->Lock();
        mLocked = (PlayerNoError == Status);
        return Status;
    }

    void UnLock()
    {
        if (!mLocked)
        {
            SE_ERROR("not locked\n");
            return;
        }

        mComponent->UnLock();
        mLocked = false;
    }
};

/*!
 * Automatic monitor macro for components.
 *
 * This macro shrinks the boilerplate code found in the
 * BaseComponentAutoLock_c at a cost of concealing an return
 * path. Due to the concealed return path this macro only
 * appear very early in a method.
 *
 * Normally the object passes as an argument should be the
 * this pointer. However occasionally it might be appropriate
 * For example, ::Buffer_Generic_c may prefer to use the lock of
 * the ::BufferPool_c instead.
 *
 * This macro can be used to give specified methods monitor-like
 * mutual exclusion properties. For more info:
 *   http://en.wikipedia.org/wiki/Monitor_%28synchronization%29
 *
 * Usage:
 * <pre>
 * PlayerStatus_t Component::Action() {
 *     ComponentMonitor(this);
 *
 *     // component activity goes here
 *     ...
 *     if (cond) {
 *         ...
 *         return PlayerNoError; // automatic unlock during return
 *     }
 *
 *     // handling a long sleep (when we might want to enable other threads)
 *     ComponentMonitor->Unlock();
 *     WaitForThingToHappen();
 *     ComponentMonitor->Lock();
 *
 *     // more activity
 *     ...
 *
 *     return PlayerSuccess; // automatic unlock during return
 * }
 * </pre>
 */
#define ComponentMonitor(x) \
    BaseComponentAutoLock_c ComponentMonitor(x); \
    { \
        PlayerStatus_t Status = ComponentMonitor.Lock(); \
        if (PlayerNoError != Status) \
            return Status; \
    }

#endif // H_BASE_COMPONENT_CLASS

