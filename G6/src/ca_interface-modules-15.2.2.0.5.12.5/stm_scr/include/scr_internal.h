#ifndef _STM_SCR_INTERNAL_H
#define _STM_SCR_INTERNAL_H

#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif
#include "linux/sched.h"
#include "ca_platform.h"
#include "ca_os_wrapper.h"
#include "ca_queue.h"
#include "stm_asc_scr.h"
#include "ca_debug.h"
#include "scr_plugin.h"
#include "stm_event.h"

#define VALIDATE_HANDLE(x) (x>>8) ^ (0x00ababab)
#define SCR_NO(x) x & 0x000000FF

#define SCR_DEBUG  0
#define SCR_ERROR  1

#if SCR_ERROR
#define scr_error_print(fmt, ...)		printk(fmt,  ##__VA_ARGS__)
#else
#define scr_error_print(fmt, ...)		no_print(fmt,  ##__VA_ARGS__)
#endif

#if SCR_DEBUG
#define scr_debug_print(fmt, ...)		printk(fmt,  ##__VA_ARGS__)
#else
#define scr_debug_print(fmt, ...)		no_print(fmt,  ##__VA_ARGS__)
#endif

typedef struct stm_scr_s scr_ctrl_t;

/*****************DEFAULT******************************/
#define SCR_INITIAL_BAUD_RATE             9600
#define SCR_INITIAL_ETU                   372
#define SCR_INITIAL_Fi                   SCR_INITIAL_ETU
#define SCR_INITIAL_Di                    1
#define SCR_INITIAL_GUARD_DELAY           2
#define SCR_DEFAULT_TIMEOUT               1010
#define SCR_DEFAULT_N                     2
#define SCR_DEFAULT_F                     1
#define SCR_DEFAULT_D                     1
#define SCR_DEFAULT_W                     10
/*****************************************************/
#define FiRFU    372	       /* Set to Default */
#define FmRFU  4000000	       /* Set to default */
#define DiRFU   1	      /* Set to default */



/*******************CLOCK*****************************/
#define SCR_CLOCK_ENABLE_MASK           0x2
#define SCR_CLOCK_SOURCE_MASK           0x1
#define SCR_CLOCK_SOURCE_COMMS        	0x0
#define SCR_CLOCK_SOURCE_EXTERNAL    	0x1
#define SCR_CLOCK_VALUE_OFFSET          0x00
#define SCR_CLOCK_CONTROL_OFFSET        0x04
/******************************************************/

//This gpio is handled seperatly as this CA specific
#define	SCR_CLASS_SELECTION_GPIO	0x0

typedef enum {
	SCR_CLK_GPIO = 0,
	SCR_RESET_GPIO,
	SCR_VCC_GPIO,
	SCR_DETECT_GPIO,
	SCR_EXTCLK_GPIO,
	SCR_MAX_GPIO
} gpio_index;


/****************STATE MACHINE DATA********************/
typedef enum {
	SCR_THREAD_STATE_IDLE = 0,
	SCR_THREAD_STATE_CARD_NOT_PRESENT,
	SCR_THREAD_STATE_DEACTIVATE,
	SCR_THREAD_STATE_CARD_PRESENT,
	SCR_THREAD_STATE_PROCESS,
	SCR_THREAD_STATE_DELETE,
	SCR_THREAD_STATE_LOW_POWER
} scr_thread_state_t;

typedef enum {
	SCR_PROCESS_CMD_INVALID = 0,
	SCR_PROCESS_CMD_TRANSFER,
	SCR_PROCESS_CMD_PPS,
	SCR_PROCESS_CMD_RESET,
	SCR_PROCESS_CMD_ABORT,
	SCR_PROCESS_CMD_IFSC,
} scr_process_command_t;

typedef struct {
	ca_os_semaphore_t sema;
	scr_thread_state_t next_state;
	scr_ctrl_t *scr_handle_p;
	scr_process_command_t type;
	void *data_p;
} scr_message_t;

struct scr_cdev_data_s {
	dev_t devt;
	struct cdev cdev;
	struct device *cdev_class_dev;
};
/**********************************************************************************/
struct scr_platform_data_s {
	struct stm_scr_platform_data *scr_platform_data;
	struct platform_device *asc_platform_device;
	struct resource *base_address;
	uint32_t id;
	struct clk *clk;
	struct clk *clk_ext;
	bool init;
	struct platform_device *pdev;
	struct stm_device_state *device_state;
	struct scr_cdev_data_s device_data;
	struct scr_cdev_data_s ca_device_data;
	struct file_operations *fops;
	int scr_major;
	struct class *scr_class;
};

typedef union {
	uint8_t crc[2];
	uint8_t lrc;
} scr_epilogue_t;
typedef struct {
	uint32_t block_waiting_timeout;
	uint32_t character_waiting_timeout;
	uint32_t block_guard_timeout;
	uint32_t block_multiplier;
} scr_protocol_t1_timeout_t;
typedef struct {
	uint8_t nad;
	uint8_t pcb;
	uint8_t len;
} scr_t1_header_t;

typedef struct {
	scr_ctrl_t *scr_handle_p;
	stm_scr_transfer_params_t  *trans_params_p;
	uint32_t bytes_read;
	uint32_t bytes_last_read;
	uint32_t bytes_written;
	uint32_t bytes_last_written;
	scr_protocol_t1_timeout_t timeout_t1;
	uint32_t next_transfer_time;
	uint8_t nad;
	uint8_t pcb;
	uint32_t rc ;
	bool our_sequence;
	bool their_sequence;
	uint8_t ifsc;
	bool first_block;
} scr_t1_t;

typedef struct {
	scr_t1_header_t header;
	uint8_t data[256];
	scr_epilogue_t scr_epilogue;
} scr_t1_block_t;

typedef struct {
	uint32_t intital_etu_ca;
	uint32_t intital_baudrate_ca;
	bool flow_control_ca;
	bool nack_control_ca;
	stm_asc_scr_databits_t parity_ca;
} scr_ca_data_t;

/*******************control block*************************************/
struct stm_scr_s {
	uint32_t  magic_num;                /* To validate the correct handle and also hold the scr_num  0xababab00 | scr_num */
	uint32_t base_address_physical;
	unsigned char __iomem *base_address_virtual;
	struct scr_platform_data_s *scr_plat_data;
#ifdef CONFIG_ARCH_STI
	struct pinctrl	*pinctrl;
	unsigned int class_sel_gpio;
	unsigned int reset_gpio;
	unsigned int vcc_gpio;
	unsigned int detect_gpio;
	struct pinctrl_state		*pins_state;
	enum scr_gpio_dir gpio_direction;
#else
	struct stm_pad_config *pad_config;
	struct stm_pad_config *class_sel_pad_config;

	struct stm_pad_state * pad_state;
	struct stm_pad_state *class_sel_pad_state;
	enum stm_pad_gpio_direction gpio_direction;
	struct stm_pad_gpio *asc_pad_p;
	struct stm_device_state *device_state;
#endif
	uint32_t  clock_source;
	stm_scr_capabilities_t scr_caps;
	uint32_t      new_inuse_freq;
	scr_t1_t  t1_block_details;
	uint32_t  baud_rate;        /* Baud rate  for the uart */
	uint32_t  retries;
	uint8_t N;      /* Extra guard time */
	bool reset_done;
	bool detect_inverted;
	scr_thread_state_t state;
	uint32_t last_status;

	/* One stop shop for all the info in atr,*/
	/* This will the global TS,T0,TA1,TB1,TA2,History bytes */
	/* Kind of duplicacy for Capabilities but good for debugging */
	scr_atr_response_t  atr_response;

	bool reset_retry;

	int8_t nad;
	bool flow_control;
	bool nack;
	stm_scr_protocol_t current_protocol; /* Current protocol used */
	scr_pps_data_t pps_data;


	stm_scr_gpio_t asc_gpio;
	stm_asc_h  asc_handle;  /* Handle to the ASC driver ,to be used in all further communication to the ASC*/
	stm_asc_scr_protocol_t asc_protocol;
	uint32_t asc_fifo_size;
	bool user_forced_parameters;
	ca_os_thread_t thread;          /* Thread control pointer*/
	bool	card_present;
	ca_os_message_q_t *message_q; /* Message queue for the thread to wait */
	struct proc_dir_entry *scr_dir;
	char owner[20];
	struct scr_ca_ops_s ca_ops;
	struct scr_ca_param_s ca_param;
	bool reset_frequency_set;
	/* indicated value of the clock rate conversion integer */
	uint32_t	fi;
	/* the indicated value of the baud rate adjustment integer */
	uint32_t	di;
	/*wait queue for poll for user space*/
	/*not use for kernel space calls*/
	wait_queue_head_t queue;
	/*event subscription for user space signalling through poll*/
	stm_event_subscription_h subscription;
	bool event_occurred;
	/*power data*/
	uint32_t reset_frequency;

	uint32_t wwt_tolerance;/* in ms */

	int32_t last_error;
};
/********************************************************************************/
int32_t scr_internal_new(uint32_t scr_dev_id,
				stm_scr_device_type_t device_type,
				stm_scr_h  *handle_p);

int32_t scr_internal_delete(scr_ctrl_t *scr_handle_p );

int32_t scr_internal_deactivate(scr_ctrl_t *scr_handle_p);

int32_t scr_internal_cold_reset(scr_ctrl_t *scr_handle_p);
int32_t scr_internal_warm_reset(scr_ctrl_t *scr_handle_p);

int32_t scr_internal_get_ctrl(scr_ctrl_t *scr_handle_p,
				stm_scr_ctrl_flags_t  ctrl_flags,
				uint32_t *value_p);
int32_t scr_internal_set_ctrl(scr_ctrl_t *scr_handle_p,
				stm_scr_ctrl_flags_t  ctrl_flags,
				uint32_t value);

int32_t scr_internal_read(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p);
int32_t scr_internal_write(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p);
int32_t scr_internal_transfer(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p);

int32_t scr_internal_reset(scr_ctrl_t *scr_handle_p, uint32_t reset_type);
int32_t scr_internal_pps(scr_ctrl_t *scr_handle_p, uint8_t *data_p);

void scr_populate_clock_default(scr_ctrl_t *scr_handle_p);
void scr_internal_change_class(scr_ctrl_t *scr_handle_p);
uint8_t scr_internal_read_class_sel_pad(scr_ctrl_t *scr_handle_p);
void scr_populate_default(scr_ctrl_t *scr_handle_p);

struct scr_platform_data_s *scr_get_platform_data(uint32_t scr_num);

void scr_thread(void *params) ;

uint32_t scr_read_atr(scr_ctrl_t *scr_handle_p);
uint32_t scr_internal_send_ifsc(scr_ctrl_t *scr_handle_p, uint8_t data);

uint32_t scr_transfer_t14(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p);
void scr_disable_irq(scr_ctrl_t *scr_handle_p);
void scr_enable_irq(scr_ctrl_t *scr_handle_p, uint32_t comp);
bool scr_card_present(scr_ctrl_t *scr_handle_p);

extern uint32_t (*scr_ca_transfer_t14)(scr_ctrl_t *scr_handle_p,
			stm_scr_transfer_params_t  *trans_params_p);

extern uint32_t (*scr_ca_populate_init_parameters)(scr_ctrl_t *scr_handle_p);

extern uint32_t (*scr_ca_recalculate_parameters)(scr_ctrl_t *scr_handle_p);

extern uint32_t (*scr_ca_read) (scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable);

extern uint32_t (*scr_ca_set_parity)(stm_asc_scr_databits_t *data_bit_p);

extern uint32_t (*scr_ca_recalculate_etu)(scr_ctrl_t *scr_handle_p,
					uint8_t f_int,
					uint8_t d_int);

extern uint32_t (*scr_ca_update_fi_di)(scr_ctrl_t *scr_handle_p,
					uint8_t f_int,
					uint8_t d_int);
extern uint32_t (*scr_ca_enable_clock)(scr_ctrl_t *scr_handle_p);
struct file_operations *scr_get_fops(void);
extern stm_asc_scr_stopbits_t (*scr_ca_set_stopbits)(scr_ctrl_t *scr_handle_p);
extern int (*scr_redo_reset)(scr_ctrl_t *scr_p, uint32_t *data_p);
extern void (*scr_ca_gpio_direction)(scr_ctrl_t *scr_handle_p);
extern struct scr_platform_data_s *get_platform_data(void);
#endif
