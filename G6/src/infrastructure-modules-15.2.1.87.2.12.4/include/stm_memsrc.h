#ifndef MEM_SRC_H
#define MEM_SRC_H

#include "stm_data_interface.h"

/** @defgroup MemorySource  Memory Source
*
*  @{
*/

/*! this is the name of the memeory source interface */
#define STM_MEMORYSOURCE_INTERFACE    "memsource-interface"

/*! Silent memory source object */
struct stm_memsrc_s;
/*!\par Description
 Memory source handle
 Each new instance of  Memory Source creates a new working context
 (internal structure) to control the injection, \c stm_memsrc_h is an
 <b>opaque handle</b> for this context.*/
typedef struct stm_memsrc_s *stm_memsrc_h;



/*!\par Description
 * List of selectors to modify the behavior, internal values or to instruct
 * Memory Source to perform a specific action.
 * \see stm_memsrc_set_control()
 * \see stm_memsrc_set_compound_control()
 */
typedef enum stm_memsrc_control_e {
	/*! When user and downstream STKPI consumer are sharing a pool of data block
	 * this control instruct Memory Source to force the flush of any outstanding
	 * injected data.*/
	STM_MEMSRC_FLUSH_SHARED_POOL,
	/*! The user asks Memory Source (and the downstream consumer) to exchange
	 * data via a shared data pool.*/
	STM_MEMSRC_USE_SHARED_DATA_POOL,
	/*! The user is working with a shared data pool and has obtained empty data
	 * blocks. If its activity has to be terminate, it can return all the empty
	 * blocks to the pool, even if not used. This to keep consistent shared pool.
	 * \Warning
	 * the user has references to shared data blocks, after this control the
	 * references are no longer valid, if by mistake the user access these
	 * blocks, this leads unpredictable behavior (to be refined – at the time
	 * of writing this control is only foreseen, not implemented)*/
	STM_MEMSRC_RETURN_ALL_EMPTY_BLOCKS
} stm_memsrc_control_t;

/*!\par Description
 *  * List of event returned by a memorysource
 *   */
typedef enum stm_memsrc_event_e {
	STM_MEMSRC_EVENT_CONTINUE_INJECTION = (1 << 0),
	/*! When the I/O mode is set to ::STM_IOMODE_NO_WAIT / STM_IOMODE_STREAMING_IO
	 *  than the memory source shall raise this event every time someone is waiting
	 *  for free space to inject                                                 */
	STM_MEMSRC_EVENT_BUFFER_UNDERFLOW = (1 << 1),
	/*! The memory source shall raise this event if there is no data for consumer to
	 *      consume.                             */
} stm_memsrc_event_t;


/*!
\param[in]  name      ASCII name of the injection context
\param[in]  mode      I/O mode mask
\param[in]  address_space  Specified the type of buffer the src will use
\param[out] memsrc_ctx      Handle of a new injection context
\pre
The Memory Source has been initialized (this is transparent to the application
and managed by STKPI system initialization).
\par Description
Create a new injection context.
\par
An ASCII \p name for the injection context must be specified. The new
operation registers (see STKPI Resource Manager API) the context with the
specified \p name (this enables the user to monitor the status of the object via
services like \p /proc, \p /sysfs or debug file systems)
\par
The I/O \p mode must be specified and can’t be modified after the context
creation (\p mode is a bitmask, see ::stm_memory_iomode_t ) and, unless
otherwise specified, it is valid for all the following operation.  This function
is not influenced by the “<b>I/O mode</b>” and can be suspended to wait for
enough resources to allocate the new context.
\par
After creation the context must be connected, otherwise any other operation,
but the delete, is refused.
\returns
- \p 0 No error (a valid object handle is returned)
- \p -ENOMEM Not enough resource (no memory)) to create a new object
- \p -EINVAL Invalid parameters (\p memsrc_ctx  or \p name = \p NULL,
     or \p mode mask has an invalid combination)
- \p -ENOSYS the asked behavior is not supported
 *
 */
int __must_check
stm_memsrc_new(const char *name,
	       stm_memory_iomode_t mode,
	       stm_data_mem_type_t address_space,
	       stm_memsrc_h * memsrc_ctx);

/*!
\param[in] memsrc_ctx      Handle of this injection context
\param[in] consumer_ctx    Handle of attached consumer
\param[in] interface_type  Specifies the type of interface the memory source
			   has to be attached to.
\pre
The user has created a new instance of Memory Source, has a consumer handle
and knows the type of interfaces it exposes.
\par Description
Connects the Memory Source context with the context of the downstream
consumer in the pipeline (uses the couple \p consumer_ctx and \p interface_type
to select the appropriate interface).
\par
Before the attach, or if the attach fails, any further operation on Memory
Source context shall be refused.  If the attach fails, only the delete shall
be allowed.
\par
The attach performs all the checks to verify the consistency of the two
interfaces. (TODO)
\par
When the downstream consumer supports only one type of interface, the
\p interface_type must be set to null (zero).
\par
The attach operation may suspend the caller and is not influenced by the
“<b>I/O mode flags </b>” specified when the context has been created.


\returns
- \p 0       No errors
- \p -EBUSY  Already connected
- \p -ENOMEM Not enough memory to register  the new connection
- \p -ENXIO  Invalid object (\p memsrc_ctx is a not src object may be \p NULL)
- \p -ENODEV The \p consumer_ctx is \p NULL, does not exists, or does not
	     register the \p interface_type specified (likely a stale
	     information for the user)
- \p -ECONNREFUSED   The connection is refused (source and consumer interfaces
		     are not compatible)

 *
 */
int __must_check
stm_memsrc_attach(stm_memsrc_h memsrc_ctx,
		  stm_object_h consumer_ctx,
		  const char *interface_type);

/*!
\param[in]    memsrc_ctx      Handle of injection context.
\param[in]    selector  selector identifying the option to set. May concerns
			a simple option, a behavioral flag, a tunable or
			just a command
\param[in]    value   Control value to set, if mainingless must be
		      set to 0 (zero)
\pre
The Memory Source contexts has been created. Not necessarily the context has to
be connected to accept control settings, preconditions can be specific per
control. The semantic of control implicitly defines the preconditions,
otherwise it is explicitly stated.
\par Description
This function controls the behavior of the memory source context (settings
values, behavioral flags or triggering specific actions).
\par
Some controls have a local scope (i.e. concerns only memory source), other may
depend on the capabilities of the downstream consumer.
\par
Below the list of all available controls and the rule of use. (The list can be
extended in the future.)
\par \p STM_MEMSRC_FLUSH_SHARED_POOL
Is allowed only if the memory context is operating with a shared data pool
(see stm_memsrc_set_compound_control()).  Forces to flush all the outstanding
data waiting to be processed by the downstream consumer. Value is not used and
must be set to 0 (zero)
\par
The function may wait and returns to caller only when the pool is empty
\par \p STM_MEMSRC_RETURN_ALL_EMPTY_BLOCKS
Is allowed only if the memory context is operating with a shared data pool
(see stm_memsrc_set_compound_control()).  If the user has to quit quickly, this
control allows to return all the empty data blocks previously acquired with a
stm_memsrc_get_empty_blocks() even if not used. This allows to keep consistent
shared data pool.
\par
Value is not used and must be set to 0 (zero)
\par
The function may wait and returns to caller only when the operation is completed.
\par \p STM_MEMSRC_USE_SHARED_DATA_POOL
TODO
\returns
- \p 0       No errors
- \p -ENXIO  Invalid object (\p memsrc_ctx is a not a memsource object
	     or may be \p NULL)
- \p -EINVAL Invalid parameters (\p selector is wrong)
- \p -EPERM  The operation isn’t allowed because of the context state
	    (out of sequence or not compatible with context attributes)
- \p -ENOSYS The \p selector is valid but not applicable (not yet supported
	      or not allowed by the downstream consumer)

 *
 */
int __must_check
stm_memsrc_set_control(stm_memsrc_h memsrc_ctx,
		       stm_memsrc_control_t selector,
		       uint32_t value);

/*! \fn stm_memsrc_get_compound_control()
\par Description
Currently not implemented. The set of compound controls is still too limited,
and part of the logic that could be in the scope of
stm_memsrc_get_compound_control()) (return the shape of shared data pool) is
done by stm_memsrc_set_compound_control(). This simplifies the user programming
model and the API
*/




/*!
\param[in]   memsrc_ctx Handle of injection context.
\param[in]   selector  Set of control values to set.
\param[out]  value References of a data structure consistent with the selector
\param[in]   size  Size of area to store \p value.
\pre
The memory source context exists and has been successfully connected
\par Description
In a single transaction, this function enables a behavior of Memory Source,
which requires a  collections of values that have to be handled together.
\par
Below the list of supported selector
\par \p STM_MEMSRC_USE_SHARED_DATA_POOL
Informs Memory Source that it is required a shared data pool to inject data into
the downstream consumer. If the user desire to set this injection method,
this function must be called after the attach and before any other I/O
operation.
\par
The argument value shall point to a structure of type ::stm_memory_pool_t.
\par
The user is allowed to set only the following fields of the structure, all the
other must be null (zero):
 - \p \b pool_size:  Specifies the desired pool size.  The effective pool size
 is assumed by STKPI components. If the desired is greater the function will
 return the real size
 - \p \b blocks: Specifies the desired number of blocks. Logic is as above.
 - \p \b ub_threshold/lb_threshold: both are  percentage of pool_size. If the
 injection overcomes or gets below the thresholds the memory source triggers
 an appropriate event <b>(still to consolidate)</b> <br>
 On return the structure will report the real shape of the pool, or an error
 status if the downstream consumer do not support the data sharing.<br>
 If the function succeeds means the downstream consumer supports the data
 sharing, and on return the user can check and use the values of all the field
 and adapt its injection behavior:
 - \p \b block_max_size/block_min_size: the I/O request must be within the
 limits
 - \p \b packet_length: If not 0 (zero), the injected blocks, tagged as
 ::STM_MEMORY_DATA_GENERIC (default) must be multiple of \p packet_length and
 between min and max limits.
 - \p \b accept_metadata: if not zero means the downstream consumer accepts
 also data blocks tagged as ::STM_MEMORY_METADATA  (max size and interleaving
 logic to be defined)


\returns
- \p 0       No errors
- \p -ENXIO  Invalid object (\p memsrc_ctx is a not a memory source object
	     or may be \p NULL)
- \p -EINVAL Invalid parameters (\p selector is wrong)
- \p -ENOSYS The \p selector is valid but the functionality is not supported
- \p -EPERM  The operation is not allowed because of the context state
	    (out of sequence)
- \p -EDOM   The selector is valid, but fields of structure referred by value
	    are set with values out of valid domain
*/
int __must_check
stm_memsrc_set_compound_control(stm_memsrc_h memsrc_ctx,
				stm_memsrc_control_t selector,
				const void *value,
				uint32_t size);

/*!
\param[in]    memsrc_ctx      Handle of injection context.
\param[in]    data_addr       Pointer to data block.
\param[in]    data_length     Number of bytes to inject.
\param[out]   injected_size   Number of bytes really injected
\pre
The Memory Source context exists and has been successfully connected.
\par Description.
This is the most simple form of raw data injection.
\par
Does not allows data tagging (always assumes data generic).
\par
This function assumes the data block is allocated by the caller in its space
and implies a data copy, so it is suggested for limited amount of data (demux
packet insertion, key encryption, etc)
\par
If ::STM_IOMODE_WAIT is set the caller can be suspended. The function returns
only when all the data have been consumed or in case of error, it means the
value returned in \p injected_size is always equal to block_length if no error
but in case of error maybe only first bytes have been consumed,
\par
If ::STM_IOMODE_NO_WAIT  is set the caller is not suspended. The function
attempts to inject \p data_length and returns the quantity of data it has been
effectively injected, this can be = 0 even in case of no errors.

\returns
- \p 0 No error
- \p -ENXIO Invalid object (\p memsrc_ctx is not a valid memsrc object )
- \p -EINVAL Invalid parameters (\p injected_size or \p data_addr are invalid)
- \p -EPERM Out of sequence (an attempt to inject when the \p context is deleted
	    or before it is connected)
- \p -ENOSYS The operation is refused if ::STM_MEMSRC_USE_SHARED_DATA_POOL has
	     been successfully set

 */
int __must_check
stm_memsrc_push_data(stm_memsrc_h memsrc_ctx,
		     void *data_addr,
		     uint32_t data_length,
		     uint32_t *injected_size);

/*!
\param[in]      memsrc_ctx      Handle of \b this injection context
\pre
A new instance of Memory Source exists and has been successfully connected to a
STKPI consumer
\par Description
Detach \b this Memory Source context from the downstream object.
\par
Both, this injection context and the consumer context will continue to exist,
although their state shall be reset to the initial state (as just after the
creation – new)
\par
This function is influenced by the <b>“I/O mode”</b> specified when the context
has been created. If the I/O model is based on a shared pool, the detach
operation may suspend the caller waiting for all the outstanding data to be
consumed, unless the I/O mode ::STM_IOMODE_FLUSH_ON_DETACH is set.
\par
The I/O mode ::STM_IOMODE_NO_WAIT has the same effect of
::STM_IOMODE_FLUSH_ON_DETACH.
\par
Before to perform the detach, the user can execute control operations to flush
any outstanding injections (alternative to the I/O mode flush), or to return
all the not yet used empty blocks (if any).
\par
The detach resets the context mode to the defaults and enables only two further
operations:
  -# delete - to destroy the context
  -# attach - to re-attach it (likely to a different downstream consumer).

\returns
- \p 0       No errors
- \p -ENXIO  Invalid parameters (memsrc_ctx is NULL or not a source context)
- \p -EPERM  The operation is not allowed because of the context  state (not
	     connected or there is an associate block pool and the caller still
	     owns empty data blocks).

*/
int __must_check
stm_memsrc_detach(stm_memsrc_h memsrc_ctx);

/*!
\param[in]      memsrc_ctx      Handle of injection context.
\pre
The Memory Source has been initialized and the injection context exists.
\par Description
Destroy the injection context. It returns an error if the context is still
connected.
\par
The function shall not wait.
\returns
- \p 0       No errors
- \p -ENXIO  Invalid object (memsrc_ctx is NULL or not a source context)
- \p -EPERM  The operation is not allowed because of the context  state (i.e.
	     still connected).

*/
int __must_check
stm_memsrc_delete(stm_memsrc_h memsrc_ctx);

/**
 @}
*/

#endif /* MEM_SRC_H */
