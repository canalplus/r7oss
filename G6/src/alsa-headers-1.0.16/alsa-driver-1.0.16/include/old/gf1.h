/*
 *  Copyright (c) by Jaroslav Kysela (Perex soft)
 */

/* memory things */

#define SNDRV_MEM_BANK_SHIFT	18
#define SNDRV_MEM_BANK_SIZE	( 1L << SNDRV_MEM_BANK_SHIFT )
#define SNDRV_MEMORY_BANKS	( ( 1024L * 1024L ) / SNDRV_MEM_BANK_SIZE )
#define SNDRV_PNP_MEMORY_BANKS	SNDRV_MEMORY_BANKS
#define SNDRV_VOICES		32
#define SNDRV_CHANNELS		16
#define SNDRV_DRUM_CHANNEL	9

/* structures */

struct SNDRV_STRU_BANK_INFO {
  unsigned int address;
  unsigned int size;
};

#if 0	/* see to gus.h */
#define SNDRV_MEMORY_BLOCK_LOCKED	0x01
#define SNDRV_MEMORY_BLOCK_16BIT	0x02
#endif

#define SNDRV_MEMORY_OWNER_DRIVER	0x00
#define SNDRV_MEMORY_OWNER_WAVE	0x01	/* owner is wave */
#define SNDRV_MEMORY_OWNER_USER	0x02

#define SNDRV_MEMORY_LOCK_DRIVER	0x00
#define SNDRV_MEMORY_LOCK_DAEMON	0x01
#define SNDRV_MEMORY_LOCK_USER	0x02

struct SNDRV_STRU_MEM_BLOCK {
  unsigned short flags: 4,
                 owner: 4,
                 lock_group: 8;
  unsigned int share;			/* share count */
  unsigned int share_id1, share_id2;	/* share ID */
  unsigned int ptr;
  unsigned int size;
  union {
    struct SNDRV_STRU_WAVE *wave;
    char *name;
  } owner_data;
  struct SNDRV_STRU_MEM_BLOCK *prev;
  struct SNDRV_STRU_MEM_BLOCK *next;
};

typedef struct SNDRV_STRU_MEM_BLOCK snd_mem_block_t;

#define MTST_NONE		0x0000
#define MTST_ONE		0x0001

#define SNDRV_ALLOC_MODE_NORMAL	0x0000
#define SNDRV_ALLOC_MODE_TEST	0x0001	/* don't use samples pointer */

struct SNDRV_STRU_MEM_ALLOC {
  unsigned short mode;

  snd_mem_block_t *first;
  snd_mem_block_t *last;

  unsigned int instruments_locked;
  unsigned int instruments_count;
  snd_instrument_t *instruments;
  snd_instrument_t *instruments_root;

  MUTEX_DEFINE( memory )
  MUTEX_DEFINE( memory_find )
  MUTEX_DEFINE( instruments )
};

typedef struct SNDRV_STRU_MEM_ALLOC snd_mem_t;

typedef struct SNDRV_STRU_VOICE snd_voice_t;
typedef struct SNDRV_STRU_NOTE snd_note_t;
typedef struct SNDRV_STRU_CHANNEL snd_channel_t;

struct SNDRV_STRU_INSTRUMENT_VOICE_COMMANDS {
  /* voice specific handlers */
  void (*voice_interrupt_wave)( struct snd_card *card, snd_voice_t *voice );
  void (*voice_interrupt_volume)( struct snd_card *card, snd_voice_t *voice );
  void (*voice_interrupt_effect)( struct snd_card *card, snd_voice_t *voice );
  /* voice specific commands */
  void (*voice_start)( struct snd_card *card, snd_voice_t *voice );
  void (*voice_go)( struct snd_card *card, snd_voice_t *voice );
  void (*voice_stop)( struct snd_card *card, snd_voice_t *voice, unsigned char mode );
  void (*voice_control)( struct snd_card *card, snd_voice_t *voice, unsigned char control );
  void (*voice_freq)( struct snd_card *card, snd_voice_t *voice, unsigned int freq );
  void (*voice_volume)( struct snd_card *card, snd_voice_t *voice, unsigned short volume );
  void (*voice_loop)( struct snd_card *card, snd_voice_t *voice, unsigned int start, unsigned int end );
  void (*voice_ramp)( struct snd_card *card, snd_voice_t *voice, unsigned char start, unsigned char end, unsigned char rate, unsigned char control );
  void (*voice_pos)( struct snd_card *card, snd_voice_t *voice, unsigned int position );
  void (*voice_pan)( struct snd_card *card, snd_voice_t *voice, unsigned short pan );
  void (*voice_lfo)( struct snd_card *card, snd_voice_t *voice, unsigned char *data );
  void (*voice_private1)( struct snd_card *card, snd_voice_t *voice, unsigned char *data );
};

struct SNDRV_STRU_INSTRUMENT_NOTE_COMMANDS {
  /* note specific commands */
  void (*note_stop)( struct snd_card *card, snd_voice_t *voice, int wait );
  void (*note_wait)( struct snd_card *card, snd_voice_t *voice );
  void (*note_off)( struct snd_card *card, snd_voice_t *voice );
  void (*note_volume)( struct snd_card *card, snd_voice_t *voice );
  void (*note_pitchbend)( struct snd_card *card, snd_voice_t *voice );
  void (*note_vibrato)( struct snd_card *card, snd_voice_t *voice );
  void (*note_tremolo)( struct snd_card *card, snd_voice_t *voice );
};

struct SNDRV_STRU_INSTRUMENT_CHANNEL_COMMANDS {
  /* channel specific commands */
  void (*chn_trigger_down)( struct snd_card *card, snd_channel_t *channel, snd_instrument_t *instrument, unsigned char note, unsigned char velocity, unsigned char priority );
  void (*chn_trigger_up)( struct snd_card *card, snd_note_t *note );
  void (*chn_control)( struct snd_card *card, snd_channel_t *channel, unsigned short p1, unsigned short p2 );
};

#define VFLG_USED		0x00000001	/* used voice */
#define VFLG_DYNAMIC		0x00000002	/* dynamicaly allocated */
#define VFLG_EFFECT_TIMER1	0x00000004	/* effect timer 1 is active */
#define VFLG_EFFECT_TIMER2	0x00000008	/* effect timer 2 is active */
#define VFLG_EFFECT_TIMER	(VFLG_EFFECT_TIMER1|VFLG_EFFECT_TIMER2)
#define VFLG_WAIT_FOR_START	0x00000010	/* ok.. wait for voice start */
#define VFLG_WAIT_FOR_START1	0x00000020	/* ok.. wait for start command */
#define VFLG_RUNNING		0x00000040	/* voice is running */
#define VFLG_RELEASING		0x00000080	/* voice is releasing */
#define VFLG_SUSTAINING		0x00000100	/* voice is sustaining */
#define VFLG_VIBRATO_RUNNING	0x00000200	/* vibrato is running */
#define VFLG_TREMOLO_RUNNING	0x00000400	/* tremolo is running */
#define VFLG_SELECT_WAVE	0x00000800	/* select wave in progress */
#define VFLG_SHUTDOWN		0x00001000	/* shutdown in progress */
#define VFLG_PAN		0x00002000	/* pan envelope in progress */
#define VFLG_IRQ		(VFLG_USED|VFLG_SHUTDOWN)
#define VFLG_OSS_PAN		0x00010000	/* OSS pan for patches */ 
#define VFLG_OSS_BALANCE	0x00020000	/* OSS balance */

enum snd_simple_env_index {
  SIMPLE_BEFORE = -1,
  SIMPLE_ATTACK = 0,
  SIMPLE_SUSTAIN = 1,
  SIMPLE_RELEASE = 2,
  SIMPLE_DONE = 3,
  SIMPLE_VOLUME = 4,
};

enum snd_patch_env_index {
  PATCH_BEFORE = -1,
  PATCH_ATTACK = 0,
  PATCH_SUSTAIN = 1,
  PATCH_VAR_RELEASE = 2,
  PATCH_FINAL_RELEASE = 3,
  PATCH_DONE = 4,
  PATCH_VOLUME = 5,			/* special for volume change */
};

enum snd_iw_env_index {
  IW_BEFORE = -1,
  IW_ATTACK = 0,
  IW_SUSTAIN = 1,
  IW_VAR_RELEASE = 2,
  IW_FINAL_RELEASE = 3,
  IW_DONE = 4,
  IW_VOLUME = 5,
};

struct SNDRV_STRU_VOICE {
  struct SNDRV_STRU_INSTRUMENT_VOICE_COMMANDS *commands;
  unsigned short number;	/* number of this voice */
  unsigned int flags;		/* see to VFLG_XXXX constants */
  unsigned int age;		/* start time in ticks */
  unsigned char start_command;  

  snd_note_t *note;
  snd_instrument_t *instrument;
  snd_layer_t *layer;
  snd_wave_t *wave;

  unsigned char priority;	/* voice priority */

  void (*steal_notify)( struct snd_card *card, snd_voice_t *voice );

  unsigned int sloop;

  unsigned short fc_register;
  signed short lfo_fc;
  signed short lfo_volume;

  struct {
    struct {
      unsigned char volume_control;
      unsigned char control;
      unsigned char mode;
      unsigned int frequency;
      unsigned short volume;
      unsigned short gf1_volume;
      unsigned int loop_start;
      unsigned int loop_end;
      unsigned char ramp_start;
      unsigned char ramp_end;
      unsigned char ramp_rate;
      unsigned char ramp_control;
      unsigned int position;
      unsigned short pan;
      unsigned char gf1_pan;
      enum snd_simple_env_index venv_state, venv_state_prev;
      unsigned char venv_next, venv_rate;
      unsigned short vlo, vro;
      unsigned char effect_accumulator;
      unsigned short effect_volume;
    } simple;
    struct {
      unsigned char volume_control;
      unsigned char control;
      unsigned char mode;
      unsigned short ofc_reg;
      unsigned char pan;
      unsigned char gf1_pan;
      unsigned short volume;
      enum snd_patch_env_index venv_state, venv_state_prev;
      short venv_index;
      unsigned char venv_next, venv_rate;
      unsigned int frequency;
      unsigned short note;
      unsigned short pitchbend;
      unsigned short voice_pan;
      unsigned int loop_start;
      unsigned int loop_end;
      unsigned int position;
      unsigned short vlo, vro;
      unsigned char effect_accumulator;
      unsigned short effect_volume;
    } patch;
    struct {
      unsigned short ofc_reg;
      unsigned short vel_atten;
      unsigned short layer_atten;
      unsigned short wave_atten;
      unsigned short gf1_volume;
      enum snd_iw_env_index venv_state, venv_state_prev;
      enum snd_iw_env_index penv_state;
      short venv_index;
      short penv_index;
      struct SNDRV_STRU_IW_ENV_RECORD *ver;
      struct SNDRV_STRU_IW_ENV_RECORD *per;
      unsigned char venv_next, venv_rate;
      int penv_pitch;
      int penv_increment;
      int penv_next;
      short penv_timer;
      short penv_in_progress;
      short pan;
      short pan_offset;
      short rel_offset;
      /* ok.. registers */
      unsigned short fc_register;
      unsigned char voice_control;
      unsigned char volume_control;
    } iw;
  } data;
  snd_voice_t *next;
  snd_voice_t *anext;
};

#define NFLG_USED		0x00000001
#define NFLG_RELEASING		0x00000002
#define NFLG_SUSTAIN		0x00000004

struct SNDRV_STRU_NOTE {
  struct SNDRV_STRU_INSTRUMENT_NOTE_COMMANDS *commands;
  unsigned short number;
  unsigned int flags;
  snd_voice_t *voices;		/* first voice */
  snd_channel_t *channel;	/* channel - owner of this note */  
  snd_instrument_t *instrument;
  unsigned char velocity;
  unsigned char priority;
  snd_note_t *next;
};

#define CFLG_LEGATO		0x00000001
#define CFLG_SUSTAIN		0x00000002

struct SNDRV_STRU_CHANNEL {
  struct SNDRV_STRU_INSTRUMENT_CHANNEL_COMMANDS *commands;
  unsigned short number;	/* number of this channel */
  unsigned int flags;		/* see to CFLG_XXXX */
  snd_note_t *notes;		/* pointer to first note */
  snd_instrument_t *instrument;
  unsigned short bank;		/* MIDI bank # */
  unsigned char volume;
  unsigned char expression;
  unsigned char pan;
  unsigned short pitchbend;
  short vib_depth;
  short trem_depth;
  unsigned short gf1_pitchbend;
  unsigned short pitchbend_range;
  unsigned short rpn, nrpn;
  unsigned short effect1_level;	/* reverb level */
  unsigned short effect3_level;	/* chorus level */
  snd_channel_t *next;
};

struct SNDRV_STRU_GF1_QUEUE {
  unsigned char *	ptr;
  unsigned int		size;
  volatile unsigned int	used;
  unsigned int		head;
  unsigned int		tail;
  unsigned int		threshold;
  unsigned int		item_size;
};

struct SNDRV_STRU_IW_LFO_PROGRAM {
  unsigned short freq_and_control;
  unsigned char depth_final;
  unsigned char depth_inc;
  unsigned short twave;
  unsigned short depth;
};

/* defines for gf1.mode */

#define GF1_MODE_NONE           0x0000
#define GF1_MODE_ENGINE		0x0001
#define GF1_MODE_SYNTH          0x0002
#define GF1_MODE_PCM_PLAY	0x0004 
#define GF1_MODE_PCM_RECORD	0x0008
#define GF1_MODE_PCM		(GF1_MODE_PCM_PLAY|GF1_MODE_PCM_RECORD)
#define GF1_MODE_BOTH		(GF1_MODE_PCM_PLAY|GF1_MODE_SYNTH)
#define GF1_MODE_MIDI		0x0010
#define GF1_MODE_DAEMON		0x0020
#define GF1_MODE_TIMER		0x0040
#define GF1_MODE_EFFECT		0x0080

/* indexs for voice_ranges array */

#define GF1_VOICE_RANGES	3
#define GF1_VOICE_RANGE_SYNTH	0
#define GF1_VOICE_RANGE_PCM	1
#define GF1_VOICE_RANGE_EFFECT	2

/* constants for interrupt handlers */

#define GF1_HANDLER_MIDI_OUT		0x00010000
#define GF1_HANDLER_MIDI_IN		0x00020000
#define GF1_HANDLER_TIMER1		0x00040000
#define GF1_HANDLER_TIMER2		0x00080000
#define GF1_HANDLER_RANGE		0x00100000
#define GF1_HANDLER_SYNTH_DMA_WRITE	0x00200000
#define GF1_HANDLER_PCM_DMA_WRITE	0x00400000
#define GF1_HANDLER_PCM_DMA_READ	0x00800000
#define GF1_HANDLER_ALL			(0xffff0000&~GF1_HANDLER_RANGE)

struct SNDRV_STRU_GF1 {

  unsigned int enh_mode: 1,		/* enhanced mode (GFA1) */
               pcm_memory: 1,		/* reserve memory for PCM */ 
  	       dma_download: 1,		/* use DMA download to GUS's DRAM */
  	       hw_lfo: 1,		/* use hardware LFO */
  	       sw_lfo: 1;		/* use software LFO */
  
  unsigned long port;		/* port of GF1 chip */
  unsigned int irq;		/* IRQ number of GF1 chip */
  unsigned int memory;		/* GUS's DRAM size in bytes */
  unsigned int rom_memory;	/* GUS's ROM size in bytes */
  unsigned int rom_present;	/* bitmask */
  unsigned int rom_banks;	/* GUS's ROM banks */

  /* registers */
  unsigned short reg_page;
  unsigned short reg_regsel;
  unsigned short reg_data8;
  unsigned short reg_data16;
  unsigned short reg_irqstat;
  unsigned short reg_dram;
  unsigned short reg_timerctrl;
  unsigned short reg_timerdata;
  /* --------- */

  unsigned char active_voice;	/* selected voice (GF1PAGE register) */
  unsigned char active_voices;	/* all active voices */

  unsigned int default_voice_address;

  unsigned short playback_freq;	/* GF1 playback (mixing) frequency */
  unsigned short mode;		/* see to GF1_MODE_XXXX */
  snd_mem_t mem_alloc;

  struct SNDRV_STRU_BANK_INFO banks_8 [ SNDRV_MEMORY_BANKS ];
  struct SNDRV_STRU_BANK_INFO banks_16[ SNDRV_MEMORY_BANKS ];

  struct SNDRV_STRU_EFFECT *effects;
  unsigned char *lfos;

  /* interrupt handlers */
  
  void (*interrupt_handler_midi_out)( struct snd_card *card );
  void (*interrupt_handler_midi_in)( struct snd_card *card );
  void (*interrupt_handler_timer1)( struct snd_card *card );
  void (*interrupt_handler_timer2)( struct snd_card *card );
  void (*interrupt_handler_synth_dma_write)( struct snd_card *card );
  void (*interrupt_handler_pcm_dma_write)( struct snd_card *card );
  void (*interrupt_handler_pcm_dma_read)( struct snd_card *card );

  /* special interrupt handlers */
  
  struct GF1_STRU_VOICE_RANGE {
    unsigned short rvoices;	/* requested voices */
    unsigned short voices;	/* total voices for this range */
    unsigned short min, max;	/* minimal & maximal voice */
#ifdef ULTRACFG_INTERRUPTS_PROFILE
    unsigned int interrupt_stat_wave;
    unsigned int interrupt_stat_volume;
#endif
    void (*interrupt_handler_wave)( struct snd_card *card, int voice );
    void (*interrupt_handler_volume)( struct snd_card *card, int voice );
    void (*voices_change_start)( struct snd_card *card );
    void (*voices_change_stop)( struct snd_card *card );
    void (*volume_change)( struct snd_card *card );
  } voice_ranges[ GF1_VOICE_RANGES ];

#ifdef ULTRACFG_INTERRUPTS_PROFILE
  /* interrupt statistics */
  
  unsigned int interrupt_stat_midi_out;
  unsigned int interrupt_stat_midi_in;
  unsigned int interrupt_stat_timer1;
  unsigned int interrupt_stat_timer2;
  unsigned int interrupt_stat_dma_read;
  unsigned int interrupt_stat_dma_write;
  unsigned int interrupt_stat_voice_lost;
#endif

  /* needed by gf1_synth.c */

  unsigned int syn_abort_flag: 2,
	       syn_stop_flag: 1,
               syn_flush_flag: 1,
               syn_dma_lock: 1,
               rpn_or_nrpn: 1;
  unsigned short syn_voices_change;
  unsigned int syn_voice_dynamic;
  snd_voice_t *syn_voices;
  snd_voice_t *syn_voice_first;
  snd_note_t *syn_notes;
  snd_channel_t *syn_channels;
  
  struct SNDRV_STRU_GF1_QUEUE wqueue;
  struct SNDRV_STRU_GF1_QUEUE rqueue;

  unsigned int effect_timer;

  unsigned short default_midi_emul;
  unsigned short midi_emul;
  unsigned short midi_master_volume;
  unsigned int default_smooth_pan: 1,
  	       smooth_pan: 1,
  	       default_full_range_pan: 1,
  	       full_range_pan: 1,
 	       default_volume_ramp: 8,
  	       volume_ramp: 8;
  
  /* needed by gf1_timer.c */

  unsigned int timer_lock: 1,
  	       timer_master: 1,
  	       timer_slave: 1,
  	       timer_midi: 1,
  	       timer_sequencer: 1;
  unsigned char timer_enabled;
  char *timer_owner;
  unsigned short timer_old_count1;
  unsigned short timer_old_count2;
  struct snd_card *timer_master_card;
  struct snd_card *timer_slave_cards[ SNDRV_CARDS ];
  unsigned int timer_wait_ticks;
  unsigned int timer_base;
  unsigned int timer_tempo;
  unsigned int timer_tempo_value;
  unsigned int timer_tempo_modulo;
  unsigned int timer_effects_ticks;
  unsigned int timer_effects_ticks_current;
  unsigned int timer_abs_tick;

  /* needed by gf1_daemon.c */
  
  unsigned int daemon_emul: 1,
               daemon_preload: 1,
               daemon_free: 1,
               daemon_iw_effect: 1,
               daemon_active: 1,
               daemon_lock: 1;

  unsigned short daemon_request;
  unsigned int daemon_instruments[ 64 ];
  unsigned short daemon_instruments_count;
  unsigned short daemon_midi_emul;
  unsigned char *daemon_preload_bank_name;
 
#ifdef ULTRACFG_GF1PCM

  /* needed by gf1_pcm.c */

  unsigned short pcm_pflags;			/* playback flags */
  unsigned short pcm_rflags;			/* record flags */
  unsigned char pcm_volume[ SNDRV_VOICES ];	/* volume level - 0-128 */
  unsigned char pcm_pan[ SNDRV_VOICES ];		/* pan - 0-128 */
  unsigned short pcm_dma_voice;
  unsigned short pcm_head;			/* playback block head */
  unsigned short pcm_tail;			/* playback block tail */
  unsigned short pcm_blocks;			/* playback blocks */
  unsigned short pcm_block_size;		/* playback block size */
  unsigned short pcm_used;			/* playback used blocks */
  unsigned short pcm_mmap_div;
  unsigned int pcm_mem;
  unsigned int pcm_pos;				/* pcm position */
  unsigned int pcm_record_overflow;

  unsigned char pcm_sampling_ctrl_reg;

#endif

#ifdef ULTRACFG_MIDI_DEVICES

  /* needed by gf1_midi.c */

  unsigned short midi_mix_voices;

  /* needed by gf1_uart.c */

  unsigned char uart_cmd;
  unsigned char uart_enable;
  unsigned int uart_overrun;
  unsigned int uart_framing;
  
#endif
};

#endif /* __DRIVER_STRU__ */

#ifdef __DRIVER_MAIN__

/* IO ports */

#define GUSP( card, x ) ( card -> gf1.port + g_u_s_##x )

#define g_u_s_MIDICTRL		(0x320-0x220)
#define g_u_s_MIDISTAT		(0x320-0x220)
#define g_u_s_MIDIDATA		(0x321-0x220)

#define g_u_s_GF1PAGE		(0x322-0x220)
#define g_u_s_GF1REGSEL		(0x323-0x220)
#define g_u_s_GF1DATALOW	(0x324-0x220)
#define g_u_s_GF1DATAHIGH	(0x325-0x220)
#define g_u_s_IRQSTAT		(0x226-0x220)
#define g_u_s_TIMERCNTRL	(0x228-0x220)
#define g_u_s_TIMERDATA		(0x229-0x220)
#define g_u_s_DRAM		(0x327-0x220)
#define g_u_s_MIXCNTRLREG	(0x220-0x220)
#define g_u_s_IRQDMACNTRLREG	(0x22b-0x220)
#define g_u_s_REGCNTRLS		(0x22f-0x220)
#define g_u_s_BOARDVERSION	(0x726-0x220)
#define g_u_s_MIXCNTRLPORT	(0x726-0x220)
#define g_u_s_IVER		(0x325-0x220)
#define g_u_s_MIXDATAPORT	(0x326-0x220)
#define g_u_s_MAXCNTRLPORT	(0x326-0x220)

/* GF1 registers */

/* global registers */
#define GF1_GB_ACTIVE_VOICES		0x0e
#define GF1_GB_VOICES_IRQ		0x0f
#define GF1_GB_GLOBAL_MODE		0x19
#define GF1_GW_LFO_BASE			0x1a
#define GF1_GB_VOICES_IRQ_READ		0x1f
#define GF1_GB_DRAM_DMA_CONTROL		0x41
#define GF1_GW_DRAM_DMA_LOW		0x42
#define GF1_GW_DRAM_IO_LOW		0x43
#define GF1_GB_DRAM_IO_HIGH		0x44
#define GF1_GB_SOUND_BLASTER_CONTROL	0x45
#define GF1_GB_ADLIB_TIMER_1		0x46
#define GF1_GB_ADLIB_TIMER_2		0x47
#define GF1_GB_REC_DMA_CONTROL		0x49
#define GF1_GB_JOYSTICK_DAC_LEVEL	0x4b
#define GF1_GB_RESET			0x4c
#define GF1_GB_DRAM_DMA_HIGH		0x50
#define GF1_GW_DRAM_IO16		0x51
#define GF1_GW_MEMORY_CONFIG		0x52
#define GF1_GB_MEMORY_CONTROL		0x53
#define GF1_GW_FIFO_RECORD_BASE_ADDR	0x54
#define GF1_GW_FIFO_PLAY_BASE_ADDR	0x55
#define GF1_GW_FIFO_SIZE		0x56
#define GF1_GW_INTERLEAVE		0x57
#define GF1_GB_COMPATIBILITY		0x59
#define GF1_GB_DECODE_CONTROL		0x5a
#define GF1_GB_VERSION_NUMBER		0x5b
#define GF1_GB_MPU401_CONTROL_A		0x5c
#define GF1_GB_MPU401_CONTROL_B		0x5d
#define GF1_GB_EMULATION_IRQ		0x60
/* voice specific registers */
#define GF1_VB_ADDRESS_CONTROL		0x00
#define GF1_VW_FREQUENCY		0x01
#define GF1_VW_START_HIGH		0x02
#define GF1_VW_START_LOW		0x03
#define GF1_VA_START			GF1_VW_START_HIGH
#define GF1_VW_END_HIGH			0x04
#define GF1_VW_END_LOW			0x05
#define GF1_VA_END			GF1_VW_END_HIGH
#define GF1_VB_VOLUME_RATE		0x06
#define GF1_VB_VOLUME_START		0x07
#define GF1_VB_VOLUME_END		0x08
#define GF1_VW_VOLUME			0x09
#define GF1_VW_CURRENT_HIGH		0x0a
#define GF1_VW_CURRENT_LOW		0x0b
#define GF1_VA_CURRENT			GF1_VW_CURRENT_HIGH
#define GF1_VB_PAN			0x0c
#define GF1_VW_OFFSET_RIGHT		0x0c
#define GF1_VB_VOLUME_CONTROL		0x0d
#define GF1_VB_UPPER_ADDRESS		0x10
#define GF1_VW_EFFECT_HIGH		0x11
#define GF1_VW_EFFECT_LOW		0x12
#define GF1_VA_EFFECT			GF1_VW_EFFECT_HIGH
#define GF1_VW_OFFSET_LEFT		0x13
#define GF1_VB_ACCUMULATOR		0x14
#define GF1_VB_MODE			0x15
#define GF1_VW_EFFECT_VOLUME		0x16
#define GF1_VB_FREQUENCY_LFO		0x17
#define GF1_VB_VOLUME_LFO		0x18
#define GF1_VW_OFFSET_RIGHT_FINAL	0x1b
#define GF1_VW_OFFSET_LEFT_FINAL	0x1c
#define GF1_VW_EFFECT_VOLUME_FINAL	0x1d

/* ICS registers */

#define ICS_MIC_DEV		0
#define ICS_LINE_DEV		1
#define ICS_CD_DEV		2
#define ICS_GF1_DEV		3
#define ICS_NONE_DEV		4
#define ICS_MASTER_DEV		5

/* LFO */

#define SNDRV_LFO_TREMOLO		0
#define SNDRV_LFO_VIBRATO		1

/* misc */

#define DMA_DOWNLOAD_SLEEP_TIME	(HZ*2)
#define BAD_DMA_DOWNLOAD_SLEEP_TIME 15

/* ramp ranges */

#define ATTEN( x )              (snd_atten_table[ x ])
#define VOLUME( x )             (snd_atten_table[ x ] << 1)
#define MIN_VOLUME              1800
#define MAX_VOLUME              4095
#define MIN_OFFSET              (MIN_VOLUME>>4)
#define MAX_OFFSET              255
#define MAX_TDEPTH		90

/* I/O functions for GF1/InterWave chip */

static inline void gf1_select_voice( struct snd_card *card, int voice )
{
  unsigned long flags;
  
  CLI( &flags );
  if ( voice != card -> gf1.active_voice )
    {
      card -> gf1.active_voice = voice;
      OUTB( voice, GUSP( card, GF1PAGE ) );
    }
  STI( &flags );
}

#ifdef ULTRACFG_MIDI_DEVICES

static inline void gf1_uart_cmd( struct snd_card *card, unsigned char b )
{
  OUTB( card -> gf1.uart_cmd = b, GUSP( card, MIDICTRL ) );
}

static inline unsigned char gf1_uart_stat( struct snd_card *card )
{
  return INB( GUSP( card, MIDISTAT ) );
}

static inline void gf1_uart_put( struct snd_card *card, unsigned char b )
{ 
  OUTB( b, GUSP( card, MIDIDATA ) );
}

static inline unsigned char gf1_uart_get( struct snd_card *card )
{
  return INB( GUSP( card, MIDIDATA ) );
}

#endif

extern void snd_delay( struct snd_card *card );
extern void snd_delay1( int loops );
extern void snd_delay2( int loops );

extern void snd_ctrl_stop( struct snd_card *card, unsigned char reg );

extern void snd_write8( struct snd_card *card, unsigned char reg, unsigned char data );
extern unsigned char snd_look8( struct snd_card *card, unsigned char reg );
extern inline unsigned char snd_read8( struct snd_card *card, unsigned char reg ) { return snd_look8( card, reg | 0x80 ); }
extern void snd_write16( struct snd_card *card, unsigned char reg, unsigned int data );
extern unsigned short snd_look16( struct snd_card *card, unsigned char reg );
extern inline unsigned short snd_read16( struct snd_card *card, unsigned char reg ) { return snd_look16( card, reg | 0x80 ); }
extern void snd_adlib_write( struct snd_card *card, unsigned char reg, unsigned char data );
extern void snd_dram_addr( struct snd_card *card, unsigned int addr );
extern void snd_poke( struct snd_card *card, unsigned int addr, unsigned char data );
extern unsigned char snd_peek( struct snd_card *card, unsigned int addr );
#ifdef ULTRACFG_PNP
extern void snd_pokew( struct snd_card *card, unsigned int addr, unsigned short data );
extern unsigned short snd_peekw( struct snd_card *card, unsigned int addr );
extern void snd_dram_setmem( struct snd_card *card, unsigned int addr, unsigned short value, unsigned int count );
#endif
extern void snd_write_addr( struct snd_card *card, unsigned char reg, unsigned int addr, short w_16bit );
extern unsigned int snd_read_addr( struct snd_card *card, unsigned char reg, short w_16bit );
extern void snd_i_ctrl_stop( struct snd_card *card, unsigned char reg );
extern void snd_i_write8( struct snd_card *card, unsigned char reg, unsigned char data );
extern unsigned char snd_i_look8( struct snd_card *card, unsigned char reg );
extern void snd_i_write16( struct snd_card *card, unsigned char reg, unsigned int data );
extern inline unsigned char snd_i_read8( struct snd_card *card, unsigned char reg ) { return snd_i_look8( card, reg | 0x80 ); }
extern unsigned short snd_i_look16( struct snd_card *card, unsigned char reg );
extern inline unsigned short snd_i_read16( struct snd_card *card, unsigned char reg ) { return snd_i_look16( card, reg | 0x80 ); }
extern void snd_i_adlib_write( struct snd_card *card, unsigned char reg, unsigned char data );
extern void snd_i_write_addr( struct snd_card *card, unsigned char reg, unsigned int addr, short w_16bit );
extern unsigned int snd_i_read_addr( struct snd_card *card, unsigned char reg, short w_16bit );

extern void snd_reselect_active_voices( struct snd_card *card );

extern void snd_engine_instrument_register(
		unsigned short mode,
		struct SNDRV_STRU_INSTRUMENT_VOICE_COMMANDS *voice_cmds,
		struct SNDRV_STRU_INSTRUMENT_NOTE_COMMANDS *note_cmds,
		struct SNDRV_STRU_INSTRUMENT_CHANNEL_COMMANDS *channel_cmds );
extern int snd_engine_instrument_register_ask( unsigned short mode );

#ifdef ULTRACFG_DEBUG
extern void snd_print_voice_registers( struct snd_card *card );
extern void snd_print_global_registers( struct snd_card *card );
extern void snd_print_setup_registers( struct snd_card *card );
extern void snd_peek_print_block( struct snd_card *card, unsigned int addr, int count, int w_16bit );
#endif
