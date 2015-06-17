#ifndef MEM_SINK_H
#define MEM_SINK_H

#include "stm_data_interface.h"

/**
 @defgroup MemorySink  Memory Sink
  @{
*/

/*! this is the name of the momry sink interface*/
#define STM_MEMORYSINK_INTERFACE	      "memsink-interface"

/*! Silent memory sink object */
struct stm_memsink_s;
/*! Memory source handle */
typedef struct stm_memsink_s *stm_memsink_h;


/*!
 *  *\par Description
 *   * List of event linked with an memsink object.
 *    *\par
 *     * The list is subject to be extended in the future.
 *     */
typedef enum stm_memsink_event_e {
	STM_MEMSINK_EVENT_DATA_AVAILABLE = (1 << 0), /*!<
	    If the I/O mode is set to ::STM_IOMODE_STREAMING_IO the Memory Sink
	    triggers this event every time new data is available. */
	STM_MEMSINK_EVENT_BUFFER_OVERFLOW = (1 << 1), /*!<
	     The memory source shall raise this event when buffer is full */

} stm_memsink_event_t;

/*!
\param[in]    name    ASCII name of the sink context
\param[in]    mode    I/O mode mask
\param[in]    address_space   Specified the address space for later buffers
\param[out]   memsink_ctx     Handle of a new sink context

\pre
The Memory Sink has been initialized (transparent to the application and
triggered by STKPI system initialization).
\par Description
Create a new sink context.
\par
An ASCII \p name for the sink context must be specified. The new operation
registers (see STKPI Resource Manager API [3]) the context with the specified
name (this enables the user to monitor the status of the object via services
like /proc, /sysfs or debug file systems)
\par
The I/O \p mode must be specified and can be modified after the context
creation (mode is a \p bitmask) and, unless otherwise specified, it is valid
for all the following I/O operation.  This function is not influenced by the
    <b>“I/O mode”</b> and can be suspended to wait for enough resources to
    allocate the new context.
\par
After creation the context must be connected, otherwise any other operation,
but the delete, is refused.
\par
The Memory  Sink is the latest stage in a STKPI pipeline, so it is due to the
upstream producer to attach and detach the Sink. For this reason the Memory
Sink doesn’t expose an Attach API.


\returns
- \p 0 No error (a valid object handle is returned)
- \p -ENOMEM Not enough resource to create the new object
- \p -EINVAL Invalid parameters (\p name or \p sink_ctx = NULL or \p mode
	     mask has an invalid combination)
- \p -ENOSYS The bit mask \p mode has an invalid combination (or the defined
	     behavior is not supported)

*/
int __must_check
stm_memsink_new(const char *name,
		stm_memory_iomode_t mode,
		stm_data_mem_type_t address_space,
		stm_memsink_h * memsink_ctx);








/*! \fn static int stm_memsink_attach(stm_object_h sink, stm_object_h source, const char* src_interface_type)
\par Description
Not exposed as User API!
The user is not allowed to call this function. This exists as an internal API,
it is exposed by the Memory Sink interface and it is triggered by the upstream
producer to complete the attach task.
\returns
- \p 0 No error (a valid object handle is returned)
*/


/*! \fn stm_memsink_get_control()
\par Description
Currently not implemented, the defined type for sink control do not
need this operation.
*/



/*!
\param[in]  memsink_ctx     Handle of data extraction context.
\param[in]  selector        selector identifying the option to set or the
			    action to do (an enumerate).
\param[in]  value   Control value to set, when not used must be set to 0 (zero)

 */
int __must_check
stm_memsink_get_control(stm_memsink_h memsink_ctx,
			stm_memsink_control_t selector,
			uint32_t value);


/*!
\param[in]    memsink_ctx Handle of data extraction context.
\param[in]    selector    Selector identifying the option to set or the action
			  to do (an enumerate).
\param[in]    value       Control value to set, when value useless (selector is
			  a command ) must be set to 0 (zero)
\pre
The Memory Sink contexts has been created. Not necessarily the context has to
be connected to accept control settings, preconditions can be specific per
control. The semantic of control implicitly defines the preconditions,
otherwise it is explicitly stated.
\par Description
This function controls the behavior of the memory sink context (settings values,
behavioral flags or triggering specific actions).
\par
Some controls have a local scope (concern only memory sink), other may depend on
the capabilities of the upstream producer.
\par
Below the list of all available controls and the rule of use:
\par \p STM_MEMSINK_FLUSH_SHARED_POOL
TODO To be refined: not sure this action alone has great value – should be a
compound atomic action: flush and stop production.
\par
Is allowed only if the memory context is operating with a shared data pool
(see stm_memsink_set_compound_control()).  Forces to flush all the outstanding
data waiting to be extracted by the user.  Value is not used and must be set to
0 (zero)
\par
The function may wait and returns to caller only when the pool is empty

\returns
- \p 0       No errors
- \p -ENXIO  Invalid object (\p memsink_ctx is a not a memsource object
	     or may be \p NULL)
- \p -EINVAL Invalid parameters (the specified selector is wrong)
- \p -EDOM   The  \p selector is valid but the value is out of a valid range
- \p -EPERM  The operation isn’t allowed because of the context state (out of
	     sequence or incompatible with attributes)
- \p -ENOSYS The \p selector is valid but not supported (or not allowed by upstream producer)


*/
int __must_check
stm_memsink_set_control(stm_memsink_h memsink_ctx,
			stm_memsink_control_t selector,
			uint32_t value);



/*! \fn stm_memsink_get_compound_control()
\par Description
Currently not implemented. The set of compound controls is still too limited,
and part of the logic that could be in the scope of
stm_memsink_get_compound_control() (return the shape of shared data pool) is done
by  stm_memsink_set_compound_control(). As a pro, this simplifies
the user programming model and the API
*/


/*!
\param[in]     memsink_ctx Handle of sink context.
\param[in]     selector    Set of control values to set.
\param[in,out] value       References of a data structure consistent
			   with the selector
\param[in]   size  Size of area to store \p value.
\pre
The memory sink context exists and has been successfully connected.
\par Description
In a single transaction, this function enables behaviors of Memory Sink, which
requires a  collections of values that have to be handled together.
\par
Below the list of supported selector:
\par \p STM_MEMSINK_USE_SHARED_DATA_POOL
Informs Memory Sink that the caller requires to extract data from a shared pool
(no-copy). If the user desire to set this extraction model, this function must
be called after the attach and before any other I/O operation
\par
The argument value shall point to a structure of type ::stm_memory_pool_t.
\par
The user is allowed to set \b only the following fields of the structure, all
the other must be null (zero):
 - \b pool_size:  Specifies the desired pool size.  The effective pool size is
 assumed by STKPI components. If the desired is greater the function will
 return the real size
 - \b blocks: Specifies the desired number of blocks. Logic is as above (in
 order to support Quality Of Service the upstream producer may decide to pose
 strong limits to the number of sharable blocks – example the decoded frames)
 - \b ub_threshold / \b lb_threshold: both are  percentage of pool_size. If the
 data production overcomes the ub_ thresholds the memory sink triggers an
 appropriate event. On memory sink the lb_threshold is less important
 (thresholds mgmt still to consolidate)
\par
On return the structure will report the real shape of the pool, or an error
status if the upstream producer do not support the data sharing.
\par
If the function succeeds, the upstream producer supports the data sharing, and
on return the user can check and use the pool attributes to adapt its extraction
behavior:
 - \b block_max_size / \b block_min_size: the I/O request must be within the
 limits
 - \b packet_length: If not 0 (zero), the extracted blocks, tagged as
 ::STM_MEMORY_DATA_GENERIC (default) shall be multiple of packet_length and
 between limits.
 - \b metadata: if not zero means the upstream_producer is capable to generate
 data blocks tagged as ::STM_MEMORY_METADATA  (max size and interleaving logic
 to be defined)
\returns
 - \p 0       No errors
 - \p -ENXIO  Invalid object (\p memsink_ctx is a not a memory sink object or
	      may be \p NULL)
 - \p -EINVAL Invalid parameters (\p selector, or \p value are not valid)
 - \p -ENOSYS The selector is valid but the functionality is not supported
 - \p -EPERM  The operation is not allowed because conflicts with the state of
	      the context (out of sequence)
 - \p -EDOM   The selector is valid but fields of structure referred by value
	      contain illegal values (out of domain)

*/
int __must_check
stm_memsink_set_compound_control(stm_memsink_h memssink_ctx,
				 stm_memsink_control_t selector,
				 const void *value,
				 uint32_t size);

int __must_check
stm_memsink_get_compound_control(stm_memsink_h memssink_ctx,
				 stm_memsink_control_t selector,
				 void *value,
				 uint32_t size);
/*!
\param[in]    memsink_ctx     Handle of injection context.
\param[in]    data_addr       Address of data block.
\param[in]    data_length     Space available.
\param[out]   extracted_size  Number of bytes really extracted

\pre
The Sink contexts has been created and successfully connected.
\par Description
This is the most simple form of raw data extraction.
\par
Does not allows data tagging (always assumes data generic).
\par
The function attempts to extract \p data_length bytes and to store them at
location \p data_addr. Assumes the memory space, at \p data_addr is allocated
by caller and implies a data copy. It is recommended for limited transfers
(like demux sections, indexes, ES user data).
\par
If ::STM_IOMODE_WAIT is set the caller is suspended only if no data is
available. Otherwise the function returns the available amount of data, limited
by \p data_length and  indicated by \p extracted_size.
If ::STM_IOMODE_NO_WAIT  is set the caller is not suspended.  The function
returns even if no data is available (extracted_size = 0). In this case it is
supposed the caller polls or waits for an event
::STM_MEMSINK_EVENT_DATA_AVAILABLE to continue the extraction.
\note nothing in the design prevents to accept this function also in case of shared data  model, however this seems to be redundant since the Memory Sink would deal with the shared data pool (hiding the mechanism),  and would copy of the data  from shared blocks into user buffer (data_addr).  At the time of writing none of the known use cases seems to get advantage from a similar schema, so the current implementation returns an error if this function is used in a sharing model. Could be changed in the future.


\returns
 - \p 0       No errors
 - \p -ENXIO  Invalid object (\p memsink_ctx is a not a memory sink object
	      or may be  \p NULL)
 - \p -EINVAL Invalid parameters (\p data_addr or \p extracted_size are NULL)
 - \p -EPERM  The operation is refused because of the context  state
	      (not in the right sequence).
 - \p -ENOSYS The operation is refused if STM_MEMSINK_USE_POOL has been
	      successfully set
 - \p -ECONNRESET The sink context has been detached while the extractor was
		  waiting (this may happen asynchronously since the detach is
		  triggered by the upstream producer)

*/
int __must_check
stm_memsink_pull_data(stm_memsink_h memsink_ctx,
		      void *data_addr,
		      uint32_t data_length,
		      uint32_t *extracted_size);

int __must_check
stm_memsink_test_for_data(stm_memsink_h memsink_ctx, uint32_t *data);



/*! \fn stm_memsink_detach()
\par Description
As for the Attach, this is not exposed as a user API
However an internal \b detach method must exists, it is exposed to other STKPI
components via the Memory Sink interface, registered when Memory Sink
initializes.
\par
The execution of this \b detach method is triggered by the upstream Detach API.
The detach of the upstream producer should stop the data production before
trigger the memory sink \b detach
\par
The memory sink \b detach shall check if a  sink client thread is waiting for
data to extract , this regardless if the model is using a shared data pool or
not. If this is the case, the thread shall be resumed and return the status
\p ECONNRESET with  no data.

\returns
*/

/*!
\param[in]      memsink_ctx     Handle of sink context.
\pre
The object has been initialized and the sink context exists.

\par Description
Destroy the sink context. It can be called immediately after a new. It returns
an error if the context is still connected.
\note the disconnection of a Memory Sink is triggered when the user executes
the \b detach of the upstream producer.
\returns
- \p 0       No errors
- \p -ENXIO  Invalid parameters (memsink_ctx is NULL)
- \p -EBUSY  The operation is not allowed because of the context  state (still connected).

*/
int __must_check
stm_memsink_delete(stm_memsink_h memsink_ctx);

/*!
\param[in]    memsink_ctx     Handle of sink context.
\param[in]    mode            I/O mode mask
\pre
The object has been initialized and the sink context exists.

\par Description
Set the iomode of the current memsink context.
\returns
- \p  0      No errors
- \p -ENXIO  Invalid parameters (memsink_ctx is NULL)
- \p -EINVAL Memsink object attributes not updated in registry.

*/
int __must_check
stm_memsink_set_iomode(stm_memsink_h memsink_ctx,
				     stm_memory_iomode_t iomode);





/**
@}
*/


#endif /* MEM_SINK_H */
