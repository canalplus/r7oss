/*
 * arch/arm/mach-stm-b2044/stm_fe_conf.h
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Subrata Chatterjee <subrata.chatterjee@st.com>
 *
 * Contents: Frontend Configuration. Refer help towards the end this file
 * for different NIM settings.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef _STM_FE_CONF_H
#define _STM_FE_CONF_H

#define STM_FE_CONF_VERSION 1

//#define NIM_STV0900_STV6120_LNBH24_VGLNA /* For stv0900 dual path */
//#define NIM_STV0900_STV6120_LNBH24_NO_VGLNA
//#define NIM_STV0900_STV6110_LNBH24
//#define NIM_STV0903_STV6110_LNBH24
//#define NIM_STV0903_STV6120_LNBH24_VGLNA
//#define NIM_STV0903_STV6120_LNBH24_NO_VGLNA
#define NIM_STV0910_STV6120_LNBH26
#define NIM_STV0367_STV4100
//#define NIM_STV0367_DTT7546	/* For STV0367+DTT7546 T/C */
//#define NIM_PACKET_INJECTOR /* DB580A packet injector in NIM A slot */
//#define NIM_TSSERVER_INSTANCE_0
//#define NIM_TSSERVER_INSTANCE_1
//#define NIM_TSSERVER_INSTANCE_2
//#define NIM_TSSERVER_INSTANCE_3
//#define NIM_TSSERVER_INSTANCE_4
//#define NIM_TSSERVER_INSTANCE_5
//#define NIM_CONFIG_INSTANCE_0
//#define NIM_CONFIG_INSTANCE_1
//#define NIM_CONFIG_INSTANCE_2
#define NIM_CONFIG_INSTANCE_3
#define NIM_CONFIG_INSTANCE_4
#define NIM_CONFIG_INSTANCE_5
#define NIM_CONFIG_INSTANCE_6
#define NIM_CONFIG_INSTANCE_7

#define IPFE_CONFIG_INSTANCE_0
#define IPFE_CONFIG_INSTANCE_1
#define IPFE_CONFIG_INSTANCE_2
#define IPFE_CONFIG_INSTANCE_3
#define IPFE_CONFIG_INSTANCE_4
#define IPFE_CONFIG_INSTANCE_5
//#define IPFE_FEC_CONFIG_INSTANCE_0
//#define IPFE_FEC_CONFIG_INSTANCE_1
//#define IPFE_FEC_CONFIG_INSTANCE_2
/*#define IPFE_FEC_CONFIG_INSTANCE_3
  #define IPFE_FEC_CONFIG_INSTANCE_4
  #define IPFE_FEC_CONFIG_INSTANCE_5*/

/* stm_fe configuration for instance 0 */
#ifdef NIM_STV0903_STV6110_LNBH24
#define STM_FE_DEMOD_NAME_0                   "STV090x-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6110-STM_FE\0"
#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV090x-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               0
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0 /*TSIN ID for NIMA*/
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_STV0903_STV6120_LNBH24_VGLNA
#define STM_FE_DEMOD_NAME_0                   "STV090x-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6120_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               0
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         (DEMOD_CUSTOM_IQ_WIRING_INVERTED |\
                                              DEMOD_CUSTOM_ADD_ON_VGLNA)
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV090x-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_STV0903_STV6120_LNBH24_NO_VGLNA
#define STM_FE_DEMOD_NAME_0                   "STV090x-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6120_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               0
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV090x-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_STV0367_STV4100
/* STV0367C-STM_FE for Cable only; STV0367T-STM_FE for Terrestial only;
  * STV0367-STM_FE for Terrestial and Cable mutually exclusive.
  */
#define STM_FE_DEMOD_NAME_2                   "STV0367T-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_2                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_2              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_2             2
#define STM_FE_DEMOD_I2C_BAUD_2               400000
#define STM_FE_DEMOD_ADDRESS_2                0x1D
#define STM_FE_DEMOD_TUNER_NAME_2             "STV4100-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_2          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_2        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_2       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_2         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_2          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_2            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_2       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_2  1
#define STM_FE_DEMOD_DEMOD_CLOCK_2            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_2          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_2               0
#define STM_FE_DEMOD_TS_TAG_2                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_2          1 /*TSIN ID for NIMB*/
#define STM_FE_DEMOD_CUSTOMISATIONS_2         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_2               0
#define STM_FE_DEMOD_ROLL_OFF_2               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_2              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_2             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_2          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_2        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_2       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_2      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_2         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_2      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_2                     "\0"
#define STM_FE_LNB_IO_TYPE_2                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_2                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_2               2
#define STM_FE_LNB_I2C_BAUD_2                 100000
#define STM_FE_LNB_ADDRESS_2                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_2           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_2          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_2           0
#define STM_FE_LNB_TONE_SEL_PIN_2             0

#define STM_FE_DISEQC_NAME_2                  "\0"
#define STM_FE_DISEQC_VERSION_2               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_2        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_2                 0
#define STM_FE_TS_REMOTE_PORT_2               0
#endif

#ifdef NIM_STV0367_DTT7546
/* STV0367C-STM_FE for Cable only; STV0367T-STM_FE for Terrestial only;
  * STV0367-STM_FE for Terrestial and Cable mutually exclusive.
  */
#define STM_FE_DEMOD_NAME_2                   "STV0367T-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_2                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_2              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_2             2
#define STM_FE_DEMOD_I2C_BAUD_2               400000
#define STM_FE_DEMOD_ADDRESS_2                0x1D
#define STM_FE_DEMOD_TUNER_NAME_2             "DTT7546-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_2          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_2        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_2       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_2         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_2          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_2            27000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_2       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_2  1
#define STM_FE_DEMOD_DEMOD_CLOCK_2            27000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_2          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_2               0
#define STM_FE_DEMOD_TS_TAG_2                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_2          1 /*TSIN ID for NIMB*/
#define STM_FE_DEMOD_CUSTOMISATIONS_2         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_2               36166
#define STM_FE_DEMOD_ROLL_OFF_2               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_2              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_2             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_2          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_2        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_2       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_2      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_2         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_2      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_2                     "\0"
#define STM_FE_LNB_IO_TYPE_2                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_2                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_2               0
#define STM_FE_LNB_I2C_BAUD_2                 100000
#define STM_FE_LNB_ADDRESS_2                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_2           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_2          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_2           0
#define STM_FE_LNB_TONE_SEL_PIN_2             0

#define STM_FE_DISEQC_NAME_2                  "\0"
#define STM_FE_DISEQC_VERSION_2               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_2        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_2                 0
#define STM_FE_TS_REMOTE_PORT_2               0
#endif

#ifdef NIM_PACKET_INJECTOR
#define STM_FE_DEMOD_NAME_0                   "DUMMY_OFDM\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x1e
#define STM_FE_DEMOD_TUNER_NAME_0             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  2
#define STM_FE_DEMOD_DEMOD_CLOCK_0            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               1
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0 /*TSIN ID for NIM A*/
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_0
#define STM_FE_DEMOD_NAME_0                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x1e
#define STM_FE_DEMOD_TUNER_NAME_0             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  2
#define STM_FE_DEMOD_DEMOD_CLOCK_0            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_0               1
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_1
#define STM_FE_DEMOD_NAME_1                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               400000
#define STM_FE_DEMOD_ADDRESS_1                0x1e
#define STM_FE_DEMOD_TUNER_NAME_1             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_1            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  2
#define STM_FE_DEMOD_DEMOD_CLOCK_1            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_1               1
#define STM_FE_DEMOD_TS_TAG_1                 0x01
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          1
#define STM_FE_DEMOD_CUSTOMISATIONS_1         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_2
#define STM_FE_DEMOD_NAME_2                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_2                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_2              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_2             2
#define STM_FE_DEMOD_I2C_BAUD_2               400000
#define STM_FE_DEMOD_ADDRESS_2                0x1e
#define STM_FE_DEMOD_TUNER_NAME_2             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_2          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_2        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_2       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_2         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_2          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_2            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_2       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_2  2
#define STM_FE_DEMOD_DEMOD_CLOCK_2            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_2          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_2               1
#define STM_FE_DEMOD_TS_TAG_2                 0x02
#define STM_FE_DEMOD_DEMUX_TSIN_ID_2          2
#define STM_FE_DEMOD_CUSTOMISATIONS_2         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_2               0
#define STM_FE_DEMOD_ROLL_OFF_2               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_2              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_2             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_2          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_2        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_2       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_2      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_2         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_2      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_2                     "\0"
#define STM_FE_LNB_IO_TYPE_2                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_2                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_2               2
#define STM_FE_LNB_I2C_BAUD_2                 100000
#define STM_FE_LNB_ADDRESS_2                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_2           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_2          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_2           0
#define STM_FE_LNB_TONE_SEL_PIN_2             0

#define STM_FE_DISEQC_NAME_2                  "\0"
#define STM_FE_DISEQC_VERSION_2               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_2        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_2                 0
#define STM_FE_TS_REMOTE_PORT_2               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_3
#define STM_FE_DEMOD_NAME_3                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_3                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_3              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_3             2
#define STM_FE_DEMOD_I2C_BAUD_3               400000
#define STM_FE_DEMOD_ADDRESS_3                0x1e
#define STM_FE_DEMOD_TUNER_NAME_3             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_3          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_3        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_3       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_3         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_3          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_3            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_3       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_3  2
#define STM_FE_DEMOD_DEMOD_CLOCK_3            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_3          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_3               1
#define STM_FE_DEMOD_TS_TAG_3                 0x03
#define STM_FE_DEMOD_DEMUX_TSIN_ID_3          3
#define STM_FE_DEMOD_CUSTOMISATIONS_3         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_3               0
#define STM_FE_DEMOD_ROLL_OFF_3               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_3              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_3             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_3          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_3        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_3       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_3      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_3         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_3      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_3                     "\0"
#define STM_FE_LNB_IO_TYPE_3                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_3                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_3               2
#define STM_FE_LNB_I2C_BAUD_3                 100000
#define STM_FE_LNB_ADDRESS_3                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_3           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_3          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_3           0
#define STM_FE_LNB_TONE_SEL_PIN_3             0

#define STM_FE_DISEQC_NAME_3                  "\0"
#define STM_FE_DISEQC_VERSION_3               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_3        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_3                 0
#define STM_FE_TS_REMOTE_PORT_3               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_4
#define STM_FE_DEMOD_NAME_4                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_4                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_4              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_4             2
#define STM_FE_DEMOD_I2C_BAUD_4               400000
#define STM_FE_DEMOD_ADDRESS_4                0x1e
#define STM_FE_DEMOD_TUNER_NAME_4             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_4          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_4        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_4       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_4         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_4          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_4            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_4       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_4  2
#define STM_FE_DEMOD_DEMOD_CLOCK_4            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_4          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_4               1
#define STM_FE_DEMOD_TS_TAG_4                 0x04
#define STM_FE_DEMOD_DEMUX_TSIN_ID_4          4
#define STM_FE_DEMOD_CUSTOMISATIONS_4         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_4               0
#define STM_FE_DEMOD_ROLL_OFF_4               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_4              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_4             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_4          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_4        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_4       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_4      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_4         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_4      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_4                     "\0"
#define STM_FE_LNB_IO_TYPE_4                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_4                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_4               2
#define STM_FE_LNB_I2C_BAUD_4                 100000
#define STM_FE_LNB_ADDRESS_4                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_4           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_4          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_4           0
#define STM_FE_LNB_TONE_SEL_PIN_4             0

#define STM_FE_DISEQC_NAME_4                  "\0"
#define STM_FE_DISEQC_VERSION_4               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_4        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_4                 0
#define STM_FE_TS_REMOTE_PORT_4               0
#endif

#ifdef NIM_TSSERVER_INSTANCE_5
#define STM_FE_DEMOD_NAME_5                   "DUMMY_QAM\0"
#define STM_FE_DEMOD_IO_TYPE_5                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_5              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_5             2
#define STM_FE_DEMOD_I2C_BAUD_5               400000
#define STM_FE_DEMOD_ADDRESS_5                0x1e
#define STM_FE_DEMOD_TUNER_NAME_5             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_5          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_5        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_5       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_5         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_5          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_5            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_5       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_5  2
#define STM_FE_DEMOD_DEMOD_CLOCK_5            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_5          (DEMOD_TS_PARALLEL_PUNCT_CLOCK | \
					      DEMOD_TS_AUTO_SPEED | \
					      DEMOD_TS_RISINGEDGE_CLOCK | \
					      DEMOD_TS_SYNCBYTE_OFF | \
					      DEMOD_TS_PARITYBYTES_OFF | \
					      DEMOD_TS_SWAP_OFF | \
					      DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_5               1
#define STM_FE_DEMOD_TS_TAG_5                 0x05
#define STM_FE_DEMOD_DEMUX_TSIN_ID_5          5
#define STM_FE_DEMOD_CUSTOMISATIONS_5         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_5               0
#define STM_FE_DEMOD_ROLL_OFF_5               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_5              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_5             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_5          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_5        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_5       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_5      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_5         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_5      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_5                     "\0"
#define STM_FE_LNB_IO_TYPE_5                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_5                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_5               2
#define STM_FE_LNB_I2C_BAUD_5                 100000
#define STM_FE_LNB_ADDRESS_5                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_5           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_5          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_5           0
#define STM_FE_LNB_TONE_SEL_PIN_5             0

#define STM_FE_DISEQC_NAME_5                  "\0"
#define STM_FE_DISEQC_VERSION_5               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_5        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_5                 0
#define STM_FE_TS_REMOTE_PORT_5               0
#endif

#ifdef NIM_STV0910_STV6120_LNBH26
#define STM_FE_DEMOD_NAME_0                   "STV0910P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6120_2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         100000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               90000000
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         0
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH26P1-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV0910P1-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE


#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0

#define STM_FE_DEMOD_NAME_1                   "STV0910P2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               100000
#define STM_FE_DEMOD_ADDRESS_1                0x68
#define STM_FE_DEMOD_TUNER_NAME_1             "STV6120_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_1            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  1
#define STM_FE_DEMOD_DEMOD_CLOCK_1            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          (DEMOD_TS_SERIAL_PUNCT_CLOCK |\
                                              DEMOD_TS_AUTO_SPEED |\
                                              DEMOD_TS_RISINGEDGE_CLOCK |\
                                              DEMOD_TS_SYNCBYTE_ON |\
                                              DEMOD_TS_PARITYBYTES_OFF |\
                                              DEMOD_TS_SWAP_OFF |\
                                              DEMOD_TS_SMOOTHER_OFF)
#define STM_FE_DEMOD_TS_CLOCK_1               90000000
#define STM_FE_DEMOD_TS_TAG_1                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          5
#define STM_FE_DEMOD_CUSTOMISATIONS_1         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "LNBH26P2-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "DISEQC_STV0910P2-STM_FE\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif

#ifdef NIM_STV0900_STV6120_LNBH24_VGLNA
#define STM_FE_DEMOD_NAME_0                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6120_2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         100000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               90000000
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_ADD_ON_VGLNA
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0

#define STM_FE_DEMOD_NAME_1                   "STV0900P2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               100000
#define STM_FE_DEMOD_ADDRESS_1                0x68
#define STM_FE_DEMOD_TUNER_NAME_1             "STV6120_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_1            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  1
#define STM_FE_DEMOD_DEMOD_CLOCK_1            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_1               90000000
#define STM_FE_DEMOD_TS_TAG_1                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          5
#define STM_FE_DEMOD_CUSTOMISATIONS_1         DEMOD_CUSTOM_ADD_ON_VGLNA
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x0a
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "DISEQC_STV0900P2-STM_FE\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif

#ifdef NIM_STV0900_STV6120_LNBH24_NO_VGLNA
#define STM_FE_DEMOD_NAME_0                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6120_2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         100000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               90000000
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE


#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0

#define STM_FE_DEMOD_NAME_1                   "STV0900P2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               100000
#define STM_FE_DEMOD_ADDRESS_1                0x68
#define STM_FE_DEMOD_TUNER_NAME_1             "STV6120_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_1            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  1
#define STM_FE_DEMOD_DEMOD_CLOCK_1            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_1               90000000
#define STM_FE_DEMOD_TS_TAG_1                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          5
#define STM_FE_DEMOD_CUSTOMISATIONS_1         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x0a
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "DISEQC_STV0900P2-STM_FE\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif

#ifdef NIM_STV0900_STV6110_LNBH24
#define STM_FE_DEMOD_NAME_0                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "STV6110_1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         100000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  1
#define STM_FE_DEMOD_DEMOD_CLOCK_0            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               90000000
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE


#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0

#define STM_FE_DEMOD_NAME_1                   "STV0900P2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               100000
#define STM_FE_DEMOD_ADDRESS_1                0x68
#define STM_FE_DEMOD_TUNER_NAME_1             "STV6110_2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x63
#define STM_FE_DEMOD_TUNER_CLOCK_1            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  1
#define STM_FE_DEMOD_DEMOD_CLOCK_1            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_1               90000000
#define STM_FE_DEMOD_TS_TAG_1                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          5
#define STM_FE_DEMOD_CUSTOMISATIONS_1         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "LNBH24-STM_FE\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x0a
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "DISEQC_STV0900P2-STM_FE\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif


#ifdef NIM_CONFIG_INSTANCE_0
/* stm_fe configuration for instance 0 */
#define STM_FE_DEMOD_NAME_0                   "\0"
#define STM_FE_DEMOD_IO_TYPE_0                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_0              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_0             2
#define STM_FE_DEMOD_I2C_BAUD_0               400000
#define STM_FE_DEMOD_ADDRESS_0                0x68
#define STM_FE_DEMOD_TUNER_NAME_0             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_0          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_0        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_0       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_0         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_0          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_0            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_0       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_0  2
#define STM_FE_DEMOD_DEMOD_CLOCK_0            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_0          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_0               1
#define STM_FE_DEMOD_TS_TAG_0                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_0          0
#define STM_FE_DEMOD_CUSTOMISATIONS_0         (DEMOD_CUSTOM_USE_AUTO_TUNER |\
                                              DEMOD_CUSTOM_IQ_WIRING_INVERTED)
#define STM_FE_DEMOD_TUNER_IF_0               0
#define STM_FE_DEMOD_ROLL_OFF_0               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_0              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_0             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_0          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_0        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_0       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_0      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_0         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_0      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_0                     "\0"
#define STM_FE_LNB_IO_TYPE_0                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_0                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_0               2
#define STM_FE_LNB_I2C_BAUD_0                 100000
#define STM_FE_LNB_ADDRESS_0                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_0           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_0          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_0           0
#define STM_FE_LNB_TONE_SEL_PIN_0             0

#define STM_FE_DISEQC_NAME_0                  "\0"
#define STM_FE_DISEQC_VERSION_0               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_0        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_0                 0
#define STM_FE_TS_REMOTE_PORT_0               0
#endif

#ifdef NIM_CONFIG_INSTANCE_1
/* stm_fe configuration for instance 1 */
#define STM_FE_DEMOD_NAME_1                   "\0"
#define STM_FE_DEMOD_IO_TYPE_1                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_1              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_1             2
#define STM_FE_DEMOD_I2C_BAUD_1               400000
#define STM_FE_DEMOD_ADDRESS_1                0x68
#define STM_FE_DEMOD_TUNER_NAME_1             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_1          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_1        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_1       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_1         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_1          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_1            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_1       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_1  2
#define STM_FE_DEMOD_DEMOD_CLOCK_1            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_1          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_1               1
#define STM_FE_DEMOD_TS_TAG_1                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_1          0
#define STM_FE_DEMOD_CUSTOMISATIONS_1         (DEMOD_CUSTOM_USE_AUTO_TUNER |\
                                              DEMOD_CUSTOM_IQ_WIRING_INVERTED)
#define STM_FE_DEMOD_TUNER_IF_1               0
#define STM_FE_DEMOD_ROLL_OFF_1               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_1              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_1             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_1          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_1        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_1       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_1      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_1         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_1      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_1                     "\0"
#define STM_FE_LNB_IO_TYPE_1                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_1                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_1               2
#define STM_FE_LNB_I2C_BAUD_1                 100000
#define STM_FE_LNB_ADDRESS_1                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_1           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_1          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_1           0
#define STM_FE_LNB_TONE_SEL_PIN_1             0

#define STM_FE_DISEQC_NAME_1                  "\0"
#define STM_FE_DISEQC_VERSION_1               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_1        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_1                 0
#define STM_FE_TS_REMOTE_PORT_1               0
#endif

#ifdef NIM_CONFIG_INSTANCE_2
/* stm_fe configuration for instance 2 */
#define STM_FE_DEMOD_NAME_2                   "\0"
#define STM_FE_DEMOD_IO_TYPE_2                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_2              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_2             2
#define STM_FE_DEMOD_I2C_BAUD_2               400000
#define STM_FE_DEMOD_ADDRESS_2                0x69
#define STM_FE_DEMOD_TUNER_NAME_2             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_2          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_2        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_2       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_2         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_2          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_2            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_2       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_2  2
#define STM_FE_DEMOD_DEMOD_CLOCK_2            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_2          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_2               2
#define STM_FE_DEMOD_TS_TAG_2                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_2          0
#define STM_FE_DEMOD_CUSTOMISATIONS_2         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_2               0
#define STM_FE_DEMOD_ROLL_OFF_2               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_2              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_2             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_2          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_2        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_2       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_2      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_2         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_2      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_2                     "\0"
#define STM_FE_LNB_IO_TYPE_2                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_2                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_2               2
#define STM_FE_LNB_I2C_BAUD_2                 100000
#define STM_FE_LNB_ADDRESS_2                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_2           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_2          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_2           0
#define STM_FE_LNB_TONE_SEL_PIN_2             0

#define STM_FE_DISEQC_NAME_2                  "\0"
#define STM_FE_DISEQC_VERSION_2               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_2        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_2                 0
#define STM_FE_TS_REMOTE_PORT_2               0
#endif

#ifdef NIM_CONFIG_INSTANCE_3
/* stm_fe configuration for instance 3 */
#define STM_FE_DEMOD_NAME_3                   "\0"
#define STM_FE_DEMOD_IO_TYPE_3                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_3              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_3             2
#define STM_FE_DEMOD_I2C_BAUD_3               400000
#define STM_FE_DEMOD_ADDRESS_3                0x68
#define STM_FE_DEMOD_TUNER_NAME_3             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_3          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_3        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_3       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_3         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_3          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_3            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_3       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_3  2
#define STM_FE_DEMOD_DEMOD_CLOCK_3            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_3          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_3               3
#define STM_FE_DEMOD_TS_TAG_3                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_3          0
#define STM_FE_DEMOD_CUSTOMISATIONS_3         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_3               0
#define STM_FE_DEMOD_ROLL_OFF_3               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_3              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_3             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_3          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_3        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_3       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_3      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_3         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_3      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_3                     "\0"
#define STM_FE_LNB_IO_TYPE_3                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_3                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_3               2
#define STM_FE_LNB_I2C_BAUD_3                 100000
#define STM_FE_LNB_ADDRESS_3                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_3           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_3          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_3           0
#define STM_FE_LNB_TONE_SEL_PIN_3             0

#define STM_FE_DISEQC_NAME_3                  "\0"
#define STM_FE_DISEQC_VERSION_3               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_3        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_3                 0
#define STM_FE_TS_REMOTE_PORT_3               0
#endif

/* stm_fe configuration for instance 4 */
#ifdef NIM_CONFIG_INSTANCE_4
#define STM_FE_DEMOD_NAME_4                   "\0"
#define STM_FE_DEMOD_IO_TYPE_4                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_4              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_4             2
#define STM_FE_DEMOD_I2C_BAUD_4               400000
#define STM_FE_DEMOD_ADDRESS_4                0x68
#define STM_FE_DEMOD_TUNER_NAME_4             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_4          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_4        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_4       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_4         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_4          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_4            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_4       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_4  2
#define STM_FE_DEMOD_DEMOD_CLOCK_4            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_4          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_4               4
#define STM_FE_DEMOD_TS_TAG_4                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_4          0
#define STM_FE_DEMOD_CUSTOMISATIONS_4         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_4               0
#define STM_FE_DEMOD_ROLL_OFF_4               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_4              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_4             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_4          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_4        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_4       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_4      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_4         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_4      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_4                     "\0"
#define STM_FE_LNB_IO_TYPE_4                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_4                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_4               2
#define STM_FE_LNB_I2C_BAUD_4                 100000
#define STM_FE_LNB_ADDRESS_4                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_4           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_4          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_4           0
#define STM_FE_LNB_TONE_SEL_PIN_4             0

#define STM_FE_DISEQC_NAME_4                  "\0"
#define STM_FE_DISEQC_VERSION_4               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_4        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_4                 0
#define STM_FE_TS_REMOTE_PORT_4               0
#endif

/* stm_fe configuration for instance 5 */
#ifdef NIM_CONFIG_INSTANCE_5
#define STM_FE_DEMOD_NAME_5                   "\0"
#define STM_FE_DEMOD_IO_TYPE_5                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_5              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_5             2
#define STM_FE_DEMOD_I2C_BAUD_5               400000
#define STM_FE_DEMOD_ADDRESS_5                0x68
#define STM_FE_DEMOD_TUNER_NAME_5             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_5          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_5        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_5       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_5         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_5          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_5            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_5       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_5  2
#define STM_FE_DEMOD_DEMOD_CLOCK_5            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_5          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_5               5
#define STM_FE_DEMOD_TS_TAG_5                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_5          0
#define STM_FE_DEMOD_CUSTOMISATIONS_5         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_5               0
#define STM_FE_DEMOD_ROLL_OFF_5               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_5              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_5             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_5          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_5        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_5       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_5      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_5         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_5      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_5                     "\0"
#define STM_FE_LNB_IO_TYPE_5                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_5                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_5               2
#define STM_FE_LNB_I2C_BAUD_5                 100000
#define STM_FE_LNB_ADDRESS_5                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_5           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_5          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_5           0
#define STM_FE_LNB_TONE_SEL_PIN_5             0

#define STM_FE_DISEQC_NAME_5                  "\0"
#define STM_FE_DISEQC_VERSION_5               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_5        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_5                 0
#define STM_FE_TS_REMOTE_PORT_5               0
#endif

/* stm_fe configuration for instance 6 */
#ifdef NIM_CONFIG_INSTANCE_6
#define STM_FE_DEMOD_NAME_6                   "\0"
#define STM_FE_DEMOD_IO_TYPE_6                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_6              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_6             2
#define STM_FE_DEMOD_I2C_BAUD_6               400000
#define STM_FE_DEMOD_ADDRESS_6                0x68
#define STM_FE_DEMOD_TUNER_NAME_6             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_6          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_6        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_6       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_6         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_6          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_6            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_6       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_6  2
#define STM_FE_DEMOD_DEMOD_CLOCK_6            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_6          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_6               5
#define STM_FE_DEMOD_TS_TAG_6                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_6          1
#define STM_FE_DEMOD_CUSTOMISATIONS_6         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_6               0
#define STM_FE_DEMOD_ROLL_OFF_6               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_6              STMFE_GPIO(15, 2) /*Reset pin for NIMA*/
#define STM_FE_DEMOD_VGLNA_TYPE_6             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_6          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_6        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_6       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_6      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_6         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_6      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_6                     "\0"
#define STM_FE_LNB_IO_TYPE_6                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_6                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_6               2
#define STM_FE_LNB_I2C_BAUD_6                 100000
#define STM_FE_LNB_ADDRESS_6                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_6           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_6          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_6           0
#define STM_FE_LNB_TONE_SEL_PIN_6             0

#define STM_FE_DISEQC_NAME_6                  "\0"
#define STM_FE_DISEQC_VERSION_6               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_6        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_6                 0x0
#define STM_FE_TS_REMOTE_PORT_6               0
#endif

/* stm_fe configuration for instance 7 */
#ifdef NIM_CONFIG_INSTANCE_7
#define STM_FE_DEMOD_NAME_7                   "\0"
#define STM_FE_DEMOD_IO_TYPE_7                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_7              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_7             2
#define STM_FE_DEMOD_I2C_BAUD_7               400000
#define STM_FE_DEMOD_ADDRESS_7                0x68
#define STM_FE_DEMOD_TUNER_NAME_7             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_7          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_7        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_7       2
#define STM_FE_DEMOD_TUNER_I2C_BAUD_7         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_7          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_7            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_7       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_7  2
#define STM_FE_DEMOD_DEMOD_CLOCK_7            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_7          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_7               5
#define STM_FE_DEMOD_TS_TAG_7                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_7          1
#define STM_FE_DEMOD_CUSTOMISATIONS_7         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_7               0
#define STM_FE_DEMOD_ROLL_OFF_7               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_7              STMFE_GPIO(15, 2) /*Reset pin for NIMA*/
#define STM_FE_DEMOD_VGLNA_TYPE_7             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_7          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_7        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_7       2
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_7      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_7         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_7      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_7                     "\0"
#define STM_FE_LNB_IO_TYPE_7                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_7                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_7               2
#define STM_FE_LNB_I2C_BAUD_7                 100000
#define STM_FE_LNB_ADDRESS_7                  0x0A
#define STM_FE_LNB_CUSTOMISATIONS_7           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_7          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_7           0
#define STM_FE_LNB_TONE_SEL_PIN_7             0

#define STM_FE_DISEQC_NAME_7                  "\0"
#define STM_FE_DISEQC_VERSION_7               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_7        DISEQC_CUSTOM_NONE

#define STM_FE_TS_REMOTE_IP_7                 0x0
#define STM_FE_TS_REMOTE_PORT_7               0
#endif

/* Frontend NIM Configuration Help * /
 *
 * Copy the configuration and replace <instance id> with actual id.
 *
 * Notes:
 * 1. All I2C addresses are 7 bit device addesses; If the values in datasheets or
 * schematics are 8 bit representations, then they have to be right shifted by
 * on bit and set accordingly in this file.
 */

/* NIM_STV0900_STV6110_LNBH24
 * Version: 1.1
 * Std: Satellite DVBS, DVBS2
 * Description: Two demodulation paths, Two TS outputs.

#define STM_FE_DEMOD_NAME_<instance id>                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "STV6110-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id>                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x68
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               0
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE


#define STM_FE_DEMOD_NAME_<instance id2>                   "STV090x-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id2>             "STV6110-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id2>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id2>                  "DISEQC_STV090x-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id2>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id2>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id2>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id2>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id2>                0x68
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id2>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id2>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id2>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id2>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id2>            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id2>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id2>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id2>            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id2>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id2>               0
#define STM_FE_DEMOD_TS_TAG_<instance id2>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id2>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id2>         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_<instance id2>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id2>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id2>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id2>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id2>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id2>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id2>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id2>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id2>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id2>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id2>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id2>               1
#define STM_FE_LNB_I2C_BAUD_<instance id2>                 100000
#define STM_FE_LNB_ADDRESS_<instance id2>                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_<instance id2>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id2>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id2>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id2>             0

#define STM_FE_DISEQC_VERSION_<instance id2>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id2>        DISEQC_CUSTOM_NONE

*/

/* NIM_STV0903_STV6110_LNBH24
 * Version: 1.1
 * Std: Satellite DVBS, DVBS2
 * Description:

#define STM_FE_DEMOD_NAME_<instance id>                   "STV090x-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "STV6110-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id>                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x68
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            16000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            16000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               0
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_NONE
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE


 */

/* NIM_STV0900_STV6120_LNBH24_VGLNA
 * Version: 2.2
 * Std: Satellite DVBS, DVBS2
 * Description: Two demodulation paths, Two TS outputs. There are two STVVGLNA chips at
 the input section.

#define STM_FE_DEMOD_NAME_<instance id>                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "STV6120_1-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id>                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x6A
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               0
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_IQ_WIRING_INVERTED |
                                                             DEMOD_CUSTOM_ADD_ON_VGLNA
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE



#define STM_FE_DEMOD_NAME_<instance id2>                   "STV0900P2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id2>             "STV6120_2-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id2>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id2>                  "DISEQC_STV0900_2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id2>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id2>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id2>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id2>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id2>                0x6A
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id2>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id2>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id2>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id2>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id2>            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id2>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id2>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id2>            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id2>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id2>               0
#define STM_FE_DEMOD_TS_TAG_<instance id2>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id2>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id2>         DEMOD_CUSTOM_IQ_WIRING_INVERTED |
                                                             DEMOD_CUSTOM_ADD_ON_VGLNA
#define STM_FE_DEMOD_TUNER_IF_<instance id2>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id2>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id2>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id2>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id2>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id2>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id2>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id2>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id2>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id2>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id2>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id2>               1
#define STM_FE_LNB_I2C_BAUD_<instance id2>                 100000
#define STM_FE_LNB_ADDRESS_<instance id2>                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_<instance id2>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id2>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id2>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id2>             0

#define STM_FE_DISEQC_VERSION_<instance id2>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id2>        DISEQC_CUSTOM_NONE


*/

/* NIM_STV0900_STV6120_LNBH24_NO_VGLNA
 * Version: 2.2
 * Std: Satellite DVBS, DVBS2
 * Description: Two demodulation paths, Two TS outputs. Two baluns used at the
 input section.

#define STM_FE_DEMOD_NAME_<instance id>                   "STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "STV6120_1-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id>                  "DISEQC_STV0900P1-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x6A
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               0
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x08
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE



#define STM_FE_DEMOD_NAME_<instance id2>                   "STV0900P2-STM_FE\0"
#define STM_FE_DEMOD_TUNER_NAME_<instance id2>             "STV6120_2-STM_FE\0"
#define STM_FE_LNB_NAME_<instance id2>                     "LNBH24-STM_FE\0"
#define STM_FE_DISEQC_NAME_<instance id2>                  "DISEQC_STV0900_2-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id2>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id2>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id2>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id2>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id2>                0x6A
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id2>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id2>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id2>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id2>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id2>            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id2>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id2>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id2>            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id2>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id2>               0
#define STM_FE_DEMOD_TS_TAG_<instance id2>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id2>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id2>         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_<instance id2>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id2>               DEMOD_ROLL_OFF_0_35
#define STM_FE_DEMOD_RESET_PIO_<instance id2>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id2>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id2>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id2>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id2>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id2>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id2>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id2>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_IO_TYPE_<instance id2>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id2>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id2>               1
#define STM_FE_LNB_I2C_BAUD_<instance id2>                 100000
#define STM_FE_LNB_ADDRESS_<instance id2>                  0x09
#define STM_FE_LNB_CUSTOMISATIONS_<instance id2>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id2>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id2>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id2>             0

#define STM_FE_DISEQC_VERSION_<instance id2>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id2>        DISEQC_CUSTOM_NONE


*/

/* NIM_STV0367_STV4100
 * Version: 3.1
 * Std: Terrestial and Cable, Mutually exclusive
 * Description: Capable of demodulation both terrestial and
 * cable in a mutually exclusive manner.
 * Use demod name STV0367C-STM_FE for Cable only; STV0367T-STM_FE for Terrestial only;
 * STV0367-STM_FE for Terrestial and Cable mutually exclusive.

#define STM_FE_DEMOD_NAME_<instance id>                   "STV0367T-STM_FE\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x1D
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "STV4100-STM_FE\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  1
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            30000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               0
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          1
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_<instance id>                     "\0"
#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_NAME_<instance id>                  "\0"
#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE


*/

/* NIM_PACKET_INJECTOR
 * Version:
 * Std: NA
 * Description:  DB580A packet injector

#define STM_FE_DEMOD_NAME_<instance id>                   "DUMMY_OFDM\0"
#define STM_FE_DEMOD_IO_TYPE_<instance id>                DEMOD_IO_I2C
#define STM_FE_DEMOD_I2C_ROUTE_<instance id>              DEMOD_IO_DIRECT
#define STM_FE_DEMOD_I2C_DEVICE_<instance id>             1
#define STM_FE_DEMOD_I2C_BAUD_<instance id>               400000
#define STM_FE_DEMOD_ADDRESS_<instance id>                0x1e
#define STM_FE_DEMOD_TUNER_NAME_<instance id>             "\0"
#define STM_FE_DEMOD_TUNER_IO_TYPE_<instance id>          DEMOD_IO_I2C
#define STM_FE_DEMOD_TUNER_I2C_ROUTE_<instance id>        DEMOD_IO_REPEATER
#define STM_FE_DEMOD_TUNER_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_TUNER_I2C_BAUD_<instance id>         400000
#define STM_FE_DEMOD_TUNER_ADDRESS_<instance id>          0x60
#define STM_FE_DEMOD_TUNER_CLOCK_<instance id>            4000000
#define STM_FE_DEMOD_DEMOD_CLOCK_TYPE_<instance id>       DEMOD_CLK_DEMOD_CLK_XTAL
#define STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_<instance id>  2
#define STM_FE_DEMOD_DEMOD_CLOCK_<instance id>            4000000
#define STM_FE_DEMOD_TS_OUT_CONFIG_<instance id>          DEMOD_TS_DEFAULT
#define STM_FE_DEMOD_TS_CLOCK_<instance id>               1
#define STM_FE_DEMOD_TS_TAG_<instance id>                 0x00
#define STM_FE_DEMOD_DEMUX_TSIN_ID_<instance id>          0
#define STM_FE_DEMOD_CUSTOMISATIONS_<instance id>         DEMOD_CUSTOM_IQ_WIRING_INVERTED
#define STM_FE_DEMOD_TUNER_IF_<instance id>               0
#define STM_FE_DEMOD_ROLL_OFF_<instance id>               DEMOD_ROLL_OFF_DEFAULT
#define STM_FE_DEMOD_RESET_PIO_<instance id>              DEMOD_RESET_PIO_NONE
#define STM_FE_DEMOD_VGLNA_TYPE_<instance id>             0
#define STM_FE_DEMOD_VGLNA_IO_TYPE_<instance id>          0
#define STM_FE_DEMOD_VGLNA_I2C_ROUTE_<instance id>        0
#define STM_FE_DEMOD_VGLNA_I2C_DEVICE_<instance id>       1
#define STM_FE_DEMOD_VGLNA_I2C_ADDRESS_<instance id>      0x00
#define STM_FE_DEMOD_VGLNA_I2C_BAUD_<instance id>         0
#define STM_FE_DEMOD_VGLNA_I2C_REP_BUS_<instance id>      DEMOD_REPEATER_BUS_1

#define STM_FE_LNB_NAME_<instance id>                     "\0"
#define STM_FE_LNB_IO_TYPE_<instance id>                  LNB_IO_I2C
#define STM_FE_LNB_I2C_ROUTE_<instance id>                LNB_IO_DIRECT
#define STM_FE_LNB_I2C_DEVICE_<instance id>               1
#define STM_FE_LNB_I2C_BAUD_<instance id>                 100000
#define STM_FE_LNB_ADDRESS_<instance id>                  0x10
#define STM_FE_LNB_CUSTOMISATIONS_<instance id>           LNB_CUSTOM_NONE
#define STM_FE_LNB_VOLTAGE_SEL_PIN_<instance id>          0
#define STM_FE_LNB_VOLTAGE_EN_PIN_<instance id>           0
#define STM_FE_LNB_TONE_SEL_PIN_<instance id>             0

#define STM_FE_DISEQC_NAME_<instance id>                  "\0"
#define STM_FE_DISEQC_VERSION_<instance id>               DISEQC_VERSION_2_0
#define STM_FE_DISEQC_CUSTOMISATIONS_<instance id>        DISEQC_CUSTOM_NONE

*/

#ifdef IPFE_CONFIG_INSTANCE_0
#define STM_FE_IP_NAME_0			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_0			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_0			6000
#define STM_FE_IP_PROTOCOL_0			0x1
#define STM_FE_IP_ETHDEVICE_0                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_0
#define STM_FE_IP_NAME_0			"\0"
#define STM_FE_IP_ADDRESS_0			"\0"
#define STM_FE_IP_PORT_NO_0			0
#define STM_FE_IP_PROTOCOL_0			0
#define STM_FE_IP_ETHDEVICE_0                  "\0"
#endif
#endif

#ifdef IPFE_FEC_CONFIG_INSTANCE_0
#define STM_FE_IP_NAME_0			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_0			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_0			6000
#define STM_FE_IP_PROTOCOL_0			0x1
#define STM_FE_IP_ETHDEVICE_0                  "eth0"

#define STM_FE_IPFEC_NAME_0			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_0			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_0		  	6002
#define STM_FE_IPFEC_SCHEME_0			0x1 /*0:row 1:column*/
#else
#define STM_FE_IPFEC_NAME_0			"\0"
#define STM_FE_IPFEC_ADDRESS_0			"\0"
#define STM_FE_IPFEC_PORT_NO_0			0
#define STM_FE_IPFEC_SCHEME_0			0
#endif


#ifdef IPFE_CONFIG_INSTANCE_1
#define STM_FE_IP_NAME_1			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_1			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_1			7000
#define STM_FE_IP_PROTOCOL_1			0x2
#define STM_FE_IP_ETHDEVICE_1                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_1
#define STM_FE_IP_NAME_1			"\0"
#define STM_FE_IP_ADDRESS_1			"\0"
#define STM_FE_IP_PORT_NO_1			0
#define STM_FE_IP_PROTOCOL_1			0
#define STM_FE_IP_ETHDEVICE_1                  "\0"
#endif
#endif


#ifdef IPFE_FEC_CONFIG_INSTANCE_1
#define STM_FE_IP_NAME_1			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_1			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_1			7000
#define STM_FE_IP_PROTOCOL_1			0x1
#define STM_FE_IP_ETHDEVICE_1                  "eth0"

#define STM_FE_IPFEC_NAME_1			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_1			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_1		  	7002
#define STM_FE_IPFEC_SCHEME_1			0x1 /*0:row 1:column*/
#else
#define STM_FE_IPFEC_NAME_1			"\0"
#define STM_FE_IPFEC_ADDRESS_1			"\0"
#define STM_FE_IPFEC_PORT_NO_1			0
#define STM_FE_IPFEC_SCHEME_1			0
#endif

#ifdef IPFE_CONFIG_INSTANCE_2
#define STM_FE_IP_NAME_2			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_2			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_2			8000
#define STM_FE_IP_PROTOCOL_2			0x2
#define STM_FE_IP_ETHDEVICE_2                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_2
#define STM_FE_IP_NAME_2			"\0"
#define STM_FE_IP_ADDRESS_2			"\0"
#define STM_FE_IP_PORT_NO_2			0
#define STM_FE_IP_PROTOCOL_2			0
#define STM_FE_IP_ETHDEVICE_2                  "\0"
#endif
#endif


#ifdef IPFE_FEC_CONFIG_INSTANCE_2
#define STM_FE_IP_NAME_2			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_2			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_2			8000
#define STM_FE_IP_PROTOCOL_2			0x1
#define STM_FE_IP_ETHDEVICE_2                  "eth0"

#define STM_FE_IPFEC_NAME_2			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_2			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_2		  	8002
#define STM_FE_IPFEC_SCHEME_2			0x2 /*0x1:row 0x2:column*/
#else
#define STM_FE_IPFEC_NAME_2			"\0"
#define STM_FE_IPFEC_ADDRESS_2			"\0"
#define STM_FE_IPFEC_PORT_NO_2			0
#define STM_FE_IPFEC_SCHEME_2			0
#endif

#ifdef IPFE_CONFIG_INSTANCE_3
#define STM_FE_IP_NAME_3			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_3			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_3			9000
#define STM_FE_IP_PROTOCOL_3			0x2
#define STM_FE_IP_ETHDEVICE_3                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_3
#define STM_FE_IP_NAME_3			"\0"
#define STM_FE_IP_ADDRESS_3			"\0"
#define STM_FE_IP_PORT_NO_3			0
#define STM_FE_IP_PROTOCOL_3			0
#define STM_FE_IP_ETHDEVICE_3                  "\0"
#endif
#endif


#ifdef IPFE_FEC_CONFIG_INSTANCE_3
#define STM_FE_IP_NAME_3			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_3			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_3			9000
#define STM_FE_IP_PROTOCOL_3			0x1
#define STM_FE_IP_ETHDEVICE_3                  "eth0"

#define STM_FE_IPFEC_NAME_3			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_3			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_3			9002
#define STM_FE_IPFEC_SCHEME_3			0x2 /*0x1:row 0x2:column*/
#else
#define STM_FE_IPFEC_NAME_3			"\0"
#define STM_FE_IPFEC_ADDRESS_3			"\0"
#define STM_FE_IPFEC_PORT_NO_3			0
#define STM_FE_IPFEC_SCHEME_3			0
#endif

#ifdef IPFE_CONFIG_INSTANCE_4
#define STM_FE_IP_NAME_4			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_4			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_4			10000
#define STM_FE_IP_PROTOCOL_4			0x2
#define STM_FE_IP_ETHDEVICE_4                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_4
#define STM_FE_IP_NAME_4			"\0"
#define STM_FE_IP_ADDRESS_4			"\0"
#define STM_FE_IP_PORT_NO_4			0
#define STM_FE_IP_PROTOCOL_4			0
#define STM_FE_IP_ETHDEVICE_4                  "\0"
#endif
#endif


#ifdef IPFE_FEC_CONFIG_INSTANCE_4
#define STM_FE_IP_NAME_4			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_4			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_4			10000
#define STM_FE_IP_PROTOCOL_4			0x1
#define STM_FE_IP_ETHDEVICE_4                  "eth0"

#define STM_FE_IPFEC_NAME_4			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_4			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_4			10002
#define STM_FE_IPFEC_SCHEME_4			0x2 /*0x1:row 0x2:column*/
#else
#define STM_FE_IPFEC_NAME_4			"\0"
#define STM_FE_IPFEC_ADDRESS_4			"\0"
#define STM_FE_IPFEC_PORT_NO_4			0
#define STM_FE_IPFEC_SCHEME_4			0
#endif

#ifdef IPFE_CONFIG_INSTANCE_5
#define STM_FE_IP_NAME_5			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_5			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_5			11000
#define STM_FE_IP_PROTOCOL_5			0x2
#define STM_FE_IP_ETHDEVICE_5                  "eth0"
#else
#ifndef IPFE_FEC_CONFIG_INSTANCE_5
#define STM_FE_IP_NAME_5			"\0"
#define STM_FE_IP_ADDRESS_5			"\0"
#define STM_FE_IP_PORT_NO_5			0
#define STM_FE_IP_PROTOCOL_5			0
#define STM_FE_IP_ETHDEVICE_5                  "\0"
#endif
#endif


#ifdef IPFE_FEC_CONFIG_INSTANCE_5
#define STM_FE_IP_NAME_5			"IPFE-STM_FE\0"
#define STM_FE_IP_ADDRESS_5			"127.0.0.1\0"
#define STM_FE_IP_PORT_NO_5			11000
#define STM_FE_IP_PROTOCOL_5			0x1
#define STM_FE_IP_ETHDEVICE_5                  "eth0"

#define STM_FE_IPFEC_NAME_5			"IPFEC-STM_FE\0"
#define STM_FE_IPFEC_ADDRESS_5			"127.0.0.1\0"
#define STM_FE_IPFEC_PORT_NO_5			11002
#define STM_FE_IPFEC_SCHEME_5			0x2 /*0x1:row 0x2:column*/
#else
#define STM_FE_IPFEC_NAME_5			"\0"
#define STM_FE_IPFEC_ADDRESS_5			"\0"
#define STM_FE_IPFEC_PORT_NO_5			0
#define STM_FE_IPFEC_SCHEME_5			0
#endif

#endif /* _STM_FE_CONF_H */
/* End of stm_fe_conf.h  */
