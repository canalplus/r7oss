
/*
 *  section for direct support of synthesizer - /dev/ultra
 */

#define ULTRA_SYNTH_VERSION	0x00010000
	/* current version of protocol - 1.00; main mask is 0x7fff0000 */
	/* changes in low 8 bites -> protocols are backwards compatible */
	/* high 8. bites -> protocols are totaly uncompatible */

/* some typedef's */

typedef struct ULTRA_SYNTH_RESET ultra_synth_reset_t;
typedef struct ULTRA_SYNTH_INFO ultra_synth_info_t;
typedef struct ULTRA_SYNTH_STATE ultra_synth_state_t;
typedef struct ULTRA_SYNTH_ULTRACLICK ultra_synth_ultraclick_t;
typedef struct ULTRA_MEMORY_BLOCK ultra_memory_block_t;
typedef struct ULTRA_MEMORY_LIST ultra_memory_list_t;
typedef struct ULTRA_MEMORY_DUMP ultra_memory_dump_t;
typedef struct ULTRA_INSTRUMENT_WAVE ultra_synth_wave_t;
typedef struct ULTRA_INSTRUMENT_LAYER ultra_synth_layer_t;
typedef struct ULTRA_INSTRUMENT ultra_instrument_t;
typedef struct ULTRA_INSTRUMENT_INFO ultra_instrument_info_t;
typedef struct ULTRA_DSP_EFFECT ultra_dsp_effect_t;
typedef struct ULTRA_MIDI_DEVICES ultra_midi_devices_t;
typedef struct ULTRA_MIDI_DEVICE_INFO ultra_midi_device_info_t;
typedef struct ULTRA_MIDI_THRU ultra_midi_thru_t;

/* control */ 
#define ULTRA_SYNTH_IOCTL_VERSION 	_IOR ( 'U', 0x00, int )
#define ULTRA_SYNTH_IOCTL_RESET0  	_IOR ( 'U', 0x01, ultra_synth_reset_t )
#define ULTRA_SYNTH_IOCTL_RESET		_IOWR( 'U', 0x01, ultra_synth_reset_t )
#define ULTRA_SYNTH_IOCTL_INFO		_IOR ( 'U', 0x02, ultra_synth_info_t )
#define ULTRA_SYNTH_IOCTL_STATE		_IOR ( 'U', 0x03, ultra_synth_state_t )
/* setup */
#define ULTRA_SYNTH_GET_ULTRACLICK	_IOR ( 'U', 0x20, ultra_synth_ultraclick_t )
#define ULTRA_SYNTH_SET_ULTRACLICK	_IOW ( 'U', 0x20, ultra_synth_ultraclick_t )
#define ULTRA_SYNTH_GET_MIDI_EMUL	_IOR ( 'U', 0x21, int )
#define ULTRA_SYNTH_SET_MIDI_EMUL	_IOW ( 'U', 0x21, int )
#define ULTRA_SYNTH_EFFECT_RESET	_IO  ( 'U', 0x22 )
#define ULTRA_SYNTH_EFFECT_SETUP	_IOW ( 'U', 0x22, ultra_dsp_effect_t )
/* timer */
#define ULTRA_SYNTH_TIMER_START		_IO  ( 'U', 0x40 )
#define ULTRA_SYNTH_TIMER_STOP		_IO  ( 'U', 0x41 )
#define ULTRA_SYNTH_TIMER_CONTINUE	_IO  ( 'U', 0x42 )
#define ULTRA_SYNTH_TIMER_BASE		_IOW ( 'U', 0x43, int )
#define ULTRA_SYNTH_TIMER_TEMPO		_IOW ( 'U', 0x44, int )
#define ULTRA_SYNTH_TIMER_MASTER	_IO  ( 'U', 0x45 )
#define ULTRA_SYNTH_TIMER_SLAVE		_IOW ( 'U', 0x46, int )
/* memory */
#define ULTRA_SYNTH_MEMORY_RESET	_IOW ( 'U', 0x60, int )
#define ULTRA_SYNTH_MEMORY_TEST		_IOW ( 'U', 0x61, ultra_instrument_t )
#define ULTRA_SYNTH_MEMORY_ALLOC	_IOW ( 'U', 0x62, ultra_instrument_t )
#define ULTRA_SYNTH_MEMORY_FREE		_IOW ( 'U', 0x63, ultra_instrument_t )
#define ULTRA_SYNTH_MEMORY_PACK		_IO  ( 'U', 0x64 )
#define ULTRA_SYNTH_MEMORY_BALLOC	_IOW ( 'U', 0x65, ultra_memory_block_t )
#define ULTRA_SYNTH_MEMORY_BFREE	_IOW ( 'U', 0x66, ultra_memory_block_t )
#define ULTRA_SYNTH_MEMORY_INFO		_IOR ( 'U', 0x67, ultra_instrument_info_t )
#define ULTRA_SYNTH_MEMORY_LIST		_IOWR( 'U', 0x67, ultra_memory_list_t )
#define ULTRA_SYNTH_MEMORY_DUMP		_IOW ( 'U', 0x70, ultra_memory_dump_t )
/* command queue */
#define ULTRA_SYNTH_WQUEUE_SET_SIZE	_IOW ( 'U', 0x80, int )
#define ULTRA_SYNTH_WQUEUE_GET_SIZE 	_IOR ( 'U', 0x80, int )
#define ULTRA_SYNTH_WQUEUE_FREE		_IOR ( 'U', 0x81, int )
#define ULTRA_SYNTH_WQUEUE_THRESHOLD	_IOW ( 'U', 0x82, int )
#define ULTRA_SYNTH_RQUEUE_SET_SIZE 	_IOW ( 'U', 0x88, int )
#define ULTRA_SYNTH_RQUEUE_GET_SIZE 	_IOR ( 'U', 0x88, int )
#define ULTRA_SYNTH_RQUEUE_USED		_IOR ( 'U', 0x89, int )
#define ULTRA_SYNTH_FLUSH		_IO  ( 'U', 0x90 )
#define ULTRA_SYNTH_ABORT		_IO  ( 'U', 0x91 )
#define ULTRA_SYNTH_QABORT		_IO  ( 'U', 0x92 )
#define ULTRA_SYNTH_STOP		_IO  ( 'U', 0x93 )
#define ULTRA_SYNTH_CONTINUE		_IO  ( 'U', 0x94 )

/* MIDI emulation */

#define ULTRA_MIDI_EMUL_GM	0x0000
#define ULTRA_MIDI_EMUL_GS	0x0001
#define ULTRA_MIDI_EMUL_MT32	0x0002
#define ULTRA_MIDI_EMUL_AUTO	0x00ff

/*
 *  commands in read/write queues
 *
 *  format of queue (one record have 8 bytes):
 *
 *    0. byte		command		see ULTRA_CMD_XXXX constants
 *    1. byte		voice or channel #
 *    2.-7. byte	arguments 
 */

/*
 *  first level - simple voice commands for MOD players
 */

/* voice stop controllers */
#define ULTRA_STOP_NOW		0x00
#define ULTRA_STOP_LOOP		0x01
#define ULTRA_STOP_ENVELOPE	0x02
/* frequency controllers */
#define ULTRA_FREQ_HZ		0x00000000
#define ULTRA_FREQ_NOTE		0x40000000
#define ULTRA_FREQ_PITCHBEND	0x80000000
#define ULTRA_FREQ_OSSNOTE	0xc0000000
#define ULTRA_FREQ_MASK		0xc0000000
#define ULTRA_FREQ_DATA		(unsigned int)(~ULTRA_FREQ_MASK)
/* volume controllers */
#define ULTRA_VOLUME_LINEAR	0x0000
#define ULTRA_VOLUME_MIDI	0x4000
#define ULTRA_VOLUME_EXP	0x8000
#define ULTRA_VOLUME_MASK	0xc000
#define ULTRA_VOLUME_DATA	(unsigned short)(~ULTRA_VOLUME_MASK)
/* pan controllers */
#define ULTRA_PAN_LINEAR	0x0000
#define ULTRA_PAN_MASK		0xc000
#define ULTRA_PAN_DATA		(unsigned short)(~ULTRA_PAN_MASK)
/* lfo commands */
#define ULTRA_LFO_SETUP		0x00
#define ULTRA_LFO_FREQ		0x01
#define ULTRA_LFO_DEPTH		0x02
#define ULTRA_LFO_ENABLE	0x03
#define ULTRA_LFO_DISABLE	0x04
#define ULTRA_LFO_SHUTDOWN	0x05
/* private1 subcommands */
#define ULTRA_PRIV1_IW_EFFECT	0x00

#define ULTRA_CMD_VOICE_PROGRAM		0x00	/* change voice program */
#define ULTRA_CMD_VOICE_START		0x01	/* voice start */
#define ULTRA_CMD_VOICE_STOP		0x02	/* voice stop */
#define ULTRA_CMD_VOICE_CONTROL		0x03	/* voice control */
#define ULTRA_CMD_VOICE_FREQ		0x04	/* voice frequency */
#define ULTRA_CMD_VOICE_VOLUME		0x05	/* voice volume */
#define ULTRA_CMD_VOICE_SLOOP		0x06	/* voice loop start in bytes * 16 */
#define ULTRA_CMD_VOICE_ELOOP		0x07	/* voice loop end in bytes * 16 */
#define ULTRA_CMD_VOICE_RAMP		0x08	/* voice ramp range & control */
#define ULTRA_CMD_VOICE_POS		0x09	/* voice position in bytes * 16 (lowest 4 bits - fraction) */
#define ULTRA_CMD_VOICE_PAN		0x0a	/* voice pan (balance) */
#define ULTRA_CMD_VOICE_LFO		0x0b	/* voice lfo setup */
#define ULTRA_CMD_VOICE_PRIVATE1	0x20	/* voice private1 command */

/*
 *  second level - channel commands
 */

#define ULTRA_CMD_CHN_PROGRAM		0x40	/* change program */
#define ULTRA_CMD_CHN_NOTE_ON		0x41	/* note on */
#define ULTRA_CMD_CHN_NOTE_OFF		0x42	/* note off */
#define ULTRA_CMD_CHN_PITCHBEND		0x43	/* pitch bend change */
#define ULTRA_CMD_CHN_BALANCE		0x45	/* change balance */
#define ULTRA_CMD_CHN_CONTROL		0x46	/* channel control */
#define ULTRA_CMD_CHN_NOTE_PRESSURE	0x47	/* note pressure */
#define ULTRA_CMD_CHN_PRESSURE		0x48	/* channel pressure */

/*
 *  common level - timing/misc
 */

/* 0x80-0xff - second byte is extended argument, not voice */

#define ULTRA_CMD_WAIT			0x80	/* wait x us (uint) 1-(uint max) */
#define ULTRA_CMD_STOP			0x81	/* see ULTRA_IOCTL_ABORT ioctl */
#define ULTRA_CMD_TEMPO			0x82	/* set tempo */

/* 0xf0-0xff - input commands */

#define ULTRA_CMD_ECHO			0xf0	/* this will be send to file input - read() function */
#define ULTRA_CMD_VOICES_CHANGE		0xf1	/* active voices were changed */

/* structures */

struct ULTRA_SYNTH_RESET {
  unsigned int voices;	  	/* active voices */
  unsigned int channel_voices;	/* bitmap of voices used for channel commands (dynamicaly allocated) */
  unsigned char reserved[32];
};

#define ULTRA_SYNTH_INFO_F_PCM		0x00000001	/* GF1 PCM during SYNTH enabled */
#define ULTRA_SYNTH_INFO_F_ENHANCED	0x00000002	/* InterWave - enhanced mode */
#define ULTRA_SYNTH_INFO_F_SERVER	0x00000004	/* instrument server is present */

struct ULTRA_SYNTH_INFO {
  unsigned char id[ 32 ];	  /* id of synthesizer */
  unsigned char info[ 80 ];	  /* info about synthesizer */
  unsigned int version;		  /* see to ULTRA_CARD_VERSION_XXXX constants */

  unsigned int flags;		  /* some info flags - see to ULTRA_STRU_INFO_F_XXXX */

  unsigned int voices;		  /* max # of voices */
  struct {
    unsigned int size;		  /* size of this bank in bytes */
    unsigned int address;	  /* start address of this bank */
  } memory_banks[16];
  struct {
    unsigned int size;		  /* size of this bank in bytes */
    unsigned int address;	  /* start address of this bank */
  } memory_rom_banks[16];
  unsigned char reserved[128];	  /* all zeroes */
};

struct ULTRA_SYNTH_STATE {
  unsigned int mixing_freq;	  /* mixing frequency in Hz */

  unsigned int memory_size;	  /* in bytes */
  unsigned int memory_free;	  /* in bytes */
  unsigned int memory_block_8; 	  /* largest free 8-bit block in memory */
  unsigned int memory_block_16;	  /* largest free 16-bit block in memory */

  unsigned char reserved[128];	  /* reserved for future use */
};

struct ULTRA_STRU_ULTRACLICK {
  unsigned int smooth_pan: 1,     /* 1 = enable, 0 = disable */
  	       full_range_pan: 1, /* 1 = enable, 0 = disable */
  	       volume_ramp: 8;	  /* 0 = disable, 1-255 */

  unsigned char reserved[16];	  /* reserved for future use */
};

/*
 *  Abstract instrument interface
 */

/* ultra_instrument_t/ultra_instrument_layer_t/ultra_instrument_wave_t -> mode */

#define ULTRA_INSTR_FM			0x00000000	/* FM / OPL3 instrument */
#define ULTRA_INSTR_SIMPLE		0x00000001	/* simple format - for MOD players */
#define ULTRA_INSTR_PATCH		0x00000002	/* old GF1 patch format */
#define ULTRA_INSTR_INTERWAVE		0x00000003	/* new InterWave format */
#define ULTRA_INSTR_COUNT		4

/* ultra_instrument_t -> flags */

#define ULTRA_INSTR_F_NORMAL		0x0000	/* normal mode */
#define ULTRA_INSTR_F_NOT_FOUND		0x0001	/* instrument cann't be loaded */
#define ULTRA_INSTR_F_ALIAS		0x0002	/* alias */
#define ULTRA_INSTR_F_NOT_LOADED	0x00ff	/* instrument not loaded (not found) */

/* ultra_instrument_t -> exclusion */

#define ULTRA_INSTR_E_NONE		0x0000	/* exclusion mode - none */
#define ULTRA_INSTR_E_SINGLE		0x0001	/* exclude single - single note from instrument group */
#define ULTRA_INSTR_E_MULTIPLE		0x0002	/* exclude multiple - stop only same note from this instrument */

/* ultra_instrument_t -> layer */

#define ULTRA_INSTR_L_NONE		0x0000	/* not layered */
#define ULTRA_INSTR_L_ON		0x0001	/* layered */
#define ULTRA_INSTR_L_VELOCITY		0x0002	/* layered by velocity */
#define ULTRA_INSTR_L_FREQUENCY		0x0003	/* layered by frequency */

/* ultra_instrument_t - data.effects[] */

#define ULTRA_INSTR_IE_NONE		0
#define ULTRA_INSTR_IE_REVERB		1
#define ULTRA_INSTR_IE_CHORUS		2
#define ULTRA_INSTR_IE_ECHO		3

struct ULTRA_INSTRUMENT_WAVE {
  unsigned int share_id1;		/* share id (part 1) - 0 = no sharing enabled */
  unsigned int share_id2;		/* share id (part 2) - 0 = no sharing enabled */

  unsigned int mode;			/* see to ULTRA_INSTR_XXXX constants */
  unsigned int format;			/* depends on mode variable */

  struct {
    unsigned int number;		/* some other ID for this instrument */
    unsigned int memory;		/* begin of waveform in onboard memory */
    unsigned char *ptr;			/* pointer to waveform in system memory */
  } address;
  
  char reserved[16];  			/* reserved for future use */
  void *private;			/* private data for wave */
  struct ULTRA_STRU_WAVE *next;
};

struct ULTRA_INSTRUMENT_LAYER {
  unsigned int mode;			/* see to ULTRA_INSTR_XXXX constants */
  void *private_data;			/* private data */
  char reserved[16];			/* reserved for future use */
  struct ULTRA_STRU_WAVE *wave;
  struct ULTRA_STRU_LAYER *next;
};

struct ULTRA_INSTRUMENT {
  union {
    struct {
#ifdef ULTRA_LITTLE_ENDIAN
      unsigned short prog;
      unsigned short bank;
#else
      unsigned short bank;
      unsigned short prog;
#endif
    } midi;
    unsigned int instrument;		/* instrument number */
  } number;

  char *name;				/* name of this instrument or NULL */

  unsigned int mode;			/* see to ULTRA_INSTR_XXXX */
  unsigned int flags;			/* see to ULTRA_INSTR_F_XXXX */
  unsigned short exclusion;		/* see to ULTRA_INSTR_E_XXXX */
  unsigned short layer;			/* see to ULTRA_INSTR_L_XXXX */
  unsigned short exclusion_group;	/* 0 - none, 1-65535 */

  unsigned char effects[ 4 ];		/* use global effect if available */
  unsigned char effects_depth[ 4 ];	/* 0-127 */

  union {
    struct {
    } iw;
    struct {
      unsigned char effect1 : 4,	/* use global effect if available */
      		    effect2 : 4;	/* use global effect if available */
      unsigned char effect1_depth;	/* 0-127 */
      unsigned char effect2_depth;	/* 0-127 */
    } patch;
  } data;

  unsigned char reserved[ 32 ];		/* reserved for future use */

  union {
    struct ULTRA_STRU_LAYER *layer;	/* first layer */
    struct {
#ifdef ULTRA_LITTLE_ENDIAN
      unsigned short prog;
      unsigned short bank;
#else
      unsigned short bank;
      unsigned short prog;
#endif
    } midi_alias;
    unsigned int alias;			/* pointer to instrument */
  } info;

  void *private_data;			/* pointer to private data */		

  ultra_instrument_t *left;		/* used by driver or user application for search speedup */
  ultra_instrument_t *right;		/* used by driver or user application for search speedup */

  ultra_instrument_t *next;		/* next instrument */
};

/*
 *  FM instrument
 */
 
struct ULTRA_INSTRUMENT_WAVE_FM {
  union {
    unsigned char data[32];		/* register settings for operator cells (.SBI format) */
  } data;
};
 
/*
 *  Simple wave-based instrument
 */

/* bits for wave-based instruments for format variable in ultra_instrument_wave_t */

#define ULTRA_WAVE_16BIT		0x0001	/* 16-bit wave */
#define ULTRA_WAVE_UNSIGNED		0x0002	/* unsigned wave */
#define ULTRA_WAVE_INVERT		0x0002	/* same as unsigned wave */
#define ULTRA_WAVE_BACKWARD		0x0004	/* backward mode (maybe used for reverb or ping-ping loop) */
#define ULTRA_WAVE_LOOP			0x0008	/* loop mode */
#define ULTRA_WAVE_BIDIR		0x0010	/* bidirectional mode */
#define ULTRA_WAVE_ULAW			0x0020	/* uLaw compressed wave */
#define ULTRA_WAVE_RAM			0x0040	/* wave is _preloaded_ in RAM (it is used for ROM simulation) */
#define ULTRA_WAVE_ROM			0x0080	/* wave is in ROM */

struct ULTRA_INSTRUMENT_WAVE_SIMPLE {
  unsigned int size;			/* size of waveform in bytes */
  unsigned int start;			/* start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_start;		/* bits loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_end;		/* loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned short loop_repeat;		/* loop repeat - 0 = forever */
};

/*
 *  UltraSound GF1 patch instrument
 */

#define ULTRA_WAVE_PATCH_ENVELOPE	0x0001	/* envelopes on */
#define ULTRA_WAVE_PATCH_SUSTAIN	0x0002	/* sustain mode */

struct ULTRA_INSTRUMENT_WAVE_PATCH {
  unsigned int size;			/* size of waveform in bytes */
  unsigned int start;			/* start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_start;		/* bits loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_end;		/* loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned short loop_repeat;		/* loop repeat - 0 = forever */
  unsigned short flags;
  unsigned int sample_rate;
  unsigned int low_frequency;	/* low frequency range for this waveform */
  unsigned int high_frequency;	/* high frequency range for this waveform */
  unsigned int root_frequency;	/* root frequency for this waveform */
  signed short tune;
  unsigned char balance;
  unsigned char envelope_rate[ 6 ];
  unsigned char envelope_offset[ 6 ];
  unsigned char tremolo_sweep;
  unsigned char tremolo_rate;
  unsigned char tremolo_depth;
  unsigned char vibrato_sweep;
  unsigned char vibrato_rate;
  unsigned char vibrato_depth;
  unsigned short scale_frequency;
  unsigned short scale_factor;	/* 0-2048 or 0-2 */
};

/*
 *  InterWave instrument
 */

struct ULTRA_INSTRUMENT_WAVE_INTERWAVE {
  unsigned int size;			/* size of waveform in bytes */
  unsigned int start;			/* start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_start;		/* bits loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned int loop_end;		/* loop start offset in bytes * 16 (lowest 4 bits - fraction) */
  unsigned short loop_repeat;		/* loop repeat - 0 = forever */
  unsigned int sample_ratio;		/* sample ratio (44100 * 1024 / rate) */
  unsigned char attenuation;		/* 0 - 127 (no corresponding midi controller) */
  unsigned char low_note;		/* lower frequency range for this waveform */
  unsigned char high_note;		/* higher frequency range for this waveform */
};

#define ULTRA_IW_LFO_SHAPE_TRIANGLE	0
#define ULTRA_IW_LFO_SHAPE_POSTRIANGLE	1

struct ULTRA_IW_LFO {
  unsigned short freq;		/* (0-2047) 0.01Hz - 21.5Hz */
  signed short depth;		/* volume +- (0-255) 0.48675dB/step */
  signed short sweep;		/* 0 - 950 deciseconds */
  unsigned char shape;		/* see to ULTRA_IW_LFO_SHAPE_XXXX */
  unsigned char delay;		/* 0 - 255 deciseconds */
};

/* struct ULTRA_IW_ENV - flags */

#define ULTRA_IW_ENV_F_RETRIGGER	0x0001	/* flags - retrigger */

#define ULTRA_IW_ENV_M_ONE_SHOT		0x0001	/* mode - one shot */
#define ULTRA_IW_ENV_M_SUSTAIN		0x0002	/* mode - sustain */
#define ULTRA_IW_ENV_M_NO_SUSTAIN	0x0003	/* mode - no sustain */

#define ULTRA_IW_ENV_I_VELOCITY		0x0001	/* index - velocity */
#define ULTRA_IW_ENV_I_FREQUENCY	0x0002	/* index - frequency */

struct ULTRA_IW_ENV_RECORD {
  unsigned short nattack;
  unsigned short nrelease;
  unsigned short sustain_offset;
  unsigned short sustain_rate;
  unsigned short release_rate;
  unsigned char hirange;
  struct {
    unsigned short offset;
    unsigned short rate;
  } *points;			/* count of points = nattack + nrelease */
  struct ULTRA_IW_ENV_RECORD *next;
};

struct ULTRA_IW_ENV {
  unsigned char flags: 8,
  		mode: 4,
  		index: 4;
  struct ULTRA_IW_ENV_RECORD *record;
};

#define ULTRA_LAYER_IW_F_RETRIGGER	0x0001	/* retrigger */

#define ULTRA_LAYER_IW_V_VEL_TIME	0x0000	/* velocity mode = time */
#define ULTRA_LAYER_IW_V_VEL_RATE	0x0001	/* velocity mode = rate */

#define ULTRA_LAYER_IW_E_KUP		0x0000	/* layer event - key up */
#define ULTRA_LAYER_IW_E_KDOWN		0x0001	/* layer event - key down */
#define ULTRA_LAYER_IW_E_RETRIG		0x0002	/* layer event - retrigger */
#define ULTRA_LAYER_IW_E_LEGATO		0x0003	/* layer event - legato */

struct ULTRA_INSTRUMENT_LAYER_INTERWAVE {
  unsigned short flags: 8,		/* see to ULTRA_LAYER_IW_F_XXXX */
      		 velocity_mode: 4,	/* see to ULTRA_LAYER_IW_V_XXXX */
      		 layer_event: 4;	/* see to ULTRA_LAYER_IW_E_XXXX */
  unsigned char low_range;		/* range for layer based */
  unsigned char high_range;		/* on either velocity or frequency */
  unsigned char pan;			/* pan offset from CC1 (0 left - 127 right) */
  unsigned char pan_freq_scale;		/* position based on frequency (0-127) */
  struct ULTRA_IW_LFO tremolo;		/* tremolo effect */
  struct ULTRA_IW_LFO vibrato;		/* vibrato effect */
  unsigned char attenuation;		/* 0-127 (no corresponding midi controller) */
  unsigned short freq_scale;		/* 0-2048, 1024 is equal to semitone scaling */
  unsigned char freq_center;		/* center for keyboard frequency scaling */
  struct ULTRA_IW_ENV penv;		/* pitch envelope */
  struct ULTRA_IW_ENV venv;		/* volume envelope */
};

/*
 *
 */

struct ULTRA_INSTRUMENT_NAME {
  union {
    struct {
#ifdef ULTRA_LITTLE_ENDIAN
      unsigned short prog;
      unsigned short bank;
#else
      unsigned short bank;
      unsigned short prog;
#endif
    } midi;
    unsigned int instrument;		/* instrument number */
  } number;
  char name[ 64 ];			/* name of this instrument */
  unsigned int mode;			/* see to ULTRA_INSTR_XXXX */
  unsigned int flags;			/* see to ULTRA_INSTR_F_XXXX */
  unsigned char reserved[32];
};

/*
 *
 */

struct ULTRA_MEMORY_LIST {
  unsigned int count;			/* count of instruments */
  union {
    unsigned int *instruments;
    struct {
#ifdef ULTRA_LITTLE_ENDIAN
      unsigned short prog;
      unsigned short bank;
#else
      unsigned short bank;
      unsigned short prog;
#endif
    } *midi;
  } info;
  unsigned char reserved[64];
};

#define ULTRA_MEMORY_DUMP_RAM		0x0001
#define ULTRA_MEMORY_DUMP_ROM		0x0002
#define ULTRA_MEMORY_DUMP_WRITE		0x0004	/* if RAM - write block to RAM */
#define ULTRA_MEMORY_DUMP_INVERT	0x0008
#define ULTRA_MEMORY_DUMP_UNSIGNED	0x0008
#define ULTRA_MEMORY_DUMP_16BIT		0x0010	/* 16-bit wave */

struct ULTRA_MEMORY_DUMP {
  unsigned int flags;		  /* see to ULTRA_MEMORY_DUMP_XXXX constants */
  unsigned int address;		  /* linear address to GUS's memory */
  unsigned int size;		  /* size of transferred block */
  unsigned char *dump;		  /* pointer to destonation memory block */
  unsigned char reserved[16];	  /* reserved for future use */
};

#define ULTRA_MEMORY_BLOCK_LOCKED	0x0001	/* lock memory (don't free this by close()) */
#define ULTRA_MEMORY_BLOCK_16BIT	0x0002	/* for old GF1 chip (256k boundaries) */
#define ULTRA_MEMORY_BLOCK_ALREADY 0x0004	/* block is already allocated */

struct ULTRA_MEMORY_BLOCK {
  unsigned short flags;	          /* see to ULTRA_MEMORY_BLOCK_XXXX constants */
  unsigned int address;		  /* linear address in GUS's memory - if alloc - filled by driver */
  unsigned int size;		  /* size of this block */
  unsigned char id[ 256 ];	  /* identification (for lock) */
};

/* this structure is for InterWave only */

#define ULTRA_EFFECT_CHIP_INTERWAVE	0x0001

#define ULTRA_EFFECT_F_IW_USED		0x0001	/* is this voice used? */
#define ULTRA_EFFECT_F_IW_ALTERNATE	0x0002	/* enable alternate path for this effect processor */

struct ULTRA_EFFECT_INTERWAVE {
  unsigned short flags;			/* see to ULTRA_EFFECT_F_IW_???? */
  unsigned short number;		/* effect number */

  unsigned int buffer_addr; 		/* used by driver */
  unsigned int buffer_size; 		/* write buffer size */
  unsigned int buffer_pos;  		/* initial write position in buffer */
  unsigned short volume;		/* volume - 0-4095 */
  unsigned short effect_volume; 	/* effect volume - 0-4095 */
  unsigned short l_offset;  		/* left offset - 0-4095 */
  unsigned short r_offset;  		/* right offset - 0-4095 */

  unsigned short vibrato_control;	/* control bits + freq */
  unsigned short vibrato_depth;		/* 0-8191 */
  unsigned short tremolo_control;	/* control bits + freq */
  unsigned short tremolo_depth;		/* 0-8191 */

  unsigned char write_mask;		/* write mask for accumulators */
};

struct ULTRA_STRU_EFFECT {
  unsigned short chip_type;	  	/* see to ULTRA_EFFECT_CHIP_???? */
  union {
    struct {
      unsigned char voice_output[ 8 ];	/* voice accumulator bitmap */
      /* eight effects processors should be enough */
      struct ULTRA_EFFECT_INTERWAVE voices[ 8 ];
    } interwave;
  } chip;
  unsigned char reserved[1024];
};

/*
 *
 *  MIDI interface
 *
 */

#define ULTRA_MIDI_DEV_COUNT		14	/* max midi devices per card */
#define ULTRA_MIDI_DEV_COMMON		0xff	/* common (control) device */

#define ULTRA_MCMD_NOTE_OFF             0x80
#define ULTRA_MCMD_NOTE_ON              0x90
#define ULTRA_MCMD_NOTE_PRESSURE        0xa0
#define ULTRA_MCMD_CONTROL              0xb0
#define ULTRA_MCMD_PGM_CHANGE           0xc0
#define ULTRA_MCMD_CHANNEL_PRESSURE     0xd0
#define ULTRA_MCMD_BENDER               0xe0

#define ULTRA_MCMD_COMMON_SYSEX         0xf0
#define ULTRA_MCMD_COMMON_MTC_QUARTER	0xf1
#define ULTRA_MCMD_COMMON_SONG_POS	0xf2
#define ULTRA_MCMD_COMMON_SONG_SELECT	0xf3
#define ULTRA_MCMD_COMMON_TUNE_REQUEST	0xf6
#define ULTRA_MCMD_COMMON_SYSEX_END     0xf7
#define ULTRA_MCMD_COMMON_CLOCK		0xf8	/* ignored - not passed to/from MIDI output/input queue */
#define ULTRA_MCMD_COMMON_START		0xfa
#define ULTRA_MCMD_COMMON_CONTINUE	0xfb
#define ULTRA_MCMD_COMMON_STOP		0xfc
#define ULTRA_MCMD_COMMON_SENSING	0xfe	/* ignored - not passed to/from MIDI output/input queue */
#define ULTRA_MCMD_COMMON_RESET		0xff	/* ignored - not passed to/from MIDI output/input queue */

#define ULTRA_MIDI_IOCTL_DEVICE_OPEN	_IOW ( 'U', 0x00, struct ULTRA_MIDI_OPEN )
#define ULTRA_MIDI_IOCTL_DEVICE_CLOSE	_IO  ( 'U', 0x00 )
#define ULTRA_MIDI_IOCTL_TIMER_BASE	_IOW ( 'U', 0x02, int )
#define ULTRA_MIDI_IOCTL_TIMER_TEMPO	_IOW ( 'U', 0x03, int )
#define ULTRA_MIDI_IOCTL_TIMER_START	_IO  ( 'U', 0x04 )
#define ULTRA_MIDI_IOCTL_TIMER_STOP	_IO  ( 'U', 0x05 )
#define ULTRA_MIDI_IOCTL_TIMER_CONTINUE	_IO  ( 'U', 0x06 )
#define ULTRA_MIDI_IOCTL_THRESHOLD	_IOW ( 'U', 0x07, int )
#define ULTRA_MIDI_IOCTL_FLUSH		_IO  ( 'U', 0x08 )
#define ULTRA_MIDI_IOCTL_REALTIME	_IOW ( 'U', 0x08, struct ULTRA_MIDI_REALTIME )
#define ULTRA_MIDI_IOCTL_ABORT		_IO  ( 'U', 0x09 )
#define ULTRA_MIDI_IOCTL_SET_THRU	_IOW ( 'U', 0x0a, struct ULTRA_MIDI_THRU )
#define ULTRA_MIDI_IOCTL_GET_THRU	_IOR ( 'U', 0x0a, struct ULTRA_MIDI_THRU )

#define ULTRA_MIDI_IOCTL_PRELOAD_BANK	_IOW ( 'U', 0x10, struct ULTRA_MIDI_PRELOAD_BANK )
#define ULTRA_MIDI_IOCTL_PRELOAD_INSTRUMENT _IOW ( 'U', 0x11, struct ULTRA_MIDI_PRELOAD_INSTRUMENTS )
#define ULTRA_MIDI_IOCTL_GET_EMULATION	_IOR ( 'U', 0x12, struct ULTRA_MIDI_EMULATION )
#define ULTRA_MIDI_IOCTL_SET_EMULATION	_IOW ( 'U', 0x12, struct ULTRA_MIDI_EMULATION )

#define ULTRA_MIDI_IOCTL_MEMORY_RESET	_IOW ( 'U', 0x13, struct ULTRA_MIDI_MEMORY_RESET )
#define ULTRA_MIDI_IOCTL_MEMORY_TEST	_IOW ( 'U', 0x14, struct ULTRA_MIDI_INSTRUMENT )
#define ULTRA_MIDI_IOCTL_MEMORY_ALLOC	_IOW ( 'U', 0x15, struct ULTRA_MIDI_INSTRUMENT )
#define ULTRA_MIDI_IOCTL_MEMORY_FREE	_IOW ( 'U', 0x16, struct ULTRA_MIDI_INSTRUMENT )
#define ULTRA_MIDI_IOCTL_MEMORY_PACK	_IOW ( 'U', 0x17, struct ULTRA_MIDI_MEMORY_PACK )
#define ULTRA_MIDI_IOCTL_MEMORY_BALLOC	_IOW ( 'U', 0x18, struct ULTRA_MIDI_MEMORY_BLOCK )
#define ULTRA_MIDI_IOCTL_MEMORY_BFREE	_IOW ( 'U', 0x19, struct ULTRA_MIDI_MEMORY_BLOCK )
#define ULTRA_MIDI_IOCTL_MEMORY_GET_NAME _IOR ( 'U', 0x1a, struct ULTRA_MIDI_INSTRUMENT_NAME )
#define ULTRA_MIDI_IOCTL_MEMORY_DUMP	_IOW ( 'U', 0x1b, struct ULTRA_MIDI_MEMORY_DUMP )
#define ULTRA_MIDI_IOCTL_MEMORY_LIST	_IOWR( 'U', 0x1c, struct ULTRA_MIDI_MEMORY_LIST )

#define ULTRA_MIDI_IOCTL_CARD_INFO	_IOR ( 'U', 0x1d, struct ULTRA_MIDI_CARD_INFO )

#define ULTRA_MIDI_IOCTL_EFFECT_RESET	_IOW ( 'U', 0x1e, struct ULTRA_MIDI_EFFECT_RESET )
#define ULTRA_MIDI_IOCTL_EFFECT_SETUP		_IOW ( 'U', 0x1f, struct ULTRA_MIDI_EFFECT )

#define ULTRA_MIDI_IOCTL_DEVICES	_IOR ( 'U', 0x20, struct ULTRA_MIDI_DEVICES )
#define ULTRA_MIDI_IOCTL_DEVICE_INFO	_IOWR( 'U', 0x21, struct ULTRA_MIDI_DEVICE_INFO )

/* common commands */

#define ULTRA_MCMD_WAIT			0x00	/* wait n-ticks (unsigned int) */
#define ULTRA_MCMD_TEMPO		0x01	/* set tempo (unsigned short) */
#define ULTRA_MCMD_TBASE		0x02	/* set time base (unsigned short) */
#define ULTRA_MCMD_ECHO			0x80	/* echo command */

#define ULTRA_MIDI_OPEN_MODE_READ	0x01
#define ULTRA_MIDI_OPEN_MODE_WRITE	0x02
#define ULTRA_MIDI_OPEN_MODE_DUPLEX	0x03

struct ULTRA_MIDI_OPEN {
  unsigned short devices_used;
  struct {
    unsigned char device;
    unsigned char mode; 
  } devices[ ULTRA_MIDI_DEV_COUNT * ULTRA_CARDS ];
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_THRU {
  unsigned short device;		/* device which will generate MIDI THRU */
  struct {
    unsigned char device;		/* card + device - 0xff - not used */
    unsigned char channel;		/* destonation channel */
    unsigned char velocity;		/* destonation velocity - 0xff - no change */
  } routing[ 16 ][ ULTRA_MIDI_DEV_COUNT ]; /* 16 midi channels and X midi devices */
  unsigned char reserved[ 64 ];
};

struct ULTRA_STRU_MIDI_PRELOAD_BANK {
  unsigned short device;
  char name[ 64 ];
  unsigned char reserved[ 64 ];
};

struct ULTRA_MIDI_PRELOAD_INSTRUMENTS {
  unsigned short device;
  unsigned short instruments_used;
  unsigned int instruments[ 256 ];
  unsigned char reserved[ 64 ];
};

struct ULTRA_MIDI_EMULATION {
  unsigned short device;
  unsigned short emulation;
  unsigned char reserved[ 64 ];
};

struct ULTRA_MIDI_MEMORY_RESET {
  unsigned short device;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_INSTRUMENT {
  unsigned short device;
  ultra_instrument_t *instrument;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_MEMORY_PACK {
  unsigned short device;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_MEMORY_BLOCK {
  unsigned short device;
  struct ULTRA_STRU_MEMORY_BLOCK *block;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_INSTRUMENT_NAME {
  unsigned short device;
  ultra_instrument_info_t *name;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_MEMORY_DUMP {
  unsigned short device;
  ultra_memory_dump_t *dump;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_MEMORY_LIST {
  unsigned short device;
  ultra_memory_list_t *list;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_CARD_INFO {
  unsigned short device;
  ultra_synth_info_t *info;
};

struct ULTRA_MIDI_EFFECT_RESET {
  unsigned short device;
  unsigned char reserved[ 32 ];
};

struct ULTRA_MIDI_EFFECT {
  unsigned short device;
  struct ULTRA_STRU_EFFECT *effect;
};

struct ULTRA_MIDI_DEVICES {
  int count;
  unsigned char devices[ 256 ];
  unsigned char modes[ 256 ];
  unsigned char reserved[ 128 ];
};

#define ULTRA_MIDI_CAP_INPUT	0x00000001	/* input possible */
#define ULTRA_MIDI_CAP_OUTPUT	0x00000002	/* output possible */
#define ULTRA_MIDI_CAP_SYNTH	0x00000004	/* emulated MIDI */
#define ULTRA_MIDI_CAP_MEMORY	0x00000008	/* memory routines can be used */

struct ULTRA_MIDI_DEVICE_INFO {
  unsigned short device;
  unsigned char name[ 64 ];
  unsigned int cap;
  unsigned char reserved[ 128 ];
};

struct ULTRA_MIDI_REALTIME {
  unsigned char *buffer;
  unsigned int size;
  unsigned char reserved[ 32 ];
};

/*
 *  Instrument server interface
 */

#define ULTRA_DAEMON_MSG_REQUEST	0x0000	/* request to daemon */
#define ULTRA_DAEMON_MSG_REPLY		0x0001	/* reply from daemon */

#define ULTRA_DAEMON_MSG_END_OF_REQUEST 0x0000	/* end of request */
#define ULTRA_DAEMON_MSG_PRELOAD	0x0001	/* preload specified bank */
#define ULTRA_DAEMON_MSG_DOWNLOAD	0x0002	/* instrument download request */
#define ULTRA_DAEMON_MSG_EMUL		0x0003	/* emulation change */
#define ULTRA_DAEMON_MSG_BFREE		0x0004	/* block free */
#define ULTRA_DAEMON_MSG_IWEFF		0x0005	/* interwave effects */

struct ULTRA_STRU_DAEMON_MESSAGE {
  unsigned short direction;
  unsigned short command;
  union {
    unsigned short emul;		/* emulation */
    unsigned char bank_name[ 64 ]; 
    unsigned int instruments[ 64 ];	/* instrument numbers */
    int iw_effect[ 2 ];			/* interwave effect numbers */
    ultra_instrument_t *instrument;
    struct ULTRA_STRU_EFFECT *effect;	/* effect */
  } info; 
};

#define ULTRA_IOCTL_DMN_INFO		_IOR	( 'U', 0x00, struct ULTRA_STRU_INFO )
#define ULTRA_IOCTL_DMN_INSTRUMENT	_IOWR	( 'U', 0x01, int )
#define ULTRA_IOCTL_DMN_FREE		_IOR	( 'U', 0x02, int )
#define ULTRA_IOCTL_DMN_MEMORY_DUMP	_IOW	( 'U', 0x03, struct ULTRA_STRU_MEMORY_DUMP )
#define ULTRA_IOCTL_DMN_MEMORY_BALLOC	_IOW	( 'U', 0x04, struct ULTRA_STRU_MEMORY_BLOCK )
#define ULTRA_IOCTL_DMN_MEMORY_BFREE	_IOW	( 'U', 0x05, struct ULTRA_STRU_MEMORY_BLOCK )
#define ULTRA_IOCTL_DMN_LOCK		_IO	( 'U', 0x06 )
#define ULTRA_IOCTL_DMN_UNLOCK		_IO	( 'U', 0x07 )
#define ULTRA_IOCTL_DMN_GET_EMULATION	_IOR	( 'U', 0x08, int )
