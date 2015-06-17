#ifndef __MME_HELPER_H__
#define __MME_HELPER_H__


#if defined(USE_MME)
#include <semaphore.h>
#include <mme.h>
#include <direct/hash.h>
#endif /* USE_MME */
#include <direct/util.h>
#include "sema_helper.h"

#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)

#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef MAX_STREAMING_COMMANDS
  #define MAX_STREAMING_COMMANDS    5
#endif
#ifndef MME_STREAMING_BUFFERSIZE
  #define MME_STREAMING_BUFFERSIZE  (32 * 1024) /* this must _not_ be less
                                                   than 8 k */
#endif


#if defined(USE_MME)
struct _MMEHelper_buffer {
  MME_DataBuffer_t *buffer;
  MME_Command_t     command;
  sem_t             sema;
};
#endif /* USE_MME */

typedef struct _MMECommon
{
  IDirectFBImageProvider_data base;

  const char *name;

  void         *image; /* decoded image data */
  unsigned int  width; /* width of the JPEG/PNG image */
  unsigned int  height; /* height of the JPEG/PNG image */

#ifdef DIRECT_BUILD_DEBUG
  /* performance monitoring */
  struct timeval starttime, endtime;
#endif /* DIRECT_BUILD_DEBUG */

  CoreSurface *decode_surface;

  unsigned int decoded_width;
  unsigned int decoded_height;

#if defined(USE_MME)
  bool name_set;
  const char *transformer_name;

  /* hardware decode specifics */
  MME_TransformerHandle_t Handle;

  MME_DataBuffer_t  **OutDataBuffers;
  MME_ScatterPage_t   OutDataBufferScatterPage;
  MME_DataBuffer_t    OutDataBuffer;

  struct _MMEHelper_buffer SendDataBuffers[MAX_STREAMING_COMMANDS];

  MME_Command_t TransformCommand; /* provide the output buffer and request
                                     transform */

  pthread_mutex_t  pending_commands_lock;
  DirectHash      *pending_commands;
  unsigned int     n_pending_buffers;

  sem_t decode_event;

  int decode_success;

  /* bytes remaining to be decoded */
  unsigned int  bytes;
  unsigned int  packets;

  /* just for statistics */
  unsigned int  n_underflows;
#endif /* USE_MME */
} MMECommon;


#if defined(USE_MME)
/* every user should provide those... */
static void TransformerCallback( MME_Event_t    Event,
                                 MME_Command_t *CallbackData,
                                 void          *UserData );
static DFBResult abort_transformer( struct _MMECommon *mme );
static DFBResult fetch_data( IDirectFBDataBuffer *buffer,
                             MME_DataBuffer_t    *dbuf,
                             int                  len );
static void
_imageprovider_update_transform_params (MMECommon                   * const mme,
                                        void                        * const params,
                                        const CoreSurfaceBufferLock * const lock);
#endif /* USE_MME */


static DFBResult
buffer_to_ptr_copy (IDirectFBDataBuffer * const buffer,
                    void                * ptr,
                    int                  len)
{
  DFBResult ret;

  while (len > 0)
    {
      unsigned int read;

      ret = buffer->WaitForData (buffer, len);
      if (ret == DFB_OK)
        ret = buffer->GetData (buffer, len, ptr, &read);

      if (ret)
        return ret;

      ptr += read;
      len -= read;
    }

  return DFB_OK;
}


#if !defined(USE_MME)

#define mme_helper_LoadLib (data) DFB_FILENOTFOUND
#define mme_helper_UnloadLib (struct _MMECommon * const data) {( )}

#else /* USE_MME */

/* Some Error and Debug Management */


static inline const char *
get_mme_event_string (MME_Event_t ev)
{
  static const char *mme_event_strings[] = {
    "MME_COMMAND_COMPLETED_EVT",
    "MME_DATA_UNDERFLOW_EVT",
    "MME_NOT_ENOUGH_MEMORY_EVT",
    "MME_NEW_COMMAND_EVT",
  };

  return (((unsigned int) ev) < D_ARRAY_SIZE (mme_event_strings))
         ? mme_event_strings[ev]
         : "* Unknown Event code *";
}

static inline const char *
get_mme_state_string (MME_CommandState_t s)
{
  static const char *mme_command_state_strings[] ={
    "MME_COMMAND_IDLE",
    "MME_COMMAND_PENDING",
    "MME_COMMAND_EXECUTING",
    "MME_COMMAND_COMPLETED",
    "MME_COMMAND_FAILED"
  };

  return (((unsigned int) s) < D_ARRAY_SIZE (mme_command_state_strings))
         ? mme_command_state_strings[s]
         : "* Unknown State code *";
}

static inline const char *
get_mme_error_string (MME_ERROR e)
{
  static const char *mme_error_type_strings[] = {
    "MME_SUCCESS",
    "MME_DRIVER_NOT_INITIALIZED",
    "MME_DRIVER_ALREADY_INITIALIZED",
    "MME_NOMEM",
    "MME_INVALID_TRANSPORT",
    "MME_INVALID_HANDLE",
    "MME_INVALID_ARGUMENT",
    "MME_UNKNOWN_TRANSFORMER",
    "MME_TRANSFORMER_NOT_RESPONDING",
    "MME_HANDLES_STILL_OPEN",
    "MME_COMMAND_STILL_EXECUTING",
    "MME_COMMAND_ABORTED",
    "MME_DATA_UNDERFLOW",
    "MME_DATA_OVERFLOW",
    "MME_TRANSFORM_DEFERRED",
    "MME_SYSTEM_INTERRUPT",
    "MME_EMBX_ERROR",
    "MME_INTERNAL_ERROR",
    "MME_NOT_IMPLEMENTED"
  };

  return (((unsigned int) e) < D_ARRAY_SIZE (mme_error_type_strings))
         ? mme_error_type_strings[e]
         : "* Unknown Error code *";
}


#if defined(MME_USE_DLOPEN)
#include <dlfcn.h>
typedef MME_ERROR (*MME_Init_func) (void);
typedef MME_ERROR (*MME_Term_func) (void);
typedef MME_ERROR (*MME_SendCommand_func) (MME_TransformerHandle_t  Handle,
                                           MME_Command_t           *CmdInfo_p);
typedef MME_ERROR (*MME_AbortCommand_func) (MME_TransformerHandle_t Handle,
                                            MME_CommandId_t         CmdId);
typedef MME_ERROR (*MME_AllocDataBuffer_func) (MME_TransformerHandle_t  handle,
                                               MME_UINT                 size,
                                               MME_AllocationFlags_t    flags,
                                               MME_DataBuffer_t        **dataBuffer_p);
typedef MME_ERROR (*MME_FreeDataBuffer_func)  (MME_DataBuffer_t         *DataBuffer_p);
typedef MME_ERROR (*MME_GetTransformerCapability_func) (const char                  *TransformerName,
                                                        MME_TransformerCapability_t *TransformerCapability_p);
typedef MME_ERROR (*MME_InitTransformer_func) (const char *Name,
                                               MME_TransformerInitParams_t *Params_p,
                                               MME_TransformerHandle_t     *Handle_p);
typedef MME_ERROR (*MME_TermTransformer_func) (MME_TransformerHandle_t handle);

static MME_ERROR _mme_helper_MME_func (void)
{
  return MME_DRIVER_NOT_INITIALIZED;
}

static MME_Init_func _MME_Init = (MME_Init_func) _mme_helper_MME_func;
static MME_Term_func _MME_Term = (MME_Term_func) _mme_helper_MME_func;
static MME_SendCommand_func  _MME_SendCommand = (MME_SendCommand_func) _mme_helper_MME_func;
static MME_AbortCommand_func _MME_AbortCommand = (MME_AbortCommand_func) _mme_helper_MME_func;
static MME_AllocDataBuffer_func _MME_AllocDataBuffer = (MME_AllocDataBuffer_func) _mme_helper_MME_func;
static MME_FreeDataBuffer_func  _MME_FreeDataBuffer  = (MME_FreeDataBuffer_func) _mme_helper_MME_func;
static MME_GetTransformerCapability_func _MME_GetTransformerCapability = (MME_GetTransformerCapability_func) _mme_helper_MME_func;
static MME_InitTransformer_func _MME_InitTransformer = (MME_InitTransformer_func) _mme_helper_MME_func;
static MME_TermTransformer_func _MME_TermTransformer = (MME_TermTransformer_func) _mme_helper_MME_func;
#define MME_Init _MME_Init
#define MME_Term _MME_Term
#define MME_SendCommand  _MME_SendCommand
#define MME_AbortCommand _MME_AbortCommand
#define MME_AllocDataBuffer _MME_AllocDataBuffer
#define MME_FreeDataBuffer _MME_FreeDataBuffer
#define MME_GetTransformerCapability _MME_GetTransformerCapability
#define MME_InitTransformer _MME_InitTransformer
#define MME_TermTransformer _MME_TermTransformer

static void *mme_helper_libmme;
#endif /* MME_USE_DLOPEN */

static bool mme_lib_inited;


static void
__attribute__((constructor))
mme_helper_ctor(void)
{
#if defined(MME_USE_DLOPEN)
  if ((mme_helper_libmme = dlopen ("libmme.so", RTLD_LAZY)) != NULL
      || (mme_helper_libmme = dlopen ("libmme_host.so", RTLD_LAZY)) != NULL)
    {
      _MME_Init = dlsym (mme_helper_libmme, "MME_Init");
      _MME_Term = dlsym (mme_helper_libmme, "MME_Term");
      _MME_SendCommand  = dlsym (mme_helper_libmme, "MME_SendCommand");
      _MME_AbortCommand = dlsym (mme_helper_libmme, "MME_AbortCommand");
      _MME_AllocDataBuffer = dlsym (mme_helper_libmme, "MME_AllocDataBuffer");
      _MME_FreeDataBuffer  = dlsym (mme_helper_libmme, "MME_FreeDataBuffer");
      _MME_GetTransformerCapability = dlsym (mme_helper_libmme, "MME_GetTransformerCapability");
      _MME_InitTransformer = dlsym (mme_helper_libmme, "MME_InitTransformer");
      _MME_TermTransformer = dlsym (mme_helper_libmme, "MME_TermTransformer");

      if (unlikely (!_MME_Init || !_MME_Term
                    || !_MME_SendCommand || !_MME_AbortCommand
                    || !_MME_AllocDataBuffer || !_MME_FreeDataBuffer
                    || !_MME_GetTransformerCapability
                    || !_MME_InitTransformer || !_MME_TermTransformer))
        {
          _MME_Init = (MME_Init_func) _mme_helper_MME_func;
          _MME_Term = (MME_Term_func) _mme_helper_MME_func;
          _MME_SendCommand = (MME_SendCommand_func) _mme_helper_MME_func;
          _MME_AbortCommand = (MME_AbortCommand_func) _mme_helper_MME_func;
          _MME_AllocDataBuffer = (MME_AllocDataBuffer_func) _mme_helper_MME_func;
          _MME_FreeDataBuffer  = (MME_FreeDataBuffer_func) _mme_helper_MME_func;
          _MME_GetTransformerCapability = (MME_GetTransformerCapability_func) _mme_helper_MME_func;
          _MME_InitTransformer = (MME_InitTransformer_func) _mme_helper_MME_func;
          _MME_TermTransformer = (MME_TermTransformer_func) _mme_helper_MME_func;

          dlclose (mme_helper_libmme);
          mme_helper_libmme = NULL;

          D_WARN ("Unable to use libmme.so for "MME_TEXT_DOMAIN": (%s)",
                  dlerror ());
        }
      else
#endif /* MME_USE_DLOPEN */
        {
          MME_ERROR mme_res = MME_Init ();
          D_DEBUG_AT (MME_DEBUG_DOMAIN, "MME_Init() returned %d (%s)\n",
                      mme_res, get_mme_error_string (mme_res));
          switch (mme_res)
            {
            case MME_DRIVER_ALREADY_INITIALIZED:
              mme_res = MME_SUCCESS;
            case MME_SUCCESS:
              break;

            default:
              /* probably kernel module or /dev node required */
              D_ONCE ("failed to initialize MME for "MME_TEXT_DOMAIN": %d (%s)",
                      mme_res, get_mme_error_string (mme_res));
              break;
            }

          mme_lib_inited = (mme_res == MME_SUCCESS);
          D_DEBUG_AT (MME_DEBUG_DOMAIN, "mme_lib_inited %d\n", mme_lib_inited);
        }
#if defined(MME_USE_DLOPEN)
    }
  else
    D_INFO ("Couldn't resolve libmme.so for "MME_TEXT_DOMAIN", can't "
            "use MME for hw accelerated decoding\n");
#endif /* MME_USE_DLOPEN */
}

static void
__attribute__((destructor))
mme_helper_dtor(void)
{
  fprintf(stderr, "%s\n", __FUNCTION__);

  /* we can't MME_Term() here - there might be other users of MME still
     active. MME should implement some use counters instead. */
//  MME_ERROR mme_res = MME_Term ();
//  D_DEBUG_AT (MME_DEBUG_DOMAIN, "MME_Term() returned %d (%s)\n",
//              mme_res, get_mme_error_string (mme_res));
  mme_lib_inited = false;

#if defined(MME_USE_DLOPEN)
  if (mme_helper_libmme)
    {
      _MME_Init = (MME_Init_func) _mme_helper_MME_func;
      _MME_Term = (MME_Term_func) _mme_helper_MME_func;
      _MME_SendCommand = (MME_SendCommand_func) _mme_helper_MME_func;
      _MME_AbortCommand = (MME_AbortCommand_func) _mme_helper_MME_func;
      _MME_AllocDataBuffer = (MME_AllocDataBuffer_func) _mme_helper_MME_func;
      _MME_FreeDataBuffer  = (MME_FreeDataBuffer_func) _mme_helper_MME_func;
      _MME_InitTransformer = (MME_InitTransformer_func) _mme_helper_MME_func;
      _MME_TermTransformer = (MME_TermTransformer_func) _mme_helper_MME_func;

      dlclose (mme_helper_libmme);
      mme_helper_libmme = NULL;
    }
#endif /* MME_USE_DLOPEN */
}


/***************************************/

static DFBResult
__attribute__((unused))
_mme_helper_get_capability (struct _MMECommon           * const data,
                            const char                  * const name,
                            MME_TransformerCapability_t * const cap)
{
  volatile u32 _len = 0;
  MME_ERROR res;

  cap->TransformerInfoSize = sizeof (_len);
  cap->TransformerInfo_p = (u32 *) &_len;

  cap->StructSize = sizeof (*cap);

  res = MME_GetTransformerCapability (name, cap);
  if (res != MME_SUCCESS)
    return DFB_FAILURE;

  D_DEBUG_AT (MME_DEBUG_DOMAIN,
              "'%s' 1: sz/v/it/ot/is/p: %u, %u (%x), %.2x%.2x%.2x%.2x, "
              "%.2x%.2x%.2x%.2x, %u, %p (%u bytes)\n",
              name,
              cap->StructSize, cap->Version, cap->Version,
              cap->InputType.FourCC[0], cap->InputType.FourCC[1],
              cap->InputType.FourCC[2], cap->InputType.FourCC[3],
              cap->OutputType.FourCC[0], cap->OutputType.FourCC[1],
              cap->OutputType.FourCC[2], cap->OutputType.FourCC[3],
              cap->TransformerInfoSize, cap->TransformerInfo_p, _len);
  if (_len)
    {
      char *buf = D_CALLOC (1, _len);

      cap->TransformerInfo_p = buf;
      cap->TransformerInfoSize = _len;

      res = MME_GetTransformerCapability (name, cap);
      if (res != MME_SUCCESS)
        {
          D_FREE (buf);
          return DFB_FAILURE;
        }

      D_DEBUG_AT (MME_DEBUG_DOMAIN,
                  "'%s' 2: sz/v/it/ot/is/p: %u, %u (%x), '%c%c%c%c' "
                  "(%.2x%.2x%.2x%.2x), '%c%c%c%c' (%.2x%.2x%.2x%.2x), %u, "
                  "%p (%u bytes)\n",
                  name,
                  cap->StructSize, cap->Version, cap->Version,
                  cap->InputType.FourCC[0], cap->InputType.FourCC[1],
                  cap->InputType.FourCC[2], cap->InputType.FourCC[3],
                  cap->InputType.FourCC[0], cap->InputType.FourCC[1],
                  cap->InputType.FourCC[2], cap->InputType.FourCC[3],
                  cap->OutputType.FourCC[0], cap->OutputType.FourCC[1],
                  cap->OutputType.FourCC[2], cap->OutputType.FourCC[3],
                  cap->OutputType.FourCC[0], cap->OutputType.FourCC[1],
                  cap->OutputType.FourCC[2], cap->OutputType.FourCC[3],
                  cap->TransformerInfoSize, cap->TransformerInfo_p, _len);
      D_DEBUG_AT (MME_DEBUG_DOMAIN, "caps: '%s'\n", buf);

      D_FREE (buf);
      cap->TransformerInfo_p = NULL;
    }

  return DFB_OK;
}

static DFBResult
mme_helper_deinit_transformer (struct _MMECommon * const data)
{
  MME_ERROR ret;

  if (!data->Handle)
    return DFB_OK;

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "terminating %s transformer w/ handle %x\n",
              data->name, data->Handle);

  /* if we are still waiting for the main decode command to finish (because
     we ran out of data but the transformer is still waiting), abort it.
     JPEG needs this. */
  /* FIXME: what about locking? */
  if (direct_hash_lookup (data->pending_commands,
                          data->TransformCommand.CmdStatus.CmdId))
    {
      abort_transformer (data);
      while ((sema_wait_event (&data->decode_event) == -1)
             && errno == EINTR)
        ;
    }

  if (data->n_underflows)
    {
      D_INFO ("%s: %d data underflow(s) during decode\n",
              data->name, data->n_underflows);
      data->n_underflows = 0;
    }

  ret = MME_TermTransformer (data->Handle);
  if (ret != MME_SUCCESS)
    {
      D_WARN ("(%5d) Couldn't terminate %s transformer w/ handle %x: %d (%s)",
              direct_gettid (), data->name, data->Handle, ret,
              get_mme_error_string (ret));
      return DFB_FAILURE;
    }

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "  -> terminated %s transformer w/ handle %x\n",
              data->name, data->Handle);

  if (data->pending_commands)
    {
      pthread_mutex_destroy (&data->pending_commands_lock);
      direct_hash_destroy (data->pending_commands);
      data->pending_commands = NULL;
    }

  data->Handle = 0;
  return DFB_OK;
}

static DFBResult
mme_helper_init_transformer2 (struct _MMECommon * const data,
                              const char const  * const transformer_names[],
                              size_t             transformer_params_size,
                              void              * const transformer_params,
                              unsigned int      * const ret_index,
                              MME_GenericCallback_t Callback)
{
  unsigned int index;

  MME_ERROR ret;
  MME_TransformerInitParams_t params;

  D_ASSUME (data->Handle == 0);
  D_ASSUME ((transformer_params_size == 0 && transformer_params == NULL)
            || (transformer_params_size && transformer_params));

  if (!mme_lib_inited)
    return DFB_NOSUCHINSTANCE;

  params.StructSize = sizeof (params);
  params.Priority   = MME_PRIORITY_BELOW_NORMAL;
  params.Callback         = Callback;
  params.CallbackUserData = data;
  params.TransformerInitParamsSize = transformer_params_size;
  params.TransformerInitParams_p   = transformer_params;

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "initializing %s tranformer:\n", data->name);

  index = 0;
  do
    {
      D_DEBUG_AT (MME_DEBUG_DOMAIN, "  -> %s\n", transformer_names[index]);
      ret = MME_InitTransformer (transformer_names[index],
                                 &params, &data->Handle);
    }
  while (ret != MME_SUCCESS && transformer_names[++index] != NULL);

  if (ret != MME_SUCCESS)
    {
      if (ret != MME_DRIVER_NOT_INITIALIZED
          && ret != MME_DRIVER_ALREADY_INITIALIZED
          && ret != MME_UNKNOWN_TRANSFORMER)
        D_WARN ("(%5d) %s transformer initialisation failed: %s",
                direct_gettid (), data->name, get_mme_error_string (ret));
      data->Handle = 0;
      return DFB_IDNOTFOUND;
    }

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "    -> OK (handle %x)\n", data->Handle);

  D_ASSUME (data->n_pending_buffers == 0);
  data->n_pending_buffers = 0;
  pthread_mutex_init (&data->pending_commands_lock, NULL);
  if ((ret = direct_hash_create (17, &data->pending_commands)) != DFB_OK)
    {
      data->pending_commands = NULL;
      mme_helper_deinit_transformer (data);
    }

  if (ret_index)
    *ret_index = index;

  return ret;
}

static DFBResult
mme_helper_init_transformer (struct _MMECommon * const data,
                             const char const  * const transformer_names[],
                             size_t             transformer_params_size,
                             void              * const transformer_params,
                             unsigned int      * const ret_index)
{
  return mme_helper_init_transformer2 (data, transformer_names,
                                       transformer_params_size,
                                       transformer_params,
                                       ret_index,
                                       &TransformerCallback);
}


/* Create MME_DataBuffer Structures required to map an Existing Buffer */
static void
create_MME_output_data_buffer (struct _MMECommon * const data,
                               MME_DataBuffer_t  * const buf,
                               unsigned int       flags,
                               void              * const dstbuf,
                               unsigned int       size)
{
  buf->StructSize           = sizeof (MME_DataBuffer_t);
  buf->Flags                = flags;
  buf->StreamNumber         = 0;
  buf->NumberOfScatterPages = 1;

  /* create scatter page detail */
  buf->ScatterPages_p  = &data->OutDataBufferScatterPage;
  buf->TotalSize       = size;
  buf->StartOffset     = 0;

  /* scatter page for the data buffer passed in */
  buf->ScatterPages_p[0].Page_p    = dstbuf;
  buf->ScatterPages_p[0].Size      = size;
  buf->ScatterPages_p[0].BytesUsed = 0;
  buf->ScatterPages_p[0].FlagsIn   = 0;
  buf->ScatterPages_p[0].FlagsOut  = 0;
}


static inline DFBResult
buffer_to_mme_copy (IDirectFBDataBuffer * const buffer,
                    MME_DataBuffer_t    * const dbuf,
                    int                  len)
{
  dbuf->ScatterPages_p[0].BytesUsed = len;

  return buffer_to_ptr_copy (buffer, dbuf->ScatterPages_p[0].Page_p, len);
}




static DFBResult
_mme_helper_start_transformer_core (struct _MMECommon     * const data,
                                    size_t                 return_params_size,
                                    void                  * const return_params,
                                    size_t                 params_size,
                                    void                  * const params,
                                    CoreSurface           * const dst_surface,
                                    CoreSurfaceBufferLock * const lock)
{
  DFBResult    res;
  unsigned int buffersize;

  D_ASSERT (data->OutDataBuffers == NULL);

  res = dfb_surface_lock_buffer (dst_surface, CSBR_BACK, CSAID_ACCEL0,
                                 CSAF_WRITE, lock);
  if (res != DFB_OK)
    return res;

  buffersize = lock->pitch * dst_surface->config.size.h;

  data->OutDataBuffers = (MME_DataBuffer_t **) D_MALLOC (sizeof (MME_DataBuffer_t *));
  if (!data->OutDataBuffers)
    goto out;
  create_MME_output_data_buffer (data, &data->OutDataBuffer,
                                 MME_ALLOCATION_PHYSICAL,
                                 lock->addr, buffersize);
  data->OutDataBuffers[0] = &data->OutDataBuffer;

  data->TransformCommand.DataBuffers_p = data->OutDataBuffers;

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "surface locked @ %p (pitch %u)\n",
              lock->addr, lock->pitch);

  /* init the commandstatus */
  memset (&(data->TransformCommand.CmdStatus), 0, sizeof (MME_CommandStatus_t));
  data->TransformCommand.CmdStatus.AdditionalInfoSize = return_params_size;
  data->TransformCommand.CmdStatus.AdditionalInfo_p = return_params;

  /* Setting up the command */
  data->TransformCommand.StructSize = sizeof (MME_Command_t);
  data->TransformCommand.CmdCode    = MME_TRANSFORM;
  data->TransformCommand.CmdEnd     = MME_COMMAND_END_RETURN_NOTIFY;
  data->TransformCommand.DueTime    = (MME_Time_t) 0;
  data->TransformCommand.NumberInputBuffers  = 0;
  data->TransformCommand.NumberOutputBuffers = 1;
  data->TransformCommand.ParamSize  = params_size;
  data->TransformCommand.Param_p    = params;

  data->TransformCommand.DataBuffers_p[0]->ScatterPages_p[0].Size
    = data->TransformCommand.DataBuffers_p[0]->TotalSize;
  data->TransformCommand.DataBuffers_p[0]->ScatterPages_p[0].BytesUsed = 0;
  data->TransformCommand.DataBuffers_p[0]->ScatterPages_p[0].FlagsIn = 0;
  data->TransformCommand.DataBuffers_p[0]->ScatterPages_p[0].FlagsOut = 0;

  return DFB_OK;

out:
  dfb_surface_unlock_buffer (dst_surface, lock);
  return D_OOM ();
}


static DFBResult
__attribute__((unused))
mme_helper_start_transformer (struct _MMECommon     * const data,
                              size_t                 return_params_size,
                              void                  * const return_params,
                              size_t                 params_size,
                              void                  * const params,
                              CoreSurface           * const dst_surface,
                              CoreSurfaceBufferLock * const lock)
{
  MME_ERROR res;

  /* setup transform command */
  res = _mme_helper_start_transformer_core (data,
                                            return_params_size, return_params,
                                            params_size, params,
                                            dst_surface, lock);
  if (res != DFB_OK)
    return res;

  _imageprovider_update_transform_params (data, params, lock);

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "issuing MME_TRANSFORM\n");

  /* lock access to hash, because otherwise the callback could be called
     before we've had a chance to put the command id into the hash */
  D_ASSERT (data->pending_commands != NULL);
  pthread_mutex_lock (&data->pending_commands_lock);

  res = MME_SendCommand (data->Handle, &data->TransformCommand);
  if (res != MME_SUCCESS)
    {
      pthread_mutex_unlock (&data->pending_commands_lock);

      D_WARN ("(%5d) %s: starting transformer failed: %d (%s)",
              direct_gettid (), data->name, res, get_mme_error_string (res));

      dfb_surface_unlock_buffer (dst_surface, lock);

      D_FREE (data->OutDataBuffers);
      data->OutDataBuffers = NULL;

      return DFB_FAILURE;
    }

  direct_hash_insert (data->pending_commands,
                      data->TransformCommand.CmdStatus.CmdId,
                      (void *) 1);
  D_DEBUG_AT (MME_DEBUG_DOMAIN, "sent packet's CmdId is %u (%.8x)\n",
              data->TransformCommand.CmdStatus.CmdId,
              data->TransformCommand.CmdStatus.CmdId);

  deb_gettimeofday (&data->starttime, NULL);

  pthread_mutex_unlock (&data->pending_commands_lock);

  return DFB_OK;
}


static DFBResult
_alloc_send_buffer (MME_TransformerHandle_t   handle,
                    size_t                    size,
                    struct _MMEHelper_buffer * const buffer)
{
  MME_ERROR res;
  size_t    this_size = MIN (size, MME_STREAMING_BUFFERSIZE);

  D_ASSERT (buffer != NULL);
  D_ASSUME (size != 0);
  D_ASSERT (buffer->buffer == NULL);

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "    -> allocating MME buffer for %u bytes\n",
              this_size);

  res = MME_AllocDataBuffer (handle, this_size, MME_ALLOCATION_PHYSICAL,
                             &buffer->buffer);
  if (unlikely (res))
    {
      D_WARN ("(%5d) MME_AllocDataBuffer() for %u bytes failed: %s",
              direct_gettid (), this_size, get_mme_error_string (res));

      return DFB_NOSYSTEMMEMORY;
    }

  buffer->command.StructSize = sizeof (MME_Command_t);
  buffer->command.CmdCode = MME_SEND_BUFFERS;
  buffer->command.CmdEnd  = MME_COMMAND_END_RETURN_NOTIFY;
  buffer->command.DueTime = (MME_Time_t) 0;
  buffer->command.NumberInputBuffers  = 1;
  buffer->command.NumberOutputBuffers = 0;
  buffer->command.DataBuffers_p = &buffer->buffer;
  buffer->command.ParamSize = 0;
  buffer->command.Param_p   = NULL;

  sema_init_event (&buffer->sema, 0);

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "      -> @ %p (%p)\n",
              buffer, buffer->buffer);

  return DFB_OK;
}


static void
mme_helper_calculate_packets (struct _MMECommon * const data)
{
  D_ASSUME (data->bytes == 0);

  /* FIXME: this is BAD (tm) -> think streaming media! */
  data->base.buffer->SeekTo (data->base.buffer, 0);

  /* find out the length of the buffer */
  data->base.buffer->GetLength (data->base.buffer, &data->bytes);

  data->packets = data->bytes / MME_STREAMING_BUFFERSIZE;
  if ((data->packets * MME_STREAMING_BUFFERSIZE) < data->bytes)
    ++data->packets;

  D_DEBUG_AT (MME_DEBUG_DOMAIN,
              "calculated transfer: %u bytes (%u packets, %u bytes each)\n",
              data->bytes, data->packets, MME_STREAMING_BUFFERSIZE);
}

static DFBResult
__attribute__((unused))
mme_helper_send_packets (struct _MMECommon * const data,
                         unsigned int       n_packets)
{
  unsigned int orig_packets;
  unsigned int currentbuffer = currentbuffer;

  D_ASSUME (n_packets != 0);

  if (!data->packets && !data->bytes)
    return DFB_OK;

  D_ASSUME (data->packets != 0);
  D_ASSUME (data->bytes != 0);

  DFBResult res = DFB_OK;

  orig_packets = n_packets = MIN (n_packets, data->packets);
  D_DEBUG_AT (MME_DEBUG_DOMAIN, "Sending %u buffers (out of %u remaining)\n",
              n_packets, data->packets);

//#define MME_OOM
//#define MME_IOERR
//#define MME_SENDFAIL
//#define MME_DATA_CORRUPTION
#if defined(MME_OOM) || defined(MME_IOERR) || defined(MME_SENDFAIL) || defined(MME_DATA_CORRUPTION)
int i = 0;
#endif
  for (; n_packets && data->packets; --data->packets, --n_packets)
    {
      struct _MMEHelper_buffer *buffer;
      unsigned int this_read = MIN (data->bytes, MME_STREAMING_BUFFERSIZE);

      ++currentbuffer;
      currentbuffer %= MAX_STREAMING_COMMANDS;

      buffer = &data->SendDataBuffers[currentbuffer];

      /* some checking - We've waited ... should we continue ... */
      if (data->decode_success == -1)
        {
          D_WARN ("(%5d) ImageProvider/%s: error sending data. Transform "
                  "error reported from callback",
                  direct_gettid (), data->name);
          return DFB_FAILURE;
        }

      /* if we are allocating a new buffer, we don't have to wait on the
         semaphore, as the new buffer will not yet be known to mme, and thus
         not in use, of course */
      if (!buffer->buffer)
        {
#ifdef MME_OOM
if (++i == 4)
  {
    fprintf (stderr, "emulating MME_AllocDataBuffer() failure (MME OOM)\n");
    res = DFB_NOSYSTEMMEMORY;
  }
else
#endif
          res = _alloc_send_buffer (data->Handle, this_read, buffer);
          if (res != DFB_OK)
            return res;
        }
      else
        {
          /* wait for some buffer to be available */
          if (unlikely (sema_trywait (&buffer->sema) == -1))
            {
              ++data->packets;
              ++n_packets;
              usleep (1);
              continue;
            }
        }

      D_DEBUG_AT (MME_DEBUG_DOMAIN, "  -> sending packet %u, using buffer %u @ %p (%p)\n",
                  orig_packets - n_packets, currentbuffer, buffer,
                  buffer->buffer);
      D_DEBUG_AT (MME_DEBUG_DOMAIN, "  -> %u bytes remaining (this read: %u)\n",
                  data->bytes, this_read);

      /* lock access to hash, because otherwise the callback could be called
         before we've had a chance to put the command id into the hash */
      /* it also locks acces to data->buffer, because otherwise the callback
         might determine we ran out of data if called between fetch_data()
         and actually sending the MME_SendCommand() below. */
      D_ASSERT (data->pending_commands != NULL);
      pthread_mutex_lock (&data->pending_commands_lock);

      /* don't move the abort out of the pending_commands lock! The JPEG
         transformer might deadlock otherwise */
      if (data->decode_success == 1)
        {
          /* corrupt data -> file can be bigger than actual JPEG data */
          pthread_mutex_unlock (&data->pending_commands_lock);
          return DFB_OK;
        }

#ifdef MME_IOERR
if (++i == 8)
  {
    fprintf (stderr, "emulating IO (read) error\n");
    res = DFB_FAILURE;
    usleep (1 * 1000 * 1000);
  }
else
#endif
      res = fetch_data (data->base.buffer, buffer->buffer, this_read);
      if (res != DFB_OK)
        {
          pthread_mutex_unlock (&data->pending_commands_lock);

          D_WARN ("(%5d) Fetching %u bytes of data failed: res: %d",
                  direct_gettid (), this_read, res);
          sema_signal_event (&buffer->sema);
          /* hm, DirectFB (always?) returns DFB_FAILURE here... */
          return DFB_IO;
        }
#ifdef MME_DATA_CORRUPTION
if (++i == 2)
  {
    fprintf (stderr, "emulating data corruption\n");
    memset (&buffer->buffer->ScatterPages_p[0].Page_p[15], 0x15, 10);
  }
#endif

      /* we just read data, nobody should have determined that we ran into
         an EOF! */
      D_ASSERT (data->decode_success != -2);

      data->bytes -= this_read;

      D_DEBUG_AT (MME_DEBUG_DOMAIN, "  -> sending command in buffer %u\n",
                  currentbuffer);

      /* send the command */
#ifdef MME_SENDFAIL
if (++i == 4)
  {
    fprintf (stderr, "emulating MME_SendCommand() failure\n");
    res = MME_NOMEM;
  }
else
#endif
      res = MME_SendCommand (data->Handle, &buffer->command);
      if (res != MME_SUCCESS)
        {
          pthread_mutex_unlock (&data->pending_commands_lock);

          D_WARN ("(%5d) send DataCommand failed: %s",
                  direct_gettid (), get_mme_error_string (res));
          res = DFB_FAILURE;
          return DFB_FAILURE;
        }

      direct_hash_insert (data->pending_commands,
                          buffer->command.CmdStatus.CmdId, (void *) 1);
      ++data->n_pending_buffers;
      D_DEBUG_AT (MME_DEBUG_DOMAIN, "sent packet's CmdId is %u (%.8x), %u packets pending now\n",
                  buffer->command.CmdStatus.CmdId,
                  buffer->command.CmdStatus.CmdId,
                  data->n_pending_buffers);
      pthread_mutex_unlock (&data->pending_commands_lock);
    }

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "done sending packets (%u remaining)\n",
              data->packets);

  return res;
}


static DFBResult
__attribute__((unused))
mme_helper_stretch_blit (struct _MMECommon * const data,
                         CoreSurface       * const src,
                         CoreSurface       * const dst,
                         DFBRectangle      * const dst_rect)
{
  CardState    state;
  DFBRectangle src_rect;

  dfb_state_init (&state, data->base.core);

  /* set clipping and sizes */
  state.clip.x1 = 0;
  state.clip.y1 = 0;
  state.clip.x2 = dst_rect->w - 1;
  state.clip.y2 = dst_rect->h - 1;

  state.modified |= SMF_CLIP;

  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.w = data->width;
  src_rect.h = data->height;

  dfb_state_set_source (&state, src);
  dfb_state_set_destination (&state, dst);

  extern const char *dfb_pixelformat_name( DFBSurfacePixelFormat format );
  D_DEBUG_AT (MME_DEBUG_DOMAIN, "StretchBlit %dx%d (%dx%d) -> %dx%d (%s -> %s)\n",
              data->width, data->height, src_rect.w, src_rect.h,
              dst_rect->w, dst_rect->h,
              dfb_pixelformat_name (src->config.format),
              dfb_pixelformat_name (dst->config.format));

  /* thankfully this is intelligent enough to do a simple blit if possible */
  dfb_gfxcard_stretchblit (&src_rect, dst_rect, &state);

  /* remove the state */
  dfb_state_set_source (&state, NULL);
  dfb_state_set_destination (&state, NULL);
  dfb_state_destroy (&state);

  return DFB_OK;
}
#endif /* USE_MME */


#endif /* __MME_HELPER_H__ */
