/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_cec is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_cec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ca_platform.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#ifndef _CA_PLATFORM_H_
#define _CA_PLATFORM_H_

struct scr_config_data{
	unsigned int	address;
	unsigned int	mask;
};

/* CI structures */
struct ci_config_data{
	unsigned int	address;
	unsigned int	mask;
};
enum ci_signal_interface {
	CI_UNKNOWN_INTERFACE,
	CI_SYSIRQ,
	CI_EPLD,
	CI_GPIO,
	CI_IIC,
	CI_STARCI2WIN,
	CI_NEW_INTERFACE,
	CI_EMI
};

enum ci_signal_type{
    CI_SIGNAL_IREQ,
    CI_SIGNAL_IOIS16,
    CI_SIGNAL_CD1,
    CI_SIGNAL_CD2,
    CI_SIGNAL_RST,
    CI_SIGNAL_VS1,
    CI_SIGNAL_VS2,
    CI_SIGNAL_SEL,          /* for CI_EN or POD_SEL */
    CI_SIGNAL_VCC_EN,       /* can be used for direct mapped */
    CI_SIGNAL_VCC_3_5EN,
    CI_SIGNAL_VCC_MICREL_Vcc5EN,
    CI_SIGNAL_VCC_MICREL_Vcc3EN,
    CI_SIGNAL_VCC_MICREL_EN0,
    CI_SIGNAL_VCC_MICREL_EN1,
    CI_SIGNAL_MAX = CI_SIGNAL_VCC_MICREL_EN1
};

typedef struct{

    uint32_t       reset_set_addr;
    uint32_t       reset_set_bitmask;
    uint32_t       reset_clear_addr;
    uint32_t       reset_clear_bitmask;
    uint32_t       cd_status_addr;
    uint32_t       cd_status_bitmask;
    uint32_t       data_status_addr;
    uint32_t       data_status_bitmask;
    uint32_t       clear_addr;
    uint32_t       clear_bitmask;
    uint32_t       mask_addr;
    uint32_t       mask_bitmask;
    uint32_t       priority_addr;
    uint32_t       priority_bitmask;
}ci_epld_intr_reg;

struct i2c_interface{
    uint8_t	 number; 	/* check this struct stm_pad_config *pad_config; */
    uint32_t     addr;
};

typedef struct epld_interface{
    uint32_t                  addr;
    ci_epld_intr_reg   epld_int_reg;
}epld_interface_t;

typedef union ci_interface_u{
    uint8_t					null_config;
    struct i2c_interface     		i2c_config;
    epld_interface_t 	     		epld_config;
#ifdef CONFIG_ARCH_STI
    uint32_t			gpio;
#else
    struct stm_pad_config       *pad_config;
#endif
}ci_interface_t;

typedef struct ci_signal_map_s{
	enum ci_signal_type         	sig_type;
	enum ci_signal_interface    	interface_type;
	bool                       			 active_high;
	uint32_t                    		bit_mask;
	uint32_t                    		interrupt_num;
	ci_interface_t              		data;
}ci_signal_map;

typedef struct ci_emi_bank_data_s{
	int bank_id;
	unsigned long emi_config_data[4] ;
	int mode ;
	uint32_t addrbits;
}ci_emi_bank_data;

enum ci_emi_bank_mode{
	DVBCI_MODE,
	EPLD_MODE
};

struct stm_ci_platform_data {
	unsigned int		 num_config_data;
	struct ci_config_data   *config_data;
	ci_signal_map	        *sigmap;
	ci_emi_bank_data	*emi_config;
#ifdef CONFIG_ARCH_STI
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
#else
	struct stm_pad_config	*dvbci_pad_config;
	struct stm_pad_state	*dvbci_pad_state;
#endif
	int			nr_banks;
	uint32_t		chip_sel;
	uint32_t		chip_sel_offset;
	uint32_t		epld_en_flag;
};

/*** SCR platform data ***/
#ifdef CONFIG_ARCH_STI

enum scr_gpio_dir{
	GPIO_IN = 0,
	GPIO_OUT,
	GPIO_BIDIR
};

/* Regfield IDs */
enum {
	CLK_MUL_SEL = 0,
	INPUT_CLK_SEL = 1,
	VCC_ENABLE = 2,
	DETECT_POL = 3,
	/* keep last */
	MAX_REGFIELDS
};

#define SRC407_OFFSET	0x20

struct st_scr_data {
	uint32_t reg_offset;
	uint32_t *reg_fields;
};

struct stm_scr_platform_data {
	struct platform_device *asc;
	unsigned int num_config_data;
	struct pinctrl	*pinctrl;
	struct pinctrl_state	*pins_default;
	int class_sel_gpio;
	int reset_gpio;
	int vcc_gpio;
	int detect_gpio;
	struct regmap *regmap;
	struct st_scr_data *scr_config;
	struct scr_config_data *config_data;
	char * clock_id;
	unsigned int asc_fifo_size;
};

struct stm_asc_gpio {
	bool		gpio_mode;
	bool		gpio_requested;
	uint32_t	gpio_num;
	enum scr_gpio_dir gpio_direction;
};

struct stm_plat_asc_data {
	struct pinctrl	*pinctrl;
	struct pinctrl_state *pins_state;
	struct pinctrl_state *pins_default;
	char *config_name;
	bool			c4_selected;
	bool			c7_selected;
	bool			c8_selected;
	struct stm_asc_gpio	*c4_pin;
	struct stm_asc_gpio	*c7_pin;
	struct stm_asc_gpio	*c8_pin;
};
#else

struct stm_scr_platform_data {
	struct platform_device *asc;
	struct stm_pad_config *pad_config;
	struct stm_pad_config *class_sel_pad;
	unsigned int num_config_data;
	struct scr_config_data *config_data;
	struct stm_device_config *device_config;
	char * clock_id;
	unsigned int asc_fifo_size;
};
#endif
#endif
