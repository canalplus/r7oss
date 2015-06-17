#ifndef _ST_RELAY_H_
#define _ST_RELAY_H_

#ifdef __cplusplus
extern "C" {
#endif

//must correlate to relay source translations in st_relay.c
//these used in main player code to id source of data dumped
enum relay_source_ids
{
	PCMPLAYER_COMMIT_MAPPED_SAMPLES,

	ST_RELAY_SOURCE_AUDIO_MANIFESTOR,	//leave room for multiple manifestors
	ST_RELAY_SOURCE_AUDIO_MANIFESTOR2,
	ST_RELAY_SOURCE_AUDIO_MANIFESTOR3,
	ST_RELAY_SOURCE_AUDIO_MANIFESTOR4,

	ST_RELAY_SOURCE_VIDEO_MANIFESTOR,	//leave room for multiple manifestors
	ST_RELAY_SOURCE_VIDEO_MANIFESTOR2,
	ST_RELAY_SOURCE_VIDEO_MANIFESTOR3,
	ST_RELAY_SOURCE_VIDEO_MANIFESTOR4,

	ST_RELAY_SOURCE_AUDIO_COLLATOR,
	ST_RELAY_SOURCE_VIDEO_COLLATOR,
	
	ST_RELAY_SOURCE_AUDIO_FRAME_PARSER,
	ST_RELAY_SOURCE_VIDEO_FRAME_PARSER,

	ST_RELAY_SOURCE_MME_LOG,
	ST_RELAY_SOURCE_LOW_LATENCY,

//don't do anything below here
	RELAY_NUMBER_OF_SOURCES
};

//must correlate to relay type translations in st_relay.c
//these used in main player code to id type of data dumped
enum relay_type_ids
{
	ST_RELAY_TYPE_DECODED_AUDIO_BUFFER,
	ST_RELAY_TYPE_DECODED_VIDEO_BUFFER,

	ST_RELAY_TYPE_CODED_AUDIO_BUFFER,
	ST_RELAY_TYPE_CODED_VIDEO_BUFFER,
	
	ST_RELAY_TYPE_PES_AUDIO_BUFFER,
	ST_RELAY_TYPE_PES_VIDEO_BUFFER,

	ST_RELAY_TYPE_DATA_TO_PCM0,
	ST_RELAY_TYPE_DATA_TO_PCM1,
	ST_RELAY_TYPE_DATA_TO_PCM2,
	ST_RELAY_TYPE_DATA_TO_PCM3,
	ST_RELAY_TYPE_DATA_TO_PCM4,
	ST_RELAY_TYPE_DATA_TO_PCM5,
	ST_RELAY_TYPE_DATA_TO_PCM6,
	ST_RELAY_TYPE_DATA_TO_PCM7,
        
	ST_RELAY_TYPE_MME_LOG,

	ST_RELAY_TYPE_CRC,
	ST_RELAY_TYPE_LOW_LATENCY_INPUT0,
	ST_RELAY_TYPE_LOW_LATENCY_INPUT1,

//don't do anything below here
	RELAY_NUMBER_OF_TYPES
};

//header can be 64 bytes total
#define ST_RELAY_ENTRY_HEADER_LEN 64
//name length reserves 28 bytes for rest of header items below
#define ST_RELAY_TYPE_NAME_LEN (64-28)

struct relay_entry_s {
//these first items will be writen as a buffer header
	unsigned char name[ST_RELAY_TYPE_NAME_LEN];	//set from st_relay_types.h

	unsigned int x;		//slots for any metadata required by 'st_relayfs_parse'
	unsigned int y;
	unsigned int z;

	unsigned int ident;     //set to 0x12345678
	unsigned int source;
	unsigned int count;
	unsigned int len;
//below here for internal use only
	unsigned int active;	//controlled via debugfs 
	struct dentry *dentry;
};
typedef struct relay_entry_s relay_entry_t;

struct relay_video_frame_info_s {	//extra info we need from video manifestor
	unsigned int width;
	unsigned int height;
	unsigned int type;
	unsigned int luma_offset;
	unsigned int chroma_offset;
};

#ifdef CONFIG_RELAY
int st_relayfs_open(void);
void st_relayfs_write(unsigned int id, unsigned int source, unsigned char *buf, unsigned int len, void *info);

unsigned int st_relayfs_getindex(unsigned int source);

void st_relayfs_freeindex(unsigned int source, unsigned int index);

#else

static inline int st_relayfs_open(void) {return 0;}
static inline void st_relayfs_write(unsigned int id, unsigned int source,
                                    unsigned char *buf, unsigned int len,
				    void *info) {}

static inline unsigned int st_relayfs_getindex(unsigned int source) { return 0; }
static inline void st_relayfs_freeindex(unsigned int source, unsigned int index) {}

#endif

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAY_H_ */

