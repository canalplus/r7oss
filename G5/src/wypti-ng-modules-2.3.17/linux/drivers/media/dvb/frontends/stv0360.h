/*
    ST Microelectronics stv0360 DVB OFDM demodulator driver

    Copyright (C) 2006,2009 ST Microelectronics
	Peter Bennett <peter.bennett@st.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef STV0360_H
#define STV0360_H

#include <linux/dvb/frontend.h>

extern struct dvb_frontend* stv0360_attach(struct i2c_adapter* i2c, 
					   u8 tuner_address, 
					   int serial_not_parallel);

#define R_ID        0x0
#define R_I2CRPT    0x1
#define R_TOPCTRL   0x2
#define R_IOCFG0    0x3
#define R_DAC0R	    0x4
#define R_IOCFG1    0x5
#define R_DAC1R	    0x6
#define R_IOCFG2    0x7
#define R_PWMFR	    0x8
#define R_STATUS    0x9
#define R_AUX_CLK   0xa
#define R_FREE_SYS1 0xb
#define R_FREE_SYS2 0xc
#define R_FREE_SYS3 0xd
#define R_AGC2_MAX  0x10
#define R_AGC2_MIN  0x11
#define R_AGC1_MAX  0x12
#define R_AGC1_MIN  0x13
#define R_AGCR	    0x14
#define R_AGC2TH    0x15
#define R_AGC12C3   0x16
#define R_AGCCTRL1  0x17
#define R_AGCCTRL2  0x18
#define R_AGC1VAL1  0x19
#define R_AGC1VAL2  0x1a
#define R_AGC2VAL1  0x1b
#define R_AGC2VAL2  0x1c
#define R_AGC2_PGA  0x1d
#define R_OVF_RATE1 0x1e
#define R_OVF_RATE2 0x1f
#define R_GAIN_SRC1 0x20
#define R_GAIN_SRC2 0x21
#define R_INC_DEROT1 0x22
#define R_INC_DEROT2 0x23
#define R_FREESTFE_1 0x26
#define R_SYR_THR    0x27
#define R_INR        0x28
#define R_EN_PROCESS 0x29
#define R_SDI_SMOOTHER 0x2a
#define R_FE_LOOP_OPEN 0x2b
#define R_EPQ        0x31
#define R_EPQ2       0x32
#define R_COR_CTL    0x80
#define R_COR_STAT   0x81
#define R_COR_INTEN  0x82
#define R_COR_INTSTAT 0x83
#define R_COR_MODEGUARD 0x84
#define R_AGC_CTL 0x85
#define R_AGC_MANUAL1	0x86
#define R_AGC_MANUAL2	0x87
#define R_AGC_TARGET	0x88
#define R_AGC_GAIN1	0x89
#define R_AGC_GAIN2	0x8a
#define R_ITB_CTL	0x8b
#define R_ITB_FREQ1	0x8c
#define R_ITB_FREQ2	0x8d
#define R_CAS_CTL	0x8e
#define R_CAS_FREQ	0x8f
#define R_CAS_DAGCGAIN	0x90
#define R_SYR_CTL	0x91
#define R_SYR_STAT	0x92
#define R_SYR_NC01	0x93
#define R_SYR_NC02	0x94
#define R_SYR_OFFSET1	0x95
#define R_SYR_OFFSET2	0x96
#define R_FFT_CTL	0x97
#define R_SCR_CTL	0x98
#define R_PPM_CTL1	0x99
#define R_TRL_CTL	0x9a
#define R_TRL_NOMRATE1	0x9b
#define R_TRL_NOMRATE2	0x9c
#define R_TRL_TIME1	0x9d
#define R_TRL_TIME2	0x9e
#define R_CRL_CTL	0x9f
#define R_CRL_FREQ1	0xa0
#define R_CRL_FREQ2	0xa1
#define R_CRL_FREQ3	0xa2
#define R_CHC_CTL1	0xa3
#define R_CHC_SNR	0xa4
#define R_BDI_CTL	0xa5
#define R_DMP_CTL	0xa6
#define R_TPS_RCVD1	0xa7
#define R_TPS_RCVD2	0xa8
#define R_TPS_RCVD3	0xa9
#define R_TPS_RCVD4	0xaa
#define R_TPS_CELLID	0xab
#define R_TPS_FREE2	0xac
#define R_TPS_SET1	0xad
#define R_TPS_SET2	0xae
#define R_TPS_SET3	0xaf
#define R_TPS_CTL	0xb0
#define R_CTL_FFTOSNUM	0xb1
#define R_CAR_DISP_SEL	0xb2
#define R_MSC_REV	0xb3
#define R_PIR_CTL	0xb4
#define R_SNR_CARRIER1	0xb5
#define R_SNR_CARRIER2	0xb6
#define R_PPM_CPAMP	0xb7
#define R_TSM_AP0	0xb8
#define R_TSM_AP1	0xb9
#define R_TSM_AP2	0xba
#define R_TSM_AP3	0xbb
#define R_TSM_AP4	0xbc
#define R_TSM_AP5	0xbd
#define R_TSM_AP6	0xbe
#define R_TSM_AP7	0xbf
#define R_CONSTMODE	0xcb
#define R_CONSTCARR1	0xcc
#define R_CONSTCARR2	0xcd
#define R_ICONSTEL	0xce
#define R_QCONSTEL	0xcf
#define R_AGC1RF	0xd4
#define R_EN_RF_AGC1	0xd5
#define R_FECM		0x40
#define R_VTH0		0x41
#define R_VTH1		0x42
#define R_VTH2		0x43
#define R_VTH3		0x44
#define R_VTH4		0x45
#define R_VTH5		0x46
#define R_FREEVIT	0x47
#define R_VITPROG	0x49
#define R_PR		0x4a
#define R_VSEARCH	0x4b
#define R_RS		0x4c
#define R_RSOUT		0x4d
#define R_ERRCTRL1	0x4e
#define R_ERRCNTM1	0x4f
#define R_ERRCNTL1	0x50
#define R_ERRCTRL2	0x51
#define R_ERRCNTM2	0x52
#define R_ERRCNTL2	0x53
#define R_ERRCTRL3	0x56
#define R_ERRCNTM3	0x57
#define R_ERRCNTL3	0x58
#define R_DILSTK1	0x59
#define R_DILSTK0	0x5a
#define R_DILBWSTK1	0x5b
#define R_DILBWST0	0x5c
#define R_LNBRX		0x5d
#define R_RSTC		0x5e
#define R_VIT_BIST	0x5f
#define R_FREEDRS	0x54
#define R_VERROR	0x55
#define R_TSTRES	0xc0
#define R_ANACTRL	0xc1
#define R_TSTBUS	0xc2
#define R_TSTCK		0xc3
#define R_TSTI2C	0xc4
#define R_TSTRAM1	0xc5
#define R_TSTRATE	0xc6
#define R_SELOUT	0xc7
#define R_FORCEIN	0xc8
#define R_TSTFIFO	0xc9
#define R_TSTRS		0xca
#define R_TSTBISTRES0	0xd0
#define R_TSTBISTRES1	0xd1
#define R_TSTBISTRES2	0xd2
#define R_TSTBISTRES3	0xd3

#define R_RF_AGC1    0xd4
#define R_EN_RF_AGC1 0xd5

// 362
#define R_RF_AGC2 0xd5
#define R_ANADIGCTRL 0xd7
#define R_PLLMDIV 0xd8
#define R_PLLNDIV 0xd9
#define R_PLLSETUP 0xda
#define R_DUAL_AD12 0xdb
#define R_TSTBIST 0xdc
#define R_PAD_COMP_CTRL 0xdd
#define R_PAD_COMP_WR 0xde
#define R_PAD_COMP_RD 0xdf
#define R_GHOSTREG 0x00        

#define R_FREQOFF1 0x2c
#define R_FREQOFF2 0x2d
#define R_FREQOFF3 0x2e
#define R_TIMOFF1 0x2f
#define R_TIMOFF2 0x30
#define R_EPQUAL 0x31

#define R_CHP_TAPS 0x33
#define R_CHP_DYN_COEFF 0x34
#define R_PPM_STATE_MAC 0x35
#define R_INR_THRESHOLD 0x36
#define R_EPQ_TPS_ID_CELL 0x37
#define R_EPQ_CFG 0x38
#define R_EPQ_STATUS 0x39
#define R_FECM 0x40
#define R_VTHR0 0x41
#define R_VTHR1 0x42
#define R_VTHR2 0x43
#define R_VTHR3 0x44
#define R_VTHR4 0x45
#define R_VTHR5 0x46
#define R_FREE_VIT 0x47
#define R_VITPROG 0x49
#define R_PRATE 0x4a
#define R_VSEARCH 0x4b
#define R_RS 0x4c
#define R_RSOUT 0x4d
#define R_ERRCTRL1 0x4e
#define R_ERRCNTM1 0x4f
#define R_ERRCNTL1 0x50
#define R_ERRCTRL2 0x51
#define R_ERRCNTM2 0x52
#define R_ERRCNTL2 0x53
#define R_FREE_DRS 0x54
#define R_VERROR 0x55
#define R_ERRCTRL3 0x56
#define R_ERRCNTM3 0x57
#define R_ERRCNTL3 0x58
#define R_DIL_STK1 0x59
#define R_DIL_STK0 0x5a
#define R_DIL_BWSTK1 0x5b
#define R_DIL_BWST0 0x5c
#define R_LNBRX 0x5d
#define R_RS_TC 0x5e
#define R_VIT_BIST 0x5f
#define R_IIR_CELL_NB 0x60
#define R_IIR_CX_COEFF1_MSB 0x61
#define R_IIR_CX_COEFF1_LSB 0x62
#define R_IIR_CX_COEFF2_MSB 0x63
#define R_IIR_CX_COEFF2_LSB 0x64
#define R_IIR_CX_COEFF3_MSB 0x65
#define R_IIR_CX_COEFF3_LSB 0x66
#define R_IIR_CX_COEFF4_MSB 0x67
#define R_IIR_CX_COEFF4_LSB 0x68
#define R_IIR_CX_COEFF5_MSB 0x69
#define R_IIR_CX_COEFF5_LSB 0x6a
#define R_FEPATH_CFG 0x6b
#define R_PMC1_FUNC 0x6c
#define R_PMC1_FORCE 0x6d
#define R_PMC2_FUNC 0x6e
#define R_DIG_AGC_R 0x70
#define R_COMAGC_TARMSB 0x71
#define R_COM_AGC_TAR_ENMODE 0x72
#define R_COM_AGC_CFG 0x73
#define R_COM_AGC_GAIN1 0x74
#define R_AUT_AGC_TARGET_MSB 0x75
#define R_LOCK_DETECT_MSB 0x76
#define R_AGCTAR_LOCK_LSBS 0x77
#define R_AUT_GAIN_EN 0x78
#define R_AUT_CFG 0x79
#define R_LOCK_N 0x7a
#define R_INT_X_3 0x7b
#define R_INT_X_2 0x7c
#define R_INT_X_1 0x7d
#define R_INT_X_0 0x7e
#define R_MIN_ERR_X_MSB 0x7f
#define R_STATUS_ERR_DA 0x6f
#define R_TSTRES 0xc0
#define R_ANACTRL 0xc1
#define R_TSTBUS 0xc2
#define R_TSTRATE 0xc6
#define R_PPM_CPAMP_DIR 0x24
#define R_PPM_CPAMP_INV 0x25

#define R_SDFR 0x08

#define R_AGC12C 0x16
#define R_FREESTFE_2 0x27
#define R_DCOFFSET 0x28 
#define R_EPQAUTO 0x32
#define R_RESERVED_1 0x8b
#define R_RESERVED_2 0x8c
#define R_RESERVED_3 0x8d
#define R_SYR_NCO1 0x93
#define R_SYR_NCO2 0x94
#define R_TPS_ID_CELL1 0xab
#define R_TPS_ID_CELL2 0xac
#define R_TPS_RCVD5_SET1 0xad
#define R_TEST_SELECT 0xb2 



#endif // STV0360_H
