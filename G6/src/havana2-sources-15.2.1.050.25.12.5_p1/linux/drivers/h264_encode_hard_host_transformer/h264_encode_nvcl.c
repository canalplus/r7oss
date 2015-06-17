/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/platform_device.h>

#include "h264_encode_nvcl.h"

#define PIC_PARAMETER_SET_ID 0

/* Interpretation of pic struct Table D-1 in ITU-T H.264 */
const uint32_t kPicStructNumTs[9] = {
	1, 1, 1, 2, 2, 3, 3, 2, 3
};
/********************************************************************
   Function Prototype Declarations
********************************************************************/
/* UVLC basic functions */
static int32_t symboltoUVLC(symbol_t *sym_p);
static void writeUVLCtobuffer(symbol_t *sym_p, bitstream_t *stream_p);
static int32_t writeSymbol_UVLC(symbol_t *sym_p, bitstream_t *stream_p, int32_t sign);
static void ue_linfo(int32_t ue, int16_t *len_p, int16_t *info_p);
static void se_linfo(int32_t se, int16_t *len_p, int16_t *info_p);

/* Bitstream coding functions */
static int ue_v(int32_t value, bitstream_t *stream_p);
static int se_v(int32_t value, bitstream_t *stream_p);
static int u_1(int32_t value, bitstream_t *stream_p);
static int u_v(int32_t n, int value, bitstream_t *stream_p);
static uint32_t byteAlign(bitstream_t *stream_p);
/* NALU basic functions */
static int32_t RBSPtoEBSP(uint8_t *streamBuf_p, int32_t beginBytePos, int32_t endBytePos, int32_t minNumBytes);
static void SODBtoRBSP(bitstream_t *stream_p);
static int32_t RBSPtoNALU(uint8_t *rbsp_p, NALU_t *nalu_p, int32_t rbspLen, int32_t nal_unit_type,
                          int32_t nal_reference_idc, int32_t minNumBytes, int32_t UseAnnexbLongStartcode);
/* NALU coding functions */
static uint32_t InitSeqParamSet(seq_parameter_set_rbsp_t *sps_p);
static uint32_t InitPicParamSet(pic_parameter_set_rbsp_t *pps_p);
static uint32_t InitVUIParam(vui_parameter_t *vui_p);
static uint32_t InitHRDParam(hrd_parameter_t *hrd_p);
static uint32_t InitSEIParam(sei_message_t *sei_p);
static uint32_t UpdateSeqParamSet(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                                  seq_parameter_set_rbsp_t *sps_p);
static uint32_t UpdatePicParamSet(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                  pic_parameter_set_rbsp_t *pps_p);
static uint32_t UpdateVUIParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                               vui_parameter_t *vui_p);
static value_scale_deconstruction_t H264HwEncodeDeconstructParameter(uint32_t targeted_value, uint32_t scale_offset);
static uint32_t H264HwEncodeReconstructParameter(uint32_t value, int32_t scale, uint32_t scale_offset);
static uint32_t UpdateHRDParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                               hrd_parameter_t *hrd_p, uint8_t type);
static uint32_t UpdateSEIParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                               H264EncodeHardParamOut_t *outParams_p,
                               sei_message_t *sei_p);
static uint32_t UpdateSEIParam_post(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                    H264EncodeHardParamOut_t *outParams_p, sei_message_t *sei_p);
static NALU_t *GenerateAccessUnitDelimiter_NALU(H264EncodeHardFrameParams_t *frameParams_p, NALU_t *nalu_p);
static uint32_t GenerateAccessUnitDelimiter_rbsp(H264EncodeHardFrameParams_t *frameParams_p, uint8_t *rbsp_p);
static NALU_t *GenerateSeqParamSet_NALU(seq_parameter_set_rbsp_t *sps_p, NALU_t *nalu_p);
static uint32_t GenerateSeqParamSet_rbsp(seq_parameter_set_rbsp_t *sps_p, uint8_t *rbsp_p);
static uint32_t GenerateVUIParam_rbsp(vui_parameter_t *vui_p, bitstream_t *stream_p);
static NALU_t *GeneratePicParamSet_NALU(seq_parameter_set_rbsp_t *sps_p, pic_parameter_set_rbsp_t *pps_p,
                                        NALU_t *nalu_p);
static uint32_t GeneratePicParamSet_rbsp(seq_parameter_set_rbsp_t *sps_p, pic_parameter_set_rbsp_t *pps_p,
                                         uint8_t *rbsp_p);
static NALU_t *GenerateSEI_NALU(sei_message_t *sei_p, NALU_t *nalu_p);
static uint32_t GenerateSEI_rbsp(sei_message_t *sei_p, uint8_t *rbsp_p);
static uint32_t GenerateSEI_message(sei_message_t *sei_p, uint8_t *rbsp_p, bitstream_t *stream_p, sei_type type);
static uint32_t GenerateSEI_payload(sei_message_t *sei_p, bitstream_t *stream_p, sei_type type);

extern struct HvaDriverData *pDriverData;

/*!
 ************************************************************************
 * \brief
 *    Makes code word and passes it back
 *    A code word has the following format: 0 0 0 ... 1 Xn ...X2 X1 X0.
 *
 * \par Input:
 *    Info   : Xn..X2 X1 X0                                             \n
 *    Length : Total number of bits in the codeword
 ************************************************************************
 */
/* NOTE this function is called with sym->inf > (1<<(sym->len/2)).  The upper bits of inf are junk */
static int32_t symboltoUVLC(symbol_t *sym_p)
{
	int32_t info_len = sym_p->len / 2;

	sym_p->codeword = (1 << info_len) | (sym_p->info & ((1 << info_len) - 1));

	return 0;
}

/*!
 ************************************************************************
 * \brief
 *    writes UVLC code to the appropriate buffer
 ************************************************************************
 */
static void writeUVLCtobuffer(symbol_t *sym_p, bitstream_t *stream_p)
{
	int32_t i;
	uint32_t mask;

	mask = (1 << (sym_p->len - 1));

	/* Add the new bits to the bitstream. */
	/* Write out a byte if it is full */
	for (i = 0; i < sym_p->len; i++) {
		stream_p->byte_buf <<= 1;
		if (sym_p->codeword & mask) {
			stream_p->byte_buf |= 1;
		}
		stream_p->bits_to_go--;
		mask >>= 1;
		if (stream_p->bits_to_go == 0) {
			stream_p->bits_to_go = 8;
			stream_p->streamBuf_p[stream_p->byte_pos++] = (uint8_t)stream_p->byte_buf;
			stream_p->byte_buf = 0;
		}
	}
}

/*!
 ************************************************************************
 * \brief
 *    generates UVLC code and passes the codeword to the buffer
 ************************************************************************
 */
static int32_t writeSymbol_UVLC(symbol_t *sym_p, bitstream_t *stream_p, int32_t sign)
{
	if (sign == 0) {
		ue_linfo(sym_p->value, &(sym_p->len), &(sym_p->info));
	} else {
		se_linfo(sym_p->value, &(sym_p->len), &(sym_p->info));
	}
	symboltoUVLC(sym_p);
	writeUVLCtobuffer(sym_p, stream_p);

	return (sym_p->len);
}

/*!
 ************************************************************************
 * \brief
 *    passes the fixed codeword to the buffer
 ************************************************************************
 */
static int32_t writeSymbol_fixed(symbol_t *sym_p, bitstream_t *stream_p)
{
	writeUVLCtobuffer(sym_p, stream_p);
	return (sym_p->len);
}

/*!
 ************************************************************************
 * \brief
 *    mapping for ue(v) syntax elements
 * \param ue
 *    value to be mapped
 * \param dummy
 *    dummy parameter
 * \param info
 *    returns mapped value
 * \param len
 *    returns mapped value length
 ************************************************************************
 */
static void ue_linfo(int32_t ue, int16_t *len_p, int16_t *info_p)
{
	int32_t i, nn;

	nn = (ue + 1) / 2;

	for (i = 0; i < 16 && nn != 0; i++) {
		nn /= 2;
	}
	*len_p = 2 * i + 1;
	*info_p = ue + 1 - (1 << i);
}

/*!
 ************************************************************************
 * \brief
 *    mapping for se(v) syntax elements
 * \param se
 *    value to be mapped
 * \param dummy
 *    dummy parameter
 * \param len
 *    returns mapped value length
 * \param info
 *    returns mapped value
 ************************************************************************
 */
static void se_linfo(int32_t se, int16_t *len_p, int16_t *info_p)
{

	int32_t i, n, sign, nn;

	sign = 0;
	if (se <= 0) {
		sign = 1;
	}

	n = (absm(se) << 1);

	/*  n+1 is the number in the code table.  Based on this we find length and info */

	nn = n / 2;
	for (i = 0; i < 16 && nn != 0; i++) {
		nn /= 2;
	}
	*len_p = i * 2 + 1;
	*info_p = n - (1 << i) + sign;
}

/*!
 *************************************************************************************
 * \brief
 *    ue_v, writes an ue(v) syntax element, returns the length in bits
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param part
 *    the Data Partition the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
static int32_t ue_v(int32_t value, bitstream_t *stream_p)
{
	symbol_t sym;
	sym.value = value;

	return writeSymbol_UVLC(&sym, stream_p, 0);
}

/*!
 *************************************************************************************
 * \brief
 *    se_v, writes an se(v) syntax element, returns the length in bits
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param part
 *    the Data Partition the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
static int32_t se_v(int32_t value, bitstream_t *stream_p)
{
	symbol_t sym;

	sym.value = value;

	return writeSymbol_UVLC(&sym, stream_p, 1);
}

/*!
 *************************************************************************************
 * \brief
 *    u_1, writes a flag (u(1) syntax element, returns the length in bits,
 *    always 1
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param part
 *    the Data Partition the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element (always 1)
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
static int32_t u_1(int32_t value, bitstream_t *stream_p)
{
	symbol_t sym;

	sym.value    = value;
	sym.len      = 1;
	sym.codeword = value; /* assuming value is unsigned */

	return writeSymbol_fixed(&sym, stream_p);
}

/*!
 *************************************************************************************
 * \brief
 *    u_v, writes a n bit fixed length syntax element, returns the length in bits,
 *
 * \param n
 *    length in bits
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param part
 *    the Data Partition the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
static int32_t u_v(int32_t n, int32_t value, bitstream_t *stream_p)
{
	symbol_t sym;

	sym.value    = value;
	sym.len      = n;
	sym.codeword = (uint32_t)value; /* assuming value is unsigned? */

	return writeSymbol_fixed(&sym, stream_p);
}

static uint32_t byteAlign(bitstream_t *stream_p)
{
	uint32_t len;
	len = 0;
	/* make sure the payload is byte aligned, stuff bits are 10..0 */
	if (stream_p->bits_to_go != 8) {
		stream_p->byte_buf <<= 1;
		stream_p->byte_buf |= 1;
		stream_p->bits_to_go--;
		len += 1;
		if (stream_p->bits_to_go != 0) {
			stream_p->byte_buf <<= (stream_p->bits_to_go);
			len += stream_p->bits_to_go;
		}
		stream_p->bits_to_go = 8;
		stream_p->streamBuf_p[stream_p->byte_pos++] = (uint8_t) stream_p->byte_buf;
		stream_p->byte_buf = 0;
	}
	return len;
}

/*!
************************************************************************
*  \brief
*     This function converts a RBSP payload to an EBSP payload
*
*  \param streamBuffer
*       pointer to data bits
*  \param begin_bytepos
*            The byte position after start-code, after which stuffing to
*            prevent start-code emulation begins.
*  \param end_bytepos
*           Size of streamBuffer in bytes.
*  \param min_num_bytes
*           Minimum number of bytes in payload. Should be 0 for VLC entropy
*           coding mode. Determines number of stuffed words for CABAC mode.
*  \return
*           Size of streamBuffer after stuffing.
*  \note
*      NAL_Payload_buffer is used as temporary buffer to store data.
*
*
************************************************************************
*/
static int32_t RBSPtoEBSP(uint8_t *streamBuf_p, int32_t beginBytePos, int32_t endBytePos, int32_t minNumBytes)
{

	int32_t i, j, count;
	uint8_t streamBuf_ref_p[NVCL_SCRATCH_BUFFER_SIZE];
	int32_t size;

	size = endBytePos - beginBytePos;
	if (size > NVCL_SCRATCH_BUFFER_SIZE) {
		dev_err(pDriverData->dev, "Error: nvcl scratch buffer is too small\n");
		return -1;
	}
	memcpy(streamBuf_ref_p, streamBuf_p, size);
	count = 0;
	j = beginBytePos;
	for (i = beginBytePos; i < endBytePos; i++) {
		if (count == ZEROBYTES_SHORTSTARTCODE && !(streamBuf_ref_p[i] & 0xFC)) {
			dev_dbg(pDriverData->dev, "ZEROBYTES_SHORTSTARTCODE detected, 0x%02x & 0xFC = 0x%02x\n", streamBuf_ref_p[i],
			        (streamBuf_ref_p[i] & 0xFC));
			streamBuf_p[j] = 0x03;
			j++;
			count = 0;
		}

		streamBuf_p[j] = streamBuf_ref_p[i];
		if (streamBuf_ref_p[i] == 0x00) {
			count++;
		} else {
			count = 0;
		}
		j++;
	}
	while (j < beginBytePos + minNumBytes) {
		streamBuf_p[j]   = 0x00;
		streamBuf_p[j + 1] = 0x00;
		streamBuf_p[j + 2] = 0x03;
		j += 3;
	}

	return j;
}

/*!
************************************************************************
* \brief
*    Converts String Of Data Bits (SODB) to Raw Byte Sequence
*    Packet (RBSP)
* \param currStream
*        bitstream_t which contains data bits.
* \return None
* \note currStream is byte-aligned at the end of this function
*
************************************************************************
*/
static void SODBtoRBSP(bitstream_t *stream_p)
{
	stream_p->byte_buf <<= 1;
	stream_p->byte_buf |= 1;
	stream_p->bits_to_go--;
	stream_p->byte_buf <<= stream_p->bits_to_go;
	stream_p->streamBuf_p[stream_p->byte_pos++] = stream_p->byte_buf;
	stream_p->bits_to_go = 8;
	stream_p->byte_buf = 0;
}

/*!
*************************************************************************************
* \brief
*    Converts an RBSP to a NALU
*
* \param rbsp
*    byte buffer with the rbsp
* \param nalu
*    nalu structure to be filled
* \param rbsp_size
*    size of the rbsp in bytes
* \param nal_unit_type
*    as in JVT doc
* \param nal_reference_idc
*    as in JVT doc
* \param min_num_bytes
*    some incomprehensible CABAC stuff
* \param UseAnnexbLongStartcode
*    when 1 and when using AnnexB bytestreams, then use a long startcode prefix
*
* \return
*    length of the NALU in bytes
*************************************************************************************
*/
static int32_t RBSPtoNALU(uint8_t *rbsp_p, NALU_t *nalu_p, int32_t rbspLen, int32_t nal_unit_type,
                          int32_t nal_reference_idc, int32_t minNumBytes, int32_t UseAnnexbLongStartcode)
{
	int32_t len;

	uint8_t *dest_p = (uint8_t *)(&(nalu_p->buf_p[1]));
	int32_t i;

	nalu_p->forbidden_bit = 0;
	nalu_p->nal_reference_idc = nal_reference_idc;
	nalu_p->nal_unit_type = nal_unit_type;
	nalu_p->startCodePrefixLen = UseAnnexbLongStartcode ? 4 : 3;
	nalu_p->buf_p[0] = nalu_p->forbidden_bit << 7      |
	                   nalu_p->nal_reference_idc << 5  |
	                   nalu_p->nal_unit_type;
	for (i = 0; i < rbspLen; i++, dest_p++, rbsp_p++) {
		*dest_p = *rbsp_p;
	}
	len = RBSPtoEBSP(&(nalu_p->buf_p[1]), 0, rbspLen, minNumBytes);
	if (len == -1) {
		dev_err(pDriverData->dev, "Error: RBSPtoEBSP failed\n");
		return len;
	}
	if (len != rbspLen) {
		dev_dbg(pDriverData->dev, "RBSP len %d != EBSP len %d\n", rbspLen, len);
	}
	len += 1;
	nalu_p->len = len;

	return len;
}

static uint32_t InitSeqParamSet(seq_parameter_set_rbsp_t *sps_p)
{
	int32_t i;
	sps_p->profile_idc                           = SPS_PROFILE_IDC_MAIN;
	sps_p->constraint_set0_flag                  = 0;                     /* Associated with SPS_PROFILE_IDC_BASELINE */
	sps_p->constraint_set1_flag                  = 1;                     /* Associated with SPS_PROFILE_IDC_MAIN */
	sps_p->constraint_set2_flag                  = 0;                     /* Associated with SPS_PROFILE_IDC_EXTENDED */
	sps_p->constraint_set3_flag                  = 0;                     /* Associated with level 1b */
	sps_p->level_idc                             = 32;
	sps_p->seq_parameter_set_id                  = 0;
	sps_p->chroma_format_idc                     = SPS_CHROMA_FORMAT_420; /* FIXED */
	sps_p->residual_colour_transform_flag        = 0;                     /* Associated with SPS_CHROMA_FORMAT_444 */
	sps_p->bit_depth_luma_minus8                 = 0;                     /* FIXED */
	sps_p->bit_depth_chroma_minus8               = 0;                     /* FIXED */
	sps_p->qpprime_y_zero_transform_bypass_flag  = 0;                     /* FIXED */
	sps_p->seq_scaling_matrix_present_flag       = 0;
	for (i = 0; i < 8; i++) {
		sps_p->seq_scaling_list_present_flag[i]  = 0;
	}
	sps_p->log2_max_frame_num_minus4             = 15;
	sps_p->pic_order_cnt_type                    = 2;                      /* FIXED */
	sps_p->log2_max_pic_order_cnt_lsb_minus4     = 0;                      /* Associated with pic_order_cnt_type = 0 */
	sps_p->delta_pic_order_always_zero_flag      = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->offset_for_non_ref_pic                = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->offset_for_top_to_bottom_field        = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->num_ref_frames_in_pic_order_cnt_cycle = 0;                      /* Associated with pic_order_cnt_type = 1 */
	for (i = 0; i < MAX_NUM_REF_FRAME; i++) {
		sps_p->offset_for_ref_frame[i] = 0;
	}
	sps_p->num_ref_frames                        = 1;                      /* FIXED - only 1 reference frame */
	sps_p->gaps_in_frame_num_value_allowed_flag  = 0;
	sps_p->pic_width_in_mbs_minus1               = 69;                     /* Example resolution 720P */
	sps_p->pic_height_in_map_units_minus1        = 44;                     /* Example resolution 720P */
	sps_p->frame_mbs_only_flag                   = 1;                      /* FIXED - only Progressive */
	sps_p->mbs_adaptive_frame_field_flag         = 0;                      /* Associated with frame_mbs_only_flag = 0 */
	sps_p->direct_8x8_inference_flag             = 1;                      /* FIXED */
	sps_p->frame_cropping_flag                   = 0;
	sps_p->frame_crop_left_offset                = 0;
	sps_p->frame_crop_right_offset               = 0;
	sps_p->frame_crop_top_offset                 = 0;
	sps_p->frame_crop_bottom_offset              = 0;
	sps_p->vui_parameters_present_flag           = 0;
	InitVUIParam(&sps_p->vui_param);
	return 0;
}

static uint32_t InitPicParamSet(pic_parameter_set_rbsp_t *pps_p)
{
	int32_t i;
	pps_p->pic_parameter_set_id                   = PIC_PARAMETER_SET_ID;
	pps_p->seq_parameter_set_id                   = 0;
	pps_p->entropy_coding_mode_flag               = PPS_ENTROPY_CODING_CAVLC; /* FIXED */
	pps_p->pic_order_present_flag                 = 0;
	pps_p->num_slice_groups_minus1                = 0;
	pps_p->slice_group_map_type                   =
	        0;                        /* Associated with num_slice_groups_minus1 > 0 */
	for (i = 0; i < MAX_NUM_SLICE_GROUPS; i++) {
		pps_p->run_length_minus1[i]               = 0;                        /* Associated with slice_group_map = 0 */
		pps_p->top_left[i]                        = 0;                        /* Associated with slice_group_map = 2 */
		pps_p->bottom_right[i]                    = 0;                        /* Associated with slice_group_map = 2 */
	}
	pps_p->slice_group_change_direction_flag      = 0;                        /* Associated with slice_group_map = 3-5 */
	pps_p->slice_group_change_rate_minus1         = 0;                        /* Associated with slice_group_map = 3-5 */
	pps_p->pic_size_in_map_units_minus1           = 0;                        /* Associated with slice_group_map = 6 */
	for (i = 0; i < MAX_NUM_SLICE_GROUPS; i++) {
		pps_p->slice_group_id[i]                  = 0;                        /* Associated with slice_group_map = 2 */
	}
	pps_p->num_ref_idx_10_active_minus1           = 0;                        /* FIXED - only 1 reference frame */
	pps_p->num_ref_idx_11_active_minus1           = 0;                        /* FIXED - Not supported */
	pps_p->weighted_pred_flag                     = 0;                        /* FIXED - Not supported */
	pps_p->weighted_bipred_idc                    = 0;                        /* FIXED - Not supported */
	pps_p->pic_init_qp_minus26                    = 0;
	pps_p->pic_init_qs_minus26                    = 0;
	pps_p->chroma_qp_index_offset                 = 0;
	pps_p->deblocking_filter_control_present_flag = 1;
	pps_p->contrained_intra_pred_flag             = 1;
	pps_p->redundant_pic_cnt_present_flag         = 0;                        /* FIXED */
	pps_p->transform_8x8_mode_flag                = 0;
	pps_p->pic_scaling_matrix_present_flag        = 0;
	for (i = 0; i < 8; i++) {
		pps_p->pic_scaling_list_present_flag[i]   = 0;
	}
	pps_p->second_chroma_qp_index_offset          = 0;
	return 0;
}

static uint32_t InitVUIParam(vui_parameter_t *vui_p)
{
	vui_p->aspect_ratio_info_present_flag          = 1;
	vui_p->aspect_ratio_idc                        = VUI_ASPECT_RATIO_1_1;
	vui_p->sar_width                               = 1;
	vui_p->sar_height                              = 1;
	vui_p->overscan_info_present_flag              = 0;
	vui_p->overscan_appropriate_flag               = 0;
	vui_p->video_signal_type_present_flag          = 1;
	vui_p->video_format                            = VUI_VIDEO_FORMAT_UNSPECIFIED;
	vui_p->video_full_range_flag                   = 0;
	vui_p->colour_description_present_flag         = 1;
	vui_p->colour_primaries                        = VUI_COLOR_UNSPECIFIED;
	vui_p->transfer_characteristics                = VUI_TRANSFER_UNSPECIFIED;
	vui_p->matrix_coefficients                     = VUI_MATRIX_UNSPECIFIED;
	vui_p->chroma_loc_info_present_flag            = 0;                            /* FIXED */
	vui_p->chroma_sample_loc_type_top_field        = 0;
	vui_p->chroma_sample_loc_type_bottom_field     = 0;
	vui_p->timing_info_present_flag                = 1;
	vui_p->num_units_in_tick                       = 1000;                         /* Example level 60fps */
	vui_p->time_scale                              = 60000;                        /* Example level 60fps */
	vui_p->fixed_frame_rate_flag                   =
	        0;                            /* Not fixed but we prefer 0 for managing frame skip */
	vui_p->nal_hrd_parameters_present_flag         = 0;
	InitHRDParam(&vui_p->nal_hrd_param);
	vui_p->vcl_hrd_parameters_present_flag         = 0;
	InitHRDParam(&vui_p->vcl_hrd_param);
	vui_p->low_delay_hrd_flag                      = 1;
	vui_p->pic_struct_present_flag                 =
	        0;                            /* Dependent on whether picture timing SEI is present */
	vui_p->bitstream_restriction_flag              = 0;
	vui_p->motion_vectors_over_pic_boundaries_flag = 1;                            /* FIXED - HW capability */
	vui_p->max_bytes_per_pic_denom                 = 0;                            /* FIXED - unlimited */
	vui_p->max_bits_per_mb_denom                   = 0;                            /* FIXED - unlimited */
	vui_p->log2_max_mv_length_horizontal           =
	        16;                           /* TBC - Constrained by HW -/+30 pel motion search window =>8 */
	vui_p->log2_max_mv_length_vertical             =
	        16;                           /* TBC - Constrained by HW -/+30 pel motion search window =>8 */
	vui_p->num_reorder_frames                      = 0;                            /* FIXED */
	vui_p->max_dec_frame_buffering                 = 1;                            /* FIXED */

	return 0;
}

static uint32_t InitHRDParam(hrd_parameter_t *hrd_p)
{
	int32_t i;
	hrd_p->cpb_cnt_minus1 = 0;                       /* FIXED */
	hrd_p->bit_rate_scale = 15;                      /* Example level 3.2 */
	hrd_p->cpb_size_scale = 15;                      /* Example level 3.2 */
	for (i = 0; i < MAX_CPB_SIZE; i++) {
		hrd_p->bit_rate_value_minus1[i]      = 305;  /* Example level 3.2 */
		hrd_p->cpb_size_value_minus1[i]      = 305;  /* Example level 3.2 */
		hrd_p->cbr_flag[i]                   = 1;    /* Example level 3.2 */
	}
	hrd_p->initial_cpb_removal_delay_length_minus1 = INITIAL_CPB_REMOVAL_DELAY_LENGTH -
	                                                 1;   /* TBC - Why different from default 23 */
	hrd_p->cpb_removal_delay_length_minus1         = CPB_REMOVAL_DELAY_LENGTH -
	                                                 1;           /* TBC - Why different from default 23 */
	hrd_p->dpb_output_delay_length_minus1          = DPB_OUTPUT_DELAY_LENGTH -
	                                                 1;            /* TBC - Why different from default 23 */
	hrd_p->time_offset_length                      = TIME_OFFSET_LENGTH;
	return 0;
}

static uint32_t InitSEIParam(sei_message_t *sei_p)
{
	int32_t i;

	sei_buffering_period_t *buf_period_p;
	sei_pic_timing_t *pic_timing_p;

	sei_p->sei_type = 0;

	buf_period_p = &sei_p->buffering_period;
	pic_timing_p = &sei_p->pic_timing;

	buf_period_p->seq_parameter_set_id = 0;
	buf_period_p->nal_initial_cbp_removal_delay        = 0xAAAAAAAA & ((1 << INITIAL_CPB_REMOVAL_DELAY_LENGTH) - 1);
	buf_period_p->nal_initial_cbp_removal_delay_offset = 0xAAAAAAAA & ((1 << INITIAL_CPB_REMOVAL_DELAY_LENGTH) - 1);
	buf_period_p->vcl_initial_cbp_removal_delay        = 0xAAAAAAAA & ((1 << INITIAL_CPB_REMOVAL_DELAY_LENGTH) - 1);
	buf_period_p->vcl_initial_cbp_removal_delay_offset = 0xAAAAAAAA & ((1 << INITIAL_CPB_REMOVAL_DELAY_LENGTH) - 1);

	pic_timing_p->cpb_removal_delay            = 0xAAAAAAAA & ((1 << CPB_REMOVAL_DELAY_LENGTH) - 1);
	pic_timing_p->dpb_output_delay             = 0xAAAAAAAA & ((1 << DPB_OUTPUT_DELAY_LENGTH) - 1);
	pic_timing_p->pic_struct                   = SEI_PIC_STRUCT_FRAME;

	for (i = 0; i < MAX_NUM_BLOCK_TS; i++) {
		pic_timing_p->clock_timestamp_flag[i]  = 0;
		pic_timing_p->ct_type[i]               = SEI_CT_TYPE_PROGRESSIVE;
		pic_timing_p->nuit_field_based_flag[i] = 0;
		pic_timing_p->counting_type[i]         = SEI_COUNTING_TYPE_0;
		pic_timing_p->full_timestamp_flag[i]   = 0;
		pic_timing_p->discontinuity_flag[i]    = 0;
		pic_timing_p->cnt_dropped_flag[i]      = 0;
		pic_timing_p->n_frames[i]              = 0xAA;
		pic_timing_p->seconds_value[i]         = 0xAA;
		pic_timing_p->minutes_value[i]         = 0xAA;
		pic_timing_p->hours_value[i]           = 0xAA;
		pic_timing_p->seconds_flag[i]          = 0;
		pic_timing_p->minutes_flag[i]          = 0;
		pic_timing_p->hours_flag[i]            = 0;
		pic_timing_p->time_offset[i]           = 0xAA;
	}
	sei_p->nalLastBPAURemovalTime = 0;
	sei_p->vclLastBPAURemovalTime = 0;
	sei_p->nalFinalArrivalTime    = 0;
	sei_p->vclFinalArrivalTime    = 0;
	sei_p->currAUts               = 0;
	sei_p->lastBPAUts             = 0;
	sei_p->forceBP                = 0;

	return 0;
}

static uint32_t UpdateSeqParamSet(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                                  seq_parameter_set_rbsp_t *sps_p)
{
	int32_t i;

	sps_p->profile_idc                           = seqParams_p->profileIdc;
#if 0/* STE does not set constraint flags */
	if (sps_p->profile_idc == SPS_PROFILE_IDC_BASELINE ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_10 ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_422 ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_444) { /* TBC */
		sps_p->constraint_set0_flag = 1;                      /* Associated with SPS_PROFILE_IDC_BASELINE */
		sps_p->constraint_set1_flag = 0;                      /* Associated with SPS_PROFILE_IDC_MAIN */
		sps_p->constraint_set2_flag = 0;                      /* Associated with SPS_PROFILE_IDC_EXTENDED */
		sps_p->constraint_set3_flag = 0;                      /* Associated with level 1b */
	} else if (sps_p->profile_idc == SPS_PROFILE_IDC_MAIN) {
		sps_p->constraint_set0_flag = 0;                      /* Associated with SPS_PROFILE_IDC_BASELINE */
		sps_p->constraint_set1_flag = 1;                      /* Associated with SPS_PROFILE_IDC_MAIN */
		sps_p->constraint_set2_flag = 0;                      /* Associated with SPS_PROFILE_IDC_EXTENDED */
		sps_p->constraint_set3_flag = 0;                      /* Associated with level 1b */
	} else if (sps_p->profile_idc == SPS_PROFILE_IDC_EXTENDED) {
		sps_p->constraint_set0_flag = 0;                      /* Associated with SPS_PROFILE_IDC_BASELINE */
		sps_p->constraint_set1_flag = 0;                      /* Associated with SPS_PROFILE_IDC_MAIN */
		sps_p->constraint_set2_flag = 1;                      /* Associated with SPS_PROFILE_IDC_EXTENDED */
		sps_p->constraint_set3_flag = 0;                      /* Associated with level 1b */
	} else if (sps_p->profile_idc == SPS_PROFILE_IDC_EXTENDED) {
		sps_p->constraint_set0_flag = 0;                      /* Associated with SPS_PROFILE_IDC_BASELINE */
		sps_p->constraint_set1_flag = 0;                      /* Associated with SPS_PROFILE_IDC_MAIN */
		sps_p->constraint_set2_flag = 1;                      /* Associated with SPS_PROFILE_IDC_EXTENDED */
		sps_p->constraint_set3_flag = 0;                      /* Associated with level 1b */
	} else
#endif
	{
		sps_p->constraint_set0_flag = 0;                     /* Associated with SPS_PROFILE_IDC_BASELINE */
		sps_p->constraint_set1_flag = 0;                     /* Associated with SPS_PROFILE_IDC_MAIN */
		sps_p->constraint_set2_flag = 0;                     /* Associated with SPS_PROFILE_IDC_EXTENDED */
		sps_p->constraint_set3_flag = 0;                     /* Associated with level 1b */
	}

	sps_p->level_idc                             = seqParams_p->levelIdc;

	if (sps_p->level_idc == SPS_LEVEL_IDC_1_B) {
		if (sps_p->profile_idc == SPS_PROFILE_IDC_BASELINE ||
		    sps_p->profile_idc == SPS_PROFILE_IDC_MAIN ||
		    sps_p->profile_idc == SPS_PROFILE_IDC_EXTENDED) {
			sps_p->level_idc                     = SPS_LEVEL_IDC_1_1;
			sps_p->constraint_set3_flag          = 1;
		} else {
			sps_p->level_idc                     = SPS_LEVEL_IDC_0_9;
			sps_p->constraint_set3_flag          = 0;
		}
	} else if (sps_p->level_idc == SPS_LEVEL_IDC_0_9) {
		dev_err(pDriverData->dev, "Error: SPS_LEVEL_IDC_0_9 not suppported\n");
		return -1;
	}
	sps_p->seq_parameter_set_id                  =
	        nVCLContext_p->seq_parameter_set_id;   /* TBC - Should this be handled internally ? */
	sps_p->chroma_format_idc                     = SPS_CHROMA_FORMAT_420; /* FIXED */
	sps_p->residual_colour_transform_flag        = 0;                     /* Associated with SPS_CHROMA_FORMAT_444 */
	sps_p->bit_depth_luma_minus8                 = 0;                     /* FIXED */
	sps_p->bit_depth_chroma_minus8               = 0;                     /* FIXED */
	sps_p->qpprime_y_zero_transform_bypass_flag  = 0;                     /* FIXED */
	sps_p->seq_scaling_matrix_present_flag       = 0;
	for (i = 0; i < 8; i++) {
		sps_p->seq_scaling_list_present_flag[i]  = 0;
	}
	sps_p->log2_max_frame_num_minus4             = seqParams_p->log2MaxFrameNumMinus4;
	sps_p->pic_order_cnt_type                    = seqParams_p->picOrderCntType;     /* FIXED */
	sps_p->log2_max_pic_order_cnt_lsb_minus4     = 0;                      /* Associated with pic_order_cnt_type = 0 */
	sps_p->delta_pic_order_always_zero_flag      = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->offset_for_non_ref_pic                = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->offset_for_top_to_bottom_field        = 0;                      /* Associated with pic_order_cnt_type = 1 */
	sps_p->num_ref_frames_in_pic_order_cnt_cycle = 0;                      /* Associated with pic_order_cnt_type = 1 */
	for (i = 0; i < MAX_NUM_REF_FRAME; i++) {
		sps_p->offset_for_ref_frame[i] = 0;                                /* Associated with pic_order_cnt_type = 1 */
	}
	sps_p->num_ref_frames                        = 1;                      /* FIXED - only 1 reference frame */
	sps_p->gaps_in_frame_num_value_allowed_flag  = 0;
	sps_p->pic_width_in_mbs_minus1               = ((seqParams_p->frameWidth + 15) >> 4) - 1;
	sps_p->pic_height_in_map_units_minus1        = ((seqParams_p->frameHeight + 15) >> 4) - 1;
	sps_p->frame_mbs_only_flag                   = 1;                      /* FIXED - only Progressive */
	sps_p->mbs_adaptive_frame_field_flag         = 0;                      /* Associated with frame_mbs_only_flag = 0 */
	sps_p->direct_8x8_inference_flag             = 1;                      /* FIXED */
	if (seqParams_p->frameWidth != ((sps_p->pic_width_in_mbs_minus1 + 1) << 4) ||
	    seqParams_p->frameHeight != ((sps_p->pic_height_in_map_units_minus1 + 1) << 4)) {
		sps_p->frame_cropping_flag               = 1;
		sps_p->frame_crop_left_offset            = 0;
		if ((seqParams_p->frameWidth & 0xF) != 0) {
			sps_p->frame_crop_right_offset       = (16 - (seqParams_p->frameWidth & 0xF)) >> 1;
		}
		sps_p->frame_crop_top_offset             = 0;
		if ((seqParams_p->frameHeight & 0xF) != 0) {
			sps_p->frame_crop_bottom_offset       = (16 - (seqParams_p->frameHeight & 0xF)) >> 1;
		}
	} else {
		sps_p->frame_cropping_flag               = 0;
	}
	sps_p->vui_parameters_present_flag           = seqParams_p->vuiParametersPresentFlag;
	if (sps_p->vui_parameters_present_flag == 1) {
		UpdateVUIParam(nVCLContext_p, seqParams_p, &sps_p->vui_param);
	}
	/* Update Context */
	nVCLContext_p->brcType = seqParams_p->brcType;
	nVCLContext_p->framerateNum               = seqParams_p->framerateNum;
	nVCLContext_p->framerateDen               = seqParams_p->framerateDen;
	if (sps_p->vui_parameters_present_flag == 1) {
		nVCLContext_p->bitrate                = (sps_p->vui_param.nal_hrd_param.bit_rate_value_minus1[0] + 1) * (1 <<
		                                                                                                         (sps_p->vui_param.nal_hrd_param.bit_rate_scale + 6));
		nVCLContext_p->picStructPresentFlag   = sps_p->vui_param.pic_struct_present_flag;
	} else {
		nVCLContext_p->bitrate                = seqParams_p->bitRate;
		nVCLContext_p->picStructPresentFlag   = 0;
	}
	nVCLContext_p->vuiParametersPresentFlag   = seqParams_p->vuiParametersPresentFlag;
	nVCLContext_p->seiPicTimingPresentFlag    = seqParams_p->seiPicTimingPresentFlag;
	nVCLContext_p->seiBufPeriodPresentFlag    = seqParams_p->seiBufPeriodPresentFlag;

	nVCLContext_p->contrainedIntraPredFlag    = (seqParams_p->useConstrainedIntraFlag ? 1 : 0);
	nVCLContext_p->transform8x8ModeFlag       = seqParams_p->transformMode;

	return 0;
}

static uint32_t UpdatePicParamSet(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                  pic_parameter_set_rbsp_t *pps_p)
{
	int32_t i;

	pps_p->pic_parameter_set_id                   = nVCLContext_p->pic_parameter_set_id;
	pps_p->seq_parameter_set_id                   = nVCLContext_p->seq_parameter_set_id;
	pps_p->entropy_coding_mode_flag               = nVCLContext_p->entropyCodingMode;
	pps_p->pic_order_present_flag                 = 0;                        /* FIXED - related to slice */
	pps_p->num_slice_groups_minus1                = 0;
	pps_p->slice_group_map_type                   =
	        0;                        /* Associated with num_slice_groups_minus1 > 0 */
	for (i = 0; i < MAX_NUM_SLICE_GROUPS; i++) {
		pps_p->run_length_minus1[i]               = 0;                        /* Associated with slice_group_map = 0 */
		pps_p->top_left[i]                        = 0;                        /* Associated with slice_group_map = 2 */
		pps_p->bottom_right[i]                    = 0;                        /* Associated with slice_group_map = 2 */
	}
	pps_p->slice_group_change_direction_flag      = 0;                        /* Associated with slice_group_map = 3-5 */
	pps_p->slice_group_change_rate_minus1         = 0;                        /* Associated with slice_group_map = 3-5 */
	pps_p->pic_size_in_map_units_minus1           = 0;                        /* Associated with slice_group_map = 6 */
	for (i = 0; i < MAX_NUM_SLICE_GROUPS; i++) {
		pps_p->slice_group_id[i]                  = 0;                        /* Associated with slice_group_map = 2 */
	}
	pps_p->num_ref_idx_10_active_minus1           = 0;                        /* FIXED - only 1 reference frame */
	pps_p->num_ref_idx_11_active_minus1           = 0;                        /* FIXED - Not supported */
	pps_p->weighted_pred_flag                     = 0;                        /* FIXED - Not supported */
	pps_p->weighted_bipred_idc                    = 0;                        /* FIXED - Not supported */
	pps_p->pic_init_qp_minus26                    = 0;
	pps_p->pic_init_qs_minus26                    = 0;
	pps_p->chroma_qp_index_offset                 = nVCLContext_p->chromaQpIndexOffset;
	pps_p->deblocking_filter_control_present_flag = 1;                        /* FIXED */
	pps_p->contrained_intra_pred_flag             = nVCLContext_p->contrainedIntraPredFlag;
	pps_p->redundant_pic_cnt_present_flag         = 0;                        /* FIXED */
	pps_p->transform_8x8_mode_flag                = nVCLContext_p->transform8x8ModeFlag;
	pps_p->pic_scaling_matrix_present_flag        = 0;
	for (i = 0; i < 8; i++) {
		pps_p->pic_scaling_list_present_flag[i]   = 0;
	}
	pps_p->second_chroma_qp_index_offset          = nVCLContext_p->chromaQpIndexOffset;

	return 0;
}

static uint32_t UpdateVUIParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                               vui_parameter_t *vui_p)
{
	vui_p->aspect_ratio_info_present_flag          = 1;
	vui_p->aspect_ratio_idc                        = seqParams_p->vuiAspectRatioIdc;
	vui_p->sar_width                               = seqParams_p->vuiSarWidth;
	vui_p->sar_height                              = seqParams_p->vuiSarHeight;
	vui_p->overscan_info_present_flag              = 0;
	vui_p->overscan_appropriate_flag               = seqParams_p->vuiOverscanAppropriateFlag;
	vui_p->video_signal_type_present_flag          = 1;
	vui_p->video_format                            = seqParams_p->vuiVideoFormat;
	vui_p->video_full_range_flag                   = seqParams_p->vuiVideoFullRangeFlag;
	vui_p->colour_description_present_flag         = 1;
	if (seqParams_p->vuiColorStd > 0 && seqParams_p->vuiColorStd < VUI_COLOR_STD_MAX) {
		vui_p->colour_primaries                    = seqParams_p->vuiColorStd;
		vui_p->transfer_characteristics            = seqParams_p->vuiColorStd;
		vui_p->matrix_coefficients                 = seqParams_p->vuiColorStd;
	} else {
		vui_p->colour_primaries                    = VUI_COLOR_UNSPECIFIED;
		vui_p->transfer_characteristics            = VUI_TRANSFER_UNSPECIFIED;
		vui_p->matrix_coefficients                 = VUI_MATRIX_UNSPECIFIED;
	}
	vui_p->chroma_loc_info_present_flag            = 0;                            /* FIXED */
	vui_p->chroma_sample_loc_type_top_field        =
	        0;                            /* Associated with chroma_loc_info_present_flag */
	vui_p->chroma_sample_loc_type_bottom_field     =
	        0;                            /* Associated with chroma_loc_info_present_flag */
	vui_p->timing_info_present_flag                = 1;
	/* Compute field rate */
	if (seqParams_p->framerateNum && seqParams_p->framerateDen) {
		vui_p->time_scale                          = SEI_TIME_SCALE;
		vui_p->num_units_in_tick                   = (uint32_t)((seqParams_p->framerateDen * vui_p->time_scale / 2) /
		                                                        seqParams_p->framerateNum);
	} else {
		dev_dbg(pDriverData->dev, "Framerate is incorrect, use default one instead\n");
		vui_p->time_scale                          = SEI_TIME_SCALE;
		vui_p->num_units_in_tick                   = (uint32_t)((H264_ENCODE_DEFAULT_FRAMERATE_DEN * vui_p->time_scale / 2) /
		                                                        H264_ENCODE_DEFAULT_FRAMERATE_NUM);
	}
	vui_p->fixed_frame_rate_flag                   = 0;                            /* Allow frame skip */
	vui_p->nal_hrd_parameters_present_flag         = 1;
	if (vui_p->nal_hrd_parameters_present_flag == 1) {
		UpdateHRDParam(nVCLContext_p, seqParams_p, &vui_p->nal_hrd_param, 0);
	}
	vui_p->vcl_hrd_parameters_present_flag         = 1;
	if (vui_p->vcl_hrd_parameters_present_flag == 1) {
		UpdateHRDParam(nVCLContext_p, seqParams_p, &vui_p->vcl_hrd_param, 1);
	}
	vui_p->low_delay_hrd_flag                      = 1;                            /* FIXED */
	vui_p->pic_struct_present_flag                 = 0;
	vui_p->bitstream_restriction_flag              = 0;
	vui_p->motion_vectors_over_pic_boundaries_flag = 1;                            /* FIXED - HW capability */
	vui_p->max_bytes_per_pic_denom                 = 0;                            /* FIXED - unlimited */
	vui_p->max_bits_per_mb_denom                   = 0;                            /* FIXED - unlimited */
	vui_p->log2_max_mv_length_horizontal           =
	        16;                           /* TBC - Constrained by HW -/+30 pel motion search window =>8 */
	vui_p->log2_max_mv_length_vertical             =
	        16;                           /* TBC - Constrained by HW -/+30 pel motion search window =>8 */
	vui_p->num_reorder_frames                      = 0;                            /* FIXED */
	vui_p->max_dec_frame_buffering                 = 1;                            /* FIXED */

	return 0;
}

static uint32_t H264HwEncodeReconstructParameter(uint32_t value, int32_t scale, uint32_t scale_offset)
{
	return (value * (1 << (scale + scale_offset)));
}

static value_scale_deconstruction_t H264HwEncodeDeconstructParameter(uint32_t targeted_value, uint32_t scale_offset)
{
	value_scale_deconstruction_t deconstructed_parameter;

	deconstructed_parameter.value = targeted_value;
	deconstructed_parameter.scale = 0;
	// Patch to avoid negs for scale
	// Patch for u_v() syntax element limit: it cannot write a more than 32 bit lenght bitpatter.
	// Limiting the value to be u_v()-coded to 65534 the problem is avoided.
	// This will limit the precision of the bitrate value coded in the stream.
	while ((((deconstructed_parameter.value & 0x01) == 0) && (deconstructed_parameter.scale < (16 + scale_offset - 1))) ||
	       (((deconstructed_parameter.value & 0x01) == 1) &&
	        ((deconstructed_parameter.scale < scale_offset) || (deconstructed_parameter.value > 65535)))) {
		deconstructed_parameter.value >>= 1;
		deconstructed_parameter.scale++;
	}
	deconstructed_parameter.scale -= scale_offset;

	/*if((H264HwEncodeReconstructParameter(deconstructed_parameter.value, deconstructed_parameter.scale, scale_offset) > targeted_value) || (deconstructed_parameter.value > 65535))
	{
	    deconstructed_parameter.value--;
	    deconstructed_parameter = H264HwEncodeDeconstructParameter(H264HwEncodeReconstructParameter(deconstructed_parameter.value, deconstructed_parameter.scale, scale_offset), scale_offset);
	}*/

	return deconstructed_parameter;
}

uint32_t H264HwEncodeReconstructBitrate(uint32_t value, int32_t scale)
{
	return H264HwEncodeReconstructParameter(value, scale, BITRATE_SCALE_OFFSET);
}

uint32_t H264HwEncodeReconstructCpbSize(uint32_t value, int32_t scale)
{
	return H264HwEncodeReconstructParameter(value, scale, CPB_SIZE_SCALE_OFFSET);
}

value_scale_deconstruction_t H264HwEncodeDeconstructBitrate(uint32_t bitrate)
{
	return H264HwEncodeDeconstructParameter(bitrate, BITRATE_SCALE_OFFSET);
}

value_scale_deconstruction_t H264HwEncodeDeconstructCpbSize(uint32_t cpb_size)
{
	return H264HwEncodeDeconstructParameter(cpb_size, CPB_SIZE_SCALE_OFFSET);
}

static uint32_t UpdateHRDParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                               hrd_parameter_t *hrd_p, uint8_t type)
{
	value_scale_deconstruction_t deconstructed_parameter;
	uint32_t reconstructed_parameter;

	/* TBC - To handle differences between NAL and VCL */

	hrd_p->cpb_cnt_minus1 = 0;                 /* FIXED */

	// Compute bit rate
	deconstructed_parameter = H264HwEncodeDeconstructBitrate(seqParams_p->bitRate);
	dev_dbg(pDriverData->dev, "Bitrate: deconstructed_parameter.value = %d, deconstructed_parameter.scale = %d\n",
	        deconstructed_parameter.value, deconstructed_parameter.scale);
	reconstructed_parameter = H264HwEncodeReconstructBitrate(deconstructed_parameter.value, deconstructed_parameter.scale);
	dev_dbg(pDriverData->dev, "Bitrate is %d bps, reconstructed bitrate is %d bps\n", seqParams_p->bitRate,
	        reconstructed_parameter);
	if (reconstructed_parameter != seqParams_p->bitRate) {
		dev_dbg(pDriverData->dev, "Bitrate is %d bps, reconstructed bitrate is %d bps\n", seqParams_p->bitRate,
		        reconstructed_parameter);
	}
	hrd_p->bit_rate_value_minus1[0] = deconstructed_parameter.value - 1;
	hrd_p->bit_rate_scale = deconstructed_parameter.scale;

	// Compute cpb buffer size
	deconstructed_parameter = H264HwEncodeDeconstructCpbSize(seqParams_p->cpbBufferSize);
	dev_dbg(pDriverData->dev, "Cpb size: deconstructed_parameter.value = %d, deconstructed_parameter.scale = %d\n",
	        deconstructed_parameter.value, deconstructed_parameter.scale);
	reconstructed_parameter = H264HwEncodeReconstructCpbSize(deconstructed_parameter.value, deconstructed_parameter.scale);
	dev_dbg(pDriverData->dev, "Cpb size is %d bits, reconstructed cpb size is %d bits\n", seqParams_p->cpbBufferSize,
	        reconstructed_parameter);
	if (reconstructed_parameter != seqParams_p->cpbBufferSize) {
		dev_dbg(pDriverData->dev, "Cpb size is %d bits, reconstructed cpb size is %d bits\n", seqParams_p->cpbBufferSize,
		        reconstructed_parameter);
	}
	hrd_p->cpb_size_value_minus1[0] = deconstructed_parameter.value - 1;
	hrd_p->cpb_size_scale = deconstructed_parameter.scale;

	hrd_p->cbr_flag[0]                   =
	        1; //FIXED since BRC algorithm is CBR seqParams_p->brcType == HVA_ENCODE_CBR? 1 : 0;/* Example level 3.2 */

	hrd_p->initial_cpb_removal_delay_length_minus1 = nVCLContext_p->initial_cpb_removal_delay_length -
	                                                 1;/* TBC - Why different from default 23 */
	hrd_p->cpb_removal_delay_length_minus1         = nVCLContext_p->cpb_removal_delay_length -
	                                                 1;/* TBC - Why different from default 23 */
	hrd_p->dpb_output_delay_length_minus1          = nVCLContext_p->dpb_output_delay_length -
	                                                 1;/* TBC - Why different from default 23 */
	hrd_p->time_offset_length                      = nVCLContext_p->time_offset_length;

	return 0;
}

static uint32_t UpdateSEIParam(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                               H264EncodeHardParamOut_t *outParams_p,
                               sei_message_t *sei_p)
{
	uint64_t deltaTime;
	uint32_t deltaTs;
	sei_buffering_period_t *buf_period_p;
	sei_pic_timing_t *pic_timing_p;

	/* Copy HRD parameters */
	sei_p->initial_cpb_removal_delay_length = nVCLContext_p->initial_cpb_removal_delay_length;
	sei_p->cpb_removal_delay_length         = nVCLContext_p->cpb_removal_delay_length;
	sei_p->dpb_output_delay_length          = nVCLContext_p->dpb_output_delay_length;
	sei_p->time_offset_length               = nVCLContext_p->time_offset_length;
	/* Copy VUI parameters */
	sei_p->picStructPresentFlag             = nVCLContext_p->picStructPresentFlag;

	/* Update removal time */
	sei_p->currAUts = outParams_p->removalTime;

	/* Units of time stamp: OneOverFPS for VBR and OneOver90k for first frame in CBR */
	deltaTs = (int32_t)sei_p->currAUts - (int32_t)sei_p->lastBPAUts;
	if ((nVCLContext_p->brcType == HVA_ENCODE_VBR) || (!frameParams_p->firstPictureInSequence)) {
		deltaTime = (uint64_t)deltaTs * 2ULL * (uint64_t)nVCLContext_p->seqParams.vui_param.num_units_in_tick *
		            ULL(SEI_90KHZ_TIME_SCALE) * (1ULL << TIMING_PRECISION);
		do_div(deltaTime, (uint64_t)nVCLContext_p->seqParams.vui_param.time_scale);
	} else {
		deltaTime = (uint64_t)deltaTs * 2ULL * (1ULL << TIMING_PRECISION);
		do_div(deltaTime, (uint64_t)SEI_90KHZ_TIME_SCALE);
	}

	dev_dbg(pDriverData->dev, "currAUts = %d, lastBPAUts = %d, deltaTs = %d, deltaTime = %llu\n",
	        sei_p->currAUts, sei_p->lastBPAUts, deltaTs, deltaTime >> TIMING_PRECISION);

	/* To take care of start slice */
	if (nVCLContext_p->seiBufPeriodPresentFlag &&
	    (sei_p->forceBP || frameParams_p->idrFlag == 1 || frameParams_p->firstPictureInSequence == 1)) {
		sei_p->sei_type |= (1 << SEI_BUFFERING_PERIOD);
	} else {
		/* TBC - To check representation */
		sei_p->sei_type &= ~(1 << SEI_BUFFERING_PERIOD);
	}
	if (nVCLContext_p->seiPicTimingPresentFlag) {
		sei_p->sei_type |= (1 << SEI_PICTURE_TIMING);
	} else {
		sei_p->sei_type &= ~(1 << SEI_PICTURE_TIMING);
	}

	buf_period_p = &sei_p->buffering_period;
	pic_timing_p = &sei_p->pic_timing;

	/* Update buffering period */
	if (sei_p->sei_type & (1 << SEI_BUFFERING_PERIOD)) {

		buf_period_p->seq_parameter_set_id = nVCLContext_p->seq_parameter_set_id;

		sei_p->nalLastBPAURemovalTime += deltaTime;
		sei_p->vclLastBPAURemovalTime += deltaTime;
		buf_period_p->nal_initial_cbp_removal_delay = 0;
		if (sei_p->nalLastBPAURemovalTime >= sei_p->nalFinalArrivalTime) {
			buf_period_p->nal_initial_cbp_removal_delay =
			        (uint32_t)((sei_p->nalLastBPAURemovalTime - sei_p->nalFinalArrivalTime) >> TIMING_PRECISION);
		}
		buf_period_p->vcl_initial_cbp_removal_delay = 0;
		if (sei_p->vclLastBPAURemovalTime >= sei_p->vclFinalArrivalTime) {
			buf_period_p->vcl_initial_cbp_removal_delay =
			        (uint32_t)((sei_p->vclLastBPAURemovalTime - sei_p->vclFinalArrivalTime) >> TIMING_PRECISION);
		}
		dev_dbg(pDriverData->dev, "nalLastBPAURemovalTime = %llu, nalFinalArrivalTime = %llu\n",
		        sei_p->nalLastBPAURemovalTime >> TIMING_PRECISION, sei_p->nalFinalArrivalTime >> TIMING_PRECISION);
		dev_dbg(pDriverData->dev, "nal_initial_cbp_removal_delay = %u\n", buf_period_p->nal_initial_cbp_removal_delay);
		dev_dbg(pDriverData->dev, "currAUts = %u, lastBPAUts = %u\n", sei_p->currAUts, sei_p->lastBPAUts);
		dev_dbg(pDriverData->dev, "deltaTs = %u, deltaTime = %llu\n", deltaTs, deltaTime >> TIMING_PRECISION);
	}
	/* Update pic timing */
	if (sei_p->sei_type & (1 << SEI_PICTURE_TIMING)) {
		if (frameParams_p->firstPictureInSequence == 1) {
			pic_timing_p->cpb_removal_delay = 0;
			pic_timing_p->dpb_output_delay  = 0;
		} else {
			// First, we spend 1 unit of time to fill the cpb buffer with a complete frame(n).
			// As cpb removal delay specifies how many clock ticks to wait after removal from
			// the cpb of the previous access unit, it makes sense to put deltaTs * 2 as clock
			// ticks is expressed in 1/fieldrate.
			pic_timing_p->cpb_removal_delay = deltaTs * 2;

			// Secondly, frame(n) is decoded and waits another unit of time to receive frame(n+1).
			// Then, frame(n+1) is decoded using frame(n) as a reference. Finally, frame(n) is displayed.
			// As dpb output delay specifies how many clock ticks to wait after frame(n) is decoded
			// and before it can be output from the dpb to the display, it makes sense to put 2
			// as clock ticks is expressed in 1/fieldrate.
			pic_timing_p->dpb_output_delay  = 2;
		}

		/* TBC - Update other pic timing parameters according to encode format? */
	}

	return 0;
}

static uint32_t UpdateSEIParam_post(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                    H264EncodeHardParamOut_t *outParams_p, sei_message_t *sei_p)
{
	uint32_t limit;
	int32_t deltaTs;
	uint32_t fillLen;
	uint64_t deltaTime;

	if (outParams_p->stuffingBits > 0) { fillLen = (outParams_p->stuffingBits / 8) + FILLER_DATA_HEADER_SIZE; }
	else { fillLen = 0; }

	dev_dbg(pDriverData->dev, "nvclNALUSize = %u, bitstreamSize = %u, bitrate = %u, fillLen = %u\n",
	        nVCLContext_p->nvclNALUSize,
	        outParams_p->bitstreamSize, nVCLContext_p->bitrate, fillLen);

	if ((nVCLContext_p->bitrate >> 4) != 0) {
		/* All ArrivalTimes and LastBPAURemovalTimes are using 90k units for simplication */
		/* A precision equivalent to TIMING PRECISION is used to maintain some accuracy of timing computation */
		deltaTime = (uint64_t)(nVCLContext_p->nvclNALUSize + outParams_p->bitstreamSize + fillLen) * ULL(
		                    SEI_90KHZ_TIME_SCALE) * (1ULL << TIMING_PRECISION);
		do_div(deltaTime, (uint64_t)nVCLContext_p->bitrate / 8ULL);
		sei_p->nalFinalArrivalTime += deltaTime;

		deltaTime = (uint64_t)outParams_p->bitstreamSize * ULL(SEI_90KHZ_TIME_SCALE) * (1ULL << TIMING_PRECISION);
		do_div(deltaTime, (uint64_t)nVCLContext_p->bitrate / 8ULL);
		sei_p->vclFinalArrivalTime += deltaTime;
	} else {
		dev_err(pDriverData->dev, "Error: division by 0 as bitrate < 16\n");
		return -1;
	}

	dev_dbg(pDriverData->dev, "nalFinalArrivalTime = %llu, vclFinalArrivalTime = %llu\n",
	        sei_p->nalFinalArrivalTime >> TIMING_PRECISION, sei_p->vclFinalArrivalTime >> TIMING_PRECISION);

	/* reset the cpb removal delay after BP SEI */
	if (sei_p->sei_type & (1 << SEI_BUFFERING_PERIOD)) {
		sei_p->lastBPAUts = sei_p->currAUts;
	}

	/* NEXT forceBP */
	limit = (nVCLContext_p->cpb_removal_delay_length < nVCLContext_p->dpb_output_delay_length) ?
	        (1 << (nVCLContext_p->cpb_removal_delay_length - 1)) : (1 << (nVCLContext_p->dpb_output_delay_length - 1));

	deltaTs = (int32_t)sei_p->currAUts - (int32_t)sei_p->lastBPAUts;

	if (deltaTs > limit) { sei_p->forceBP = 1; } /* is it too late???  or anticipating using a reduced limit */

	/* Updating frame parameters */
	//HVA Encoder does not use this parameter.
	//frameParams_p->nalFinalArrivalTime = (sei_p->nalFinalArrivalTime * 4081 + (5625<<(TIMING_PRECISION+5)))/ (5625<<(TIMING_PRECISION+4));

	return 0;
}

/*!
*************************************************************************************
* \brief
*    U32 GenerateAccessUnitDelimiter_NALU ();
*
* \note
*
*
* \return
*    A NALU containing the Sequence ParameterSet
*
*************************************************************************************
*/
static NALU_t *GenerateAccessUnitDelimiter_NALU(H264EncodeHardFrameParams_t *frameParams_p, NALU_t *nalu_p)
{
	int32_t rbspLen;
	int32_t naluLen;
	uint8_t rbsp_p[NVCL_SCRATCH_BUFFER_SIZE];
	rbspLen = naluLen = 0;

	if (nalu_p == 0) {
		dev_err(pDriverData->dev, "Error: invalid nalu pointer\n");
		return 0;
	}
	// Init nalu structure
	nalu_p->maxLen  = 1;

	rbspLen = GenerateAccessUnitDelimiter_rbsp(frameParams_p, rbsp_p);
	if (rbspLen == -1) {
		dev_err(pDriverData->dev, "Error: rbsp generation of access unit delimiter failed\n");
		return 0;
	}
	if (rbspLen > 1) {
		dev_err(pDriverData->dev, "Error: bad rbspLen (%d)\n", rbspLen);
		return 0;
	}
	naluLen = RBSPtoNALU(rbsp_p, nalu_p, rbspLen, NALU_TYPE_AUD, NALU_PRIORITY_DISPOSABLE, 0, 1);
	if (naluLen == -1) {
		dev_err(pDriverData->dev, "Error: RBSPtoNALU failed\n");
		return 0;
	}
	nalu_p->len = naluLen;

	return nalu_p;
}

/*!
 *************************************************************************************
 * \brief
 *    GenerateAccessUnitDelimiter_rbsp
 *
 * \param rbsp
 *    buffer to be filled with the rbsp, size should be at least MAXIMUMPARSETRBSPSIZE
 *
 * \return
 *    size of the RBSP in bytes
 *
 *************************************************************************************
 */
static uint32_t GenerateAccessUnitDelimiter_rbsp(H264EncodeHardFrameParams_t *frameParams_p, uint8_t *rbsp_p)
{
	bitstream_t stream;
	bitstream_t *stream_p = &stream;
	uint32_t len;
	uint32_t lenInBytes;
	len = 0;

	stream_p->streamBuf_p = (uint8_t *)rbsp_p;
	stream_p->bits_to_go = 8;
	stream_p->byte_pos = 0;

	len += u_v(3, (int32_t)frameParams_p->pictureCodingType,
	           stream_p);      /* Primary Pic TYpe : 0 for I picture, 1 for I/P, 2 for I/P/B */

	SODBtoRBSP(stream_p);     /* copies the last couple of bits into the byte buffer */
	lenInBytes = stream_p->byte_pos;

	return lenInBytes;
}

static NALU_t *GenerateSeqParamSet_NALU(seq_parameter_set_rbsp_t *sps_p, NALU_t *nalu_p)
{
	uint8_t rbsp_p[NVCL_SCRATCH_BUFFER_SIZE];
	int32_t rbspLen;
	int32_t naluLen;
	rbspLen = naluLen = 0;

	if (nalu_p == 0) {
		dev_err(pDriverData->dev, "Error: invalid nalu pointer\n");
		return 0;
	}
	// Init nalu structure
	nalu_p->maxLen  = NVCL_SCRATCH_BUFFER_SIZE;

	rbspLen = GenerateSeqParamSet_rbsp(sps_p, rbsp_p);
	if (rbspLen == -1) {
		dev_err(pDriverData->dev, "Error: rbsp generation of sequence parameter set failed\n");
		return 0;
	}
	naluLen = RBSPtoNALU(rbsp_p, nalu_p, rbspLen, NALU_TYPE_SPS, NALU_PRIORITY_HIGHEST, 0, 1);
	if (naluLen == -1) {
		dev_err(pDriverData->dev, "Error: RBSPtoNALU failed\n");
		return 0;
	}
	nalu_p->len = naluLen;

	return nalu_p;
}

static uint32_t GenerateSeqParamSet_rbsp(seq_parameter_set_rbsp_t *sps_p, uint8_t *rbsp_p)
{
	bitstream_t stream;
	bitstream_t *stream_p = &stream;
	uint32_t len;
	uint32_t lenInBytes;
	int32_t i;

	len = 0;

	stream_p->streamBuf_p = (uint8_t *)rbsp_p;
	stream_p->bits_to_go = 8;
	stream_p->byte_pos = 0;
	len += u_v(8, sps_p->profile_idc,             stream_p);
	len += u_1(sps_p->constraint_set0_flag,    stream_p);
	len += u_1(sps_p->constraint_set1_flag,    stream_p);
	len += u_1(sps_p->constraint_set2_flag,    stream_p);
	len += u_1(sps_p->constraint_set3_flag,    stream_p);
	len += u_v(4, 0,                              stream_p);
	len += u_v(8, sps_p->level_idc,               stream_p);
	len += ue_v(sps_p->seq_parameter_set_id,    stream_p);
	/* Fidelity Range Extensions stuff */
	if ((sps_p->profile_idc == SPS_PROFILE_IDC_HIGH) ||
	    (sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_10) ||
	    (sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_422) ||
	    (sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_444)) {
		len += ue_v(sps_p->chroma_format_idc,      stream_p);
		len += ue_v(sps_p->bit_depth_luma_minus8,  stream_p);
		len += ue_v(sps_p->bit_depth_chroma_minus8, stream_p);
		len += u_1(sps_p->qpprime_y_zero_transform_bypass_flag, stream_p);
		len += u_1(sps_p->seq_scaling_matrix_present_flag, stream_p);
		if (sps_p->seq_scaling_matrix_present_flag == 1) {
			for (i = 0; i < 8; i++) {
				/* len += u_1 ( sps_p->seq_scaling_list_present_flag, stream_p); */
				len += u_1(0, stream_p);
			}
		}
	}

	len += ue_v(sps_p->log2_max_frame_num_minus4,                   stream_p);
	len += ue_v(sps_p->pic_order_cnt_type,                          stream_p);

	if (sps_p->pic_order_cnt_type == 0) {
		len += ue_v(sps_p->log2_max_pic_order_cnt_lsb_minus4,       stream_p);
	} else if (sps_p->pic_order_cnt_type == 1) {
		len += u_1(sps_p->delta_pic_order_always_zero_flag,           stream_p);
		len += se_v(sps_p->offset_for_non_ref_pic,                     stream_p);
		len += se_v(sps_p->offset_for_top_to_bottom_field,             stream_p);
		len += ue_v(sps_p->num_ref_frames_in_pic_order_cnt_cycle,      stream_p);
		for (i = 0; i < sps_p->num_ref_frames_in_pic_order_cnt_cycle; i++) {
			len += se_v(sps_p->offset_for_ref_frame[i],                  stream_p);
		}
	}
	len += ue_v(sps_p->num_ref_frames,                               stream_p);   /* FP: num_ref_frames forced to '1' */
	len += u_1(sps_p->gaps_in_frame_num_value_allowed_flag,         stream_p);
	len += ue_v(sps_p->pic_width_in_mbs_minus1,                      stream_p);
	len += ue_v(sps_p->pic_height_in_map_units_minus1,               stream_p);
	len += u_1(sps_p->frame_mbs_only_flag,                          stream_p);
	if (sps_p->frame_mbs_only_flag == 0) {
		len += u_1(sps_p->mbs_adaptive_frame_field_flag,            stream_p);
	}
	len += u_1(sps_p->direct_8x8_inference_flag,                    stream_p);

	len += u_1(sps_p->frame_cropping_flag,                          stream_p);
	if (sps_p->frame_cropping_flag == 1) {
		len += ue_v(sps_p->frame_crop_left_offset,                     stream_p);
		len += ue_v(sps_p->frame_crop_right_offset,                    stream_p);
		len += ue_v(sps_p->frame_crop_top_offset,                      stream_p);
		len += ue_v(sps_p->frame_crop_bottom_offset,                   stream_p);
	}

	len += u_1(sps_p->vui_parameters_present_flag,                  stream_p);
	if (sps_p->vui_parameters_present_flag == 1) {
		len += GenerateVUIParam_rbsp(&sps_p->vui_param, stream_p);        /* currently a dummy, asserting */
	}

	SODBtoRBSP(stream_p);     /* copies the last couple of bits into the byte buffer */
	lenInBytes = stream_p->byte_pos;

	return lenInBytes;
}

static uint32_t GenerateVUIParam_rbsp(vui_parameter_t *vui_p, bitstream_t *stream_p)
{
	int32_t len;
	int32_t i;

	len = 0;
	len += u_1(vui_p->aspect_ratio_info_present_flag,          stream_p);
	if (vui_p->aspect_ratio_info_present_flag == 1) {
		len += u_v(8, vui_p->aspect_ratio_idc,                 stream_p);
		if (vui_p->aspect_ratio_idc == 255) {
			len += u_v(16, vui_p->sar_width,                   stream_p);
			len += u_v(16, vui_p->sar_height,                  stream_p);
		}
	}

	len += u_1(vui_p->overscan_info_present_flag,             stream_p);
	if (vui_p->overscan_info_present_flag == 1) {
		len += u_1(vui_p->overscan_appropriate_flag,           stream_p);
	}

	len += u_1(vui_p->video_signal_type_present_flag,          stream_p);
	if (vui_p->video_signal_type_present_flag == 1) {
		len += u_v(3, vui_p->video_format,                      stream_p);
		len += u_1(vui_p->video_full_range_flag,               stream_p);
		len += u_1(vui_p->colour_description_present_flag,     stream_p);
		if (vui_p->colour_description_present_flag == 1) {
			len += u_v(8, vui_p->colour_primaries,             stream_p);
			len += u_v(8, vui_p->transfer_characteristics,     stream_p);
			len += u_v(8, vui_p->matrix_coefficients,          stream_p);
		}
	}

	len += u_1(vui_p->chroma_loc_info_present_flag,            stream_p);
	if (vui_p->chroma_loc_info_present_flag == 1) {
		len += ue_v(vui_p->chroma_sample_loc_type_top_field,    stream_p);
		len += ue_v(vui_p->chroma_sample_loc_type_bottom_field, stream_p);
	}

	len += u_1(vui_p->timing_info_present_flag,                stream_p);
	if (vui_p->timing_info_present_flag == 1) {
		len += u_v(32, vui_p->num_units_in_tick,                stream_p);
		len += u_v(32, vui_p->time_scale,                       stream_p);
		len += u_1(vui_p->fixed_frame_rate_flag,               stream_p);
	}

	len += u_1(vui_p->nal_hrd_parameters_present_flag,          stream_p);
	if (vui_p->nal_hrd_parameters_present_flag == 1) {
		len += ue_v(vui_p->nal_hrd_param.cpb_cnt_minus1,        stream_p);
		len += u_v(4, vui_p->nal_hrd_param.bit_rate_scale,      stream_p);
		len += u_v(4, vui_p->nal_hrd_param.cpb_size_scale,      stream_p);
		for (i = 0; i < (vui_p->nal_hrd_param.cpb_cnt_minus1 + 1); i++) {
			len += ue_v(vui_p->nal_hrd_param.bit_rate_value_minus1[i], stream_p);
			len += ue_v(vui_p->nal_hrd_param.cpb_size_value_minus1[i], stream_p);
			len += u_1(vui_p->nal_hrd_param.cbr_flag[i],              stream_p);
		}
		len += u_v(5, vui_p->nal_hrd_param.initial_cpb_removal_delay_length_minus1, stream_p);
		len += u_v(5, vui_p->nal_hrd_param.cpb_removal_delay_length_minus1,         stream_p);
		len += u_v(5, vui_p->nal_hrd_param.dpb_output_delay_length_minus1,          stream_p);
		len += u_v(5, vui_p->nal_hrd_param.time_offset_length,                      stream_p);
	}

	len += u_1(vui_p->vcl_hrd_parameters_present_flag,        stream_p);
	if (vui_p->vcl_hrd_parameters_present_flag == 1) {
		len += ue_v(vui_p->vcl_hrd_param.cpb_cnt_minus1,        stream_p);
		len += u_v(4, vui_p->vcl_hrd_param.bit_rate_scale,      stream_p);
		len += u_v(4, vui_p->vcl_hrd_param.cpb_size_scale,      stream_p);
		for (i = 0; i < (vui_p->vcl_hrd_param.cpb_cnt_minus1 + 1); i++) {
			len += ue_v(vui_p->vcl_hrd_param.bit_rate_value_minus1[i], stream_p);
			len += ue_v(vui_p->vcl_hrd_param.cpb_size_value_minus1[i], stream_p);
			len += u_1(vui_p->vcl_hrd_param.cbr_flag[i],              stream_p);
		}
		len += u_v(5, vui_p->vcl_hrd_param.initial_cpb_removal_delay_length_minus1, stream_p);
		len += u_v(5, vui_p->vcl_hrd_param.cpb_removal_delay_length_minus1,         stream_p);
		len += u_v(5, vui_p->vcl_hrd_param.dpb_output_delay_length_minus1,          stream_p);
		len += u_v(5, vui_p->vcl_hrd_param.time_offset_length,                      stream_p);
	}

	if ((vui_p->nal_hrd_parameters_present_flag == 1) || (vui_p->vcl_hrd_parameters_present_flag == 1)) {
		len += u_1(vui_p->low_delay_hrd_flag,                      stream_p);
	}

	len += u_1(vui_p->pic_struct_present_flag,                     stream_p);
	len += u_1(vui_p->bitstream_restriction_flag,                  stream_p);
	if (vui_p->bitstream_restriction_flag == 1) {
		len += u_1(vui_p->motion_vectors_over_pic_boundaries_flag, stream_p);
		len += ue_v(vui_p->max_bytes_per_pic_denom,                 stream_p);
		len += ue_v(vui_p->max_bits_per_mb_denom,                   stream_p);
		len += ue_v(vui_p->log2_max_mv_length_horizontal,           stream_p);
		len += ue_v(vui_p->log2_max_mv_length_vertical,             stream_p);
		len += ue_v(vui_p->num_reorder_frames,                      stream_p);
		len += ue_v(vui_p->max_dec_frame_buffering,                 stream_p);
	}
	return len; /* bits */
}

static NALU_t *GeneratePicParamSet_NALU(seq_parameter_set_rbsp_t *sps_p, pic_parameter_set_rbsp_t *pps_p,
                                        NALU_t *nalu_p)
{
	uint8_t rbsp_p[NVCL_SCRATCH_BUFFER_SIZE];
	int32_t rbspLen;
	int32_t naluLen;
	rbspLen = naluLen = 0;

	if (nalu_p == 0) {
		dev_err(pDriverData->dev, "Error: invalid nalu pointer\n");
		return 0;
	}
	// Init nalu structure
	nalu_p->maxLen  = NVCL_SCRATCH_BUFFER_SIZE;

	rbspLen = GeneratePicParamSet_rbsp(sps_p, pps_p, rbsp_p);
	if (rbspLen == -1) {
		dev_err(pDriverData->dev, "Error: rbsp generation of picture parameter set failed\n");
		return 0;
	}
	naluLen = RBSPtoNALU(rbsp_p, nalu_p, rbspLen, NALU_TYPE_PPS, NALU_PRIORITY_HIGHEST, 0, 1);
	if (naluLen == -1) {
		dev_err(pDriverData->dev, "Error: RBSPtoNALU failed\n");
		return 0;
	}
	nalu_p->len = naluLen;

	return nalu_p;
}

static uint32_t GeneratePicParamSet_rbsp(seq_parameter_set_rbsp_t *sps_p, pic_parameter_set_rbsp_t *pps_p,
                                         uint8_t *rbsp_p)
{
	bitstream_t stream;
	bitstream_t *stream_p = &stream;
	uint32_t len;
	uint32_t lenInBytes;
	int32_t i;
	uint32_t value;
	uint32_t cnt;

	len = 0;

	stream_p->streamBuf_p = (uint8_t *)rbsp_p;
	stream_p->bits_to_go = 8;
	stream_p->byte_pos = 0;
	len += ue_v(pps_p->pic_parameter_set_id,                   stream_p);
	len += ue_v(pps_p->seq_parameter_set_id,                   stream_p);
	len += u_1(pps_p->entropy_coding_mode_flag,               stream_p);
	len += u_1(pps_p->pic_order_present_flag,                stream_p);
	len += ue_v(pps_p->num_slice_groups_minus1,                stream_p);
	if (pps_p->num_slice_groups_minus1 > 0) {
		len += ue_v(pps_p->slice_group_map_type,               stream_p);
		if (pps_p->slice_group_map_type == 0) {
			for (i = 0; i < (pps_p->num_slice_groups_minus1 + 1); i++) {
				len += ue_v((int32_t)pps_p->run_length_minus1,          stream_p);
			}
		} else if (pps_p->slice_group_map_type == 2) {
			for (i = 0; i < (pps_p->num_slice_groups_minus1 + 1); i++) {
				len += ue_v((int32_t)pps_p->top_left,                   stream_p);
				len += ue_v((int32_t)pps_p->bottom_right,               stream_p);
			}
		} else if (pps_p->slice_group_map_type == 3 ||
		           pps_p->slice_group_map_type == 4 ||
		           pps_p->slice_group_map_type == 5) {
			len += u_1(pps_p->slice_group_change_direction_flag, stream_p);
			len += ue_v((int32_t)pps_p->slice_group_change_rate_minus1,    stream_p);
		} else if (pps_p->slice_group_map_type == 6) {
			len += ue_v((int32_t)pps_p->pic_size_in_map_units_minus1,      stream_p);
			value = pps_p->num_slice_groups_minus1 + 1;
			cnt = 0;
			while (value > 0) {
				value >>= 1;
				cnt ++;
			}
			for (i = 0; i < (pps_p->pic_size_in_map_units_minus1 + 1); i++) {
				len += u_v(cnt, pps_p->slice_group_id[i],          stream_p);
			}
		}
	}

	len += ue_v(pps_p->num_ref_idx_10_active_minus1,           stream_p);
	len += ue_v(pps_p->num_ref_idx_11_active_minus1,           stream_p);

	len += u_1(pps_p->weighted_pred_flag,                     stream_p);       // weighted pred flag
	len += u_v(2, pps_p->weighted_bipred_idc,                 stream_p);       // weighted bipred flag
	len += se_v(pps_p->pic_init_qp_minus26,                    stream_p);
	len += se_v(pps_p->pic_init_qs_minus26,                    stream_p);
	len += se_v(pps_p->chroma_qp_index_offset,                 stream_p);

	len += u_1(pps_p->deblocking_filter_control_present_flag, stream_p);
	len += u_1(pps_p->contrained_intra_pred_flag,            stream_p);
	len += u_1(pps_p->redundant_pic_cnt_present_flag,         stream_p);
	if (sps_p->profile_idc == SPS_PROFILE_IDC_HIGH ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_10 ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_422 ||
	    sps_p->profile_idc == SPS_PROFILE_IDC_HIGH_444) {
		len += u_1(pps_p->transform_8x8_mode_flag,             stream_p);
		len += u_1(pps_p->pic_scaling_matrix_present_flag,     stream_p);
		len += se_v(pps_p->second_chroma_qp_index_offset,       stream_p);
	}
	SODBtoRBSP(stream_p);  /* copies the last couple of bits into the byte buffer */
	lenInBytes = stream_p->byte_pos;

	return lenInBytes;
}

uint32_t H264HwEncodeEstimateSEISize(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p)
{
	uint32_t size; /* bits */
	uint32_t totalSize;
	uint16_t len, info;

	totalSize = 0;
	if (nVCLContext_p->seiBufPeriodPresentFlag &&
	    (nVCLContext_p->seiMsg.forceBP || frameParams_p->idrFlag == 1 || frameParams_p->firstPictureInSequence == 1)) {
		len = 0;
		ue_linfo(nVCLContext_p->seq_parameter_set_id, &len, &info);
		size = len;
		/* nal_hrd_bp_present_flag */
		size += nVCLContext_p->initial_cpb_removal_delay_length;
		size += nVCLContext_p->initial_cpb_removal_delay_length; /* offset */
		/* vcl_hrd_bp_present_flag */
		size += nVCLContext_p->initial_cpb_removal_delay_length;
		size += nVCLContext_p->initial_cpb_removal_delay_length; /* offset */

		if (size % 8 != 0) { size += (8 - (size % 8)); }
		totalSize += size;

		totalSize += 8; /* payload type */
		totalSize += ((size >> (3 + 8)) + 1) << 3; /* 3 - bit2byte 8-0xff representation */
	}

	dev_dbg(pDriverData->dev, "seiBufPeriod = %u bytes\n", totalSize / 8);

	if (nVCLContext_p->seiPicTimingPresentFlag) {
		size = 0;
		size += nVCLContext_p->cpb_removal_delay_length;
		size += nVCLContext_p->dpb_output_delay_length; /* offset */
		if (nVCLContext_p->picStructPresentFlag) {
			size += 4;        /* SEI_PIC_STRUCT_FRAME */
		}

		if (size % 8 != 0) { size += (8 - (size % 8)); }
		totalSize += size;

		totalSize += 8; /* payload type */
		totalSize += ((size >> (3 + 8)) + 1) << 3; /* 3 - bit2byte 8-0xff representation */
	}

	dev_dbg(pDriverData->dev, "seiPicTiming = %u bytes\n", totalSize / 8);

	/* NALU overhead */
	/* 4 - start code */
	/* 1 - nal header */
	/* 1 - nal trailer  SODBtoRBSP */
	totalSize += ((1 + 1 + 4) << 3);

	dev_dbg(pDriverData->dev, "NALU size = %u bytes\n", totalSize / 8);

	return totalSize;
}

static NALU_t *GenerateSEI_NALU(sei_message_t *sei_p, NALU_t *nalu_p)
{
	uint8_t rbsp_p[NVCL_SCRATCH_BUFFER_SIZE];
	int32_t rbspLen;
	int32_t naluLen;
	rbspLen = naluLen = 0;

	if (nalu_p == 0) {
		dev_err(pDriverData->dev, "Error: invalid nalu pointer\n");
		return 0;
	}
	// Init nalu structure
	nalu_p->maxLen  = NVCL_SCRATCH_BUFFER_SIZE;

	rbspLen = GenerateSEI_rbsp(sei_p, rbsp_p);
	if (rbspLen == -1) {
		dev_err(pDriverData->dev, "Error: rbsp generation of sei failed\n");
		return 0;
	}
	naluLen = RBSPtoNALU(rbsp_p, nalu_p, rbspLen, NALU_TYPE_SEI, NALU_PRIORITY_DISPOSABLE, 0, 1);
	if (naluLen == -1) {
		dev_err(pDriverData->dev, "Error: RBSPtoNALU failed\n");
		return 0;
	}
	nalu_p->len = naluLen;

	return nalu_p;
}

static uint32_t GenerateSEI_rbsp(sei_message_t *sei_p, uint8_t *rbsp_p)
{
	bitstream_t stream;
	bitstream_t *stream_p = &stream;
	uint32_t lenInBytes;
	uint8_t payload[256]; /* maximum size of payload */
	sei_type type;
	uint32_t len;

	lenInBytes = 0; len = 0;

	stream_p->streamBuf_p = (uint8_t *)rbsp_p;
	stream_p->bits_to_go = 8;
	stream_p->byte_pos = 0;
	if (sei_p->sei_type & (1 << SEI_BUFFERING_PERIOD)) {
		stream_p->streamBuf_p = payload;
		stream_p->bits_to_go = 8;
		stream_p->byte_pos = 0;
		type = SEI_BUFFERING_PERIOD;
		len = GenerateSEI_message(sei_p, (rbsp_p + lenInBytes), stream_p, type);
		lenInBytes += (len >> 3);
	}

	dev_dbg(pDriverData->dev, "seiBufPeriod = %d / %d (bytes)\n", len, len / 8);

	if (sei_p->sei_type & (1 << SEI_PICTURE_TIMING)) {
		stream_p->streamBuf_p = payload;
		stream_p->bits_to_go = 8;
		stream_p->byte_pos = 0;
		type = SEI_PICTURE_TIMING;
		len = GenerateSEI_message(sei_p, (rbsp_p + lenInBytes), stream_p, type);
		lenInBytes += (len >> 3);
	}

	dev_dbg(pDriverData->dev, "seiPicTiming = %d / %d (bytes)\n", len, len / 8);

#if 1
	stream_p->streamBuf_p = (uint8_t *)rbsp_p;
	stream_p->bits_to_go = 8;
	stream_p->byte_pos = lenInBytes;
	SODBtoRBSP(stream_p);      /* copies the last couple of bits into the byte buffer */
	lenInBytes = stream_p->byte_pos;
#else
	*(rbsp_p + lenInBytes) = 0x80;
	lenInBytes++;
#endif

	return lenInBytes;
}

static uint32_t GenerateSEI_message(sei_message_t *sei_p, uint8_t *rbsp_p, bitstream_t *stream_p, sei_type type)
{
	uint8_t  i;
	uint32_t len;
	uint32_t len2;
	uint32_t offset;

	offset = 0; /* byte pointer */
	len = GenerateSEI_payload(sei_p, stream_p, type);
	len2 = (len >> 3);

	while (type > 255) {
		rbsp_p[offset++] = 0xFF;
		type = (sei_type)(type - 255);
	}
	rbsp_p[offset++] = type;
	while (len2 > 255) {
		rbsp_p[offset++] = 0xFF;
		len2 = (len2 - 255);
	}
	rbsp_p[offset++] = len2;
	for (i = 0; i < len; i++) {
		*(rbsp_p + offset + i) = *(stream_p->streamBuf_p + i);
	}

	len += (offset * 8);
	stream_p->byte_pos += offset;
	return len;
}

static uint32_t GenerateSEI_payload(sei_message_t *sei_p, bitstream_t *stream_p, sei_type type)
{

	uint32_t len;
	int32_t i;
	sei_buffering_period_t *buf_period_p;
	sei_pic_timing_t *pic_timing_p;

	len = 0;

	if (type == SEI_BUFFERING_PERIOD) {
		buf_period_p = &sei_p->buffering_period;

		len += ue_v(buf_period_p->seq_parameter_set_id, stream_p);

		len += u_v(sei_p->initial_cpb_removal_delay_length, buf_period_p->nal_initial_cbp_removal_delay, stream_p);
		len += u_v(sei_p->initial_cpb_removal_delay_length, buf_period_p->nal_initial_cbp_removal_delay_offset, stream_p);

		len += u_v(sei_p->initial_cpb_removal_delay_length, buf_period_p->vcl_initial_cbp_removal_delay, stream_p);
		len += u_v(sei_p->initial_cpb_removal_delay_length, buf_period_p->vcl_initial_cbp_removal_delay_offset, stream_p);

		len += byteAlign(stream_p);

	}

	if (type == SEI_PICTURE_TIMING) {
		pic_timing_p = &sei_p->pic_timing;

		len += u_v(sei_p->cpb_removal_delay_length, pic_timing_p->cpb_removal_delay, stream_p);
		len += u_v(sei_p->dpb_output_delay_length, pic_timing_p->dpb_output_delay, stream_p);

		if (sei_p->picStructPresentFlag) {
			len += u_v(4, pic_timing_p->pic_struct,                                      stream_p);

			for (i = 0; i < kPicStructNumTs[pic_timing_p->pic_struct]; i++) {
				len += u_1(pic_timing_p->clock_timestamp_flag[i],                        stream_p);
				if (pic_timing_p->clock_timestamp_flag[i] == 1) {
					len += u_v(2, pic_timing_p->ct_type[i],                               stream_p);
					len += u_1(pic_timing_p->nuit_field_based_flag[i],                   stream_p);
					len += u_v(5, pic_timing_p->counting_type[i],                         stream_p);
					len += u_1(pic_timing_p->full_timestamp_flag[i],                     stream_p);
					len += u_1(pic_timing_p->discontinuity_flag[i],                      stream_p);
					len += u_1(pic_timing_p->cnt_dropped_flag[i],                        stream_p);
					len += u_v(8, pic_timing_p->n_frames[i],                              stream_p);
					if (pic_timing_p->full_timestamp_flag[i] == 1) {
						len += u_v(6, pic_timing_p->seconds_value[i],                     stream_p);
						len += u_v(6, pic_timing_p->minutes_value[i],                     stream_p);
						len += u_v(5, pic_timing_p->hours_value[i],                       stream_p);
					} else {
						if (pic_timing_p->seconds_flag[i] == 1) {
							len += u_v(6, pic_timing_p->seconds_value[i],                 stream_p);
							if (pic_timing_p->minutes_flag[i] == 1) {
								len += u_v(6, pic_timing_p->minutes_value[i],             stream_p);
								if (pic_timing_p->hours_flag[i] == 1) {
									len += u_v(5, pic_timing_p->hours_value[i],           stream_p);
								}
							}
						}
					}
					if (sei_p->time_offset_length > 0) {
						len += u_v(sei_p->time_offset_length, pic_timing_p->time_offset[i],  stream_p);
					}
				}
			}
		}
		len += byteAlign(stream_p);
	}

	return len;
}

uint32_t H264HwEncodeInitParamsNAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                                   bool newSequenceFlag)
{
	if (newSequenceFlag == true) {
		memset(nVCLContext_p, 0, sizeof(H264EncodeNVCLContext_t));

		InitSeqParamSet(&nVCLContext_p->seqParams);
		InitPicParamSet(&nVCLContext_p->picParams);
		InitSEIParam(&nVCLContext_p->seiMsg);

		nVCLContext_p->seq_parameter_set_id = 0;
		nVCLContext_p->pic_parameter_set_id = PIC_PARAMETER_SET_ID;
		nVCLContext_p->initial_cpb_removal_delay_length = INITIAL_CPB_REMOVAL_DELAY_LENGTH;
		nVCLContext_p->cpb_removal_delay_length   = CPB_REMOVAL_DELAY_LENGTH;
		nVCLContext_p->dpb_output_delay_length    = DPB_OUTPUT_DELAY_LENGTH;
		nVCLContext_p->time_offset_length         = TIME_OFFSET_LENGTH;
	}
	/* initialization of access unit assuming this function is called for every primary picture */
	nVCLContext_p->nvclNALUSize = 0;
	if ((seqParams_p->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_BASELINE) ||
	    (seqParams_p->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_EXTENDED)) {
		nVCLContext_p->pic_parameter_set_id = HVA_ENCODE_CAVLC;
		nVCLContext_p->entropyCodingMode = HVA_ENCODE_CAVLC;
	} else {
		nVCLContext_p->pic_parameter_set_id = HVA_ENCODE_CABAC;
		nVCLContext_p->entropyCodingMode = HVA_ENCODE_CABAC;
	}
	nVCLContext_p->chromaQpIndexOffset = 2;
	return 0;
}

/*!
 ************************************************************************
 * \brief
 *        Create access unit Nal
 * \params
 *        EncoderHandle
 *        ap_bitstream: user space address of the output buffer
 * \return
 *        the size of AUD NAL in bits
 ************************************************************************
 */
uint32_t H264HwEncodeCreateAccessUnitDelimiterNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                                  H264EncodeHardFrameParams_t *frameParams_p, uint8_t *streamBuf_p)
{
	NALU_t nalu;
	NALU_t *nalu_p = &nalu;
	uint8_t nalu_buf[NVCL_SCRATCH_BUFFER_SIZE];
	uint8_t  start[4] = {0x00, 0x00, 0x00, 0x01};
	uint32_t offset = 0;

	/* Generate Access Unit Delimiter */
	nalu_p->buf_p = nalu_buf;
	nalu_p = GenerateAccessUnitDelimiter_NALU(frameParams_p, nalu_p);
	if (nalu_p == 0) {
		return 0;
	}

	memcpy(streamBuf_p + offset, start, sizeof(start));
	offset += sizeof(start);

	memcpy(streamBuf_p + offset, nalu_p->buf_p, nalu_p->len);
	offset += nalu_p->len;

	nVCLContext_p->nvclNALUSize += offset;

	dev_dbg(pDriverData->dev, "Access unit delimiter size is %u bytes\n", offset);

	return offset * 8;
}

uint32_t H264HwEncodeCreateSeqParamSetNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                          H264EncodeHardSequenceParams_t *seqParams_p, uint8_t *streamBuf_p)
{
	NALU_t nalu;
	NALU_t *nalu_p = &nalu;
	uint8_t nalu_buf[NVCL_SCRATCH_BUFFER_SIZE];
	uint8_t start[4] = {0x00, 0x00, 0x00, 0x01};
	uint32_t offset = 0;

	/* Update frame parameters */
	UpdateSeqParamSet(nVCLContext_p, seqParams_p, &nVCLContext_p->seqParams);

	/* Generate SPS NALU */
	nalu_p->buf_p = nalu_buf;
	nalu_p = GenerateSeqParamSet_NALU(&nVCLContext_p->seqParams, nalu_p);
	if (nalu_p == 0) {
		return 0;
	}

	memcpy(streamBuf_p + offset, start, sizeof(start));
	offset += sizeof(start);

	memcpy(streamBuf_p + offset, nalu_p->buf_p, nalu_p->len);
	offset += nalu_p->len;

	nVCLContext_p->nvclNALUSize += offset;

	dev_dbg(pDriverData->dev, "Sequence parameter set size is %u bytes\n", offset);

	return offset * 8;
}

uint32_t H264HwEncodeCreatePicParamSetNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                          H264EncodeHardFrameParams_t *frameParams_p, uint8_t *streamBuf_p)
{
	NALU_t nalu;
	NALU_t *nalu_p = &nalu;
	uint8_t nalu_buf[NVCL_SCRATCH_BUFFER_SIZE];
	uint8_t  start[4] = {0x00, 0x00, 0x00, 0x01};
	uint32_t offset = 0;

	/* Update frame parameters */
	UpdatePicParamSet(nVCLContext_p, frameParams_p, &nVCLContext_p->picParams);

	/* Generate PPS NALU */
	nalu_p->buf_p = nalu_buf;
	nalu_p = GeneratePicParamSet_NALU(&nVCLContext_p->seqParams, &nVCLContext_p->picParams, nalu_p);
	if (nalu_p == 0) {
		return 0;
	}

	memcpy(streamBuf_p + offset, start, sizeof(start));
	offset += sizeof(start);

	memcpy(streamBuf_p + offset, nalu_p->buf_p, nalu_p->len);
	offset += nalu_p->len;

	nVCLContext_p->nvclNALUSize += offset;

	dev_dbg(pDriverData->dev, "Picture parameter set size is %u bytes\n", offset);

	return offset * 8;
}

uint32_t H264HwEncodeCreateSEINAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                  H264EncodeHardParamOut_t *outParams_p, uint8_t *streamBuf_p)
{
	NALU_t nalu;
	NALU_t *nalu_p = &nalu;
	uint8_t nalu_buf[NVCL_SCRATCH_BUFFER_SIZE];
	uint8_t  start[4] = {0x00, 0x00, 0x00, 0x01};
	uint32_t offset = 0;

	/* Update frame parameters */
	UpdateSEIParam(nVCLContext_p, frameParams_p, outParams_p, &nVCLContext_p->seiMsg);

	/* Generate SEI NALU */
	nalu_p->buf_p = nalu_buf;
	nalu_p = GenerateSEI_NALU(&nVCLContext_p->seiMsg, nalu_p);
	if (nalu_p == 0) {
		return 0;
	}

	memcpy(streamBuf_p + offset, start, sizeof(start));
	offset += sizeof(start);

	memcpy(streamBuf_p + offset, nalu_p->buf_p, nalu_p->len);
	offset += nalu_p->len;

	nVCLContext_p->nvclNALUSize += offset;

	/* Update frame parameters */
	UpdateSEIParam_post(nVCLContext_p, frameParams_p, outParams_p, &nVCLContext_p->seiMsg);

	dev_dbg(pDriverData->dev, "Sei size is %u bytes\n", offset);

	return offset * 8;
}

uint32_t H264HwEncodeCreateFillerNAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardParamOut_t *outParams_p,
                                     uint8_t *streamBuf_p)
{
	uint8_t  start[] = {0x00, 0x00, 0x00, 0x01};
	uint32_t offset = 0;
	/* update stuffing bits */
	nVCLContext_p->fil.stuffingBits = outParams_p->stuffingBits;

	memcpy(streamBuf_p + offset, start, sizeof(start));
	offset += sizeof(start);

	streamBuf_p[offset] = 0x0c;
	offset += 1;

	memset(streamBuf_p + offset, 0xff, outParams_p->stuffingBits / 8);
	offset += outParams_p->stuffingBits / 8;

	streamBuf_p[offset] = 0x80;
	offset += 1;

	nVCLContext_p->nvclNALUSize += offset;

	dev_dbg(pDriverData->dev, "Filler data size is %u bytes\n", offset);
	dev_dbg(pDriverData->dev, "Hva stuffing bits is %u bytes\n", outParams_p->stuffingBits >> 3);

	return offset * 8;
}

// Flexible slice header

/* Private Function Prototypes. */
static uint32_t add_nalu_annexb_start_code(bitstream_t *bitstream);
static uint32_t add_nalu_header(bitstream_t *bitstream, H264EncodeHardFrameParams_t *frameParams_p);
static uint32_t add_slice_header(bitstream_t *bitstream, uint32_t *slice_qp_delta_offset,
                                 H264EncodeHardFrameParams_t *frameParams_p, H264EncodeNVCLContext_t *nVCLContext_p);
static void     add_slice_header_byte_align(bitstream_t *bitstream);
static void     print_flexible_slice_header_struct(const FLEXIBLE_SLICE_HEADER *flexible_slice_header);

/*!
 *******************************************************************************
 * \brief
 *    Management of nal unit ANNEXB start code (bytes)
 * \input
 *    bitstream Storage for bitstream holding NALU.
 * \return
 *    Bit size added in bitstream.
 *******************************************************************************
 */
static uint32_t add_nalu_annexb_start_code(bitstream_t *bitstream)
{
	/* NALU strat code. */
	int32_t nalu_startcodeprefix_len; /*! 4 for parameter sets and first slice in picture, 3 for everything else (suggested) */

	/* Add annexb if needed. */
	nalu_startcodeprefix_len = 2 + ZEROBYTES_SHORTSTARTCODE;
	if (nalu_startcodeprefix_len > 3) {
		bitstream->streamBuf_p[bitstream->byte_pos++] = 0;
	}

	bitstream->streamBuf_p[bitstream->byte_pos++] = 0;
	bitstream->streamBuf_p[bitstream->byte_pos++] = 0;
	bitstream->streamBuf_p[bitstream->byte_pos++] = 1;

	return (bitstream->byte_pos * 8);
}

/*!
 *******************************************************************************
 * \brief
 *    Write a NALU header
 *
 * \return
 *    Number of bits used
 *******************************************************************************
*/
static uint32_t add_nalu_header(bitstream_t *bitstream, H264EncodeHardFrameParams_t *frameParams_p)
{
	uint8_t  NAL_first_byte = 0;
	int32_t nalu_type;          /*! NALU_TYPE_xxxx */
	int32_t nalu_reference_idc; /*! NALU_PRIORITY_xxxx */
	int32_t nalu_forbidden_bit; /*! should be always FALSE */

	/* RR 1-Manage first byte of NALU */
	if (frameParams_p->idrFlag) {
		nalu_type          = NALU_TYPE_IDR;
		nalu_reference_idc = NALU_PRIORITY_HIGHEST;
	} else {
		nalu_type          = NALU_TYPE_SLICE;
		nalu_reference_idc = NALU_PRIORITY_HIGH;
	}

	nalu_forbidden_bit = 0;
	NAL_first_byte     = nalu_forbidden_bit << 7 | nalu_reference_idc << 5 | nalu_type;

	/* Create stream */
	bitstream->streamBuf_p[bitstream->byte_pos++] = NAL_first_byte;

	return (8);
}

/*!
 *******************************************************************************
 * \brief
 *    Write slice header including all applicable fields after first_mb_in_slice
 *    and compute bit offset for slice_qp_delta and num_mbs_in_slice_minus1.
 *
 * \return
 *    Number of bits written.
 *******************************************************************************
*/
static uint32_t add_slice_header(bitstream_t *bitstream, uint32_t *slice_qp_delta_offset,
                                 H264EncodeHardFrameParams_t *frameParams_p, H264EncodeNVCLContext_t *nVCLContext_p)
{
	uint32_t len                               = 0;

	len += ue_v((frameParams_p->pictureCodingType == HVA_ENCODE_I_FRAME ? 7 : 5), bitstream);
	len += ue_v((PIC_PARAMETER_SET_ID + (nVCLContext_p->entropyCodingMode)), bitstream);
	len += u_v(frameParams_p->log2MaxFrameNumMinus4 + 4, frameParams_p->frameNum, bitstream);

	if (frameParams_p->idrFlag) {
		len += ue_v(frameParams_p->idrPicId, bitstream);
	}

	if (frameParams_p->pictureCodingType == HVA_ENCODE_P_FRAME) {
		len += u_1(0, bitstream); /* FP: the override_flag is forced to zero with single ref. */
	}

	if (frameParams_p->pictureCodingType != HVA_ENCODE_I_FRAME) {
		len += u_1(0, bitstream);
	}

	/* NZ: dec_ref_pic_marking */
	if (frameParams_p->idrFlag) {
		len += u_1(0, bitstream); /* FP: no_output_of_prior_pics_flag forced to zero */
		len += u_1(0, bitstream);
	} else {
		len += u_1(0, bitstream); /* adaptive_ref_pic_marking_mode_flag = 0 */
	}

	if ((frameParams_p->pictureCodingType != HVA_ENCODE_I_FRAME) &&
	    (nVCLContext_p->entropyCodingMode == HVA_ENCODE_CABAC)) {
		len += ue_v(0, bitstream);        /* cabac_init_idc = 0*/
	}

	/* Set bit offset to slice_qp_delta. */
	*slice_qp_delta_offset += len;

	len += ue_v(frameParams_p->disableDeblockingFilterIdc, bitstream); /* Turn loop filter on/off on slice basis  */

	if (frameParams_p->disableDeblockingFilterIdc != 1) {
		len += se_v(frameParams_p->sliceAlphaC0OffsetDiv2, bitstream);
		len += se_v(frameParams_p->sliceBetaOffsetDiv2, bitstream);
	}

	return len;
}

/*!
 ************************************************************************
 * \brief
 *    If needed, bytes aligns the flexible slice header data. In fact, bits
 *    in the temporary bitstream write buffer are written to the bitstream
 *    after being left shifted as required.
 * \param currStream
 *        bitstream_t which contains data bits.
 * \return None
 *
 ************************************************************************
 */
static void add_slice_header_byte_align(bitstream_t *bitstream)
{

	/* Write remaining bits in the bitstream after left shifting them. */
	if (bitstream->bits_to_go < 8) {
		bitstream->byte_buf <<= bitstream->bits_to_go;
		bitstream->streamBuf_p[bitstream->byte_pos++] = (uint8_t) bitstream->byte_buf;
		bitstream->bits_to_go = 8;
		bitstream->byte_buf = 0;
	}
}

/*!
 ************************************************************************
 * \brief
 *    Prints content of flexible slice header data structure to stdout.
 *
 ************************************************************************
 */
static void print_flexible_slice_header_struct(const FLEXIBLE_SLICE_HEADER *flexible_slice_header)
{
	uint32_t i, j;

	dev_dbg(pDriverData->dev, "[Flexible Slice Header Contents]\n");
	dev_dbg(pDriverData->dev, "|-frame_num                = %3u [0x%08X]\n", flexible_slice_header->frame_num,
	        flexible_slice_header->frame_num);
	dev_dbg(pDriverData->dev, "|-header_bitsize           = %3u [0x%08X]\n", flexible_slice_header->header_bitsize,
	        flexible_slice_header->header_bitsize);
	dev_dbg(pDriverData->dev, "|-offset0_bitsize          = %3u [0x%08X]\n", flexible_slice_header->offset0_bitsize,
	        flexible_slice_header->offset0_bitsize);
	dev_dbg(pDriverData->dev, "|-offset1_bitsize          = %3u [0x%08X]\n", flexible_slice_header->offset1_bitsize,
	        flexible_slice_header->offset1_bitsize);

	for (i = 0; i < FLEXIBLE_SLICE_HEADER_MAX_BYTESIZE * 8; i += 16) {
		dev_dbg(pDriverData->dev, "|-slice_header_data[%2u-%2u] = [", i, i + 15);
		for (j = 0; j < 16; j++) {
			if (j == 15) {
				dev_dbg(pDriverData->dev, "0x%02X", flexible_slice_header->buffer[i + j]);
			} else {
				dev_dbg(pDriverData->dev, "0x%02X ", flexible_slice_header->buffer[i + j]);
			}
		}
		dev_dbg(pDriverData->dev, "]\n");
	}

	dev_dbg(pDriverData->dev, "\n");
}

/*!
 *******************************************************************************
 * \brief
 *    Fill flexible slice header structure.
 *
 * \param
 *     flexible_slice_header pointer to flexible slice header to be filled
 *     frame_num             Encoded frame number
 * \return
 *    slice header size
 *******************************************************************************
*/
uint32_t H264HwEncodeFillFlexibleSliceHeader(FLEXIBLE_SLICE_HEADER *flexible_slice_header,
                                             H264EncodeHardFrameParams_t *frameParams_p, H264EncodeNVCLContext_t *nVCLContext_p)
{
	bitstream_t bitstream = {0, 8, 0, flexible_slice_header->buffer}; /* Placeholder for flexible slice header fields. */

	dev_dbg(pDriverData->dev, "flexible_slice_header = %p\n", flexible_slice_header);
	dev_dbg(pDriverData->dev, "frameParams_p = %p\n", frameParams_p);
	dev_dbg(pDriverData->dev, "nVCLContext_p = %p\n", nVCLContext_p);
	dev_dbg(pDriverData->dev, "buffer = %p\n", flexible_slice_header->buffer);

	dev_dbg(pDriverData->dev, "idrFlag = %u\n", frameParams_p->idrFlag);
	dev_dbg(pDriverData->dev, "idrPicId = %u\n", frameParams_p->idrPicId);
	dev_dbg(pDriverData->dev, "pictureCodingType = %u (%s)\n", frameParams_p->pictureCodingType,
	        StringifyPictureCodingType(frameParams_p->pictureCodingType));
	dev_dbg(pDriverData->dev, "disableDeblockingFilterIdc = %u (%s)\n", frameParams_p->disableDeblockingFilterIdc,
	        StringifyDeblocking(frameParams_p->disableDeblockingFilterIdc));
	dev_dbg(pDriverData->dev, "sliceAlphaC0OffsetDiv2 = %u\n", frameParams_p->sliceAlphaC0OffsetDiv2);
	dev_dbg(pDriverData->dev, "sliceBetaOffsetDiv2 = %u\n", frameParams_p->sliceBetaOffsetDiv2);

	dev_dbg(pDriverData->dev, "entropyCodingMode = %u (%s)\n", nVCLContext_p->entropyCodingMode,
	        StringifyEntropyCodingMode(nVCLContext_p->entropyCodingMode));

	flexible_slice_header->header_bitsize = 0;

	/* Write VCL NALU start code and VCL NALU header and update number of bits written to flexible slice header. */
	flexible_slice_header->header_bitsize += add_nalu_annexb_start_code(&bitstream);
	dev_dbg(pDriverData->dev, "add_nalu_annexb_start_code: header_bitsize = %u\n", flexible_slice_header->header_bitsize);

	flexible_slice_header->header_bitsize += add_nalu_header(&bitstream, frameParams_p);
	dev_dbg(pDriverData->dev, "add_nalu_header: header_bitsize = %u\n", flexible_slice_header->header_bitsize);

	/* Set sh_offset0 in bits. Insertion point for first_mb_in_slice. */
	flexible_slice_header->offset0_bitsize = flexible_slice_header->header_bitsize;

	/* Write slice header, update number of bits accumulated and compute bit offset to slice_qp_delta and num_mbs_in_slice_minus1. */
	flexible_slice_header->header_bitsize += add_slice_header(&bitstream, &flexible_slice_header->offset1_bitsize,
	                                                          frameParams_p, nVCLContext_p);
	dev_dbg(pDriverData->dev, "header_bitsize = %u\n", flexible_slice_header->header_bitsize);
	dev_dbg(pDriverData->dev, "offset0_bitsize = %u\n", flexible_slice_header->offset0_bitsize);
	dev_dbg(pDriverData->dev, "offset1_bitsize = %u\n", flexible_slice_header->offset1_bitsize);

	/* Check whether bits remain to be written to the bitstream. */
	add_slice_header_byte_align(&bitstream);

	print_flexible_slice_header_struct(flexible_slice_header);

	return flexible_slice_header->header_bitsize;
}
