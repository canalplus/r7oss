#ifndef	_STM_CI_INTERNAL_H
#define	_STM_CI_INTERNAL_H


#include "ca_os_wrapper.h"
#include "ca_platform.h"
#include "stm_ci.h"
#include "ci_debug.h"

#define	MAGIC_NUM_MASK			0xaabbcc00
#define	CI_VALIDATE_HANDLE(magic_num)	((magic_num >> 8) ^ (0x00aabbcc))
#define	CI_NO(x)			(x & 0x000000FF)

/* Configurable	using proc/sysfs */
#define	CI_DATA_AVAILABLE_TIMEOUT  (HZ)	/* in ms */
#define	CI_CAM_FREE_TIMEOUT   (HZ*2)  /*  in ms	*/

/*****************DEFAULT Constants******************************/
/****************STATE MACHINE DATA********************/
typedef	enum ci_state_e {
	CI_STATE_OPEN = 0,
	CI_STATE_CARD_NOT_PRESENT,
	CI_STATE_CARD_PRESENT,
	CI_STATE_CARD_READY,	/* buffer negotaition */
	CI_STATE_PROCESS,
	CI_STATE_CLOSE
} ci_state_t;

/*Enum for CI Hardware details*/
struct ci_hw_data_s {
	struct stm_ci_platform_data *platform_data;
	struct platform_device *pdev;
	struct resource	*ci_base;
	struct resource	*epld_base;
	struct resource	*iomem_offset;
	struct resource	*attribute_mem_offset;
	struct resource *emi_base_addr;
	bool init;
	int dev_id;
};

/*Enum for CI Access mode*/
typedef	enum {
	CI_ATTRIB_ACCESS = 0,
	CI_IO_ACCESS,
	CI_COMMON_ACCESS
} ci_access_mode_t;

/*Enum for CI Vcc Type*/
typedef	enum {
	CI_VCC_NONE = 0,
	CI_VCC,
	CI_VCC_DIRECT,
	CI_VCC_MICREL,
	CI_VCC_ALT_VOLT_EN
} ci_vcc_type_t;
/*Enum for CI CAM Voltage */
typedef	enum {
	CI_VOLT_5000 = 0,
	CI_VOLT_3300
} ci_voltage_t;
/*Structure for	CI Interface data*/
typedef	union {
	struct {
		struct stm_pad_state *pad_state;
	} pad;
	struct {
		struct i2c_adapter *i2c_adap;
		uint8_t	 ci_i2c_addr;
	} i2c;
	struct {
		struct i2c_adapter *i2c_adap;
		uint8_t	 ci_i2c_addr;
		uint8_t	 cam_ctrl_value;
	} starwin;
} ci_info_t;
/* CI function ptrs*/
struct ci_dvb_ca_en50221 {

	/* Functions for controlling slots */
	int (*slot_reset)(struct ci_dvb_ca_en50221 *ca);
	int (*slot_status)(struct ci_dvb_ca_en50221 *ca,
				uint16_t *card_status_p);
	int (*slot_power_enable)(struct	ci_dvb_ca_en50221 *ca,
				ci_voltage_t voltage);
	int (*slot_power_disable)(struct ci_dvb_ca_en50221 *ca);
	int  (*switch_access_mode)(struct ci_dvb_ca_en50221 *ca,
				ci_access_mode_t  access_mode);
	int  (*slot_enable)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_init)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_shutdown)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_detect_irq_install)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_detect_irq_uninstall)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_detect_irq_enable)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_detect_irq_disable)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_data_irq_install)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_data_irq_uninstall)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_data_irq_enable)(struct ci_dvb_ca_en50221 *ca);
	int  (*slot_data_irq_disable)(struct ci_dvb_ca_en50221 *ca);
	/* private data, used by caller	*/
	void *data;
};
struct ci_hw_addr {
	uint32_t	base_addr_phy;
	uint32_t	epld_addr_phy;
	uint32_t	io_addr_phy;
	uint32_t	attr_addr_phy;
	unsigned char __iomem	*base_addr_v;
	unsigned char __iomem	*epld_addr_v;
	unsigned char __iomem	*io_addr_v;
	unsigned char __iomem	*attr_addr_v;
};
struct ci_device {
	bool		data_irq_enabled;
	/* DA interrupt enabled for CIPLUS*/
	bool		daie_set;
	/* FR interrupt enabled for CIPLUS*/
	bool		frie_set;
	bool		link_initialized;
	bool		cam_present;/* Configuration registers base address */
	uint32_t	cor_address;
	/*Index value written to COR to enable configuration*/
	uint8_t		configuration_index;
	/* IO or Memory Access */
	ci_access_mode_t	access_mode;
	stm_ci_capabilities_t	ci_caps;
	ci_voltage_t		cam_voltage;
	uint8_t			*cis_buf_p;
	ci_epld_intr_reg	epld_reg;
};
/*******************control block*************************************/
typedef	struct stm_ci_s {
	/* To validate the correct handle and also hold	the ci_num  0xababab00 | ci_num	*/
	uint32_t		magic_num;
	spinlock_t		lock;
	ca_os_semaphore_t	ctrl_sem_p;
	ca_os_mutex_t	lock_ci_param_p;
	struct ci_hw_data_s	*ci_hw_data_p; /*this contains signal map */
	/* pointer back	to the public data structure */
	struct ci_dvb_ca_en50221	ca;
	struct ci_hw_addr	hw_addr;
	ci_state_t		currstate;
	ci_vcc_type_t		vcc_type;	     /*	IO or Memory Access */
	struct ci_device	device;
	struct proc_dir_entry	*ci_dir;
	ca_os_timeout_t	da_timeout;
	ca_os_timeout_t	fr_timeout;
	bool			ciplus_in_polling;
	ci_signal_map		sig_map[CI_SIGNAL_MAX +	1];
	ci_info_t		ci_info[CI_SIGNAL_MAX +	1];
} stm_ci_t;
/*****************Function Prototypes********************************/
ca_error_code_t ci_internal_new(uint32_t dev_id , stm_ci_t *ci_p);
void ci_internal_delete(stm_ci_t *ci_p);

ca_error_code_t ci_internal_link_init(stm_ci_t *ci_p);
ca_error_code_t ci_internal_card_configure(stm_ci_t	*ci_p);

ca_error_code_t ci_internal_read(stm_ci_t *ci_p,
				    stm_ci_msg_params_t  *params_p);

ca_error_code_t ci_internal_read_cis(stm_ci_t *ci_p,
				unsigned int	*actual_cis_size,
				unsigned char	*cis_buf_p);

ca_error_code_t ci_internal_write(stm_ci_t *ci_p,
				     stm_ci_msg_params_t  *params_p);


ca_error_code_t  ci_internal_get_slot_status(stm_ci_t *ci_p,
				bool *is_present);

ca_error_code_t ci_internal_set_command(stm_ci_t	       *ci_p,
					   stm_ci_command_t	command,
					   uint8_t		value);

ca_error_code_t ci_internal_get_status(stm_ci_t	      *ci_p,
					  stm_ci_status_t     status,
					  uint8_t	      *value_p);

ca_error_code_t ci_internal_soft_reset(stm_ci_t *ci_p);
struct ci_hw_data_s *ci_get_paltform_data(uint32_t ci_num);

ca_error_code_t ci_check_state(stm_ci_t *ci_p, ci_state_t desired_state);

ca_error_code_t waitforFR(stm_ci_t *ci_p);
int32_t	ci_register_char_driver(void);

void  ci_unregister_char_driver(void);
#endif
