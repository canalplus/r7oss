/****************************************************************************
 ** hw_commandir.h **********************************************************
 ****************************************************************************
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * -- Original hw_default.h
 * Modified for CommandIR Transceivers, April-June 2008, Matthew Bodkin
 * Modified for CommandIR III - March-August 2010 - Matthew Bodkin
 */

#ifndef HW_COMMANDIR_H
#define HW_COMMANDIR_H

#include <usb.h>

extern struct ir_remote *repeat_remote;
extern char *progname;


#define TRUE	0
#define FALSE	1

#define RX_BUFFER_SIZE 1024
#define TX_BUFFER_SIZE 1024
#define TX_QUEUE 1
#define RX_QUEUE 0
#define MAX_COMMANDIRS 4
#define MAX_COMMAND 8192

/* transmitter channel control */
#define MAX_DEVICES		4
#define MAX_TX_TIMERS    16
#define DEVICE_CHANNELS	4
#define MAX_MASK 		0xffff
#define MAX_SIGNALQ		100
// 32-bits is the most emitters we can support on one CommandIR:
#define MAX_EMITTERS	32

/* CommandIR control codes */
#define CHANNEL_EN_MASK	1
#define FREQ_HEADER     2
#define MCU_CTRL_SIZE   3
#define TX_HEADER       7
#define TX_HEADER_NEW	8
/* New for CommandIR II  */

#define READ_INPUTS		10
#define PROC_SET		11
#define INIT_FUNCTION	12
#define RX_SELECT		13
#define TX_COMMANDIR_II 14
/* Internal to driver */
#define TX_LIRC_T	    15
#define FREQ_HEADER_LIRC 16
#define RXDECODE_HEADER_LIRC 17
#define INIT_HEADER_LIRC 18
#define DEINIT_HEADER_LIRC 19
#define GET_VERSION 	20

#define COMMANDIR_2_PULSE_MASK 0x8000
#define COMMANDIR_2_OVERFLOW_MASK 0x4000

#define DEFAULT_PULSE_WIDTH 13

#define USB_CMDIR_VENDOR_ID		0x10c4
#define USB_CMDIR_PRODUCT_ID	0x0003
#define USB_CMDIR_MINOR_BASE	192

#define HW_COMMANDIR_MINI 	1
#define HW_COMMANDIR_2		2
#define HW_COMMANDIR_3		3
#define HW_COMMANDIR_UNKNOWN 127

#define MAX_HW_MINI_PACKET 64

// CommandIR has lots of buffer room, we don't need to poll constantly
#define USB_TIMEOUT_MS 5000
#define USB_TIMEOUT_US 1000
#define WAIT_BETWEEN_READS_US 10000
#define MAX_WAIT_BETWEEN_READS_US 5000000
#define MIN_WAIT_BETWEEN_READS_US 5000

#define USB_MAX_BUSES	8
#define USB_MAX_BUSDEV	127

#define RX_HEADER_DATA 		0x01
#define RX_HEADER_EVENTS 	0x02
#define RX_HEADER_TXAVAIL 	0x03


// We keep CommandIR's OPEN even on -deinit for speed and to monitor
// Other non-LIRC events (plugin, suspend, etc) - and settings!
#define USB_KEEP_WARM 1

// CommandIR lircd.conf event driven code definitions
#define LIRCCODE_GAP  125000
#define JACK_PLUG_1		0x01
#define JACK_PLUG_2		0x02
#define JACK_PLUG_3		0x03
#define JACK_PLUG_4		0x04
#define JACK_PLUG_5		0x11
#define JACK_PLUG_6		0x12
#define JACK_PLUG_7		0x13
#define JACK_PLUG_8		0x14
#define JACK_PLUG_9		0x21
#define JACK_PLUG_10	0x22
#define JACK_PLUG_11	0x23
#define JACK_PLUG_12	0x24
#define JACK_PLUG_13	0x31
#define JACK_PLUG_14	0x32
#define JACK_PLUG_15	0x33
#define JACK_PLUG_16	0x34

#define JACK_UNPLUG_1	0x05
#define JACK_UNPLUG_2	0x06
#define JACK_UNPLUG_3	0x07
#define JACK_UNPLUG_4	0x08
#define JACK_UNPLUG_5	0x15
#define JACK_UNPLUG_6	0x16
#define JACK_UNPLUG_7	0x17
#define JACK_UNPLUG_8	0x18
#define JACK_UNPLUG_9	0x25
#define JACK_UNPLUG_10	0x26
#define JACK_UNPLUG_11	0x27
#define JACK_UNPLUG_12	0x28
#define JACK_UNPLUG_13	0x35
#define JACK_UNPLUG_14	0x36
#define JACK_UNPLUG_15	0x37
#define JACK_UNPLUG_16	0x38

#define SELECT_TX_INTERNAL	0x09
#define SELECT_TX_ExTERNAL	0x0A

#define SELECT_TX_ON_1		0x0D
#define SELECT_TX_ON_2		0x1D
#define SELECT_TX_ON_3		0x2D
#define SELECT_TX_ON_4		0x3D

#define JACK_PLUG_RX_1		0x0B
#define JACK_UNPLUG_RX_1	0x0C
#define JACK_PLUG_RX_2		0x1B
#define JACK_UNPLUG_RX_2	0x1C
#define JACK_PLUG_RX_3		0x2B
#define JACK_UNPLUG_RX_3	0x2C
#define JACK_PLUG_RX_4		0x3B
#define JACK_UNPLUG_RX_4	0x3C

#define COMMANDIR_PLUG_1	0x41
#define COMMANDIR_PLUG_2	0x42
#define COMMANDIR_PLUG_3	0x43
#define COMMANDIR_PLUG_4	0x44

#define COMMANDIR_UNPLUG_1	0x45
#define COMMANDIR_UNPLUG_2	0x46
#define COMMANDIR_UNPLUG_3	0x47
#define COMMANDIR_UNPLUG_4	0x48

#define COMMANDIR_REORDERED	0x50
#define COMMANDIR_READY		0x51
#define COMMANDIR_STOPPED	0x52
#define COMMANDIR_POLL_FASTER	0x53
#define COMMANDIR_POLL_SLOWER	0x54

#define SETTRANSMITTERS_1	0xf0
#define SETTRANSMITTERS_2	0xf1
#define SETTRANSMITTERS_3	0xf2
#define SETTRANSMITTERS_4	0xf3
#define SETTRANSMITTERS_5	0xf4
#define SETTRANSMITTERS_6	0xf5
#define SETTRANSMITTERS_7	0xf6
#define SETTRANSMITTERS_8	0xf7
#define SETTRANSMITTERS_9	0xf8
#define SETTRANSMITTERS_10	0xf9
#define SETTRANSMITTERS_11	0xfa
#define SETTRANSMITTERS_12	0xfb
#define SETTRANSMITTERS_13	0xfc
#define SETTRANSMITTERS_14	0xfd
#define SETTRANSMITTERS_15	0xfe
#define SETTRANSMITTERS_16	0xff

// What's in a returning data packet
#define COMMANDIR_RX_EVENTS 		0x02
#define COMMANDIR_RX_DATA			0x01


/**********************************************************************
 *
 * internal function prototypes
 *
 **********************************************************************/

struct send_tx_mask {
	unsigned char numBytes[2];
	unsigned char idByte;
	unsigned long new_tx_mask;
};



struct tx_signal
{
	char * raw_signal;
	int raw_signal_len;
	int raw_signal_tx_bitmask;
	int * bitmask_emitters_list;
	int num_bitmask_emitters_list;
	int raw_signal_frequency;
	struct tx_signal * next;
};

struct commandir_3_tx_signal
{
	unsigned short tx_bit_mask1;
	unsigned short tx_bit_mask2;
	unsigned short tx_min_gap;
	unsigned short tx_signal_count;
	unsigned short pulse_width;
	unsigned short pwm_offset;
};

struct commandir_device
{
	usb_dev_handle *cmdir_udev;
	int interface;
	int hw_type;
	int hw_revision;
	int hw_subversion;
	int busnum;
	int devnum;
	int endpoint_max[4];
	int num_transmitters;
	int num_receivers;
	int num_timers;
	int tx_jack_sense;
	unsigned char rx_jack_sense;
	unsigned char rx_data_available;
	
	int * next_enabled_emitters_list;
	int num_next_enabled_emitters;
	char signalid;
	
	struct tx_signal * next_tx_signal;
	struct tx_signal * last_tx_signal;
	
	unsigned char lastSendSignalID;
	unsigned char commandir_last_signal_id;
	unsigned char flush_buffer;
	
	// CommandIR Mini Specific:
	int mini_freq;
	
	unsigned char commandir_tx_start[MAX_TX_TIMERS*4];
	unsigned char commandir_tx_end[MAX_TX_TIMERS*4];
	unsigned char commandir_tx_available[MAX_TX_TIMERS];
	unsigned char tx_timer_to_channel_map[MAX_TX_TIMERS];
	
	struct commandir_device * next_commandir_device;
};

struct commandirIII_status {
	unsigned char jack_status[4];
	unsigned char rx_status;
	unsigned char tx_status;
	unsigned char versionByte;
	unsigned char expansionByte;
};

static void hardware_disconnect(struct commandir_device * a);

/*** Parent Thread Functions ***/
static int commandir_init();
static int commandir_send(struct ir_remote *remote,struct ir_ncode *code);
static char *commandir_rec(struct ir_remote *remotes);
static int commandir_ioctl(unsigned int cmd, void *arg);
static lirc_t commandir_readdata(lirc_t timeout);
static int commandir_deinit(void);
static int commandir_receive_decode(struct ir_remote *remote,
                   ir_code *prep,ir_code *codep,ir_code *postp,
                   int *repeat_flagp,
                   lirc_t *min_remaining_gapp, lirc_t *max_remaining_gapp);

/*** USB Thread Functions ***/
static void commandir_child_init();
int do_we_know_device(unsigned int bus_num, int devnum);
int claim_and_setup_commandir(unsigned int bus_num, int devnum, 
	struct usb_device *dev);
static void hardware_scan();
static void hardware_setorder();
static void hardware_disconnect(struct commandir_device * a);
static void software_disconnects();
static void set_detected(unsigned int bus_num, int devnum);
static void commandir_read_loop();
static void shutdown_usb();

/*** Processing Functions ***/
static void add_to_tx_pipeline(unsigned char *buffer, int bytes,
	unsigned int frequency);
static int check_irsend_commandir(unsigned char *command);
static void recalc_tx_available(struct commandir_device * pcd);
static int cmdir_convert_RX(unsigned char *orig_rxbuffer);
static int commandir2_convert_RX(unsigned short *bufferrx,
	unsigned char numvalues);
static void pipeline_check(struct commandir_device * pcd);
static void commandir_2_transmit_next(struct commandir_device * pcd);

static int get_hardware_tx_bitmask(struct commandir_device * pcd);

static void set_convert_int_bitmask_to_list_of_enabled_bits(
	unsigned long * bitmask, int bitmask_len);
static void set_all_next_tx_mask(int * ar_new_tx_mask, int new_tx_len, 
	unsigned long bitmask);
static void set_new_signal_bitmasks(struct commandir_device * pcd, 
	struct tx_signal * ptx);

static void update_tx_available(struct commandir_device * pcd);
static int commandir_read();

/** CommandIR III Specific **/
static int commandir3_convert_RX(unsigned char *rxBuffer,
	int numNewValues);
static void raise_event(unsigned int eventid);

#define MAX_FIRMWARE_PACKET 64
#define MAX_RX_PACKET 512

#define MAX_INCOMING_BUFFER 1024

#define RX_MODE_INTERNAL	1
#define RX_MODE_XANTECH		2
#define RX_MODE_HAUPPAUGE	3
#define REC_TIMESTAMPS 5

struct pulse_timestamps {
	unsigned char idbyte;
	unsigned short pca_fall_at[REC_TIMESTAMPS];
	unsigned short pca_rise_at[REC_TIMESTAMPS];
	unsigned short PCA_overflow_counter[REC_TIMESTAMPS];
};

#define USB_RX_PULSE_DEF 31
#define USB_RX_PULSE 32
#define USB_RX_SPACE 33
#define USB_RX_PULSE_DEMOD 34
#define USB_RX_SPACE_DEMOD 35
#define USB_NO_DATA_BYTE 36

struct commandir3_tx_signal
{
	unsigned short tx_bit_mask1;
	unsigned short tx_bit_mask2;
	unsigned short tx_min_gap;
	unsigned short tx_signal_count;
	unsigned short pulse_width;
	unsigned short pwm_offset;
};



struct usb_rx_space3 {
 unsigned short pca_overflow_count;
 unsigned short pca_offset;
};

struct usb_rx_pulse3 {
 unsigned short t0_count;
};

struct usb_rx_pulse_def3 {
 unsigned short frequency;
 unsigned short pwm;
};

struct usb_rx_demod_pulse {
 unsigned short pca_overflow_count;
 unsigned short pca_offset;
};

#endif
