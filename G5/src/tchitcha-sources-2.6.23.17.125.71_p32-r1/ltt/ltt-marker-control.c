/*
 * Copyright (C) 2007 Mathieu Desnoyers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * LTT marker control module over /proc
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/vmalloc.h>
#include <linux/marker.h>
#include <linux/ltt-tracer.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#define DEFAULT_CHANNEL "cpu"
#define DEFAULT_PROBE "default"

LIST_HEAD(probes_list);

/*
 * Mutex protecting the probe slab cache and probe IDs consistency.
 * Nests inside the traces mutex.
 */
DEFINE_MUTEX(probes_mutex);

static uint8_t mark_next_compact_id = MARKER_CORE_IDS;
/* Next available ID. Dynamic range : 128-65535 */
static uint16_t mark_next_id = MARKER_COMPACT_IDS;

struct ltt_available_probe default_probe = {
	.name = "default",
	.format = NULL,
	.probe_func = ltt_trace,
	.callbacks[0] = ltt_serialize_data,
};

static struct kmem_cache *markers_loaded_cachep;
static LIST_HEAD(markers_loaded_list);
/*
 * List sorted by name strcmp order.
 */
static LIST_HEAD(probes_registered_list);

static struct proc_dir_entry *pentry;

static struct chan_name_info {
	enum ltt_channels channels;
	const char *name;
	unsigned int channel_index;
} channel_names[] = {
	{ LTT_CHANNEL_FACILITIES, "facilities", GET_CHANNEL_INDEX(facilities) },
	{ LTT_CHANNEL_INTERRUPTS, "interrupts", GET_CHANNEL_INDEX(interrupts) },
	{ LTT_CHANNEL_PROCESSES, "processes", GET_CHANNEL_INDEX(processes) },
	{ LTT_CHANNEL_MODULES, "modules", GET_CHANNEL_INDEX(modules) },
	{ LTT_CHANNEL_CPU, "cpu", GET_CHANNEL_INDEX(cpu) },
	{ LTT_CHANNEL_COMPACT, "compact", GET_CHANNEL_INDEX(compact) },
	{ LTT_CHANNEL_NETWORK, "network", GET_CHANNEL_INDEX(network) },
};

static struct id_name_info {
	enum marker_id n_id;
	const char *name;
} id_names[] = {
	{ MARKER_ID_COMPACT, "compact" },
	{ MARKER_ID_DYNAMIC, "dynamic" },
};

static struct file_operations ltt_fops;

static inline int get_channel_index_from_name(const char *name)
{
	struct chan_name_info *info;

	if (!name)
		name = "cpu";
	for (info = channel_names;
		info < channel_names + ARRAY_SIZE(channel_names); info++) {
		if (!strcmp(name, info->name))
			return info->channel_index;
	}
	return -ENOENT;
}

static inline enum marker_id get_id_from_name(const char *name)
{
	struct id_name_info *info;

	if (!name)
		name = "dynamic";
	for (info = id_names; info < id_names + ARRAY_SIZE(id_names); info++) {
		if (!strcmp(name, info->name))
			return info->n_id;
	}
	return -ENOENT;
}

static inline char *skip_spaces(char *buf)
{
	while (*buf != '\0' && isspace(*buf))
		buf++;
	return buf;
}

static inline char *skip_nonspaces(char *buf)
{
	while (*buf != '\0' && !isspace(*buf))
		buf++;
	return buf;
}

static void get_marker_string(char *buf, char **start,
		char **end)
{
	*start = skip_spaces(buf);
	*end = skip_nonspaces(*start);
	**end = '\0';
}

static inline int is_pdata_in_active_markers(void *pdata)
{
	struct ltt_active_marker *amark;

	list_for_each_entry(amark, &markers_loaded_list, node) {
		if ((void *)amark == pdata)
			return 1;
	}
	return 0;
}

/*
 * Defragment the markers IDs. Should only be called when the IDs are not used
 * by anyone, typically when all probes are disarmed. Clients of the markers
 * rely on having their code markers armed during a "session" to make sure there
 * will be no ID compaction. (for instance, a marker ID is never reused during a
 * trace).
 * There is no need to synchronize the hash table entries with the section
 * elements because none is armed.
 */
void probe_id_defrag(void)
{
	struct ltt_active_marker *amark;

	mark_next_compact_id = MARKER_CORE_IDS;
	mark_next_id = MARKER_COMPACT_IDS;

	mutex_lock(&probes_mutex);
	list_for_each_entry(amark, &markers_loaded_list, node) {
		switch (marker_id_type(amark->id)) {
		case MARKER_ID_COMPACT:
			amark->id = mark_next_compact_id++;
			break;
		case MARKER_ID_DYNAMIC:
			amark->id = mark_next_id++;
			break;
		default:
			/* default is to keep the static ID */
			break;
		}
	}
	ltt_init_compact_markers(mark_next_compact_id);
	mutex_unlock(&probes_mutex);
}
EXPORT_SYMBOL_GPL(probe_id_defrag);

/*
 * Must be called with ltt_lock_traces() taken so the number of active traces
 * stays to 0 if we are registering a compact ID.
 * This is required because we must call ltt_init_compact_markers() to recompute
 * the number of reserved bits.
 */
static int _check_id_avail(enum marker_id id)
{
	/* First check if there are still IDs available */
	switch (id) {
	case MARKER_ID_DYNAMIC:
		if (mark_next_id == 0)
			return -ENOSPC;
		break;
	case MARKER_ID_COMPACT:
		if (ltt_traces.num_active_traces > 0)
			return -EBUSY;
		if (mark_next_compact_id == 0)
			return -ENOSPC;
		break;
	default:
		/* Only allow 0-7 range for core IDs */
		if ((uint16_t)id >= MARKER_CORE_IDS)
			return -EPERM;
	}
	return 0;
}

static uint16_t assign_id(enum marker_id id)
{
	uint16_t new_id;

	switch (id) {
	case MARKER_ID_COMPACT:
		new_id = mark_next_compact_id++;
		ltt_init_compact_markers(mark_next_compact_id);
		return new_id;
	case MARKER_ID_DYNAMIC:
		return mark_next_id++;
	default:
		return (uint16_t)id;
	}
}

int ltt_probe_register(struct ltt_available_probe *pdata)
{
	int ret = 0;
	int comparison;
	struct ltt_available_probe *iter;

	mutex_lock(&probes_mutex);
	list_for_each_entry_reverse(iter, &probes_registered_list, node) {
		comparison = strcmp(pdata->name, iter->name);
		if (!comparison) {
			ret = -EBUSY;
			goto end;
		} else if (comparison > 0) {
			/* We belong to the location right after iter. */
			list_add(&pdata->node, &iter->node);
			goto end;
		}
	}
	/* Should be added at the head of the list */
	list_add(&pdata->node, &probes_registered_list);
end:
	mutex_unlock(&probes_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(ltt_probe_register);

/*
 * Called when a probe does not want to be called anymore.
 */
int ltt_probe_unregister(struct ltt_available_probe *pdata)
{
	int ret = 0;
	struct ltt_active_marker *amark, *tmp;
	void *retptr;

	mutex_lock(&probes_mutex);
	list_for_each_entry_safe(amark, tmp, &markers_loaded_list, node) {
		if (amark->probe == pdata) {
			retptr = marker_probe_unregister_private_data(amark);
			if (IS_ERR(retptr)) {
				ret = PTR_ERR(retptr);
				goto end;
			}
			list_del(&amark->node);
			kmem_cache_free(markers_loaded_cachep, amark);
		}
	}
	list_del(&pdata->node);
end:
	mutex_unlock(&probes_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(ltt_probe_unregister);

/*
 * Connect marker "mname" to probe "pname".
 */
int ltt_marker_connect(const char *mname, const char *pname,
	enum marker_id id, uint16_t channel, int user, int align)

{
	int ret;
	int found = 0;
	int comparison;
	struct ltt_active_marker *pdata;
	struct ltt_available_probe *iter;

	/*
	 * Do not let userspace mess with core markers.
	 */
	if (user && id != MARKER_ID_COMPACT && id != MARKER_ID_DYNAMIC)
		return -EPERM;
	if (!pname)
		pname = DEFAULT_PROBE;

	ltt_lock_traces();
	mutex_lock(&probes_mutex);
	ret = _check_id_avail(id);
	if (ret)
		goto end;
	list_for_each_entry(iter, &probes_registered_list, node) {
		comparison = strcmp(pname, iter->name);
		if (!comparison)
			found = 1;
		if (comparison <= 0)
			break;
	}
	if (!found) {
		ret = -ENOENT;
		goto end;
	}
	pdata = kmem_cache_zalloc(markers_loaded_cachep, GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto end;
	}
	pdata->probe = iter;
	pdata->id = assign_id(id);
	pdata->channel = channel;
	pdata->align = align;
	/*
	 * ID has priority over channel in case of conflict.
	 */
	if (id == MARKER_ID_COMPACT)
		pdata->channel = get_channel_index_from_name("compact");
	else if (pdata->channel == get_channel_index_from_name("compact"))
		pdata->channel = get_channel_index_from_name(DEFAULT_CHANNEL);
	ret = marker_probe_register(mname, NULL, ltt_trace, pdata);
	if (ret)
		kmem_cache_free(markers_loaded_cachep, pdata);
	else {
		struct ltt_probe_private_data call_data;

		call_data.trace = NULL;
		call_data.force = 0;
		call_data.channel = GET_CHANNEL_INDEX(facilities);
		call_data.id = MARKER_CORE_IDS;
		__trace_mark(0, core_marker_id, &call_data,
				"name %s id %hu "
				"int #1u%zu long #1u%zu pointer #1u%zu "
				"size_t #1u%zu alignment #1u%u",
				mname, pdata->id,
				sizeof(int), sizeof(long), sizeof(void *),
				sizeof(size_t), ltt_get_alignment(pdata));
		list_add(&pdata->node, &markers_loaded_list);
	}
end:
	mutex_unlock(&probes_mutex);
	ltt_unlock_traces();
	return ret;
}
EXPORT_SYMBOL_GPL(ltt_marker_connect);

/*
 * Disconnect marker "mname".
 */
int ltt_marker_disconnect(const char *mname, int user)
{
	struct ltt_active_marker *pdata;
	int ret = 0;

	mutex_lock(&probes_mutex);
	pdata = marker_get_private_data(mname);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	} else if (!is_pdata_in_active_markers(pdata)) {
		/*
		 * Not registered by us.
		 */
		ret = -EPERM;
		goto end;
	} else if (user && pdata->id < MARKER_CORE_IDS) {
		ret = -EPERM;
		goto end;
	}
	pdata = marker_probe_unregister(mname);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	} else {
		list_del(&pdata->node);
		kmem_cache_free(markers_loaded_cachep, pdata);
	}
end:
	mutex_unlock(&probes_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(ltt_marker_disconnect);

/*
 * Sets a marker id.
 * Keep the same loaded marker element.
 */
static int do_set_id(const char *name, enum marker_id new_id, int user)
{
	struct ltt_active_marker *pdata;
	int ret;

	/*
	 * Do not let userspace mess with core markers.
	 */
	if (user && new_id != MARKER_ID_COMPACT && new_id != MARKER_ID_DYNAMIC)
		return -EPERM;

	ltt_lock_traces();
	mutex_lock(&probes_mutex);
	pdata = marker_get_private_data(name);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	} else if (!is_pdata_in_active_markers(pdata)) {
		/*
		 * Not registered by us.
		 */
		ret = -EPERM;
		goto end;
	} else if (user && pdata->id < MARKER_CORE_IDS) {
		ret = -EPERM;
		goto end;
	}
	pdata = marker_probe_unregister(name);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	}
	ret = _check_id_avail(new_id);
	if (ret)
		goto end;
	pdata->id = assign_id(new_id);
	if (new_id == MARKER_ID_COMPACT)
		pdata->channel = get_channel_index_from_name("compact");
	else if (pdata->channel == get_channel_index_from_name("compact"))
		pdata->channel = get_channel_index_from_name(DEFAULT_CHANNEL);
	ret = marker_probe_register(name, NULL, ltt_trace, pdata);
	if (ret) {
		list_del(&pdata->node);
		kmem_cache_free(markers_loaded_cachep, pdata);
		printk(KERN_ERR "Bogus update of marker %s\n", name);
	} else {
		struct ltt_probe_private_data call_data;

		call_data.trace = NULL;
		call_data.force = 0;
		call_data.channel = GET_CHANNEL_INDEX(facilities);
		call_data.id = MARKER_CORE_IDS;
		__trace_mark(0, core_marker_id, &call_data,
				"name %s id %hu "
				"int #1u%zu long #1u%zu pointer #1u%zu "
				"size_t #1u%zu alignment #1u%u",
				name, pdata->id,
				sizeof(int), sizeof(long), sizeof(void *),
				sizeof(size_t), ltt_get_alignment(pdata));
	}
end:
	mutex_unlock(&probes_mutex);
	ltt_unlock_traces();
	return ret;
}

/*
 * Sets a marker channel.
 * Keep the same loaded marker element.
 */
static int do_set_channel(const char *name, uint16_t new_channel, int user)
{
	struct ltt_active_marker *pdata;
	int ret;

	/*
	 * Do not let userspace mess with core markers.
	 */
	if (user && get_channel_index_from_name("facilities") == new_channel)
		return -EPERM;

	ltt_lock_traces();
	mutex_lock(&probes_mutex);
	pdata = marker_get_private_data(name);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	} else if (!is_pdata_in_active_markers(pdata)) {
		/*
		 * Not registered by us.
		 */
		ret = -EPERM;
		goto end;
	} else if (user && pdata->id < MARKER_CORE_IDS) {
		ret = -EPERM;
		goto end;
	}
	pdata = marker_probe_unregister(name);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto end;
	}
	pdata->channel = new_channel;
	if (new_channel == get_channel_index_from_name("compact")
			&& pdata->id >= MARKER_COMPACT_IDS) {
		ret = _check_id_avail(MARKER_ID_COMPACT);
		if (!ret)
			goto end;
		pdata->id = assign_id(MARKER_ID_COMPACT);
	} else if (pdata->id < MARKER_COMPACT_IDS
			&& pdata->id >= MARKER_CORE_IDS) {
		ret = _check_id_avail(MARKER_ID_DYNAMIC);
		if (!ret)
			goto end;
		pdata->id = assign_id(MARKER_ID_DYNAMIC);
	}

	ret = marker_probe_register(name, NULL, ltt_trace, pdata);
	if (ret) {
		list_del(&pdata->node);
		kmem_cache_free(markers_loaded_cachep, pdata);
		printk(KERN_ERR "Bogus update of marker %s\n", name);
	} else {
		struct ltt_probe_private_data call_data;

		call_data.trace = NULL;
		call_data.force = 0;
		call_data.channel = GET_CHANNEL_INDEX(facilities);
		call_data.id = MARKER_CORE_IDS;
		__trace_mark(0, core_marker_id, &call_data,
				"name %s id %hu "
				"int #1u%zu long #1u%zu pointer #1u%zu "
				"size_t #1u%zu alignment #1u%u",
				name, pdata->id,
				sizeof(int), sizeof(long), sizeof(void *),
				sizeof(size_t), ltt_get_alignment(pdata));
	}
end:
	mutex_unlock(&probes_mutex);
	ltt_unlock_traces();
	return ret;
}

void ltt_dump_marker_state(struct ltt_trace_struct *trace)
{
	struct marker_iter iter;
	struct ltt_active_marker *pdata;
	struct ltt_probe_private_data call_data;

	call_data.trace = trace;
	call_data.force = 0;
	call_data.channel = 0;
	call_data.id = MARKER_CORE_IDS;

	marker_iter_reset(&iter);
	marker_iter_start(&iter);
	for (; iter.marker != NULL; marker_iter_next(&iter)) {
		if (_immediate_read(iter.marker->state)) {
			pdata = iter.marker->private;
			if (is_pdata_in_active_markers(pdata)) {
				__trace_mark(0, core_marker_id, &call_data,
					"name %s id %hu "
					"int #1u%zu long #1u%zu pointer #1u%zu "
					"size_t #1u%zu alignment #1u%u",
					iter.marker->name, pdata->id,
					sizeof(int), sizeof(long),
					sizeof(void *), sizeof(size_t),
					ltt_get_alignment(pdata));
				if (iter.marker->format)
					__trace_mark(0, core_marker_format,
						&call_data,
						"name %s format %s",
						iter.marker->name,
						iter.marker->format);
			}
		}
	}
	marker_iter_stop(&iter);
}
EXPORT_SYMBOL_GPL(ltt_dump_marker_state);

/*
 * function handling proc entry write.
 *
 * connect marker_name [probe_name] [id type (compact/dynamic)] [channel]
 * disconnect marker_name
 *
 * Actions can be done on a connected, unarmed marker:
 * set_id marker_name compact/dynamic
 * set_channel marker_name channel
 *
 * arm marker_name
 * disarm marker_name
 *
 * note : when a core probe registers, it directly establishes the connexion.
 * Cannot be disconnected by users.
 */
static ssize_t ltt_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *offset)
{
	char *kbuf;
	char *iter, *marker_action, *marker_name = NULL, *probe_name = NULL,
			*id = NULL, *channel = NULL;
	ssize_t ret;
	int channel_index;
	enum marker_id n_id;

	if (!count)
		return -EINVAL;

	kbuf = vmalloc(count + 1);
	kbuf[count] = '\0';		/* Transform into a string */
	ret = copy_from_user(kbuf, buffer, count);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}
	get_marker_string(kbuf, &marker_action, &iter);
	if (!marker_action || marker_action == iter) {
		ret = -EINVAL;
		goto end;
	}
	if (iter < kbuf + count) {
		iter++;			/* skip the added '\0' */
		get_marker_string(iter, &marker_name, &iter);
		if (marker_name == iter)
			marker_name = NULL;
	}
	if (iter < kbuf + count) {
		iter++;			/* skip the added '\0' */
		get_marker_string(iter, &probe_name, &iter);
		if (probe_name == iter)
			probe_name = NULL;
	}
	if (iter < kbuf + count) {
		iter++;			/* skip the added '\0' */
		get_marker_string(iter, &id, &iter);
		if (id == iter)
			id = NULL;
	}
	if (iter < kbuf + count) {
		iter++;			/* skip the added '\0' */
		get_marker_string(iter, &channel, &iter);
		if (channel == iter)
			channel = NULL;
	}

	printk(KERN_DEBUG "Marker control : '%s' '%s' id: %s channel: %s\n",
			marker_action, marker_name, id, channel);

	if (!marker_name) {
		ret = -EINVAL;
		goto end;
	}

	if (!strcmp(marker_action, "arm")) {
		ret = marker_arm(marker_name);
		if (ret)
			goto end;
	} else if (!strcmp(marker_action, "disarm")) {
		ret = marker_disarm(marker_name);
		if (ret)
			goto end;
	} else if (!strcmp(marker_action, "connect")) {
		channel_index = get_channel_index_from_name(channel);
		if (channel_index < 0) {
			ret = channel_index;
			goto end;
		}
		n_id = get_id_from_name(id);
		if (n_id < 0) {
			ret = n_id;
			goto end;
		}
		ret = ltt_marker_connect(marker_name, probe_name, n_id,
					(uint16_t)channel_index, 1, 1);
		if (ret)
			goto end;
	} else if (!strcmp(marker_action, "disconnect")) {
		ret = ltt_marker_disconnect(marker_name, 1);
		if (ret)
			goto end;
	} else if (!strcmp(marker_action, "set_id")) {
		if (!id) {
			ret = -EINVAL;
			goto end;
		}
		n_id = get_id_from_name(id);
		if (n_id < 0) {
			ret = n_id;
			goto end;
		}
		ret = do_set_id(marker_name, n_id, 1);
		if (ret)
			goto end;
	} else if (!strcmp(marker_action, "set_channel")) {
		if (!channel) {
			ret = -EINVAL;
			goto end;
		}
		channel_index = get_channel_index_from_name(channel);
		if (channel_index < 0) {
			ret = channel_index;
			goto end;
		}
		ret = do_set_channel(marker_name, (uint16_t)channel_index, 1);
		if (ret)
			goto end;
	} else {
		ret = -EINVAL;
		goto end;
	}
	ret = count;
end:
	vfree(kbuf);
	return ret;
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct marker_iter *iter = m->private;

	marker_iter_next(iter);
	if (!iter->marker) {
		/*
		 * Setting the iter module to -1UL will make sure
		 * that no module can possibly hold the current marker.
		 */
		iter->module = (void *)-1UL;
		return NULL;
	}
	return iter->marker;
}

static void *s_start(struct seq_file *m, loff_t *pos)
{
	struct marker_iter *iter = m->private;

	if (!*pos)
		marker_iter_reset(iter);
	marker_iter_start(iter);
	if (!iter->marker) {
		/*
		 * Setting the iter module to -1UL will make sure
		 * that no module can possibly hold the current marker.
		 */
		iter->module = (void *)-1UL;
		return NULL;
	}
	return iter->marker;
}

static void s_stop(struct seq_file *m, void *p)
{
	marker_iter_stop(m->private);
}

static int s_show(struct seq_file *m, void *p)
{
	struct marker_iter *iter = m->private;

	seq_printf(m, "marker: %s format: \"%s\" state: %d "
		"probe: 0x%p pdata: 0x%p\n",
		iter->marker->name, iter->marker->format,
		_immediate_read(iter->marker->state), iter->marker->call,
		iter->marker->private);
	return 0;
}

static const struct seq_operations ltt_seq_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show,
};

static int ltt_open(struct inode *inode, struct file *file)
{
	/*
	 * Iterator kept in m->private.
	 * Restart iteration on all modules between reads because we do not lock
	 * the module mutex between those.
	 */
	int ret;
	struct marker_iter *iter;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	ret = seq_open(file, &ltt_seq_op);
	if (ret == 0)
		((struct seq_file *)file->private_data)->private = iter;
	else
		kfree(iter);
	return ret;
}

static struct file_operations ltt_fops = {
	.write = ltt_write,
	.open = ltt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

static void disconnect_all_markers(void)
{
	struct ltt_active_marker *pdata, *tmp;

	list_for_each_entry_safe(pdata, tmp, &markers_loaded_list, node) {
		marker_probe_unregister_private_data(pdata);
		list_del(&pdata->node);
		kmem_cache_free(markers_loaded_cachep, pdata);
	}
}

static int __init marker_control_init(void)
{
	int ret;

	pentry = create_proc_entry("ltt", S_IWUSR, NULL);
	if (!pentry)
		return -EBUSY;
	markers_loaded_cachep = KMEM_CACHE(ltt_active_marker, 0);

	ret = ltt_probe_register(&default_probe);
	BUG_ON(ret);
	ret = ltt_marker_connect("core_marker_format", DEFAULT_PROBE,
		MARKER_ID_SET_MARKER_FORMAT, GET_CHANNEL_INDEX(facilities),
		0, 1);
	BUG_ON(ret);
	ret = ltt_marker_connect("core_marker_id", DEFAULT_PROBE,
		MARKER_ID_SET_MARKER_ID, GET_CHANNEL_INDEX(facilities),
		0, 1);
	BUG_ON(ret);
#ifdef CONFIG_LTT_HEARTBEAT
	ltt_init_compact_markers(mark_next_compact_id);
	ret = ltt_marker_connect("core_heartbeat_32", DEFAULT_PROBE,
		MARKER_ID_HEARTBEAT_32, 0, 0, 1);
	BUG_ON(ret);
	ret = ltt_marker_connect("core_heartbeat_64", DEFAULT_PROBE,
		MARKER_ID_HEARTBEAT_64, 0, 0, 1);
	BUG_ON(ret);
#endif
	pentry->proc_fops = &ltt_fops;

	return 0;
}
module_init(marker_control_init);

static void __exit marker_control_exit(void)
{
	int ret;

	remove_proc_entry("ltt", NULL);
	ret = ltt_marker_disconnect("core_marker_format", 0);
	BUG_ON(ret);
	ret = ltt_marker_disconnect("core_marker_id", 0);
	BUG_ON(ret);
#ifdef CONFIG_LTT_HEARTBEAT
	ret = ltt_marker_disconnect("core_heartbeat_32", 0);
	BUG_ON(ret);
	ret = ltt_marker_disconnect("core_heartbeat_64", 0);
	BUG_ON(ret);
#endif
	ret = ltt_probe_unregister(&default_probe);
	BUG_ON(ret);
	disconnect_all_markers();
	kmem_cache_destroy(markers_loaded_cachep);
}
module_exit(marker_control_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("Linux Trace Toolkit Marker Control");
