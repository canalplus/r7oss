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

#include "st_relayfs_se.h"
#include "player_threads.h"

#include "ring_generic.h"
#include "havana_stream.h"
#include "hevc.h"
#include "codec_mme_video_hevc.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoHevc_c"

#define BUFFER_HEVC_CODEC_STREAM_PARAMETER_CONTEXT             "HevcCodecStreamParameterContext"
#define BUFFER_HEVC_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_HEVC_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, false, false, NOT_SPECIFIED}

static BufferDataDescriptor_t            HevcCodecStreamParameterContextDescriptor = BUFFER_HEVC_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;
// --------

typedef struct HevcCodecExtraCRCInfo_s
{
    // Parser info
    uint32_t DecodeIndex;
    uint32_t IDR;
    uint32_t poc;
    // Buffer cached addresses
    uint8_t *omega_luma;
    uint8_t *omega_chroma;
    uint32_t omega_luma_size;
    uint8_t *ppb;
    uint32_t ppb_size;
    uint8_t *raster_luma;
    uint8_t *raster_chroma;
    uint32_t raster_stride;
    uint32_t raster_width;
    uint32_t raster_height;
    uint8_t *raster_decimated_luma;
    uint8_t *raster_decimated_chroma;
    uint32_t raster_decimated_stride;
    uint32_t raster_decimated_width;
    uint32_t raster_decimated_height;
} HevcCodecExtraCRCInfo_t;

typedef struct HevcCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    hevcdecpix_transform_param_t        DecodeParameters;
    hevcdecpix_status_t                 DecodeStatus;

    HevcCodecExtraCRCInfo_t     ExtraCRCInfo;
} HevcCodecDecodeContext_t;

#define BUFFER_HEVC_CODEC_DECODE_CONTEXT       "HevcCodecDecodeContext"
#define BUFFER_HEVC_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_HEVC_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(HevcCodecDecodeContext_t)}

static BufferDataDescriptor_t            HevcCodecDecodeContextDescriptor = BUFFER_HEVC_CODEC_DECODE_CONTEXT_TYPE;

// --------

// HEVC Intermediate Buffers
// Allocated in heap-style memory pools
// Note: - alignment of 8 to be checked with Hades designer
//       - min allocation size of 1024 to be fine tuned to avoid fragmentation and not spoil too much space
//       - POOL_SIZE values taken from architecture document "HADES_BW&FOOTPRINT.xlsx"
//       - POOL_SIZE values are doubled to support dual hevc decode.

// Number of frames that must be in PP pools
#define HEVC_NB_OF_FRAME_BUFFERED           6

#define HEVC_SLICE_TABLE_BUFFER             "HevcSliceTblBuf"
#define HEVC_SLICE_TABLE_BUFFER_TYPE        {HEVC_SLICE_TABLE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, (HEVCPP_BUFFER_ALIGNMENT+1), 1024, false, false, 0}

#define HEVC_CTB_TABLE_BUFFER               "HevcCtbTblBuf"
#define HEVC_CTB_TABLE_BUFFER_TYPE          {HEVC_CTB_TABLE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, (HEVCPP_BUFFER_ALIGNMENT+1), 1024, false, false, 0}

#define HEVC_SLICE_HEADERS_BUFFER           "HevcSliceHdrBuf"
#define HEVC_SLICE_HEADERS_BUFFER_TYPE      {HEVC_SLICE_HEADERS_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, (HEVCPP_BUFFER_ALIGNMENT+1), 1024, false, false, 0}

#define HEVC_CTB_COMMANDS_BUFFER            "HevcCtbCmdBuf"
#define HEVC_CTB_COMMANDS_BUFFER_TYPE       {HEVC_CTB_COMMANDS_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, (HEVCPP_BUFFER_ALIGNMENT+1), 1024, false, false, 0}

#define HEVC_CTB_RESIDUALS_BUFFER           "HevcCtbResBuf"
#define HEVC_CTB_RESIDUALS_BUFFER_TYPE      {HEVC_CTB_RESIDUALS_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, (HEVCPP_BUFFER_ALIGNMENT+1), 1024, false, false, 0}
static BufferDataDescriptor_t HevcPreprocessorBufferDescriptor[] =
{
    HEVC_SLICE_TABLE_BUFFER_TYPE,
    HEVC_CTB_TABLE_BUFFER_TYPE,
    HEVC_SLICE_HEADERS_BUFFER_TYPE,
    HEVC_CTB_COMMANDS_BUFFER_TYPE,
    HEVC_CTB_RESIDUALS_BUFFER_TYPE
};

#define HEVC_FORMAT_PIC_BASE_ADDR                                   (0x0)

/*!
* \brief
* Register : SCALING_LIST_PRED
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_SIZE                      (32)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_OFFSET                    (HEVC_FORMAT_PIC_BASE_ADDR + 0x0)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_RESET_VALUE               (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_BITFIELD_MASK             (0x01FF003F)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_RWMASK                    (0x01FF003F)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_ROMASK                    (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_WOMASK                    (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_UNUSED_MASK               (0xFE00FFC0)

/*!
* \brief
* Bit-field : scaling_list_pred_mode_flag
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_mode_flag_OFFSET (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_mode_flag_WIDTH (1)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_mode_flag_MASK (0x00000001)

/*!
* \brief
* Bit-field : scaling_list_pred_matrix_id_delta
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_matrix_id_delta_OFFSET (0x00000001)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_matrix_id_delta_WIDTH (5)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_pred_matrix_id_delta_MASK (0x0000003E)

/*!
* \brief
* Bit-field : scaling_list_dc_coeff_minus8
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_dc_coeff_minus8_OFFSET (0x00000010)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_dc_coeff_minus8_WIDTH (9)
#define HEVC_FORMAT_PIC_SCALING_LIST_PRED_scaling_list_dc_coeff_minus8_MASK (0x01FF0000)

/*!
* \brief
* Register : SCALING_LIST_DELTA_COEF
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_SIZE                (32)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_OFFSET              (HEVC_FORMAT_PIC_BASE_ADDR + 0x4)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_RESET_VALUE         (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_BITFIELD_MASK       (0xFFFFFFFF)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_RWMASK              (0xFFFFFFFF)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_ROMASK              (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_WOMASK              (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_UNUSED_MASK         (0x00000000)

/*!
* \brief
* Bit-field : scaling_list_delta_coef0
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef0_OFFSET (0x00000000)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef0_WIDTH (8)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef0_MASK (0x000000FF)

/*!
* \brief
* Bit-field : scaling_list_delta_coef1
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef1_OFFSET (0x00000008)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef1_WIDTH (8)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef1_MASK (0x0000FF00)

/*!
* \brief
* Bit-field : scaling_list_delta_coef2
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef2_OFFSET (0x00000010)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef2_WIDTH (8)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef2_MASK (0x00FF0000)

/*!
* \brief
* Bit-field : scaling_list_delta_coef3
*/

#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef3_OFFSET (0x00000018)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef3_WIDTH (8)
#define HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF_scaling_list_delta_coef3_MASK (0xFF000000)

//----------
// TODO: Taken from bit_fields.h.

#define  MASK(reg,fld)          reg##_##fld##_MASK
#define  OFFSET(reg,fld)        reg##_##fld##_OFFSET
#define  WIDTH(reg,fld)         reg##_##fld##_WIDTH

// Note: these functions are defined as static in a .h file because of constraints in the APEX/TLM platform

static void
inline field_value_set
(uint32_t *reg,
 uint32_t mask,
 uint8_t  offset,
 uint32_t value)
{
    value <<= offset;
    value &= mask;
    mask = ~mask;
    mask &= *reg;
    *reg = mask | value;
}

static uint32_t
inline unsigned_field_value_get
(uint32_t *reg,
 uint32_t mask,
 uint8_t  offset)
{
    mask &= *reg;
    return mask >> offset;
}

static int32_t
inline signed_field_value_get
(uint32_t *reg,
 uint32_t mask,
 uint8_t  offset,
 uint8_t  width)
{
    int32_t value;
    value = unsigned_field_value_get(reg, mask, offset);
    mask = 1 << (width - 1);

    if (value & mask)
    {
        value |= 0xffffffff << width;
    }

    return value;
}

#define  FIELD_SET(cmd,reg,fld,val)  field_value_set(&cmd,MASK(reg,fld),OFFSET(reg,fld),val)
#define  FIELD_INIT(cmd,reg,fld,val) do { cmd=0; FIELD_SET(cmd,reg,fld,val); } while(0)
#define  FIELD_UGET(cmd,reg,fld)     unsigned_field_value_get(&cmd,MASK(reg,fld),OFFSET(reg,fld))
#define  FIELD_SGET(cmd,reg,fld)     signed_field_value_get(&cmd,MASK(reg,fld),OFFSET(reg,fld),WIDTH(reg,fld))

#define  CMD_SET(cmd,reg,fld,val)    field_value_set(cmd+reg,MASK(reg,fld),OFFSET(reg,fld),val)
#define  CMD_INIT(cmd,reg,fld,val)   do { cmd[reg]=0; CMD_SET(cmd,reg,fld,val); } while(0)
#define  CMD_UGET(cmd,reg,fld)       unsigned_field_value_get(cmd+reg,MASK(reg,fld),OFFSET(reg,fld))
#define  CMD_SGET(cmd,reg,fld)       signed_field_value_get(cmd+reg,MASK(reg,fld),OFFSET(reg,fld),WIDTH(reg,fld))


// /////////////////////////////////////////////////////////////////////////
//
// Macros for dumping C structures to STrelay
//
//#define HEVC_CODEC_DUMP_MME

#ifdef HEVC_CODEC_DUMP_MME
#define st_relayfs_write_mme_text(rid, format, args...) st_relayfs_print_se(rid, ST_RELAY_SOURCE_SE, format, ##args)

static const char *indent_first[5] = {"", "\t", "\t,\t", "\t\t,\t", "\t\t\t,\t"};
static const char *indent_next[5] = {"", "\t", "\t\t", "\t\t\t", "\t\t\t\t"};

#define LOG_ELEM(rid,cmd,elem) st_relayfs_write_mme_text(rid,"\t%d /* %s */",cmd->elem,#elem)
#define LOG_NEXT_ELEM(rid,indent,cmd,elem)  do { \
        st_relayfs_write_mme_text(rid,"\n%s,",indent_next[indent]); \
        LOG_ELEM(rid,cmd,elem); \
} while(0)
#define LOG_FIRST_ELEM(rid,indent,cmd,elem)  do { \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent]); \
        LOG_ELEM(rid,cmd,elem); \
} while(0)
#define LOG_TABLE(rid,cmd,tab,size) do { \
        st_relayfs_write_mme_text(rid,"\t{%d",cmd->tab[0]); \
        unsigned int _j; \
        for(_j=1;_j<size;_j++) \
                st_relayfs_write_mme_text(rid,", %d",cmd->tab[_j]); \
        st_relayfs_write_mme_text(rid,"} /* %s */",#tab); \
} while(0)
#define LOG_NEXT_TABLE(rid,indent,cmd,tab,size) do { \
        st_relayfs_write_mme_text(rid,"\n%s,",indent_next[indent]); \
        LOG_TABLE(rid,cmd,tab,size); \
} while(0)

#define LOG_ROTBUF(rid,indent,cmd,name) do { \
        circular_buffer_t *cmd2 = &cmd->name; \
        LOG_ELEM(rid,cmd2,base_addr); \
        LOG_NEXT_ELEM(rid,indent,cmd2,length); \
        LOG_NEXT_ELEM(rid,indent,cmd2,start_offset); \
        LOG_NEXT_ELEM(rid,indent,cmd2,end_offset); \
} while(0)

#define LOG_PICBUF(rid,indent,cmd,name) do { \
        hevcdecpix_decoded_buffer_t *cmd2 = &cmd->name; \
        LOG_ELEM(rid,cmd2,raster2b_not_omega4); \
        LOG_NEXT_ELEM(rid,indent,cmd2,luma_offset); \
        LOG_NEXT_ELEM(rid,indent,cmd2,chroma_offset); \
        LOG_NEXT_ELEM(rid,indent,cmd2,stride); \
} while(0)

#define LOG_REFBUF(rid,indent,cmd,name) do { \
        hevcdecpix_reference_buffer_t *cmd3 = &cmd->name; \
        LOG_ELEM(rid,cmd3,enable_flag); \
        LOG_NEXT_ELEM(rid,indent,cmd3,ppb_offset); \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_PICBUF(rid,indent+1,cmd3,pixels); \
        st_relayfs_write_mme_text(rid,"\n%s} /* pixels */",indent_first[indent+1]); \
} while(0)

#define LOG_DISPBUF(rid,indent,cmd,name) do { \
        hevcdecpix_display_buffer_t *cmd3 = &cmd->name; \
        LOG_ELEM(rid,cmd3,enable_flag); \
        LOG_NEXT_ELEM(rid,indent,cmd3,resize_flag); \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_PICBUF(rid,indent+1,cmd3,pixels); \
        st_relayfs_write_mme_text(rid,"\n%s} /* pixels */",indent_first[indent+1]); \
} while(0)

#define LOG_PICINFO(rid,indent,cmd,name) do { \
        hevcdecpix_pic_info_t *cmd2 = &cmd->name; \
        LOG_ELEM(rid,cmd2,poc); \
        LOG_NEXT_ELEM(rid,indent,cmd2,non_existing_flag); \
        LOG_NEXT_ELEM(rid,indent,cmd2,long_term_flag); \
} while(0)

#define LOG_NEXT_STRUCT(rid,indent,TYPE,cmd,name) do { \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_##TYPE(rid,indent+1,cmd,name); \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],#name); \
} while(0)

#define LOG_INTBUF(rid,indent,cmd,name) do { \
        intermediate_buffer_t *cmd3 = &cmd->name; \
        LOG_ELEM(rid,cmd3,slice_table_base_addr); \
        LOG_NEXT_ELEM(rid,indent,cmd3,slice_table_length); \
        LOG_NEXT_ELEM(rid,indent,cmd3,ctb_table_base_addr); \
        LOG_NEXT_ELEM(rid,indent,cmd3,ctb_table_length); \
        LOG_NEXT_ELEM(rid,indent,cmd3,slice_headers_base_addr); \
        LOG_NEXT_ELEM(rid,indent,cmd3,slice_headers_length); \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_ROTBUF(rid,indent+1,cmd3,ctb_commands); \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],"ctb_commands"); \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_ROTBUF(rid,indent+1,cmd3,ctb_residuals); \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],"ctb_residuals"); \
} while(0)

#define LOG_FIRST_STRUCT(rid,indent,TYPE,cmd,name) do { \
        st_relayfs_write_mme_text(rid,"\n%s{\t{",indent_first[indent]); \
        LOG_##TYPE(rid,indent+1,cmd,name); \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],#name); \
} while(0)

#define LOG_STRUCT_TABLE(rid,indent,TYPE,cmd,name,size) do { \
        st_relayfs_write_mme_text(rid,"\t{"); \
        LOG_##TYPE(rid,indent+1,cmd,name[0]); \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s[0] */",indent_next[indent+1],#name); \
        unsigned int _i; \
        for(_i=1;_i<size;_i++) { \
                st_relayfs_write_mme_text(rid,"\n%s,\t{",indent_next[indent]); \
                LOG_##TYPE(rid,indent+1,cmd,name[_i]); \
                st_relayfs_write_mme_text(rid,"\n%s} /* %s[%d] */",indent_next[indent+1],#name,_i); \
        } \
        st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent],#name); \
} while(0)

#define LOG_NEXT_STRUCT_TABLE(rid,indent,TYPE,cmd,name,size) do { \
        st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
        LOG_STRUCT_TABLE(rid,indent+1,TYPE,cmd,name,size); \
} while(0)

#define LOG_RSZCOEFF(rid,indent,cmd,name) do { \
        hevc_resize_coeff_t *cmd2 = &cmd->name; \
        LOG_ELEM(rid,cmd2,round); \
        LOG_NEXT_TABLE(rid,indent,cmd2,coeff,HEVC_RESIZE_TAPS); \
} while(0)

#define LOG_RESIZE(rid,indent,cmd,name) do { \
  hevcdecpix_resize_param_t *cmd3 = &cmd->name; \
    st_relayfs_write_mme_text(rid,"\n%s{",indent_next[indent+1]); \
    LOG_RSZCOEFF(rid,indent+1,cmd3,v_dec_coeff); \
    st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],"v_dec_coeff"); \
    st_relayfs_write_mme_text(rid,"\n%s{",indent_first[indent+1]); \
    LOG_RSZCOEFF(rid,indent+1,cmd3,h_flt_coeff); \
    st_relayfs_write_mme_text(rid,"\n%s} /* %s */",indent_next[indent+1],"h_flt_coeff"); \
} while(0)
#endif // HEVC_CODEC_DUMP_MME


// ////////////////////////////////////////////////////////////////////////////
//
//  Level information: copied from hevclevels.c in STHM'
//

//! Maximum number of words in 16x16 (minimum CTB size for level<5.0) CTB command area
#define MAX_CTB_16x16_COMMAND_SIZE      22  // = 1 CTB + 3 SAO + 1 TU split + 1 TU cbf +  4x4 CU/PU/MVD
//! Maximum number of words in 32x32 (minimum CTB size for level>=5.0) CTB command area
#define MAX_CTB_32x32_COMMAND_SIZE      73  // = 1 CTB + 3 SAO + 1 TU split + 4 TU cbf + 16x4 CU/PU/MVD
//! Maximum number of samples required to store the debinarized residuals of a 16x16 block
#define MAX_16x16_RESIDUAL_SIZE         400 // Assuming 400 samples for a 16x16 block is enough ( > 384 samples used for a decompressed 16x16 4:2:0 block)

//! Level parameters picked from Annex A of HEVC WD8d7 document
enum {L10, L20, L21, L30, L31, L40, L41, L50, L51, L52, L60, L61, L62, HEVC_NUM_LEVELS};

//! Structure describing the characteristics of a level in the HEVC standard
typedef struct level_data
{
    uint32_t level_idc;           //!< Value of the level_idc syntax element
    uint32_t MaxLumaSR;           //!< Max luma sample rate (samples/s)
    uint32_t MaxLumaPS;           //!< Max luma picture size (samples)
    uint32_t MaxBR_M;             //!< Max bit rate for Main tier (1000 bits/s)
    uint32_t MaxBR_H;             //!< Max bit rate for High tier (1000 bits/s)
    uint32_t MaxCPB_M;            //!< Max CPB size for Main tier (1000 bits)
    uint32_t MaxCPB_H;            //!< Max CPB size for High tier (1000 bits)
    uint32_t MinCR;               //!< Minimum compression ratio
    uint32_t MaxSlicesPerPicture; //!< Max slices per picture
    uint32_t MaxTileRows;         //!< Max # of tile rows
    uint32_t MaxTileCols;         //!< Max # of tile columns
    uint32_t MaxDim;              //!< Sqrt(MaxLumaPS * 8) floored to a multiple of 8 (samples)
    uint32_t Log2MinCtbSize;      //!> Log2(minimum CTB size in luma samples)
} level_t;

static level_t
hevc_levels[HEVC_NUM_LEVELS] =
{
    /*   Level     MaxLumaSR   MaxLumaPS  MaxBR_M   MaxBR_H MaxCPB_M  MaxCPB_H  CR    MS   TR   TC  MaxDim Log2MinCtbSize */
    {  10 * 3 ,     552960u ,    36864u ,    128 , /*-*/ 0 ,    350 , /*-*/ 0 , 2 ,  16 ,  1 ,  1 ,   536, 4 },
    {  20 * 3 ,    3686400u ,   122880u ,   1500 , /*-*/ 0 ,   1500 , /*-*/ 0 , 2 ,  16 ,  1 ,  1 ,   984, 4 },
    {  21 * 3 ,    7372800u ,   245760u ,   3000 , /*-*/ 0 ,   3000 , /*-*/ 0 , 2 ,  20 ,  1 ,  1 ,  1400, 4 },
    {  30 * 3 ,   16588800u ,   552960u ,   6000 , /*-*/ 0 ,   6000 , /*-*/ 0 , 2 ,  30 ,  2 ,  2 ,  2096, 4 },
    {  31 * 3 ,   33177600u ,   983040u ,  10000 , /*-*/ 0 ,  10000 , /*-*/ 0 , 2 ,  40 ,  3 ,  3 ,  2800, 4 },
    {  40 * 3 ,   66846720u ,  2228224u ,  12000 ,   30000 ,  12000 ,   30000 , 4 ,  75 ,  5 ,  5 ,  4216, 4 },
    {  41 * 3 ,  133693440u ,  2228224u ,  20000 ,   50000 ,  20000 ,   50000 , 4 ,  75 ,  5 ,  5 ,  4216, 4 },
    {  50 * 3 ,  267386880u ,  8912896u ,  25000 ,  100000 ,  25000 ,  100000 , 6 , 200 , 11 , 10 ,  8440, 5 },
    {  51 * 3 ,  534773760u ,  8912896u ,  40000 ,  160000 ,  40000 ,  160000 , 8 , 200 , 11 , 10 ,  8440, 5 },
    {  52 * 3 , 1069547520u ,  8912896u ,  60000 ,  240000 ,  60000 ,  240000 , 8 , 200 , 11 , 10 ,  8440, 5 },
    {  60 * 3 , 1069547520u , 35651584u ,  60000 ,  240000 ,  60000 ,  240000 , 8 , 600 , 22 , 20 , 16888, 5 },
    {  61 * 3 , 2139095040u , 35651584u , 120000 ,  480000 , 120000 ,  480000 , 8 , 600 , 22 , 20 , 16888, 5 },
    {  62 * 3 , 4278190080u , 35651584u , 240000 ,  800000 , 240000 ,  800000 , 6 , 600 , 22 , 20 , 16888, 5 }
};


// /////////////////////////////////////////////////////////////////////////
//
// Ring for intermediate process synchro
//

ppFramesRing_c::ppFramesRing_c(int maxSize)
    : mData(NULL)
    , mRData(NULL)
    , mWData(NULL)
    , mSize(0)
    , mFakeTaskNb(0)
    , mMaxSize(maxSize)
    , mLock()
    , mEv()
    , mEvFakeTask()
{
    OS_InitializeMutex(&mLock);
    OS_InitializeEvent(&mEv);
    OS_InitializeEvent(&mEvFakeTask);
}

int ppFramesRing_c::FinalizeInit(void)
{
    mData = new HevcFramesInPreprocessorChain_t[mMaxSize];
    if (!mData)
    {
        SE_ERROR("Failed to allocate ppFramesRing data\n");
        return -1;
    }

    memset(mData, 0, mMaxSize * sizeof(HevcFramesInPreprocessorChain_t));
    mRData = mData;
    mWData = mData;

    return 0;
}

ppFramesRing_c::~ppFramesRing_c(void)
{
    OS_TerminateEvent(&mEvFakeTask);
    OS_TerminateEvent(&mEv);
    OS_TerminateMutex(&mLock);
    delete [] mData;
}

bool ppFramesRing_c::isEmpty()
{
    bool ret = false;
    OS_LockMutex(&mLock);
    ret = (mSize == 0);
    OS_UnLockMutex(&mLock);
    return ret;
}

bool ppFramesRing_c::noFakeTaskPending()
{
    bool ret = false;
    OS_LockMutex(&mLock);
    ret = (mFakeTaskNb == 0);
    OS_UnLockMutex(&mLock);
    return ret;
}

bool ppFramesRing_c::CheckRefListDecodeIndex(int idx)
{
    bool found = false;
    HevcFramesInPreprocessorChain_t *data = mRData;

    OS_LockMutex(&mLock);
    while (data != mWData)
    {
        if (data->Used)
        {
            if (data->DecodeFrameIndex == idx)
            {
                found = true;
                break;
            }
        }
        data = (data == mData + mMaxSize - 1) ? mData : data + 1;
    }
    OS_UnLockMutex(&mLock);
    return found;
}

int ppFramesRing_c::Insert(HevcFramesInPreprocessorChain_t *ppFrame)
{
    if (mSize == mMaxSize)
    {
        SE_ERROR("Failed to insert PP frame (ring full)\n");
        return -1;
    }

    OS_LockMutex(&mLock);
    // Copy ppFrame in internal _data array
    *mWData = *ppFrame;
    mWData->Used = true;
    mWData->FakeCmd = false;
    mSize++;
    if (mWData == mData + mMaxSize - 1)
    {
        mWData = mData;
    }
    else
    {
        mWData++;
    }
    OS_UnLockMutex(&mLock);
    //SE_INFO(group_decoder_video, "PP task nb: %d\n", mSize);
    OS_SetEventInterruptible(&mEv);
    return 0;
}

int ppFramesRing_c::InsertFakeEntry(int frameIdx, HevcFramesInPreprocessorChainAction_t action)
{
    int ret = 0;
    if (mSize == mMaxSize)
    {
        SE_ERROR("Failed to insert PP frame (ring full)\n");
        return -1;
    }

    /* One fake task at a time */
    ret = waitForFakeTaskPending();
    if (ret < 0)
    {
        SE_ERROR("Wait for pending fake tasks failed\n");
        return -1;
    }

    OS_LockMutex(&mLock);
    memset(mWData, 0, sizeof(HevcFramesInPreprocessorChain_t));
    mWData->Used = true;
    mWData->FakeCmd = true;
    mWData->Action = action;
    mWData->DecodeFrameIndex = frameIdx;
    mSize++;
    mFakeTaskNb++;
    if (mWData == mData + mMaxSize - 1)
    {
        mWData = mData;
    }
    else
    {
        mWData++;
    }
    SE_VERBOSE(group_decoder_video, "Inserted fake action: %d (mFakeTaskNb:%d size=%d)\n",
               (int)action, mFakeTaskNb, mSize);
    OS_UnLockMutex(&mLock);

    OS_SetEventInterruptible(&mEv);
    return 0;
}

int ppFramesRing_c::Extract(HevcFramesInPreprocessorChain_t *ppFrame)
{
    int ret = 0;

    if (isEmpty())
    {
        OS_ResetEvent(&mEv);
        // Memory barrier to ensure mEv->Valid is set.
        OS_Smp_Mb();
        // Timeout is arbitrary set to 100ms to avoid useless context changes (could be set to OS_INFINITE)
        ret = OS_WaitForEventInterruptible(&mEv, 100);
        // Test isEmpty is required !
        if ((ret != OS_NO_ERROR) || (isEmpty()))
        {
            // We are blocked here as no entry is made into the ring
            // No entry in the ring could be due to 2 reasons 1. Legal 2. Illegal
            // In the start of payback, the time between Intermediate Task creation and
            // First entry into the ring is > 100 ms so this case is legal.
            // When task which inserts entry into the ring is blocked due to
            // non signalling of mEvFakeTask and this function is waiting for
            // and entry into the ring, so this deadlock case is Illegal.
            // For the second reason invoking signalFakeTaskPending() to signal
            // mEvFakeTask event for waitForFakeTaskPending()
            signalFakeTaskPending();
            SE_DEBUG(group_decoder_video, "ppFramesRing extract timeout or interrupted\n");
            return -1;
        }
    }
    OS_LockMutex(&mLock);
    /* Copy data to avoid potential race conditions at the end of IntermediateProcess */
    *ppFrame = *mRData;
    mSize--;
    if (ppFrame->FakeCmd)
    {
        mFakeTaskNb--;
        SE_VERBOSE(group_decoder_video, "Extracted fake action: %d (mFakeTaskNb:%d size=%d)\n",
                   (int)ppFrame->Action, mFakeTaskNb, mSize);
    }
    if (mRData == mData + mMaxSize - 1)
    {
        mRData = mData;
    }
    else
    {
        mRData++;
    }
    //SE_VERBOSE(group_decoder_video, "Extracted action: %d (size=%d)\n", ppFrame->Action, mSize);
    OS_UnLockMutex(&mLock);
    return 0;
}

void ppFramesRing_c::signalFakeTaskPending()
{
    if (noFakeTaskPending())
    {
        OS_SetEventInterruptible(&mEvFakeTask);
    }
}

int ppFramesRing_c::waitForFakeTaskPending()
{
    int ret = 0;
    // Try blocking caller if no fake task is pending or if a task is pending
    if ((!noFakeTaskPending()) || (!isEmpty()))
    {
        OS_ResetEvent(&mEvFakeTask);
        // Memory barrier to ensure mEvFakeTask->Valid is set.
        OS_Smp_Mb();
        // Timeout is arbitrary set to 1s to avoid useless context changes (could be set to OS_INFINITE)
        ret = OS_WaitForEventInterruptible(&mEvFakeTask, OS_INFINITE);
        // Test isEmpty is required !
        if ((ret != OS_NO_ERROR) || (!noFakeTaskPending()))
        {
            SE_DEBUG(group_decoder_video, "ppFramesRing extract timeout or interrupted\n");
            return -1;
        }
    }
    return 0;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor function, fills in the codec specific parameter values
//

Codec_MmeVideoHevc_c::Codec_MmeVideoHevc_c()
    : Codec_MmeVideo_c()
    , InitTransformerParam()
    , DiscardFramesInPreprocessorChain(false)
    , mPpFramesRing()
    , mPreProcessorBufferType()
    , mPreProcessorBufferAllocator()
    , mPreprocessorBufferSize()
    , mPreProcessorBufferPool()
    , mPreProcessorDevice(NULL)
    , mSliceTableEntries(0)
    , mLastLevelIdc(0xdeadbeef)  // flush cache of level parameters
#ifdef WA_PPB_OUT_NON_REF
    , mFakePPBAllocator(NULL)
    , mFakePPBAddress(NULL)
#endif
    , LocalReferenceFrameList()
{
// TODO: Check if H264 settings still apply for HEVC.
    Configuration.CodecName                             = "HEVC video";
    // Not used
    Configuration.StreamParameterContextCount           = NOT_SPECIFIED;
    Configuration.StreamParameterContextDescriptor      = &HevcCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &HevcCodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 1;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
// TODO: Update this later
    Configuration.TransformName[0]                      = HEVC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = HEVC_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
//
//    Configuration.SizeOfTransformCapabilityStructure    = sizeof(hevcdecpix_transformer_capability_t);
//    Configuration.TransformCapabilityStructurePointer   = (void *)(&H264TransformCapability);
//

    //
    // Since we pre-process the data, we shrink the coded data buffers
    // before entering the generic code. as a consequence we do
    // not require the generic code to shrink buffers on our behalf,
    // and we do not want the generic code to find the coded data buffer
    // for us.
    //
    Configuration.ShrinkCodedDataBuffersAfterDecode     = false;
    Configuration.IgnoreFindCodedDataBuffer     = true;

    //
    // trick mode parameters
    //
// TODO: Following is not quite understood yet and better left commented out for the time being
//    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
//    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
//    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 60;
//    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 60;
//
//    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
//    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease = Rational_t(4,3);
//
//    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
//    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

// /////////////////////////////////////////////////////////////////////////
//
//      FinalizeInit finalizes init work by doing operations that may fail
//      (and such not doable in ctor)
//
CodecStatus_t Codec_MmeVideoHevc_c::FinalizeInit(void)
{
    // Create the ring used to inform the intermediate process of a datum on the way
    mPpFramesRing = new ppFramesRing_c(50);
    if (mPpFramesRing == NULL)
    {
        SE_ERROR("Failed to create ring to hold frames in pre-processor chain\n");
        return CodecError;
    }
    if (mPpFramesRing->FinalizeInit() != 0)
    {
        SE_ERROR("Failed to init PpFramesRing\n");
        return CodecError;
    }

    //
    // Create the intermediate process
    //
    OS_Thread_t  Thread;
    if (OS_CreateThread(&Thread, Codec_MmeVideoHevc_IntermediateProcess, this, &player_tasks_desc[SE_TASK_VIDEO_HEVCINT]) != OS_NO_ERROR)
    {
        SE_ERROR("Failed to create intermediate process\n");
        return CodecError;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset
//      are executed for all levels of the class.
//

Codec_MmeVideoHevc_c::~Codec_MmeVideoHevc_c()
{
    int ret = mPpFramesRing->InsertFakeEntry(0, HevcActionTermination);
    if (ret < 0)
    {
        SE_ERROR("Failed to send HevcActionTermination\n");
    }

    int count = 5;
    while (!mPpFramesRing->isEmpty() && (count-- > 0))
    {
        SE_WARNING("Waiting for HEVC intermediate process to stop %d\n", count);
        OS_SleepMilliSeconds(100);
    }

    //
    // Make sure the pre-processor device is closed (note this may be done in halt,
    // but we check again, since you can enter reset without halt during a failed
    // start up).
    //

    if (mPreProcessorDevice != NULL)
    {
        HevcppClose(mPreProcessorDevice);
    }

    Halt();

    // Delete the frames in pre-processor chain ring
    delete mPpFramesRing;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Halt function
//

CodecStatus_t   Codec_MmeVideoHevc_c::Halt(void)
{
    DeallocatePreProcBufferPool();

    return Codec_MmeVideo_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port, we take this opportunity to
//      create the buffer pools for use in the pre-processor, and the
//      macroblock structure buffers
//

CodecStatus_t Codec_MmeVideoHevc_c::Connect(Port_c *Port)
{
    unsigned int        i;
    //
    // Let the ancestor classes create the ring for decoded frames
    //
    CodecStatus_t Status = Codec_MmeVideo_c::Connect(Port);
    if (Status != CodecNoError)
    {
        SetComponentState(ComponentInError);
        return Status;
    }

    //
    // Create pre-processor intermediate buffer types if they don't already
    // exist
    //
    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i ++)
    {
        Status = BufferManager->FindBufferDataType(
                     HevcPreprocessorBufferDescriptor[i].TypeName,
                     &mPreProcessorBufferType[i]);

        if (Status != BufferNoError)
        {
            Status = BufferManager->CreateBufferDataType(
                         &HevcPreprocessorBufferDescriptor[i],
                         &mPreProcessorBufferType[i]);

            if (Status != BufferNoError)
            {
                SE_ERROR("Failed to create the %s buffer type (%08x)\n",
                         HevcPreprocessorBufferDescriptor[i].TypeName, Status);
                SetComponentState(ComponentInError);
                return CodecError;
            }
        }
    }

    //
    // Open a new instance of the preprocessor device driver
    //
    hevcpp_status_t PPStatus = HevcppOpen(&mPreProcessorDevice);
    if (PPStatus != hevcpp_ok)
    {
        SE_ERROR("Failed to open the pre-processor device (%d)\n", PPStatus);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Output partial decode buffers - not entirely sure what to do here,
//      it may well be necessary to fire off the pre-processing of any
//      accumulated slices.
//

CodecStatus_t   Codec_MmeVideoHevc_c::OutputPartialDecodeBuffers(void)
{
    int ret = 0;

    ret = mPpFramesRing->InsertFakeEntry(0, HevcActionCallOutputPartialDecodeBuffers);
    if (ret < 0)
    {
        SE_ERROR("Failed to OutputPartialDecodeBuffers\n");
        return CodecError;
    }
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Discard queued decodes, this will include any between here
//      and the preprocessor.
//

CodecStatus_t   Codec_MmeVideoHevc_c::DiscardQueuedDecodes(void)
{
    int ret = 0;

    DiscardFramesInPreprocessorChain = true;

    ret = mPpFramesRing->InsertFakeEntry(0, HevcActionCallDiscardQueuedDecodes);
    if (ret < 0)
    {
        SE_ERROR("Failed to DiscardQueuedDecodes\n");
        return CodecError;
    }
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Release reference frame, this must be capable of releasing a
//      reference frame that has not yet exited the pre-processor.
//

CodecStatus_t   Codec_MmeVideoHevc_c::ReleaseReferenceFrame(unsigned int ReferenceFrameDecodeIndex)
{
    int ret = 0;

    ret = mPpFramesRing->InsertFakeEntry(ReferenceFrameDecodeIndex,
                                         HevcActionCallReleaseReferenceFrame);
    if (ret < 0)
    {
        SE_ERROR("Failed to ReleaseReferenceFrame\n");
        return CodecError;
    }
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Check reference frame list needs to account for those currently
//      in the pre-processor chain.
//

CodecStatus_t   Codec_MmeVideoHevc_c::CheckReferenceFrameList(
    unsigned int              NumberOfReferenceFrameLists,
    ReferenceFrameList_t      ReferenceFrameList[])
{
    unsigned int            i, j;
    bool refFound;

    //
    // Check we can cope
    //

    if (NumberOfReferenceFrameLists > HEVC_NUM_REF_FRAME_LISTS)
    {
        SE_ERROR("HEVC has only %d reference frame lists, requested to check %d\n", HEVC_NUM_REF_FRAME_LISTS, NumberOfReferenceFrameLists);
        return CodecUnknownFrame;
    }

    //
    // Construct local lists consisting of those elements we do not know about
    //

    for (i = 0; i < NumberOfReferenceFrameLists; i++)
    {
        LocalReferenceFrameList[i].EntryCount   = 0;

        for (j = 0; j < ReferenceFrameList[i].EntryCount; j++)
        {
            refFound = mPpFramesRing->CheckRefListDecodeIndex(ReferenceFrameList[i].EntryIndicies[j]);
            if (!refFound)
            {
                LocalReferenceFrameList[i].EntryIndicies[LocalReferenceFrameList[i].EntryCount++] = ReferenceFrameList[i].EntryIndicies[j];
            }
        }
    }

    return Codec_MmeVideo_c::CheckReferenceFrameList(NumberOfReferenceFrameLists, LocalReferenceFrameList);
}

// /////////////////////////////////////////////////////////////////////////
//
//      UpdatePreprocessorBufferSize
//      computes the maximum size of each of the 5 intermediate buffer
//      in function of the level of the stream (found in SPS)
//      Returns < 0 if error, > 0 if PreprocessorBufferSize changed, 0 if success
//
//      See STHM': hevc_limits_get()
//

int Codec_MmeVideoHevc_c::UpdatePreprocessorBufferSize(HevcFrameParameters_t *FrameParameters)
{
    int ret = 1;
    int level_index;
    uint32_t level = 0;
    uint32_t pic_width_in_luma_samples, pic_height_in_luma_samples;
    unsigned int IBSize[HEVC_NUM_INTERMEDIATE_BUFFERS];

    if (FrameParameters == NULL)
    {
        SE_WARNING("FrameParameters NULL\n");
        ret = -1;
        goto bail;
    }

    level = FrameParameters->SliceHeader.SequenceParameterSet->level_idc;
    pic_width_in_luma_samples  = FrameParameters->SliceHeader.SequenceParameterSet->pic_width_in_luma_samples;
    pic_height_in_luma_samples = FrameParameters->SliceHeader.SequenceParameterSet->pic_height_in_luma_samples;

    if (level == mLastLevelIdc)
    {
        SE_DEBUG(group_decoder_video, "Level (%d) unchanged\n", level);
        ret = 0;
        goto bail;
    }

    mLastLevelIdc = level;

    // Find level in array of levels
    for (level_index = 0; level_index < HEVC_NUM_LEVELS; level_index ++)
    {
        if (hevc_levels[level_index].level_idc == level)
        {
            break;
        }
    }
    if (level_index < HEVC_NUM_LEVELS)
    {
        GetIntermediateBufferSize(level_index, pic_width_in_luma_samples, pic_height_in_luma_samples, mPreprocessorBufferSize);
        ret = 0;
        goto bail;
    }

    SE_ERROR("unsupported level_idc %d (level %d.%d)\n", level, level / 30, (level % 30) / 3);
    memset(mPreprocessorBufferSize, 0, sizeof(mPreprocessorBufferSize));

    for (level_index = 0; level_index <= L51; level_index ++)
    {
        int ib_index;

        GetIntermediateBufferSize(level_index, pic_width_in_luma_samples, pic_height_in_luma_samples, IBSize);

        for (ib_index = 0; ib_index < HEVC_NUM_INTERMEDIATE_BUFFERS; ib_index++)
        {
            if (IBSize[ib_index] > mPreprocessorBufferSize[ib_index])
            {
                mPreprocessorBufferSize[ib_index] = IBSize[ib_index];
                ret = 1;
            }
        }

        if (hevc_levels[level_index].level_idc == HADES_MAX_LEVEL_IDC)
        {
            break;
        }
    }


bail:
    SE_DEBUG(group_decoder_video, "size of IB: level_idc %d, %d %d %d %d %d\n",
             level,
             mPreprocessorBufferSize[HEVC_SLICE_TABLE],
             mPreprocessorBufferSize[HEVC_CTB_TABLE],
             mPreprocessorBufferSize[HEVC_SLICE_HEADERS],
             mPreprocessorBufferSize[HEVC_CTB_COMMANDS],
             mPreprocessorBufferSize[HEVC_CTB_RESIDUALS]);
    return ret;
}

CodecStatus_t Codec_MmeVideoHevc_c::GetIntermediateBufferSize(int level_index,
                                                              uint32_t pic_width_in_luma_samples,
                                                              uint32_t pic_height_in_luma_samples,
                                                              unsigned int *IBSize)
{
    int i;
    uint32_t log2_min_ctb_size;
    uint32_t width_in_ctb, height_in_ctb;
    uint32_t offset;
    uint32_t size_in_ctb;
    uint32_t ctb_table;
    uint32_t tile_columns;
    uint32_t ctb_cmd_size, ctb_command_size;
    uint32_t ctb_res_size, ctb_residual_size;
    // STHM' brainy computations :-)
    log2_min_ctb_size = hevc_levels[level_index].Log2MinCtbSize;

    if (log2_min_ctb_size == 4)
    {
        ctb_cmd_size = MAX_CTB_16x16_COMMAND_SIZE * sizeof(uint32_t);
        ctb_res_size = MAX_16x16_RESIDUAL_SIZE; // in bytes
    }
    else
    {
        ctb_cmd_size = MAX_CTB_32x32_COMMAND_SIZE * sizeof(uint32_t);
        ctb_res_size = 4 * MAX_16x16_RESIDUAL_SIZE; // in bytes
    }

    offset = (1 << log2_min_ctb_size) - 1;

    if (pic_width_in_luma_samples > hevc_levels[level_index].MaxDim)
        SE_ERROR("picture width %u in stream level_idc %u should be less than %u\n",
                 pic_width_in_luma_samples, hevc_levels[level_index].level_idc, hevc_levels[level_index].MaxDim);

    if (pic_height_in_luma_samples > hevc_levels[level_index].MaxDim)
        SE_ERROR("picture height %u in stream level_idc %u should be less than %u\n",
                 pic_width_in_luma_samples, hevc_levels[level_index].level_idc, hevc_levels[level_index].MaxDim);

    width_in_ctb = (pic_width_in_luma_samples  + offset) >> log2_min_ctb_size;
    height_in_ctb = (pic_height_in_luma_samples + offset) >> log2_min_ctb_size;
    size_in_ctb = width_in_ctb * height_in_ctb;
    tile_columns = (pic_width_in_luma_samples + 255) >> 8;
    ctb_table = height_in_ctb * tile_columns;
    ctb_command_size = ctb_cmd_size * size_in_ctb;
    ctb_residual_size = ctb_res_size * size_in_ctb;
    // Fill out intermediate buffer size in bytes
    IBSize[HEVC_SLICE_TABLE]   = hevc_levels[level_index].MaxSlicesPerPicture * SLICE_TABLE_ENTRY_SIZE * sizeof(uint32_t);
    // Double CTB_TABLE size for cut2
    IBSize[HEVC_CTB_TABLE]     = 2 * ctb_table * CTB_TABLE_ENTRY_SIZE * sizeof(uint32_t);
    IBSize[HEVC_SLICE_HEADERS] = hevc_levels[level_index].MaxSlicesPerPicture * MAX_SLICE_HEADER_SIZE * sizeof(uint32_t);
    IBSize[HEVC_CTB_COMMANDS]  = ctb_command_size;
    IBSize[HEVC_CTB_RESIDUALS] = ctb_residual_size;

    // PP writes an extra 0-word at end of all tables except CTB table
    IBSize[HEVC_SLICE_TABLE] += 4;
    IBSize[HEVC_SLICE_HEADERS] += 4;
    IBSize[HEVC_CTB_COMMANDS] += 4;
    IBSize[HEVC_CTB_RESIDUALS] += 8;

    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
    {
        BUF_ALIGN_UP(mPreprocessorBufferSize[i], HEVCPP_BUFFER_ALIGNMENT);
    }

    SE_DEBUG(group_decoder_video, "size of IB: level_idc %d, %d %d %d %d %d\n",
             hevc_levels[level_index].level_idc, IBSize[HEVC_SLICE_TABLE],
             IBSize[HEVC_CTB_TABLE] , IBSize[HEVC_SLICE_HEADERS],
             IBSize[HEVC_CTB_COMMANDS], IBSize[HEVC_CTB_RESIDUALS]);
    return PlayerNoError;
}

void Codec_MmeVideoHevc_c::DeallocatePreProcBufferPool(void)
{
#ifdef WA_PPB_OUT_NON_REF
    AllocatorClose(&mFakePPBAllocator);
#endif

    // Free buffer pools
    for (int i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i ++)
    {
        if (mPreProcessorBufferPool[i] != NULL)
        {
            BufferManager->DestroyPool(mPreProcessorBufferPool[i]);
            mPreProcessorBufferPool[i] = NULL;
        }

        AllocatorClose(&mPreProcessorBufferAllocator[i]);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
// Create or re-allocate the pre-processor buffer pools
CodecStatus_t Codec_MmeVideoHevc_c::AllocatePreProcBufferPool(HevcFrameParameters_t *FrameParameters)
{
    uint32_t pic_width_in_luma_samples = FrameParameters->SliceHeader.SequenceParameterSet->pic_width_in_luma_samples;
    uint32_t pic_height_in_luma_samples = FrameParameters->SliceHeader.SequenceParameterSet->pic_height_in_luma_samples;
    uint32_t max_numbers_of_ppbs = (pic_width_in_luma_samples / 16) * (pic_height_in_luma_samples / 16);
    allocator_status_t AllocatorStatus = allocator_ok;
    void *PPBufferMemoryPool[3];
    unsigned int PPPoolSize, TotalPPSize = 0;
    BufferStatus_t Status;
    int i = 0;

    //free pool/allocator if memory is already allocated
    DeallocatePreProcBufferPool();

    /* Allocation in reverse order -> to allocate big buffers first */
    for (i = HEVC_NUM_INTERMEDIATE_BUFFERS - 1; i >= 0 ; i --)
    {
        // HEVC_CTB_RESIDUALS pool must be > 2 x HEVC_CTB_RESIDUALS max buffer size
        if ((i == HEVC_CTB_RESIDUALS) || (i == HEVC_CTB_COMMANDS))
        {
            PPPoolSize = 3 * mPreprocessorBufferSize[i];
        }
        else
        {
            PPPoolSize = HEVC_NB_OF_FRAME_BUFFERED * mPreprocessorBufferSize[i];
        }

        /* For small resolution streams, pool must be oversized to avoid GetBuffer failure */
        if ((pic_height_in_luma_samples < 256) || (pic_width_in_luma_samples < 256))
        {
            PPPoolSize *= 2;
        }
        PPPoolSize = BUF_ALIGN_UP(PPPoolSize, HEVCPP_BUFFER_ALIGNMENT);
        TotalPPSize += PPPoolSize;
        SE_INFO(group_decoder_video, "Requesting PP pool[%d] of size: %d\n", i, PPPoolSize);

        allocator_status_t AllocatorStatus = PartitionAllocatorOpen(&mPreProcessorBufferAllocator[i],
                                                                    Configuration.AncillaryMemoryPartitionName,
                                                                    PPPoolSize, MEMORY_DEFAULT_ACCESS);

        if (AllocatorStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate pre-processor buffer pool of type %s (%08x)\n",
                     HevcPreprocessorBufferDescriptor[i].TypeName,
                     AllocatorStatus);
            SetComponentState(ComponentInError);
            // No need to free existing allocators, dtor() will take care of it
            return CodecError;
        }

        PPBufferMemoryPool[CachedAddress] = AllocatorUserAddress(mPreProcessorBufferAllocator[i]);
        PPBufferMemoryPool[PhysicalAddress] = AllocatorPhysicalAddress(mPreProcessorBufferAllocator[i]);

        Status = BufferManager->CreatePool(
                     &mPreProcessorBufferPool[i],
                     mPreProcessorBufferType[i],
                     NOT_SPECIFIED,
                     PPPoolSize,
                     PPBufferMemoryPool,
                     NULL, NULL, true);

        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to create pre-processor buffer pool of type %s (%08x)\n",
                     HevcPreprocessorBufferDescriptor[i].TypeName,
                     Status);
            SetComponentState(ComponentInError);
            // No need to destroy existing pools, dtor() will take care of it
            return CodecError;
        }
    }

#ifdef WA_PPB_OUT_NON_REF
    AllocatorStatus = PartitionAllocatorOpen(&mFakePPBAllocator,
                                             Configuration.AncillaryMemoryPartitionName,
                                             32 * max_numbers_of_ppbs, true);   // PPB max: 4 words / MB, 16x16 pixels

    if (AllocatorStatus != allocator_ok)
    {
        SE_ERROR("Failed to allocate Fake PPB (%08x)\n", AllocatorStatus);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    mFakePPBAddress = AllocatorPhysicalAddress(mFakePPBAllocator);
    TotalPPSize += 32 * max_numbers_of_ppbs;
#endif

    SE_INFO(group_decoder_video, "Total size of PP pools: %d\n", TotalPPSize);
    return CodecNoError;
}

// //////////////////////////////////////////////////////////////////////////////
//      Remove superfluous zero bytes at end of nal unit
//      Might be the first 0 of a 4-byte start code, or anti-emulated zero bytes
//
void Codec_MmeVideoHevc_c::TrimCodedBuffer(uint8_t *buffer, unsigned int DataOffset, unsigned int *CodedSlicesSize)
{
    uint8_t *at_start = buffer + DataOffset;
    uint8_t *at_end = at_start + *CodedSlicesSize - 1;
    uint8_t *at_tmp;
    uint32_t *tmp;
    // Little endian data format
    uint32_t mask = 0xFFFFFF00U;
    uint32_t val = 0x300000U;

    while ((at_end > at_start) && (*at_end == 0))  { at_end--; }

    at_tmp = at_end;
    tmp = (uint32_t *)(at_tmp - 3);
    // Fast search : try 12 bytes before
    while ((tmp > (uint32_t *)at_start) && (((*tmp) & mask) == val))
    {
        /* Lowest common multiple is 12 (= 3x4 bytes)*/
        at_tmp = (uint8_t *)tmp;
        tmp -= 3;
    }
    // Accurate search of the last 3-bytes != 00 00 03
    if (tmp < (uint32_t *)at_end - 3)
    {
        at_end = at_tmp;
        tmp = (uint32_t *)(at_end - 3);
        while ((at_end > at_start) && (((*tmp) & mask) == val))
        {
            at_end -= 3;
            tmp = (uint32_t *)at_end;
        }
    }

    *CodedSlicesSize = at_end - at_start + 1;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Input, must feed the data to the preprocessor chain.
//      This function needs to replicate a considerable amount of the
//      ancestor classes, because this function is operating
//      in a different process to the ancestors, and because the
//      ancestor data, and the data used here, will be in existence
//      at the same time.
//

CodecStatus_t Codec_MmeVideoHevc_c::Input(Buffer_t CodedBuffer)
{
    unsigned int            i, j;
    int                     ret = 0;
    bool                    allocatePoolNeeded = false;
    hevcpp_status_t         PPStatus;
    BufferStatus_t          BufferStatus;
    unsigned int            CodedDataLength;
    void                   *CodedDataPhysAddr;
    void                   *CodedDataCachedAddr;
    unsigned int            NumPocTotalCurr;
    void                   *IntermediateBufferPhysAddr[HEVC_NUM_INTERMEDIATE_BUFFERS];
    ParsedFrameParameters_t *ParsedFrameParameters;
    ParsedVideoParameters_t *ParsedVideoParameters;
    HevcFrameParameters_t   *FrameParameters;
    HevcFramesInPreprocessorChain_t ppFrame;
    hevcdecpreproc_transform_param_t *ppCmd;

    /* Before locking buffers, check PP and block this thread if not available */
    PPStatus = HevcppGetPreProcAvailability(mPreProcessorDevice);
    if (PPStatus != hevcpp_ok)
    {
        SE_ERROR("HEVC PP not available\n");
        return CodecError;
    }

    //
    // First extract the useful pointers from the buffer all held locally
    //
    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType,
                                         (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);
    FrameParameters = (HevcFrameParameters_t *) ParsedFrameParameters->FrameParameterStructure;

    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType,
                                         (void **)(&ParsedVideoParameters));
    SE_ASSERT(ParsedVideoParameters != NULL);

    //
    // If this does not contain any frame data, then we simply wish
    // to slot it into place for the intermediate process.
    //
    if (!ParsedFrameParameters->NewFrameParameters ||
        (ParsedFrameParameters->DecodeFrameIndex == INVALID_INDEX))
    {
        memset(&ppFrame, 0, sizeof(HevcFramesInPreprocessorChain_t));
        ppFrame.Action                = HevcActionPassOnFrame;
        ppFrame.CodedBuffer           = CodedBuffer;
        ppFrame.ParsedFrameParameters = ParsedFrameParameters;
        ppFrame.DecodeFrameIndex      = ParsedFrameParameters->DecodeFrameIndex;

        CodedBuffer->IncrementReferenceCount(IdentifierHevcIntermediate);
        ret = mPpFramesRing->Insert(&ppFrame);
        if (ret < 0)
        {
            SE_ERROR("Failed to insert frame for PP\n");
            CodedBuffer->DecrementReferenceCount(IdentifierHevcIntermediate);
            return CodecError;
        }
        return CodecNoError;
    }

    //
    // We believe we have a frame - check that this is marked as a first slice
    //
    if (!ParsedVideoParameters->FirstSlice)
    {
        SE_ERROR("Non first slice, when one is expected\n");
        return CodecError;
    }

    // Allocate intermediate buffers and attach them to coded buffer
    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
    {
        if (mPreProcessorBufferPool[i] == NULL)
        {
            allocatePoolNeeded = true;
            break;
        }
    }
    ret = UpdatePreprocessorBufferSize(FrameParameters);
    if (ret < 0)
    {
        SE_ERROR("Unable to get PPBuffer size\n");
        return CodecError;
    }
    if ((ret > 0) || (allocatePoolNeeded))
    {
        SE_INFO(group_decoder_video, "PPBuffer size has changed -> updating the PP Pool size\n");
        // Waiting for end of ALL PP pending tasks
        while (!mPpFramesRing->isEmpty());
        long long unsigned  int b = OS_GetTimeInMicroSeconds();
        if (CodecNoError != AllocatePreProcBufferPool(FrameParameters))
        {
            SE_ERROR("Unable to allocate PP buffer pools\n");
            return CodecError;
        }
        SE_INFO(group_decoder_video, "Alloc PP pool duration: %llu us\n", OS_GetTimeInMicroSeconds() - b);
    }

    memset(&ppFrame, 0, sizeof(HevcFramesInPreprocessorChain_t));
    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
    {
        // SE_DEBUG(group_decoder_video, "PreprocessorBufferSize %d = %d\n", i, mPreprocessorBufferSize[i]);
        /* Request for a pp buffer with aligned size as start addr will be aligned */
        BufferStatus = mPreProcessorBufferPool[i]->GetBuffer(&ppFrame.PreProcessorBuffer[i],
                                                             UNSPECIFIED_OWNER,
                                                             BUF_ALIGN_UP(mPreprocessorBufferSize[i], HEVCPP_BUFFER_ALIGNMENT));
        if (BufferStatus != BufferNoError)
        {
            break;
        }

        CodedBuffer->AttachBuffer(ppFrame.PreProcessorBuffer[i]);
        // Now that buffer is attached to the coded buffer we can release our hold on it
        ppFrame.PreProcessorBuffer[i]->DecrementReferenceCount();
        // Need to store physical address for preprocessor
        ppFrame.PreProcessorBuffer[i]->ObtainDataReference(NULL,
                                                           NULL,
                                                           &IntermediateBufferPhysAddr[i],
                                                           PhysicalAddress);
        SE_ASSERT(IntermediateBufferPhysAddr[i] != NULL); // not expected to be empty ?
    }

    //
    // Failed to allocate all buffers. Free already allocated buffers if any
    // and return error
    //
    if (BufferStatus != BufferNoError)
    {
        for (j = 0; j < i; j++)
        {
            CodedBuffer->DetachBuffer(ppFrame.PreProcessorBuffer[j]);
        }

        SE_ERROR("Failed to get a pre-processor buffer of type %d (%08x)\n", j, BufferStatus);
        return CodecError;
    }

    //
    // Fill in relevant fields
    //
    ppFrame.Action                = HevcActionPassOnPreProcessedFrame;
    ppFrame.CodedBuffer           = CodedBuffer;
    ppFrame.ParsedFrameParameters = ParsedFrameParameters;
    ppFrame.DecodeFrameIndex      = ParsedFrameParameters->DecodeFrameIndex;

    //
    // Preprocessor driver requires coded buffer's physical and cached addresses
    //
    CodedBuffer->ObtainDataReference(NULL,
                                     &CodedDataLength,
                                     &CodedDataCachedAddr,
                                     CachedAddress);  // Get data size and cached address
    SE_ASSERT(CodedDataCachedAddr != NULL); // not expected to be empty ? .. TBC
    CodedBuffer->ObtainDataReference(NULL,
                                     NULL,
                                     &CodedDataPhysAddr,
                                     PhysicalAddress);  // Get data physical address
    SE_ASSERT(CodedDataPhysAddr != NULL); // not expected to be empty ? .. TBC

#ifdef HEVC_HADES_CUT1
    // PP cut1 generates errors if AEB are taken into account (fixed in cut2)
    TrimCodedBuffer((uint8_t *)CodedDataCachedAddr, ParsedFrameParameters->DataOffset, &FrameParameters->CodedSlicesSize);
#endif

    // Get cached address of intermediate buffer so driver preproc can compute CRCs if required
    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
    {
        ppFrame.PreProcessorBuffer[i]->ObtainDataReference(NULL,
                                                           NULL,
                                                           &ppFrame.QueInfo.iIntermediateBufferCachedAddress[i],
                                                           CachedAddress);  // Get data size and cached address
        SE_ASSERT(ppFrame.QueInfo.iIntermediateBufferCachedAddress[i] != NULL); // not expected to be empty ? .. TBC
    }

    //
    // Prepare data structure for preprocessor driver
    //
    ppCmd = &ppFrame.QueInfo.iCmd;
    ppCmd->bit_buffer.base_addr = (uint32_t) CodedDataPhysAddr & (~HEVCPP_BUFFER_ALIGNMENT);
#ifdef HEVC_HADES_CUT1
    ppCmd->bit_buffer.length = BUF_ALIGN_UP(CodedDataLength, HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->bit_buffer.start_offset = ParsedFrameParameters->DataOffset + ((uint32_t) CodedDataPhysAddr & HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->bit_buffer_stop_offset = ppCmd->bit_buffer.start_offset + FrameParameters->CodedSlicesSize;
#else
    ppCmd->bit_buffer.length = FrameParameters->CodedSlicesSize ;
    ppCmd->bit_buffer.start_offset = ((uint32_t) CodedDataPhysAddr & HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->bit_buffer.end_offset = ppCmd->bit_buffer.start_offset + FrameParameters->CodedSlicesSize;
#endif
    /* Addr must be aligned on HEVCPP_BUFFER_ALIGNMENT for HW (as IBSize[i]) */
    ppCmd->intermediate_buffer.slice_table_base_addr = BUF_ALIGN_UP(IntermediateBufferPhysAddr[HEVC_SLICE_TABLE], HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->intermediate_buffer.slice_table_length = mPreprocessorBufferSize[HEVC_SLICE_TABLE];
    ppCmd->intermediate_buffer.ctb_table_base_addr = BUF_ALIGN_UP(IntermediateBufferPhysAddr[HEVC_CTB_TABLE], HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->intermediate_buffer.ctb_table_length = mPreprocessorBufferSize[HEVC_CTB_TABLE];
    ppCmd->intermediate_buffer.slice_headers_base_addr = BUF_ALIGN_UP(IntermediateBufferPhysAddr[HEVC_SLICE_HEADERS], HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->intermediate_buffer.slice_headers_length = mPreprocessorBufferSize[HEVC_SLICE_HEADERS];
    ppCmd->intermediate_buffer.ctb_commands.base_addr = BUF_ALIGN_UP(IntermediateBufferPhysAddr[HEVC_CTB_COMMANDS], HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->intermediate_buffer.ctb_commands.length = mPreprocessorBufferSize[HEVC_CTB_COMMANDS];
    ppCmd->intermediate_buffer.ctb_commands.start_offset = 0;
#ifndef HEVC_HADES_CUT1
    ppCmd->intermediate_buffer.ctb_commands.end_offset = mPreprocessorBufferSize[HEVC_CTB_COMMANDS];
#endif
    ppCmd->intermediate_buffer.ctb_residuals.base_addr = BUF_ALIGN_UP(IntermediateBufferPhysAddr[HEVC_CTB_RESIDUALS], HEVCPP_BUFFER_ALIGNMENT);
    ppCmd->intermediate_buffer.ctb_residuals.length = mPreprocessorBufferSize[HEVC_CTB_RESIDUALS];
    ppCmd->intermediate_buffer.ctb_residuals.start_offset = 0;
#ifndef HEVC_HADES_CUT1
    ppCmd->intermediate_buffer.ctb_residuals.end_offset = mPreprocessorBufferSize[HEVC_CTB_RESIDUALS];
#endif
    ppFrame.QueInfo.iDecodeIndex = ParsedFrameParameters->DecodeFrameIndex;
    ppFrame.QueInfo.iCodedBufferCachedAddress = (void *)((uint32_t) CodedDataCachedAddr - ((uint32_t) CodedDataPhysAddr & HEVCPP_BUFFER_ALIGNMENT));

    if (ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0].EntryCount != ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1].EntryCount)
    {
        SE_ERROR("Ref pic lists do not have same number of elements\n");
        return CodecError;
    }

    NumPocTotalCurr = ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0].EntryCount;
    FillOutPreprocessorCommand(ppCmd,
                               FrameParameters->SliceHeader.SequenceParameterSet,
                               FrameParameters->SliceHeader.PictureParameterSet,
                               NumPocTotalCurr, ParsedFrameParameters->DecodeFrameIndex);
    CodedBuffer->IncrementReferenceCount(IdentifierHevcIntermediate);

    //
    // Add buffer to the preprocessor queue. This call will block if the queue
    // is full or if we have reached the max number of buffers that can be
    // queued concurrently by a single client.
    //
    SE_VERBOSE(group_decoder_video, ">PP frame %d\n", ppFrame.DecodeFrameIndex);

    PPStatus = HevcppQueueBuffer(mPreProcessorDevice, &ppFrame.QueInfo);
    if (PPStatus != hevcpp_ok)
    {
        for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
        {
            CodedBuffer->DetachBuffer(ppFrame.PreProcessorBuffer[i]);
        }

        CodedBuffer->DecrementReferenceCount(IdentifierHevcIntermediate);
        ppFrame.Action = HevcActionNull;
        SE_ERROR("Failed to queue a buffer, junking frame\n");
        return CodecError;
    }

    /* Add buffer to ring buffer so Intermediate process can pick it up later
       (must be inline with HevcppQueueBuffer calls) */
    ret = mPpFramesRing->Insert(&ppFrame);
    if (ret < 0)
    {
        SE_ERROR("Failed to insert PP frame[%d]\n", ppFrame.DecodeFrameIndex);
        return CodecError;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an HEVC mme transformer.
//

CodecStatus_t   Codec_MmeVideoHevc_c::HandleCapabilities(void)
{
// TODO: These values are probably not correct. Change them later.
    // Raster output
    BufferFormat_t  DisplayFormat = FormatVideo420_Raster2B;
    // Elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement |
                                                  DecimatedManifestationElement |
                                                  VideoMacroblockStructureElement |
                                                  VideoDecodeCopyElement;
    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat, Elements,
                                                         Configuration.ListOfDecodeBufferComponents);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to extend the buffer request to incorporate the macroblock sizing
//

CodecStatus_t   Codec_MmeVideoHevc_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request)
{
    // HevcSequenceParameterSet_t *sps;
    //unsigned int                            ppb_length;
    //
    // Fill out the standard fields first
    //
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);
    //
    // and now the extended field
    //
    // HEVC PPB: 4 words per 16x16 block, see ppb_t[PPB_CMD_SIZE] in STHM'
    Request->MacroblockSize                             = 16 * 16;
    Request->PerMacroblockMacroblockStructureSize       = 4 * sizeof(uint32_t);
    Request->PerMacroblockMacroblockStructureFifoSize   = 0;  // Hades doesn't overrun PPB buffer (or does it ? FIXME)
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Populate the transformers initialization parameters.
//
//      This method is required the fill out the TransformerInitParamsSize
//      and TransformerInitParams_p members of Codec_MmeBase_c::MMEInitializationParameters
//

CodecStatus_t   Codec_MmeVideoHevc_c::FillOutTransformerInitializationParameters(void)
{
    // Pass the instance number of the stream (used for CRC checking)
    InitTransformerParam.InstanceId = Stream->GetInstanceId();
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(InitTransformerParam);
    MMEInitializationParameters.TransformerInitParams_p         = &InitTransformerParam;
//
    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoHevc_c::FillOutSetStreamParametersCommand(void)
{
    // Should never get here
    SE_ERROR("Implementation error\n");
    return CodecError;
}

// TODO: rework this static class
uint32_t Codec_MmeVideoHevc_c::scaling_list_command_init(uint32_t *pt,
                                                         uint8_t use_default,
                                                         scaling_list_t *scaling_list)
{
    uint32_t idx = 0;
    uint32_t word;

    for (uint8_t size_id = 0; size_id < MAX_SCALING_SIZEID; size_id++)
    {
        uint8_t max_matrixID = (size_id == 3) ? 2 : MAX_SCALING_MATRIXID;

        for (uint8_t matrix_id = 0; matrix_id < max_matrixID; matrix_id++)
        {
            if (use_default)
            {
                FIELD_INIT(word, HEVC_FORMAT_PIC_SCALING_LIST_PRED, scaling_list_pred_mode_flag, 0);
                FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_PRED, scaling_list_pred_matrix_id_delta, 0);
                pt[idx++] = word;
            }
            else
            {
                scaling_list_elem_t *scaling_list_elem = &(*scaling_list)[size_id][matrix_id];
                FIELD_INIT(word, HEVC_FORMAT_PIC_SCALING_LIST_PRED, scaling_list_pred_mode_flag, scaling_list_elem->scaling_list_pred_mode_flag);
                FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_PRED, scaling_list_pred_matrix_id_delta, scaling_list_elem->scaling_list_pred_matrix_id_delta);

                if (scaling_list_elem->scaling_list_pred_mode_flag)
                {
                    if (size_id > 1) // 16x16 or 32x32 TU
                    {
                        FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_PRED, scaling_list_dc_coeff_minus8, scaling_list_elem->scaling_list_dc_coef_minus8);
                    }

                    pt[idx++] = word;
                    word = 0;
                    uint8_t num_coef = (size_id == 0 ? 16 : 64);

                    for (uint8_t i = 0; i < num_coef; i += 4)
                    {
                        FIELD_INIT(word, HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF, scaling_list_delta_coef0, scaling_list_elem->scaling_list_delta_coef[i + 0]);
                        FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF, scaling_list_delta_coef1, scaling_list_elem->scaling_list_delta_coef[i + 1]);
                        FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF, scaling_list_delta_coef2, scaling_list_elem->scaling_list_delta_coef[i + 2]);
                        FIELD_SET(word, HEVC_FORMAT_PIC_SCALING_LIST_DELTA_COEF, scaling_list_delta_coef3, scaling_list_elem->scaling_list_delta_coef[i + 3]);
                        pt[idx++] = word;
                    }
                }
                else
                {
                    pt[idx++] = word;
                }
            }
        }
    }

    return idx;
}

CodecStatus_t   Codec_MmeVideoHevc_c::FillOutDecodeCommand(void)
{
    HevcCodecDecodeContext_t *Context = (HevcCodecDecodeContext_t *)DecodeContext;
    Buffer_t                 PreProcessorBuffer;
    unsigned int             InputBuffer[HEVC_NUM_INTERMEDIATE_BUFFERS];
    unsigned int             InputBufferSize[HEVC_NUM_INTERMEDIATE_BUFFERS];
    unsigned int             i;
    unsigned int             j;
    unsigned int             NumRefPic;
    int32_t                  abs_diff_poc, min_abs_diff_poc;

    //
    // Detach the pre-processor buffers and re-attach them to the decode context
    // this will make its release automatic on the freeing of the decode context.
    //

    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
    {
        CodedFrameBuffer->ObtainAttachedBufferReference(mPreProcessorBufferType[i], &PreProcessorBuffer);
        SE_ASSERT(PreProcessorBuffer != NULL);

        DecodeContextBuffer->AttachBuffer(PreProcessorBuffer);      // Must be ordered, otherwise the pre-processor
        CodedFrameBuffer->DetachBuffer(PreProcessorBuffer);         // buffer will get released in the middle
        // Store address and size of buffer
        PreProcessorBuffer->ObtainDataReference(NULL, InputBufferSize + i, (void **)&InputBuffer[i], PhysicalAddress);
        SE_ASSERT(InputBuffer[i] != NULL); // not expected to be empty ? .. TBC
    }

    //
    // Fill out the sub-components of the command data
    //
    memset(&Context->DecodeParameters, 0, sizeof(hevcdecpix_transform_param_t));
    memset(&Context->DecodeStatus, 0xa5, sizeof(hevcdecpix_status_t));
    //
    HevcSequenceParameterSet_t *sps;
    HevcPictureParameterSet_t *pps;
    hevcdecpix_transform_param_t *cmd;
    HevcSliceHeader_t *sliceheader = &((HevcFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure)->SliceHeader;
    sps = sliceheader->SequenceParameterSet;
    pps = sliceheader->PictureParameterSet;
    cmd = &Context->DecodeParameters;

    // Scaling list
    if (sps->scaling_list_enable_flag)
    {
        if (pps->pps_scaling_list_data_present_flag)
        {
            cmd->scaling_list_command_size = scaling_list_command_init(cmd->scaling_list_command, false, &pps->scaling_list);
        }
        else
        {
            cmd->scaling_list_command_size = scaling_list_command_init(cmd->scaling_list_command, !sps->sps_scaling_list_data_present_flag, &sps->scaling_list);
        }
    }
    else
    {
        cmd->scaling_list_command_size = 0;
    }

    for (i = cmd->scaling_list_command_size; i < HEVC_MAX_SCALING_LIST_BUFFER; i++)
    {
        cmd->scaling_list_command[i] = 0;
    }

    // Set bases of DPB and PPBs to 0 so that offset == address
    cmd->dpb_base_addr = 0;
    cmd->ppb_base_addr = 0;

    // Reference frame lists

    // before filling up the ref_picture_info[] array, set all pictures to 'non-existing'
    for (i = 0; i < HEVC_MAX_REFERENCE_PICTURES; i++)
    {
        cmd->ref_picture_info[i].non_existing_flag = 1;
        cmd->ref_picture_buffer[i].enable_flag = false;
    }

    NumRefPic = 0;

    for (i = 0; i < DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount; i++)
    {
        // Is reference picture already in array ?
        for (j = 0; j < NumRefPic; j++)
            if (cmd->ref_picture_info[j].poc == DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].ReferenceDetails[i].PicOrderCnt)
            {
                break;
            }

        cmd->initial_ref_pic_list_l0[i] = j;

        // Store info if this is a new reference picture
        if (j == NumRefPic)
        {
            unsigned int BufferIndex = DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryIndicies[i];
            cmd->ref_picture_buffer[j].pixels.luma_offset   = (uint32_t) Stream->GetDecodeBufferManager()->Luma(BufferState[BufferIndex].Buffer, VideoDecodeCopy);
            cmd->ref_picture_buffer[j].pixels.chroma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Chroma(BufferState[BufferIndex].Buffer, VideoDecodeCopy);
            cmd->ref_picture_buffer[j].pixels.stride = Stream->GetDecodeBufferManager()->ComponentStride(BufferState[BufferIndex].Buffer, VideoDecodeCopy, 0, 0);
            cmd->ref_picture_buffer[j].pixels.raster2b_not_omega4 = false;
            cmd->ref_picture_buffer[j].ppb_offset = (uint32_t) Stream->GetDecodeBufferManager()->ComponentBaseAddress(BufferState[BufferIndex].Buffer, VideoMacroblockStructure);

            if ((cmd->ref_picture_buffer[j].pixels.luma_offset != 0) && (cmd->ref_picture_buffer[j].pixels.chroma_offset != 0))
            {
                cmd->ref_picture_buffer[j].enable_flag = true;
                cmd->ref_picture_info[j].non_existing_flag = 0;
            }
            else
            {
                SE_WARNING("New reference buffer has either or both luma and chroma pointers NULL. Marking ref[%d] as unavailable.\n", j);
                cmd->ref_picture_buffer[j].enable_flag = false;
                cmd->ref_picture_info[j].non_existing_flag = 1;
            }

            cmd->ref_picture_info[j].long_term_flag = DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].ReferenceDetails[i].LongTermReference;
            cmd->ref_picture_info[j].poc = DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].ReferenceDetails[i].PicOrderCnt;
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d L    %08x -> %08x\n",
                       ParsedFrameParameters->DecodeFrameIndex, j,
                       cmd->ref_picture_buffer[j].pixels.luma_offset,
                       cmd->ref_picture_buffer[j].pixels.luma_offset +
                       (uint32_t) Stream->GetDecodeBufferManager()->LumaSize(BufferState[BufferIndex].Buffer, VideoDecodeCopy));
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d C    %08x -> %08x\n",
                       ParsedFrameParameters->DecodeFrameIndex, j,
                       cmd->ref_picture_buffer[j].pixels.chroma_offset,
                       cmd->ref_picture_buffer[j].pixels.chroma_offset +
                       (uint32_t) Stream->GetDecodeBufferManager()->ChromaSize(BufferState[BufferIndex].Buffer, VideoDecodeCopy));
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d PPB  %08x -> %08x\n",
                       ParsedFrameParameters->DecodeFrameIndex, j,
                       cmd->ref_picture_buffer[j].ppb_offset,
                       cmd->ref_picture_buffer[j].ppb_offset +
                       (uint32_t) Stream->GetDecodeBufferManager()->ComponentSize(BufferState[BufferIndex].Buffer, VideoMacroblockStructure));
            NumRefPic++;
        }
    }

    // initial_ref_pic_list_l0 is filled up by repeating the reference indexes (see standard section 8.3.4)
    if (DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount != 0)
        for (i = DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount; i < HEVC_MAX_REFERENCE_INDEX; i++)
        {
            cmd->initial_ref_pic_list_l0[i] = cmd->initial_ref_pic_list_l0[i % DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount];
        }

    for (i = 0; i < DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].EntryCount; i++)
    {
        // Is reference picture already in array ?
        for (j = 0; j < NumRefPic; j++)
            if (cmd->ref_picture_info[j].poc == DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].ReferenceDetails[i].PicOrderCnt)
            {
                break;
            }

        cmd->initial_ref_pic_list_l1[i] = j;

        // Store info if this is a new reference picture
        if (j == NumRefPic)
        {
            unsigned int BufferIndex = DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].EntryIndicies[i];
            cmd->ref_picture_buffer[j].pixels.luma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Luma(BufferState[BufferIndex].Buffer, VideoDecodeCopy);
            cmd->ref_picture_buffer[j].pixels.chroma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Chroma(BufferState[BufferIndex].Buffer, VideoDecodeCopy);
            cmd->ref_picture_buffer[j].pixels.stride = Stream->GetDecodeBufferManager()->ComponentStride(BufferState[BufferIndex].Buffer, VideoDecodeCopy, 0, 0);
            cmd->ref_picture_buffer[j].pixels.raster2b_not_omega4 = false;
            cmd->ref_picture_buffer[j].ppb_offset = (uint32_t) Stream->GetDecodeBufferManager()->ComponentBaseAddress(BufferState[BufferIndex].Buffer, VideoMacroblockStructure);

            if ((cmd->ref_picture_buffer[j].pixels.luma_offset != 0) && (cmd->ref_picture_buffer[j].pixels.chroma_offset != 0))
            {
                cmd->ref_picture_buffer[j].enable_flag = true;
                cmd->ref_picture_info[j].non_existing_flag = 0;
            }
            else
            {
                SE_WARNING("New reference buffer has either or both luma and chroma pointers NULL. Marking ref[%d] as unavailable.\n", j);
                cmd->ref_picture_buffer[j].enable_flag = false;
                cmd->ref_picture_info[j].non_existing_flag = 1;
            }

            cmd->ref_picture_info[j].long_term_flag = DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].ReferenceDetails[i].LongTermReference;
            cmd->ref_picture_info[j].poc = DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].ReferenceDetails[i].PicOrderCnt;
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d L     %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, j, cmd->ref_picture_buffer[j].pixels.luma_offset,
                       cmd->ref_picture_buffer[j].pixels.luma_offset + (uint32_t) Stream->GetDecodeBufferManager()->LumaSize(BufferState[BufferIndex].Buffer, VideoDecodeCopy));
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d C     %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, j, cmd->ref_picture_buffer[j].pixels.chroma_offset,
                       cmd->ref_picture_buffer[j].pixels.chroma_offset + (uint32_t) Stream->GetDecodeBufferManager()->ChromaSize(BufferState[BufferIndex].Buffer, VideoDecodeCopy));
            SE_VERBOSE(group_decoder_video, "DEC %d: refpic %d PPB   %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, j, cmd->ref_picture_buffer[j].ppb_offset,
                       cmd->ref_picture_buffer[j].ppb_offset + (uint32_t) Stream->GetDecodeBufferManager()->ComponentSize(BufferState[BufferIndex].Buffer, VideoMacroblockStructure));
            NumRefPic++;
        }
    }

    // initial_ref_pic_list_l1 is filled up by repeating the reference indexes (see standard section 8.3.4)
    if (DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].EntryCount != 0)
        for (i = DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].EntryCount; i < HEVC_MAX_REFERENCE_INDEX; i++)
        {
            cmd->initial_ref_pic_list_l1[i] = cmd->initial_ref_pic_list_l1[i % DecodeContext->ReferenceFrameList[REF_PIC_LIST_1].EntryCount];
        }

    // The preprocessor is generating ctb commands for decode using
    // ReferenceFrameList[REF_PIC_LIST_0].EntryCount number of
    // references so same number of references must be sent to hades
    // to avoid firmware crash
    if (DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount != 0)
    {
        cmd->num_reference_pictures = DecodeContext->ReferenceFrameList[REF_PIC_LIST_0].EntryCount;
    }
    else
    {
        // We don't know how many references are actually needed to
        // decode the frame so we put max value to avoid firmware hang
        if (ParsedFrameParameters->PictureHasMissingRef)
        {
            cmd->num_reference_pictures = HEVC_MAX_REFERENCE_PICTURES;
        }
        else
        {
            // There are no valid references and there are no missing
            // references as well. This normally does not happen in P
            // and B pictures so set max value for them to avoid
            // firmware hang. Set value 0 for I (no references).
            if (ParsedVideoParameters->SliceType != SliceTypeI)
            {
                cmd->num_reference_pictures = HEVC_MAX_REFERENCE_PICTURES;
            }
            else
            {
                cmd->num_reference_pictures = 0;
            }
        }
    }

    // Find best reference for ERC emulation
    for (i = 0; i < NumRefPic; i++)
    {
        abs_diff_poc = cmd->ref_picture_info[i].poc >= sliceheader->PicOrderCnt ? cmd->ref_picture_info[i].poc - sliceheader->PicOrderCnt : sliceheader->PicOrderCnt - cmd->ref_picture_info[i].poc;

        if (i == 0)
        {
            min_abs_diff_poc = abs_diff_poc;
        }
        else if (abs_diff_poc < min_abs_diff_poc)
        {
            min_abs_diff_poc = abs_diff_poc;
            cmd->erc_ref_pic_idx = i;
        }
    }

    // Current picture
#if 0 // WA for now Temporal scalability is not yet supported (see bug 46850), the following code line must be re-activated asap bug 50803 will be closed
    cmd->curr_picture_buffer.enable_flag = sliceheader->reference_flag ||
                                           ((unsigned int) sliceheader->temporal_id < (unsigned int) sps->sps_max_sub_layers - 1); // enable Omega RCN only if the picture is a reference
#else
    cmd->curr_picture_buffer.enable_flag = sliceheader->reference_flag;  // enable Omega RCN only if the picture is a reference
#endif
    cmd->curr_picture_buffer.pixels.raster2b_not_omega4 = false;
    cmd->curr_picture_buffer.pixels.luma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy);
    cmd->curr_picture_buffer.pixels.chroma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy);
    cmd->curr_picture_buffer.pixels.stride = Stream->GetDecodeBufferManager()->ComponentStride(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy, 0, 0);
    cmd->curr_picture_buffer.ppb_offset = (uint32_t) Stream->GetDecodeBufferManager()->ComponentBaseAddress(BufferState[CurrentDecodeBufferIndex].Buffer, VideoMacroblockStructure);
    cmd->curr_picture_info.non_existing_flag = 0;
    cmd->curr_picture_info.long_term_flag = 0;
    cmd->curr_picture_info.poc = sliceheader->PicOrderCnt;
    SE_VERBOSE(group_decoder_video, "DEC %d: picbuf L      %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, cmd->curr_picture_buffer.pixels.luma_offset,
               cmd->curr_picture_buffer.pixels.luma_offset + (uint32_t) Stream->GetDecodeBufferManager()->LumaSize(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy));
    SE_VERBOSE(group_decoder_video, "DEC %d: picbuf C      %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, cmd->curr_picture_buffer.pixels.chroma_offset,
               cmd->curr_picture_buffer.pixels.chroma_offset + (uint32_t) Stream->GetDecodeBufferManager()->ChromaSize(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy));
    SE_VERBOSE(group_decoder_video, "DEC %d: picbuf PPB    %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, cmd->curr_picture_buffer.ppb_offset,
               cmd->curr_picture_buffer.ppb_offset + (uint32_t) Stream->GetDecodeBufferManager()->ComponentSize(BufferState[CurrentDecodeBufferIndex].Buffer, VideoMacroblockStructure));

#ifdef WA_PPB_OUT_NON_REF
    if (! cmd->curr_picture_buffer.enable_flag)
    {
        cmd->curr_picture_buffer.ppb_offset = (uint32_t) mFakePPBAddress;
        SE_VERBOSE(group_decoder_video, "DEC %d: fake PPB      %08x -> %08x\n",
                   ParsedFrameParameters->DecodeFrameIndex, cmd->curr_picture_buffer.ppb_offset,
                   cmd->curr_picture_buffer.ppb_offset + mFakePPBAllocator->BufferSize);
    }

#endif
    // Display buffer
    cmd->curr_display_buffer.enable_flag = sliceheader->output_flag;
    cmd->curr_display_buffer.resize_flag = 0;
    cmd->curr_display_buffer.pixels.raster2b_not_omega4 = true;
    cmd->curr_display_buffer.pixels.stride = Stream->GetDecodeBufferManager()->ComponentStride(
                                                 BufferState[CurrentDecodeBufferIndex].Buffer,
                                                 PrimaryManifestationComponent,
                                                 0, 0);
    cmd->curr_display_buffer.pixels.luma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent);
    cmd->curr_display_buffer.pixels.chroma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent);
    SE_VERBOSE(group_decoder_video, "DEC %d: dispbuf L     %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, cmd->curr_display_buffer.pixels.luma_offset,
               cmd->curr_display_buffer.pixels.luma_offset + (uint32_t) Stream->GetDecodeBufferManager()->LumaSize(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent));
    SE_VERBOSE(group_decoder_video, "DEC %d: dispbuf C     %08x -> %08x\n", ParsedFrameParameters->DecodeFrameIndex, cmd->curr_display_buffer.pixels.chroma_offset,
               cmd->curr_display_buffer.pixels.chroma_offset + (uint32_t) Stream->GetDecodeBufferManager()->ChromaSize(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent));

    // Resize buffer
    if (Stream->GetDecodeBufferManager()->ComponentPresent(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent))
    {
        unsigned int vdec;
        unsigned int hdec;
        cmd->curr_resize_buffer.enable_flag = cmd->curr_display_buffer.enable_flag;
        cmd->curr_resize_buffer.enable_flag = true; // JLX DEBUG SIRON: VSOC 2.1.9 model hangs when decimation is enabled
        cmd->curr_resize_buffer.resize_flag = true;
        cmd->curr_resize_buffer.pixels.raster2b_not_omega4 = true;
        cmd->curr_resize_buffer.pixels.stride = Stream->GetDecodeBufferManager()->ComponentStride(
                                                    BufferState[CurrentDecodeBufferIndex].Buffer,
                                                    DecimatedManifestationComponent,
                                                    0, 0);
        cmd->curr_resize_buffer.pixels.luma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent);
        cmd->curr_resize_buffer.pixels.chroma_offset = (uint32_t) Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent);
        hdec = Stream->GetDecodeBufferManager()->DecimationFactor(BufferState[CurrentDecodeBufferIndex].Buffer, 0);
        vdec = Stream->GetDecodeBufferManager()->DecimationFactor(BufferState[CurrentDecodeBufferIndex].Buffer, 1);
        SE_VERBOSE(group_decoder_video, "DEC %d: rszbuf L      %08x -> %08x\n",
                   ParsedFrameParameters->DecodeFrameIndex,
                   cmd->curr_resize_buffer.pixels.luma_offset,
                   cmd->curr_resize_buffer.pixels.luma_offset +
                   (uint32_t) Stream->GetDecodeBufferManager()->LumaSize(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent));
        SE_VERBOSE(group_decoder_video, "DEC %d: rszbuf C      %08x -> %08x\n",
                   ParsedFrameParameters->DecodeFrameIndex,
                   cmd->curr_resize_buffer.pixels.chroma_offset,
                   cmd->curr_resize_buffer.pixels.chroma_offset +
                   (uint32_t) Stream->GetDecodeBufferManager()->ChromaSize(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent));

        // Not all these decimations factors are supported by the Streaming Engine
        if (vdec == 2)
        {
            cmd->v_dec_factor = 1;
        }
        else if (vdec == 4)
        {
            cmd->v_dec_factor = 2;
        }
        else if (vdec == 8)
        {
            cmd->v_dec_factor = 3;
        }
        else
        {
            cmd->v_dec_factor = 0;
        }

        if (hdec == 2)
        {
            cmd->h_dec_factor = 1;
        }
        else if (hdec == 4)
        {
            cmd->h_dec_factor = 2;
        }
        else if (hdec == 8)
        {
            cmd->h_dec_factor = 3;
        }
        else
        {
            cmd->h_dec_factor = 0;
        }

        // Default resize coefficients
        cmd->luma_resize_param.v_dec_coeff.round = 128;
        cmd->luma_resize_param.v_dec_coeff.coeff[0] = 128;
        cmd->luma_resize_param.v_dec_coeff.coeff[1] = 128;
        cmd->luma_resize_param.v_dec_coeff.coeff[2] = 0;
        cmd->luma_resize_param.v_dec_coeff.coeff[3] = 0;
        cmd->luma_resize_param.v_dec_coeff.coeff[4] = 0;
        cmd->luma_resize_param.v_dec_coeff.coeff[5] = 0;
        cmd->luma_resize_param.v_dec_coeff.coeff[6] = 0;
        cmd->luma_resize_param.v_dec_coeff.coeff[7] = 0;
        cmd->luma_resize_param.h_flt_coeff.round = 128;
        cmd->luma_resize_param.h_flt_coeff.coeff[0] = 8;
        cmd->luma_resize_param.h_flt_coeff.coeff[1] = -14;
        cmd->luma_resize_param.h_flt_coeff.coeff[2] = 18;
        cmd->luma_resize_param.h_flt_coeff.coeff[3] = 236;
        cmd->luma_resize_param.h_flt_coeff.coeff[4] = 18;
        cmd->luma_resize_param.h_flt_coeff.coeff[5] = -14;
        cmd->luma_resize_param.h_flt_coeff.coeff[6] = 8;
        cmd->luma_resize_param.h_flt_coeff.coeff[7] = -4;
        cmd->chroma_resize_param.v_dec_coeff.round = 128;
        cmd->chroma_resize_param.v_dec_coeff.coeff[0] = 128;
        cmd->chroma_resize_param.v_dec_coeff.coeff[1] = 128;
        cmd->chroma_resize_param.v_dec_coeff.coeff[2] = 0;
        cmd->chroma_resize_param.v_dec_coeff.coeff[3] = 0;
        cmd->chroma_resize_param.v_dec_coeff.coeff[4] = 0;
        cmd->chroma_resize_param.v_dec_coeff.coeff[5] = 0;
        cmd->chroma_resize_param.v_dec_coeff.coeff[6] = 0;
        cmd->chroma_resize_param.v_dec_coeff.coeff[7] = 0;
        cmd->chroma_resize_param.h_flt_coeff.round = 128;
        cmd->chroma_resize_param.h_flt_coeff.coeff[0] = 8;
        cmd->chroma_resize_param.h_flt_coeff.coeff[1] = -14;
        cmd->chroma_resize_param.h_flt_coeff.coeff[2] = 18;
        cmd->chroma_resize_param.h_flt_coeff.coeff[3] = 236;
        cmd->chroma_resize_param.h_flt_coeff.coeff[4] = 18;
        cmd->chroma_resize_param.h_flt_coeff.coeff[5] = -14;
        cmd->chroma_resize_param.h_flt_coeff.coeff[6] = 8;
        cmd->chroma_resize_param.h_flt_coeff.coeff[7] = -4;
    }
    else
    {
        cmd->curr_resize_buffer.enable_flag = 0;
        cmd->curr_resize_buffer.resize_flag = 0;
        cmd->curr_resize_buffer.pixels.raster2b_not_omega4 = 0;
        cmd->curr_resize_buffer.pixels.stride = 0;
        cmd->curr_resize_buffer.pixels.luma_offset = 0;
        cmd->curr_resize_buffer.pixels.chroma_offset = 0;
        cmd->v_dec_factor = 0;
        cmd->h_dec_factor = 0;
    }
    // TODO prog task timeout (unit: 4096 ticks on CLK_FC_HADES at 332.9Mhz ~= 12us)
#ifndef HEVC_HADES_CUT1
    cmd->time_out_in_tick = 4052; /* 50 ms FC Timeout */
#else
    cmd->time_out_in_tick = 0;
#endif

    // Intermediate buffers
    cmd->intermediate_buffer.slice_table_base_addr = (uint32_t) InputBuffer[HEVC_SLICE_TABLE];
    cmd->intermediate_buffer.slice_table_length = (uint32_t) InputBufferSize[HEVC_SLICE_TABLE];
    cmd->intermediate_buffer.ctb_table_base_addr = (uint32_t) InputBuffer[HEVC_CTB_TABLE];
    cmd->intermediate_buffer.ctb_table_length = (uint32_t) InputBufferSize[HEVC_CTB_TABLE];
    cmd->intermediate_buffer.slice_headers_base_addr = (uint32_t) InputBuffer[HEVC_SLICE_HEADERS];
    cmd->intermediate_buffer.slice_headers_length = (uint32_t) InputBufferSize[HEVC_SLICE_HEADERS];
    cmd->intermediate_buffer.ctb_commands.base_addr = (uint32_t) InputBuffer[HEVC_CTB_COMMANDS];
    cmd->intermediate_buffer.ctb_commands.length = (uint32_t) InputBufferSize[HEVC_CTB_COMMANDS];
#ifndef HEVC_HADES_CUT1
    cmd->intermediate_buffer.ctb_commands.end_offset = (uint32_t) InputBufferSize[HEVC_CTB_COMMANDS];
#endif
    cmd->intermediate_buffer.ctb_commands.start_offset = 0;
    cmd->intermediate_buffer.ctb_residuals.base_addr = (uint32_t) InputBuffer[HEVC_CTB_RESIDUALS];
    cmd->intermediate_buffer.ctb_residuals.length = (uint32_t) InputBufferSize[HEVC_CTB_RESIDUALS];
#ifndef HEVC_HADES_CUT1
    cmd->intermediate_buffer.ctb_residuals.end_offset = (uint32_t) InputBufferSize[HEVC_CTB_RESIDUALS];
#endif
    cmd->intermediate_buffer.ctb_residuals.start_offset = 0;
    cmd->slice_table_entries = mSliceTableEntries;
    SE_VERBOSE(group_decoder_video, "DEC %d: slice_table   %08x -> %08x\n",
               ParsedFrameParameters->DecodeFrameIndex, cmd->intermediate_buffer.slice_table_base_addr,
               cmd->intermediate_buffer.slice_table_base_addr +   cmd->intermediate_buffer.slice_table_length);
    SE_VERBOSE(group_decoder_video, "DEC %d: ctb_table     %08x -> %08x\n",
               ParsedFrameParameters->DecodeFrameIndex, cmd->intermediate_buffer.ctb_table_base_addr,
               cmd->intermediate_buffer.ctb_table_base_addr +  cmd->intermediate_buffer.ctb_table_length);
    SE_VERBOSE(group_decoder_video, "DEC %d: slice_headers %08x -> %08x\n",
               ParsedFrameParameters->DecodeFrameIndex, cmd->intermediate_buffer.slice_headers_base_addr,
               cmd->intermediate_buffer.slice_headers_base_addr + cmd->intermediate_buffer.slice_headers_length);
    SE_VERBOSE(group_decoder_video, "DEC %d: ctb_commands  %08x -> %08x\n",
               ParsedFrameParameters->DecodeFrameIndex, cmd->intermediate_buffer.ctb_commands.base_addr,
               cmd->intermediate_buffer.ctb_commands.base_addr +  cmd->intermediate_buffer.ctb_commands.length);
    SE_VERBOSE(group_decoder_video, "DEC %d: ctb_residuals %08x -> %08x\n",
               ParsedFrameParameters->DecodeFrameIndex, cmd->intermediate_buffer.ctb_residuals.base_addr,
               cmd->intermediate_buffer.ctb_residuals.base_addr + cmd->intermediate_buffer.ctb_residuals.length);
    // SE_DEBUG(group_decoder_video, "DEC: %d %d %d %d %d\n",
    // cmd->intermediate_buffer.slice_table_length,
    // cmd->intermediate_buffer.ctb_table_length,
    // cmd->intermediate_buffer.slice_headers_length,
    // cmd->intermediate_buffer.ctb_commands.length,
    // cmd->intermediate_buffer.ctb_residuals.length);
    // MCC programming
    cmd->mcc_mode   = HEVC_MCC_MODE_CACHE;
    cmd->mcc_opcode = HEVC_MCC_OPCODE_32;
#ifdef HEVC_HADES_CUT1
    // Soft resets
    cmd->ctbe_host_soft_reset = 0;
    cmd->ctbe_fc_soft_reset   = 0;
    cmd->pp_host_soft_reset   = 0;
    cmd->decoding_mode = HEVC_NORMAL_DECODE_WITHOUT_ERROR_RECOVERY;
#else
    cmd->decoding_mode = HEVC_ERC_MAX_DECODE;
#endif
    // PPS & SPS elements
    cmd->first_picture_in_sequence_flag = !sps->is_active;
    sps->is_active = true;
    cmd->level_idc = sps->level_idc != 0 ? sps->level_idc : HADES_MAX_LEVEL_IDC;
    cmd->loop_filter_across_tiles_enabled_flag = pps->loop_filter_across_tiles_enabled_flag;
    cmd->additional_flags = HEVC_ADDITIONAL_FLAG_NONE;
    cmd->pcm_loop_filter_disable_flag = sps->pcm_loop_filter_disable_flag;
    cmd->sps_temporal_mvp_enabled_flag = sps->sps_temporal_mvp_enabled_flag;
    cmd->log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2;
    cmd->pic_width_in_luma_samples = sps->pic_width_in_luma_samples;
    cmd->pic_height_in_luma_samples = sps->pic_height_in_luma_samples;

    if (sps->pcm_enabled_flag)
    {
        cmd->pcm_bit_depth_luma_minus1 = sps->pcm_bit_depth_luma_minus1;
        cmd->pcm_bit_depth_chroma_minus1 = sps->pcm_bit_depth_chroma_minus1;
    }
    else
    {
        cmd->pcm_bit_depth_luma_minus1 = 0;
        cmd->pcm_bit_depth_chroma_minus1 = 0;
    }

#ifndef HEVC_HADES_CUT1
    cmd->sps_max_dec_pic_buffering_minus1 = sps->sps_max_dec_pic_buffering[0] - 1;
#endif
    cmd->log2_max_coding_block_size_minus3 = sps->log2_max_coding_block_size - 3;
    cmd->scaling_list_enable = sps->scaling_list_enable_flag;
    cmd->transform_skip_enabled_flag = pps->transform_skip_enabled_flag;
    cmd->lists_modification_present_flag = pps->lists_modification_present_flag;
    cmd->num_tile_columns = pps->num_tile_columns;
    cmd->num_tile_rows = pps->num_tile_rows;

    for (i = 0; i < cmd->num_tile_columns; i++)
    {
        cmd->tile_column_width[i] = pps->tile_width[i];
    }

    for (; i < HEVC_MAX_TILE_COLUMNS; i++)
    {
        cmd->tile_column_width[i] = 0;
    }

    for (i = 0; i < cmd->num_tile_rows; i++)
    {
        cmd->tile_row_height[i] = pps->tile_height[i];
    }

    for (; i < HEVC_MAX_TILE_ROWS; i++)
    {
        cmd->tile_row_height[i] = 0;
    }

    //when sign_data_hiding_flag is true, sign_data_hiding_threshold is inferred to 4
    cmd->sign_data_hiding_flag = pps->sign_data_hiding_flag;
    cmd->weighted_pred_flag = pps->weighted_pred_flag[P_SLICE];
    cmd->weighted_bipred_flag = pps->weighted_pred_flag[B_SLICE];
    cmd->constrained_intra_pred_flag = pps->constrained_intra_pred_flag;
    cmd->strong_intra_smoothing_enable_flag = sps->sps_strong_intra_smoothing_enable_flag;
    cmd->pps_cb_qp_offset = pps->pps_cb_qp_offset;
    cmd->pps_cr_qp_offset = pps->pps_cr_qp_offset;
    DumpDecodeParameters(cmd, ParsedFrameParameters->DecodeFrameIndex);
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(hevcdecpix_status_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(hevcdecpix_transform_param_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);
    // Send decode index, IDR flag, poc and cached addresses thru a MME buffer
    // Used by CRC & soft CRC system in the Hades driver
    Context->BaseContext.MMECommand.NumberInputBuffers = 1;
    Context->BaseContext.MMECommand.NumberOutputBuffers = 0;
    Context->BaseContext.MMECommand.DataBuffers_p = Context->BaseContext.MMEBufferList;
    Context->BaseContext.MMEBufferList[0] = &(Context->BaseContext.MMEBuffers[0]);
    Context->BaseContext.MMEBuffers[0].StructSize = sizeof(MME_DataBuffer_t);
    Context->BaseContext.MMEBuffers[0].UserData_p = NULL;
    Context->BaseContext.MMEBuffers[0].Flags = 0;
    Context->BaseContext.MMEBuffers[0].StreamNumber = 0;
    Context->BaseContext.MMEBuffers[0].NumberOfScatterPages = 1;
    Context->BaseContext.MMEBuffers[0].ScatterPages_p = &(Context->BaseContext.MMEPages[0]);
    Context->BaseContext.MMEBuffers[0].StartOffset = 0;
    Context->BaseContext.MMEBuffers[0].TotalSize = sizeof(HevcCodecExtraCRCInfo_t);
    Context->BaseContext.MMEPages[0].Page_p = & (Context->ExtraCRCInfo);
    Context->BaseContext.MMEPages[0].Size = sizeof(HevcCodecExtraCRCInfo_t);
    Context->BaseContext.MMEPages[0].BytesUsed = sizeof(HevcCodecExtraCRCInfo_t);
    Context->BaseContext.MMEPages[0].FlagsIn = MME_DATA_PHYSICAL;
    Context->BaseContext.MMEPages[0].FlagsOut = MME_DATA_PHYSICAL;
    Context->ExtraCRCInfo.DecodeIndex = ParsedFrameParameters->DecodeFrameIndex;
    Context->ExtraCRCInfo.IDR = sliceheader->idr_flag;
    Context->ExtraCRCInfo.poc = sliceheader->PicOrderCnt;

    if (sliceheader->reference_flag)
    {
        Context->ExtraCRCInfo.omega_luma   = Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy, CachedAddress);
        Context->ExtraCRCInfo.omega_chroma = Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, VideoDecodeCopy, CachedAddress);
        Context->ExtraCRCInfo.ppb = (uint8_t *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(BufferState[CurrentDecodeBufferIndex].Buffer, VideoMacroblockStructure, CachedAddress);
    }
    else
    {
        Context->ExtraCRCInfo.omega_luma = Context->ExtraCRCInfo.omega_chroma = Context->ExtraCRCInfo.ppb = NULL;
    }

    Context->ExtraCRCInfo.omega_luma_size = ParsedVideoParameters->Content.DecodeWidth * ParsedVideoParameters->Content.DecodeHeight;
    Context->ExtraCRCInfo.ppb_size = ((ParsedVideoParameters->Content.DecodeWidth + 0x3f) & 0xFFFFFFC0) *
                                     ((ParsedVideoParameters->Content.DecodeHeight + 0x3f) & 0xFFFFFFC0) / 256 * 16; // worst case 64x64 CTBs, 4 words per 16x16 block
    Context->ExtraCRCInfo.raster_luma   = Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent, CachedAddress);
    Context->ExtraCRCInfo.raster_chroma = Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent, CachedAddress);
    Context->ExtraCRCInfo.raster_stride = Stream->GetDecodeBufferManager()->ComponentStride(BufferState[CurrentDecodeBufferIndex].Buffer, PrimaryManifestationComponent, 0, 0);
    Context->ExtraCRCInfo.raster_width  = ParsedVideoParameters->Content.DisplayWidth;
    Context->ExtraCRCInfo.raster_height = ParsedVideoParameters->Content.DisplayHeight;

    if (Stream->GetDecodeBufferManager()->ComponentPresent(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent))
    {
        Context->ExtraCRCInfo.raster_decimated_luma =   Stream->GetDecodeBufferManager()->Luma(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent, CachedAddress);
        Context->ExtraCRCInfo.raster_decimated_chroma = Stream->GetDecodeBufferManager()->Chroma(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent, CachedAddress);
        Context->ExtraCRCInfo.raster_decimated_stride = Stream->GetDecodeBufferManager()->ComponentStride(BufferState[CurrentDecodeBufferIndex].Buffer, DecimatedManifestationComponent, 0, 0);
        Context->ExtraCRCInfo.raster_decimated_width =  ParsedVideoParameters->Content.DisplayWidth  / Stream->GetDecodeBufferManager()->DecimationFactor(BufferState[CurrentDecodeBufferIndex].Buffer, 0);
        Context->ExtraCRCInfo.raster_decimated_height = ParsedVideoParameters->Content.DisplayHeight / Stream->GetDecodeBufferManager()->DecimationFactor(BufferState[CurrentDecodeBufferIndex].Buffer, 1);
    }
    else
    {
        Context->ExtraCRCInfo.raster_decimated_luma =   NULL;
        Context->ExtraCRCInfo.raster_decimated_chroma = NULL;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The intermediate process, taking output from the pre-processor
//      and feeding it to the lower level of the codec.
//

OS_TaskEntry(Codec_MmeVideoHevc_IntermediateProcess)
{
    Codec_MmeVideoHevc_c *Codec = (Codec_MmeVideoHevc_c *)Parameter;
    Codec->IntermediateProcess();
    OS_TerminateThread();
    return NULL;
}

// ----------------------

void Codec_MmeVideoHevc_c::IntermediateProcess()
{
    CodecStatus_t                   Status;
    BufferStatus_t                  BufStatus;
    hevcpp_status_t                 PPStatus;
    struct hevcpp_ioctl_dequeue_t   PPEntry;
    unsigned char                   AllowBadPPFrames;
    bool                            MovingToDiscardUntilNewFrame;
    bool                            DiscardUntilNewFrame;
    Buffer_t                        PreProcessorBuffer;
    unsigned int                    i;
    HevcFramesInPreprocessorChain_t ppFrame;
    bool                            Terminating = false;

    //
    // Main loop
    //
    MovingToDiscardUntilNewFrame = false;
    DiscardUntilNewFrame = false;

    while (!Terminating)
    {
        // Blocking call to get next ppFrame
        if (0 > mPpFramesRing->Extract(&ppFrame))
        {
            continue;
        }

        //
        // If SE is in low power state, process should keep inactive, as we are not allowed to issue any MME commands
        //
        if (Stream && Stream->IsLowPowerState())
        {
            ppFrame.Action = HevcActionNull;
        }

        //
        // Process activity - note aborted activity differs in consequences for each action.
        //
        switch (ppFrame.Action)
        {
        case HevcActionTermination:
            Terminating = true;
            continue;

        case HevcActionNull:
            break;

        case HevcActionCallOutputPartialDecodeBuffers:
            if (Stream)
            {
                Codec_MmeVideo_c::OutputPartialDecodeBuffers();
            }
            break;

        case HevcActionCallDiscardQueuedDecodes:
            DiscardFramesInPreprocessorChain = false;
            Codec_MmeVideo_c::DiscardQueuedDecodes();
            break;

        case HevcActionCallReleaseReferenceFrame:
            Codec_MmeVideo_c::ReleaseReferenceFrame(ppFrame.DecodeFrameIndex);
            break;

        case HevcActionPassOnPreProcessedFrame:
            PPStatus = HevcppGetPreProcessedBuffer(mPreProcessorDevice, &PPEntry);

            if (PPStatus == hevcpp_error)
            {
                SE_ERROR("Failed to retrieve a buffer from the pre-processor\n");
            }

            if ((PPEntry.hwStatus == hevcpp_unrecoverable_error) || (PPEntry.hwStatus == hevcpp_timeout))
            {
                MovingToDiscardUntilNewFrame = true;
                SE_WARNING("Discarding picture with pre-processing timeout/unrecoverable pre-processing error\n");
            }
            else if ((PPEntry.hwStatus == hevcpp_recoverable_error))
            {
                AllowBadPPFrames  = Player->PolicyValue(Playback, Stream, PolicyAllowBadHevcPreProcessedFrames);

                if (AllowBadPPFrames != PolicyValueApply)
                {
                    SE_WARNING("Due to policy enforcement discarding picture with recoverable pre-processing error\n");
                    MovingToDiscardUntilNewFrame    = true;
                }
            }

            if (Stream)
            {
#ifdef HEVC_HADES_CUT1
                if (PPEntry.iStatus.error & HEVCPP_ERROR_STATUS_STARTCODE_DETECTED)
                {
                    Stream->Statistics().HevcPreprocErrorSCDetected ++;
                }
                if (PPEntry.iStatus.error & HEVCPP_ERROR_STATUS_END_OF_STREAM_ISSUE)
                {
                    Stream->Statistics().HevcPreprocErrorEOS ++;
                }
                if (PPEntry.iStatus.error & HEVCPP_ERROR_STATUS_END_OF_DMA)
                {
                    Stream->Statistics().HevcPreprocErrorEndOfDma ++;
                }
                if (PPEntry.iStatus.error & HEVCPP_ERROR_STATUS_RANGE_ERROR)
                {
                    Stream->Statistics().HevcPreprocErrorRange ++;
                }
                if (PPEntry.iStatus.error & HEVCPP_ERROR_STATUS_ENTROPY_DECODE_ERROR)
                {
                    Stream->Statistics().HevcPreprocErrorEntropyDecode ++;
                }
#endif
            }
            ppFrame.CodedBuffer->SetUsedDataSize(0);
            BufStatus = ppFrame.CodedBuffer->ShrinkBuffer(0);
            if (BufStatus != BufferNoError)
            {
                SE_INFO(group_decoder_video, "Failed to shrink buffer\n");
            }

            if (ppFrame.ParsedFrameParameters->FirstParsedParametersForOutputFrame)
            {
                DiscardUntilNewFrame = false;
            }

            if (DiscardFramesInPreprocessorChain || MovingToDiscardUntilNewFrame || DiscardUntilNewFrame)
            {
                if (Stream)
                {
                    if (ppFrame.ParsedFrameParameters->FirstParsedParametersForOutputFrame)
                    {
                        Stream->ParseToDecodeEdge->RecordNonDecodedFrame(ppFrame.CodedBuffer, ppFrame.ParsedFrameParameters);
                    }
                    else
                    {
                        Codec_MmeVideo_c::OutputPartialDecodeBuffers();
                    }
                }

                for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
                {
                    ppFrame.CodedBuffer->DetachBuffer(ppFrame.PreProcessorBuffer[i]);    // This should release the pre-processor buffer
                }

                if (MovingToDiscardUntilNewFrame)
                {
                    DiscardUntilNewFrame        = true;
                    MovingToDiscardUntilNewFrame    = false;
                }
                break;
            }

            // Shrink intermediate buffers
            // Caution: do not shrink to zero otherwise the next ObtainDataReference() will return NULL
#define SHRINK_IB(INDEX, LENGTH) \
            {                                                \
                int new_length = PPEntry.iStatus.LENGTH;     \
                if (new_length == 0)                         \
                    new_length = 4; /* keep the buffer allocated */                \
                if (ppFrame.PreProcessorBuffer[INDEX]) {                          \
                ppFrame.PreProcessorBuffer[INDEX]->SetUsedDataSize( new_length ); \
                ppFrame.PreProcessorBuffer[INDEX]->ShrinkBuffer( new_length );    \
                }                                                                  \
            }
            SHRINK_IB(HEVC_SLICE_TABLE,   slice_table_stop_offset);
            SHRINK_IB(HEVC_CTB_TABLE,     ctb_table_stop_offset);
            SHRINK_IB(HEVC_SLICE_HEADERS, slice_headers_stop_offset);
            SHRINK_IB(HEVC_CTB_COMMANDS,  ctb_commands_stop_offset);
            SHRINK_IB(HEVC_CTB_RESIDUALS, ctb_residuals_stop_offset);

            mSliceTableEntries = PPEntry.iStatus.slice_table_entries;
            SE_DEBUG(group_decoder_video,
                     "slice_table_stop_offset:%d ctb_table_stop_offset:%d "
                     "slice_headers_stop_offset:%d ctb_commands_stop_offset:%d "
                     "ctb_residuals_stop_offset:%d slice_table_entries:%d error:0x%08x\n",
                     PPEntry.iStatus.slice_table_stop_offset,
                     PPEntry.iStatus.ctb_table_stop_offset,
                     PPEntry.iStatus.slice_headers_stop_offset,
                     PPEntry.iStatus.ctb_commands_stop_offset,
                     PPEntry.iStatus.ctb_residuals_stop_offset,
                     PPEntry.iStatus.slice_table_entries,
                     PPEntry.iStatus.error);

            mPpFramesRing->signalFakeTaskPending();
        // Fall through

        case HevcActionPassOnFrame:
            //
            // Now mimic the input procedure as done in the player process
            //
            // Clear the NewStreamParameters flag so base class does not send a Set Stream Parameter MME command
            ppFrame.ParsedFrameParameters->NewStreamParameters = false;
            SE_VERBOSE(group_decoder_video, ">DEC IB input decodeIdx: %d\n",
                       ppFrame.ParsedFrameParameters->DecodeFrameIndex);

            Status = Codec_MmeVideo_c::Input(ppFrame.CodedBuffer);
            if (Status != CodecNoError)
            {
                if (Stream && ppFrame.ParsedFrameParameters->FirstParsedParametersForOutputFrame)
                {
                    Stream->ParseToDecodeEdge->RecordNonDecodedFrame(ppFrame.CodedBuffer, ppFrame.ParsedFrameParameters);
                    Codec_MmeVideo_c::OutputPartialDecodeBuffers();
                }

                // If pre processor buffers are still attached to the coded buffer, make sure they get freed
                if (ppFrame.DecodeFrameIndex != INVALID_INDEX)
                {
                    for (i = 0; i < HEVC_NUM_INTERMEDIATE_BUFFERS; i++)
                    {
                        ppFrame.CodedBuffer->ObtainAttachedBufferReference(mPreProcessorBufferType[i], &PreProcessorBuffer);
                        if (PreProcessorBuffer != NULL)
                        {
                            ppFrame.CodedBuffer->DetachBuffer(PreProcessorBuffer);
                        }
                    }
                }

                DiscardUntilNewFrame    = true;
            }

            break;
        }

        // Release this entry
        if (ppFrame.ParsedFrameParameters)
        {
            SE_VERBOSE(group_decoder_video, "<DEC %d\n", ppFrame.ParsedFrameParameters->DecodeFrameIndex);
        }
        if (ppFrame.CodedBuffer)
        {
            ppFrame.CodedBuffer->DecrementReferenceCount(IdentifierHevcIntermediate);
        }

        mPpFramesRing->signalFakeTaskPending();
    }

    //
    // Clean up the ring
    //
    while (!mPpFramesRing->isEmpty())
    {
        mPpFramesRing->Extract(&ppFrame);

        if (ppFrame.CodedBuffer != NULL)
        {
            ppFrame.CodedBuffer->DecrementReferenceCount(IdentifierHevcIntermediate);
        }
        mPpFramesRing->signalFakeTaskPending();
    }

    SE_INFO(group_decoder_video, "Exiting HEVCIntermediateProcess\n");
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out a single preprocessor command for a HEVC coded
//      frame.
//

void Codec_MmeVideoHevc_c::FillOutPreprocessorCommand(hevcdecpreproc_transform_param_t     *cmd,
                                                      HevcSequenceParameterSet_t *sps, HevcPictureParameterSet_t *pps, uint32_t num_poc_total_curr, uint32_t DecodeFrameIndex)
{
    uint32_t i;
    cmd->max_slice = MAX_NUM_SLICES;
    cmd->num_poc_total_curr = num_poc_total_curr;
#ifndef HEVC_HADES_CUT1
    cmd->disable_hevc     = 0;
    cmd->disable_ovhd     = 0;
    cmd->rdplug_dma_1_cid = 0;
    cmd->wrplug_dma_1_cid = 0;
    cmd->wrplug_dma_2_cid = 0;
    cmd->wrplug_dma_3_cid = 0;
    cmd->wrplug_dma_4_cid = 0;
    cmd->wrplug_dma_5_cid = 0;
#endif
    // ---------------
    // SPS parameters
    cmd->chroma_format_idc = sps->chroma_format_idc;
    cmd->separate_colour_plane_flag = sps->separate_colour_plane_flag;
    cmd->pic_width_in_luma_samples = sps->pic_width_in_luma_samples;
    cmd->pic_height_in_luma_samples = sps->pic_height_in_luma_samples;
    cmd->pcm_enabled_flag = sps->pcm_enabled_flag;

    if (cmd->pcm_enabled_flag)
    {
        cmd->pcm_bit_depth_luma_minus1 = sps->pcm_bit_depth_luma_minus1;
        cmd->pcm_bit_depth_chroma_minus1 = sps->pcm_bit_depth_chroma_minus1;
        cmd->log2_min_pcm_coding_block_size_minus3 = sps->log2_min_pcm_coding_block_size - 3;
        cmd->log2_diff_max_min_pcm_coding_block_size = sps->log2_max_pcm_coding_block_size - sps->log2_min_pcm_coding_block_size;
    }
    else
    {
        cmd->pcm_bit_depth_luma_minus1 = 0;
        cmd->pcm_bit_depth_chroma_minus1 = 0;
        cmd->log2_min_pcm_coding_block_size_minus3 = 0;
        cmd->log2_diff_max_min_pcm_coding_block_size = 0;
    }

    cmd->log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb - 4;
    cmd->lists_modification_present_flag = pps->lists_modification_present_flag;
    cmd->log2_min_coding_block_size_minus3 = sps->log2_min_coding_block_size - 3;
    cmd->log2_diff_max_min_coding_block_size = sps->log2_max_coding_block_size - sps->log2_min_coding_block_size;
    cmd->log2_min_transform_block_size_minus2 = sps->log2_min_transform_block_size - 2;
    cmd->log2_diff_max_min_transform_block_size = sps->log2_max_transform_block_size - sps->log2_min_transform_block_size;
    cmd->max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter;
    cmd->max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra;
    cmd->transform_skip_enabled_flag = pps->transform_skip_enabled_flag;
    cmd->pps_loop_filter_across_slices_enabled_flag = pps->pps_loop_filter_across_slices_enabled_flag;
    cmd->amp_enabled_flag = sps->amp_enabled_flag;
    cmd->sample_adaptive_offset_enabled_flag = sps->sample_adaptive_offset_enabled_flag;
    cmd->num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets;
    cmd->long_term_ref_pics_present_flag = sps->long_term_ref_pics_present_flag;
    cmd->num_long_term_ref_pics_sps = sps->num_long_term_ref_pics_sps;
    cmd->sps_temporal_mvp_enabled_flag = sps->sps_temporal_mvp_enabled_flag;

    // Only the num_short_term_ref_pic_sets first elements are filled.
    // Unused values shall be set to 0
    for (i = 0; i < cmd->num_short_term_ref_pic_sets; i++)
    {
        cmd->num_delta_pocs[i] = sps->st_rps[i].rps_s0.num_pics + sps->st_rps[i].rps_s1.num_pics;
    }

    for (; i < HEVC_MAX_SHORT_TERM_RPS; i++)
    {
        cmd->num_delta_pocs[i] = 0;
    }

    // ---------------
    // PPS parameters
    cmd->dependent_slice_segments_enabled_flag = pps->dependent_slice_segments_enabled_flag;
    cmd->sign_data_hiding_flag = pps->sign_data_hiding_flag;
    cmd->cabac_init_present_flag = pps->cabac_init_present_flag;
    cmd->num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active - 1;
    cmd->num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active - 1;
    cmd->init_qp_minus26 = pps->init_qp_minus26;
    cmd->cu_qp_delta_enabled_flag = pps->cu_qp_delta_enabled_flag;
    cmd->diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth;
    cmd->weighted_pred_flag = pps->weighted_pred_flag[P_SLICE];
    cmd->weighted_bipred_flag = pps->weighted_pred_flag[B_SLICE];
    cmd->output_flag_present_flag = pps->output_flag_present_flag;
    cmd->deblocking_filter_control_present_flag = pps->deblocking_filter_control_present_flag;
    cmd->deblocking_filter_override_enabled_flag = pps->deblocking_filter_override_enabled_flag;
    cmd->pps_deblocking_filter_disable_flag = pps->pps_deblocking_filter_disable_flag;
    cmd->pps_beta_offset_div2 = pps->pps_beta_offset_div2;
    cmd->pps_tc_offset_div2 = pps->pps_tc_offset_div2;
    cmd->transquant_bypass_enable_flag = pps->transquant_bypass_enable_flag;
    cmd->tiles_enabled_flag = pps->tiles_enabled_flag;
    cmd->entropy_coding_sync_enabled_flag = pps->entropy_coding_sync_enabled_flag;
    //! Number of tiles in width inside the picture. Shall be >= 1
    cmd->num_tile_columns = pps->num_tile_columns;
    //! Number of tiles in height inside the picture. Shall be >= 1
    cmd->num_tile_rows = pps->num_tile_rows;

    //! Width of each tile column, in number of ctb. Unused values shall be set to 0
    for (i = 0; i < cmd->num_tile_columns; i++)
    {
        cmd->tile_column_width[i] = pps->tile_width[i];
    }

    for (; i < HEVC_MAX_TILE_COLUMNS; i++)
    {
        cmd->tile_column_width[i] = 0;
    }

    //! Height of each tile row, in number of ctb. Unused values shall be set to 0
    for (i = 0; i < cmd->num_tile_rows; i++)
    {
        cmd->tile_row_height[i] = pps->tile_height[i];
    }

    for (; i < HEVC_MAX_TILE_ROWS; i++)
    {
        cmd->tile_row_height[i] = 0;
    }

    cmd->slice_segment_header_extension_flag = pps->slice_segment_header_extension_flag;
    cmd->num_extra_slice_header_bits = pps->num_extra_slice_header_bits;
    cmd->pps_slice_chroma_qp_offsets_present_flag = pps->pps_slice_chroma_qp_offsets_present_flag;
    // ---------------
    // Slice header parameters
    //! This parameter is computed using syntax elements found in the first slice header of the picture
    //! (short_term_ref_pic_set(), short_term_ref_pic_set_idx, num_long_term_pics...)
    // TBD: SAO (*sample_adaptive*)???
    // TBD: how to parse aps_id?
    // Dump MME command via strelay
#ifdef HEVC_CODEC_DUMP_MME
    st_relayfs_write_mme_text(ST_RELAY_TYPE_MME_TEXT1, "\n\t/* Picture #%d */", DecodeFrameIndex);
    LOG_FIRST_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, bitstream_start_offset);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT1, 1, ROTBUF, cmd, bit_buffer);
#ifdef HEVC_HADES_CUT1
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, bit_buffer_stop_offset);
#endif
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, max_slice);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT1, 1, INTBUF, cmd, intermediate_buffer);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, chroma_format_idc);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, separate_colour_plane_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pic_width_in_luma_samples);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pic_height_in_luma_samples);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pcm_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pcm_bit_depth_luma_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pcm_bit_depth_chroma_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_max_pic_order_cnt_lsb_minus4);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, lists_modification_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_min_coding_block_size_minus3);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_diff_max_min_coding_block_size);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_min_transform_block_size_minus2);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_diff_max_min_transform_block_size);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_min_pcm_coding_block_size_minus3);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, log2_diff_max_min_pcm_coding_block_size);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, max_transform_hierarchy_depth_inter);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, max_transform_hierarchy_depth_intra);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, transform_skip_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pps_loop_filter_across_slices_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, amp_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, sample_adaptive_offset_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_short_term_ref_pic_sets);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, long_term_ref_pics_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_long_term_ref_pics_sps);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, sps_temporal_mvp_enabled_flag);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_delta_pocs, HEVC_MAX_SHORT_TERM_RPS);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, dependent_slice_segments_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, sign_data_hiding_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, cabac_init_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_ref_idx_l0_default_active_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_ref_idx_l1_default_active_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, init_qp_minus26);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, cu_qp_delta_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, diff_cu_qp_delta_depth);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, weighted_pred_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, weighted_bipred_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, output_flag_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, deblocking_filter_control_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, deblocking_filter_override_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pps_deblocking_filter_disable_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pps_beta_offset_div2);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pps_tc_offset_div2);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, transquant_bypass_enable_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, tiles_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, entropy_coding_sync_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_tile_columns);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_tile_rows);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, tile_column_width, HEVC_MAX_TILE_COLUMNS);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, tile_row_height, HEVC_MAX_TILE_ROWS);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, slice_segment_header_extension_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_extra_slice_header_bits);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, pps_slice_chroma_qp_offsets_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT1, 1, cmd, num_poc_total_curr);
    st_relayfs_write_mme_text(ST_RELAY_TYPE_MME_TEXT1, "\n\t},");
#endif // HEVC_CODEC_DUMP_MME
}

////////////////////////////////////////////////////////////////////////////
///
/// Unconditionally return success.
///
/// Success and failure codes are located entirely in the generic MME structures
/// allowing the super-class to determine whether the decode was successful. This
/// means that we have no work to do here.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeVideoHevc_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    HevcCodecDecodeContext_t    *HevcContext    = (HevcCodecDecodeContext_t *)Context;

    if (HevcContext->DecodeStatus.error_code != HEVC_DECODER_NO_ERROR)
    {
        SE_INFO(group_decoder_video, "HEVC decode error %08x\n", HevcContext->DecodeStatus.error_code);
#if 0
        DumpMMECommand(&Context->MMECommand);
#endif
    }

    return CodecNoError;
}

//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(HEVC_DECODER_ERROR_RECOVERED);
        E(HEVC_DECODER_ERROR_NOT_RECOVERED);

    default: return "HEVC_DECODER_UNKNOWN_ERROR";
    }

#undef E
}

CodecStatus_t   Codec_MmeVideoHevc_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    unsigned int              i, j;
    unsigned int              ErroneousCTBs;
    unsigned int              log2_max_coding_block_size, max_coding_block_size;
    unsigned int              TotalCTBs;
    unsigned long long        DecodeHadesTime;
    HevcCodecDecodeContext_t *HevcContext        = (HevcCodecDecodeContext_t *)Context;
    MME_Command_t            *MMECommand         = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t      *CmdStatus          = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    hevcdecpix_status_t      *AdditionalInfo_p   = (hevcdecpix_status_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->error_code != HEVC_DECODER_NO_ERROR)
        {
            SE_INFO(group_decoder_video, "%s  %x\n", LookupError(AdditionalInfo_p->error_code), AdditionalInfo_p->error_code);
            //
            // Calculate decode quality from error status fields
            //
            log2_max_coding_block_size = 3 + HevcContext->DecodeParameters.log2_max_coding_block_size_minus3;
            max_coding_block_size = 1;

            while (log2_max_coding_block_size-- > 0)
            {
                max_coding_block_size <<= 1;
            }

            if (!max_coding_block_size) { max_coding_block_size = 1; }

            TotalCTBs = (HevcContext->DecodeParameters.pic_width_in_luma_samples * HevcContext->DecodeParameters.pic_height_in_luma_samples)
                        / (max_coding_block_size * max_coding_block_size);
            ErroneousCTBs = 0;

            for (i = 0; i < HEVC_ERROR_LOC_PARTITION; i++)
                for (j = 0; j < HEVC_ERROR_LOC_PARTITION; j++)
                {
                    ErroneousCTBs += HevcContext->DecodeStatus.error_loc[i][j];
                }

            Context->DecodeQuality = (ErroneousCTBs < TotalCTBs) ?
                                     Rational_t((TotalCTBs - ErroneousCTBs), TotalCTBs) : 0;
#if 0
            SE_DEBUG(group_decoder_video, "AZAAZA - %4d %4d - %d.%09d\n", TotalCTBs, ErroneousCTBs,
                     Context->DecodeQuality.IntegerPart(),  Context->DecodeQuality.RemainderDecimal(9));

            for (unsigned int i = 0; i < HEVC_STATUS_PARTITION; i++)
                SE_DEBUG(group_decoder_video, "  %02x %02x %02x %02x %02x %02x\n",
                         HevcContext->DecodeStatus.status[0][i], HevcContext->DecodeStatus.status[1][i],
                         HevcContext->DecodeStatus.status[2][i], HevcContext->DecodeStatus.status[3][i],
                         HevcContext->DecodeStatus.status[4][i], HevcContext->DecodeStatus.status[5][i]);

#endif

            switch (AdditionalInfo_p->error_code)
            {
            case HEVC_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case HEVC_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            default:
                Stream->Statistics().FrameDecodeError++;
                break;
            }

            if (CmdStatus->Error == MME_COMMAND_TIMEOUT)
            {
                Stream->Statistics().FrameDecodeErrorTaskTimeOutError++;
            }

        }
        else
        {
            // Fabric controller clock value needed to calculate hades decode time on cannes (333 MHz)
            // TODO: Get rate from hades
            const unsigned int  HadesFCClockCannes = 333;
            DecodeHadesTime = HevcContext->DecodeStatus.decode_time_in_tick / HadesFCClockCannes;

            // TODO (as frame parser): for now assuming hevc is always frame encoded and displayed as progressive

            Stream->Statistics().MaxVideoHwDecodeTime = max(DecodeHadesTime , Stream->Statistics().MaxVideoHwDecodeTime);
            Stream->Statistics().MinVideoHwDecodeTime = min(DecodeHadesTime, Stream->Statistics().MinVideoHwDecodeTime);

            // Avoid MinVideoHwDecodeTime = 0 for first frame decode
            if (!Stream->Statistics().MinVideoHwDecodeTime)
            {
                Stream->Statistics().MinVideoHwDecodeTime = DecodeHadesTime;
            }

            if (Stream->Statistics().FrameCountDecoded)
            {
                Stream->Statistics().AvgVideoHwDecodeTime = (DecodeHadesTime +
                                                             ((Stream->Statistics().FrameCountDecoded - 1) * Stream->Statistics().AvgVideoHwDecodeTime))
                                                            / (Stream->Statistics().FrameCountDecoded);
            }
            else
            {
                Stream->Statistics().AvgVideoHwDecodeTime = 0;
            }
        }
    }

    return CodecNoError;
}
//}}}

CodecStatus_t   Codec_MmeVideoHevc_c::DumpSetStreamParameters(void    *Parameters)
{
    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoHevc_c::DumpDecodeParameters(void *Parameters)
{
    return DumpDecodeParameters(Parameters, 0);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoHevc_c::DumpDecodeParameters(void *Parameters, unsigned int DecodeFrameIndex)
{
#ifdef HEVC_CODEC_DUMP_MME
    hevcdecpix_transform_param_t *cmd = (hevcdecpix_transform_param_t *)Parameters;
//
    st_relayfs_write_mme_text(ST_RELAY_TYPE_MME_TEXT2, "\n\t/* Picture #%d */", DecodeFrameIndex);
    LOG_FIRST_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, slice_table_entries);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, INTBUF, cmd, intermediate_buffer);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, dpb_base_addr);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, ppb_base_addr);
    LOG_NEXT_STRUCT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, REFBUF, cmd, ref_picture_buffer, HEVC_MAX_REFERENCE_PICTURES);
    LOG_NEXT_STRUCT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, PICINFO, cmd, ref_picture_info, HEVC_MAX_REFERENCE_PICTURES);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, erc_ref_pic_idx);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, v_dec_factor);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, h_dec_factor);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, RESIZE, cmd, luma_resize_param);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, RESIZE, cmd, chroma_resize_param);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, REFBUF, cmd, curr_picture_buffer);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, DISPBUF, cmd, curr_display_buffer);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, DISPBUF, cmd, curr_resize_buffer);
    LOG_NEXT_STRUCT(ST_RELAY_TYPE_MME_TEXT2, 1, PICINFO, cmd, curr_picture_info);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, num_reference_pictures);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, initial_ref_pic_list_l0, HEVC_MAX_REFERENCE_INDEX);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, initial_ref_pic_list_l1, HEVC_MAX_REFERENCE_INDEX);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, mcc_mode);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, mcc_opcode);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, decoding_mode);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, additional_flags);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, first_picture_in_sequence_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, level_idc);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, sps_max_dec_pic_buffering_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, loop_filter_across_tiles_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pcm_loop_filter_disable_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, sps_temporal_mvp_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, log2_parallel_merge_level_minus2);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pic_width_in_luma_samples);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pic_height_in_luma_samples);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pcm_bit_depth_luma_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pcm_bit_depth_chroma_minus1);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, log2_max_coding_block_size_minus3);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, scaling_list_enable);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, transform_skip_enabled_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, lists_modification_present_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, num_tile_columns);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, num_tile_rows);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, tile_column_width, HEVC_MAX_TILE_COLUMNS);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, tile_row_height, HEVC_MAX_TILE_ROWS);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, sign_data_hiding_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, weighted_pred_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, weighted_bipred_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, constrained_intra_pred_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, strong_intra_smoothing_enable_flag);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pps_cb_qp_offset);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, pps_cr_qp_offset);
    LOG_NEXT_ELEM(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, scaling_list_command_size);
    LOG_NEXT_TABLE(ST_RELAY_TYPE_MME_TEXT2, 1, cmd, scaling_list_command, HEVC_MAX_SCALING_LIST_BUFFER);
    st_relayfs_write_mme_text(ST_RELAY_TYPE_MME_TEXT2, "\n\t},");
#endif // HEVC_CODEC_DUMP_MME
//
    return CodecNoError;
}

