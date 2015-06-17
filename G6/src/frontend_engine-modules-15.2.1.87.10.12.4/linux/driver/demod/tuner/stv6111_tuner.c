/*********************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name :stv6111_tuner.c
Author :			LLA

tuner lla file

Date		Modification			Name
----		------------			-------

************************************************************************/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>
#include "fe_sat_tuner.h"
#include "stv6111_tuner.h"
#include <gen_macros.h>

	/* STV6111 tuner definition */
/*CTRL1*/
#define RSTV6111_CTRL1  0x0000
#define FSTV6111_K  0x000000f8
#define FSTV6111_RDIV  0x00000004
#define FSTV6111_ODIV  0x00000003

/*CTRL2*/
#define RSTV6111_CTRL2  0x0001
#define FSTV6111_BB_MAG  0x000100c0
#define FSTV6111_BB_MODE  0x00010030
#define FSTV6111_DCLOOP_OFF  0x00010008
#define FSTV6111_GPOUT  0x00010004
#define FSTV6111_BB_WBD  0x00010002
#define FSTV6111_RX_ON  0x00010001

/*CTRL3*/
#define RSTV6111_CTRL3  0x0002
#define FSTV6111_LNA_SEL_GAIN  0x00020080
#define FSTV6111_LNA_ADC_OFF  0x00020040
#define FSTV6111_LNA_ADC_GET  0x00020020
#define FSTV6111_LNA_AGC  0x0002001f

/*CTRL4*/
#define RSTV6111_CTRL4  0x0003
#define FSTV6111_LO_DIV  0x00030080
#define FSTV6111_LNA_AGC_MODE  0x00030060
#define FSTV6111_LNA_AGC_LOW  0x00030010
#define FSTV6111_LNA_AGC_HIGH  0x00030008
#define FSTV6111_LNA_AGC_REF  0x00030007

/*CTRL5*/
#define RSTV6111_CTRL5  0x0004
#define FSTV6111_N_LSB  0x000400ff

/*CTRL6*/
#define RSTV6111_CTRL6  0x0005
#define FSTV6111_F_L  0x000500fe
#define FSTV6111_N_MSB  0x00050001

/*CTRL7*/
#define RSTV6111_CTRL7  0x0006
#define FSTV6111_F_M  0x000600ff

/*CTRL8*/
#define RSTV6111_CTRL8  0x0007
#define FSTV6111_PLL_ICP  0x000700e0
#define FSTV6111_VCO_AMP  0x00070018
#define FSTV6111_F_H  0x00070007

/*CTRL9*/
#define RSTV6111_CTRL9  0x0008
#define FSTV6111_BB_FILT_SEL  0x000800fc
#define FSTV6111_PLL_SD_ON  0x00080002
#define FSTV6111_PLL_LOCK  0x00080001

/*CTRL10*/
#define RSTV6111_CTRL10  0x0009
#define FSTV6111_ID  0x000900e0
#define FSTV6111_BB_CAL_START  0x00090008
#define FSTV6111_VCO_CAL_START  0x00090004
#define FSTV6111_TCAL  0x00090003

/*CTRL11*/
#define RSTV6111_CTRL11  0x000a
#define FSTV6111_SPARE  0x000a00ff

#if 0	/* TEST registers for internal use only */
/*TEST1*/
#define RSTV6111_TEST1  0x000b
#define FSTV6111_VCO_CAL_EXT  0x000b00fc
#define FSTV6111_VCO_CAL_OFF  0x000b0002
#define FSTV6111_VCO_COMP  0x000b0001

/*TEST2*/
#define RSTV6111_TEST2  0x000c
#define FSTV6111_BB_CAL_EXT  0x000c00fc
#define FSTV6111_BB_CAL_OFF  0x000c0002
#define FSTV6111_LNA_AGC_TLOCK  0x000c0001

/*TEST3*/
#define RSTV6111_TEST3  0x000d
#define FSTV6111_PLL_CAL_EXT  0x000d00fc
#define FSTV6111_PLL_CAL_OFF  0x000d0002
#define FSTV6111_TEST_SPARE_LSB  0x000d0001

/*TEST4*/
#define RSTV6111_TEST4  0x000e
#define FSTV6111_TEST_SPARE_MSB  0x000e0080
#define FSTV6111_OLT  0x000e0040
#define FSTV6111_CKADC_DIV  0x000e0030
#define FSTV6111_DIVTEST  0x000e000c
#define FSTV6111_TEST_PLL_CP  0x000e0003

/*TEST5*/
#define RSTV6111_TEST5  0x000f
#define FSTV6111_BB_R_EXT  0x000f00f0
#define FSTV6111_DCLOOP_CLK_DIV  0x000f0008
#define FSTV6111_DCLOOP2_OFF  0x000f0004
#define FSTV6111_DCLOOP1_OFF  0x000f0002
#define FSTV6111_MIXER_CMFB_PWD  0x000f0001
#endif	/* formerly excluded lines */

static FE_LLA_LOOKUP_t FE_STV6111_LNAGain_NF_LookUp = {
					32,
					{
					/*Gain *100dB*/	  /*Reg*/
						{ 2572, 0		 },
						{ 2575, 1		 },
						{ 2580, 2		 },
						{ 2588, 3		 },
						{ 2596, 4		 },
						{ 2611, 5		 },
						{ 2633, 6		 },
						{ 2664, 7		 },
						{ 2701, 8		 },
						{ 2753, 9		 },
						{ 2816, 10	 },
						{ 2902, 11	 },
						{ 2995, 12	 },
						{ 3104, 13	 },
						{ 3215, 14	 },
						{ 3337, 15	 },
						{ 3492, 16	 },
						{ 3614, 17	 },
						{ 3731, 18	 },
						{ 3861, 19	 },
						{ 3988, 20	 },
						{ 4124, 21	 },
						{ 4253, 22	 },
						{ 4386, 23	 },
						{ 4505, 24	 },
						{ 4623, 25	 },
						{ 4726, 26	 },
						{ 4821, 27	 },
						{ 4903, 28	 },
						{ 4979, 29	 },
						{ 5045, 30	 },
						{ 5102, 31	 }
					}
			 };


static FE_LLA_LOOKUP_t FE_STV6111_LNAGain_IIP3_LookUp =	{
					32,
					{
					/*Gain *100dB*/	  /*reg*/
						{	1548, 0		 },
						{	1552, 1		 },
						{	1569, 2		 },
						{	1565, 3		 },
						{	1577, 4		 },
						{	1594, 5		 },
						{	1627, 6		 },
						{	1656, 7		 },
						{	1700, 8		 },
						{	1748, 9		 },
						{	1805, 10	 },
						{	1896, 11	 },
						{	1995, 12	 },
						{	2113, 13	 },
						{	2233, 14	 },
						{	2366, 15	 },
						{	2543, 16	 },
						{	2687, 17	 },
						{	2842, 18	 },
						{	2999, 19	 },
						{	3167, 20	 },
						{	3342, 21	 },
						{	3507, 22	 },
						{	3679, 23	 },
						{	3827, 24	 },
						{	3970, 25	 },
						{	4094, 26	 },
						{	4210, 27	 },
						{	4308, 28	 },
						{	4396, 29	 },
						{	4468, 30	 },
						{	4535, 31	 }
					}
			 };




static FE_LLA_LOOKUP_t FE_STV6111_Gain_RFAGC_LookUp =	{
					52,
					{	/*Gain *100dB*/	  /*reg*/
						{  4870, 0x3000 },
						{  4850, 0x3C00 },
						{  4800, 0x4500 },
						{  4750, 0x4800 },
						{  4700, 0x4B00 },
						{  4650, 0x4D00 },
						{  4600, 0x4F00 },
						{  4550, 0x5100 },
						{  4500, 0x5200 },
						{  4420, 0x5500 },
						{  4316, 0x5800 },
						{  4200, 0x5B00 },
						{  4119, 0x5D00 },
						{  3999, 0x6000 },
						{  3950, 0x6100 },
						{  3876, 0x6300 },
						{  3755, 0x6600 },
						{  3641, 0x6900 },
						{  3567, 0x6B00 },
						{  3425, 0x6F00 },
						{  3550, 0x7100 },
						{  3236, 0x7400 },
						{  3118, 0x7700 },
						{  3004, 0x7A00 },
						{  2917, 0x7C00 },
						{  2776, 0x7F00 },
						{  2635, 0x8200 },
						{  2516, 0x8500 },
						{  2406, 0x8800 },
						{  2290, 0x8B00 },
						{  2170, 0x8E00 },
						{  2073, 0x9100 },
						{  1949, 0x9400 },
						{  1836, 0x9700 },
						{  1712, 0x9A00 },
						{  1631, 0x9C00 },
						{  1515, 0x9F00 },
						{  1400, 0xA200 },
						{  1323, 0xA400 },
						{  1203, 0xA700 },
						{  1091, 0xAA00 },
						{  1011, 0xAC00 },
						{  904, 0xAF00 },
						{  787, 0xB200 },
						{  685, 0xB500 },
						{  571, 0xB800 },
						{  464, 0xBB00 },
						{  374, 0xBE00 },
						{  275, 0xC200 },
						{  181, 0xC600 },
						{  102, 0xCC00 },
						{  49, 0xD900 }
					}
				};



static FE_LLA_LOOKUP_t FE_STV6111_Gain_Channel_AGC_NF_LookUp = {
					55,  /*Gain *100dB*/	  /*reg*/
					{
						{  7082, 0x3000	 },
						{  7052, 0x4000	},
						{  7007, 0x4600	},
						{  6954, 0x4A00	 },
						{  6909, 0x4D00	},
						{  6833, 0x5100	 },
						{  6753, 0x5400	 },
						{  6659, 0x5700	 },
						{  6561, 0x5A00	 },
						{  6472, 0x5C00	 },
						{  6366, 0x5F00	 },
						{  6259, 0x6100	 },
						{  6151, 0x6400	 },
						{  6026, 0x6700	 },
						{  5920, 0x6900	 },
						{  5835, 0x6B00	 },
						{  5770, 0x6C00	 },
						{  5681, 0x6E00	 },
						{  5596, 0x7000	 },
						{  5503, 0x7200	 },
						{  5429, 0x7300	},
						{  5319, 0x7500	 },
						{  5220, 0x7700	 },
						{  5111, 0x7900	 },
						{  4983, 0x7B00	 },
						{  4876, 0x7D00	 },
						{  4755, 0x7F00	 },
						{  4635, 0x8100	 },
						{  4499, 0x8300	 },
						{  4405, 0x8500	 },
						{  4323, 0x8600	 },
						{  4233, 0x8800	 },
						{  4156, 0x8A00	 },
						{  4038, 0x8C00	 },
						{  3935, 0x8E00	 },
						{  3823, 0x9000	 },
						{  3712, 0x9200	 },
						{  3601, 0x9500	 },
						{  3511, 0x9700	 },
						{  3413, 0x9900	 },
						{  3309, 0x9B00	 },
						{  3213, 0x9D00	 },
						{  3088, 0x9F00	 },
						{  2992, 0xA100	 },
						{  2878, 0xA400	 },
						{  2769, 0xA700	 },
						{  2645, 0xAA00	 },
						{  2538, 0xAD00	 },
						{  2441, 0xB000	 },
						{  2350, 0xB600	 },
						{  2237, 0xBA00	 },
						{  2137, 0xBF00	 },
						{  2039, 0xC500	 },
						{  1938, 0xDF00	 },
						{  1927, 0xFF00	 }
					}
				};


static FE_LLA_LOOKUP_t FE_STV6111_Gain_Channel_AGC_IIP3_LookUp = {
					55,  /*Gain *100dB*/	  /*reg*/
					{
						{  7088, 0x3000	 },
						{  7071, 0x4000	},
						{  7011, 0x4600	},
						{  6949, 0x4A00	 },
						{  6932, 0x4D00	},
						{  6838, 0x5100	 },
						{  6753, 0x5400	 },
						{  6745, 0x5700	 },
						{  6574, 0x5A00	 },
						{  6493, 0x5C00	 },
						{  6389, 0x5F00	 },
						{  6312, 0x6100	 },
						{  6204, 0x6400	 },
						{  6116, 0x6700	 },
						{  6026, 0x6900	 },
						{  5912, 0x6B00	 },
						{  5842, 0x6C00	 },
						{  5772, 0x6E00	 },
						{  5689, 0x7000	 },
						{  5609, 0x7200	 },
						{  5555, 0x7300	},
						{  5433, 0x7500	 },
						{  5327, 0x7700	 },
						{  5209, 0x7900	 },
						{  5081, 0x7B00	 },
						{  4989, 0x7D00	 },
						{  4882, 0x7F00	 },
						{  4759, 0x8100	 },
						{  4626, 0x8300	 },
						{  4523, 0x8500	 },
						{  4468, 0x8600	 },
						{  4369, 0x8800	 },
						{  4288, 0x8A00	 },
						{  4182, 0x8C00	 },
						{  4039, 0x8E00	 },
						{  3998, 0x9000	 },
						{  3872, 0x9200	 },
						{  3724, 0x9500	 },
						{  3639, 0x9700	 },
						{  3549, 0x9900	 },
						{  3448, 0x9B00	 },
						{  3347, 0x9D00	 },
						{  3218, 0x9F00	 },
						{  3128, 0xA100	 },
						{  2978, 0xA400	 },
						{  2837, 0xA700	 },
						{  2748, 0xAA00	 },
						{  2651, 0xAD00	 },
						{  2542, 0xB000	 },
						{  2399, 0xB600	 },
						{  2259, 0xBA00	 },
						{  2181, 0xBF00	 },
						{  2058, 0xC500	 },
						{  1966, 0xDF00	 },
						{  1963, 0xFF00	 }
					}
				};

TUNER_Error_t STV6111_TunerInit(void *pTunerInit_v,
						STCHIP_Handle_t *TunerHandle)
{
	SAT_TUNER_Params_t	hTunerParams = NULL;
	STCHIP_Info_t	ChipInfo;
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;
	/*Changed to retain common function prototype for all Tuners: */
	SAT_TUNER_Params_t pTunerInit = (SAT_TUNER_Params_t)pTunerInit_v;
	STCHIP_Register_t		DefSTV6111Val[STV6111_NBREGS] = {
		{ RSTV6111_CTRL1, 0x7c },
		{ RSTV6111_CTRL2, 0x41 },
		{ RSTV6111_CTRL3, 0x8f },
		{ RSTV6111_CTRL4, 0x02 },
		{ RSTV6111_CTRL5, 0xce },
		{ RSTV6111_CTRL6, 0x54 },
		{ RSTV6111_CTRL7, 0x55 },
		{ RSTV6111_CTRL8, 0x45 },
		{ RSTV6111_CTRL9, 0x46 },
		{ RSTV6111_CTRL10, 0xbd },
		{ RSTV6111_CTRL11, 0x11 },

	};


	/*
	**	REGISTER CONFIGURATION
	**	 ----------------------
	*/
	#ifdef HOST_PC
	/* Allocation of the chip structure	*/
	hTunerParams = calloc(1, sizeof(SAT_TUNER_InitParams_t));
	#endif

#ifdef CHIP_STAPI
	if (*TunerHandle) {
		hTunerParams = (SAT_TUNER_Params_t) ((*TunerHandle)->pData);
		memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));
	} else {
		error = TUNER_INVALID_HANDLE;
	}
#endif

	if (hTunerParams == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams->Model = pTunerInit->Model; /* Tuner model */
	/* Reference Clock in Hz */
	hTunerParams->Reference = pTunerInit->Reference;
	hTunerParams->IF = pTunerInit->IF;/* IF Hz intermediate frequency */
	/* hardware IQ invertion */
	hTunerParams->IQ_Wiring = pTunerInit->IQ_Wiring;
	/* Wide band tuner(6130 like, band selection) */
	hTunerParams->BandSelect = pTunerInit->BandSelect;
	/* Demod Model used with this tuner */
	hTunerParams->DemodModel = pTunerInit->DemodModel;
#ifdef DLL
	hTunerParams->Lna_agc_mode = pTunerInit->Lna_agc_mode;
	hTunerParams->Lna_agc_ref = pTunerInit->Lna_agc_ref;
#endif
	/*fix Coverity CID 20142 */
	if (strlen((char *)pTunerInit->TunerName) < MAXNAMESIZE)
		/* Tuner name */
		strcpy((char *)ChipInfo.Name, (char *)pTunerInit->TunerName);
	else
		error = TUNER_TYPE_ERR;
	ChipInfo.RepeaterHost = pTunerInit->RepeaterHost; /* Repeater host */
	/* Repeater enable/disable function */
	ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;
	/* Tuner need to enable repeater */
	ChipInfo.Repeater = TRUE;
	/* Init tuner I2C address */
	ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;
	/* Store tunerparams pointer into Chip structure */
	ChipInfo.pData = hTunerParams;

	ChipInfo.NbRegs = STV6111_NBREGS;
	ChipInfo.NbFields = STV6111_NBFIELDS;
	ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8; /*same as 6110*/
	ChipInfo.WrStart = RSTV6111_CTRL1;
	ChipInfo.WrSize = 8;
	ChipInfo.RdStart = RSTV6111_CTRL1;
	ChipInfo.RdSize = 8;

	#ifdef HOST_PC
		hTuner = ChipOpen(&ChipInfo);
		(*TunerHandle) = hTuner;
	#endif
	#ifdef CHIP_STAPI
		hTuner = *TunerHandle; /*obtain hTuner */
		memcpy(hTuner, &ChipInfo, sizeof(STCHIP_Info_t));
	#endif

	if (((*TunerHandle) == NULL) || (hTuner == NULL))
		error = TUNER_INVALID_HANDLE;
	/*******************************
	**	CHIP REGISTER MAP IMAGE INITIALIZATION
	**	 ----------------------
	********************************/
	ChipUpdateDefaultValues(hTuner, DefSTV6111Val);

	/*******************************
	 **	REGISTER CONFIGURATION
	 **	 ----------------------
	 ********************************/

#ifdef HOST_PC	/* used only to generated DLL */
	ChipSetField(hTuner, FSTV6111_LNA_AGC_MODE, hTunerParams->Lna_agc_mode);
	ChipSetField(hTuner, FSTV6111_LNA_AGC_REF, hTunerParams->Lna_agc_ref);
#endif
	/*Update the clock divider before registers initialization */
	/*Allowed values 1, 2, 4 and 8*/
	switch (pTunerInit->OutputClockDivider) {
	case 1:
	default:
		ChipSetFieldImage(hTuner, FSTV6111_ODIV, 0);
		break;

	case 2:
		ChipSetFieldImage(hTuner, FSTV6111_ODIV, 1);
		break;

	case 4:
		ChipSetFieldImage(hTuner, FSTV6111_ODIV, 2);
		break;

	case 8:
	case 0: /*Tuner output clock not used then divide by 8*/
		ChipSetFieldImage(hTuner, FSTV6111_ODIV, 3);
		break;
	}

	ChipSetField(hTuner, FSTV6111_ODIV, 0);

	/*I2C registers initialization*/
	error = STV6111_TunerWrite(hTuner);

	/*************R/W test code**************************
	STV6111_TunerRead(hTuner);
	{	int i = 0;
		printk("\n OPEN");
		for (i = 0 ; i < 8 ; i++) {
			printk("\tRegister CTRL[%d] = 0x%x",
			hTuner->pRegMapImage[i].Addr+1,
				hTuner->pRegMapImage[i].Value);
		}
	}
	***************************************************/

return error;
}

TUNER_Error_t STV6111_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20138 */
	if (hTuner)
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
	else
		error = TUNER_INVALID_HANDLE;

	if (!hTunerParams || hTuner->Error)
		return error;
	if (StandbyOn) /*Power down >>>>ON*/ {
		ChipSetFieldImage(hTuner, FSTV6111_RX_ON, 0);
	} else /*Power down >>>OFF*/ {
		ChipSetFieldImage(hTuner, FSTV6111_RX_ON, 1);
	}

	error = STV6111_TunerWrite(hTuner);
	/*************R/W test code**************************
	STV6111_TunerRead(hTuner);
	{	int i = 0;
		printk("\n Standby.reg is .CTRL2(bit0)");
		for (i = 0 ; i < 4 ; i++) {
			printk("\tRegister CTRL[%d] = 0x%x",
			hTuner->pRegMapImage[i].Addr+1,
					hTuner->pRegMapImage[i].Value);
		}
	}
	***************************************************/
return error;
}


TUNER_Error_t STV6111_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	U32 divider;
	U32 p,
		rDiv,
		swallow;

	S32 i, fequencyVco;
	U8 Icp;
	/*fix Coverity CID: 20135 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t)hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;
	/*if FXTAL >= 27MHz set R to 2 else if FXTAL < 27MHz set R to 1 */
	if (hTunerParams->Reference >= 27000000)
		rDiv = 1;
	else
		rDiv = 0;

	if (Frequency <= 325000) /* in kHz */ {
		p = 3;
	} else if (Frequency <= 650000) {
		p = 2;
	} else if (Frequency <= 1300000) {
		p = 1;
	} else {
		p = 0;
	}

	fequencyVco = Frequency * PowOf2(p + 1);

	swallow = (Frequency * PowOf2(p + rDiv + 1));
	/*N = (Frequency *P*R)/Fxtal */
	divider = swallow / (hTunerParams->Reference / 1000);
	/*F = (2^18 *(Frequency * P * R) % Fxtal) / Fxtal */
	swallow = swallow % (hTunerParams->Reference / 1000);
	if (swallow < 0x3FFF) /*to avoid U32 bits saturation*/
		swallow = (0x40000 * swallow)/(hTunerParams->Reference / 1000);
	else if (swallow < 0x3FFF0) /*to avoid U32 bits saturation*/
		swallow = (0x8000 * swallow)/(hTunerParams->Reference / 8000);
	else				/*to avoid U32 bits saturation*/
		swallow = (0x2000 * swallow)/(hTunerParams->Reference / 32000);

	if (ChipGetFieldImage(hTuner, FSTV6111_RDIV) != rDiv) {
		ChipSetFieldImage(hTuner, FSTV6111_RDIV, rDiv);
		ChipSetRegisters(hTuner, RSTV6111_CTRL1, 1);
	}

	ChipSetField(hTuner, FSTV6111_LO_DIV, p);
	ChipSetField(hTuner, FSTV6111_N_LSB, (divider & 0xFF));
	ChipSetField(hTuner, FSTV6111_N_MSB, ((divider >> 8) & 0x01));
	ChipSetField(hTuner, FSTV6111_F_H, ((swallow >> 15) & 0x07));
	ChipSetField(hTuner, FSTV6111_F_M, ((swallow >> 7) & 0xFF));
	ChipSetField(hTuner, FSTV6111_F_L, (swallow & 0x7F));

	/*Set K value, FXtal = 15+K*/
	ChipSetField(hTuner, FSTV6111_K,
				hTunerParams->Reference/1000000 - 15);

	/*Set charge pump current according to the VCO freq*/
	if (fequencyVco < 2700)
		Icp = 0;
	else if (fequencyVco < 2950)
		Icp = 1;
	else if (fequencyVco < 3300)
		Icp = 2;
	else if (fequencyVco < 3700)
		Icp = 3;
	else if (fequencyVco < 4200)
		Icp = 5;
	else if (fequencyVco < 4800)
		Icp = 6;
	else
		Icp = 7;
	ChipSetField(hTuner, FSTV6111_PLL_ICP, Icp);

	ChipSetField(hTuner, FSTV6111_LNA_SEL_GAIN, 1);
	/*VCO auto calibration to verify*/
	ChipSetField(hTuner, FSTV6111_VCO_CAL_START, 1);
	i = 0;
	while (i < 10 && ChipGetField(hTuner, FSTV6111_VCO_CAL_START)) {
		WAIT_N_MS(1);
		i++;
	}

	/*LNA calibration IIP3 vs NF mode*/
	WAIT_N_MS(10);
	if (ChipGetField(hTuner, FSTV6111_LNA_AGC_LOW) == 1)
		ChipSetField(hTuner, FSTV6111_LNA_SEL_GAIN, 0);


	/*************R/W test code**************************
	STV6111_TunerRead(hTuner);
	{
		int i = 0;
		for (i = 0 ; i < 8 ; i++) {
			printk("\tRegister CTRL[%d] = 0x%x",
			hTuner->pRegMapImage[i].Addr+1,
					hTuner->pRegMapImage[i].Value);
		}
	}
	**************************************************/

return error;
}

U32 STV6111_TunerGetFrequency(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 frequency = 0;
	U32 divider = 0;
	U32 swallow;

	/*fix Coverity CID: 20128 */
	if (!hTuner)
		return frequency;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return frequency;

	divider = MAKEWORD(ChipGetField(hTuner, FSTV6111_N_MSB),
			ChipGetField(hTuner, FSTV6111_N_LSB)); /*N*/
	swallow = ((ChipGetField(hTuner, FSTV6111_F_H) & 0x07) << 15) +
			((ChipGetField(hTuner, FSTV6111_F_M) & 0xFF) << 7) +
			(ChipGetField(hTuner, FSTV6111_F_L) & 0x7F);

	frequency = (hTunerParams->Reference/1000) /
				PowOf2(ChipGetField(hTuner, FSTV6111_RDIV));
	frequency = (frequency * divider) + ((frequency * swallow) / 0x40000);
	frequency /= PowOf2(1 + ChipGetField(hTuner, FSTV6111_LO_DIV));

	/*Frequency = (1 / (2^PDIV + 1)) * (Fxtal / (2^RDIV)) * (NDIV +
								(F / 2^18))*/

return frequency;
}


TUNER_Error_t STV6111_TunerSetBandwidth(STCHIP_Handle_t hTuner, U32 Bandwidth)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 u8;
	S32 i = 0;

	/*fix Coverity CID: 20134 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t)hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;

	if ((Bandwidth / 2) > 50000000) /*if BW/2 > 50 MHz clamp to 50 MHz*/ {
		u8 = 44;
	} else if ((Bandwidth / 2) < 6000000) {
		u8 = 0;
	} else {
		u8 = (Bandwidth / 2)/1000000 - 6;
	}
	ChipSetField(hTuner, FSTV6111_BB_FILT_SEL, u8);
	ChipSetField(hTuner, FSTV6111_BB_CAL_START, 1);/*Start the calibration*/
	i = 0;
	/* Wait for FSTV6111_BB_CAL_START reset at the end of the calibration */
	while  (i < 10 && ChipGetField(hTuner, FSTV6111_BB_CAL_START)) {
		WAIT_N_MS(1);
		i++;
	}

return error;
}

U32 STV6111_TunerGetBandwidth(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 bandwidth = 0;
	U8 u8 = 0;

	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
		if (hTunerParams && !hTuner->Error) {
			u8 = ChipGetField(hTuner, FSTV6111_BB_FILT_SEL);
			/* x2 for ZIF tuner BW/2 = F+6 Mhz*/
			bandwidth = (u8 + 6) * 2000000;
		}
	}

	return bandwidth;
}

TUNER_Error_t STV6111_TunerSetGain(STCHIP_Handle_t hTuner, S32 Gain)
/*	BBMODE = 00 => VGA2 controled by VGACI
	BBMODE = 01 => VGA2 = GainMax - 3dB
	BBMODE = 10 => VGA2 = GainMax - 6dB
	BBMODE = 11 => VGA2 = GainMax - 0dB
	if Gain is not [-3, -6, 0] then do nothing
*/
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner)
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
		/*to do*/
return error;
}

S32 STV6111_TunerGetGain(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	S32 gain = 0;

	if (hTuner)
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
return gain;
}

S32 STV6111_GetLUT(FE_LLA_LOOKUP_t *GainLUT, S32 RegValue)
{
	S32 realValue = 0, Imin, Imax, i;

	Imin = 0;
	Imax = GainLUT->size - 1;

	if (RegValue <= GainLUT->table[0].regval)
		realValue = GainLUT->table[0].realval;
	else if (RegValue >= GainLUT->table[Imax].regval)
		realValue = GainLUT->table[Imax].realval;

	else {
		while  ((Imax - Imin) > 1) {
			i = (Imax + Imin) >> 1;
			/*equivalent to i = (Imax+Imin)/2; */
			if (INRANGE(GainLUT->table[Imin].regval, RegValue,
						GainLUT->table[i].regval))
				Imax = i;
			else
				Imin = i;
		}

		realValue = (((S32)RegValue - GainLUT->table[Imin].regval)
				* (GainLUT->table[Imax].realval -
						GainLUT->table[Imin].realval)
				/ (GainLUT->table[Imax].regval -
						GainLUT->table[Imin].regval))
				+ GainLUT->table[Imin].realval;
	}
return realValue;
}


S32 STV6111_TunerGetRFGain(STCHIP_Handle_t hTuner, U32 AGCIntegrator,
								S32 BBGain)
{
	S32 Gain100dB = 1,/*RefBBgain = 12,*/
		lna_AGC_mode, val, LnaGain = 0;

	static FE_LLA_LOOKUP_t GainLUT;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20129 */
	if (!hTuner)
		return Gain100dB+LnaGain;
	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return Gain100dB + LnaGain;

	BBGain = 0;

	lna_AGC_mode = ChipGetField(hTuner, FSTV6111_LNA_AGC_MODE);
	if (lna_AGC_mode == 0) {
		/*RF AGC Mode */
		GainLUT = FE_STV6111_Gain_RFAGC_LookUp;

		val = ChipGetField(hTuner, FSTV6111_LNA_SEL_GAIN);
		ChipSetField(hTuner, FSTV6111_LNA_ADC_GET, 1);

		LnaGain = ChipGetField(hTuner, FSTV6111_LNA_AGC);
		if (val == 0) {
			/* NF mode*/
			LnaGain = STV6111_GetLUT(&FE_STV6111_LNAGain_NF_LookUp,
								LnaGain);
		} else {
			/*IIP3 mode*/
			LnaGain = STV6111_GetLUT(
						&FE_STV6111_LNAGain_IIP3_LookUp,
								LnaGain);
		}
		LnaGain -= 2400;

	} else {
		/*Channel AGC Mode*/
		LnaGain = 0;
		val = ChipGetField(hTuner, FSTV6111_LNA_SEL_GAIN);

		if (val == 0) {
			/* NF mode*/
			GainLUT = FE_STV6111_Gain_Channel_AGC_NF_LookUp;

		} else {
			/*IIP3 mode*/
			GainLUT = FE_STV6111_Gain_Channel_AGC_IIP3_LookUp;
		}

	}

	Gain100dB = STV6111_GetLUT(&GainLUT, AGCIntegrator);

return Gain100dB;
}

TUNER_Error_t STV6111_TunerSetOutputClock(STCHIP_Handle_t hTuner, S32 Divider)
{
	/*sets the crystal oscillator divisor value, for the output clock
	Divider =:
	0 ==> Tuner output clock not used
	1 ==> divide by 1
	2 ==> divide by 2
	4 ==> divide by 4
	8 ==> divide by 8
	*/
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20137 */
	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams && !hTuner->Error) {
		/*Allowed values 1, 2, 4 and 8*/
		switch (Divider) {
		case 1:
		default:
			ChipSetField(hTuner, FSTV6111_ODIV, 0);
			break;
		case 2:
			ChipSetField(hTuner, FSTV6111_ODIV, 1);
			break;
		case 4:
			ChipSetField(hTuner, FSTV6111_ODIV, 2);
			break;
		case 8:
		case 0:
			/*Tuner output clock not used then divide by 8*/
			ChipSetField(hTuner, FSTV6111_ODIV, 3);
			break;
		}
	}

return error;
}

TUNER_Error_t STV6111_TunerSetAttenuator(STCHIP_Handle_t hTuner,
							BOOL AttenuatorOn)
{
	return TUNER_NO_ERR;
}
BOOL STV6111_TunerGetAttenuator(STCHIP_Handle_t hTuner)
{
	return FALSE;
}


BOOL STV6111_TunerGetStatus(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	BOOL locked = FALSE;

	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
		if (hTunerParams && !hTuner->Error) {
			if (!hTuner->Error) {
				locked = ChipGetField(hTuner,
							FSTV6111_PLL_LOCK);
			}
		}
	}
return locked;
}

TUNER_Error_t STV6111_TunerWrite(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	/*fix Coverity CID: 20131 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams && !hTuner->Error) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20))
			/*Write one register at a time for 7111 cut1.0 */
			for (i = hTuner->WrStart; i < hTuner->WrSize; i++)
				chipError |= ChipSetRegisters(hTuner, i, 1);
		else
			chipError = ChipSetRegisters(hTuner, hTuner->WrStart,
						     hTuner->WrSize);

		switch (chipError) {
		case CHIPERR_NO_ERROR:
			error = TUNER_NO_ERR;
			break;

		case CHIPERR_INVALID_HANDLE:
			error = TUNER_INVALID_HANDLE;
			break;

		case CHIPERR_INVALID_REG_ID:
			error = TUNER_INVALID_REG_ID;
			break;

		case CHIPERR_INVALID_FIELD_ID:
			error = TUNER_INVALID_FIELD_ID;
			break;

		case CHIPERR_INVALID_FIELD_SIZE:
			error = TUNER_INVALID_FIELD_SIZE;
			break;

		case CHIPERR_I2C_NO_ACK:
		default:
			error = TUNER_I2C_NO_ACK;
			break;

		case CHIPERR_I2C_BURST:
			error = TUNER_I2C_BURST;
			break;
		}
	}

	return error;
}

TUNER_Error_t STV6111_TunerRead(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	/*fix Coverity CID: 20133 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams && !hTuner->Error) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20))
			/*Read one register at a time for cut1.0 */
			for (i = hTuner->RdStart; i < hTuner->RdSize; i++)
				chipError |= ChipGetRegisters(hTuner, i, 1);
		else
			chipError = ChipGetRegisters(hTuner,
						     hTuner->RdStart,
						     hTuner->RdSize);

		switch (chipError) {
		case CHIPERR_NO_ERROR:
			error = TUNER_NO_ERR;
			break;

		case CHIPERR_INVALID_HANDLE:
			error = TUNER_INVALID_HANDLE;
			break;

		case CHIPERR_INVALID_REG_ID:
			error = TUNER_INVALID_REG_ID;
			break;

		case CHIPERR_INVALID_FIELD_ID:
			error = TUNER_INVALID_FIELD_ID;
			break;

		case CHIPERR_INVALID_FIELD_SIZE:
			error = TUNER_INVALID_FIELD_SIZE;
			break;

		case CHIPERR_I2C_NO_ACK:
		default:
			error = TUNER_I2C_NO_ACK;
			break;

		case CHIPERR_I2C_BURST:
			error = TUNER_I2C_BURST;
			break;
		}

	}

	return error;
}

TUNER_Model_t STV6111_TunerGetModel(STCHIP_Handle_t hChip)
{
	TUNER_Model_t model = TUNER_NULL;

	if (hChip && hChip->pData)
		model = ((SAT_TUNER_Params_t) (hChip->pData))->Model;

	return model;
}

void STV6111_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, S8 IQ_Wiring)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring =
							(TUNER_IQ_t)IQ_Wiring;

}

S8 STV6111_TunerGetIQ_Wiring(STCHIP_Handle_t hChip)
{
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;

	if (hChip && hChip->pData)
		wiring = ((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring;

return (S8)wiring;
}

void STV6111_TunerSetReferenceFreq(STCHIP_Handle_t hChip, U32 Reference)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->Reference = Reference;

}

U32 STV6111_TunerGetReferenceFreq(STCHIP_Handle_t hChip)
{
	U32 reference = 0;

	if (hChip && hChip->pData)
		reference = ((SAT_TUNER_Params_t) (hChip->pData))->Reference;

return reference;
}

void STV6111_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->IF = IF;

}

U32 STV6111_TunerGetIF_Freq(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->IF;

return ifreq;
}

void STV6111_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->BandSelect =
					(TUNER_WIDEBandS_t)BandSelect;

}

U8 STV6111_TunerGetBandSelect(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->BandSelect;

return (U8)ifreq;
}
/*The default mode = IIP3
If the LNA_AGC_LOW = 1 and tuner mode = IIP3 then set the tuner to LowNF
*/
TUNER_Error_t STV6111_TunerSetMode(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;
	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams == NULL)
		return TUNER_INVALID_HANDLE;

	if (hTunerParams->Model == TUNER_STV6111) {
		ChipGetField(hTuner, FSTV6111_LNA_AGC_LOW);
		ChipGetField(hTuner, FSTV6111_LNA_SEL_GAIN);

		if (ChipGetField(hTuner, FSTV6111_LNA_AGC_LOW)) {
			if (ChipGetField(hTuner, FSTV6111_LNA_SEL_GAIN))
				/*Low NF*/
				ChipSetField(hTuner, FSTV6111_LNA_SEL_GAIN, 0);
		} else {
			if (!ChipGetField(hTuner, FSTV6111_LNA_SEL_GAIN))
				/*Low NF*/
				ChipSetField(hTuner, FSTV6111_LNA_SEL_GAIN, 1);
		}
	}
return error;
}

TUNER_Error_t STV6111_TunerTerm(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner) {
		#ifndef CHIP_STAPI
			if (hTuner->pData)
				free(hTuner->pData);

			ChipClose(hTuner);
		#endif
	}

	return error;
}
