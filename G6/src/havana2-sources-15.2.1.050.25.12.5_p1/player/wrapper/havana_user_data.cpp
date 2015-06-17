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

#include "havana_user_data.h"


int pull_user_data_se(stm_object_h src_object,
                      struct stm_data_block *block_list,
                      uint32_t block_count,
                      uint32_t *filled_blocks)
{
    UserDataSource_c *thisManifestor = (UserDataSource_c *)src_object;
    int         retCode;
    uint32_t    copiedBytes = 0;
    SE_DEBUG(group_havana, "in SE and checking for data block_list->len = %d\n", block_list->len);
    //
    // nothing read so far
    //
    *filled_blocks = 0;
    //
    // Check if something to read and if connection still opened
    //
    retCode = thisManifestor->UDBlockWaitFor();

    if (retCode < 0)
    {
        return retCode;
    }

    //
    // Read User Data blocks
    //
    copiedBytes = thisManifestor->UDBlockRead((uint8_t *)block_list->data_addr, block_list->len);
    block_list->len = copiedBytes;
    *filled_blocks = 1;
    // return total copied bytes
    return  copiedBytes;
}

int pull_test_for_user_data_se(stm_object_h src_object, uint32_t *size)
{
    UserDataSource_c *thisManifestor = (UserDataSource_c *)src_object;
    SE_DEBUG(group_havana, "in SE and checking for data\n");
    *size = thisManifestor->UDBlockAvailable();
    return 0;
}


struct stm_data_interface_pull_src se_pull_interface =
{
    pull_user_data_se,
    pull_test_for_user_data_se
};



//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor :
//      Action  : Initialise state
//      Input   :
//      Output  :
//      Result  :
//

UserDataSource_c::UserDataSource_c(void)
    : UserDataPlayStream(NULL)
    , Initialized(false)
    , Connected(false)
    , UDBlockBufferLock()
    , SignalNewUD()
    , UDBlockBuffers()
    , UDBlockBufferSize()
    , UDBlockWritePos(0)
    , UDBlockReadPos(0)
    , UDBlockFreeLen(MAX_UD_BUFFERS)
    , NewUDComplete(false)
    , PullSinkInterface()
    , SinkHandle(NULL)
{
    SE_VERBOSE(group_havana, "\n");

    OS_InitializeMutex(&UDBlockBufferLock);
    OS_SemaphoreInitialize(&SignalNewUD, 0);
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor :
//      Action  : Give up switch off the lights and go home
//      Input   :
//      Output  :
//      Result  :
//

UserDataSource_c::~UserDataSource_c(void)
{
    SE_VERBOSE(group_havana, "\n");

    if (Initialized)
    {
        Disconnect(SinkHandle);
    }

    OS_TerminateMutex(&UDBlockBufferLock);
    OS_SemaphoreTerminate(&SignalNewUD);
}
//}}}


//{{{  Init
//{{{  doxynote
/// \brief                      Actually first decode decoded buffer
/// \param Buffer               The stream object
/// \return                     Success or fail
//}}}
HavanaStatus_t  UserDataSource_c::Init(PlayerStream_t  PlayerStream)
{
    SE_DEBUG(group_havana, "\n");

    // Already initilized ?
    if (Initialized)
    {
        return HavanaError;
    }

    // Save PlayerStream  to retrieve Buffer / Metadata
    UserDataPlayStream = PlayerStream;

    // Initialization OK
    Initialized = true;

    return HavanaNoError;
}
//}}}


//{{{  GetCurrentBlockLen
//{{{  doxynote
/// \brief                      Return the lenght of the next to read UD block
/// \return                     Retuns the number of bytes.
//}}}
uint32_t  UserDataSource_c::GetCurrentBlockLen()
{
    uint32_t userDataBlockLen;
    uint32_t blockLenOffset = 4;
    userDataBlockLen  = UDBlockBuffers[(UDBlockReadPos + blockLenOffset) % UDBlockBufferSize];
    userDataBlockLen += UDBlockBuffers[(UDBlockReadPos + blockLenOffset + 1) % UDBlockBufferSize] << 8;
    return userDataBlockLen;
}
//}}}

//{{{  UDBlockRead
//{{{  doxynote
/// \brief                      Copy User Data blocks into user buffer.
///                             Note that blocks are entirely copied and removed from circular buffer
/// \param userBufferAddr       user buffer address (kernel address)
/// \param userBufferLen        user buffer lenght
/// \return                     Retuns the number of copied bytes.
//}}}
uint32_t  UserDataSource_c::UDBlockRead(uint8_t *userBufferAddr, uint32_t userBufferLen)
{
    SE_DEBUG(group_havana, "\n");

    // Check if init done
    if (!Initialized)
    {
        return HavanaError;
    }

    OS_LockMutex(&UDBlockBufferLock);
    //
    // Copy UserData Blocks while user buffer can receive them
    //
    bool     canCopyUDBlock      = true;
    uint32_t userBufRemainingLen = userBufferLen;
    uint32_t udCopiedLen         = 0;

    // Make sure no disconnection done
    if (Connected)
    {
        while (canCopyUDBlock == true)
        {
            // Current block_len
            uint32_t userDataBlockLen = GetCurrentBlockLen();

            if (UDBlockAvailable() == 0)
            {
                // No more UD block to read in circular buffer
                // or connection closed
                canCopyUDBlock = false;
                continue;
            }

            // Enough space remaining in user buffer
            if (userDataBlockLen <= userBufRemainingLen)
            {
                // Need to loop ?
                if ((UDBlockReadPos + userDataBlockLen) >=  UDBlockBufferSize)
                {
                    uint32_t udBlockPart1, udBlockPart2;
                    // Read will be done in two parts
                    udBlockPart1 = UDBlockBufferSize - UDBlockReadPos;
                    udBlockPart2 = userDataBlockLen - udBlockPart1;
                    memcpy(userBufferAddr,
                           &UDBlockBuffers[UDBlockReadPos],
                           udBlockPart1);
                    UDBlockReadPos  = 0;
                    userBufferAddr += udBlockPart1;

                    // Check if part2 is not null
                    if (udBlockPart2 != 0)
                    {
                        memcpy(userBufferAddr,
                               &UDBlockBuffers[UDBlockReadPos],
                               udBlockPart2);
                        UDBlockReadPos  += udBlockPart2;
                        userBufferAddr  += udBlockPart2;
                    }
                }
                else
                {
                    // No splitted block
                    memcpy(userBufferAddr,
                           &UDBlockBuffers[UDBlockReadPos],
                           userDataBlockLen);
                    UDBlockReadPos  += userDataBlockLen;
                    userBufferAddr  += userDataBlockLen;
                }

                // increase total lenght copied
                udCopiedLen    += userDataBlockLen;
                // increase available lenght in UDBlock Buffer
                UDBlockFreeLen += userDataBlockLen;
                // decrease available lenght in user buffer
                userBufRemainingLen -= userDataBlockLen;
            }
            else
            {
                // User buffer cannot receive next User Data block (not enough space)
                canCopyUDBlock = false;
                continue;
            }
        } // end While
    } // Connected

    OS_UnLockMutex(&UDBlockBufferLock);
    return udCopiedLen;
}
//}}}

//{{{  UDBlockAvailable
//{{{  doxynote
/// \brief                      Give the number of readable bytes of User Data in the circular buffer .
/// \return                     Retuns the number of bytes
//}}}
int32_t  UserDataSource_c::UDBlockAvailable()
{
    //
    // First check if still attached
    //
    if (!Connected)
    {
        return -ENOENT;
    }

    return (UDBlockBufferSize - UDBlockFreeLen);
}
//}}}

//{{{  UDBlockWaitFor
//{{{  doxynote
/// \brief                      Depending memeorysink pull mode will wait for user data
/// \return                     Retuns code < 0 if connection closed
//}}}
int32_t  UserDataSource_c::UDBlockWaitFor()
{
    SE_DEBUG(group_havana, "\n");

    //
    // check if circular buffer is empty
    //
    int32_t nbAvailableUD = UDBlockAvailable();
    if (nbAvailableUD < 0)
    {
        return -ENOENT;
    }

    if (nbAvailableUD == 0)
    {
        //
        // no User Data block available in circular buffer
        // check if user ask for waiting mode
        //
        if ((PullSinkInterface.mode & STM_IOMODE_NON_BLOCKING_IO) == STM_IOMODE_NON_BLOCKING_IO)
        {
            // no data and no wait
            return 0;
        }
        else
        {
            // no data but wait
            do
            {
                // Need to wait User data block availability
                OS_Status_t retOS = OS_SemaphoreWaitInterruptible(&SignalNewUD);
                if ((retOS != OS_NO_ERROR) || (!NewUDComplete) || (!Connected))
                {
                    return -EINTR;
                }

                NewUDComplete = false;
                // discard already consumed and signaled blocks
                nbAvailableUD = UDBlockAvailable();
            }
            while (nbAvailableUD == 0);
        }
    }

    return nbAvailableUD;
}
//}}}

//{{{  GetUserDataFromDecodeBuffer
//{{{  doxynote
/// \brief                      Extract User Data from decoded buffer if present
///                             and copy them in circular buffer
/// \param Buffer               buffer
/// \return                     Success or fail
//}}}
HavanaStatus_t  UserDataSource_c::GetUserDataFromDecodeBuffer(Buffer_t Buffer)
{
    SE_DEBUG(group_havana, "\n");
    HavanaStatus_t                  Status = HavanaNoError;

    // Check if init done
    if (!Initialized)
    {
        return HavanaError;
    }

    /* nothing to do if not connected to memeorySink */
    if (!Connected)
    {
        return HavanaNoError;
    }

    OS_LockMutex(&UDBlockBufferLock);
    // If available userdata buffer detach it
    // and insert it in UserData Ring
    Status = InsertNewUserDataIntoUserDataQueue(Buffer);
    OS_UnLockMutex(&UDBlockBufferLock);
    return Status;
}
//}}}

//{{{  Connect
//{{{  doxynote
/// \brief                      Connect to memory Sink port
/// \param Buffer               SinkHandle : memory sink object to connnect to
/// \return                     Success or fail
//}}}
HavanaStatus_t UserDataSource_c::Connect(stm_object_h  SinkHandle)
{
    SE_DEBUG(group_havana, "\n");
    int           retval;
    stm_object_h  sinkType;
    char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       returnedSize;

    // Check if init done and not already connected
    if (!Initialized || Connected)
    {
        SE_ERROR("Not in proper state (this %p Sink %p) (Initialized %d Connected %d)\n", this, SinkHandle, Initialized, Connected);
        return HavanaError;
    }

    //
    // Check sink object support STM_DATA_INTERFACE_PULL interface
    //
    retval = stm_registry_get_object_type(SinkHandle, &sinkType);
    if (retval)
    {
        SE_ERROR("error in stm_registry_get_object_type(%p, &%p) (%d)\n", SinkHandle, sinkType, retval);
        return HavanaError ;
    }

    retval = stm_registry_get_attribute(SinkHandle,
                                        STM_DATA_INTERFACE_PULL,
                                        tagTypeName,
                                        sizeof(PullSinkInterface),
                                        &PullSinkInterface,
                                        (int *)&returnedSize);
    if ((retval) || (returnedSize != sizeof(PullSinkInterface)))
    {
        SE_ERROR("error in stm_registry_get_attribute(...) %d\n", retval);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "\n Getting tag type %s\n", tagTypeName);
    //
    // call the sink interface's connect handler to connect the consumer
    //
    SE_DEBUG(group_havana, "Userdata tries to connect to memorySink\n");
    retval = PullSinkInterface.connect((stm_object_h)this       , // SRC
                                       SinkHandle,                // SINK
                                       (struct stm_data_interface_pull_src *) &se_pull_interface);

    if (retval)
    {
        SE_ERROR("error in PullSinkInterface.connect(%p) ret = %d\n", SinkHandle, retval);
        return HavanaError;
    }

    // Save sink object for disconnection
    this->SinkHandle = SinkHandle;
    // Successful connection
    Connected               = true;
    UDBlockWritePos         = 0;
    UDBlockReadPos          = 0;
    UDBlockBufferSize       = MAX_UD_BUFFERS;
    UDBlockFreeLen          = MAX_UD_BUFFERS;
    NewUDComplete           = false;
    return HavanaNoError;
}
//}}}

//{{{  Disconnect
//{{{  doxynote
/// \brief                      Disconnect from memory Sink port (if connected)
///                             Release pending pull call if any
/// \return                     Success or fail
//}}}
HavanaStatus_t UserDataSource_c::Disconnect(stm_object_h  SinkHandle)
{
    SE_DEBUG(group_havana, "\n");
    int   retval;

    //
    // Check that Sink object correspond the connected one
    //
    if (this->SinkHandle != SinkHandle)
    {
        return HavanaError;
    }

    if (Connected == true)
    {
        // Make sure no on going read
        OS_LockMutex(&UDBlockBufferLock);
        // close connection for user
        Connected = false;
        OS_UnLockMutex(&UDBlockBufferLock);
        //
        // Release pending pull call, if any
        //
        NewUDComplete = false;
        OS_SemaphoreSignal(&SignalNewUD);
        //
        // call the sink interface's disconnect handler to disconnect the consumer
        //
        SE_DEBUG(group_havana, "Userdata tries to disconnect to memorySink\n");
        retval = PullSinkInterface.disconnect((stm_object_h)this, SinkHandle);

        if (retval)
        {
            SE_ERROR("error in PullSinkInterface.disconnect(%p)\n", SinkHandle);
        }

        // reset old sink object
        this->SinkHandle = NULL;
    }

    return HavanaNoError;
}
//}}}


//{{{  InsertNewUserDataIntoUserDataQueue
//{{{  doxynote
/// \brief                      Check if incoming decoded buffer gets attached user Data
///                             If yes insert them in circular buffer
/// \param Buffer               buffer
/// \return                     Success or fail
//}}}
HavanaStatus_t UserDataSource_c::InsertNewUserDataIntoUserDataQueue(Buffer_t Buffer)
{
    ParsedFrameParameters_t          *ParsedFrameParameters;
    uint8_t                          *UserDataBuffer;
    Buffer_t                          OriginalCodedFrameBuffer;
    uint32_t                          blockCount;
    bool                              copyDone = false;
    SE_DEBUG(group_havana, "\n");
    //
    // obtain user data buffer reference
    //
    Buffer->ObtainAttachedBufferReference(UserDataPlayStream->GetCodedFrameBufferType(), &OriginalCodedFrameBuffer);
    SE_ASSERT(OriginalCodedFrameBuffer != NULL);

    OriginalCodedFrameBuffer->ObtainMetaDataReference(UserDataPlayStream->GetPlayer()->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    //
    // Check if User Data available for this frame
    //
    if (ParsedFrameParameters->UserDataNumber <= 0)
    {
        return HavanaError;
    }

    OriginalCodedFrameBuffer->ObtainMetaDataReference(UserDataPlayStream->GetPlayer()->MetaDataUserDataType, (void **)(&UserDataBuffer));
    SE_ASSERT(UserDataBuffer != NULL);

    //
    // User Data block can be copied in the circular buffer
    //
    for (blockCount = 0; blockCount < ParsedFrameParameters->UserDataNumber; blockCount++)
    {
        uint32_t userDataBlockLen = *((uint16_t *)UserDataBuffer + 2);

        // enough space for this block ?
        // no : => in the first implementation, more recent data are dropped
        //         instead of older data !!
        if (userDataBlockLen <= UDBlockFreeLen)
        {
            // Need to loop
            if ((UDBlockWritePos + userDataBlockLen) >=  UDBlockBufferSize)
            {
                uint32_t udBlockPart1, udBlockPart2;
                udBlockPart1 = UDBlockBufferSize - UDBlockWritePos;
                udBlockPart2 = userDataBlockLen  - udBlockPart1;
                memcpy(&UDBlockBuffers[UDBlockWritePos], UserDataBuffer, udBlockPart1);
                UDBlockWritePos  = 0;
                UserDataBuffer  += udBlockPart1;

                // Check part2 is no null
                if (udBlockPart2 != 0)
                {
                    memcpy(&UDBlockBuffers[UDBlockWritePos], UserDataBuffer, udBlockPart2);
                    UDBlockWritePos += udBlockPart2;
                    UserDataBuffer  += udBlockPart2;
                }
            }
            else
            {
                memcpy(&UDBlockBuffers[UDBlockWritePos], UserDataBuffer, userDataBlockLen);
                UDBlockWritePos += userDataBlockLen;
                UserDataBuffer  += userDataBlockLen;
            }

            // decrease free lenght
            UDBlockFreeLen -= userDataBlockLen;
            // at least one copy done
            copyDone = true;
        }
    }

    // Signal new data available if necessary
    if (copyDone  && !NewUDComplete)
    {
        NewUDComplete = true;
        OS_SemaphoreSignal(&SignalNewUD);
    }

    return HavanaNoError;
}
//}}}
