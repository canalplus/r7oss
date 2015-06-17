/*
 * LTTng serializing code.
 *
 * Copyright Mathieu Desnoyers, March 2007.
 *
 * Licensed under the GPLv2.
 *
 * See this discussion about weirdness about passing va_list and then va_list to
 * functions. (related to array argument passing). va_list seems to be
 * implemented as an array on x86_64, but not on i386... This is why we pass a
 * va_list * to ltt_vtrace.
 */

#include <stdarg.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/ltt-tracer.h>

enum ltt_type {
	LTT_TYPE_SIGNED_INT,
	LTT_TYPE_UNSIGNED_INT,
	LTT_TYPE_STRING,
	LTT_TYPE_COMPACT,
	LTT_TYPE_NONE,
};

#define LTT_ATTRIBUTE_COMPACT (1<<0)
#define LTT_ATTRIBUTE_NETWORK_BYTE_ORDER (1<<1)

/*
 * Inspired from vsnprintf
 *
 * The serialization format string supports the basic printf format strings.
 * In addition, it defines new formats that can be used to serialize more
 * complex/non portable data structures.
 *
 * Typical use:
 *
 * field_name %ctype
 * field_name #tracetype %ctype
 * field_name #tracetype %ctype1 %ctype2 ...
 *
 * A conversion is performed between format string types supported by GCC and
 * the trace type requested. GCC type is used to perform type checking on format
 * strings. Trace type is used to specify the exact binary representation
 * in the trace. A mapping is done between one or more GCC types to one trace
 * type. Sign extension, if required by the conversion, is performed following
 * the trace type.
 *
 * If a gcc format is not declared with a trace format, the gcc format is
 * also used as binary representation in the trace.
 *
 * Strings are supported with %s.
 * A single tracetype (sequence) can take multiple c types as parameter.
 *
 * c types:
 *
 * see printf(3).
 *
 * Note: to write a uint32_t in a trace, the following expression is recommended
 * si it can be portable:
 *
 * ("#4u%lu", (unsigned long)var)
 *
 * trace types:
 *
 * Serialization specific formats :
 *
 * Fixed size integers
 * #1u     writes uint8_t
 * #2u     writes uint16_t
 * #4u     writes uint32_t
 * #8u     writes uint64_t
 * #1d     writes int8_t
 * #2d     writes int16_t
 * #4d     writes int32_t
 * #8d     writes int64_t
 * i.e.:
 * #1u%lu #2u%lu #4d%lu #8d%lu #llu%hu #d%lu
 *
 * * Attributes:
 *
 * b: (for bitfield)
 * #btracetype%ctype
 *            will be packed, truncated, in the compact header if event is saved
 *            in compact channel and is first arg. Else the attribute has no
 *            effect. No sign extension is done when packing compact data.
 * n:  (for network byte order)
 * #ntracetype%ctype
 *            is written in the trace in network byte order.
 *
 * i.e.: #cn4u%lu, #n%lu, #c%u
 *
 * TODO (eventually)
 * Variable length sequence
 * #a #tracetype1 #tracetype2 %array_ptr %elem_size %num_elems
 *            In the trace:
 *            #a specifies that this is a sequence
 *            #tracetype1 is the type of elements in the sequence
 *            #tracetype2 is the type of the element count
 *            GCC input:
 *            array_ptr is a pointer to an array that contains members of size
 *            elem_size.
 *            num_elems is the number of elements in the array.
 * i.e.: #a #lu #lu %p %lu %u
 *
 * Callback
 * #k         callback (taken from the probe data)
 *            The following % arguments are exepected by the callback
 *
 * i.e.: #a #lu #lu #k %p
 *
 * Note: No conversion is done from floats to integers, nor from integers to
 * floats between c types and trace types. float conversion from double to float
 * or from float to double is also not supported.
 *
 * REMOVE
 * %*b     expects sizeof(data), data
 *         where sizeof(data) is 1, 2, 4 or 8
 *
 * Fixed length struct, union or array.
 * FIXME: unable to extract those sizes statically.
 * %*r     expects sizeof(*ptr), ptr
 * %*.*r   expects sizeof(*ptr), __alignof__(*ptr), ptr
 * struct and unions removed.
 * Fixed length array:
 * [%p]#a[len #tracetype]
 * i.e.: [%p]#a[12 #lu]
 *
 * Variable length sequence
 * %*.*:*v expects sizeof(*ptr), __alignof__(*ptr), elem_num, ptr
 *         where elem_num is the number of elements in the sequence
 */

__attribute__((no_instrument_function))
static inline const char *parse_trace_type(const char *fmt,
		char *trace_size, enum ltt_type *trace_type,
		unsigned long *attributes)
{
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */
				/* 'z' support added 23/7/1999 S.H.    */
				/* 'z' changed to 'Z' --davidm 1/25/99 */
				/* 't' added for ptrdiff_t */

	/* parse attributes. */
repeat:
	switch (*fmt) {
	case 'b':
		*attributes |= LTT_ATTRIBUTE_COMPACT;
		++fmt;
		goto repeat;
	case 'n':
		*attributes |= LTT_ATTRIBUTE_NETWORK_BYTE_ORDER;
		++fmt;
		goto repeat;
	}

	/* get the conversion qualifier */
	qualifier = -1;
	if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' ||
	    *fmt == 'Z' || *fmt == 'z' || *fmt == 't' ||
	    *fmt == 'S' || *fmt == '1' || *fmt == '2' ||
	    *fmt == '4' || *fmt == 8) {
		qualifier = *fmt;
		++fmt;
		if (qualifier == 'l' && *fmt == 'l') {
			qualifier = 'L';
			++fmt;
		}
	}

	switch (*fmt) {
	case 'c':
		*trace_type = LTT_TYPE_UNSIGNED_INT;
		*trace_size = sizeof(unsigned char);
		goto parse_end;
	case 's':
		*trace_type = LTT_TYPE_STRING;
		goto parse_end;
	case 'p':
		*trace_type = LTT_TYPE_UNSIGNED_INT;
		*trace_size = sizeof(void *);
		goto parse_end;
	case 'd':
	case 'i':
		*trace_type = LTT_TYPE_SIGNED_INT;
		break;
	case 'o':
	case 'u':
	case 'x':
	case 'X':
		*trace_type = LTT_TYPE_UNSIGNED_INT;
		break;
	default:
		if (!*fmt)
			--fmt;
		goto parse_end;
	}
	switch (qualifier) {
	case 'L':
		*trace_size = sizeof(long long);
		break;
	case 'l':
		*trace_size = sizeof(long);
		break;
	case 'Z':
	case 'z':
		*trace_size = sizeof(size_t);
		break;
	case 't':
		*trace_size = sizeof(ptrdiff_t);
		break;
	case 'h':
		*trace_size = sizeof(short);
		break;
	case '1':
		*trace_size = sizeof(uint8_t);
		break;
	case '2':
		*trace_size = sizeof(uint16_t);
		break;
	case '4':
		*trace_size = sizeof(uint32_t);
		break;
	case '8':
		*trace_size = sizeof(uint64_t);
		break;
	default:
		*trace_size = sizeof(int);
	}

parse_end:
	return fmt;
}

/*
 * Restrictions:
 * Field width and precision are *not* supported.
 * %n not supported.
 */
__attribute__((no_instrument_function))
static inline const char *parse_c_type(const char *fmt,
		char *c_size, enum ltt_type *c_type)
{
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */
				/* 'z' support added 23/7/1999 S.H.    */
				/* 'z' changed to 'Z' --davidm 1/25/99 */
				/* 't' added for ptrdiff_t */

	/* process flags : ignore standard print formats for now. */
repeat:
	switch (*fmt) {
	case '-':
	case '+':
	case ' ':
	case '#':
	case '0':
		++fmt;
		goto repeat;
	}

	/* get the conversion qualifier */
	qualifier = -1;
	if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' ||
	    *fmt == 'Z' || *fmt == 'z' || *fmt == 't' ||
	    *fmt == 'S') {
		qualifier = *fmt;
		++fmt;
		if (qualifier == 'l' && *fmt == 'l') {
			qualifier = 'L';
			++fmt;
		}
	}

	switch (*fmt) {
	case 'c':
		*c_type = LTT_TYPE_UNSIGNED_INT;
		*c_size = sizeof(unsigned char);
		goto parse_end;
	case 's':
		*c_type = LTT_TYPE_STRING;
		goto parse_end;
	case 'p':
		*c_type = LTT_TYPE_UNSIGNED_INT;
		*c_size = sizeof(void *);
		goto parse_end;
	case 'd':
	case 'i':
		*c_type = LTT_TYPE_SIGNED_INT;
		break;
	case 'o':
	case 'u':
	case 'x':
	case 'X':
		*c_type = LTT_TYPE_UNSIGNED_INT;
		break;
	default:
		if (!*fmt)
			--fmt;
		goto parse_end;
	}
	switch (qualifier) {
	case 'L':
		*c_size = sizeof(long long);
		break;
	case 'l':
		*c_size = sizeof(long);
		break;
	case 'Z':
	case 'z':
		*c_size = sizeof(size_t);
		break;
	case 't':
		*c_size = sizeof(ptrdiff_t);
		break;
	case 'h':
		*c_size = sizeof(short);
		break;
	default:
		*c_size = sizeof(int);
	}

parse_end:
	return fmt;
}



__attribute__((no_instrument_function))
static inline char *serialize_trace_data(char *buffer, char *str,
		char trace_size, enum ltt_type trace_type,
		char c_size, enum ltt_type c_type, int align, va_list *args)
{
	union {
		unsigned long v_ulong;
		uint64_t v_uint64;
		struct {
			const char *s;
			size_t len;
		} v_string;
	} tmp;

	/*
	 * Be careful about sign extension here.
	 * Sign extension is done with the destination (trace) type.
	 */
	switch (trace_type) {
	case LTT_TYPE_SIGNED_INT:
		switch (c_size) {
		case 1:	tmp.v_ulong = (long)(int8_t)va_arg(*args, int);
			break;
		case 2:	tmp.v_ulong = (long)(int16_t)va_arg(*args, int);
			break;
		case 4:	tmp.v_ulong = (long)(int32_t)va_arg(*args, int);
			break;
		case 8:	tmp.v_uint64 = va_arg(*args, int64_t);
			break;
		default:
			BUG();
		}
		break;
	case LTT_TYPE_UNSIGNED_INT:
		switch (c_size) {
		case 1:	tmp.v_ulong = (unsigned long)(uint8_t)
					va_arg(*args, unsigned int);
			break;
		case 2:	tmp.v_ulong = (unsigned long)(uint16_t)
					va_arg(*args, unsigned int);
			break;
		case 4:	tmp.v_ulong = (unsigned long)(uint32_t)
					va_arg(*args, unsigned int);
			break;
		case 8:	tmp.v_uint64 = va_arg(*args, uint64_t);
			break;
		default:
			BUG();
		}
		break;
	case LTT_TYPE_STRING:
		tmp.v_string.s = va_arg(*args, const char *);
		if ((unsigned long)tmp.v_string.s < PAGE_SIZE)
			tmp.v_string.s = "<NULL>";
		tmp.v_string.len = strlen(tmp.v_string.s)+1;
		if (buffer)
			memcpy(str, tmp.v_string.s, tmp.v_string.len);
		str += tmp.v_string.len;
		goto copydone;
	default:
		BUG();
	}

	/*
	 * If trace_size is lower or equal to 4 bytes, there is no sign
	 * extension to do because we are already encoded in a long. Therefore,
	 * we can combine signed and unsigned ops. 4 bytes float also works
	 * with this, because we do a simple copy of 4 bytes into 4 bytes
	 * without manipulation (and we do not support conversion from integers
	 * to floats).
	 * It is also the case if c_size is 8 bytes, which is the largest
	 * possible integer.
	 */
	if (align)
		str += ltt_align((long)str, trace_size);
	if (trace_size <= 4 || c_size == 8) {
		if (buffer) {
			switch (trace_size) {
			case 1:	if (c_size == 8)
					*(uint8_t *)str = (uint8_t)tmp.v_uint64;
				else
					*(uint8_t *)str = (uint8_t)tmp.v_ulong;
				break;
			case 2:	if (c_size == 8)
					*(uint16_t *)str =
							(uint16_t)tmp.v_uint64;
				else
					*(uint16_t *)str =
							(uint16_t)tmp.v_ulong;
				break;
			case 4:	if (c_size == 8)
					*(uint32_t *)str =
							(uint32_t)tmp.v_uint64;
				else
					*(uint32_t *)str =
							(uint32_t)tmp.v_ulong;
				break;
			case 8:	/*
				 * c_size cannot be other than 8 here because
				 * trace_size > 4.
				 */
				*(uint64_t *)str = (uint64_t)tmp.v_uint64;
				break;
			default:
				BUG();
			}
		}
		str += trace_size;
		goto copydone;
	} else {
		/*
		 * Perform sign extension.
		 */
		if (buffer) {
			switch (trace_type) {
			case LTT_TYPE_SIGNED_INT:
				*(int64_t *)str = (int64_t)tmp.v_ulong;
				break;
			case LTT_TYPE_UNSIGNED_INT:
				*(uint64_t *)str = (uint64_t)tmp.v_ulong;
				break;
			default:
				BUG();
			}
		}
		str += trace_size;
		goto copydone;
	}

copydone:
	return str;
}

__attribute__((no_instrument_function))
char *ltt_serialize_data(char *buffer, char *str,
			struct ltt_serialize_closure *closure,
			void *serialize_private,
			int align,
			const char *fmt, va_list *args)
{
	char trace_size = 0, c_size = 0;	/*
						 * 0 (unset), 1, 2, 4, 8 bytes.
						 */
	enum ltt_type trace_type = LTT_TYPE_NONE, c_type = LTT_TYPE_NONE;
	unsigned long attributes = 0;

	for (; *fmt ; ++fmt) {
		switch (*fmt) {
		case '#':
			/* tracetypes (#) */
			++fmt;			/* skip first '#' */
			if (*fmt == '#')	/* Escaped ## */
				break;
			attributes = 0;
			fmt = parse_trace_type(fmt, &trace_size, &trace_type,
				&attributes);
			break;
		case '%':
			/* c types (%) */
			++fmt;			/* skip first '%' */
			if (*fmt == '%')	/* Escaped %% */
				break;
			fmt = parse_c_type(fmt, &c_size, &c_type);
			/*
			 * Output c types if no trace types has been
			 * specified.
			 */
			if (!trace_size)
				trace_size = c_size;
			if (trace_type == LTT_TYPE_NONE)
				trace_type = c_type;
			if (c_type == LTT_TYPE_STRING)
				trace_type = LTT_TYPE_STRING;
			/* perform trace write */
			str = serialize_trace_data(buffer, str, trace_size,
						trace_type, c_size, c_type,
						align, args);
			trace_size = 0;
			c_size = 0;
			trace_type = LTT_TYPE_NONE;
			c_size = LTT_TYPE_NONE;
			attributes = 0;
			break;
			/* default is to skip the text, doing nothing */
		}
	}
	return str;
}
EXPORT_SYMBOL_GPL(ltt_serialize_data);

/*
 * get_one_arg
 * @fmt: format string
 * @args: va args list
 * @compact_data: compact data to fill
 *
 * Get one argument of the format string.
 */
static inline int get_compact_arg(const char **orig_fmt, va_list *args,
		uint32_t *compact_data)
{
	int found = 0;
	const char *fmt = *orig_fmt;
	char trace_size = 0, c_size = 0;	/*
						 * 0 (unset), 1, 2, 4, 8 bytes.
						 */
	enum ltt_type trace_type = LTT_TYPE_NONE, c_type = LTT_TYPE_NONE;
	unsigned long attributes = 0;

	for (; *fmt ; ++fmt) {
		switch (*fmt) {
		case '#':
			/* tracetypes (#) */
			++fmt;			/* skip first '#' */
			if (*fmt == '#')	/* Escaped ## */
				break;
			attributes = 0;
			fmt = parse_trace_type(fmt, &trace_size, &trace_type,
				&attributes);
			if (attributes & LTT_ATTRIBUTE_COMPACT)
				found = 1;
			break;
		case '%':
			/* c types (%) */
			++fmt;			/* skip first '%' */
			if (*fmt == '%')	/* Escaped %% */
				break;
			fmt = parse_c_type(fmt, &c_size, &c_type);
			if (found) {
				if (c_size <= 4)
					*compact_data = va_arg(*args, uint32_t);
				else
					*compact_data = (uint32_t)
							va_arg(*args, uint64_t);
			}
			trace_size = 0;
			c_size = 0;
			trace_type = LTT_TYPE_NONE;
			c_size = LTT_TYPE_NONE;
			attributes = 0;
			goto end;	/* Stop after 1st c type */
			/* default is to skip the text, doing nothing */
		}
	}
end:
	if (found)
		*orig_fmt = fmt;
	return found;
}

/*
 * Calculate data size
 * Assume that the padding for alignment starts at a sizeof(void *) address.
 */
static __attribute__((no_instrument_function))
size_t ltt_get_data_size(struct ltt_serialize_closure *closure,
				void *serialize_private,
				int align,
				const char *fmt, va_list *args)
{
	ltt_serialize_cb cb = closure->callbacks[0];
	closure->cb_idx = 0;
	return (size_t)cb(NULL, NULL, closure, serialize_private, align,
				fmt, args);
}

static __attribute__((no_instrument_function))
void ltt_write_event_data(char *buffer,
				struct ltt_serialize_closure *closure,
				void *serialize_private,
				int align,
				const char *fmt, va_list *args)
{
	ltt_serialize_cb cb = closure->callbacks[0];
	closure->cb_idx = 0;
	cb(buffer, buffer, closure, serialize_private, align, fmt, args);
}


__attribute__((no_instrument_function))
void ltt_vtrace(const struct marker *mdata, void *call_data,
		const char *fmt, va_list *args)
{
	int align;
	struct ltt_active_marker *pdata;
	uint16_t eID;
	size_t data_size, slot_size;
	int channel_index;
	struct ltt_channel_struct *channel;
	struct ltt_trace_struct *trace, *dest_trace = NULL;
	void *transport_data;
	uint64_t tsc;
	char *buffer;
	va_list args_copy;
	struct ltt_serialize_closure closure;
	struct ltt_probe_private_data *private_data = call_data;
	u32 compact_data = 0;
	void *serialize_private = NULL;
	int cpu;

	pdata = (struct ltt_active_marker *)mdata->private;
	if (unlikely(private_data && private_data->id < MARKER_CORE_IDS))
		eID = private_data->id;
	else
		eID = pdata->id;
	closure.callbacks = pdata->probe->callbacks;

	/*
	 * This test is useful for quickly exiting static tracing when no
	 * trace is active.
	 */
	if (likely(ltt_traces.num_active_traces == 0
		&& (!private_data || !private_data->force)))
		return;

	preempt_disable();
	if (likely(!private_data || !private_data->force
			|| private_data->cpu == -1))
		cpu = smp_processor_id();
	else
		cpu = private_data->cpu;
	ltt_nesting[smp_processor_id()]++;

	if (unlikely(private_data && private_data->trace))
		dest_trace = private_data->trace;
	if (unlikely(private_data && private_data->channel))
		channel_index = private_data->channel;
	else
		channel_index = pdata->channel;
	if (unlikely(private_data))
		serialize_private = private_data->serialize_private;
	/* Skip the compact data for size calculation in compact channel */
	if (unlikely(channel_index == GET_CHANNEL_INDEX(compact))) {
		get_compact_arg(&fmt, args, &compact_data);
		align = 0;	/* Force no alignment for compact channel */
	} else
		align = ltt_get_alignment(pdata);
	va_copy(args_copy, *args);
	data_size = ltt_get_data_size(&closure, serialize_private, align,
					fmt, &args_copy);
	va_end(args_copy);

	/* Iterate on each trace */
	list_for_each_entry_rcu(trace, &ltt_traces.head, list) {
		if (unlikely(!trace->active
			&& (!private_data || !private_data->force)))
			continue;
		if (unlikely((private_data && private_data->trace)
				&& trace != dest_trace))
			continue;
		if (!ltt_run_filter(trace, eID))
			continue;
		channel = ltt_get_channel_from_index(trace, channel_index);
		/* reserve space : header and data */
		buffer = ltt_reserve_slot(trace, channel, &transport_data,
					data_size, &slot_size, &tsc, cpu);
		if (unlikely(!buffer))
			continue; /* buffer full */

		va_copy(args_copy, *args);
		/* Out-of-order write : header and data */
		if (likely(channel_index != GET_CHANNEL_INDEX(compact)))
			buffer = ltt_write_event_header(trace, channel, buffer,
						eID, data_size, tsc);
		else
			buffer = ltt_write_compact_header(trace, channel,
						buffer, eID, data_size,
						tsc, compact_data);
		ltt_write_event_data(buffer, &closure, serialize_private, align,
					fmt, &args_copy);
		va_end(args_copy);
		/* Out-of-order commit */
		ltt_commit_slot(channel, &transport_data, buffer, slot_size);
	}
	ltt_nesting[smp_processor_id()]--;
	preempt_enable();
}
EXPORT_SYMBOL_GPL(ltt_vtrace);

__attribute__((no_instrument_function))
void ltt_trace(const struct marker *mdata, void *call_data,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ltt_vtrace(mdata, call_data, fmt, &args);
	va_end(args);
}
EXPORT_SYMBOL_GPL(ltt_trace);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("Linux Trace Toolkit Next Generation Serializer");
