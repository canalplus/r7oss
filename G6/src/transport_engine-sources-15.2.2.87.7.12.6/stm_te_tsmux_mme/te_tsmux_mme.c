/******************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

 This file is part of the Transport Engine Library.

 Transport Engine is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License version 2 as published by
 the Free Software Foundation.

 Transport Engine is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with Transport Engine; see the file COPYING.  If not, write to the Free
 Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 The Transport Engine Library may alternatively be licensed under a proprietary
 license from ST.

 Source file name : te_tsmux_mme.c

 Implements the stm_te tsmux MME interface
 ******************************************************************************/

#include <linux/init.h>    /* Initialisation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <mme.h>
#include <stm_te.h>
#include <stm_te_dbg.h>
#include <stm_se.h>
#include "wrapper/tsmux_mme.h"
#include "wrapper/TsmuxTransformerTypes.h"
#include "te_internal_cfg.h"
#include "te_tsmux_mme.h"
#include "te_tsg_filter.h"
#include "te_tsg_sec_filter.h"
#include "te_tsg_index_filter.h"
#include <stm_memsink.h>
#include <stm_memsrc.h>
#include <stm_event.h>

#define MAX_PES_HEADER_SIZE 20
#define MAX_STREAM_WAIT_TIME (7 * HZ)
#define MAX_DTS 0x1ffffffffull
#define InTimePeriod(V, S, E)	((E < S) ? ((V >= S) || (V < E)) : \
					((V >= S) && (V < E)))

/* TimeIsBefore:-
 * This macro is used to detect if a given PCR/PTS/DTS timestamp, V, is BEFORE
 * another timestamp, R.
 *
 * Unfortunately, this cannot just be a simple (V < R) type check.
 * The problem is that one of the timestamps could have wrapped and is
 * therefore smaller than the other timestamp, but would effectively be
 * later in time than the other.
 *
 * The normal (non-wrapped) case is where both V and R are on
 * the same side of the DTS wrap boundary, MAX_DTS.
 * In this case V should be LESS than R as a first condition.
 * To ensure we have V and R both on the same side of the wrap
 * boundary we also must check that the distance between V
 * and R is less than the distance from R to the wrap boundary
 * PLUS the distance from zero to V.
 *
 * The second (wrapped) case is when V and R are on different
 * sides of the DTS wrap boundary, MAX_DTS.
 * In this case V should be GREATER than R as the first condition.
 * To ensure that we are spanning the wrap boundary, we then also
 * need to check that the distance between V and R is GREATER
 * than the distance from V to the wrap boundary PLUS the distance
 * from zero to R.
 */
#define TimeIsBefore(V, R) (((V < R) && ((R - V) < (MAX_DTS - R + V))) || \
				((V > R) && ((V - R) > (MAX_DTS - V + R))))
#define	PcrLimit(V)	((V) & MAX_DTS)
#define TimeUnset(V)	(V == TSMUX_DEFAULT_64BIT_UNSPECIFIED)

/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static TSMUX_TransformerCapability_t	Capability;

struct te_send_buffer_params {
	MME_Command_t buffer_cmd;
	TSMUX_SendBuffersParam_t send_params;
	MME_DataBuffer_t *buffer_p;
	MME_DataBuffer_t buffer;
};

struct te_transform_params {
	MME_Command_t trans_cmd;
	TSMUX_TransformParam_t trans_params;
	MME_DataBuffer_t *buffer_p;
	MME_DataBuffer_t buffer;
};

struct te_tsg_buffer {
	struct stm_data_block	*block_list;
	uint32_t		block_count;
	uint32_t		length;
	bool			in_mme;
	struct te_tsg_filter    *parent;
	struct list_head	lh;
	unsigned char		pes_header[MAX_PES_HEADER_SIZE];
	int			pes_header_size;
	unsigned long long	dts;
	bool			eos;
	bool			insert_dit;
	bool			insert_rap;
	bool			is_pause;
	uint32_t		index_flags;
	/*
	 * This element added for flow control
	 * We will release this buffer when
	 * Previous index data in muxed out
	 */
	bool			buffer_queued;
	bool			ts_discontinuity_indicator;
};

/* Local Prototypes */
static int te_tsmux_mme_release_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *buffer, bool queue_in_tsg);
static bool tsmux_ready(struct te_tsmux *tsmx);
static bool check_mux_buffer_size(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count);
static bool check_all_tsg_flushing(struct te_tsmux *tsmx);
static bool check_audio_video_tsg_flushing(struct te_tsmux *tsmx);
static bool check_tsmux_restart(struct te_tsmux *tsmx,
		struct te_tsg_buffer *tsg_buffer,
		struct stm_se_compressed_frame_metadata_s *metadata,
		int err);
static bool check_all_tsg_ftd_marker(struct te_tsmux *tsmx,
		struct te_tsg_filter *cur_tsg);
static bool check_tsg_ftd_count_and_state(
		struct te_tsmux *tsmx, struct te_tsg_filter *current_tsg);
static bool check_tsg_ftd_wait(
		struct te_tsmux *tsmx, struct te_tsg_buffer *tsg_buffer,
		struct stm_se_compressed_frame_metadata_s *metadata);
static unsigned int stream_type_is_audio_video(unsigned int stream_type);
static int make_tsg_buffer(struct te_tsg_filter *tsg,
		struct stm_data_block *block_list,
		uint32_t block_count,
		struct te_tsg_buffer **tsg_buffer);

static void delete_tsg_buffers(struct te_tsmux *tsmx,
	struct te_tsg_filter *tsg);
static void delete_all_tsg_buffers(struct te_tsmux *tsmx);
static MME_ERROR get_capability(void);
static MME_ERROR open_transformer(struct te_tsmux *tsmux_p);
static MME_ERROR close_transformer(struct te_tsmux *tsmux_p);
static MME_ERROR add_program(struct te_tsmux *tsmux_p, bool tables_prog);
static MME_ERROR add_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p);
static MME_ERROR add_sec_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p);
static MME_ERROR remove_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p);
static int send_buffer_to_mme(struct te_tsmux *tsmx,
		struct te_tsg_buffer *tsg_buffer);
static int do_transform(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks);
static void MmeCallback(MME_Event_t event, MME_Command_t *callbackData,
		void *userData);
static bool tsmux_ready(struct te_tsmux *tsmx);
static bool tsmux_ready_locked(struct te_tsmux *tsmx);
static bool check_enough_data(struct te_tsmux *tsmx,
	struct te_tsg_filter *tsg);
static void delete_tsg_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *buffer);
static void abort_tsg(struct te_tsmux *tsmx, struct te_tsg_filter *tsg);
static int restart_tsmux(struct te_tsmux *tsmx, struct te_tsg_filter *tsg);
static int send_event(struct te_tsmux *tsmx,
		struct te_tsg_buffer *tsg_buffer,
		void *prv_data, int event_id);
static void auto_set_dont_pause_other(struct te_tsmux *tsmx,
		struct te_tsg_filter *cur_tsg);
static void auto_reset_dont_pause_other(struct te_tsmux *tsmx,
		struct te_tsg_filter *cur_tsg);

static bool meta_is_video(struct stm_se_compressed_frame_metadata_s *metadata)
{
	if (metadata->encoding >= STM_SE_ENCODE_STREAM_ENCODING_VIDEO_FIRST &&
		metadata->encoding <= STM_SE_ENCODE_STREAM_ENCODING_VIDEO_LAST)
		return true;
	else
		return false;
}

static bool meta_is_eos(struct stm_se_compressed_frame_metadata_s *metadata)
{
	if (metadata->discontinuity & STM_SE_DISCONTINUITY_EOS)
		return true;
	else
		return false;
}

static bool meta_insert_dit(struct stm_se_compressed_frame_metadata_s *metadata)
{
	/* Temporary method for inserting a DIT, not really correct as we
	 * actually want to know when video/audio encoding parameters change
	 * this requires new fields in the discontinuity enum in stm_se.h
	 */
	if (metadata->discontinuity & STM_SE_DISCONTINUITY_DISCONTINUOUS)
		return true;
	else
		return false;
}

static bool meta_insert_rap(struct stm_se_compressed_frame_metadata_s *metadata)
{
	/* Currently we can only generate rap on video stream */
	if (meta_is_video(metadata) && metadata->video.new_gop)
		return true;
	else
		return false;
}

static int __init te_tsmux_mme_init_module(void)
{
	pr_info("Load module te_tsmux_mme by %s (pid %i)\n",
			current->comm, current->pid);
	return 0;
}

static void __exit te_tsmux_mme_cleanup_module(void)
{
	pr_info("Unload module te_tsmux_mme by %s (pid %i)\n",
			current->comm, current->pid);
	return;
}

module_init(te_tsmux_mme_init_module);
module_exit(te_tsmux_mme_cleanup_module);


int te_tsmux_mme_init(void)
{
	MME_ERROR Error = MME_SUCCESS;
	int result = 0;

	Error = TsmuxRegister();
	if (Error != MME_SUCCESS) {
		pr_err("MME TsmuxRegister error %d\n", Error);

		if (Error == MME_INVALID_ARGUMENT)
			result = -EINVAL;
		else
			result = -EAGAIN;
		goto error;
	}
	Error = get_capability();
	if (Error != MME_SUCCESS) {
		pr_err("MME get_capability error %d\n", Error);
		result = -EINVAL;
	}

error:
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_init);

int te_tsmux_mme_term(void)
{
	MME_ERROR Error = MME_SUCCESS;
	int result = 0;

	Error = MME_DeregisterTransformer(TSMUX_MME_TRANSFORMER_NAME);
	if (Error != MME_SUCCESS) {
		pr_err("MME_DeregisterTransformer error %d\n", Error);

		result = -EINVAL;
	}
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_term);

int te_tsmux_mme_submit_section(struct te_tsmux *tsmx, struct te_tsg_sec_filter *sec)
{
	struct stm_se_compressed_frame_metadata_s metadata;
	struct stm_data_block block_list;
	int err;

	if (sec->table == NULL) {
		pr_err("no section data for sec filter %p\n", &sec->tsg.obj);
		return -EINVAL;
	}

	pr_debug("submitting section %p length %d\n", sec->table, sec->table_len);

	memset(&metadata, 0x00, sizeof(metadata));
	memset(&block_list, 0x00, sizeof(block_list));

	if (mutex_lock_interruptible(&sec->tsg.obj.lock) != 0)
		return -EINTR;

	metadata.native_time_format = TIME_FORMAT_PTS;
	metadata.native_time = TSMUX_DEFAULT_64BIT_UNSPECIFIED;

	block_list.len = sec->table_len;
	block_list.data_addr = sec->table;

	err = te_tsmux_mme_send_buffer(tsmx, &sec->tsg, &metadata, &block_list, 1);

	if (err == 0)
		sec->submitted = true;
	else
		pr_err("couldn't submit section\n");

	mutex_unlock(&sec->tsg.obj.lock);

	return err;
}
EXPORT_SYMBOL(te_tsmux_mme_submit_section);

int te_tsmux_mme_cancel_section(struct te_tsmux *tsmx, struct te_tsg_sec_filter *sec)
{
	struct stm_se_compressed_frame_metadata_s metadata;
	struct stm_data_block block_list;

	pr_debug("cancelling section\n");

	memset(&metadata, 0x00, sizeof(metadata));
	memset(&block_list, 0x00, sizeof(block_list));

	return te_tsmux_mme_send_buffer(tsmx, &sec->tsg, &metadata, &block_list, 1);
}

static int add_and_submit_section(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg)
{
	struct te_tsg_sec_filter *sec;
	int err;

	sec = container_of(tsg, struct te_tsg_sec_filter, tsg);
	err = add_sec_stream(tsmx, tsg);
	if (err != MME_SUCCESS) {
		pr_err("MME add_stream error %d\n", err);
		err = -EINVAL;
		return err;
	}
	tsg->state = TE_TSG_STATE_RUNNING;

	if (sec->table != NULL)
		err = te_tsmux_mme_submit_section(tsmx, sec);

	return err;
}

EXPORT_SYMBOL(te_tsmux_mme_cancel_section);
static int te_tsmux_mme_start_filter(struct te_tsmux *tsmx, struct te_tsg_filter *tsg)
{
	MME_ERROR Error;
	int err = 0;

	switch (tsg->obj.type) {
	case STM_TE_TSG_FILTER:
		/* Can't add an tsg stream if it is not connected */
		/* allow tsg fitler to start, if this is stopped */
		if (!(tsg->state == TE_TSG_STATE_CONNECTED ||
			tsg->state == TE_TSG_STATE_STOPPED)) {
			pr_err("Attempt to start tsmux with tsg filter not connected\n");
			err = -EINVAL;
			goto error;
		}
		Error = add_stream(tsmx, tsg);
		if (Error != MME_SUCCESS) {
			pr_err("MME add_stream error %d\n", Error);
			err = -EINVAL;
			goto error;
		}
		tsg->state = TE_TSG_STATE_STARTED;
		break;

	case STM_TE_TSG_SEC_FILTER:
/* To avoid releasing lock during restart of tsmux */
		if (!tsmx->restarting) {

			err = add_and_submit_section(tsmx, tsg);
		}
		break;
	default:
		pr_err("unknown TSG type");
		err = -EINVAL;
	}
error:
	return err;
}

int te_tsmux_mme_start(struct te_tsmux *tsmx)
{
	MME_ERROR Error = MME_SUCCESS;
	struct te_tsg_filter *tsg;
	int result = 0;

	/* Can't start a tsmux if it is now in state NEW */
	/* Should allow to start tsmux if stopped */
	if (!(tsmx->state == TE_TSMUX_STATE_NEW ||
		tsmx->state == TE_TSMUX_STATE_STOPPED)) {
		pr_err("Attempt to start tsmux in invalid state\n");
		result = -EINVAL;
		goto out;
	}
	/* Initialise the TSMUX MME transformer */
	Error = open_transformer(tsmx);
	if (Error != MME_SUCCESS) {
		pr_err("MME open_transformer error %d\n", Error);
		result = -EINVAL;
		goto out;
	}

	tsmx->transformer_valid = true;

	/* Add program to the TSMUX MME transformer */
	Error = add_program(tsmx, FALSE);
	if (Error != MME_SUCCESS) {
		pr_err("MME add_program error %d\n", Error);
		result = -EINVAL;
		goto error_close_trans;
	}

	Error = add_program(tsmx, TRUE);
	if (Error != MME_SUCCESS) {
		pr_err("MME add_program for tables error %d\n", Error);
		result = -EINVAL;
		goto error_close_trans;
	}

	/* Now add a stream for each tsg filter and mark them as started */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		result = te_tsmux_mme_start_filter(tsmx, tsg);
		if (result != 0)
			goto error_close_trans;
	}
	tsmx->state = TE_TSMUX_STATE_STARTED;
	goto out;

error_close_trans:
	tsmx->transformer_valid = false;
	close_transformer(tsmx);
out:
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_start);

int te_tsmux_mme_stop(struct te_tsmux *tsmx)
{
	MME_ERROR Error = MME_SUCCESS;
	struct te_tsg_filter *tsg;
	int result = 0;

	if (tsmx->state == TE_TSMUX_STATE_STOPPED) {
		pr_debug("Tsmux stop called when already stopped\n");
		goto out;
	}
	if (tsmx->transformer_valid) {
		/* Remove stream each tsg filter and mark as stopped
		 * This should also free all currently queued buffer */
		list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
			if (tsg->state == TE_TSG_STATE_STOPPED)
				continue;
			Error = remove_stream(tsmx, tsg);
			if (Error != MME_SUCCESS) {
				pr_err("MME remove_stream error %d\n",
						Error);
				result = -EINVAL;
			}
			tsg->state = TE_TSG_STATE_STOPPED;
		}
		Error = close_transformer(tsmx);
		if (Error != MME_SUCCESS) {
			pr_err("MME close_transformer error %d\n",
					Error);
			result = -EINVAL;
		}
		tsmx->transformer_valid = false;
	}
	/* Close transformer should have freed all tsg buffers but just
	 * in case lets do it again */
	delete_all_tsg_buffers(tsmx);
	tsmx->state = TE_TSMUX_STATE_STOPPED;

	/* we need to calculate new PCR again */
	tsmx->min_DTS = tsmx->last_PCR = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	if(tsmx->memory_used || tsmx->total_buffers)
		pr_err("te_tsmux_mme_stop .. after stop buffer in use\n");

	/* Wake up the get_data in case it is waiting */
	if (!tsmx->restarting)
		wake_up_interruptible(&tsmx->ready_wq);


out:
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_stop);

int te_tsmux_mme_tsg_connect(struct te_tsmux *tsmx, struct te_tsg_filter *tsg_p)
{
	int result = 0;

	if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
		result = -EINTR;
		goto error;
	}
	if (tsmx->state == TE_TSMUX_STATE_STOPPED &&
		(tsg_p->state == TE_TSG_STATE_DISCONNECTED ||
		 tsg_p->state == TE_TSG_STATE_NEW)){
		pr_debug("reconnect tsg filter\n");
	} else if (tsmx->state >= TE_TSMUX_STATE_STARTED) {
		pr_err("Attempt to connect an tsg filter when tsmux is running\n");
		result = -EINVAL;
		goto error;
	}
	tsg_p->state = TE_TSG_STATE_CONNECTED;
	/* The tsmux might be waiting on this tsg filter being connected, so
	 * wake it up. */
	wake_up_interruptible(&tsmx->ready_wq);
	mutex_unlock(&tsmx->obj.lock);
error:
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_tsg_connect);

int te_tsmux_mme_tsg_disconnect(struct te_tsmux *tsmx,
	struct te_tsg_filter *tsg)
{
	int result = 0;

	if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
		result = -EINTR;
		goto error;
	}
	if (tsg->state != TE_TSG_STATE_STOPPED) {
		pr_err("Attempt to disconnect tsg filter when tsmux is not stopped\n");
		result = -EINVAL;
	}
	/* Delete any remaining tsg buffers */
	delete_tsg_buffers(tsmx, tsg);
	tsg->state = TE_TSG_STATE_DISCONNECTED;
	/* If the tsmux is still operating, then wake it up in case it is
	 * waiting on this tsg filter */
	if (tsmx->transformer_handle)
		wake_up_interruptible(&tsmx->ready_wq);

	mutex_unlock(&tsmx->obj.lock);
error:
	return result;
}
EXPORT_SYMBOL(te_tsmux_mme_tsg_disconnect);

static void process_metadata(struct te_tsg_buffer *tsg_buffer,
	struct stm_se_compressed_frame_metadata_s *metadata)
{
	uint32_t index_flags = STM_TE_TSG_INDEX_PUSI | STM_TE_TSG_INDEX_PTS;
	index_flags |= STM_TE_TSG_INDEX_STREAM_ID |
			STM_TE_TSG_INDEX_STREAM_TYPE;
	if (meta_insert_dit(metadata)) {
		index_flags |= STM_TE_TSG_INDEX_DIT;
		tsg_buffer->insert_dit = true;
		pr_debug("dit_transition received!\n");
	}
	if (tsg_buffer->parent->include_rap && meta_insert_rap(metadata)) {
		index_flags |= STM_TE_TSG_INDEX_RAP;
		tsg_buffer->insert_rap = true;
		pr_debug("rap_transition received! tsg %p\n",
			tsg_buffer->parent);
	}
	if (meta_is_video(metadata) &&
		metadata->video.picture_type != STM_SE_PICTURE_TYPE_UNKNOWN) {

		switch (metadata->video.picture_type) {
		case(STM_SE_PICTURE_TYPE_I):
			index_flags |= STM_TE_TSG_INDEX_I_FRAME;
		break;
		case(STM_SE_PICTURE_TYPE_P):
			index_flags |= STM_TE_TSG_INDEX_P_FRAME;
		break;
		case(STM_SE_PICTURE_TYPE_B):
			index_flags |= STM_TE_TSG_INDEX_B_FRAME;
		break;
		default:
			break;
		}
	}
	tsg_buffer->index_flags = index_flags;

	if (meta_is_eos(metadata))
		tsg_buffer->eos = true;

	/* TODO Bug 39702 Insert code to check for a PREPARE_TIME_DISCONTINUITY
	 * so that we can trigger a pause
	 */

	return;
}

static void mask_index_flags(struct te_tsg_buffer *tsg_buffer)
{
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	uint32_t index_mask = 0;
	struct te_tsg_index_filter *tsg_index_filter;
	int i;

	/* Build up the index flags */
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		tsg_index_filter = (struct te_tsg_index_filter *)
			tsg->tsg_index_filters[i];
		if (NULL != tsg_index_filter)
			index_mask |= tsg_index_filter->index_mask;
	}

	tsg_buffer->index_flags &= index_mask;

	return;
}

static int get_pes_dts(struct te_tsg_buffer *tsg_buffer,
	unsigned long long *dts)
{
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	unsigned char *Data;
	unsigned long long pes_dts;

	Data = tsg_buffer->block_list->data_addr;
	pes_dts = 0;
	pr_debug("MME length %d, Data[7] = 0x%x\n", tsg_buffer->length,
		Data[7]);

	/* TODO Add checks for valid PES header before extracting DTS */
	if (tsg_buffer->length == 0) {
		/* If buffer is zero bytes just ignore dts */
	} else if ((Data[7] & 0xc0) == 0x80) {
		/* Read the PTS for the DTS value */
		pes_dts = ((unsigned long long)Data[9]  & 0x0e) << 29;
		pes_dts |= ((unsigned long long)Data[10] & 0xff) << 22;
		pes_dts |= ((unsigned long long)Data[11] & 0xfe) << 14;
		pes_dts |= ((unsigned long long)Data[12] & 0xff) << 7;
		pes_dts |= ((unsigned long long)Data[13] & 0xfe) >> 1;
	} else if ((Data[7] & 0xc0) == 0xc0) {
		/* Read the actual DTS value */
		pes_dts = ((unsigned long long)Data[14] & 0x0e) << 29;
		pes_dts |= ((unsigned long long)Data[15] & 0xff) << 22;
		pes_dts |= ((unsigned long long)Data[16] & 0xfe) << 14;
		pes_dts |= ((unsigned long long)Data[17] & 0xff) << 7;
		pes_dts |= ((unsigned long long)Data[18] & 0xfe) >> 1;
	} else {
		/* Return last_DTS */
		pr_debug("No PES header on this buffer %p\n", Data);
		pes_dts = tsg->last_DTS;
	}
	/* store stream id for indexing record */
	tsg_buffer->parent->stream_id_index = Data[3];
	pr_debug("Found DTS =  0x%010llx\n", pes_dts);
	*dts = pes_dts;

	return 0;
}

static void get_dts(struct te_tsmux *tsmx,
	struct te_tsg_buffer *tsg_buffer,
	struct stm_se_compressed_frame_metadata_s *metadata)
{
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	unsigned long long dts = 0ULL;

	if (!tsg_buffer->eos) {
		/* We don't yet support different DTS from PTS */
		/*
		 * This is PTS from encoder on which
		 * we have to do muxing
		 */
		dts = PcrLimit(metadata->encoded_time);
		/*
		 * This is orignal stream PTS
		 * Application needs it for indexing purpose
		 */
		tsg->native_DTS = PcrLimit(metadata->native_time);
		if (tsg->obj.type != TE_OBJ_TSG_SEC_FILTER) {
			/* If we are receiving PES packets, then the dts value
			 * needs to be overridden by the value in the pes header
			 */
			if (tsg->stream_is_pes)
				get_pes_dts(tsg_buffer, &dts);
		} else {
			/* We want to insert the section as soon as possible
			 * so set the DTS to the beginning of the next PCR
			 * period. If at start set DTS to zero.
			 */
			if (TimeUnset(tsmx->last_PCR))
				dts = 0;
			else
				dts = PcrLimit(tsmx->last_PCR +
						tsmx->pcr_period);
			pr_debug("Section to be inserted with DTS=0x%010llx",
				dts);
		}
	} else {
		/* For the EOS buffer, create a dummy timestamp */
		dts = PcrLimit(tsg->last_DTS + tsmx->pcr_period);
		pr_debug("Created dummy timestamp for EOS on stream=%d:%d DTS=0x%010llx\n",
			tsg->program_id, tsg->stream_id, dts);
	}
	tsg_buffer->dts = dts;
	pr_debug("DTS for tsg_buffer %p is 0x%010llx\n", tsg_buffer, dts);

	return;
}

static int check_dts(struct te_tsmux *tsmx, struct te_tsg_buffer *tsg_buffer)
{
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	int err = 0;
	unsigned long long dts = tsg_buffer->dts;

	if (!tsg->dts_integrity_threshold || tsg_buffer->eos)
		goto out;

	if (TimeUnset(tsg->last_DTS)) {
		pr_err("Invalid last_DTS on stream %d:%d\n",
				tsg->program_id, tsg->stream_id);
		err = -EINVAL;
		goto out;
	}
	pr_debug("Checking Stream=%d:%d DTS=0x%010llx against lastDTS=0x%010llx\n",
		tsg->program_id, tsg->stream_id, dts, tsg->last_DTS);
	if ((tsg->state != TE_TSG_STATE_PAUSED) &&
			!InTimePeriod(dts, tsg->last_DTS,
				PcrLimit(tsg->last_DTS +
					tsg->dts_integrity_threshold))) {
		pr_err("Failed timestamp check Stream=%d:%d LastDTS=0x%010llx ThisDTS=0x%010llx\n",
				tsg->program_id, tsg->stream_id,
				tsg->last_DTS, dts);
		tsg->dts_discont_cnt--;
		if (!tsg->dts_discont_cnt) {
			pr_info("Discontinuity detected\n");
			tsg->full_discont_marker = true;
			tsg->dts_discont_cnt =
					TSMUX_MAX_DTS_DISCONTINUITY_COUNT;
		}
		err = -EINVAL;
		goto out;
	} else {
		if ((!tsg->full_discont_marker)
				&& (tsg->dts_discont_cnt
				< TSMUX_MAX_DTS_DISCONTINUITY_COUNT)
				&& (tsg->state != TE_TSG_STATE_PAUSED)) {
			tsg->dts_discont_cnt++;
		}
	}

	if (!TimeUnset(tsmx->last_PCR)) {
		pr_debug("Checking Stream=%d:%d DTS=0x%010llx against lastPCR=0x%010llx\n",
			tsg->program_id, tsg->stream_id,
			dts, tsmx->last_PCR);
		if (TimeIsBefore(dts, tsmx->last_PCR)) {
			pr_err("Failed timestamp check Stream=%d:%d LastPCR=0x%010llx ThisDTS=0x%010llx\n",
					tsg->program_id, tsg->stream_id,
					tsmx->last_PCR, dts);
			/* This additional logic is required to bring
			 * the stream in sync after FTD if it remains
			 * beyond from last PCR continuously. It could
			 * be possible in bypass mode. It avoids continuous
			 * failure.*/
			auto_set_dont_pause_other(tsmx, tsg);
			err = -EINVAL;
			goto out;
		} else if (TimeIsBefore(tsmx->last_PCR
				+ tsg->dontwaitlimit,
				dts))
				auto_reset_dont_pause_other(tsmx, tsg);
	}

out:
	return err;
}

/* This function is used to check restart condition */
/* has been met or not after FTD */
static bool check_tsmux_restart(struct te_tsmux *tsmx,
		struct te_tsg_buffer *tsg_buffer,
		struct stm_se_compressed_frame_metadata_s *metadata,
		int err)
{
	bool tsmux_restart_done = false;
	bool full_discontinuity;
	uint64_t	dts;
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	dts = tsg_buffer->dts;

	full_discontinuity = check_all_tsg_ftd_marker(tsmx, tsg);
	if (full_discontinuity) {
		tsmux_restart_done = true;
		pr_warn("Full Discontinuity detected on all streams\n");
	} else {
		if ((!full_discontinuity) && (tsg->full_discont_marker)
				&& err) {
			pr_debug("checking partial discontinuity err = %d\n",
					err);
			if (TimeUnset(tsg->ftd_first_DTS)) {
				/* Store DTS after FTD */
				tsg->ftd_first_DTS = dts;
				pr_info("Stream Type = %u  ftd_first DTS = %llx\n",
							tsg->stream_type,
							tsg->ftd_first_DTS);

			 } else {

				 /* Check discontinuous DTS within range */
				if (InTimePeriod(dts,
					tsg->ftd_first_DTS,
					PcrLimit(tsg->ftd_first_DTS +
					tsg->dts_integrity_threshold))) {
					tsmux_restart_done =
						check_tsg_ftd_count_and_state(
							tsmx, tsg);
					 if (tsmux_restart_done)
						pr_warn(
							"Full Discontinuity not detected on all streams\n");
					 if (!tsmux_restart_done)
						tsmux_restart_done =
							check_tsg_ftd_wait(
							tsmx,
							tsg_buffer, metadata);
				 } else {
					 /* Reset ftd_first_DTS if it fails
					  * to pass dts_integrity_threshold
					  * check. It can fail due to corrupt
					  * value of dts_integrity_threshold*/
					 tsg->ftd_first_DTS =
						TSMUX_DEFAULT_64BIT_UNSPECIFIED;
					 pr_debug("Reseting ftd_first_DTS\n");
				 }
			 }
		 }
	}
	return tsmux_restart_done;
}

static bool check_all_tsg_ftd_marker(
		struct te_tsmux *tsmx, struct te_tsg_filter *cur_tsg)
{
	bool full_discontinuity = true;
	struct te_tsg_filter *tsg;

	/* Check if full discontinuity on all the streams*/
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (!stream_type_is_audio_video(tsg->stream_type))
			continue;
		if (!tsg->full_discont_marker)
			full_discontinuity = false;
	}

	return full_discontinuity;
}

static bool check_tsg_ftd_count_and_state(
		struct te_tsmux *tsmx, struct te_tsg_filter *cur_tsg)
{
	bool full_discontinuity = true;
	struct te_tsg_filter *tsg;
	uint32_t current_stream_id;

	current_stream_id = cur_tsg->stream_id;

	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (!stream_type_is_audio_video(tsg->stream_type))
			continue;

		/* It is used to check that discontinuity on other */
		/* stream are occuring or not. If occuring, set */
		/* false till FTD is not detected on all streams */
		if ((tsg->dts_discont_cnt !=
				TSMUX_MAX_DTS_DISCONTINUITY_COUNT)
				&& (tsg->stream_id != current_stream_id)) {
			if (!tsg->full_discont_marker)
				full_discontinuity = false;
		}
		if (!(tsg->state & (TE_TSG_STATE_STOPPED |
							TE_TSG_STATE_PAUSED))) {
			if (!tsg->full_discont_marker)
				full_discontinuity = false;
		}
	}

	return full_discontinuity;
}

static bool check_tsg_ftd_wait(
		struct te_tsmux *tsmx, struct te_tsg_buffer *tsg_buffer,
		struct stm_se_compressed_frame_metadata_s *metadata)
{
	bool full_discontinuity = false;
	uint64_t	dts;
	bool intime;
	struct te_tsg_filter *cur_tsg = tsg_buffer->parent;
	dts = tsg_buffer->dts;
	intime = InTimePeriod(dts,
							cur_tsg->ftd_first_DTS,
					PcrLimit(cur_tsg->ftd_first_DTS
					+ tsmx->full_discont_wait_time));

	pr_debug("Stream Type = %u  ftd first DTS = %llx DTS = %llx",
				cur_tsg->stream_type, cur_tsg->ftd_first_DTS,
				dts);
	/* Reset if current DTS is behind than ftd_first_DTS.
	 * ftd_first_DTS must be less than current DTS to ftd
	 * wait workable */
	if (TimeIsBefore(dts, cur_tsg->ftd_first_DTS))
		cur_tsg->ftd_first_DTS =
					TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	/* Wait if discontinuity has not been detected */
	/* on all the streams till FTD wait limit is over */
	if ((cur_tsg->full_discont_marker)
				&& !intime) {
		full_discontinuity = true;
		if (cur_tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			full_discontinuity = false;
		if (!stream_type_is_audio_video(cur_tsg->stream_type))
			full_discontinuity = false;
	}
	pr_debug("Wait full_discontinuity = %d intime = %d",
			full_discontinuity, intime);
	return full_discontinuity;
}

static unsigned int get_pes_stream_id(unsigned int stream_type)
{
	unsigned int stream_id = 0;
	switch (stream_type) {
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG1:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG2:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG4:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_H264:
		stream_id = 0xe0;
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG1:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG2:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_AAC:
		stream_id = 0xc0;
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_AC3:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_DTS:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_LPCM:
		stream_id = 0xbd;
		break;
	default:
		pr_warn("Unrecognised stream_type\n");
		break;
	}
	return stream_id;
}

static unsigned int stream_type_is_video(unsigned int stream_type)
{
	bool result = false;

	switch (stream_type) {
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG1:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG2:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG4:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_H264:
		result = true;
		break;
	default:
		result = false;
		break;
	}
	return result;
}

static unsigned int stream_type_is_audio(unsigned int stream_type)
{
	bool result = false;

	switch (stream_type) {
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG1:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG2:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_AAC:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_AC3:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_DTS:
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_LPCM:
		result = true;
		break;
	default:
		result = false;
		break;
	}
	return result;
}

static unsigned int stream_type_is_audio_video(unsigned int stream_type)
{
	bool result = false;
	if (stream_type_is_audio(stream_type) ||
			stream_type_is_video(stream_type))
		result = true;
	return result;
}
static void add_pes_header(struct te_tsg_buffer *tsg_buffer)
{
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	unsigned char *data = &tsg_buffer->pes_header[0];
	unsigned int header_length = 0;
	unsigned long long PTS = tsg_buffer->dts;
	/* We don't yet support DTS generation */

	/* packet_start_code_prefix */
	*data++ = 0x00;
	*data++ = 0x00;
	*data++ = 0x01;
	/* stream_id */
	if (tsg->pes_stream_id != TSMUX_DEFAULT_UNSPECIFIED)
		tsg->stream_id_index = *data++ = tsg->pes_stream_id;
	else
		tsg->stream_id_index = *data++ =
			get_pes_stream_id(tsg->stream_type);

	/* PES_packet_length */
	if (stream_type_is_video(tsg->stream_type)) {
		*data++ = 0x0;
		*data++ = 0x0;
	} else {
		*data++ = ((tsg_buffer->length + 8) >> 8) & 0xff;
		*data++ = ((tsg_buffer->length + 8) & 0xff);
	}
	/* '10', PES_scambling_control, PES_priority,
	 *  data_alignment_indicator, copyright, original_or_copy
	 *  Just setting the '10' */
	*data++ = 0x80;
	/* PTS_DTS_flags, ESCR_flag, ES_rate_flag, DSM_trick_mode_flag,
	 * additional_copy_flag, PES_CRC_flag, PES_extension_flag
	 * Just setting PTS_DTS_flags */
	/* PTS_DTS_flags = '10' */
	*data++ = 0x80;
	/* PES_header_data_length = 5 bytes */
	*data++ = 0x05;
	/* PTS */
	*data++ = ((PTS >> 29) & 0x0e) | 1 | 0x20;
	*data++ = ((PTS >> 22) & 0xff);
	*data++ = ((PTS >> 14) & 0xfe) | 1;
	*data++ = ((PTS >> 7) & 0xff);
	*data++ = ((PTS << 1) & 0xfe) | 1;
	header_length = 14;
	tsg_buffer->pes_header_size = header_length;

	pr_debug("Creating PES header for dts 0x%010llx [%02x%02x%02x%02x%02x]\n",
		PTS,
		tsg_buffer->pes_header[9],
		tsg_buffer->pes_header[10],
		tsg_buffer->pes_header[11],
		tsg_buffer->pes_header[12],
		tsg_buffer->pes_header[13]);

	return;
}

static bool first_dts_check_with_pcr(struct te_tsmux *tsmx,
					struct te_tsg_filter *tsg)
{
	bool result = true;
	pr_debug("Checking stream=%d DTS=0x%010llx Last PCR =0x%010llx\n",
		tsg->stream_id, tsg->first_DTS,
		tsmx->last_PCR);
	if (!TimeUnset(tsmx->last_PCR) &&
		!InTimePeriod(tsg->first_DTS,
			tsmx->last_PCR - tsg->dts_integrity_threshold,
			tsmx->last_PCR + tsg->dts_integrity_threshold)){
		pr_err("Failed first DTS check with PCR Stream=%d:%d  DTS=0x%010llx last_PCR=0x%010llx\n",
			tsg->program_id, tsg->stream_id, tsg->first_DTS,
			tsmx->last_PCR);
		result = false;
	}
	return result;

}

static bool first_dts_ok(struct te_tsg_filter *tsg,
	struct te_tsg_filter *reftsg)
{
	bool result = true;
	unsigned long long threshold;

	pr_debug("Checking stream=%d DTS=0x%010llx against other stream=%d DTS=0x%010llx\n",
		tsg->stream_id, tsg->first_DTS, reftsg->stream_id,
		reftsg->first_DTS);
	threshold = max(tsg->dts_integrity_threshold,
			reftsg->dts_integrity_threshold);
	if (!InTimePeriod(tsg->first_DTS,
			PcrLimit(reftsg->first_DTS - threshold),
			PcrLimit(reftsg->first_DTS + threshold))) {
		result = false;
	}

	return result;
}

static void abort_tsg(struct te_tsmux *tsmx, struct te_tsg_filter *tsg)
{
	struct te_tsg_buffer *tsg_buffer, *tmp_buffer;
	MME_ERROR Error = MME_SUCCESS;

	/* Set tsg state to stopped to ignore future buffers */
	tsg->state = TE_TSG_STATE_STOPPED;
	/* Remove the stream from the mme muxer */
	Error = remove_stream(tsmx, tsg);
	if (Error != MME_SUCCESS) {
		pr_warn("MME remove_stream error %d\n",
				Error);
	}
	/* Release all pending queued buffers */
	list_for_each_entry_safe(tsg_buffer, tmp_buffer,
			&tsg->tsg_mme_buffer_list, lh) {
		delete_tsg_buffer(tsmx, tsg_buffer);
	}
	return;
}

void reinit_ftd_para(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg)
{
	struct te_tsg_buffer *tsg_buffer, *tmp_buffer;

	if (tsg->state != TE_TSG_STATE_STOPPED)
		abort_tsg(tsmx, tsg);
	tsg->full_discont_marker = false;
	tsg->dts_discont_cnt = TSMUX_MAX_DTS_DISCONTINUITY_COUNT;
	/* Internal parameters */
	tsg->last_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	tsg->first_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	tsg->ftd_first_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;

	list_for_each_entry_safe(tsg_buffer, tmp_buffer,
				&tsg->tsg_buffer_list, lh) {
			delete_tsg_buffer(tsmx, tsg_buffer);
	}

	INIT_LIST_HEAD(&tsg->tsg_buffer_list);
	INIT_LIST_HEAD(&tsg->tsg_mme_buffer_list);

	return ;
}
EXPORT_SYMBOL(reinit_ftd_para);

/* This function restarts tsmux when ftd is detected */
static int restart_tsmux(struct te_tsmux *tsmx,
	struct te_tsg_filter *cur_tsg_filter)
{
	int err = 0;
	struct te_tsg_filter *tsg;
	bool eos_state = false;

	pr_info("Restarting tsmux\n");

	tsmx->restart_complete = false;
	tsmx->restarting = true;
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		reinit_ftd_para(tsmx, tsg);
		if (tsg->eos_detected &&
				stream_type_is_audio_video(tsg->stream_type))
				eos_state = true;
	}

	err = te_tsmux_mme_stop(tsmx);
	if (err) {
		pr_err("Couldn't stop tsmux.\n");
		goto out;
	}

	if (eos_state)
		goto out;

	tsmx->global_flags = (PCR_DISCONTINUITY_FLAG
			| TABLE_DISCONTINUITY_FLAG);
	err = te_tsmux_mme_start(tsmx);
	if (err) {
		pr_err("Couldn't start tsmux.\n");
		goto out;
	}
	tsmx->global_flags = 0;

	/* It aborts the stream after End of Stream detection */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->eos_detected) {
			abort_tsg(tsmx, tsg);
			tsg->eos_detected = false;
			pr_debug("Stream type = %u eos detected\n",
					tsg->stream_type);
		}
		tsg->queued_buf = false;
	}

	tsmx->restart_complete = true;

	/* To avoid Section data insertion issue after restart */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			err = add_and_submit_section(tsmx, tsg);
	}

out:
	tsmx->restarting = false;
	wake_up_interruptible(&tsmx->ready_wq);
	return err;
}

static bool do_first_dts_checks(struct te_tsg_filter *tsg)
{
	bool result = true;

	/* Don't do first dts checks on section streams */
	if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
		result = false;
	/* Don't do first dts checks on a tsg which is stopped */
	if (tsg->state == TE_TSG_STATE_STOPPED)
		result = false;
	/* Don't do first dts checks if first DTS has not been received */
	if (TimeUnset(tsg->first_DTS))
		result = false;

	return result;
}

static void check_first_dts(struct te_tsmux *tsmx, struct te_tsg_filter *tsg)
{
	struct te_tsg_filter *othertsg;

	pr_debug("Cross checking stream %d:%d first DTS against other streams\n",
		tsg->program_id, tsg->stream_id);

	if (!do_first_dts_checks(tsg)) {
		pr_debug("Don't need to check this streams first dts\n");
		goto out;
	}

	list_for_each_entry(othertsg, &tsmx->tsg_filters, obj.lh) {
		if (othertsg == tsg)
			continue;
		if (!do_first_dts_checks(othertsg))
			continue;

		if (!first_dts_ok(tsg, othertsg)) {
			pr_warn("Failed initial timestamp check Stream=%d:%d DTS=0x%010llx otherStream=%d:%d DTS=0x%010llx\n",
				tsg->program_id, tsg->stream_id,
				tsg->first_DTS,
				othertsg->program_id, othertsg->stream_id,
				othertsg->first_DTS);
			/* New stream should be ahead ahead of last PCR
			 * In case this stream arrived quite last
			 */
			/* This condition might not be true,
			 * if this stream is generating initial PCR
			 * On lenient way to check DTS of this stream
			 * within range of dts integrity threshold
			 */
			if (!first_dts_check_with_pcr(tsmx, tsg)) {
				pr_err("Pausing stream %d:%d for tsmux operation\n",
					tsg->program_id, tsg->stream_id);
				/* TODO We should prefer abort of non-video
				 * stream
				 */
				te_tsmux_mme_tsg_pause(tsmx, tsg);
			}
		}
	}
out:
	return;
}

static void update_initial_pcr(struct te_tsmux *tsmx,
	struct te_tsg_filter *tsg)
{
	struct te_tsg_filter *tmptsg;
	unsigned long long totalTStdBits = 0;
	unsigned long long timeToFill;
	bool prewrapped = false;
	bool updatePCR = false;

	if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
		return;
	/* Some tsgs may have been stopped due to bad
	 * initial timestamps, so ignore them */
	if (!(tsg->state & (TE_TSG_STATE_RUNNING |
			TE_TSG_STATE_FLUSHING)))
		return;

	if (TimeUnset(tsmx->min_DTS)) {
		tsmx->min_DTS = tsg->first_DTS;
		updatePCR = true;
	}
	/* Normal case where both minDTS and tsg->first_DTS are on
	 * the same side of the DTS wrap boundary.
	 * We take the new DTS if it is less than the current minDTS.
	 * This is confirmed with the second condition which compares
	 * the distance between the timestamps compared to the distance
	 * to the MAX plus the distance from zero. In this case the
	 * distance between the timestamps should be shorter */
	if ((tsg->first_DTS < tsmx->min_DTS) &&
			((tsmx->min_DTS - tsg->first_DTS) <
				(MAX_DTS - tsmx->min_DTS + tsg->first_DTS))) {
		tsmx->min_DTS = tsg->first_DTS;
		updatePCR = true;
	}
	/* Case where one of the input DTS values is pre-wrapped due
	 * to, for example, audio timestamp correction.
	 * In this case the 2 timestamps span across the max timestamp
	 * boundary. So here the new DTS is taken if it is MORE than
	 * the current minDTS!.
	 * We confirm that we are spanning the max DTS boundary by
	 * confirming that the distance between the 2 timestamps is
	 * greater than the sum of the distance to the max value plus
	 * the distance from zero. In this case distance between the
	 * timestamps will be greater.
	 * We also flag here that we have found a pre-wrapped timestamp
	 * so that we can adjust the PCR offset calculation accordingly
	 */
	if ((tsg->first_DTS > tsmx->min_DTS) &&
			((tsg->first_DTS - tsmx->min_DTS) >
				(MAX_DTS - tsg->first_DTS + tsmx->min_DTS))) {
		tsmx->min_DTS = tsg->first_DTS;
		updatePCR = true;
		prewrapped = true;
	}


	if (updatePCR) {
		list_for_each_entry(tmptsg, &tsmx->tsg_filters, obj.lh) {
			if (tmptsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
				continue;
			totalTStdBits += tmptsg->bit_buffer_size;
		}

		timeToFill = (totalTStdBits * 90000ULL) / tsmx->bitrate;
		if (timeToFill < tsmx->pcr_period) {
			pr_debug("Time to fill is less than PCR period, extending\n");
			timeToFill = tsmx->pcr_period;
		}
		tsmx->last_PCR = PcrLimit(tsmx->min_DTS - timeToFill);

		pr_debug("minDTS=0x%010llx timetofill=0x%010llx calc PCR=0x%010llx\n",
			tsmx->min_DTS, timeToFill, tsmx->last_PCR);

		/* Offset the PCR and all PTS/DTS values if the first PCR is
		 * wrapped or if one of the input DTS values was pre-wrapped
		 * relative to the others (which guarantees that the PCR is
		 * wrapped).
		 * The shift is calculated so that the first PCR value is zero.
		 * NOTE: The shift is not applied at this level instead it is
		 * recalculated down in the muxer code. This is just used for
		 * debug
		 */
		if (prewrapped || (tsmx->last_PCR > tsmx->min_DTS)) {
			/* Add 1 to take last_PCR to 0 instead of 0x1ffffffff */
			tsmx->shift_PCR = MAX_DTS - tsmx->last_PCR + 1;
			pr_debug("Initial PCR=0x%010llx shift=0x%010llx\n",
				tsmx->last_PCR, tsmx->shift_PCR);
		}
	}

	return;
}

static void move_tsg_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *buffer)
{
	struct te_tsg_filter *tsg = buffer->parent;

	list_move_tail(&buffer->lh, &tsg->tsg_mme_buffer_list);
	buffer->in_mme = true;
	tsg->num_buffers++;
	tsmx->total_buffers++;
	tsmx->memory_used += buffer->length;

	return;
}

static void delete_tsg_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *buffer)
{
	struct te_tsg_filter *tsg = buffer->parent;

	if (buffer->in_mme) {
		tsg->num_buffers--;
		if (tsg->num_buffers <
				(MAX_BUF_PER_STREAM - tsg->input_buffers)) {
			tsg->queued_buf = false;
			pr_debug("stream, prg %d %d has space now\n",
					tsg->stream_id, tsg->program_id);
		}
		tsmx->total_buffers--;
		tsmx->memory_used -= buffer->length;
		/*
		 * One of queued buffer will be release here
		 * We have to parse through all buffer list
		 * and release first one which is queued
		 */
		{
			struct te_tsg_buffer *tsg_buffer, *tmp_buffer;
			/* Use safe ver as delete_buffer deletes the entry */
			list_for_each_entry_safe(tsg_buffer, tmp_buffer,
				&tsg->tsg_mme_buffer_list, lh) {
				if (tsg_buffer->buffer_queued == true) {
					te_tsmux_mme_release_buffer(tsmx,
							tsg_buffer , false);
					tsg_buffer->buffer_queued = false;
					break;
				}
			}
		}
	} else if (tsg->num_buffers ==
			(MAX_BUF_PER_STREAM - tsg->input_buffers)) {
				tsg->queued_buf = false;
				pr_debug("stream, prg %d %d has space now\n",
					tsg->stream_id, tsg->program_id);
	}

	pr_debug("Deleting tsg_buffer %p, Stream %d:%d now has %d buffers in mme, mux total memory=%d\n",
		buffer, tsg->program_id, tsg->stream_id, tsg->num_buffers,
		tsmx->memory_used);
	list_del(&buffer->lh);
	kfree(buffer->block_list);
	kfree(buffer);

	return;
}

static int send_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *tsg_buffer)
{
	int err = 0;
	struct te_tsg_filter *tsg = tsg_buffer->parent;

	/* Add pes header if necessary */
	if (!tsg->stream_is_pes)
		add_pes_header(tsg_buffer);

	/* Send buffer to MME layer */
	pr_debug("Sending buffer %p\n",
			tsg_buffer->block_list[0].data_addr);
	err = send_buffer_to_mme(tsmx, tsg_buffer);
	if (err)
		goto error;

	/* Now we can update the last_DTS value */
	tsg->last_DTS = tsg_buffer->dts;
	goto out;

error:
	delete_tsg_buffer(tsmx, tsg_buffer);
out:
	return err;
}

int te_tsmux_mme_tsg_pause(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg)
{
	int err = 0;
	struct te_tsg_buffer *tsg_buffer;

	if (tsg->state == TE_TSG_STATE_PAUSED)
		goto out;
	if (!(tsg->state & (TE_TSG_STATE_STARTED | TE_TSG_STATE_RUNNING))) {
		pr_err("Attempt to pause a tsg which is not started or running\n");
		err = -EINVAL;
		goto out;
	}
	pr_debug("Pausing stream %d:%d\n", tsg->program_id, tsg->stream_id);
	pr_debug("PCR, DTS, First DTS and number_buffers 0x%010llx 0x%010llx 0x%010llx %d\n",
		tsmx->last_PCR, tsg->last_DTS, tsg->first_DTS,
		tsg->num_buffers);
	/* Send a null buffer into the mme to pause the stream */
	err = make_tsg_buffer(tsg, NULL, 0, &tsg_buffer);
	if (err) {
		pr_err("Couldn't create new null buffer for tsg %p\n", tsg);
		goto out;
	}
	list_add_tail(&tsg_buffer->lh, &tsg->tsg_buffer_list);

	tsg_buffer->dts = 0ULL;
	tsg_buffer->is_pause = true;

	pr_debug("Sending pause buffer\n");
	err = send_buffer_to_mme(tsmx, tsg_buffer);
	if (err)
		goto error;
	tsg->state = TE_TSG_STATE_PAUSED;
	/* Now wake up the tsmux in case it is waiting for this tsg */
	wake_up_interruptible(&tsmx->ready_wq);

error:
	/* NOTE: We always delete the buffer because it is no longer needed */
	delete_tsg_buffer(tsmx, tsg_buffer);
out:
	return err;
}

static bool do_pause(struct te_tsmux *tsmx, struct te_tsg_filter *tsg)
{
	bool result = false;

	pr_debug("Checking if we should pause stream %d:%d\n",
		tsg->program_id, tsg->stream_id);
	/* If for section pause is nothing, so skip for section */
	if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
		return result;
	/* If we have not received a frame yet, pause it */
	if (TimeUnset(tsg->first_DTS)) {
		pr_debug("Stream %d:%d no initial frame, should be paused\n",
				tsg->program_id, tsg->stream_id);
		result = true;
	} else {
		if (TimeIsBefore(tsg->last_DTS, tsmx->last_PCR)) {
			pr_debug("Stream %d:%d behind PCR point, should be paused",
					tsg->program_id, tsg->stream_id);
			result = true;
		}
		if (!check_enough_data(tsmx, tsg)) {
			pr_debug("Stream %d:%d not ready, should be paused\n",
					tsg->program_id, tsg->stream_id);
			result = true;
		}
	}

	return result;
}

static void run_auto_pause(struct te_tsmux *tsmx)
{
	struct te_tsg_filter *tsg;
	/* Go through each stream checking if it is ready or not */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (!(tsg->state & (TE_TSG_STATE_STARTED |
				TE_TSG_STATE_RUNNING)))
			continue;
		/* If this stream is preventing us from muxing, pause it */
		if (do_pause(tsmx, tsg))
			te_tsmux_mme_tsg_pause(tsmx, tsg);
	}

}

static bool check_auto_pause(struct te_tsmux *tsmx)
{
	bool do_auto_pause = false;
	struct te_tsg_filter *tsg;
	pr_debug("Checking if we should run auto-pause\n");
	/* If we are using more than 90% of memory do auto-pause */
	if (tsmx->memory_used >= ((tsmx->max_memory * 90)/100)) {
		pr_debug("Auto-pausing: using too much memory\n");
		do_auto_pause = true;
		goto out;
	}
	/* Check each stream to see if it has data from beyond the dont-wait
	 * limit, if so then we need to run the auto-pause
	 */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (jiffies > (tsmx->last_DTS_time_stamp
						+ MAX_STREAM_WAIT_TIME))
				tsg->dont_pause_other = false;

		if (tsg->state != TE_TSG_STATE_RUNNING)
			continue;
		if (TimeUnset(tsg->last_DTS))
			continue;
		/* We need to have data to be able to pause based
		 * on this stream */
		if (tsg->num_buffers == 0)
			continue;
		/* Check to make sure the last DTS is not behind the current PCR
		 * point */
		if (TimeIsBefore(tsg->last_DTS, tsmx->last_PCR))
			continue;
		/* Do not used sections to calculate auto pause */
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;

		/* Don't run auto-pause if non-audio/video streams are ahead*/
		if (!stream_type_is_video(tsg->stream_type) &&
				!stream_type_is_audio(tsg->stream_type))
			continue;

		if (tsg->dont_pause_other)
			continue;
		/* If we haven't yet run the muxer, then the check the last DTS
		 * value against the first DTS on this stream rather than the
		 * last PCR, as the last PCR may include a large time to fill
		 * the decoder buffers and therefore it could trigger the auto-
		 * pause too soon after starting.
		 */
		if (tsmx->state == TE_TSMUX_STATE_STARTED) {
			/* Check the time of the last
			 * buffer's DTS against the first DTS on this stream
			 * plus the dontwaitlimit to see if we have gone
			 * beyond this time
			 */
			if (!InTimePeriod(tsg->last_DTS, tsg->first_DTS,
					PcrLimit(tsg->first_DTS +
							tsg->dontwaitlimit))) {
				pr_debug("Auto-pausing: stream %d:%d beyond time limit, lastDTS=0x%010llx, firstDTS=0x%010llx, limit=0x%010llx\n",
						tsg->program_id,
						tsg->stream_id,
						tsg->last_DTS, tsg->first_DTS,
						PcrLimit(tsg->first_DTS +
							tsg->dontwaitlimit));
				do_auto_pause = true;
				goto out;
			} else if (jiffies >
						(tsmx->last_DTS_time_stamp
						+ MAX_STREAM_WAIT_TIME)) {
					do_auto_pause = true;
					goto out;
			}
		} else {
			/* Check the time of the last
			 * buffer's DTS against the last PCR plus the
			 * dontwaitlimit to see if we have gone beyond
			 * this time
			 */
			if (!InTimePeriod(tsg->last_DTS, tsmx->last_PCR,
					PcrLimit(tsmx->last_PCR +
							tsg->dontwaitlimit))) {
				pr_debug("Auto-pausing: stream %d:%d beyond time limit, lastDTS=0x%010llx, lastPCR=0x%010llx, limit=0x%010llx\n",
						tsg->program_id,
						tsg->stream_id,
						tsg->last_DTS, tsmx->last_PCR,
						PcrLimit(tsmx->last_PCR +
							tsg->dontwaitlimit));
				do_auto_pause = true;
				goto out;
			}
		}
	}

out:
	return do_auto_pause;
}

static void auto_set_dont_pause_other(struct te_tsmux *tsmx,
		struct te_tsg_filter *cur_tsg)
{
	struct te_tsg_filter *tsg;
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->state != TE_TSG_STATE_RUNNING)
			continue;
		if (TimeUnset(tsg->last_DTS))
			continue;

		/* Do not used sections to calculate dont_pause_other */
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (tsg->stream_type == cur_tsg->stream_type)
			continue;
		/* Don't set dont_pause_other if stream is non A/V */
		if (!stream_type_is_audio_video(tsg->stream_type))
			continue;

		tsg->dont_pause_other = true;
	}
}

static void auto_reset_dont_pause_other(struct te_tsmux *tsmx,
		struct te_tsg_filter *cur_tsg)
{
	struct te_tsg_filter *tsg;
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->state != TE_TSG_STATE_RUNNING)
			continue;
		if (TimeUnset(tsg->last_DTS))
			continue;

		/* Do not used sections to calculate dont_pause_other */
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (tsg->stream_type == cur_tsg->stream_type)
			continue;
		/* Don't reset dont_pause_other if stream is non A/V */
		if (!stream_type_is_audio_video(tsg->stream_type))
			continue;
		if (!tsg->ignore_stream_auto_pause)
			tsg->dont_pause_other = false;
	}
}

static int send_event(struct te_tsmux *tsmx,
		struct te_tsg_buffer *tsg_buffer,
		void *prv_data, int event_id)
{
	int ret = 0;

	stm_event_t event;
	event.event_id = event_id;
	switch (event_id) {
	case STM_TE_TSMUX_TSG_EOS_EVENT:
		event.object = prv_data;
		ret = stm_event_signal(&event);
		if (ret)
			pr_err("EOS event could not send obj = %p err = %d\n",
					prv_data, ret);
		break;

	default:
		break;
	}

	return ret;
}
int te_tsmux_mme_send_buffer(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg,
		struct stm_se_compressed_frame_metadata_s *metadata,
		struct stm_data_block *block_list,
		uint32_t block_count)
{
	int err = 0;
	struct te_tsg_buffer *tsg_buffer;
	bool tsmux_restart_done = false;
	bool enough_data = false;

	/* If the tsg is not started (i.e. tsmux was started) or running then
	 * we can't accept any data */
	/* Mutex lock is not required during restart of tsmux after FTD
	 * for section. Calling thread is already holding the same lock. */
	if (!(tsmx->restarting && (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER))) {
		if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
			err = -EINTR;
			goto out_nounlock;
		}
	}

	tsg->total_received_frames++;

	if (meta_is_eos(metadata)) {
		tsg->eos_detected = true;
		pr_debug("Stream = %u EOS detected\n", tsg->stream_type);
		err = send_event(tsmx, tsg_buffer, (void *)tsg ,
						STM_TE_TSMUX_TSG_EOS_EVENT);
		if (err)
			pr_err("Couldn't raise event\n");
	}
	if (!(tsg->state & (TE_TSG_STATE_STARTED | TE_TSG_STATE_RUNNING
				| TE_TSG_STATE_PAUSED))) {

		pr_err(
				"Sending data to an tsg which is not started, running or paused\n");
		err = -EINVAL;
		goto out;
	 }
	if (tsmx->state &
		(TE_TSMUX_STATE_FLUSHING | TE_TSMUX_STATE_FLUSHED)) {
		pr_warn("Tsmux is flushing, ignoring buffer\n");
		err = -EINVAL;
		goto out;
	}
	
	/* The max buffers represents the number of buffer structures available
	 * inside the multiplexor. But these buffer structures are used by both
	 * the input stream buffers and the output transform buffer. So the
	 * check for max buffers needs to be made against a value at least 1
	 * smaller than the maximum. We choose 5 less just to be safe */
	if (tsmx->total_buffers >= (tsmx->max_buffers - 5)) {
		pr_err("Tsmux using too many buffers, ignoring buffer -2\n");
		err = -EINVAL;
		goto out;
	}

	if (tsg->num_buffers >= MAX_BUF_PER_STREAM) {
		pr_err("Stream exceeds buffers limit\n");
		err = -EINVAL;
		goto out;
	}

	if (tsg->stream_man_paused) {
		pr_err("Buffer not accepted-Manual Pause active\n");
		err = -EINVAL;
		goto out;
	}

	if (TimeUnset(tsmx->last_PCR) &&
			tsmx->restart_complete) {
		if (metadata->discontinuity !=
			STM_SE_DISCONTINUITY_DISCONTINUOUS)
			metadata->discontinuity =
					STM_SE_DISCONTINUITY_DISCONTINUOUS;
	}

	err = make_tsg_buffer(tsg, block_list, block_count, &tsg_buffer);
	if (err) {
		pr_err("Couldn't create new buffer for tsg %p\n", tsg);
		goto out;
	}
	list_add_tail(&tsg_buffer->lh, &tsg->tsg_buffer_list);

	if (TimeUnset(tsg->first_DTS) && tsmx->restart_complete)
		tsg_buffer->ts_discontinuity_indicator = true;

	process_metadata(tsg_buffer, metadata);

	/* End of stream mechanism */
	if (stream_type_is_audio_video(tsg->stream_type)) {

		if (tsg_buffer->eos) {
			if (tsg_buffer->length != 0) {
				pr_warn("End of stream buffer has\
						data which will be ignored\n");
				tsg_buffer->length = 0;
			}
			tsg->state = TE_TSG_STATE_FLUSHING;
			if (check_all_tsg_flushing(tsmx)) {
				pr_debug("All tsg now flushing, set tsmux to flush\n");
				tsmx->state = TE_TSMUX_STATE_FLUSHING;
			}
		}
	}

	mask_index_flags(tsg_buffer);

	/* Get the DTS value and store on buffer */
	get_dts(tsmx, tsg_buffer, metadata);

	/* If we haven't received a buffer on this stream before then
	 * set the state to running and initialise the timestamps
	 */
	if (TimeUnset(tsg->first_DTS)) {
		tsg->first_DTS = tsg_buffer->dts;
		tsg->last_DTS = tsg_buffer->dts;
		/* Check this first DTS against the others */
		if (tsg->ignore_first_dts_check == 0)
			check_first_dts(tsmx, tsg);
		/* If the initial dts check failed and we stopped this
		 * stream, then set error and return.
		 */
		if (tsg->state == TE_TSG_STATE_STOPPED) {
			err = -EINVAL;
			goto out;
		}
		/* This was the first buffer sent on this tsg so change
		 * the state to running */
		tsg->state = TE_TSG_STATE_RUNNING;
		/* Update the initial PCR if necessary */
		update_initial_pcr(tsmx, tsg);
		pr_debug("Stream Type = %u  first DTS = %llx",
							tsg->stream_type,
							tsg->first_DTS);
		if (tsg->ignore_stream_auto_pause)
			tsg->dont_pause_other = true;
		else
			tsg->dont_pause_other = false;
	}

	/* Check DTS */
	err = check_dts(tsmx, tsg_buffer);
	/* FTD will not be considered if any of A/V stream */
	/* is in flushing state.*/
	if (check_audio_video_tsg_flushing(tsmx))
		tsmux_restart_done = check_tsmux_restart(tsmx,
					tsg_buffer, metadata, err);
	if (err || tsmux_restart_done) {
		if (tsmux_restart_done) {
			err = restart_tsmux(tsmx, tsg);
			if (err)
				goto out;
			else
				pr_info("TSMUX restarted successfully\n");
		}
		if (err)
			/* If error related to check DTS */
			delete_tsg_buffer(tsmx, tsg_buffer);
		err = -EINVAL;
		goto out;
	}

	/* If the stream is paused, un-pause it */
	if (tsg->state ==  TE_TSG_STATE_PAUSED) {
		pr_debug("Unpausing stream %d:%d\n", tsg->program_id,
				tsg->stream_id);
		tsg->state = TE_TSG_STATE_RUNNING;
	}

	if (tsg->full_discont_marker && (!tsg_buffer->eos)) {
		pr_err("FTD on Stream=%d:%d LastDTS=0x%010llx ThisDTS=0x%010llx\n",
				tsg->program_id, tsg->stream_id,
				tsg->last_DTS, tsg_buffer->dts);
		err = -EINVAL;
		goto out;
	}

	/* Send the buffer to the muxer */
	err = send_buffer(tsmx, tsg_buffer);
	if (err)
		goto out;
	else
		tsg->total_accepted_frames++;

	tsmx->last_DTS_time_stamp = jiffies;
	enough_data = check_enough_data(tsmx, tsg);
	if (enough_data || (tsmx->state == TE_TSMUX_STATE_FLUSHING)
			|| tsg->eos_detected) {
		if (tsmx->pull_intf.notify && tsmux_ready(tsmx)) {
			err = tsmx->pull_intf.notify(tsmx->external_connection,
					STM_MEMSINK_EVENT_DATA_AVAILABLE);
			if (err)
				pr_err("DATA_AVAILABLE event not subscribed properly\n");
			pr_debug("Data event sent\n");
		}
	}
	/* Check if we have enough data for next PCR period and wake up the
	 * tsmux get_data in case it is waiting on us.
	 * We also wake up the tsmux if this tsg is in the flushing or paused
	 * states, as in this case we don't wait for more data */
	if (tsg->state & (TE_TSG_STATE_FLUSHING | TE_TSG_STATE_PAUSED) ||
				enough_data) {
		pr_debug("***Tsg %d:%d now ready waking mux\n",
				tsg->program_id, tsg->stream_id);
		/* Wake up the tsmux in case it is waiting for this tsg */
		wake_up_interruptible(&tsmx->ready_wq);
	}

out:
	/* To avoid  unlock during tsmux restart after FTD for section.
	 * Unlocking will be done in restart function. */
	if (!(tsmx->restarting && (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)))
		mutex_unlock(&tsmx->obj.lock);
out_nounlock:
	return err;
}
EXPORT_SYMBOL(te_tsmux_mme_send_buffer);

int te_tsmux_mme_get_data(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err  = 0;
/*	MME_ERROR Error = MME_SUCCESS; */

	pr_debug("Getting data with block_count %d addr %p\n", block_count,
			block_list[0].data_addr);
	if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
		err = -EINTR;
		goto error_nounlock;
	}
	*filled_blocks = 0;
	if (tsmx->state == TE_TSMUX_STATE_STOPPED) {
		pr_err("Request for data on stopped tsmux\n");
		err = -EINVAL;
		goto error;
	}
	if (tsmx->state == TE_TSMUX_STATE_FLUSHED) {
		pr_debug("Flushing is done, returning 0 data\n");
		err = 0;
		goto error;
	}
	/* Check supplied buffer is large enough */
	if (!check_mux_buffer_size(tsmx, block_list, block_count)) {
		pr_err("Supplied mux buffer is too small, should be %d bytes\n",
				tsmx->buffer_size);
		err = -EINVAL;
		goto error;
	}
	pr_debug("***Get data starting wait\n");
	mutex_unlock(&tsmx->obj.lock);
	/* Wait for enough queued input data (pcr period + extra) or
	 * until we get the flushing flag set.
	 * We timeout if we don't receive any data in 200 milli-seconds as this
	 * probably means we have run out of v4l2 buffers before being ready
	 * or someone has killed the sending process, so we need to return
	 * to allow the v4l2 to stop its kernel thread cleanly */
	err = wait_event_interruptible_timeout(tsmx->ready_wq,
			tsmux_ready_locked(tsmx), msecs_to_jiffies(200));
	if (err < 0) {
		pr_err("Interrupted while waiting for tsmux to be ready\n");
		err = -EINTR;
		goto error_nounlock;
	} else if (err == 0) {
		pr_debug("Timeout while waiting for tsmux to be ready\n");
		err = -EAGAIN;
		goto error_nounlock;
	}
	/* If the tsmux was stopped while waiting, then return an error */
	if (tsmx->state == TE_TSMUX_STATE_STOPPED) {
		pr_warn("Tsmux stopped while waiting for data\n");
		err = -EINTR;
		goto error;
	}
	pr_debug("***Get data exited wait\n");
	/* Lock the tsmux so no more buffers can be sent until we have completed
	 * the transform */
	/* Issue a transform */
	err = do_transform(tsmx, block_list, block_count, filled_blocks);
	if (err < 0) {
		pr_err("Couldn't do transform, err=%d\n", err);
		/* mutex_unlock(&tsmx->obj.lock);*/
		goto error;
	}
	/* Check if we were flushing and have now finished */
	if (tsmx->state == TE_TSMUX_STATE_FLUSHED)
		pr_debug("Flushing and flush is done\n");

error:
	mutex_unlock(&tsmx->obj.lock);
error_nounlock:
	return err;
}
EXPORT_SYMBOL(te_tsmux_mme_get_data);

int te_tsmux_mme_testfordata(struct te_tsmux *tsmx, uint32_t *size)
{
	int err = 0;

	if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
		err = -EINTR;
		goto error;
	}
	*size = 0;
	/* If the tsmux is not in states: STARTED, RUNNING, FLUSHING, FLUSHED
	 * or STOPPED, then it is an error */
	if (tsmx->state < TE_TSMUX_STATE_STARTED) {
		pr_err("Testing for data on tsmux in invalid state\n");
		err = -EINVAL;
		goto error_unlock;
	}

	pr_debug("Testfordata ready check\n");
	if (tsmux_ready(tsmx))
		*size = tsmx->buffer_size;

error_unlock:
	mutex_unlock(&tsmx->obj.lock);
error:
	return err;
}
EXPORT_SYMBOL(te_tsmux_mme_testfordata);

static int te_tsmux_mme_release_buffer(struct te_tsmux *tsmx,
	struct te_tsg_buffer *buffer, bool queue_in_tsg)
{
	int err = 0;
	struct te_tsg_filter *tsg = buffer->parent;
	if (buffer->buffer_queued == false ||
		(queue_in_tsg == false && buffer->buffer_queued == true)) {
		if (buffer->block_count) {
			pr_debug("Releasing tsg_buffer %p, with buffer %p, count %d\n",
				buffer,
				buffer->block_list[0].data_addr,
				buffer->block_count);

			if (tsg->async_src_if) {
				err = tsg->async_src_if->release_buffer(
					tsg->external_connection,
					buffer->block_list,
					buffer->block_count);
				if (err) {
					pr_err("Couldn't release buffer %p, addr %p, err %d\n",
						buffer,
						buffer->block_list[0].data_addr,
						err);
					goto out;
				}
			}
		} else {
			pr_debug("Releasing null pause buffer\n");
		}
	}

out:
	if (queue_in_tsg == true)
		move_tsg_buffer(tsmx, buffer);

	return err;
}

static bool tsmux_ready(struct te_tsmux *tsmx)
{
	struct te_tsg_filter *tsg;
	bool ready = true;
	bool do_auto_pause = false;
	bool all_paused = true;
	pr_debug("Ready check start\n");
	/* If the tsmux is flushing or stopped then we are automatically
	 * ready */
	if (tsmx->state & (TE_TSMUX_STATE_FLUSHING | TE_TSMUX_STATE_FLUSHED |
			TE_TSMUX_STATE_STOPPED)) {
		ready = true;
		goto out;
	}
	/* We are not ready if we have not received any buffer yet i.e. we
	 * have not yet updated the last_PCR
	 */
	if (TimeUnset(tsmx->last_PCR)) {
		ready = false;
		goto out;
	}
	/* check if all streams are pausing then we are not ready */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (tsg->scattered_stream)
			continue;
		if (tsg->state != TE_TSG_STATE_PAUSED) {
			all_paused = false;
			break;
		}
	}
	/* This is error condition and this should not happen */
	if (all_paused == true) {
		pr_err("all streams are paused .. so not ready\n");
		ready = false;
		goto out;
	}
	/* Check if we need to run auto-pause */
	if (check_auto_pause(tsmx)) {
		pr_debug("Running auto-pause\n");
		run_auto_pause(tsmx);
		do_auto_pause = true;
	} else {
	/* run auto pause on subt stream to stop burst */
		list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
			if (tsg->scattered_stream) {
				if (TimeUnset(tsg->first_DTS)) {
					te_tsmux_mme_tsg_pause(tsmx, tsg);
					do_auto_pause = true;
				} else if (TimeIsBefore(tsg->last_DTS,
							tsmx->last_PCR)) {
					te_tsmux_mme_tsg_pause(tsmx, tsg);
					do_auto_pause = true;
				} else if ((TimeIsBefore(tsmx->last_PCR,
							tsg->last_DTS)) &&
					(tsg->num_buffers == 0)) {
					te_tsmux_mme_tsg_pause(tsmx, tsg);
					do_auto_pause = true;
				}
			}

		}
	}
	/* Check for manual pause */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if(tsg->stream_man_paused) {
			do_auto_pause = true;
			te_tsmux_mme_tsg_pause(tsmx, tsg);
		}
	}

	ready = true;
	/* Now check each tsg filter to see if they are all ready */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		pr_debug("Ready checking stream %d:%d state=%d\n",
				tsg->program_id, tsg->stream_id, tsg->state);
		/* A section tsg stream is always ready */
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER) {
			pr_debug("Stream is section data, so ready\n");
			continue;
		}
		switch (tsg->state) {
		case TE_TSG_STATE_DISCONNECTED:
		case TE_TSG_STATE_FLUSHING:
			pr_debug("Stream is disconnected or flushing, so ready\n");
			break;
		case TE_TSG_STATE_PAUSED:
		case TE_TSG_STATE_RUNNING:
			if (tsg->state == TE_TSG_STATE_PAUSED &&
					do_auto_pause) {
				pr_debug("Stream is paused and auto-pause is on, so ready\n");
			} else {
				if ((tsg->num_buffers == 0)) {
					pr_debug("No buffers, so not ready!\n");
					ready = false;
				}
				if (check_enough_data(tsmx, tsg)) {
					pr_debug("Stream has enough data, so ready\n");

				} else {
					pr_debug("Stream does not have enough data, so not ready\n");
					ready = false;
				}
			}
			break;
		case TE_TSG_STATE_CONNECTED:
		case TE_TSG_STATE_STARTED:
			pr_debug("Stream is not running, not ready\n");
			ready = false;
			break;
		default:
			pr_debug("Stream not in defined state, considered ready\n");
			break;
		}
		if (ready == false)
			break;
	}
	/* If we are now ready to run for the first time, then set
	 * the tsmux state to running
	 */
	if (ready && tsmx->state == TE_TSMUX_STATE_STARTED)
		tsmx->state = TE_TSMUX_STATE_RUNNING;
	/* Check that if we ran the auto-pause, we are now ready to run
	 * the muxer, if this is not the case there is something wrong
	 */
	if (!ready && do_auto_pause)
		pr_debug("Not ready to run mux after auto-pausing\n");

out:
	pr_debug("Ready check end, ready=%s\n", (ready ? "YES" : "NO"));
	return ready;
}

static bool tsmux_ready_locked(struct te_tsmux *tsmx)
{
	bool ready = false;

	if (mutex_trylock(&tsmx->obj.lock) == 0) {
		ready = false;
		goto out;
	}
	ready = tsmux_ready(tsmx);

	if (!ready)
		mutex_unlock(&tsmx->obj.lock);
out:
	return ready;
}

static bool check_mux_buffer_size(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count)
{
	int length = 0;
	int count = block_count;
	bool good = false;

	if (count) {
		while (count--) {
			length += block_list->len;
			block_list++;
		}
	}
	if (length >= tsmx->buffer_size)
		good = true;
	pr_debug("Mux buffer size %d, required is %d\n",
			length, tsmx->buffer_size);
	return good;
}

static bool check_all_tsg_flushing(struct te_tsmux *tsmx)
{
	bool flushing = true;
	struct te_tsg_filter *tsg;

	/* Check if all tsg filters are now flushing and mark the
	 * tsmux as flushing if this is the case
	 */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		/* The tsmux can be considered to be flushing if
		 * A/V streams are either stopped, flushing or paused
		 */
		if (stream_type_is_audio_video(tsg->stream_type)) {
			if (!(tsg->state & (TE_TSG_STATE_FLUSHING |
							TE_TSG_STATE_STOPPED |
							TE_TSG_STATE_PAUSED)))
						flushing = false;

		} else {
			tsg->state = TE_TSG_STATE_FLUSHING;
		}

	}

	/* Set all section filters to flushing, if all 'normal' streams are. */
	if (flushing) {
		list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
			if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
				tsg->state = TE_TSG_STATE_FLUSHING;
		}
	}

	return flushing;
}

static bool check_audio_video_tsg_flushing(struct te_tsmux *tsmx)
{
	bool flushing = true;
	struct te_tsg_filter *tsg;

	/* Check flushing state of audio and video */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER)
			continue;
		if (!stream_type_is_audio_video(tsg->stream_type))
			continue;
		if (tsg->eos_detected)
			flushing = false;
	}

	return flushing;
}

static int make_tsg_buffer(struct te_tsg_filter *tsg,
	struct stm_data_block *block_list,
	uint32_t block_count,
	struct te_tsg_buffer **tsg_buffer)
{
	struct te_tsg_buffer *new_tsg_buffer;
	struct stm_data_block *new_block_list;
	uint32_t i;
	uint32_t length = 0;
	struct stm_data_block *ref_block;

	new_tsg_buffer = kzalloc(sizeof(*new_tsg_buffer), GFP_KERNEL);
	if (!new_tsg_buffer) {
		pr_err("No memory for new tsg_buffer\n");
		return -ENOMEM;
	}
	if (block_count) {
		new_block_list = kzalloc((sizeof(*new_block_list) *
				block_count), GFP_KERNEL);
		if (!new_block_list) {
			pr_err("No memory for tsg_buffer block list\n");
			kfree(new_tsg_buffer);
			return -ENOMEM;
		}
		ref_block = block_list;
		for (i = 0; i < block_count; i++) {
			new_block_list[i].data_addr = ref_block->data_addr;
			new_block_list[i].len = ref_block->len;
			new_block_list[i].next = &(new_block_list[i+1]);
			length += ref_block->len;
			ref_block++;
		}
		new_block_list[i-1].next = NULL;
		new_tsg_buffer->block_list = new_block_list;
	} else {
		new_tsg_buffer->block_list = NULL;
	}
	new_tsg_buffer->block_count = block_count;
	new_tsg_buffer->length = length;
	new_tsg_buffer->parent = tsg;
	new_tsg_buffer->pes_header_size = 0;
	new_tsg_buffer->in_mme = false;
	/*
	 * Check if this buffer needs to queued
	 * or we can release it
	 * In case we have few queued element
	 * Then put this buffer at queue because
	 * This may cause OOB Out of order buffer
	 */
	if ((tsg->num_buffers >= (MAX_BUF_PER_STREAM - tsg->input_buffers)) ||
		tsg->queued_buf == true) {
		new_tsg_buffer->buffer_queued = true;
		tsg->queued_buf = true;
	}

	*tsg_buffer = new_tsg_buffer;
	pr_debug("created TSG buffer %p on stream %d:%d queued %d\n",
		*tsg_buffer, tsg->program_id,
		tsg->stream_id, new_tsg_buffer->buffer_queued);

	return 0;
}

static void delete_tsg_buffers(struct te_tsmux *tsmx,
	struct te_tsg_filter *tsg)
{
	struct te_tsg_buffer *tsg_buffer, *tmp_buffer;

	/* Use safe version because delete_buffer deletes the entry */
	list_for_each_entry_safe(tsg_buffer, tmp_buffer,
			&tsg->tsg_mme_buffer_list, lh) {
		delete_tsg_buffer(tsmx, tsg_buffer);
	}
}

static void delete_all_tsg_buffers(struct te_tsmux *tsmx)
{
	struct te_tsg_filter *tsg;

	pr_debug("Deleting all tsg buffers\n");

	/*
	 * Only delete normal filters, SEC filters will be taken care
	 * off by MME call back
	 */
	list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
		if (tsg->obj.type != TE_OBJ_TSG_SEC_FILTER)
			delete_tsg_buffers(tsmx, tsg);
	}
	return;
}

static bool check_enough_data(struct te_tsmux *tsmx, struct te_tsg_filter *tsg)
{
	bool Result;
	uint64_t ReadyPoint;

	Result = true;

	/* We determine if there is enough data available for this stream by
	 * checking if the latest DTS value received is ahead of the current
	 * mux PCR value by at least 2 PCR periods + the mux ahead limit.
	 * This is a fudge which should eventually be replaced by a more
	 * accurate calculation based on the current decoder bit buffer level
	 * and the PCR period i.e. we really need enough data for the next PCR
	 * period plus enough to take the bit buffer up to the desired level.
	 */
	ReadyPoint = PcrLimit(tsmx->last_PCR + tsmx->pcr_period +
			tsg->multiplexaheadlimit);
	pr_debug("Check enough data: latest DTS = 0x%010llx, last PCR = 0x%010llx, ready point = 0x%010llx\n",
			tsg->last_DTS, tsmx->last_PCR, ReadyPoint);
	if (InTimePeriod(tsg->last_DTS, tsmx->last_PCR, ReadyPoint) || tsg->num_buffers == 0) {
		pr_debug("Stream %d:%d not ready, num_buffers=%d\n",
			tsg->program_id, tsg->stream_id, tsg->num_buffers);
		Result = false;
	} else {
		pr_debug("STREAM %d:%d READY, num_buffers=%d\n",
			tsg->program_id, tsg->stream_id, tsg->num_buffers);
		Result = true;
	}
	return Result;
}

static MME_ERROR get_capability(void)
{
	MME_ERROR			Status;
	MME_TransformerCapability_t	MMECapability;

	pr_debug("    Capability test\n");

	memset(&Capability, 0x00, sizeof(TSMUX_TransformerCapability_t));
	memset(&MMECapability, 0x00, sizeof(MME_TransformerCapability_t));

	Capability.StructSize	= sizeof(TSMUX_TransformerCapability_t);

	MMECapability.StructSize	= sizeof(MME_TransformerCapability_t);
	MMECapability.TransformerInfoSize =
			sizeof(TSMUX_TransformerCapability_t);
	MMECapability.TransformerInfo_p	= &Capability;

	Status	= MME_GetTransformerCapability(TSMUX_MME_TRANSFORMER_NAME,
			&MMECapability);
	pr_debug("Correct   - Status = %d\n", Status);

	pr_debug("Fields\n");
	pr_debug("    ApiVersion                       = %d\n",
		Capability.ApiVersion);
	pr_debug("    MaximumNumberOfPrograms          = %d\n",
		Capability.MaximumNumberOfPrograms);
	pr_debug("    MaximumNumberOfStreams           = %d\n",
		Capability.MaximumNumberOfStreams);
	pr_debug("    MaximumNumberOfBuffersPerStream  = %d\n",
		Capability.MaximumNumberOfBuffersPerStream);
	pr_debug("\n");

	return Status;
}

static MME_ERROR open_transformer(struct te_tsmux *tsmux_p)
{
	MME_ERROR			Status = MME_SUCCESS;
	TSMUX_InitTransformerParam_t	Parameters;
	MME_TransformerInitParams_t	MMETransformInit;

	memset(&Parameters, 0x00, sizeof(TSMUX_InitTransformerParam_t));

	/* Set parameters based on control values set on the tsmux object */
	Parameters.StructSize		= sizeof(TSMUX_InitTransformerParam_t);
	Parameters.TimeStampedPackets	=
		((tsmux_p->output_type == STM_TE_OUTPUT_TYPE_TTS) ? 1 : 0);
	Parameters.PcrPeriod90KhzTicks		= tsmux_p->pcr_period;
	Parameters.GeneratePcrStream		= tsmux_p->pcr_stream;
	Parameters.PcrPid			= tsmux_p->pcr_pid;
	Parameters.TableGenerationFlags		=
		((tsmux_p->table_gen & STM_TE_TSMUX_CNTRL_TABLE_GEN_PAT_PMT) ?
			TSMUX_TABLE_GENERATION_PAT_PMT : 0) |
		((tsmux_p->table_gen & STM_TE_TSMUX_CNTRL_TABLE_GEN_SDT) ?
			TSMUX_TABLE_GENERATION_SDT : 0);
	Parameters.TablePeriod90KhzTicks	= tsmux_p->table_period;
	Parameters.TransportStreamId		= tsmux_p->ts_id;
	Parameters.FixedBitrate			= tsmux_p->bitrate_is_constant;
	Parameters.Bitrate			= tsmux_p->bitrate;
	Parameters.RecordIndexIdentifiers	= 1;
	Parameters.GlobalFlags = tsmux_p->global_flags;

	memset(&MMETransformInit, 0x00, sizeof(MME_TransformerInitParams_t));

	MMETransformInit.StructSize	= sizeof(MME_TransformerInitParams_t);
	MMETransformInit.Priority	= MME_PRIORITY_NORMAL;
	MMETransformInit.Callback	= MmeCallback;
	MMETransformInit.CallbackUserData	= (void *)tsmux_p;
	MMETransformInit.TransformerInitParamsSize =
			sizeof(TSMUX_InitTransformerParam_t);
	MMETransformInit.TransformerInitParams_p	= &Parameters;

	pr_debug("    Open Transformer\n");

	Status	= MME_InitTransformer(TSMUX_MME_TRANSFORMER_NAME,
			&MMETransformInit, &tsmux_p->transformer_handle);

	pr_debug("Status = %d\n", Status);
	pr_debug("    Handle   = %08x\n", tsmux_p->transformer_handle);

	return Status;
}

static MME_ERROR close_transformer(struct te_tsmux *tsmux_p)
{
	MME_ERROR Status = MME_SUCCESS;

	pr_debug("Close transformer\n");

	Status = MME_TermTransformer(tsmux_p->transformer_handle);

	pr_debug("Status = %d\n", Status);

	return Status;
}

static MME_ERROR add_program(struct te_tsmux *tsmux_p, bool tables_prog)
{
	MME_ERROR		Status = MME_SUCCESS;
	TSMUX_SetGlobalParams_t Parameters;
	TSMUX_AddProgram_t	*AddProgram;
	MME_Command_t		MMECommand;

	memset(&Parameters, 0x00, sizeof(TSMUX_SetGlobalParams_t));

	Parameters.StructSize	= sizeof(TSMUX_SetGlobalParams_t);
	Parameters.CommandCode	= TSMUX_COMMAND_ADD_PROGRAM;

	AddProgram		= &Parameters.CommandSubStructure.AddProgram;

	if (!tables_prog) {
		AddProgram->ProgramId	= tsmux_p->program_id;
		AddProgram->TablesProgram = FALSE;
		AddProgram->ProgramNumber = tsmux_p->program_number;
		AddProgram->PMTPid	= tsmux_p->pmt_pid;
		strncpy(AddProgram->ProviderName, tsmux_p->sdt_prov_name,
			TSMUX_MAXIMUM_PROGRAM_NAME_SIZE-1);
		strncpy(AddProgram->ServiceName, tsmux_p->sdt_serv_name,
			TSMUX_MAXIMUM_PROGRAM_NAME_SIZE-1);
		AddProgram->OptionalDescriptorSize = tsmux_p->pmt_descriptor_size;
		if (AddProgram->OptionalDescriptorSize)
			memcpy(AddProgram->Descriptor, tsmux_p->pmt_descriptor,
				tsmux_p->pmt_descriptor_size);
	} else {
		AddProgram->PMTPid	= tsmux_p->pmt_pid;
		AddProgram->ProgramId	= tsmux_p->program_id + 1;
		AddProgram->TablesProgram = TRUE;
		AddProgram->ProgramNumber = TSMUX_DEFAULT_UNSPECIFIED;
	}

	/* No support for scrambled streams yet */
	AddProgram->StreamsMayBeScrambled = 0;

	memset(&MMECommand, 0x00, sizeof(MME_Command_t));

	MMECommand.StructSize		= sizeof(MME_Command_t);
	MMECommand.CmdCode		= MME_SET_PARAMS;
	MMECommand.CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	MMECommand.NumberInputBuffers	= 0;
	MMECommand.NumberOutputBuffers	= 0;
	MMECommand.DataBuffers_p	= NULL;
	MMECommand.ParamSize		= sizeof(TSMUX_SetGlobalParams_t);
	MMECommand.Param_p		= &Parameters;

	pr_debug("    Add a program (%d)\n", AddProgram->ProgramId);

	Status	= MME_SendCommand(tsmux_p->transformer_handle, &MMECommand);
	pr_debug("Status = %d\n", Status);

	pr_debug("    ID = %d, State = %d (Completed = %d)\n",
		MMECommand.CmdStatus.CmdId, MMECommand.CmdStatus.State,
		MMECommand.CmdStatus.State == MME_COMMAND_COMPLETED);

	return Status;
}

static MME_ERROR add_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p)
{
	MME_ERROR			 Status = MME_SUCCESS;
	TSMUX_SetGlobalParams_t		 Parameters;
	TSMUX_AddStream_t		*AddStream;
	MME_Command_t			 MMECommand;

	memset(&Parameters, 0x00, sizeof(TSMUX_SetGlobalParams_t));

	Parameters.StructSize	= sizeof(TSMUX_SetGlobalParams_t);
	Parameters.CommandCode	= TSMUX_COMMAND_ADD_STREAM;

	AddStream		= &Parameters.CommandSubStructure.AddStream;
	AddStream->ProgramId	= tsg_p->program_id;
	AddStream->StreamId	= tsg_p->stream_id;
	AddStream->IncorporatePcr	= tsg_p->include_pcr;
	AddStream->StreamContent	= TSMUX_STREAM_CONTENT_PES;

	AddStream->StreamPid	= tsg_p->stream_pid;
	AddStream->StreamType	= tsg_p->stream_type;
	AddStream->OptionalDescriptorSize =
			tsg_p->stream_descriptor_size;
	if (AddStream->OptionalDescriptorSize)
		memcpy(AddStream->Descriptor, tsg_p->stream_descriptor,
				tsg_p->stream_descriptor_size);
	AddStream->DTSIntegrityThreshold90KhzTicks =
			tsg_p->dts_integrity_threshold;
	AddStream->DecoderBitBufferSize = tsg_p->bit_buffer_size;
	AddStream->MultiplexAheadLimit90KhzTicks = tsg_p->multiplexaheadlimit;

	memset(&MMECommand, 0x00, sizeof(MME_Command_t));

	MMECommand.StructSize		= sizeof(MME_Command_t);
	MMECommand.CmdCode		= MME_SET_PARAMS;
	MMECommand.CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	MMECommand.NumberInputBuffers	= 0;
	MMECommand.NumberOutputBuffers	= 0;
	MMECommand.DataBuffers_p	= NULL;
	MMECommand.ParamSize		= sizeof(TSMUX_SetGlobalParams_t);
	MMECommand.Param_p		= &Parameters;

	pr_debug("    Add a stream %d:%d\n", tsg_p->program_id,
		tsg_p->stream_id);

	Status	= MME_SendCommand(tsmux_p->transformer_handle, &MMECommand);

	pr_debug("Status = %d\n", Status);
	pr_debug("    ID = %d, State = %d (Completed = %d)\n",
		MMECommand.CmdStatus.CmdId, MMECommand.CmdStatus.State,
		MMECommand.CmdStatus.State == MME_COMMAND_COMPLETED);

	return Status;
}

static MME_ERROR add_sec_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p)
{
	MME_ERROR			 Status = MME_SUCCESS;
	TSMUX_SetGlobalParams_t		 Parameters;
	TSMUX_AddStream_t		*AddStream;
	MME_Command_t			 MMECommand;

	memset(&Parameters, 0x00, sizeof(TSMUX_SetGlobalParams_t));

	Parameters.StructSize	  = sizeof(TSMUX_SetGlobalParams_t);
	Parameters.CommandCode	  = TSMUX_COMMAND_ADD_STREAM;

	AddStream		  = &Parameters.CommandSubStructure.AddStream;
	AddStream->ProgramId	  = tsg_p->program_id;
	AddStream->StreamId	  = tsg_p->stream_id;
	AddStream->IncorporatePcr = 0;
	AddStream->StreamContent  = TSMUX_STREAM_CONTENT_SECTION;
	AddStream->StreamPid	  = tsg_p->stream_pid;

	memset(&MMECommand, 0x00, sizeof(MME_Command_t));

	MMECommand.StructSize		= sizeof(MME_Command_t);
	MMECommand.CmdCode		= MME_SET_PARAMS;
	MMECommand.CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	MMECommand.NumberInputBuffers	= 0;
	MMECommand.NumberOutputBuffers	= 0;
	MMECommand.DataBuffers_p	= NULL;
	MMECommand.ParamSize		= sizeof(TSMUX_SetGlobalParams_t);
	MMECommand.Param_p		= &Parameters;

	pr_debug("    Add a section stream %d:%d\n", tsg_p->program_id,
		tsg_p->stream_id);

	Status	= MME_SendCommand(tsmux_p->transformer_handle, &MMECommand);

	pr_debug("Status = %d\n", Status);
	pr_debug("    ID = %d, State = %d (Completed = %d)\n",
		MMECommand.CmdStatus.CmdId, MMECommand.CmdStatus.State,
		MMECommand.CmdStatus.State == MME_COMMAND_COMPLETED);

	return Status;
}

static MME_ERROR remove_stream(struct te_tsmux *tsmux_p,
		struct te_tsg_filter *tsg_p)
{
	MME_ERROR			 Status = MME_SUCCESS;
	TSMUX_SetGlobalParams_t		 Parameters;
	TSMUX_RemoveStream_t		*RemoveStream;
	MME_Command_t			 MMECommand;

	memset(&Parameters, 0x00, sizeof(TSMUX_SetGlobalParams_t));

	Parameters.StructSize	= sizeof(TSMUX_SetGlobalParams_t);
	Parameters.CommandCode	= TSMUX_COMMAND_REMOVE_STREAM;

	RemoveStream		= &Parameters.CommandSubStructure.RemoveStream;
	RemoveStream->ProgramId	= tsg_p->program_id;
	RemoveStream->StreamId	= tsg_p->stream_id;

	memset(&MMECommand, 0x00, sizeof(MME_Command_t));

	MMECommand.StructSize		= sizeof(MME_Command_t);
	MMECommand.CmdCode		= MME_SET_PARAMS;
	MMECommand.CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	MMECommand.NumberInputBuffers	= 0;
	MMECommand.NumberOutputBuffers	= 0;
	MMECommand.DataBuffers_p	= NULL;
	MMECommand.ParamSize		= sizeof(TSMUX_SetGlobalParams_t);
	MMECommand.Param_p		= &Parameters;

	pr_debug("    Remove a stream %d:%d\n", tsg_p->program_id,
		tsg_p->stream_id);

	Status	= MME_SendCommand(tsmux_p->transformer_handle, &MMECommand);

	pr_debug("Status = %d\n", Status);
	pr_debug("    ID = %d, State = %d (Completed = %d)\n",
		MMECommand.CmdStatus.CmdId, MMECommand.CmdStatus.State,
		MMECommand.CmdStatus.State == MME_COMMAND_COMPLETED);

	return Status;
}

static int setup_scatter_pages(struct te_tsg_buffer *tsg_buffer,
		struct te_send_buffer_params *params)
{
	int err = 0;
	MME_ScatterPage_t *scatter_pages;
	int num_pages, count, totalsize;
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	struct stm_data_block *block;
	unsigned int i;

	num_pages = tsg_buffer->block_count;
	/* Add an extra page for the PES header for ES only streams */
	/* Send the data if this is not paused and not EOF */
	if (!tsg->stream_is_pes && !tsg_buffer->is_pause && !tsg_buffer->eos)
		num_pages++;

	scatter_pages = kzalloc((sizeof(*scatter_pages) * num_pages),
			GFP_KERNEL);
	if (!scatter_pages) {
		err = -ENOMEM;
		kfree(params);
		goto out;
	}
	count = 0;
	totalsize = tsg_buffer->length;
	/* Add the extra page for the PES header for ES only streams */
	/* Send the data if this is not paused and not EOF */
	if (tsg->obj.type == TE_OBJ_TSG_FILTER
		 && !tsg->stream_is_pes && !tsg_buffer->is_pause &&
		!tsg_buffer->eos) {
		scatter_pages[0].Page_p = tsg_buffer->pes_header;
		scatter_pages[0].Size = tsg_buffer->pes_header_size;
		totalsize += tsg_buffer->pes_header_size;
		count++;
	}

	block = tsg_buffer->block_list;
	for (i = count; i < num_pages; i++) {
		scatter_pages[i].Page_p = block->data_addr;
		scatter_pages[i].Size = block->len;
		block++;
	}
	params->buffer.ScatterPages_p	= scatter_pages;
	params->buffer.NumberOfScatterPages	= num_pages;
	params->buffer.TotalSize		= totalsize;
out:
	return err;
}

static int send_buffer_to_mme(struct te_tsmux *tsmx,
	struct te_tsg_buffer *tsg_buffer)

{
	int err = 0;
	struct te_tsg_filter *tsg = tsg_buffer->parent;
	struct te_send_buffer_params *params;
	MME_Command_t *buffer_cmd;
	TSMUX_SendBuffersParam_t *send_params;
	MME_DataBuffer_t *buffer;
	MME_ERROR Status;
	uint32_t index_flags = tsg_buffer->index_flags;


	pr_debug("Send_buffer: DTS=0x%010llx\n", tsg_buffer->dts);

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params) {
		err = -ENOMEM;
		goto out;
	}

	send_params = &params->send_params;
	send_params->StructSize		= sizeof(TSMUX_SendBuffersParam_t);
	send_params->ProgramId		= tsg->program_id;
	send_params->StreamId		= tsg->stream_id;
	send_params->DTS		= tsg_buffer->dts;
	send_params->DTSDuration	= 1000;
	send_params->BufferFlags	= (tsg_buffer->length == 0) ?
			TSMUX_BUFFER_FLAG_NO_DATA : 0;
	if (!tsg->stream_is_pes && !tsg_buffer->is_pause &&
		!tsg_buffer->eos)
		send_params->BufferFlags  = 0;
	if (tsg_buffer->insert_dit) {
		send_params->BufferFlags		|=
				TSMUX_BUFFER_FLAG_REQUEST_DIT_INSERTION;
		send_params->DITTransitionFlagValue	= 0;
	}
	if (tsg_buffer->insert_rap)
		send_params->BufferFlags		|=
				TSMUX_BUFFER_FLAG_REQUEST_RAP_BIT;
	if (tsg_buffer->ts_discontinuity_indicator)
		send_params->BufferFlags		|=
				TSMUX_BUFFER_FLAG_DISCONTINUITY;

	send_params->IndexIdentifierSize	= 0;

	if (index_flags) {
		send_params->IndexIdentifierSize = 4;
		send_params->IndexIdentifier[0] =
				index_flags & 0xFF;
		send_params->IndexIdentifier[1] =
				(index_flags & 0xFF00)>>8;
		send_params->IndexIdentifier[2] =
				(index_flags & 0xFF0000)>>16;
		send_params->IndexIdentifier[3] =
				(index_flags & 0xFF000000)>>24;
		pr_debug("Inserting indexes 0x%08x on tsg %p stream %u\n",
			index_flags, tsg, tsg->stream_id);
	} else {
		send_params->IndexIdentifierSize = 0;
	}
	if (tsg->obj.type == TE_OBJ_TSG_SEC_FILTER && tsg_buffer->length != 0) {
		struct te_tsg_sec_filter *sec
			= container_of(tsg, struct te_tsg_sec_filter, tsg);

		pr_debug("TSG section filter repeat interval: %u msec\n",
				sec->rep_ivl);

		if (sec->rep_ivl > 0) {
			send_params->BufferFlags |= TSMUX_BUFFER_FLAG_REPEATING;
			send_params->RepeatInterval90KhzTicks = sec->rep_ivl * 90;
		}
	}
	/*CopyLongLongToByte( ftell(Input), send_params->IndexIdentifier );*/

	buffer_cmd = &params->buffer_cmd;
	buffer_cmd->StructSize		= sizeof(MME_Command_t);
	buffer_cmd->CmdCode		= MME_SEND_BUFFERS;
	buffer_cmd->CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	buffer_cmd->NumberInputBuffers	= 1;
	buffer_cmd->NumberOutputBuffers	= 0;
	buffer_cmd->DataBuffers_p	= &params->buffer_p;
	buffer_cmd->ParamSize		= sizeof(TSMUX_SendBuffersParam_t);
	buffer_cmd->Param_p		= &params->send_params;

	params->buffer_p		= &params->buffer;

	buffer = &params->buffer;
	buffer->StructSize		= sizeof(MME_DataBuffer_t);
	buffer->UserData_p		= (void *)tsg_buffer;
	buffer->Flags			= 0;
	buffer->StreamNumber		= 0;
	buffer->StartOffset		= 0;
	err = setup_scatter_pages(tsg_buffer, params);
	if (err)
		goto out;

	/* The pages are already filled in in this program */
	pr_debug("    Send a buffer %p\n", tsg_buffer);

	Status	= MME_SendCommand(tsmx->transformer_handle, buffer_cmd);
	if (Status != MME_SUCCESS && Status != MME_TRANSFORM_DEFERRED) {
		pr_err("MME send command error %d\n", Status);
		kfree(params->buffer.ScatterPages_p);
		kfree(params);
		err = -EPERM;
	}

out:
	return err;
}

void write_to_tsg_index(struct te_tsg_index_filter *idx,
		uint32_t indexes,
		uint32_t packet_offset,
		uint16_t pid,
		uint64_t pts,
		struct te_tsg_filter *tsg)
{
	int j;
	uint32_t bitmask = 0x00000001;

	for (j = 0; j < 32; j++) {
		if (indexes & bitmask) {
			idx->index_buffer_p[idx->write_pointer].index =	bitmask;
			if (bitmask & STM_TE_TSG_INDEX_DIT)
				idx->index_buffer_p[idx->write_pointer].pid =
					0x1e;
			else
				idx->index_buffer_p[idx->write_pointer].pid =
					pid;
			if (bitmask & STM_TE_TSG_INDEX_PTS && tsg) {
				idx->index_buffer_p[idx->write_pointer].pts =
					pts;
				idx->index_buffer_p[idx->write_pointer].native_pts =
					tsg->native_DTS;
			}
			pr_debug("[][%08X][%08X]",
				bitmask, packet_offset);
			if (tsg) {
				pr_debug("[%04X][%016llX][%08X][%08X]",
				idx->index_buffer_p[idx->write_pointer].pid
				,pts, tsg->stream_id_index,
				tsg->stream_type);
				idx->index_buffer_p[idx->write_pointer].stream_id =
				tsg->stream_id_index;
				idx->index_buffer_p[idx->write_pointer].stream_type
				= tsg->stream_type;
			}
			idx->index_buffer_p[idx->write_pointer++].packet_count =
							packet_offset;
			idx->write_pointer %= idx->buffer_size;
			if (idx->write_pointer == idx->read_pointer) {
				idx->read_pointer++;
				idx->read_pointer %= idx->buffer_size;
				idx->overflows++;
				pr_warn("TSG index filter overflow! %p", idx);
			}
			indexes &= ~bitmask;
		}

		bitmask = bitmask << 1;
		if (!indexes)
			break;
	}
}

static void output_index_table_records(
	struct te_obj *filter,
	TSMUX_CommandStatus_t *tsf)
{
	struct te_tsg_index_filter *idx;
	int i;
	idx = (struct te_tsg_index_filter *)filter;
	if (idx == NULL)
		return;

	if (mutex_lock_interruptible(&idx->obj.lock) != 0)
		return;
	for (i = 0; i < tsf->NumberOfIndexRecords; i++) {
		if (tsf->IndexRecords[i].Stream ==
				TSMUX_DEFAULT_UNSPECIFIED) {
			uint32_t indexes;
			pr_debug("[INDEX_DATA][%02X%02X%02X%02X][%08X][%016llX]",
			tsf->IndexRecords[i].IndexIdentifier[3],
			tsf->IndexRecords[i].IndexIdentifier[2],
			tsf->IndexRecords[i].IndexIdentifier[1],
			tsf->IndexRecords[i].IndexIdentifier[0],
			tsf->IndexRecords[i].PacketOffset,
			tsf->IndexRecords[i].pts);

			/* Add to internal index buffer for each bit */
			indexes = tsf->IndexRecords[i].IndexIdentifier[3]<<24;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[2]<<16;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[1]<<8;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[0];
			pr_debug("mask %x\n", idx->index_mask);
			indexes &= idx->index_mask;
			if (indexes &
				(STM_TE_TSG_INDEX_PAT |
				 STM_TE_TSG_INDEX_PMT |
				 STM_TE_TSG_INDEX_SDT)) {
				/*
				 * write only PAT, PMT and SDT indexes
				 */
				indexes = indexes & (STM_TE_TSG_INDEX_PAT |
						STM_TE_TSG_INDEX_PMT |
						STM_TE_TSG_INDEX_SDT);
				/*
				 * Since this is indexing at tsmux level
				 * So pid and pts are not
				 * valid. Pass 0 for Pid and PTS
				 */
				write_to_tsg_index(idx, indexes,
					tsf->IndexRecords[i].PacketOffset,
					0, 0, NULL);
			}
		}
	}
	if (tsf->NumberOfIndexRecords)
		wake_up_interruptible(&idx->reader_waitq);
	mutex_unlock(&idx->obj.lock);
}

static void output_index_records(struct te_tsg_filter *tsg,
	uint32_t stream_pid,
	struct te_obj *filter,
	TSMUX_CommandStatus_t *tsf)
{
	struct te_tsg_index_filter *idx;
	int i;
	uint32_t stream = tsg->stream_id;
	idx = (struct te_tsg_index_filter *)filter;
	if (idx == NULL)
		return;

	if (mutex_lock_interruptible(&idx->obj.lock) != 0)
		return;
	for (i = 0; i < tsf->NumberOfIndexRecords; i++) {
		if (tsf->IndexRecords[i].Stream == stream) {
			uint32_t indexes;
			pr_debug("[INDEX_DATA][%02X%02X%02X%02X][%08X][%u][%016llX]",
			tsf->IndexRecords[i].IndexIdentifier[3],
			tsf->IndexRecords[i].IndexIdentifier[2],
			tsf->IndexRecords[i].IndexIdentifier[1],
			tsf->IndexRecords[i].IndexIdentifier[0],
			tsf->IndexRecords[i].PacketOffset,
			tsf->IndexRecords[i].Stream,
			tsf->IndexRecords[i].pts);

			/* Add to internal index buffer for each bit */
			indexes = tsf->IndexRecords[i].IndexIdentifier[3]<<24;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[2]<<16;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[1]<<8;
			indexes |= tsf->IndexRecords[i].IndexIdentifier[0];

			write_to_tsg_index(idx, indexes,
				tsf->IndexRecords[i].PacketOffset,
				(stream_pid == TSMUX_DEFAULT_UNSPECIFIED)
				? (0x100 * tsf->IndexRecords[i].Program) +
					tsf->IndexRecords[i].Stream
				: stream_pid,
				tsf->IndexRecords[i].pts , tsg);
		} else {
			pr_debug("Index record discarded on stream %u, searching for tsg stream %u\n",
				tsf->IndexRecords[i].Stream,
				stream);
		}
	}
	if (tsf->NumberOfIndexRecords)
		wake_up_interruptible(&idx->reader_waitq);
	mutex_unlock(&idx->obj.lock);
}

static int do_transform(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err = 0;
	struct te_transform_params *params;
	MME_Command_t *trans_cmd;
	TSMUX_TransformParam_t *trans_params;
	MME_DataBuffer_t *buffer;
	MME_ScatterPage_t *scatter_pages;
	struct stm_data_block *block;
	uint32_t length = 0;
	uint32_t filled = 0;
	unsigned int i;
	MME_ERROR Status;
	struct te_tsg_filter *tsg;

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params) {
		err = -ENOMEM;
		goto out;
	}
	scatter_pages = kzalloc((sizeof(*scatter_pages) * block_count),
			GFP_KERNEL);
	if (!scatter_pages) {
		err = -ENOMEM;
		kfree(params);
		goto out;
	}
	block = block_list;
	for (i = 0; i < block_count; i++) {
		scatter_pages[i].Page_p = block->data_addr;
		scatter_pages[i].Size = block->len;
		scatter_pages[i].BytesUsed = 0;
		length += block->len;
		block++;
	}

	trans_params = &params->trans_params;
	trans_params->StructSize	= sizeof(TSMUX_TransformParam_t);
	if (tsmx->state == TE_TSMUX_STATE_FLUSHING)
		pr_debug("Tsmx flushing setting transform flush flag\n");
	trans_params->TransformFlags =
			(tsmx->state == TE_TSMUX_STATE_FLUSHING)
			? TSMUX_TRANSFORM_FLAG_FLUSH : 0;

	trans_cmd = &params->trans_cmd;
	trans_cmd->StructSize		= sizeof(MME_Command_t);
	trans_cmd->CmdCode		= MME_TRANSFORM;
	trans_cmd->CmdEnd		= MME_COMMAND_END_RETURN_NOTIFY;
	trans_cmd->NumberInputBuffers	= 0;
	trans_cmd->NumberOutputBuffers	= 1;
	trans_cmd->DataBuffers_p	= &params->buffer_p;
	trans_cmd->ParamSize		= sizeof(TSMUX_TransformParam_t);
	trans_cmd->Param_p		= &params->trans_params;

	trans_cmd->CmdStatus.AdditionalInfoSize	=
			sizeof(TSMUX_CommandStatus_t);
	trans_cmd->CmdStatus.AdditionalInfo_p	= &tsmx->transform_status;

	params->buffer_p			= &params->buffer;

	buffer = &params->buffer;
	buffer->StructSize	= sizeof(MME_DataBuffer_t);
	buffer->UserData_p	= (void *)(unsigned long long)0xdeadbeef;
	buffer->Flags		= 0;
	buffer->StreamNumber	= 0;
	buffer->NumberOfScatterPages	= block_count;
	buffer->ScatterPages_p	= scatter_pages;
	buffer->TotalSize	= length;
	buffer->StartOffset	= 0;

	pr_debug("    Sending a transform %d\n", i);

	Status = MME_SendCommand(tsmx->transformer_handle, trans_cmd);
	if (Status != MME_SUCCESS) {
		pr_err("Transform failed %d\n", Status);
		err = -EPERM;
		goto error;
	}

	/* Check the amount of data produced */
	pr_debug("Transform returned %d bytes\n",
		tsmx->transform_status.OutputSize);
	length = tsmx->transform_status.OutputSize;
	tsmx->total_ts_packets += tsmx->transform_status.TSPacketsOut;

	pr_debug("Displaying index info for this transform %d index records\n",
		tsmx->transform_status.NumberOfIndexRecords);

	/* Match the TSG filter and output indexes if there is an attached
	tsg index filter */
	if (tsmx->transform_status.NumberOfIndexRecords) {
		BOOL table_record = false;
		list_for_each_entry(tsg, &tsmx->tsg_filters, obj.lh) {
			pr_debug("Checking indexes %u on tsg %p\n",
				tsmx->transform_status.NumberOfIndexRecords,
				tsg);
			/*
			 * Need any tsg filter to get access of index
			 * filer
			 */
			if (table_record == false) {
				output_index_table_records(
					tsg->tsg_index_filters[0],
					&tsmx->transform_status);
				table_record = true;
			}
			output_index_records(tsg,
				tsg->stream_pid,
				tsg->tsg_index_filters[0],
				&tsmx->transform_status);
		}
	}

	/* Return value is either error code or number of bytes */
	err = length;
	block = block_list;
	while (length > 0) {
		if (length > block->len) {
			length -= block->len;
		} else {
			block->len = length;
			length = 0;
		}
		filled++;
		block++;
	}
	*filled_blocks = filled;

	pr_debug("There are %d filled blocks\n", *filled_blocks);
error:
	kfree(scatter_pages);
	kfree(params);
out:
	return err;
}

static void print_transform_status(TSMUX_CommandStatus_t *trans_status)
{
	int i;

	pr_debug("    OutputSize                  = %d\n",
			trans_status->OutputSize);
	pr_debug("    OffsetFromStart             = %lld\n",
			(uint64_t)trans_status->OffsetFromStart);
	pr_debug("    OutputDuration              = %d\n",
			trans_status->OutputDuration);
	pr_debug("    Bitrate                     = %d\n",
			trans_status->Bitrate);
	pr_debug("    PCR                         = %010llx\n",
			(uint64_t)trans_status->PCR);
	pr_debug("    TransformOutputFlags        = %08x\n",
			trans_status->TransformOutputFlags);
	/*pr_debug("    BBL    = %6d    %6d    %6d    %6d\n",
			trans_status->DecoderBitBufferLevels[0][0],
			trans_status->DecoderBitBufferLevels[0][1],
			trans_status->DecoderBitBufferLevels[0][2],
			trans_status->DecoderBitBufferLevels[0][3]);*/
	pr_debug("             %6d    %6d    %6d    %6d\n",
			trans_status->DecoderBitBufferLevels[1][0],
			trans_status->DecoderBitBufferLevels[1][1],
			trans_status->DecoderBitBufferLevels[1][2],
			trans_status->DecoderBitBufferLevels[1][3]);
	/*pr_debug("             %6d    %6d    %6d    %6d\n",
			trans_status->DecoderBitBufferLevels[2][0],
			trans_status->DecoderBitBufferLevels[2][1],
			trans_status->DecoderBitBufferLevels[2][2],
			trans_status->DecoderBitBufferLevels[2][3]);
	pr_debug("             %6d    %6d    %6d    %6d\n",
			trans_status->DecoderBitBufferLevels[3][0],
			trans_status->DecoderBitBufferLevels[3][1],
			trans_status->DecoderBitBufferLevels[3][2],
			trans_status->DecoderBitBufferLevels[3][3]);*/
	for (i = 0; i < trans_status->CompletedBufferCount; i++)
		pr_debug("Completed buffer %p\n",
				(void *)trans_status->CompletedBuffers[i]);
	pr_debug("    NumberOfIndexRecords        = %d\n",
			trans_status->NumberOfIndexRecords);
	return;
}

static void process_completed_buffers(struct te_tsmux *tsmx,
	unsigned int count,
	struct te_tsg_buffer **buffers)
{
	unsigned int i;

	if (count) {
		for (i = 0; i < count; i++) {
			if (!buffers[i])
				pr_err("Invalid tsg_buffer\n");
			delete_tsg_buffer(tsmx, buffers[i]);
		}
	}

}

static void MmeCallback(MME_Event_t event, MME_Command_t *callbackData,
	void *userData)
{
	struct te_tsmux *tsmx = (struct te_tsmux *)userData;
	/*unsigned int i;*/

	switch (callbackData->CmdCode)	{
	case MME_SET_PARAMS: {
		TSMUX_SetGlobalParams_t		*GlobalParams =
			(TSMUX_SetGlobalParams_t *)callbackData->Param_p;

		switch (GlobalParams->CommandCode) {
		case TSMUX_COMMAND_ADD_PROGRAM:
			pr_debug("Add program complete.\n");
			break;
		case TSMUX_COMMAND_REMOVE_PROGRAM:
			pr_debug("Remove program complete.\n");
			break;
		case TSMUX_COMMAND_ADD_STREAM:
			pr_debug("Add Stream complete.\n");
			break;
		case TSMUX_COMMAND_REMOVE_STREAM:
			pr_debug("Remove stream complete.\n");
			break;

		default:
			pr_debug("Invalid Command code on set params (%d)\n",
					GlobalParams->CommandCode);
		}
		break;
	}

	case MME_TRANSFORM: {
		TSMUX_CommandStatus_t *status = &tsmx->transform_status;
		TSMUX_Error_t error =
				(TSMUX_Error_t)callbackData->CmdStatus.Error;

		pr_debug("Transform complete %08x.\n",
				callbackData->CmdStatus.Error);
		if (error == TSMUX_NO_ERROR ||
				error == TSMUX_MULTIPLEX_DTS_VIOLATION) {
			tsmx->last_PCR	= ((uint64_t)(status->PCR +
					tsmx->pcr_period) & 0x1ffffffffULL);
			pr_debug("last_PCR: 0x%010llx\n", tsmx->last_PCR);
			print_transform_status(status);
		}
		/* Go through all completed buffers and delete them */
		process_completed_buffers(tsmx, status->CompletedBufferCount,
			(struct te_tsg_buffer **)&status->CompletedBuffers);
		/* If we were flushing and there is no more data to be
		 * consumed then mark the tsmux as flush done
		 */
		if ((tsmx->state == TE_TSMUX_STATE_FLUSHING) &&
			!(status->TransformOutputFlags &
				TSMUX_TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA)) {
			tsmx->state = TE_TSMUX_STATE_FLUSHED;
			/* We should now delete any remaining buffers in
			 * the tsg filters as we cannot now accept any more
			 */
			delete_all_tsg_buffers(tsmx);
		}


		break;
	}

	case MME_SEND_BUFFERS: {
		struct te_send_buffer_params *params =
				(struct te_send_buffer_params *)callbackData;
		struct te_tsg_buffer *tsg_buffer =
			(struct te_tsg_buffer *)
				callbackData->DataBuffers_p[0]->UserData_p;

		pr_debug("Send Buffers complete - Evt %d - Userdata %p.\n",
			event, tsg_buffer);
		/*
		 * Two conditions are possible here
		 * 1) We need to release this buffer as
		 * We have sufficient data space to queue next buffer
		 *
		 * 2) We don't have sufficient space to queue next buffers
		 * Therefore we just need to add this buffer to tsg_queue
		 * Buf we will not call release function for this buffer
		 */
		if ((callbackData->CmdStatus.Error & 0xFFFF) == MME_SUCCESS) {
			te_tsmux_mme_release_buffer(tsmx, tsg_buffer, true);
			kfree(params->buffer.ScatterPages_p);
			kfree(params);
		}
		break;
	}

	default:
		pr_debug("Invalid Command code (%d)\n",
				callbackData->CmdCode);
		break;
	}
	return;
}


