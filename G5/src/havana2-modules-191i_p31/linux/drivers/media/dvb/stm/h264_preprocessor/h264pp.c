/*
** h264pp.c - implementation of a simple H264 pre-processor driver
**
** Copyright 2005 STMicroelectronics, all right reserved.
**
**          MAJOR  MINOR  DESCRIPTION
**          -----  -----  ----------------------------------------------
**          246     0     Drive the H264 pre-processor hardware
*/

#define MAJOR_NUMBER    246
#define MINOR_NUMBER    0

#include "osdev_device.h"

/* --- */

#include "h264ppio.h"
#include "h264pp.h"

// ////////////////////////////////////////////////////////////////////////////////
//
// defines

// ////////////////////////////////////////////////////////////////////////////////
//
// Structure definitions

//
// The definitions for individual open instances
//

typedef struct H264ppProcessBuffer_s
{
    boolean			 Processed;
    OSDEV_Semaphore_t		*BufferComplete;
    OSDEV_Semaphore_t		*VirtualPPClaim;
    h264pp_ioctl_queue_t	 Parameters;

    unsigned int		 PP;
    unsigned int		 PP_ITS;
    unsigned int		 OutputSize;
    unsigned int		 DecodeTime;
} H264ppProcessBuffer_t;

//

typedef struct H264ppOpenContext_s
{
    boolean			Closing;
    boolean			InQueue;
    boolean			InDequeue;

    OSDEV_Semaphore_t		BufferComplete;
    OSDEV_Semaphore_t		VirtualPPClaim;

    unsigned int		ProcessBufferInsert;
    unsigned int		ProcessBufferExtract;
    OSDEV_Semaphore_t		ProcessBufferClaim;
    H264ppProcessBuffer_t	ProcessBuffers[H264_PP_MAX_SUPPORTED_BUFFERS_PER_OPEN];
} H264ppOpenContext_t;

//
// The definitions for the master hardware context
//

typedef struct H264ppState_s
{
    boolean			 Busy;
    H264ppProcessBuffer_t	*BufferState;

    boolean			 ForceWorkAroundGNBvd42331;
    unsigned int		 last_mb_adaptive_frame_field_flag;		// Copy of h264 variable
    unsigned int                 last_entropy_coding_mode_flag;
    unsigned int		 Accumulated_ITS;
} H264ppState_t;

//

typedef struct H264ppDeviceContext_s
{
    OSDEV_Semaphore_t	 Lock;

    unsigned int         NumberOfPreProcessors;
    unsigned int	 RegisterBase[H264_PP_MAX_SUPPORTED_PRE_PROCESSORS];
    unsigned int	 InterruptNumber[H264_PP_MAX_SUPPORTED_PRE_PROCESSORS];
    boolean		 ApplyWorkAroundForGnbvd42331;

    OSDEV_Semaphore_t	 PPClaim;
    H264ppState_t	 PPState[H264_PP_MAX_SUPPORTED_PRE_PROCESSORS];
} H264ppDeviceContext_t;

// ////////////////////////////////////////////////////////////////////////////////
//
// Static Data

H264ppDeviceContext_t	DeviceContext;

// ////////////////////////////////////////////////////////////////////////////////
//
//

static OSDEV_OpenEntrypoint(  H264ppOpen );
static OSDEV_CloseEntrypoint( H264ppClose );
static OSDEV_IoctlEntrypoint( H264ppIoctl );
static OSDEV_MmapEntrypoint(  H264ppMmap );

static OSDEV_Descriptor_t H264ppDeviceDescriptor =
{
	Name:           "H264 Pre-processor Module",
	MajorNumber:    MAJOR_NUMBER,

	OpenFn:         H264ppOpen,
	CloseFn:        H264ppClose,
	IoctlFn:        H264ppIoctl,
	MmapFn:         H264ppMmap
};

/* --- External entrypoints --- */

OSDEV_PlatformLoadEntrypoint( H264ppLoadModule );
OSDEV_PlatformUnloadEntrypoint( H264ppUnloadModule );
OSDEV_RegisterPlatformDriverFn( "h264pp", H264ppLoadModule, H264ppUnloadModule);

/* --- Internal entrypoints --- */

static void H264ppInitializeDevice( 	void );
static void H264ppReleaseDevice(     void );
static void H264ppQueueBufferToDevice( 	H264ppProcessBuffer_t	 *Buffer );
static void H264ppWorkAroundGNBvd42331( H264ppState_t		 *State,
					unsigned int		  N );

// ////////////////////////////////////////////////////////////////////////////////
//
//    The Initialize module function

OSDEV_PlatformLoadEntrypoint( H264ppLoadModule )
{
unsigned int	i;
OSDEV_Status_t  Status;

//

    OSDEV_LoadEntry();

    //
    // Initialize the hardware context
    //


    DeviceContext.ApplyWorkAroundForGnbvd42331	= true;
    DeviceContext.NumberOfPreProcessors		= min( min(PlatformData->NumberBaseAddresses,PlatformData->NumberInterrupts), H264_PP_MAX_SUPPORTED_PRE_PROCESSORS );

    OSDEV_InitializeSemaphore( &DeviceContext.Lock, 1 );
    OSDEV_InitializeSemaphore( &DeviceContext.PPClaim, DeviceContext.NumberOfPreProcessors );

    for( i=0; i<DeviceContext.NumberOfPreProcessors; i++ )
    {
	DeviceContext.RegisterBase[i]		= (unsigned int)OSDEV_IOReMap( PlatformData->BaseAddress[i], H264_PP_REGISTER_SIZE );
	DeviceContext.InterruptNumber[i]	= PlatformData->Interrupt[i];
	DeviceContext.PPState[i].Busy		= false;
    }

    //
    // Initialize the hardware device
    //

    H264ppInitializeDevice();

    //
    // Register our device
    //

    Status = OSDEV_RegisterDevice( &H264ppDeviceDescriptor );
    if( Status != OSDEV_NoError )
    {
	OSDEV_Print( "H264ppLoadModule : Unable to get major %d\n", MAJOR_NUMBER );
	OSDEV_LoadExit( OSDEV_Error );
    }

    OSDEV_LinkDevice(  H264_PP_DEVICE, MAJOR_NUMBER, MINOR_NUMBER );

//

    OSDEV_Print( "H264ppLoadModule : H264pp device loaded\n" );
    OSDEV_LoadExit( OSDEV_NoError );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Terminate module function

OSDEV_PlatformUnloadEntrypoint( H264ppUnloadModule )
{
OSDEV_Status_t          Status;

    //
    // Release the hardware device
    //

    H264ppReleaseDevice();

/* --- */

    OSDEV_UnloadEntry();


/* --- */

    Status = OSDEV_DeRegisterDevice( &H264ppDeviceDescriptor );
    if( Status != OSDEV_NoError )
	OSDEV_Print( "H264ppUnloadModule : Unregister of device failed\n");

/* --- */

    OSDEV_DeInitializeSemaphore( &DeviceContext.Lock );

/* --- */

    OSDEV_Print( "H264pp device unloaded\n" );
    OSDEV_UnloadExit(OSDEV_NoError);
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Interrupt handler

OSDEV_InterruptHandlerEntrypoint( H264ppInterruptHandler )
{
unsigned int			 N	= (unsigned int)Parameter;
unsigned int			 PP_ITS;
unsigned int			 PP_WDL;
unsigned int			 PP_ISBG;
unsigned int			 PP_IPBG;
unsigned long long		 Now;
H264ppState_t			*State;
H264ppProcessBuffer_t		*Record;

    //
    // Read and clear the interrupt status
    //

    Now			= OSDEV_GetTimeInMilliSeconds();
    State		= &DeviceContext.PPState[N];

    PP_ITS		= OSDEV_ReadLong( PP_ITS(N) );
    PP_ISBG		= OSDEV_ReadLong( PP_ISBG(N) );
    PP_IPBG		= OSDEV_ReadLong( PP_IPBG(N) );
    PP_WDL		= OSDEV_ReadLong( PP_WDL(N) );

    OSDEV_WriteLong( PP_ITS(N), PP_ITS );               // Clear interrupt status

#if 0
if( PP_ITS != PP_ITM__DMA_CMP )
{
printk( "$$$$$$ H264ppInterruptHandler (PP %d)  - Took %dms $$$$$$\n", N, Now );

printk( "       PP_READ_START           = %08x\n", OSDEV_ReadLong(PP_READ_START(N)) );
printk( "       PP_READ_STOP            = %08x\n", OSDEV_ReadLong(PP_READ_STOP(N)) );
printk( "       PP_BBG                  = %08x\n", OSDEV_ReadLong(PP_BBG(N)) );
printk( "       PP_BBS                  = %08x\n", OSDEV_ReadLong(PP_BBS(N)) );
printk( "       PP_ISBG                 = %08x\n", PP_ISBG );
printk( "       PP_IPBG                 = %08x\n", OSDEV_ReadLong(PP_IPBG(N)) );
printk( "       PP_IBS                  = %08x\n", OSDEV_ReadLong(PP_IBS(N)) );
printk( "       PP_WDL                  = %08x\n", PP_WDL );
printk( "       PP_CFG                  = %08x\n", OSDEV_ReadLong(PP_CFG(N)) );
printk( "       PP_PICWIDTH             = %08x\n", OSDEV_ReadLong(PP_PICWIDTH(N)) );
printk( "       PP_CODELENGTH           = %08x\n", OSDEV_ReadLong(PP_CODELENGTH(N)) );
printk( "       PP_MAX_OPC_SIZE         = %08x\n", OSDEV_ReadLong(PP_MAX_OPC_SIZE(N)) );
printk( "       PP_MAX_CHUNK_SIZE       = %08x\n", OSDEV_ReadLong(PP_MAX_CHUNK_SIZE(N)) );
printk( "       PP_MAX_MESSAGE_SIZE     = %08x\n", OSDEV_ReadLong(PP_MAX_MESSAGE_SIZE(N)) );
printk( "       PP_ITM                  = %08x\n", OSDEV_ReadLong(PP_ITM(N)) );
printk( "       PP_ITS                  = %08x\n", PP_ITS );
}
#endif

    //
    // If appropriate, signal the code.
    //

    State->Accumulated_ITS		|= PP_ITS;
    if( ((PP_ITS & PP_ITM__DMA_CMP) != 0) && State->Busy )
    {
	//
	// Record the completion of this process
	//

	Record	= State->BufferState;

	Record->PP		= N;
	Record->PP_ITS		= State->Accumulated_ITS;
        Record->OutputSize	= (PP_WDL - PP_ISBG);
	Record->DecodeTime	= Now - Record->DecodeTime;

	Record->Processed	= true;

	OSDEV_ReleaseSemaphore( Record->BufferComplete );
	OSDEV_ReleaseSemaphore( Record->VirtualPPClaim );

	//
	// If we have seen an error condition, then force the pre-processor
	// to launch the H264ppWorkAroundGNBvd42331 on the next pass.
	//

	if( DeviceContext.ApplyWorkAroundForGnbvd42331 && (PP_ITS != PP_ITM__DMA_CMP) )
	    State->ForceWorkAroundGNBvd42331	= true;

	//
	// Indicate that the pre-processor is free
	//

	State->Busy	= 0;
	OSDEV_ReleaseSemaphore( &DeviceContext.PPClaim );
    }

//

    return IRQ_HANDLED;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Open function

static OSDEV_OpenEntrypoint(  H264ppOpen )
{
H264ppOpenContext_t	*H264ppOpenContext;

    //
    // Entry
    //

    OSDEV_OpenEntry();

    //
    // Allocate and initialize the private data structure.
    //

    H264ppOpenContext    = (H264ppOpenContext_t *)OSDEV_Malloc( sizeof(H264ppOpenContext_t) );
    OSDEV_PrivateData   = (void *)H264ppOpenContext;

    if( H264ppOpenContext == NULL )
    {
	OSDEV_Print( "H264ppOpen - Unable to allocate memory for open context.\n" );
	OSDEV_OpenExit( OSDEV_Error );
    }

    //
    // Initialize the data structure
    //

    memset( H264ppOpenContext, 0x00, sizeof(H264ppOpenContext_t) );

    H264ppOpenContext->Closing      		= false;
    H264ppOpenContext->ProcessBufferInsert	= 0;
    H264ppOpenContext->ProcessBufferExtract	= 0;

    //
    // Initialize the semaphores
    //

    OSDEV_InitializeSemaphore( &H264ppOpenContext->VirtualPPClaim, H264_PP_VIRTUAL_PP_PER_OPEN ); 			// How many queues can we run at once
    OSDEV_InitializeSemaphore( &H264ppOpenContext->ProcessBufferClaim, H264_PP_MAX_SUPPORTED_BUFFERS_PER_OPEN );	// The table contains this many entries
    OSDEV_InitializeSemaphore( &H264ppOpenContext->BufferComplete, 0 );							// used to signal a buffer complete

//

    OSDEV_OpenExit( OSDEV_NoError );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Close function

static OSDEV_CloseEntrypoint(  H264ppClose )
{
unsigned int		 i;
H264ppOpenContext_t	*H264ppOpenContext;

//

    OSDEV_CloseEntry();
    H264ppOpenContext               = (H264ppOpenContext_t  *)OSDEV_PrivateData;

    H264ppOpenContext->Closing      = true;

    //
    // Signal both the queue and dequeue semaphores
    //

    OSDEV_ReleaseSemaphore( &H264ppOpenContext->ProcessBufferClaim );
    OSDEV_ReleaseSemaphore( &H264ppOpenContext->BufferComplete );

    //
    // Grab all of my virtual pre-processors, to be sure all decodes have completed
    //

    for( i=0; i<H264_PP_VIRTUAL_PP_PER_OPEN; i++ )
	OSDEV_ClaimSemaphore( &H264ppOpenContext->VirtualPPClaim );

    //
    // Make sure everyone exits
    //

    while( H264ppOpenContext->InQueue || H264ppOpenContext->InDequeue )
	OSDEV_SleepMilliSeconds( 2 );

    //
    // De-initialize the semaphores
    //

    OSDEV_DeInitializeSemaphore( &H264ppOpenContext->ProcessBufferClaim );
    OSDEV_DeInitializeSemaphore( &H264ppOpenContext->BufferComplete );

    //
    // Free the context memory
    //

    OSDEV_Free( H264ppOpenContext );

//

    OSDEV_CloseExit( OSDEV_NoError );
}

// ////////////////////////////////////////////////////////////////////////////////
//
//    The mmap function to make the H264pp buffers available to the user for writing

static OSDEV_MmapEntrypoint( H264ppMmap )
{
H264ppOpenContext_t  *H264ppOpenContext;
OSDEV_Status_t    MappingStatus;

//

    OSDEV_MmapEntry();
    H264ppOpenContext    = (H264ppOpenContext_t  *)OSDEV_PrivateData;

//

    OSDEV_Print( "H264ppMmap - No allocated memory to map.\n" );
    MappingStatus	= OSDEV_Error;

//

    OSDEV_MmapExit( MappingStatus );
}

// ////////////////////////////////////////////////////////////////////////////////
//
//    The ioctl function to handle queueing of a buffer to the preprocessor

static OSDEV_Status_t H264ppIoctlQueueBuffer(	H264ppOpenContext_t	*H264ppOpenContext,
						unsigned int		 ParameterAddress )
{
H264ppProcessBuffer_t		*Record;

    //
    // Check if we are closing, then claim our resources and recheck the closing state
    //

    if( H264ppOpenContext->Closing )
    {
	OSDEV_Print( "H264ppIoctlQueueBuffer - device closing.\n" );
	return OSDEV_Error;
    }

    H264ppOpenContext->InQueue		= true;
    OSDEV_ClaimSemaphore( &H264ppOpenContext->VirtualPPClaim );			// Claim a virtual pre-processor
    OSDEV_ClaimSemaphore( &H264ppOpenContext->ProcessBufferClaim );		// Claim a buffer to use

    if( H264ppOpenContext->Closing )
    {
	OSDEV_Print( "H264ppIoctlQueueBuffer - device closing.\n" );
	OSDEV_ReleaseSemaphore( &H264ppOpenContext->VirtualPPClaim );
	H264ppOpenContext->InQueue	= false;
	return OSDEV_Error;
    }

    //
    // Now we can use next buffer in the list, we need to initialize it
    //

    Record	= &H264ppOpenContext->ProcessBuffers[(H264ppOpenContext->ProcessBufferInsert++) % H264_PP_MAX_SUPPORTED_BUFFERS_PER_OPEN];

    memset( Record, 0x00, sizeof(H264ppProcessBuffer_t) );

    Record->Processed		= false;
    Record->BufferComplete	= &H264ppOpenContext->BufferComplete;
    Record->VirtualPPClaim	= &H264ppOpenContext->VirtualPPClaim;

    OSDEV_CopyToDeviceSpace( &Record->Parameters, ParameterAddress, sizeof(h264pp_ioctl_queue_t) );

    //
    // queue the decode on a real device
    //

    H264ppQueueBufferToDevice( Record );

//

    H264ppOpenContext->InQueue	= false;
    return OSDEV_NoError;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The ioctl function to get a buffer that has been handled by the pre-processor

static OSDEV_Status_t H264ppIoctlGetPreprocessedBuffer( H264ppOpenContext_t	*H264ppOpenContext,
							unsigned int		 ParameterAddress )
{
h264pp_ioctl_dequeue_t	 Params;
H264ppProcessBuffer_t	*Record;

//

    H264ppOpenContext->InDequeue	= true;
    Record				= &H264ppOpenContext->ProcessBuffers[(H264ppOpenContext->ProcessBufferExtract++) % H264_PP_MAX_SUPPORTED_BUFFERS_PER_OPEN];

    while( !H264ppOpenContext->Closing )
    {
	//
	// Anything to report
	//

	if( Record->Processed )
	{
#if 0
	    if( Record->PP_ITS != PP_ITM__DMA_CMP )
		printk( "H264ppIoctlGPB (PP %d)  - Took %dms - ITS = %04x - QID = %d\n", Record->PP, Record->DecodeTime, Record->PP_ITS, Record->Parameters.QueueIdentifier );
#endif

	    //
	    // Take the buffer
	    //

	    Params.QueueIdentifier	= Record->Parameters.QueueIdentifier;
	    Params.OutputSize		= Record->OutputSize;
	    Params.ErrorMask		= Record->PP_ITS & ~(PP_ITM__SRS_COMP | PP_ITM__DMA_CMP);
	    OSDEV_CopyToUserSpace( ParameterAddress, &Params, sizeof(h264pp_ioctl_dequeue_t) );

	    //
	    // Free up the record for re-use
	    //

	    OSDEV_ReleaseSemaphore( &H264ppOpenContext->ProcessBufferClaim );
	    H264ppOpenContext->InDequeue	= false;

	    return OSDEV_NoError;
	}

	//
	// Wait for a buffer completed signal
	//

	OSDEV_ClaimSemaphore( &H264ppOpenContext->BufferComplete );

    } 

//

    OSDEV_Print( "H264ppIoctlGetPreprocessedBuffer - device closing.\n" );
    H264ppOpenContext->InDequeue	= false;
    return OSDEV_Error;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The ioctl function

static OSDEV_IoctlEntrypoint( H264ppIoctl )
{
OSDEV_Status_t           Status;
H264ppOpenContext_t      *H264ppOpenContext;

/* --- */

    OSDEV_IoctlEntry();
    H264ppOpenContext    = (H264ppOpenContext_t  *)OSDEV_PrivateData;

    switch( OSDEV_IoctlCode )
    {
	case H264_PP_IOCTL_QUEUE_BUFFER:
			Status = H264ppIoctlQueueBuffer(                H264ppOpenContext, OSDEV_ParameterAddress );
			break;

	case H264_PP_IOCTL_GET_PREPROCESSED_BUFFER:
			Status = H264ppIoctlGetPreprocessedBuffer(      H264ppOpenContext, OSDEV_ParameterAddress );
			break;

	default:        OSDEV_Print( "H264ppIoctl : Invalid ioctl %08x\n", OSDEV_IoctlCode );
			Status = OSDEV_Error;
    }

/* --- */

    OSDEV_IoctlExit( Status );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The function to initialize the hardware device

static void H264ppInitializeDevice( 	void )
{
unsigned int	i;
unsigned int	N;
int		Status;

    //
    // Ensure we will have no interrupt surprises and install the interrupt handler
    //

    for( N=0; N<DeviceContext.NumberOfPreProcessors; N++ )
    {
	OSDEV_WriteLong( PP_ITS(N),                     0xffffffff );           // Clear interrupt status
	OSDEV_WriteLong( PP_ITM(N),                     0x00000000 );

	//
	// Initialize subcontect data
	//

	DeviceContext.PPState[N].ForceWorkAroundGNBvd42331		= true;
	DeviceContext.PPState[N].last_mb_adaptive_frame_field_flag	= 0;	// Doesn't matter, will be initialized on first frame
	DeviceContext.PPState[N].last_entropy_coding_mode_flag	        = 0;	// Doesn't matter, will be initialized on first frame

	//
	// Perform soft reset
	//

	OSDEV_WriteLong( PP_SRS(N),			1 );			// Perform a soft reset
	for( i=0; i<H264_PP_RESET_TIME_LIMIT; i++ )
	{
	    if( (OSDEV_ReadLong(PP_ITS(N)) & PP_ITM__SRS_COMP) != 0 )
		break;

	    OSDEV_SleepMilliSeconds( 1 );
	}

	if( i == H264_PP_RESET_TIME_LIMIT )
	    OSDEV_Print( "H264ppInitializeDevice - Failed to soft reset PP %d.\n", N );

	OSDEV_WriteLong( PP_ITS(N),                     0xffffffff );           // Clear interrupt status

//

	OSDEV_WriteLong( PP_MAX_OPC_SIZE(N),            5 );                    // Setup the ST bus parameters
	OSDEV_WriteLong( PP_MAX_CHUNK_SIZE(N),          0 );
	OSDEV_WriteLong( PP_MAX_MESSAGE_SIZE(N),        3 );

//

#if defined (CONFIG_KERNELVERSION) // STLinux 2.3
	Status  = request_irq( DeviceContext.InterruptNumber[N], H264ppInterruptHandler, IRQF_DISABLED, "H264 PP", (void *)N );
#else
	Status  = request_irq( DeviceContext.InterruptNumber[N], H264ppInterruptHandler, SA_INTERRUPT, "H264 PP", (void *)N );
#endif

	if( Status != 0 )
	    OSDEV_Print( "H264ppInitializeDevice - Unable to request IRQ %d for PP %d.\n", DeviceContext.InterruptNumber[N], N );
    }
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The function to release the hardware device

static void H264ppReleaseDevice(     void )
{
unsigned int    N;

    for( N=0; N<DeviceContext.NumberOfPreProcessors; N++ )
    {
        free_irq(DeviceContext.InterruptNumber[N], (void *)N );
    }
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The function to handle queueing a pre-process to an actual device.

static void H264ppQueueBufferToDevice( 	H264ppProcessBuffer_t	 *Buffer )
{
unsigned int	 N;
H264ppState_t	*State;
unsigned int	 InputBufferBase;
unsigned int	 OutputBufferBase;
unsigned int	 SourceAddress;
unsigned int	 EndAddress;
unsigned int	 SliceErrorStatusAddress;
unsigned int	 IntermediateAddress;
unsigned int	 IntermediateEndAddress;

    //
    // Claim a real pre-processor, then lock while we find which one we are going to use
    //

    OSDEV_ClaimSemaphore( &DeviceContext.PPClaim );

    OSDEV_ClaimSemaphore( &DeviceContext.Lock );

    State	= NULL;
    for( N=0; N<DeviceContext.NumberOfPreProcessors; N++ )
	if( !DeviceContext.PPState[N].Busy )
	{
	    State		= &DeviceContext.PPState[N];
	    State->Busy		= true;
	    State->BufferState	= Buffer;
	    break;
	}

    OSDEV_ReleaseSemaphore( &DeviceContext.Lock );

    if( N == DeviceContext.NumberOfPreProcessors )
    {
	OSDEV_Print( "H264ppQueueBufferToDevice - No free pre-processor - implementation error (should be one, the semaphore says there is).\n" );
	return;
    }

    //
    // Optionally check for and perform the workaround to GNBvd42331
    //

    if( DeviceContext.ApplyWorkAroundForGnbvd42331 )
	H264ppWorkAroundGNBvd42331( State, N );

    //
    // Calculate the address values
    //

    InputBufferBase		= (unsigned int)Buffer->Parameters.InputBufferPhysicalAddress;
    OutputBufferBase		= (unsigned int)Buffer->Parameters.OutputBufferPhysicalAddress;

    SliceErrorStatusAddress     = OutputBufferBase;
    IntermediateAddress         = OutputBufferBase + H264_PP_SESB_SIZE;
    IntermediateEndAddress      = IntermediateAddress + H264_PP_OUTPUT_SIZE - 1;
    SourceAddress               = InputBufferBase;
    EndAddress                  = SourceAddress + Buffer->Parameters.InputSize - 1;

    //
    // Program the preprocessor
    //
    // Standard pre-processor initialization
    //

    dma_cache_wback(Buffer->Parameters.InputBufferCachedAddress,Buffer->Parameters.InputSize);
    dma_cache_wback(Buffer->Parameters.OutputBufferCachedAddress,H264_PP_SESB_SIZE);

    OSDEV_WriteLong( PP_ITS(N),                 0xffffffff );                           // Clear interrupt status

#if 1
    OSDEV_WriteLong( PP_ITM(N),                 PP_ITM__BIT_BUFFER_OVERFLOW |           // We are interested in every interrupt
						PP_ITM__BIT_BUFFER_UNDERFLOW |
						PP_ITM__INT_BUFFER_OVERFLOW |
						PP_ITM__ERROR_BIT_INSERTED |
						PP_ITM__ERROR_SC_DETECTED |
						PP_ITM__SRS_COMP |
						PP_ITM__DMA_CMP );
#else
    OSDEV_WriteLong( PP_ITM(N),                 PP_ITM__DMA_CMP );
#endif

    //
    // Setup the decode
    //

    OSDEV_WriteLong( PP_BBG(N),                 (SourceAddress & 0xfffffff8) );
    OSDEV_WriteLong( PP_BBS(N),                 (EndAddress    | 0x7) );
    OSDEV_WriteLong( PP_READ_START(N),          SourceAddress );
    OSDEV_WriteLong( PP_READ_STOP(N),           EndAddress );

    OSDEV_WriteLong( PP_ISBG(N),                SliceErrorStatusAddress );
    OSDEV_WriteLong( PP_IPBG(N),                IntermediateAddress );
    OSDEV_WriteLong( PP_IBS(N),                 IntermediateEndAddress );

    OSDEV_WriteLong( PP_CFG(N),                 PP_CFG__CONTROL_MODE__START_STOP | Buffer->Parameters.Cfg );
    OSDEV_WriteLong( PP_PICWIDTH(N),            Buffer->Parameters.PicWidth );
    OSDEV_WriteLong( PP_CODELENGTH(N),          Buffer->Parameters.CodeLength );

    //
    // Launch the pre-processor
    //

    Buffer->DecodeTime				= OSDEV_GetTimeInMilliSeconds();
    State->Accumulated_ITS			= 0;

    OSDEV_WriteLong( PP_START(N),               1 );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The function to implement the bug GNBvd42331 workaround

#define GNBvd42331_CFG          0x4DB9C923
#define GNBvd42331_PICWIDTH     0x00010001
#define GNBvd42331_CODELENGTH   0x000001D0

static unsigned char	 GNBvd42331Data[] = 
{
	0x00, 0x00, 0x00, 0x01, 0x27, 0x64, 0x00, 0x15, 0x08, 0xAC, 0x1B, 0x16, 0x39, 0xB2, 0x00, 0x00,
	0x00, 0x01, 0x28, 0x03, 0x48, 0x47, 0x86, 0x83, 0x50, 0x13, 0x02, 0xC1, 0x4A, 0x15, 0x00, 0x00,
	0x00, 0x01, 0x65, 0xB0, 0x34, 0x80, 0x00, 0x00, 0x03, 0x01, 0x6F, 0x70, 0x00, 0x14, 0x0A, 0xFF,
	0xF6, 0xF7, 0xD0, 0x01, 0xAE, 0x5E, 0x3D, 0x7C, 0xCA, 0xA9, 0xBE, 0xCC, 0xB3, 0x3B, 0x50, 0x92,
	0x27, 0x47, 0x24, 0x34, 0xE5, 0x24, 0x84, 0x53, 0x7C, 0xF5, 0x2C, 0x6E, 0x7B, 0x48, 0x1F, 0xC9,
	0x8D, 0x73, 0xA8, 0x3F, 0x00, 0x00, 0x01, 0x0A, 0x03
};

static unsigned char	*GNBvd42331DataPhysicalAddress	= NULL;

//

static void H264ppWorkAroundGNBvd42331( H264ppState_t		 *State,
					unsigned int		  N )
{
unsigned int	i;
unsigned int	mb_adaptive_frame_field_flag;
unsigned int	entropy_coding_mode_flag;
unsigned int	PerformWorkaround;
unsigned int	SavedITM;
unsigned int	BufferBase;
unsigned int	SourceAddress;
unsigned int	EndAddress;
unsigned int	SliceErrorStatusAddress;
unsigned int	IntermediateAddress;
unsigned int	IntermediateEndAddress;

    //
    // Do we have to worry.
    //

    mb_adaptive_frame_field_flag			= ((State->BufferState->Parameters.Cfg & 1) != 0);
    entropy_coding_mode_flag				= ((State->BufferState->Parameters.Cfg & 2) != 0);

    PerformWorkaround					= !mb_adaptive_frame_field_flag && 
							  State->last_mb_adaptive_frame_field_flag &&
							  entropy_coding_mode_flag;

    State->last_mb_adaptive_frame_field_flag	= mb_adaptive_frame_field_flag;

    if( !PerformWorkaround && !State->ForceWorkAroundGNBvd42331 && entropy_coding_mode_flag == State->last_entropy_coding_mode_flag)
	return;

//OSDEV_Print( "H264ppWorkAroundGNBvd42331 - Deploying GNBvd42331 workaround block to PP %d - %08x.\n", N, State->BufferState->Parameters.Cfg );
    //last entropy coding mode is the new one
    State->last_entropy_coding_mode_flag = entropy_coding_mode_flag;

    State->ForceWorkAroundGNBvd42331	= 0;

    //
    // we transfer the workaround stream to the output buffer (offset by 64k to not interfere with the output).
    //

    memcpy( (void *)((unsigned int)State->BufferState->Parameters.OutputBufferCachedAddress + 0x10000), GNBvd42331Data, sizeof(GNBvd42331Data) );

    GNBvd42331DataPhysicalAddress	= (unsigned char *)State->BufferState->Parameters.OutputBufferPhysicalAddress + 0x10000;

    dma_cache_wback((State->BufferState->Parameters.OutputBufferCachedAddress + 0x10000),sizeof(GNBvd42331Data));

    //
    // Derive the pointers - we use the next buffer to be queued as output as our output 
    //

    BufferBase                  = (unsigned int)State->BufferState->Parameters.OutputBufferPhysicalAddress;

    SliceErrorStatusAddress     = BufferBase;
    IntermediateAddress         = BufferBase + H264_PP_SESB_SIZE;
    IntermediateEndAddress      = IntermediateAddress + H264_PP_OUTPUT_SIZE - 1;
    SourceAddress               = (unsigned int)GNBvd42331DataPhysicalAddress;
    EndAddress                  = (unsigned int)GNBvd42331DataPhysicalAddress + sizeof(GNBvd42331Data) - 1;

    //
    // Launch the workaround block
    //

    SavedITM		= OSDEV_ReadLong(PP_ITM(N));			// Turn off interrupts
    OSDEV_WriteLong( PP_ITM(N), 0 );

    OSDEV_WriteLong( PP_BBG(N),                 (SourceAddress & 0xfffffff8) );
    OSDEV_WriteLong( PP_BBS(N),                 (EndAddress    | 0x7) );
    OSDEV_WriteLong( PP_READ_START(N),          SourceAddress );
    OSDEV_WriteLong( PP_READ_STOP(N),           EndAddress );

    OSDEV_WriteLong( PP_ISBG(N),                SliceErrorStatusAddress );
    OSDEV_WriteLong( PP_IPBG(N),                IntermediateAddress );
    OSDEV_WriteLong( PP_IBS(N),                 IntermediateEndAddress );

    OSDEV_WriteLong( PP_CFG(N),                 GNBvd42331_CFG );
    OSDEV_WriteLong( PP_PICWIDTH(N),            GNBvd42331_PICWIDTH );
    OSDEV_WriteLong( PP_CODELENGTH(N),          GNBvd42331_CODELENGTH );

    OSDEV_WriteLong( PP_ITS(N),                 0xffffffff );		// Clear interrupt status
    OSDEV_WriteLong( PP_START(N),               1 );

    //
    // Wait for it to complete
    //

    for( i=0; i<H264_PP_RESET_TIME_LIMIT; i++ )
    {
	OSDEV_SleepMilliSeconds( 1 );

	if( (OSDEV_ReadLong(PP_ITS(N)) & PP_ITM__DMA_CMP) != 0 )
	    break;

    }

    if( (i == H264_PP_RESET_TIME_LIMIT) || (OSDEV_ReadLong(PP_ITS(N)) != PP_ITM__DMA_CMP) )
	OSDEV_Print( "H264ppWorkAroundGNBvd42331 - Failed to execute GNBvd42331 workaround block to PP %d (ITS %08x).\n", N, OSDEV_ReadLong(PP_ITS(N)) );

    //
    // Restore the interrupts
    //

    OSDEV_WriteLong( PP_ITS(N),                     0xffffffff );           // Clear interrupt status
    OSDEV_WriteLong( PP_ITM(N), SavedITM );

}
