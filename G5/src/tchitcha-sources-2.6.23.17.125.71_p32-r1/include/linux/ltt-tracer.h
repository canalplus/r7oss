/*
 * Copyright (C) 2005,2006 Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * This contains the definitions for the Linux Trace Toolkit tracer.
 */

#ifndef _LTT_TRACER_H
#define _LTT_TRACER_H

#include <stdarg.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/cache.h>
#include <linux/kernel.h>
#include <linux/timex.h>
#include <linux/wait.h>
#include <linux/relay.h>
#include <linux/ltt-core.h>
#include <linux/marker.h>
#include <linux/ltt.h>
#include <asm/semaphore.h>
#include <asm/atomic.h>
#include <asm/local.h>

/* Number of bytes to log with a read/write event */
#define LTT_LOG_RW_SIZE			32L

/* Interval (in jiffies) at which the LTT per-CPU timer fires */
#define LTT_PERCPU_TIMER_INTERVAL	1

#ifndef LTT_ARCH_TYPE
#define LTT_ARCH_TYPE			LTT_ARCH_TYPE_UNDEFINED
#endif

#ifndef LTT_ARCH_VARIANT
#define LTT_ARCH_VARIANT		LTT_ARCH_VARIANT_NONE
#endif

struct ltt_active_marker;

/* Maximum number of callbacks per marker */
#define LTT_NR_CALLBACKS	10

struct ltt_serialize_closure;
struct ltt_probe_private_data;

/* Serialization callback '%k' */
typedef char *(*ltt_serialize_cb)(char *buffer, char *str,
			struct ltt_serialize_closure *closure,
			void *serialize_private, int align,
			const char *fmt, va_list *args);

struct ltt_serialize_closure {
	ltt_serialize_cb *callbacks;
	long cb_args[LTT_NR_CALLBACKS];
	unsigned int cb_idx;
};

char *ltt_serialize_data(char *buffer, char *str,
			struct ltt_serialize_closure *closure,
			void *serialize_private,
			int align,
			const char *fmt, va_list *args);

struct ltt_available_probe {
	const char *name;		/* probe name */
	const char *format;
	marker_probe_func *probe_func;
	ltt_serialize_cb callbacks[LTT_NR_CALLBACKS];
	struct list_head node;		/* registered probes list */
};

struct ltt_probe_private_data {
	uint16_t channel;		/*
					 * Channel override for probe, used
					 * if non 0.
					 */
	struct ltt_trace_struct *trace;	/*
					 * Specify to which trace the
					 * information must be recorded.
					 * Used if !NULL.
					 */
	int force;			/*
					 * Should we write to traces even if
					 * tracing is stopped ? Internal use
					 * only.
					 */
	uint16_t id;			/*
					 * Core event static ID.
					 * Unused if >= MARKER_CORE_IDS.
					 */
	void *serialize_private;	/*
					 * Private data for serialization
					 * functions.
					 */
	int cpu;			/* CPU on behalf of which the data
					 * must be written. -1 if unset.
					 * Only used if we are in forced
					 * write mode.
					 */
};

struct ltt_active_marker {
	struct list_head node;		/* active markers list */
	const char *name;
	const char *format;
	struct ltt_available_probe *probe;
	int align;
	uint16_t id;
	uint16_t channel;
};

extern void ltt_vtrace(const struct marker *mdata, void *call_data,
	const char *fmt, va_list *args);
extern void ltt_trace(const struct marker *mdata, void *call_data,
	const char *fmt, ...);

/*
 * Unique ID assigned to each registered probe.
 */
enum marker_id {
	MARKER_ID_SET_MARKER_ID = 0,	/* Static IDs available (range 0-7) */
	MARKER_ID_SET_MARKER_FORMAT,
	MARKER_ID_HEARTBEAT_32,
	MARKER_ID_HEARTBEAT_64,
	MARKER_ID_COMPACT,		/* Compact IDs (range: 8-127)	    */
	MARKER_ID_DYNAMIC,		/* Dynamic IDs (range: 128-65535)   */
};

/* static ids 0-7 reserved for internal use. */
#define MARKER_CORE_IDS		8
/* dynamic ids 8-127 reserved for compact events. */
#define MARKER_COMPACT_IDS	128
static inline enum marker_id marker_id_type(uint16_t id)
{
	if (id < MARKER_CORE_IDS)
		return (enum marker_id)id;
	else if (id < MARKER_COMPACT_IDS)
		return MARKER_ID_COMPACT;
	else
		return MARKER_ID_DYNAMIC;
}

#ifdef CONFIG_LTT

struct ltt_trace_struct;

/* LTTng lockless logging buffer info */
struct ltt_channel_buf_struct {
	/* Use the relay void *start as buffer start pointer */
	local_t offset;			/* Current offset in the buffer */
	atomic_long_t consumed;		/* Current offset in the buffer
					 * standard atomic access (shared)
					 */
	atomic_long_t active_readers;	/* Active readers count
					   standard atomic access (shared) */
	atomic_t wakeup_readers;	/* Boolean : wakeup readers waiting ? */
	local_t *commit_count;		/* Commit count per sub-buffer */
	spinlock_t full_lock;		/* buffer full condition spinlock, only
					 * for userspace tracing blocking mode
					 * synchronization with reader.
					 */
	local_t events_lost;
	local_t corrupted_subbuffers;
	struct timeval	current_subbuffer_start_time;
	wait_queue_head_t write_wait;	/* Wait queue for blocking user space
					 * writers */
	struct work_struct wake_writers;/* Writers wake-up work struct */
} ____cacheline_aligned;

struct ltt_channel_struct {
	char channel_name[PATH_MAX];
	struct ltt_trace_struct	*trace;
	struct ltt_channel_buf_struct buf[NR_CPUS];
	int overwrite;
	struct kref kref;
	int compact;

	void *trans_channel_data;

	/*
	 * buffer_begin - called on buffer-switch to a new sub-buffer
	 * @buf: the channel buffer containing the new sub-buffer
	 */
	void (*buffer_begin) (struct rchan_buf *buf,
			u64 tsc, unsigned int subbuf_idx);
	/*
	 * buffer_end - called on buffer-switch to a new sub-buffer
	 * @buf: the channel buffer containing the previous sub-buffer
	 */
	void (*buffer_end) (struct rchan_buf *buf,
			u64 tsc, unsigned int offset, unsigned int subbuf_idx);
};

struct user_dbg_data {
	unsigned long avail_size;
	unsigned long write;
	unsigned long read;
};

struct ltt_trace_ops {
	int (*create_dirs) (struct ltt_trace_struct *new_trace);
	void (*remove_dirs) (struct ltt_trace_struct *new_trace);
	int (*create_channel) (char *trace_name, struct ltt_trace_struct *trace,
				struct dentry *dir, char *channel_name,
				struct ltt_channel_struct **ltt_chan,
				unsigned int subbuf_size,
				unsigned int n_subbufs, int overwrite);
	void (*wakeup_channel) (struct ltt_channel_struct *ltt_channel);
	void (*finish_channel) (struct ltt_channel_struct *channel);
	void (*remove_channel) (struct ltt_channel_struct *channel);
	void *(*reserve_slot) (struct ltt_trace_struct *trace,
				struct ltt_channel_struct *channel,
				void **transport_data, size_t data_size,
				size_t *slot_size, u64 *tsc, int cpu);
	void (*commit_slot) (struct ltt_channel_struct *channel,
				void **transport_data, void *reserved,
				size_t slot_size);
	int (*user_blocking) (struct ltt_trace_struct *trace,
				unsigned int index, size_t data_size,
				struct user_dbg_data *dbg);
	void (*user_errors) (struct ltt_trace_struct *trace,
				unsigned int index, size_t data_size,
				struct user_dbg_data *dbg, int cpu);
#ifdef CONFIG_HOTPLUG_CPU
	int (*handle_cpuhp) (struct notifier_block *nb,
				unsigned long action, void *hcpu,
				struct ltt_trace_struct *trace);
#endif
};

struct ltt_transport {
	char *name;
	struct module *owner;
	struct list_head node;
	struct ltt_trace_ops ops;
};


enum trace_mode { LTT_TRACE_NORMAL, LTT_TRACE_FLIGHT, LTT_TRACE_HYBRID };

/* Per-trace information - each trace/flight recorder represented by one */
struct ltt_trace_struct {
	struct list_head list;
	int active;
	char trace_name[NAME_MAX];
	int paused;
	enum trace_mode mode;
	struct ltt_transport *transport;
	struct ltt_trace_ops *ops;
	struct kref ltt_transport_kref;
	u32 freq_scale;
	u64 start_freq;
	u64 start_tsc;
	unsigned long long start_monotonic;
	struct timeval		start_time;
	struct {
		struct dentry			*trace_root;
		struct dentry			*control_root;
	} dentry;
	struct {
		struct ltt_channel_struct	*facilities;
		struct ltt_channel_struct	*interrupts;
		struct ltt_channel_struct	*processes;
		struct ltt_channel_struct	*modules;
		struct ltt_channel_struct	*network;
		struct ltt_channel_struct	*cpu;
		struct ltt_channel_struct	*compact;
	} channel;
	struct rchan_callbacks callbacks;
	struct kref kref; /* Each channel has a kref of the trace struct */
} ____cacheline_aligned;

/*
 * First and last channels in ltt_trace_struct.
 */
#define ltt_channel_index_size()	sizeof(struct ltt_channel_struct *)
#define ltt_channel_index_begin()	GET_CHANNEL_INDEX(facilities)
#define ltt_channel_index_end()	\
	(GET_CHANNEL_INDEX(compact) + ltt_channel_index_size())

enum ltt_channels { LTT_CHANNEL_FACILITIES, LTT_CHANNEL_INTERRUPTS,
	LTT_CHANNEL_PROCESSES, LTT_CHANNEL_MODULES, LTT_CHANNEL_CPU,
	LTT_CHANNEL_COMPACT, LTT_CHANNEL_NETWORK };

/* Hardcoded event headers
 *
 * event header for a trace with active heartbeat : 32 bits timestamps
 *
 * headers are 8 bytes aligned : that means members are aligned on memory
 * boundaries *if* structure starts on a 8 bytes boundary. In order to insure
 * such alignment, a dynamic per trace alignment value must be set.
 *
 * Remember that the C compiler does align each member on the boundary
 * equivalent to their own size.
 *
 * As relay subbuffers are aligned on pages, we are sure that they are 8 bytes
 * aligned, so the buffer header and trace header are aligned.
 *
 * Event headers are aligned depending on the trace alignment option.
 */

struct ltt_event_header_hb {
	uint32_t timestamp;
	uint16_t event_id;
	uint16_t event_size;
} __attribute((packed));

struct ltt_event_header_nohb {
	uint64_t timestamp;
	uint16_t event_id;
	uint16_t event_size;
} __attribute((packed));

struct ltt_event_header_compact {
	uint32_t bitfield; /* E bits for event ID, 32-E bits for timestamp */
} __attribute((packed));

struct ltt_trace_header {
	uint32_t magic_number;
	uint32_t arch_type;
	uint32_t arch_variant;
	uint32_t float_word_order;	 /* Only useful for user space traces */
	uint8_t arch_size;
	uint8_t major_version;
	uint8_t minor_version;
	uint8_t flight_recorder;
	uint8_t has_heartbeat;
	uint8_t alignment;		/* Event header alignment */
	uint8_t tsc_lsb_truncate;	/* LSB truncate for compact channel */
	uint8_t tscbits;		/* TSC bits kept for compact channel */
	uint8_t compact_data_shift;     /* bit shift for compact data */
	uint32_t freq_scale;
	uint64_t start_freq;
	uint64_t start_tsc;
	uint64_t start_monotonic;
	uint64_t start_time_sec;
	uint64_t start_time_usec;
} __attribute((packed));


/*
 * We use asm/timex.h : cpu_khz/HZ variable in here : we might have to deal
 * specifically with CPU frequency scaling someday, so using an interpolation
 * between the start and end of buffer values is not flexible enough. Using an
 * immediate frequency value permits to calculate directly the times for parts
 * of a buffer that would be before a frequency change.
 */
struct ltt_block_start_header {
	struct {
		uint64_t cycle_count;
		uint64_t freq; /* khz */
	} begin;
	struct {
		uint64_t cycle_count;
		uint64_t freq; /* khz */
	} end;
	uint32_t lost_size;	/* Size unused at the end of the buffer */
	uint32_t buf_size;	/* The size of this sub-buffer */
	struct ltt_trace_header	trace;
} __attribute((packed));

#ifdef CONFIG_LTT_ALIGNMENT

/* Calculate the offset needed to align the type */
static inline unsigned int ltt_align(size_t align_drift,
		 size_t size_of_type)
{
	size_t alignment = min(sizeof(void *), size_of_type);
	return ((alignment - align_drift) & (alignment-1));
}
/* Default arch alignment */
#define LTT_ALIGN

static inline int ltt_get_alignment(struct ltt_active_marker *marker)
{
	return (likely(!marker || marker->align) ? sizeof(void *) : 0);
}

#else
static inline unsigned int ltt_align(size_t align_drift,
		 size_t size_of_type)
{
	return 0;
}

#define LTT_ALIGN __attribute__((packed))

static inline int ltt_get_alignment(struct ltt_active_marker *marker)
{
	return 0;
}
#endif /* CONFIG_LTT_ALIGNMENT */

/*
 * ltt_subbuf_header_len - called on buffer-switch to a new sub-buffer
 *
 * returns the client header size at the beginning of the buffer.
 */
static inline unsigned int ltt_subbuf_header_len(void)
{
	return sizeof(struct ltt_block_start_header);
}

/* Get the offset of the channel in the ltt_trace_struct */
#define GET_CHANNEL_INDEX(chan)	\
	(unsigned int)&((struct ltt_trace_struct *)NULL)->channel.chan

static inline struct ltt_channel_struct *ltt_get_channel_from_index(
		struct ltt_trace_struct *trace, unsigned int index)
{
	return *(struct ltt_channel_struct **)((void *)trace+index);
}


/*
 * ltt_get_header_size
 *
 * Calculate alignment offset for arch size void*. This is the
 * alignment offset of the event header.
 *
 * Important note :
 * The event header must be a size multiple of the void* size. This is necessary
 * to be able to calculate statically the alignment offset of the variable
 * length data fields that follows. The total offset calculated here :
 *
 *	 Alignment of header struct on arch size
 * + sizeof(header struct)
 * + padding added to end of struct to align on arch size.
 * */
static inline unsigned char ltt_get_header_size(
		struct ltt_channel_struct *channel,
		void *address,
		size_t data_size,
		size_t *before_hdr_pad)
{
	unsigned int padding;
	unsigned int header;
	size_t after_hdr_pad;

#ifdef CONFIG_LTT_HEARTBEAT
	if (unlikely(channel->compact))
		header = sizeof(struct ltt_event_header_compact);
	else
		header = sizeof(struct ltt_event_header_hb);
#else
	header = sizeof(struct ltt_event_header_nohb);
#endif

	/* Padding before the header. Calculated dynamically */
	*before_hdr_pad = ltt_align((unsigned long)address, header);
	padding = *before_hdr_pad;

	/*
	 * Padding after header, considering header aligned on ltt_align.
	 * Calculated statically if header size is known. For compact
	 * channels, do not align the data.
	 */
#ifdef CONFIG_LTT_HEARTBEAT
	if (unlikely(channel->compact))
		after_hdr_pad = 0;
	else
		after_hdr_pad = ltt_align(header, sizeof(void *));
#else
	after_hdr_pad = ltt_align(header, sizeof(void *));
#endif
	padding += after_hdr_pad;

	return header+padding;
}

enum ltt_heartbeat_functor_msg { LTT_HEARTBEAT_START, LTT_HEARTBEAT_STOP };

#ifdef CONFIG_LTT_HEARTBEAT

extern int ltt_tsc_lsb_truncate;
extern int ltt_tscbits;
extern int ltt_compact_data_shift;

extern void ltt_init_compact_markers(unsigned int num_compact_events);
extern int ltt_heartbeat_trigger(enum ltt_heartbeat_functor_msg msg);
extern void ltt_write_full_tsc(struct ltt_trace_struct *trace);


static inline char *ltt_write_compact_header(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *channel,
		void *ptr,
		uint16_t eID, size_t event_size,
		u64 tsc, u32 data)
{
	struct ltt_event_header_compact *compact_hdr;
	int tscbits = ltt_tscbits;
	int lsb_truncate = ltt_tsc_lsb_truncate;
	u32 compact_tsc;

	compact_hdr = (struct ltt_event_header_compact *)ptr;
	compact_tsc = ((u32)tsc >> lsb_truncate) & ((1 << tscbits) - 1);
	compact_hdr->bitfield = (data << ltt_compact_data_shift)
				| ((u32)eID << tscbits) | compact_tsc;
	return ptr + sizeof(*compact_hdr);
}

/*
 * ltt_write_event_header
 *
 * Writes the event header to the pointer.
 *
 * @channel : pointer to the channel structure
 * @ptr : buffer pointer
 * @eID : event ID
 * @event_size : size of the event, excluding the event header.
 * @tsc : time stamp counter.
 *
 * returns : pointer where the event data must be written.
 */
static inline char *ltt_write_event_header(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *channel,
		void *ptr,
		uint16_t eID, size_t event_size,
		u64 tsc)
{
	size_t after_hdr_pad;
	struct ltt_event_header_hb *hb;

	event_size = min(event_size, (size_t)0xFFFFU);
	hb = (struct ltt_event_header_hb *)ptr;
	hb->timestamp = (u32)tsc;
	hb->event_id = eID;
	hb->event_size = (uint16_t)event_size;
	if (unlikely(channel->compact))
		after_hdr_pad = 0;
	else
		after_hdr_pad = ltt_align(sizeof(*hb), sizeof(void *));
	return ptr + sizeof(*hb) + after_hdr_pad;
}

#else /* CONFIG_LTT_HEARTBEAT */

static inline void ltt_init_compact_markers(unsigned int num_compact_events) { }
static inline void ltt_write_full_tsc(struct ltt_trace_struct *trace) { }

static inline char *ltt_write_compact_header(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *channel,
		void *ptr,
		uint16_t eID, size_t event_size,
		u64 tsc, u32 data)
{
	return ptr;
}

static inline char *ltt_write_event_header(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *channel,
		void *ptr,
		uint16_t eID, size_t event_size,
		u64 tsc)
{
	size_t after_hdr_pad;
	struct ltt_event_header_nohb *nohb;

	event_size = min(event_size, (size_t)0xFFFFU);
	nohb = (struct ltt_event_header_nohb *)ptr;
	nohb->timestamp = (u64)tsc;
	nohb->event_id = eID;
	nohb->event_size = (uint16_t)event_size;
	after_hdr_pad = ltt_align(sizeof(*nohb), sizeof(void *));
	return ptr + sizeof(*nohb) + after_hdr_pad;
}

#endif /* CONFIG_LTT_HEARTBEAT */

/* Lockless LTTng */

/* Buffer offset macros */

#define BUFFER_OFFSET(offset, chan) ((offset) & (chan->alloc_size-1))
#define SUBBUF_OFFSET(offset, chan) ((offset) & (chan->subbuf_size-1))
#define SUBBUF_ALIGN(offset, chan) \
	(((offset) + chan->subbuf_size) & (~(chan->subbuf_size-1)))
#define SUBBUF_TRUNC(offset, chan) \
	((offset) & (~(chan->subbuf_size-1)))
#define SUBBUF_INDEX(offset, chan) \
	(BUFFER_OFFSET((offset), chan) / chan->subbuf_size)

/*
 * for flight recording. must be called after relay_commit.
 * This function decrements de subbuffer's lost_size each time the commit count
 * reaches back the reserve offset (module subbuffer size). It is useful for
 * crash dump.
 * We use slot_size - 1 to make sure we deal correctly with the case where we
 * fill the subbuffer completely (so the subbuf index stays in the previous
 * subbuffer).
 */
#ifdef CONFIG_LTT_VMCORE
static inline void ltt_write_commit_counter(struct rchan_buf *buf,
		void *reserved, size_t slot_size)
{
	struct ltt_channel_struct *channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_block_start_header *header =
		(struct ltt_block_start_header *)buf->data;
	long offset, subbuf_idx, commit_count;
	uint32_t lost_old, lost_new;

	offset = reserved + slot_size - buf->start;
	subbuf_idx = (offset - 1) / buf->chan->subbuf_size;
	for (;;) {
		lost_old = header->lost_size;
		commit_count =
			local_read(&channel->buf[buf->cpu].commit_count[
				subbuf_idx]);
		if (!SUBBUF_OFFSET(offset - commit_count, buf->chan)) {
			lost_new = (uint32_t)buf->chan->subbuf_size
				- SUBBUF_OFFSET(commit_count, buf->chan);
			lost_old = cmpxchg_local(&header->lost_size, lost_old,
				lost_new);
			if (lost_old <= lost_new)
				break;
		} else {
			break;
		}
	}
}
#else
static inline void ltt_write_commit_counter(struct rchan_buf *buf,
		void *reserved, size_t slot_size)
{
}
#endif

/*
 * ltt_reserve_slot
 *
 * Atomic slot reservation in a LTTng buffer. It will take care of
 * sub-buffer switching.
 *
 * Parameters:
 *
 * @trace : the trace structure to log to.
 * @buf : the buffer to reserve space into.
 * @data_size : size of the variable length data to log.
 * @slot_size : pointer to total size of the slot (out)
 * @tsc : pointer to the tsc at the slot reservation (out)
 * @cpu : cpu id
 *
 * Return : NULL if not enough space, else returns the pointer
 * 					to the beginning of the reserved slot.
 */
static inline void *ltt_reserve_slot(
		struct ltt_trace_struct *trace,
		struct ltt_channel_struct *channel,
		void **transport_data,
		size_t data_size,
		size_t *slot_size,
		u64 *tsc,
		int cpu)
{
	return trace->ops->reserve_slot(trace, channel, transport_data,
			data_size, slot_size, tsc, cpu);
}


/*
 * ltt_commit_slot
 *
 * Atomic unordered slot commit. Increments the commit count in the
 * specified sub-buffer, and delivers it if necessary.
 *
 * Parameters:
 *
 * @buf : the buffer to commit to.
 * @reserved : address of the beginning of the reserved slot.
 * @slot_size : size of the reserved slot.
 */
static inline void ltt_commit_slot(
		struct ltt_channel_struct *channel,
		void **transport_data,
		void *reserved,
		size_t slot_size)
{
	struct ltt_trace_struct *trace = channel->trace;

	trace->ops->commit_slot(channel, transport_data, reserved, slot_size);
}

/*
 * 4 control channels :
 * ltt/control/facilities
 * ltt/control/interrupts
 * ltt/control/processes
 * ltt/control/network
 *
 * 2 cpu channels :
 * ltt/cpu
 * ltt/compact
 */
#define LTT_RELAY_ROOT		"ltt"
#define LTT_CONTROL_ROOT	"control"
#define LTT_FACILITIES_CHANNEL	"facilities"
#define LTT_INTERRUPTS_CHANNEL	"interrupts"
#define LTT_PROCESSES_CHANNEL	"processes"
#define LTT_MODULES_CHANNEL	"modules"
#define LTT_NETWORK_CHANNEL	"network"
#define LTT_CPU_CHANNEL		"cpu"
#define LTT_COMPACT_CHANNEL	"compact"
#define LTT_FLIGHT_PREFIX	"flight-"

/* System types */
#define LTT_SYS_TYPE_VANILLA_LINUX	1

/* Architecture types */
#define LTT_ARCH_TYPE_I386		1
#define LTT_ARCH_TYPE_PPC		2
#define LTT_ARCH_TYPE_SH		3
#define LTT_ARCH_TYPE_S390		4
#define LTT_ARCH_TYPE_MIPS		5
#define LTT_ARCH_TYPE_ARM		6
#define LTT_ARCH_TYPE_PPC64		7
#define LTT_ARCH_TYPE_X86_64		8
#define LTT_ARCH_TYPE_C2		9
#define LTT_ARCH_TYPE_POWERPC		10
#define LTT_ARCH_TYPE_X86		11
#define LTT_ARCH_TYPE_UNDEFINED		12

/* Standard definitions for variants */
#define LTT_ARCH_VARIANT_NONE		0

/* Tracer properties */
#define LTT_DEFAULT_SUBBUF_SIZE_LOW	65536
#define LTT_DEFAULT_N_SUBBUFS_LOW	2
#define LTT_DEFAULT_SUBBUF_SIZE_MED	262144
#define LTT_DEFAULT_N_SUBBUFS_MED	2
#define LTT_DEFAULT_SUBBUF_SIZE_HIGH	1048576
#define LTT_DEFAULT_N_SUBBUFS_HIGH	2
#define LTT_TRACER_MAGIC_NUMBER		0x00D6B7ED
#define LTT_TRACER_VERSION_MAJOR	1
#define LTT_TRACER_VERSION_MINOR	0

/*
 * Size reserved for high priority events (interrupts, NMI, BH) at the end of a
 * nearly full buffer. User space won't use this last amount of space when in
 * blocking mode. This space also includes the event header that would be
 * written by this user space event.
 */
#define LTT_RESERVE_CRITICAL		4096

/* Register and unregister function pointers */

enum ltt_module_function {
	LTT_FUNCTION_RUN_FILTER,
	LTT_FUNCTION_FILTER_CONTROL,
	LTT_FUNCTION_STATEDUMP
};

extern int ltt_module_register(enum ltt_module_function name, void *function,
		struct module *owner);
extern void ltt_module_unregister(enum ltt_module_function name);

void ltt_transport_register(struct ltt_transport *transport);
void ltt_transport_unregister(struct ltt_transport *transport);

/* Exported control function */

enum ltt_control_msg {
	LTT_CONTROL_START,
	LTT_CONTROL_STOP,
	LTT_CONTROL_CREATE_TRACE,
	LTT_CONTROL_DESTROY_TRACE
};

union ltt_control_args {
	struct {
		enum trace_mode mode;
		unsigned subbuf_size_low;
		unsigned n_subbufs_low;
		unsigned subbuf_size_med;
		unsigned n_subbufs_med;
		unsigned subbuf_size_high;
		unsigned n_subbufs_high;
	} new_trace;
};

extern int ltt_control(enum ltt_control_msg msg, char *trace_name,
		char *trace_type, union ltt_control_args args);

enum ltt_filter_control_msg {
	LTT_FILTER_DEFAULT_ACCEPT,
	LTT_FILTER_DEFAULT_REJECT };

extern int ltt_filter_control(enum ltt_filter_control_msg msg,
		char *trace_name);

void ltt_write_trace_header(struct ltt_trace_struct *trace,
		struct ltt_trace_header *header);
extern void ltt_buffer_destroy(struct ltt_channel_struct *ltt_chan);
extern void ltt_wakeup_writers(struct work_struct *work);

void ltt_core_register(int (*function)(u8, void *));

void ltt_core_unregister(void);

void ltt_release_trace(struct kref *kref);
void ltt_release_transport(struct kref *kref);

extern int ltt_probe_register(struct ltt_available_probe *pdata);
extern int ltt_probe_unregister(struct ltt_available_probe *pdata);
extern int ltt_marker_connect(const char *mname, const char *pname,
		enum marker_id id, uint16_t channel, int user, int align);
extern int ltt_marker_disconnect(const char *mname, int user);
extern void ltt_dump_marker_state(struct ltt_trace_struct *trace);
extern void probe_id_defrag(void);

void ltt_lock_traces(void);
void ltt_unlock_traces(void);

/* Relay IOCTL */

/* Get the next sub buffer that can be read. */
#define RELAY_GET_SUBBUF		_IOR(0xF5, 0x00, __u32)
/* Release the oldest reserved (by "get") sub buffer. */
#define RELAY_PUT_SUBBUF		_IOW(0xF5, 0x01, __u32)
/* returns the number of sub buffers in the per cpu channel. */
#define RELAY_GET_N_SUBBUFS		_IOR(0xF5, 0x02, __u32)
/* returns the size of the sub buffers. */
#define RELAY_GET_SUBBUF_SIZE		_IOR(0xF5, 0x03, __u32)

#endif /* CONFIG_LTT */

#endif /* _LTT_TRACER_H */
