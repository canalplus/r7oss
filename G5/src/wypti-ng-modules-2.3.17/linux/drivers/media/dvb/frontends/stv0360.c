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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "dvb_frontend.h"

#include "stv0360.h"

struct stv0360_state {
	struct dvb_frontend frontend;
        u8 demod_address;
	struct i2c_adapter* i2c;
        u8 *init_tab;
        u8 version;
        int serial_not_parallel;
        int crystal_freq; // 25 or 27 (we only handle 27)
};


static int debug = 0;
#define dprintk(fmt,args...) \
	do { \
		if (debug) printk(KERN_DEBUG "%s: " fmt,__FUNCTION__, ## args); \
	} while (0)

static u8 init_tab [] = {
R_ID            ,0x21, R_I2CRPT        ,0x51, R_TOPCTRL    ,0x2,  R_IOCFG0        ,0x40, R_DAC0R         ,0x00,
R_IOCFG1        ,0x40, R_DAC1R         ,0x00, R_IOCFG2     ,0x60, R_PWMFR         ,0x00, R_STATUS        ,0xf9,
R_AUX_CLK       ,0x1c, R_FREE_SYS1     ,0x00, R_FREE_SYS2  ,0x00, R_FREE_SYS3     ,0x00, R_AGC2_MAX      ,0xff,
R_AGC2_MIN      ,0x0,  R_AGC1_MAX      ,0x0,  R_AGC1_MIN   ,0x0,  R_AGCR          ,0xbc, R_AGC2TH        ,0xfe,
R_AGC12C3       ,0x00, R_AGCCTRL1      ,0x85, R_AGCCTRL2   ,0x1f, R_AGC1VAL1      ,0x00, R_AGC1VAL2      ,0x0,
R_AGC2VAL1      ,0x2,  R_AGC2VAL2      ,0x08, R_AGC2_PGA   ,0x00, R_OVF_RATE1     ,0x00, R_OVF_RATE2     ,0x00,
R_GAIN_SRC1     ,0xce, R_GAIN_SRC2     ,0x10, R_INC_DEROT1 ,0x55, R_INC_DEROT2    ,0x4b, R_FREESTFE_1    ,0x03,
R_SYR_THR       ,0x1c, R_INR           ,0xff, R_EN_PROCESS ,0x1,  R_SDI_SMOOTHER  ,0xff, R_FE_LOOP_OPEN  ,0x00,
R_EPQ           ,0x11, R_EPQ2          ,0xd,  R_COR_CTL    ,0x20, R_COR_STAT      ,0xf6, R_COR_INTEN     ,0x00,
R_COR_INTSTAT   ,0x3d, R_COR_MODEGUARD ,0x3,  R_AGC_CTL    ,0x18, R_AGC_MANUAL1   ,0x00, R_AGC_MANUAL2   ,0x00,
R_AGC_TARGET    ,0x20, R_AGC_GAIN1     ,0x9,  R_AGC_GAIN2  ,0x10, R_ITB_CTL       ,0x00, R_ITB_FREQ1     ,0x00,
R_ITB_FREQ2     ,0x00, R_CAS_CTL       ,0x85, R_CAS_FREQ   ,0xB3, R_CAS_DAGCGAIN  ,0x10, R_SYR_CTL       ,0x00,
R_SYR_STAT      ,0x17, R_SYR_NC01      ,0x00, R_SYR_NC02   ,0x00, R_SYR_OFFSET1   ,0x00,R_SYR_OFFSET2   ,0x00,
R_FFT_CTL       ,0x00, R_SCR_CTL       ,0x00, R_PPM_CTL1    ,0x30, R_TRL_CTL       ,0x94,R_TRL_NOMRATE1  ,0xb0,
R_TRL_NOMRATE2  ,0x56, R_TRL_TIME1     ,0xee, R_TRL_TIME2   ,0xfd, R_CRL_CTL       ,0x4F,R_CRL_FREQ1     ,0x90,
R_CRL_FREQ2     ,0x9b, R_CRL_FREQ3     ,0x0,  R_CHC_CTL1     ,0xb1, R_CHC_SNR       ,0xd7,R_BDI_CTL       ,0x00,
R_DMP_CTL       ,0x00, R_TPS_RCVD1     ,0x32, R_TPS_RCVD2   ,0x2,  R_TPS_RCVD3     ,0x01,R_TPS_RCVD4     ,0x31,
R_TPS_CELLID    ,0x00, R_TPS_FREE2     ,0x00, R_TPS_SET1    ,0x01, R_TPS_SET2      ,0x02,R_TPS_SET3      ,0x4,
R_TPS_CTL       ,0x00, R_CTL_FFTOSNUM  ,0x2b, R_CAR_DISP_SEL,0x0C, R_MSC_REV       ,0x0A,R_PIR_CTL       ,0x00,
R_SNR_CARRIER1  ,0xA8, R_SNR_CARRIER2  ,0x86, R_PPM_CPAMP   ,0xcc, R_TSM_AP0       ,0x00,R_TSM_AP1       ,0x00,
R_TSM_AP2       ,0x00, R_TSM_AP3       ,0x00, R_TSM_AP4     ,0x00, R_TSM_AP5       ,0x00,R_TSM_AP6       ,0x00,
R_TSM_AP7       ,0x00, R_CONSTMODE     ,0x02, R_CONSTCARR1  ,0xD2, R_CONSTCARR2    ,0x04,R_ICONSTEL      ,0xf7,
R_QCONSTEL      ,0xf8, R_AGC1RF        ,0xff, R_EN_RF_AGC1  ,0x83, R_FECM          ,0x00,R_VTH0  ,0x1E,
R_VTH1          ,0x1e, R_VTH2          ,0x0F, R_VTH3        ,0x09, R_VTH4          ,0x00,R_VTH5  ,0x05,
R_FREEVIT       ,0x00, R_VITPROG       ,0x92, R_PR          ,0x2,  R_VSEARCH       ,0xb0,R_RS    ,0xfc, /*0xbc*/
R_RSOUT         /* 0x15*/,0x1f, R_ERRCTRL1      ,0x12, R_ERRCNTM1    ,0x00, R_ERRCNTL1      ,0x00,R_ERRCTRL2      ,0x32,
R_ERRCNTM2      ,0x00, R_ERRCNTL2      ,0x00, R_ERRCTRL3    ,0x12, R_ERRCNTM3      ,0x00,R_ERRCNTL3      ,0x00,
R_DILSTK1       ,0x00, R_DILSTK0       ,0x03, R_DILBWSTK1   ,0x00, R_DILBWST0      ,0x03,R_LNBRX ,0x80,
R_RSTC          ,0xB0, R_VIT_BIST      ,0x07, R_FREEDRS     ,0x00, R_VERROR        ,0x00,R_TSTRES        ,0x00,
R_ANACTRL       ,0x00, R_TSTBUS        ,0x00, R_TSTCK       ,0x00, R_TSTI2C        ,0x00,R_TSTRAM1       ,0x00,
R_TSTRATE       ,0x00, R_SELOUT        ,0x00, R_FORCEIN     ,0x00, R_TSTFIFO       ,0x00,R_TSTRS ,0x00,
R_TSTBISTRES0   ,0x00, R_TSTBISTRES1   ,0x00, R_TSTBISTRES2 ,0x00, R_TSTBISTRES3   ,0x00 ,0xff,0xff
};

static u8 init_tab_362[] = {
R_PLLNDIV            ,0x0f, R_ID                 ,0x41, R_I2CRPT             ,0x22, 
R_TOPCTRL            ,0x00, R_IOCFG0             ,0x40, R_DAC0R              ,0x00, R_IOCFG1             ,0x00, 
R_DAC1R              ,0x00, R_IOCFG2             ,0x00, R_SDFR               ,0x00, R_STATUS             ,0xf8, 
R_AUX_CLK            ,0x0a, R_FREE_SYS1          ,0x00, R_FREE_SYS2          ,0x00, R_FREE_SYS3          ,0x00, 
R_AGC2_MAX           ,0xff, R_AGC2_MIN           ,0x00, R_AGC1_MAX           ,0xff, R_AGC1_MIN           ,0x00, 
R_AGCR               ,0xbc, R_AGC2TH             ,0x00, R_AGC12C             ,0x40, R_AGCCTRL1           ,0x85, 
R_AGCCTRL2           ,0x18, R_AGC1VAL1           ,0xff, R_AGC1VAL2           ,0x0f, R_AGC2VAL1           ,0x71, 
R_AGC2VAL2           ,0x02, R_AGC2_PGA           ,0x00, R_OVF_RATE1          ,0x00, R_OVF_RATE2          ,0x00, 
R_GAIN_SRC1          ,0xdc, R_GAIN_SRC2          ,0x80, R_INC_DEROT1         ,0x55, R_INC_DEROT2         ,0x5d, 
R_PPM_CPAMP_DIR      ,0x62, R_PPM_CPAMP_INV      ,0x00, R_FREESTFE_1         ,0x03, R_FREESTFE_2         ,0x1c, 
R_DCOFFSET           ,0x7e, R_EN_PROCESS         ,0xb1, R_SDI_SMOOTHER       ,0xff, R_FE_LOOP_OPEN       ,0x00, 
R_FREQOFF1           ,0x00, R_FREQOFF2           ,0x00, R_FREQOFF3           ,0x00, R_TIMOFF1            ,0x00, 
R_TIMOFF2            ,0x00, R_EPQUAL             ,0x05, R_EPQAUTO            ,0x06, R_CHP_TAPS           ,0x03, 
R_CHP_DYN_COEFF      ,0x13, R_PPM_STATE_MAC      ,0x23, R_INR_THRESHOLD      ,0xc0, R_EPQ_TPS_ID_CELL    ,0x89, 
R_EPQ_CFG            ,0x00, R_EPQ_STATUS         ,0xe8, R_FECM               ,0X00, R_VTHR0              ,0x1e, 
R_VTHR1              ,0x14, R_VTHR2              ,0x0f, R_VTHR3              ,0x09, R_VTHR4              ,0x00, 
R_VTHR5              ,0x05, R_FREE_VIT           ,0x00, R_VITPROG            ,0x92, R_PRATE              ,0xaf, 
R_VSEARCH            ,0x30, R_RS                 ,0xfc, R_RSOUT              ,0x05, R_ERRCTRL1           ,0x12, 
R_ERRCNTM1           ,0x00, R_ERRCNTL1           ,0x00, R_ERRCTRL2           ,0xb3, R_ERRCNTM2           ,0x00, 
R_ERRCNTL2           ,0x00, R_FREE_DRS           ,0x00, R_VERROR             ,0x00, R_ERRCTRL3           ,0x12, 
R_ERRCNTM3           ,0x00, R_ERRCNTL3           ,0x00, R_DILSTK1            ,0x00, R_DILSTK0            ,0x03, 
R_DIL_BWSTK1         ,0x00, R_DIL_BWST0          ,0x03, R_LNBRX              ,0x80, R_RS_TC              ,0xb0, 
R_VIT_BIST           ,0x07, R_IIR_CELL_NB        ,0x8d, R_IIR_CX_COEFF1_MSB  ,0x00, R_IIR_CX_COEFF1_LSB  ,0x00, 
R_IIR_CX_COEFF2_MSB  ,0x00, R_IIR_CX_COEFF2_LSB  ,0x00, R_IIR_CX_COEFF3_MSB  ,0x00, R_IIR_CX_COEFF3_LSB  ,0x00, 
R_IIR_CX_COEFF4_MSB  ,0x00, R_IIR_CX_COEFF4_LSB  ,0x00, R_IIR_CX_COEFF5_MSB  ,0x00, R_IIR_CX_COEFF5_LSB  ,0x00, 
R_FEPATH_CFG         ,0x00, R_PMC1_FUNC          ,0x25, R_PMC1_FORCE         ,0x00, R_PMC2_FUNC          ,0x00, 
R_DIG_AGC_R          ,0xe0, R_COMAGC_TARMSB      ,0x0b, R_COM_AGC_TAR_ENMODE ,0x41, R_COM_AGC_CFG        ,0x3a, 
R_COM_AGC_GAIN1      ,0x39, R_AUT_AGC_TARGET_MSB ,0x00, R_LOCK_DETECT_MSB    ,0x06, R_AGCTAR_LOCK_LSBS   ,0x00, 
R_AUT_GAIN_EN        ,0xf4, R_AUT_CFG            ,0xf0, R_LOCK_N             ,0x20, R_INT_X_3            ,0x7f, 
R_INT_X_2            ,0xff, R_INT_X_1            ,0xff, R_INT_X_0            ,0xff, R_MIN_ERR_X_MSB      ,0x0f, 
R_STATUS_ERR_DA      ,0x0f, R_COR_CTL            ,0x20, R_COR_STAT           ,0xf6, R_COR_INTEN          ,0x00, 
R_COR_INTSTAT        ,0x3f, R_COR_MODEGUARD      ,0x03, R_AGC_CTL            ,0x18, R_AGC_MANUAL1        ,0x00, 
R_AGC_MANUAL2        ,0x00, R_AGC_TARGET         ,0x20, R_AGC_GAIN1          ,0x73, R_AGC_GAIN2          ,0x1a, 
R_RESERVED_1         ,0x00, R_RESERVED_2         ,0x00, R_RESERVED_3         ,0x00, R_CAS_CTL            ,0x44, 
R_CAS_FREQ           ,0xb3, R_CAS_DAGCGAIN       ,0x11, R_SYR_CTL            ,0x00, R_SYR_STAT           ,0x17, 
R_SYR_NCO1           ,0x00, R_SYR_NCO2           ,0x00, R_SYR_OFFSET1        ,0x00, R_SYR_OFFSET2        ,0x00, 
R_FFT_CTL            ,0x00, R_SCR_CTL            ,0x50, R_PPM_CTL1           ,0x38, R_TRL_CTL            ,0x14, 
R_TRL_NOMRATE1       ,0xad, R_TRL_NOMRATE2       ,0x56, R_TRL_TIME1          ,0xdc, R_TRL_TIME2          ,0xf3, 
R_CRL_CTL            ,0x4f, R_CRL_FREQ1          ,0xbb, R_CRL_FREQ2          ,0x57, R_CRL_FREQ3          ,0x00, 
R_CHC_CTL1           ,0x11, R_CHC_SNR            ,0xe8, R_BDI_CTL            ,0x00, R_DMP_CTL            ,0x00, 
R_TPS_RCVD1          ,0x32, R_TPS_RCVD2          ,0x00, R_TPS_RCVD3          ,0x00, R_TPS_RCVD4          ,0x31, 
R_TPS_ID_CELL1       ,0x00, R_TPS_ID_CELL2       ,0x00, R_TPS_RCVD5_SET1     ,0x00, R_TPS_SET2           ,0x00, 
R_TPS_SET3           ,0x00, R_TPS_CTL            ,0x00, R_CTL_FFTOSNUM       ,0x1a, R_TEST_SELECT        ,0x09, 
R_MSC_REV            ,0x0a, R_PIR_CTL            ,0x00, R_SNR_CARRIER1       ,0xa1, R_SNR_CARRIER2       ,0x9a, 
R_PPM_CPAMP          ,0xbc, R_TSM_AP0            ,0x00, R_TSM_AP1            ,0x00, R_TSM_AP2            ,0x00, 
R_TSM_AP3            ,0x00, R_TSM_AP4            ,0x00, R_TSM_AP5            ,0x00, R_TSM_AP6            ,0x00, 
R_TSM_AP7            ,0x00, R_TSTRES             ,0x00, R_ANACTRL            ,0x00, R_TSTBUS             ,0x00, 
R_TSTRATE            ,0x00, R_CONSTMODE          ,0x00, R_CONSTCARR1         ,0x00, R_CONSTCARR2         ,0x00, 
R_ICONSTEL           ,0x00, R_QCONSTEL           ,0x00, R_TSTBISTRES0        ,0x02, R_TSTBISTRES1        ,0x00, 
R_TSTBISTRES2        ,0x00, R_TSTBISTRES3        ,0x00, R_RF_AGC1            ,0xff, R_RF_AGC2            ,0x83, 
R_ANADIGCTRL         ,0x01, R_PLLMDIV            ,0x00, R_PLLSETUP           ,0x10, R_DUAL_AD12          ,0x08, 
R_TSTBIST            ,0x00, R_PAD_COMP_CTRL      ,0x00, R_PAD_COMP_WR        ,0x00, R_PAD_COMP_RD        ,0xE0,
0xff,0xff};

static int stv0360_writereg (struct stv0360_state* state, u8 reg, u8 data)
{
	int ret  = 0;
	u8 buf [] = { reg, data };
	struct i2c_msg msg = { .addr = state->demod_address, .flags = 0, .buf = buf, .len = 2 };

	ret = i2c_transfer (state->i2c, &msg, 1); 

	if (ret != 1)
		printk("%s: error (addr == 0x%02x,reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			__FUNCTION__, msg.addr,reg, data, ret);

	return (ret != 1) ? -1 : 0;
}

static int stv0360_readreg (struct stv0360_state* state, u8 reg)
{
	int ret;
	u8 b0 [] = { reg };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { .addr = state->demod_address, .flags = 0, .buf = b0, .len = 1 },
			   { .addr = state->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	//dprintk ("%s %x %x %x\n", __FUNCTION__,state->demod_address,reg,state->i2c);

	ret = i2c_transfer (state->i2c, msg, 1);
	ret |= i2c_transfer (state->i2c, msg+1, 1);

	if (ret != 1) {
		printk("%s: error (reg == 0x%02x,  ret == %i)\n",
			__FUNCTION__, reg, ret);
	  return -EIO;
	}
	return b1[0];
}

static int stv0360_set_inversion (struct stv0360_state* state, int inversion)
{
	u8 val;

	dprintk ("%s\n", __FUNCTION__);

	switch (inversion) {
	case INVERSION_AUTO:
	  return -EOPNOTSUPP;
	case INVERSION_ON:
	  val = stv0360_readreg (state, R_GAIN_SRC1);
		return stv0360_writereg (state, R_GAIN_SRC1, val | 0x80);
	case INVERSION_OFF:
		val = stv0360_readreg (state, R_GAIN_SRC1);
		return stv0360_writereg (state, R_GAIN_SRC1, val & ~0x80);
	default:
		return -EINVAL;
	}
}

static int stv0360_set_tps (struct stv0360_state *state, struct dvb_ofdm_parameters *p)
{
  u8 val;
//  u8 status;

	dprintk ("%s\n", __FUNCTION__);

	if ((p->code_rate_HP < FEC_1_2) || ((p->code_rate_HP > FEC_7_8) && (p->code_rate_HP != FEC_AUTO)))
		return -EINVAL;

	if ((p->code_rate_LP < FEC_1_2) || ((p->code_rate_LP > FEC_7_8) && (p->code_rate_LP != FEC_AUTO)))
		return -EINVAL;

	if (p->code_rate_HP == FEC_4_5 || p->code_rate_LP == FEC_4_5)
		return -EINVAL;

	if (p->guard_interval < GUARD_INTERVAL_1_32 ||
	    (p->guard_interval > GUARD_INTERVAL_1_4 && p->guard_interval != GUARD_INTERVAL_AUTO))
		return -EINVAL;

	if (p->transmission_mode != TRANSMISSION_MODE_2K &&
	    p->transmission_mode != TRANSMISSION_MODE_8K && 
	    p->transmission_mode != TRANSMISSION_MODE_AUTO)
		return -EINVAL;

	if (p->constellation != QPSK &&
	    p->constellation != QAM_16 &&
	    p->constellation != QAM_64)
		return -EINVAL;

	if (p->hierarchy_information < HIERARCHY_NONE ||
	    p->hierarchy_information > HIERARCHY_4)
		return -EINVAL;

#if 0
	if (p->bandwidth < BANDWIDTH_8_MHZ && p->bandwidth > BANDWIDTH_6_MHZ)
		return -EINVAL;
#endif

#warning we currently only support 8MHz
	if (p->bandwidth != BANDWIDTH_8_MHZ) return -EINVAL;

#if 0
	if (p->bandwidth == BANDWIDTH_7_MHZ)
		stv0360_writereg (state, 0x09, stv0360_readreg (state, 0x09 | 0x10));
	else
		stv0360_writereg (state, 0x09, stv0360_readreg (state, 0x09 & ~0x10));
#endif

        val  = (p->transmission_mode == TRANSMISSION_MODE_8K) ? (1<<2) : 0;
	val |= p->guard_interval & 3;

	val |= 1<<4; /* Force it added PB 20/11/2006 */

	stv0360_writereg (state, R_COR_MODEGUARD, val);
	//stv0360_writereg (state, R_CHP_TAPS,(p->transmission_mode == TRANSMISSION_MODE_8K) ? 0x3 : 0x1);

	stv0360_writereg (state, R_COR_CTL, 0);

	// should be 5
	msleep(5);

	stv0360_writereg (state, R_COR_CTL, (1<<5));

	if (p->transmission_mode == TRANSMISSION_MODE_2K)
	  stv0360_writereg(state, R_CHP_TAPS, 0x1);
	else
	  stv0360_writereg(state, R_CHP_TAPS, 0x3);
				    
	return 0;
}

static int stv0360_get_tps (struct stv0360_state* state, struct dvb_ofdm_parameters *p)
{
	static const fe_modulation_t qam_tab [3] = { QPSK, QAM_16, QAM_64 };
	static const fe_code_rate_t fec_tab [5] = { FEC_1_2, FEC_2_3, FEC_3_4,
						    FEC_5_6, FEC_7_8 };
	u8 val;

	if (!(stv0360_readreg(state, R_STATUS) & 0x80))  /*  tps valid? */
		return -EAGAIN;

	val = stv0360_readreg (state, R_TPS_RCVD2);

	if (((val>>4) & 0x7) > 3)
	  printk(KERN_ERR "%s: Invalid Hierarchy detected\n",__FUNCTION__);
	else
	  p->hierarchy_information = HIERARCHY_NONE + ((val>>4) & 0x7);

	if ((val & 0x3) > 2)
	  printk(KERN_ERR "%s: Invalid Modulation detected\n",__FUNCTION__);
	else
		p->constellation = qam_tab[val & 0x3];

	val = stv0360_readreg (state, R_TPS_RCVD3);

	if ((val & 0x07) > 4)
	  printk(KERN_ERR "%s: Invalid FEC rate detected\n",__FUNCTION__);
	else
	  p->code_rate_HP = fec_tab[val & 0x07];

	if (((val >> 4) & 0x07) > 4)
	  printk(KERN_ERR "%s: Invalid FEC rate detected\n",__FUNCTION__);
	else
	  p->code_rate_LP = fec_tab[(val>>4) & 0x07];

	val = stv0360_readreg (state, R_TPS_RCVD4);

	p->guard_interval = GUARD_INTERVAL_1_32 + ((val >> 4) & 0x3);
	p->transmission_mode = TRANSMISSION_MODE_2K + (val & 0x1);

	return 0;
}

#if 0
#define M8_F_GAIN_SRC_HI        (0x0A+1)
#define M8_F_GAIN_SRC_LO        (0xB4+4)

#define M8_F_TRL_NOMRATE0       (0x00+1)
#define M8_F_TRL_NOMRATE8_1     (0xAC+1)
#define M8_F_TRL_NOMRATE16_9    (0x56)

#define M8_GAIN_SRC_HI        	0x0B
#define M8_GAIN_SRC_LO        	0xB8 	/*For normal IF Mode*/ 

#define M8_E_GAIN_SRC_HI                0x08
#define M8_E_GAIN_SRC_LO                0x86    /* For long path IF / IQ modes */
#endif

static unsigned short int CellsCoeffs_8MHz[6][5]={
                                                {0x10EF,0xE205,0x10EF,0xCE49,0x6DA7},  /* CELL 1 COEFFS */
                                                {0x2151,0xc557,0x2151,0xc705,0x6f93},  /* CELL 2 COEFFS */
                                                {0x2503,0xc000,0x2503,0xc375,0x7194},  /* CELL 3 COEFFS */
                                                {0x20E9,0xca94,0x20e9,0xc153,0x7194},  /* CELL 4 COEFFS */
                                                {0x06EF,0xF852,0x06EF,0xC057,0x7207},  /* CELL 5 COEFFS */
                                                {0x0000,0x0ECC,0x0ECC,0x0000,0x3647}   /* CELL 6 COEFFS */
                                          } ;


static int stv0362_set_filter_coefficents(struct stv0360_state* state, unsigned short int coeffs[6][5])
{
  int i,j;
  int ret = 0;

#define	F_IIRCELLNB		0x7

  for (i=0;i<6;i++) {
    ret |= stv0360_writereg( state, R_IIR_CELL_NB,
			     (stv0360_readreg( state, R_IIR_CELL_NB) & ~F_IIRCELLNB) | i);

    for (j=0;j<5;j++) {
      ret |= stv0360_writereg( state, R_IIR_CX_COEFF1_MSB + (j << 1),
			       coeffs[i][j] >> 8 & 0xff);

      ret |= stv0360_writereg( state, R_IIR_CX_COEFF1_LSB + (j << 1) ,
			       coeffs[i][j] & 0xff);
    }
  }

  return ret;
}

static int stv0362_agc_IIR_lock_detector_set(struct stv0360_state* state)
{
  int ret = 0;

#define	F_LOCK_DETECT_LSB	     0xF
#define	F_AUT_AGC_TARGET_LSB	     0xF0
#define	F_LOCK_DETECT_CHOICE         0x3

  ret  = stv0360_writereg( state, R_AGCTAR_LOCK_LSBS, 
			   stv0360_readreg(state, R_AGCTAR_LOCK_LSBS) & ~F_LOCK_DETECT_LSB);

  /* Lock detect 1 */
  ret |= stv0360_writereg( state, R_LOCK_N, 
			   (stv0360_readreg(state, R_AGCTAR_LOCK_LSBS) & ~F_LOCK_DETECT_CHOICE) | 0x0);
  ret |= stv0360_writereg( state, R_LOCK_DETECT_MSB,  0x6);
  ret |= stv0360_writereg( state, R_AGCTAR_LOCK_LSBS, 0x40);

  /* Lock detect 2 */
  ret |= stv0360_writereg( state, R_LOCK_N, 
			   (stv0360_readreg(state, R_AGCTAR_LOCK_LSBS) & ~F_LOCK_DETECT_CHOICE) | 0x1);
  ret |= stv0360_writereg( state, R_LOCK_DETECT_MSB,  0x6);
  ret |= stv0360_writereg( state, R_AGCTAR_LOCK_LSBS, 0x40);

  /* Lock detect 3 */
  ret |= stv0360_writereg( state, R_LOCK_N, 
			   (stv0360_readreg(state, R_AGCTAR_LOCK_LSBS) & ~F_LOCK_DETECT_CHOICE) | 0x2);
  ret |= stv0360_writereg( state, R_LOCK_DETECT_MSB,  0x1);
  ret |= stv0360_writereg( state, R_AGCTAR_LOCK_LSBS, 0x00);

  /* Lock detect 4 */
  ret |= stv0360_writereg( state, R_LOCK_N, 
			   (stv0360_readreg(state, R_AGCTAR_LOCK_LSBS) & ~F_LOCK_DETECT_CHOICE) | 0x3);
  ret |= stv0360_writereg( state, R_LOCK_DETECT_MSB,  0x1);
  ret |= stv0360_writereg( state, R_AGCTAR_LOCK_LSBS, 0x00);

  return ret;
}

int stv0360_i2c_gate_ctrl(struct dvb_frontend* fe, int enable)
{
  struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

  if (enable)
    return stv0360_writereg (state, R_I2CRPT, 0xd3/*0xd3*/);  /* open i2c bus switch d3 */
  else
    return stv0360_writereg (state, R_I2CRPT, 0x51);  /* 53 close i2c bus switch */
}

static int stv0360_init (struct dvb_frontend* fe)
{
  struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;
  int i=0;

	dprintk("init chip\n");

	while (!(state->init_tab[i] == 0xff && state->init_tab[i+1]==0xff))
	{
	  stv0360_writereg (state, state->init_tab[i], state->init_tab[i+1]);
	  i+=2;
	}

	stv0360_writereg(state, R_RS, (stv0360_readreg(state, R_RS) & ~(1<<6)) | 
			 ( state->serial_not_parallel ? (1<<6) : 0 ));

	if (fe->ops.tuner_ops.init)
	  fe->ops.tuner_ops.init(fe);

	stv0362_agc_IIR_lock_detector_set(state);

	stv0362_set_filter_coefficents(state, CellsCoeffs_8MHz);

	return 0;
}

static int stv0360_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

	//u16 rs_ber = (stv0360_readreg (state, 0x0d) << 9)
	//	   | (stv0360_readreg (state, 0x0e) << 1);

	u8 sync = stv0360_readreg (state, R_STATUS);
#if 0
	u8 tps  = stv0360_readreg (state, R_TPS_RCVD2);
	u8 hplp  = stv0360_readreg (state, R_TPS_RCVD3);
	u8 guardmode  = stv0360_readreg (state, R_TPS_RCVD4);
	u8 cellid  = stv0360_readreg (state, R_TPS_CELLID);
	u32 freq1 = stv0360_readreg (state, R_CRL_FREQ1);
	u32 freq2 = stv0360_readreg (state, R_CRL_FREQ2);
	u32 freq3 = stv0360_readreg (state, R_CRL_FREQ3);
	u32 ffttosnum = stv0360_readreg (state, R_CTL_FFTOSNUM);
	u32 lnbrx = stv0360_readreg (state, R_LNBRX);
	u32 verror = stv0360_readreg(state, R_VERROR);
	u32 corstate = stv0360_readreg(state, R_COR_STAT);
	u32 readit = stv0360_readreg(state,R_PPM_CPAMP);
#endif
	u32 adc1;

//	int stat;

	sync = stv0360_readreg (state, R_STATUS);

	//stv0360_writereg(state, R_EN_RF_AGC1, 0x0);

	adc1 = stv0360_readreg (state, R_RF_AGC1);

	//stv0360_writereg(state, R_EN_RF_AGC1, 0x80);
	if (adc1>=157)
	{
	  adc1 = stv0360_readreg(state, R_AGC2VAL1) | (stv0360_readreg(state, R_AGC2VAL2) & 0xf) << 8;	
	}
	
	*status = 0;

	if (sync & (1<<5))
		*status |= FE_HAS_SIGNAL;

	if (sync & (1<<6))
		*status |= FE_HAS_CARRIER;

	if (sync & (1<<3))
		*status |= FE_HAS_VITERBI;  

	if (sync & (1<<7))
		*status |= FE_HAS_SYNC;

	if (sync & (1<<3))
		*status |= FE_HAS_LOCK;

#if 0 // only need to do this when we found lock
	stat = stv0360_readreg(state, R_STATUS );

	if (stat & (1<<7)) {
	  int hp,lp,vth,punc;

	  hp = stv0360_readreg(state, R_TPS_RCVD3) & 0x7;
	  lp = (stv0360_readreg(state, R_TPS_RCVD3) >> 4) & 0x7;

	  /* Is it a LP or HP rate */
	  if ((stv0360_readreg(state, R_TPS_RCVD2) >> 4))
	    punc = lp; else punc = hp;

	  if (punc==4) hp = 5;

	  switch(punc)
	  {
	  case 0:
	      vth = stv0360_readreg(state, R_VTH0);
	      if (vth <60) stv0360_writereg(state,R_VTH0,45);
	      break;
	  case 1:
	      vth = stv0360_readreg(state, R_VTH1);
	      if (vth <40) stv0360_writereg(state,R_VTH1,30);
	      break;
	  case 2:
	      vth = stv0360_readreg(state, R_VTH2);
	      if (vth <30) stv0360_writereg(state,R_VTH2,23);
	      break;
	  case 3:
	      vth = stv0360_readreg(state, R_VTH3);
	      if (vth <18) stv0360_writereg(state,R_VTH3,14);
	      break;
	  case 5:
	      vth = stv0360_readreg(state, R_VTH5);
	      if (vth <10) stv0360_writereg(state,R_VTH5,8);
	      break;
	  }

	}
#endif

	return 0;
}

static int stv0360_read_ber(struct dvb_frontend* fe, u32* ber)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

	*ber = stv0360_readreg(state, R_ERRCNTL2) | stv0360_readreg(state, R_ERRCNTM2) << 8;

	return 0;
}

static int stv0360_read_signal_strength(struct dvb_frontend* fe, u16* signal_strength)
{
//	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

	*signal_strength = 0xdead;

	return 0;
}

static int stv0360_read_snr(struct dvb_frontend* fe, u16* snr)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

	*snr = stv0360_readreg(state, R_SNR_CARRIER1) | (stv0360_readreg(state, R_SNR_CARRIER2) & 0x1f) << 8;

	return 0;
}

static int stv0360_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;

	*ucblocks = stv0360_readreg(state, R_ERRCNTL1) | stv0360_readreg(state, R_ERRCNTM1) << 8;

	return 0;
}

static int stv0360_set_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;
	int ctrl = 0,gain = 0;

	stv0360_writereg(state, R_TOPCTRL, 0x0);
	stv0360_writereg(state, R_FEPATH_CFG, 0x0);

#define M8_F_TRL_NOMRATE0       0x01            /* ok */
#define M8_F_TRL_NOMRATE8_1     0xAB    /* ok */
#define M8_F_TRL_NOMRATE16_9    0x56    /* ok */

	ctrl = stv0360_readreg(state, R_TRL_CTL);

	stv0360_writereg(state, R_TRL_CTL,      (ctrl & 0x7f)| (M8_F_TRL_NOMRATE0<<7));
	stv0360_writereg(state, R_TRL_NOMRATE1, M8_F_TRL_NOMRATE8_1);
	stv0360_writereg(state, R_TRL_NOMRATE2, M8_F_TRL_NOMRATE16_9);

#define M8_GAIN_SRC_HI          0x0B
#define M8_GAIN_SRC_LO          0xB8    /*For normal IF Mode*/

	gain = stv0360_readreg(state, R_GAIN_SRC1);

	stv0360_writereg(state, R_GAIN_SRC1, (gain & 0xf0) |M8_GAIN_SRC_HI);
	stv0360_writereg(state, R_GAIN_SRC2, M8_GAIN_SRC_LO);


	//stv0360_writereg (state, R_I2CRPT, 0x51 | 0x80/*0xd3*/);  /* open i2c bus switch d3 */
	//	state->config->pll_set(fe, p);
	//stv0360_writereg (state, R_I2CRPT, 0x51/*0x53*/);   /* close i2c bus switch 53 */

	if (fe->ops.tuner_ops.set_params)
	  fe->ops.tuner_ops.set_params(fe,p);

	stv0360_writereg(state, R_VTH0,0x1E);
	stv0360_writereg(state, R_VTH1,0x14);
	stv0360_writereg(state, R_VTH2,0x0F);
	stv0360_writereg(state, R_VTH3,0x09);
       	stv0360_writereg(state, R_VTH4,0x0);
	stv0360_writereg(state, R_VTH5,0x05);

	

	// it 0x4 should be last read value???
	//stv0360_writereg(state, R_PR, 0x4 | 0x30);
	stv0360_writereg(state, R_PR, 0xaf);

	stv0360_writereg(state, R_VSEARCH, 0x30);

	//stv0360_writereg(state, R_VSEARCH, 0x4 | 0x30);

	stv0360_writereg(state, R_PR, stv0360_readreg(state, R_PR) | 0x80);
	//msleep(5);

	stv0360_set_inversion (state, p->inversion);
	//msleep(5);

	stv0360_set_tps (state, &p->u.ofdm);

	//msleep(5);

	
	return 0;
}

static int stv0360_get_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;
	u8 reg = stv0360_readreg (state, R_GAIN_SRC1);

	p->inversion = reg & 0x80 ? INVERSION_ON : INVERSION_OFF;
	return stv0360_get_tps (state, &p->u.ofdm);
}

static int stv0360_get_tune_settings(struct dvb_frontend* fe, struct dvb_frontend_tune_settings* fesettings)
{
        fesettings->min_delay_ms = 150;
        fesettings->step_size = 166667;
        fesettings->max_drift = 166667*2;
        return 0;
}

static void stv0360_release(struct dvb_frontend* fe)
{
	struct stv0360_state* state = (struct stv0360_state*) fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops stv0360_ops;

struct dvb_frontend* stv0360_attach(struct i2c_adapter* i2c, u8 demod_address, int serial_not_parallel)
{
	struct stv0360_state* state = NULL;
	int reg;
     

	/* allocate memory for the internal state */
	state = (struct stv0360_state*) kzalloc(sizeof(struct stv0360_state), GFP_KERNEL);
	if (!state)
	  return NULL;

	/* setup the state */
	state->demod_address = demod_address;
	state->serial_not_parallel = serial_not_parallel;
	state->i2c = i2c;
	state->demod_address = demod_address;
	state->crystal_freq  = 27; // We only support 27 at the moment
	memcpy(&state->frontend.ops, &stv0360_ops, sizeof(struct dvb_frontend_ops));

	/* check if the demod is there */
	state->version = reg = stv0360_readreg(state, R_ID); 
	dprintk("%s: found %x\n",__FUNCTION__,reg);
	if (reg != 0x10 && reg != 0x21  && reg != 0x41) goto error;

	if (reg == 0x10 || reg == 0x21) state->init_tab = init_tab;
	else state->init_tab = init_tab_362;

	/* create dvb_frontend */
	//state->frontend.ops = state->ops;
	state->frontend.demodulator_priv = state;
	state->frontend.ops.i2c_gate_ctrl = stv0360_i2c_gate_ctrl;	
	return &state->frontend;

error:
	if (state) kfree(state);
	return NULL;
}

static struct dvb_frontend_ops stv0360_ops = {

	.info = {
		.name 			= "ST STV0360 DVB-T",
		.type 			= FE_OFDM,
		.frequency_min 		= 470000000,
		.frequency_max 		= 860000000,
		.frequency_stepsize 	= 166667,
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		      FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		      FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_TRANSMISSION_MODE_AUTO |
		      FE_CAN_GUARD_INTERVAL_AUTO |
	              FE_CAN_RECOVER
	},

	.release = stv0360_release,

	.init = stv0360_init,

	.set_frontend = stv0360_set_frontend,
	.get_frontend = stv0360_get_frontend,
	.get_tune_settings = stv0360_get_tune_settings,

	.read_status = stv0360_read_status,
	.read_ber = stv0360_read_ber,
	.read_signal_strength = stv0360_read_signal_strength,
	.read_snr = stv0360_read_snr,
	.read_ucblocks = stv0360_read_ucblocks,
};

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");

MODULE_DESCRIPTION("ST STV0360 DVB-T Demodulator driver");
MODULE_AUTHOR("Peter Bennett");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(stv0360_attach);
