#ifndef __STM_DATA_INTERFACE_H
#define __STM_DATA_INTERFACE_H

#include <stm_common.h>

#define STM_DATA_INTERFACE_PULL "stm-data-interface-pull"
#define STM_DATA_INTERFACE_PUSH "stm-data-interface-push"
#define STM_DATA_INTERFACE_PUSH_NOTIFY "stm-data-interface-push-notify"
/*TODO : SE to change name to clear name for direct capture interface*/
#define STM_DATA_INTERFACE_PUSH_RELEASE "stm-data-interface-push-release"
#define STM_DATA_INTERFACE_SWTS_PUSH "stm-data-interface-swts-push"
#define STM_DATA_INTERFACE_PUSH_GET "stm-data-interface-push-get"
#define STM_DATA_INTERFACE_SECURE_SINK "stm-data-interface-secure-sink"

/* Simple scatterlist, optionally supports chaining */
typedef struct stm_data_block {
	void *data_addr;
	uint32_t len;
	struct stm_data_block *next;
} stm_data_block_t;

/* Indicates if the pull/push calls
 * are to be taken as blocking or non-blocking */
typedef enum {
	STM_IOMODE_BLOCKING_IO = 0x0000, /*!< synchronous can wait */
	STM_IOMODE_NON_BLOCKING_IO = 0x0001, /*!< synchronous no waits */
	STM_IOMODE_STREAMING_IO = 0x0002, /*!< asynchronous */
} stm_memory_iomode_t;

/* Indicates what address the blocks should contain */
typedef enum stm_data_mem_type {
	USER = 0,
	KERNEL_VMALLOC,
	KERNEL,
	PHYSICAL,
} stm_data_mem_type_t;

typedef enum stm_memsink_control_e {
	STM_MEMSINK_USE_SHARED_DATA_POOL, /*!< The user asks Memory Sink to extract
					    data via a shared data pool.          */
	STM_MEMSINK_OPAQUE_CTRL,
} stm_memsink_control_t;


/* Allows the sink to express to the source
 * the alignemnt of data. It is up to the source
 * to conform to this */
#define ALIGN_DO_NOT_CARE (0)
#define ALIGN_SYSTEM_WORD (4)
#define ALIGN_CACHE_LINE  (32)

/* Supplied by a source object to the sink object in the sources
 * call to connect*/
typedef struct stm_data_interface_pull_src {
	int __must_check (*pull_data)(stm_object_h src_object,
				      struct stm_data_block *block_list,
				      uint32_t block_count,
				      uint32_t *data_blocks_filled);
	int __must_check (*test_for_data)(stm_object_h src_object, uint32_t *size);

	int __must_check (*set_compound_control)(stm_object_h src_object,
						 stm_memsink_control_t selector,
						 const void *value,
						 uint32_t	size);

	int __must_check (*get_compound_control)(stm_object_h src_object,
						 stm_memsink_control_t selector,
						 void	*value,
						 uint32_t	size);

} stm_data_interface_pull_src_t;


/* Interface that is registered by an object implementing the pull
 * abstraction. The source will use these functions to connect (supplying
 * it's own functions that will be used to obatin data) and disconnect,
 * It is up to the connecting source to ensure that data available via
 * the pull method (see above) meets the requirement of the sink */
typedef struct stm_data_interface_pull_sink {
	int __must_check (*connect)(stm_object_h src_object,
				    stm_object_h sink_object,
				    struct stm_data_interface_pull_src *pull_src);
	int __must_check (*disconnect)(stm_object_h src_object,
				       stm_object_h sink_object);
	int __must_check (*notify)(stm_object_h sink_object,
				   unsigned int event_id);
	enum stm_data_mem_type mem_type;
	stm_memory_iomode_t mode;
	uint32_t alignment;
	uint32_t max_transfer;
	uint32_t paketized;


} stm_data_interface_pull_sink_t;

/* Interface that is registered by an object implementing the push
 * abstraction. The source will use these functions to connect,
 * disconnect and supply data. The data to supply must conform to
 * the requirements of the sink as stated in this definition. */
typedef struct stm_data_interface_push_sink {
	int __must_check (*connect)(stm_object_h src_object,
				    stm_object_h sink_object);
	int __must_check (*disconnect)(stm_object_h src_object,
				       stm_object_h sink_object);
	int (*push_data)(stm_object_h sink_object,
			 struct stm_data_block *block_list,
			 uint32_t block_count,
			 uint32_t *data_blocks);
	enum stm_data_mem_type mem_type;
	stm_memory_iomode_t mode;
	uint32_t alignment;
	uint32_t max_transfer;
	uint32_t paketized;


} stm_data_interface_push_sink_t;

/* Interface that is registered by an object implementing the Memsrc
 * abstraction. The source will use these functions to notify
 * events. */
typedef struct stm_data_interface_push_notify {
	int __must_check (*notify)(stm_object_h src_object,
				   unsigned int event_id);
} stm_data_interface_push_notify_t;

/* For V4l_attach */
/*TODO : SE to change name to clear name for direct capture interface*/
/* Supplied by a source object to the sink object in the sources
 * call to connect*/

typedef struct stm_data_interface_release_src {
	int __must_check (*release_data)(stm_object_h src_object,
					 stm_object_h released_object);
} stm_data_interface_release_src_t;

/* Interface that is registered by an object implementing the push/release abstraction.
 * The source will use these functions to connect, disconnect and supply data.
 * The data to supply must conform to the requirements of the sink as stated in this definition.
 * The sink will use release interface provided with the connect to release the object
 */
typedef struct stm_data_interface_push_release_sink {
	int __must_check (*connect)(stm_object_h src_object,
				    stm_object_h sink_object,
				    struct stm_data_interface_release_src *release_src);
	int __must_check (*disconnect)(stm_object_h src_object,
				       stm_object_h sink_object);
	int (*push_data)(stm_object_h sink_object,
			 stm_object_h pushed_object);
} stm_data_interface_push_release_sink_t;


/* This interface is used so that any sink connected to the pixel capture can
 * provide empty buffers and receive filled ones.
 *
 * In the specific case of usage of stm_data_interface_push_get_sink_t for pixel
 * capture:
 * void *data is casted as a struct *stm_i_push_get_sink_get_desc for get_buffer
 * void *data is casted as a struct *stm_i_push_get_sink_push_desc for push_buffer
 *
 * (*get_buffer):
 * Caller of get_buffer fills the following fields of stm_i_push_get_sink_get_desc
 * stm_i_push_get_sink_get_desc.width
 * stm_i_push_get_sink_get_desc.height
 * stm_i_push_get_sink_get_desc.format
 *
 * Callee of get_buffer allocates the requested bytes using a fixed alignment and
 * fill the following fields of stm_i_push_get_sink_get_desc.
 * stm_i_push_get_sink_get_desc.pitch
 * stm_i_push_get_sink_get_desc.video_buffer_addr
 * stm_i_push_get_sink_get_desc.chroma_buffer_offset
 *
 * (*push_buffer):
 * Caller of push_buffer (ie the stkpi capture component) fills all the fields of
 * stm_pixel_capture_buffer_descr_t at the exception of length & rgb_address.
 *
 * Caller of push_buffer writes stm_pixel_capture_buffer_descr_s.bytesused = 0; to
 * signal that:
 * 1 buffers are not needed any-more and can be recycled
 * 2 and content shall not be displayed.
 *
 * (*notify_flush_start):
 * buffer consumer call this api to signal the start of a buffer flushing operation
 * to the allocator. This function implementation is optional and buffer consumer
 * must check it is not NULL prior to calling it.
 *
 * (*notify_flush_end):
 * buffer consumer call this api to signal the end of a buffer flushing operation
 * to the allocator. This function implementation is optional and buffer consumer
 * must check it is not NULL prior to calling it.
 */

typedef struct stm_data_interface_push_get_sink {
	int (*get_buffer)(stm_object_h sink_object,
			  void *data);
	int (*push_buffer)(stm_object_h sink_object,
			   void *data);
	void (*notify_flush_start)(stm_object_h sink_object);
	void (*notify_flush_end)(stm_object_h sink_object);
} stm_data_interface_push_get_sink_t;

/* Interface implemented by sinks that support secure data path (SDP).
 *
 * This interface is used only for setting up and shutting down a SDP between a
 * source and a sink object.  The actual payload transfer still goes through
 * stm_data_interface_push_sink_t.
 *
 * By registering this interface a sink object informs its potential sources
 * that it supports SDP.  Source objects can therefore test whether their sink
 * supports this interface for deciding whether to enable SDP, i.e. create one
 * or more secure regions for exchanging data with the sink.
 *
 * When a source object enables SDP, it must call connect exactly once before
 * (and disconnect exactly once after) transferring data to the sink.  This
 * allows the sink to determine whether to grant and revoke access to the secure
 * regions.
 *
 * Optionally, a source object can pre-announce at connect-time the secure
 * regions used.  This saves the sink from granting and revoking access for each
 * data transfer.
 *
 * Unstable interface: may evolve during SDP development.
 */
typedef struct stm_data_interface_secure_sink {
	/* Inform sink that SDP is enabled between src_object and sink_object
	 * and optionally pre-announce secure regions.
	 *
	 * block_list: Pointer to array describing pre-announced secure region
	 * or NULL.
	 * block_count: Number of pre-announced regions in block_list[].
	 *
	 * Return 0 on success or -errno.
	 */
	int __must_check (*connect)(stm_object_h src_object,
				stm_object_h sink_object,
				struct stm_data_block *block_list,
				uint32_t block_count);

	/* Inform sink that SDP is about to be disabled between src_object and
	 * sink_object.
	 *
	 * The sink should revoke pre-announced regions.
	 *
	 * Return 0 on success or -errno.
	 */
	int __must_check (*disconnect)(stm_object_h src_object,
			stm_object_h sink_object);
} stm_data_interface_secure_sink_t;

#endif /* __STM_DATA_INTERFACE_H */
