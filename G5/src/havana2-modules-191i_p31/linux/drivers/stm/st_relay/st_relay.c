#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

#include <asm/io.h>

#include "st_relay.h"

MODULE_LICENSE("GPL");

//EXPORT_SYMBOL(st_relayfs_open);
//EXPORT_SYMBOL(st_relayfs_close);
EXPORT_SYMBOL(st_relayfs_write);
EXPORT_SYMBOL(st_relayfs_getindex);
EXPORT_SYMBOL(st_relayfs_freeindex);

unsigned long g_flags;
struct rchan *	fatpipe_chan;
struct dentry *	strelay_dir;

//must correlate to relay_type_ids in st_relay.h
unsigned char relay_type_names[RELAY_NUMBER_OF_TYPES][ST_RELAY_TYPE_NAME_LEN] = {
	"DecodedForManifestorAudio",
	"DecodedForManifestorVideo",
	"CodedForFrameParserAudio",
	"CodedForFrameParserVideo",
	"PesForCollatorAudio",
	"PesForCollatorVideo",
	"DataToPCM0",
	"DataToPCM1",
	"DataToPCM2",
	"DataToPCM3",
	"DataToPCM4",
	"DataToPCM5",
	"DataToPCM6",
	"DataToPCM7",
	"MMELog",
	[ST_RELAY_TYPE_CRC] = "CRC",
    "LowLatencyInput0",
    "LowLatencyInput1",
};

relay_entry_t relay_entries[RELAY_NUMBER_OF_TYPES];

unsigned int relay_source_item_counts[RELAY_NUMBER_OF_SOURCES][RELAY_NUMBER_OF_TYPES];

unsigned int audio_manifestor_indexes[4] = { 0,0,0,0 };
unsigned int video_manifestor_indexes[4] = { 0,0,0,0 };

#define SUBBUF_SIZE	(768*1024)
#define N_SUBBUFS	10

struct rchan *st_relay_chan = NULL;
struct rchan *st_test_chan = NULL;
static DEFINE_SPINLOCK(st_chan_lock);

/*
 * file_create() callback.  Creates relay file in debugfs.
 */
static struct dentry *st_create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      int mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;
	buf_file = debugfs_create_file(filename, mode, parent, buf, &relay_file_operations);
	*is_global = 1;
	return buf_file;
}

/*
 * file_remove() default callback.  Removes relay file in debugfs.
 */
static int st_remove_buf_file_handler(struct dentry *dentry)
{
	printk("remove_buf_file_handler: dentry %p\n", dentry);
	debugfs_remove(dentry);
	return 0;
}

/*
 * relayfs callbacks
 */
static struct rchan_callbacks st_relayfs_callbacks =
{
	.create_buf_file = st_create_buf_file_handler,
	.remove_buf_file = st_remove_buf_file_handler,
};


//cloned this from kernels include/linux/relay.h to add length written return value
//and avoid changing it in the kernel tree itself
static inline int st_relay_write(struct rchan *chan,
			       const void *data,
			       size_t length)
{
	unsigned long flags;
	struct rchan_buf *buf;

	local_irq_save(flags);
	buf = chan->buf[smp_processor_id()];
	if (unlikely(buf->offset + length > chan->subbuf_size))
		length = relay_switch_subbuf(buf, length);
	memcpy(buf->data + buf->offset, data, length);
	buf->offset += length;
	local_irq_restore(flags);
	return length;
}

// id = entry type
// source = marker of what code sent data
void st_relayfs_write(unsigned int id, unsigned int source, unsigned char *buf, unsigned int len, void *info)
{
    unsigned long flags;

    //did we get to open our channel?
    if(!st_relay_chan) return;

    //check exists!
    if(id >= RELAY_NUMBER_OF_TYPES) {
	printk("st_relayfs_write: invalid id %d >= %d \n",id,RELAY_NUMBER_OF_TYPES);
    } else {
	//and is it active?
	if(relay_entries[id].active) {
		int wrote1,wrote2,wrote3;

		relay_entries[id].x      = 0;	//mini meta data
		relay_entries[id].y      = 0;
		relay_entries[id].z      = 0;
		relay_entries[id].ident  = 0x12345678;
		relay_entries[id].source = source;
		relay_entries[id].count  = relay_source_item_counts[source][id];
		relay_entries[id].len    = len;

		if(id == ST_RELAY_TYPE_DECODED_VIDEO_BUFFER) {
		//for these we need to hunt two separate parts, Luma and Chroma
		//TODO - we assume info->type is SURF_YCBCR420MB right now...

			struct relay_video_frame_info_s *vid_info = info;

			relay_entries[id].x      = vid_info->width;
			relay_entries[id].y      = vid_info->height;
			relay_entries[id].len    = (((relay_entries[id].x * relay_entries[id].y)*3)/2);

			spin_lock_irqsave(&st_chan_lock, flags);
			wrote1 = st_relay_write(st_relay_chan, relay_entries[id].name, ST_RELAY_ENTRY_HEADER_LEN);
			if(wrote1!=0) {

				wrote2 = st_relay_write(st_relay_chan, buf+(vid_info->luma_offset),
									(relay_entries[id].x * relay_entries[id].y));

				if(wrote2!=0) {

					wrote3 = st_relay_write(st_relay_chan, buf+(vid_info->chroma_offset), 
										((relay_entries[id].x * relay_entries[id].y)/2));

					if(wrote3!=0) {
						relay_source_item_counts[source][id]++;
					} else {
						wrote1 = wrote3-len;	//use wrote1 to signify error and how much failed to write
					}
				} else {
					wrote1 = wrote2-len;		//use wrote1 to signify error and how much failed to write
				}
			} else wrote1 -= ST_RELAY_ENTRY_HEADER_LEN;	//use wrote1 to signify error and how much failed to write
			spin_unlock_irqrestore(&st_chan_lock, flags);

		} else {
			spin_lock_irqsave(&st_chan_lock, flags);
			wrote1 = st_relay_write(st_relay_chan, relay_entries[id].name, ST_RELAY_ENTRY_HEADER_LEN);
			if(wrote1!=0) {
				wrote2 = st_relay_write(st_relay_chan, buf, len);
				if(wrote2!=0) {
					relay_source_item_counts[source][id]++;
				} else {
					wrote1 = wrote2-len;		//use wrote1 to signify error and how much failed to write
				}
			} else wrote1 -= ST_RELAY_ENTRY_HEADER_LEN;	//use wrote1 to signify error and how much failed to write
			spin_unlock_irqrestore(&st_chan_lock, flags);
		}

		if(wrote1<0) {
			printk("st_relayfs_write: DISABLING %s - client not reading data quick enough! (%d lost) \n",
									relay_entries[id].name, (0-wrote1));
			relay_entries[id].active = 0;
		}
	}
    }
}

unsigned int st_relayfs_getindex(unsigned int source)
{
	unsigned int n, index = 0;

	switch(source) {
		case ST_RELAY_SOURCE_AUDIO_MANIFESTOR:
			for(n=0;n<4;n++) {
				if(audio_manifestor_indexes[n]==0) {
					audio_manifestor_indexes[n]=1;
					index=n;
				}
			}
			break;
		case ST_RELAY_SOURCE_VIDEO_MANIFESTOR:
			for(n=0;n<4;n++) {
				if(video_manifestor_indexes[n]==0) {
					video_manifestor_indexes[n]=1;
					index=n;
				}
			}
			break;
	}
	return index;
}

void st_relayfs_freeindex(unsigned int source, unsigned int index)
{
	switch(source) {
		case ST_RELAY_SOURCE_AUDIO_MANIFESTOR:
			audio_manifestor_indexes[index]=0;
			break;
		case ST_RELAY_SOURCE_VIDEO_MANIFESTOR:
			video_manifestor_indexes[index]=0;
			break;
	}
}

int st_relayfs_open(void)
{
	int n,m;

	//clear and fill in entries table with names/default
	memset( &relay_entries, 0x00, sizeof(relay_entries) );
	for(n=0;n<RELAY_NUMBER_OF_TYPES;n++) {
		strcpy(relay_entries[n].name, relay_type_names[n]);
		relay_entries[n].active = 0;
	}

	//clear count table
	for(n=0;n<RELAY_NUMBER_OF_SOURCES;n++)
		for(m=0;m<RELAY_NUMBER_OF_TYPES;m++)
			relay_source_item_counts[n][m] = 0;

	//set up our debugfs dir and entries
	strelay_dir = debugfs_create_dir("st_relay", NULL);
	if (!strelay_dir) {
		printk("%s: ERROR: Couldn't create debugfs app directory.\n", __FUNCTION__);
		return 1;
	} else {
		st_relay_chan = relay_open("data", strelay_dir, SUBBUF_SIZE,
				  N_SUBBUFS, &st_relayfs_callbacks, 0);

		if(st_relay_chan) {
			for(n=0;n<RELAY_NUMBER_OF_TYPES;n++) {
				relay_entries[n].dentry = 
					debugfs_create_u32(relay_entries[n].name, 0666, strelay_dir, &(relay_entries[n].active));
			}
		} else {
			printk("st_relayfs_open: ERROR: Couldn't create relay channel.\n");
		}
	}
	return 0;
}

void st_relayfs_close(void)
{
	int n;
	for(n=0;n<RELAY_NUMBER_OF_TYPES;n++) {
		if(relay_entries[n].dentry) debugfs_remove(relay_entries[n].dentry);
	}
	if(strelay_dir) debugfs_remove(strelay_dir);

	if (st_relay_chan) relay_close(st_relay_chan);
	st_relay_chan = 0;
}

module_init(st_relayfs_open);
module_exit(st_relayfs_close);
