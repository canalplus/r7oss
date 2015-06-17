/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.



This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.



This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.



You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
#ifndef TMSYSSCANXPRESS_H
#define TMSYSSCANXPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

/************************************************************************/
/*                                                                      */
/* Description:                                                         */
/*  Describes the ScanXpress structure.                                 */
/*                                                                      */
/* Settings:                                                            */
/*  bScanXpressMode       - ScanXpress Mode (True/False).               */
/*  pFw_code              - Table containing the firmware.              */
/*  Fw_code_size          - Size of the firmware.                       */
/*                                                                      */
/************************************************************************/
typedef struct _tmsysScanXpressConfig_t {
	Bool bScanXpressMode;
	UInt8 *puFw_code;
	UInt32 uFw_code_size;
} tmsysScanXpressConfig_t, *ptmsysScanXpressConfig_t;

/************************************************************************/
/*                                                                      */
/* Description:                                                         */
/*  Describes the ScanXpress structure.                                 */
/*                                                                      */
/* Settings:                                                            */
/*  uFrequency            - Frequency of the ScanXpress Request.        */
/*  uCS                   - Channel bandwidth.                          */
/*  uSpectralInversion    - Spectral inversion.                         */
/*  Confidence            - Confidence threshold.                       */
/*  ChannelType           - Channel type.                               */
/*                                                                      */
/************************************************************************/
typedef struct _tmsysScanXpressRequest_t {
	UInt32 uFrequency;
	UInt32 uCS;
	UInt32 uSpectralInversion;
	tmFrontEndConfidence_t eConfidence;/* TODO: remove, not used anymore */
	/* output value */
	Int32 TunerStep;
	tmFrontEndChannelType_t eChannelType;
	UInt32 uAnlgFrequency;
	tmbslFrontEndTVStandard_t eTVStandard;
	UInt32 uDgtlFrequency;
	UInt32 uDgtlBandwidth;
} tmsysScanXpressRequest_t, *ptmsysScanXpressRequest_t;

/************************************************************************/
/*                                                                      */
/* Description:                                                         */
/*  Describes the ScanXpressChannelFound structure.                     */
/*                                                                      */
/* Settings:                                                            */
/*  eChannelType         - AChannel type.                               */
/*  uAnlgFrequency       - Frequency in Hz.                             */
/*  uDgtlFrequency       - Frequency in Hz.                             */
/*                                                                      */
/************************************************************************/
typedef struct _tmsysScanXpressFoundChannel_t {
	tmFrontEndChannelType_t eChannelType;
	UInt32 uAnlgFrequency;
	tmbslFrontEndTVStandard_t eTVStandard;
	UInt32 uDgtlFrequency;
	UInt32 uDgtlBandwidth;
} tmsysScanXpressFoundChannel_t, *ptmsysScanXpressFoundChannel_t;

#ifdef __cplusplus
}
#endif

#endif /* TMDLSCANXPRESS_H */
/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
