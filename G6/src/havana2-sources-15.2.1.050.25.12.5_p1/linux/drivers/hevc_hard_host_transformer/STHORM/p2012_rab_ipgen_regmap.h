/**
 * \brief defines memory mapped registers
 *
 * Creation date: Fri Mar  1 16:37:52 2013
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Jean-Philippe COUSIN, STMicroelectronics (jean-philippe.cousin@st.com)
 *          Gilles PELISSIER, STMicroelectronics (gilles.pelissier@st.com)
 *
 */

#ifndef _p2012_rab_ipgen_regmap_h_
#define _p2012_rab_ipgen_regmap_h_



#ifndef  HAL_MACROS_BITFIELD
#define HAL_MACROS_BITFIELD

/**
\name macro to extract bitfield
*/
#define HAL_EXTRACT_BITFIELD(regname_, fieldname_, regval_) ((regval_ & regname_ ## __ ## fieldname_ ## _FMSK) >> regname_ ## __ ## fieldname_ ## _FLOC)

/**
\name macro to build bitfield
*/
#define HAL_MAKE_BITFIELD(regname_, fieldname_, fieldval_) ((fieldval_ << regname_ ## __ ## fieldname_ ## _FLOC) & regname_ ## __ ## fieldname_ ## _FMSK)

#endif // MACROS_BITFIELD



// ==============================================================
// start submodule: p2012_rab_gp


#ifndef _rab_WORD_TYPE_
#define _rab_WORD_TYPE_
typedef uint32_t rab_word_t;
#endif
/**
\name BANK RAB_<reg_name>
<H1> General Purpose registers for Remapping Address Block </H1>
*/

#ifndef gp_RAB_DESC
#define gp_RAB_DESC "General Purpose registers for Remapping Address Block"
#endif
/**
\name RAB_CFG

Configuration register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_CFG </TD><TD> 32 </TD><TD> 0x00000000 </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000001 </TD><TD> 0x00000001 </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [0:0] </TD><TD> BYPASS </TD><TD> When this field is high, Remapping Address Block is activated. Otherwise, when it is low, RAB is bypassed. </TD></TR>
 </TABLE>
<H2> default enumeration for RAB_CFG__BYPASS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_CFG__BYPASS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_CFG__BYPASS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
*/
// defines for register RAB_CFG
#define RAB_CFG_RNAM "RAB_CFG"
#define RAB_CFG_RDES "Configuration register"
#define RAB_CFG_ROFF 0x0
#define RAB_CFG_RSIZ 32
#define RAB_CFG_RRWM IPU3_RW
#define RAB_CFG_RRST 0x1
#define RAB_CFG_RMSK 0x1


// defines for field RAB_CFG__BYPASS
#define RAB_CFG__BYPASS_FNAM "RAB_CFG__BYPASS"
#define RAB_CFG__BYPASS_FDES "When this field is high, Remapping Address Block is activated. Otherwise, when it is low, RAB is bypassed."
#define RAB_CFG__BYPASS_FLOC 0
#define RAB_CFG__BYPASS_FSIZ 1
#define RAB_CFG__BYPASS_FMSK 0x1


// default enum for RAB_CFG__BYPASS
#define RAB_CFG__BYPASS_ON  1
#define RAB_CFG__BYPASS_OFF 0
/**
\name RAB_IT_MASK

Interrupt mask register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_IT_MASK </TD><TD> 32 </TD><TD> 0x00000004 </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x0000000f </TD><TD> 0x0000000f </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [3:0] </TD><TD> MASK </TD><TD> Mask to be applied on ERR_STS register </TD></TR>
 </TABLE>
*/
// defines for register RAB_IT_MASK
#define RAB_IT_MASK_RNAM "RAB_IT_MASK"
#define RAB_IT_MASK_RDES "Interrupt mask register"
#define RAB_IT_MASK_ROFF 0x4
#define RAB_IT_MASK_RSIZ 32
#define RAB_IT_MASK_RRWM IPU3_RW
#define RAB_IT_MASK_RRST 0xf
#define RAB_IT_MASK_RMSK 0xf


// defines for field RAB_IT_MASK__MASK
#define RAB_IT_MASK__MASK_FNAM "RAB_IT_MASK__MASK"
#define RAB_IT_MASK__MASK_FDES "Mask to be applied on ERR_STS register"
#define RAB_IT_MASK__MASK_FLOC 0
#define RAB_IT_MASK__MASK_FSIZ 4
#define RAB_IT_MASK__MASK_FMSK 0xf


/**
\name RAB_ERR_STS

Status error register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_ERR_STS </TD><TD> 32 </TD><TD> 0x00000008 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0x0000000f </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [0:0] </TD><TD> MISS </TD><TD> Input address doesn’ t match RAB entry. If address must be translated, Software is in charge to allocate a new entry in RAB. </TD></TR>
 <TR><TD> [1:1] </TD><TD> MULTI_HITS </TD><TD> Input address hits several RAB entries. Software can read RAB_GP_HIT_ERR_STS to know which entries have hit the same address. </TD></TR>
 <TR><TD> [2:2] </TD><TD> WRITE_ERROR </TD><TD> Input address has hit RAB entry with READ ONLY protection. Software can read RAB_GP_HIT_ERR_STS to know which entry has hit. </TD></TR>
 <TR><TD> [3:3] </TD><TD> READ_ERROR </TD><TD> Input address has hit RAB entry with WRITE ONLY protection. Software can read RAB_GP_HIT_ERR_STS to know which entry has hit. </TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_STS__MISS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_STS__MISS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_STS__MISS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_STS__MULTI_HITS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_STS__MULTI_HITS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_STS__MULTI_HITS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_STS__WRITE_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_STS__WRITE_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_STS__WRITE_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_STS__READ_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_STS__READ_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_STS__READ_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
*/
// defines for register RAB_ERR_STS
#define RAB_ERR_STS_RNAM "RAB_ERR_STS"
#define RAB_ERR_STS_RDES "Status error register"
#define RAB_ERR_STS_ROFF 0x8
#define RAB_ERR_STS_RSIZ 32
#define RAB_ERR_STS_RRWM IPU3_RO
#define RAB_ERR_STS_RRST 0x0
#define RAB_ERR_STS_RMSK 0xf


// defines for field RAB_ERR_STS__MISS
#define RAB_ERR_STS__MISS_FNAM "RAB_ERR_STS__MISS"
#define RAB_ERR_STS__MISS_FDES "Input address doesn’ t match RAB entry. If address must be translated, Software is in charge to allocate a new entry in RAB."
#define RAB_ERR_STS__MISS_FLOC 0
#define RAB_ERR_STS__MISS_FSIZ 1
#define RAB_ERR_STS__MISS_FMSK 0x1


// default enum for RAB_ERR_STS__MISS
#define RAB_ERR_STS__MISS_ON  1
#define RAB_ERR_STS__MISS_OFF 0
// defines for field RAB_ERR_STS__MULTI_HITS
#define RAB_ERR_STS__MULTI_HITS_FNAM "RAB_ERR_STS__MULTI_HITS"
#define RAB_ERR_STS__MULTI_HITS_FDES "Input address hits several RAB entries. Software can read RAB_GP_HIT_ERR_STS to know which entries have hit the same address."
#define RAB_ERR_STS__MULTI_HITS_FLOC 1
#define RAB_ERR_STS__MULTI_HITS_FSIZ 1
#define RAB_ERR_STS__MULTI_HITS_FMSK 0x2


// default enum for RAB_ERR_STS__MULTI_HITS
#define RAB_ERR_STS__MULTI_HITS_ON  1
#define RAB_ERR_STS__MULTI_HITS_OFF 0
// defines for field RAB_ERR_STS__WRITE_ERROR
#define RAB_ERR_STS__WRITE_ERROR_FNAM "RAB_ERR_STS__WRITE_ERROR"
#define RAB_ERR_STS__WRITE_ERROR_FDES "Input address has hit RAB entry with READ ONLY protection. Software can read RAB_GP_HIT_ERR_STS to know which entry has hit."
#define RAB_ERR_STS__WRITE_ERROR_FLOC 2
#define RAB_ERR_STS__WRITE_ERROR_FSIZ 1
#define RAB_ERR_STS__WRITE_ERROR_FMSK 0x4


// default enum for RAB_ERR_STS__WRITE_ERROR
#define RAB_ERR_STS__WRITE_ERROR_ON  1
#define RAB_ERR_STS__WRITE_ERROR_OFF 0
// defines for field RAB_ERR_STS__READ_ERROR
#define RAB_ERR_STS__READ_ERROR_FNAM "RAB_ERR_STS__READ_ERROR"
#define RAB_ERR_STS__READ_ERROR_FDES "Input address has hit RAB entry with WRITE ONLY protection. Software can read RAB_GP_HIT_ERR_STS to know which entry has hit."
#define RAB_ERR_STS__READ_ERROR_FLOC 3
#define RAB_ERR_STS__READ_ERROR_FSIZ 1
#define RAB_ERR_STS__READ_ERROR_FMSK 0x8


// default enum for RAB_ERR_STS__READ_ERROR
#define RAB_ERR_STS__READ_ERROR_ON  1
#define RAB_ERR_STS__READ_ERROR_OFF 0
/**
\name RAB_ERR_BSET

Bit set of Status error register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_ERR_BSET </TD><TD> 32 </TD><TD> 0x0000000c </TD><TD> EXTERNAL_WRITE_ONLY </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0x0000000f </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [0:0] </TD><TD> MISS </TD><TD> Input address doesn’ t match RAB entry. </TD></TR>
 <TR><TD> [1:1] </TD><TD> MULTI_HITS </TD><TD> Input address hits several RAB entries. </TD></TR>
 <TR><TD> [2:2] </TD><TD> WRITE_ERROR </TD><TD> Input address has hit RAB entry with READ ONLY protection. </TD></TR>
 <TR><TD> [3:3] </TD><TD> READ_ERROR </TD><TD> Input address has hit RAB entry with WRITE ONLY protection. </TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BSET__MISS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BSET__MISS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BSET__MISS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BSET__MULTI_HITS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BSET__MULTI_HITS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BSET__MULTI_HITS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BSET__WRITE_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BSET__WRITE_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BSET__WRITE_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BSET__READ_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BSET__READ_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BSET__READ_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
*/
// defines for register RAB_ERR_BSET
#define RAB_ERR_BSET_RNAM "RAB_ERR_BSET"
#define RAB_ERR_BSET_RDES "Bit set of Status error register"
#define RAB_ERR_BSET_ROFF 0xc
#define RAB_ERR_BSET_RSIZ 32
#define RAB_ERR_BSET_RRWM IPU3_WO
#define RAB_ERR_BSET_RRST 0x0
#define RAB_ERR_BSET_RMSK 0xf


// defines for field RAB_ERR_BSET__MISS
#define RAB_ERR_BSET__MISS_FNAM "RAB_ERR_BSET__MISS"
#define RAB_ERR_BSET__MISS_FDES "Input address doesn’ t match RAB entry."
#define RAB_ERR_BSET__MISS_FLOC 0
#define RAB_ERR_BSET__MISS_FSIZ 1
#define RAB_ERR_BSET__MISS_FMSK 0x1


// default enum for RAB_ERR_BSET__MISS
#define RAB_ERR_BSET__MISS_ON  1
#define RAB_ERR_BSET__MISS_OFF 0
// defines for field RAB_ERR_BSET__MULTI_HITS
#define RAB_ERR_BSET__MULTI_HITS_FNAM "RAB_ERR_BSET__MULTI_HITS"
#define RAB_ERR_BSET__MULTI_HITS_FDES "Input address hits several RAB entries."
#define RAB_ERR_BSET__MULTI_HITS_FLOC 1
#define RAB_ERR_BSET__MULTI_HITS_FSIZ 1
#define RAB_ERR_BSET__MULTI_HITS_FMSK 0x2


// default enum for RAB_ERR_BSET__MULTI_HITS
#define RAB_ERR_BSET__MULTI_HITS_ON  1
#define RAB_ERR_BSET__MULTI_HITS_OFF 0
// defines for field RAB_ERR_BSET__WRITE_ERROR
#define RAB_ERR_BSET__WRITE_ERROR_FNAM "RAB_ERR_BSET__WRITE_ERROR"
#define RAB_ERR_BSET__WRITE_ERROR_FDES "Input address has hit RAB entry with READ ONLY protection."
#define RAB_ERR_BSET__WRITE_ERROR_FLOC 2
#define RAB_ERR_BSET__WRITE_ERROR_FSIZ 1
#define RAB_ERR_BSET__WRITE_ERROR_FMSK 0x4


// default enum for RAB_ERR_BSET__WRITE_ERROR
#define RAB_ERR_BSET__WRITE_ERROR_ON  1
#define RAB_ERR_BSET__WRITE_ERROR_OFF 0
// defines for field RAB_ERR_BSET__READ_ERROR
#define RAB_ERR_BSET__READ_ERROR_FNAM "RAB_ERR_BSET__READ_ERROR"
#define RAB_ERR_BSET__READ_ERROR_FDES "Input address has hit RAB entry with WRITE ONLY protection."
#define RAB_ERR_BSET__READ_ERROR_FLOC 3
#define RAB_ERR_BSET__READ_ERROR_FSIZ 1
#define RAB_ERR_BSET__READ_ERROR_FMSK 0x8


// default enum for RAB_ERR_BSET__READ_ERROR
#define RAB_ERR_BSET__READ_ERROR_ON  1
#define RAB_ERR_BSET__READ_ERROR_OFF 0
/**
\name RAB_ERR_BCLR

Bit clear of Status error register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_ERR_BCLR </TD><TD> 32 </TD><TD> 0x00000010 </TD><TD> EXTERNAL_WRITE_ONLY </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0x0000000f </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [0:0] </TD><TD> MISS </TD><TD> Input address doesn’ t match RAB entry. </TD></TR>
 <TR><TD> [1:1] </TD><TD> MULTI_HITS </TD><TD> Input address hits several RAB entries. </TD></TR>
 <TR><TD> [2:2] </TD><TD> WRITE_ERROR </TD><TD> Input address has hit RAB entry with READ ONLY protection. </TD></TR>
 <TR><TD> [3:3] </TD><TD> READ_ERROR </TD><TD> Input address has hit RAB entry with WRITE ONLY protection. </TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BCLR__MISS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BCLR__MISS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BCLR__MISS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BCLR__MULTI_HITS </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BCLR__MULTI_HITS_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BCLR__MULTI_HITS_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BCLR__WRITE_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BCLR__WRITE_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BCLR__WRITE_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> default enumeration for RAB_ERR_BCLR__READ_ERROR </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_ERR_BCLR__READ_ERROR_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_ERR_BCLR__READ_ERROR_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
*/
// defines for register RAB_ERR_BCLR
#define RAB_ERR_BCLR_RNAM "RAB_ERR_BCLR"
#define RAB_ERR_BCLR_RDES "Bit clear of Status error register"
#define RAB_ERR_BCLR_ROFF 0x10
#define RAB_ERR_BCLR_RSIZ 32
#define RAB_ERR_BCLR_RRWM IPU3_WO
#define RAB_ERR_BCLR_RRST 0x0
#define RAB_ERR_BCLR_RMSK 0xf


// defines for field RAB_ERR_BCLR__MISS
#define RAB_ERR_BCLR__MISS_FNAM "RAB_ERR_BCLR__MISS"
#define RAB_ERR_BCLR__MISS_FDES "Input address doesn’ t match RAB entry."
#define RAB_ERR_BCLR__MISS_FLOC 0
#define RAB_ERR_BCLR__MISS_FSIZ 1
#define RAB_ERR_BCLR__MISS_FMSK 0x1


// default enum for RAB_ERR_BCLR__MISS
#define RAB_ERR_BCLR__MISS_ON  1
#define RAB_ERR_BCLR__MISS_OFF 0
// defines for field RAB_ERR_BCLR__MULTI_HITS
#define RAB_ERR_BCLR__MULTI_HITS_FNAM "RAB_ERR_BCLR__MULTI_HITS"
#define RAB_ERR_BCLR__MULTI_HITS_FDES "Input address hits several RAB entries."
#define RAB_ERR_BCLR__MULTI_HITS_FLOC 1
#define RAB_ERR_BCLR__MULTI_HITS_FSIZ 1
#define RAB_ERR_BCLR__MULTI_HITS_FMSK 0x2


// default enum for RAB_ERR_BCLR__MULTI_HITS
#define RAB_ERR_BCLR__MULTI_HITS_ON  1
#define RAB_ERR_BCLR__MULTI_HITS_OFF 0
// defines for field RAB_ERR_BCLR__WRITE_ERROR
#define RAB_ERR_BCLR__WRITE_ERROR_FNAM "RAB_ERR_BCLR__WRITE_ERROR"
#define RAB_ERR_BCLR__WRITE_ERROR_FDES "Input address has hit RAB entry with READ ONLY protection."
#define RAB_ERR_BCLR__WRITE_ERROR_FLOC 2
#define RAB_ERR_BCLR__WRITE_ERROR_FSIZ 1
#define RAB_ERR_BCLR__WRITE_ERROR_FMSK 0x4


// default enum for RAB_ERR_BCLR__WRITE_ERROR
#define RAB_ERR_BCLR__WRITE_ERROR_ON  1
#define RAB_ERR_BCLR__WRITE_ERROR_OFF 0
// defines for field RAB_ERR_BCLR__READ_ERROR
#define RAB_ERR_BCLR__READ_ERROR_FNAM "RAB_ERR_BCLR__READ_ERROR"
#define RAB_ERR_BCLR__READ_ERROR_FDES "Input address has hit RAB entry with WRITE ONLY protection."
#define RAB_ERR_BCLR__READ_ERROR_FLOC 3
#define RAB_ERR_BCLR__READ_ERROR_FSIZ 1
#define RAB_ERR_BCLR__READ_ERROR_FMSK 0x8


// default enum for RAB_ERR_BCLR__READ_ERROR
#define RAB_ERR_BCLR__READ_ERROR_ON  1
#define RAB_ERR_BCLR__READ_ERROR_OFF 0
/**
\name RAB_HIT_ERR_STS0

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS0 </TD><TD> 32 </TD><TD> 0x00000014 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS0
#define RAB_HIT_ERR_STS0_RNAM "RAB_HIT_ERR_STS0"
#define RAB_HIT_ERR_STS0_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS0_ROFF 0x14
#define RAB_HIT_ERR_STS0_RSIZ 32
#define RAB_HIT_ERR_STS0_RRWM IPU3_RO
#define RAB_HIT_ERR_STS0_RRST 0x0
#define RAB_HIT_ERR_STS0_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS0__HIT
#define RAB_HIT_ERR_STS0__HIT_FNAM "RAB_HIT_ERR_STS0__HIT"
#define RAB_HIT_ERR_STS0__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS0__HIT_FLOC 0
#define RAB_HIT_ERR_STS0__HIT_FSIZ 32
#define RAB_HIT_ERR_STS0__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS1

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS1 </TD><TD> 32 </TD><TD> 0x00000018 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS1
#define RAB_HIT_ERR_STS1_RNAM "RAB_HIT_ERR_STS1"
#define RAB_HIT_ERR_STS1_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS1_ROFF 0x18
#define RAB_HIT_ERR_STS1_RSIZ 32
#define RAB_HIT_ERR_STS1_RRWM IPU3_RO
#define RAB_HIT_ERR_STS1_RRST 0x0
#define RAB_HIT_ERR_STS1_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS1__HIT
#define RAB_HIT_ERR_STS1__HIT_FNAM "RAB_HIT_ERR_STS1__HIT"
#define RAB_HIT_ERR_STS1__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS1__HIT_FLOC 0
#define RAB_HIT_ERR_STS1__HIT_FSIZ 32
#define RAB_HIT_ERR_STS1__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS2

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS2 </TD><TD> 32 </TD><TD> 0x0000001c </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS2
#define RAB_HIT_ERR_STS2_RNAM "RAB_HIT_ERR_STS2"
#define RAB_HIT_ERR_STS2_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS2_ROFF 0x1c
#define RAB_HIT_ERR_STS2_RSIZ 32
#define RAB_HIT_ERR_STS2_RRWM IPU3_RO
#define RAB_HIT_ERR_STS2_RRST 0x0
#define RAB_HIT_ERR_STS2_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS2__HIT
#define RAB_HIT_ERR_STS2__HIT_FNAM "RAB_HIT_ERR_STS2__HIT"
#define RAB_HIT_ERR_STS2__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS2__HIT_FLOC 0
#define RAB_HIT_ERR_STS2__HIT_FSIZ 32
#define RAB_HIT_ERR_STS2__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS3

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS3 </TD><TD> 32 </TD><TD> 0x00000020 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS3
#define RAB_HIT_ERR_STS3_RNAM "RAB_HIT_ERR_STS3"
#define RAB_HIT_ERR_STS3_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS3_ROFF 0x20
#define RAB_HIT_ERR_STS3_RSIZ 32
#define RAB_HIT_ERR_STS3_RRWM IPU3_RO
#define RAB_HIT_ERR_STS3_RRST 0x0
#define RAB_HIT_ERR_STS3_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS3__HIT
#define RAB_HIT_ERR_STS3__HIT_FNAM "RAB_HIT_ERR_STS3__HIT"
#define RAB_HIT_ERR_STS3__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS3__HIT_FLOC 0
#define RAB_HIT_ERR_STS3__HIT_FSIZ 32
#define RAB_HIT_ERR_STS3__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS4

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS4 </TD><TD> 32 </TD><TD> 0x00000024 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS4
#define RAB_HIT_ERR_STS4_RNAM "RAB_HIT_ERR_STS4"
#define RAB_HIT_ERR_STS4_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS4_ROFF 0x24
#define RAB_HIT_ERR_STS4_RSIZ 32
#define RAB_HIT_ERR_STS4_RRWM IPU3_RO
#define RAB_HIT_ERR_STS4_RRST 0x0
#define RAB_HIT_ERR_STS4_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS4__HIT
#define RAB_HIT_ERR_STS4__HIT_FNAM "RAB_HIT_ERR_STS4__HIT"
#define RAB_HIT_ERR_STS4__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS4__HIT_FLOC 0
#define RAB_HIT_ERR_STS4__HIT_FSIZ 32
#define RAB_HIT_ERR_STS4__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS5

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS5 </TD><TD> 32 </TD><TD> 0x00000028 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS5
#define RAB_HIT_ERR_STS5_RNAM "RAB_HIT_ERR_STS5"
#define RAB_HIT_ERR_STS5_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS5_ROFF 0x28
#define RAB_HIT_ERR_STS5_RSIZ 32
#define RAB_HIT_ERR_STS5_RRWM IPU3_RO
#define RAB_HIT_ERR_STS5_RRST 0x0
#define RAB_HIT_ERR_STS5_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS5__HIT
#define RAB_HIT_ERR_STS5__HIT_FNAM "RAB_HIT_ERR_STS5__HIT"
#define RAB_HIT_ERR_STS5__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS5__HIT_FLOC 0
#define RAB_HIT_ERR_STS5__HIT_FSIZ 32
#define RAB_HIT_ERR_STS5__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS6

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS6 </TD><TD> 32 </TD><TD> 0x0000002c </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS6
#define RAB_HIT_ERR_STS6_RNAM "RAB_HIT_ERR_STS6"
#define RAB_HIT_ERR_STS6_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS6_ROFF 0x2c
#define RAB_HIT_ERR_STS6_RSIZ 32
#define RAB_HIT_ERR_STS6_RRWM IPU3_RO
#define RAB_HIT_ERR_STS6_RRST 0x0
#define RAB_HIT_ERR_STS6_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS6__HIT
#define RAB_HIT_ERR_STS6__HIT_FNAM "RAB_HIT_ERR_STS6__HIT"
#define RAB_HIT_ERR_STS6__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS6__HIT_FLOC 0
#define RAB_HIT_ERR_STS6__HIT_FSIZ 32
#define RAB_HIT_ERR_STS6__HIT_FMSK 0xffffffff


/**
\name RAB_HIT_ERR_STS7

Status hit errors register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_HIT_ERR_STS7 </TD><TD> 32 </TD><TD> 0x00000030 </TD><TD> EXTERNAL_READ_ONLY </TD><TD> INTERNAL_READ_WRITE </TD><TD> 0x00000000 </TD><TD> 0xffffffff </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:0] </TD><TD> HIT </TD><TD> Entry which has hit in error state </TD></TR>
 </TABLE>
*/
// defines for register RAB_HIT_ERR_STS7
#define RAB_HIT_ERR_STS7_RNAM "RAB_HIT_ERR_STS7"
#define RAB_HIT_ERR_STS7_RDES "Status hit errors register"
#define RAB_HIT_ERR_STS7_ROFF 0x30
#define RAB_HIT_ERR_STS7_RSIZ 32
#define RAB_HIT_ERR_STS7_RRWM IPU3_RO
#define RAB_HIT_ERR_STS7_RRST 0x0
#define RAB_HIT_ERR_STS7_RMSK 0xffffffff


// defines for field RAB_HIT_ERR_STS7__HIT
#define RAB_HIT_ERR_STS7__HIT_FNAM "RAB_HIT_ERR_STS7__HIT"
#define RAB_HIT_ERR_STS7__HIT_FDES "Entry which has hit in error state"
#define RAB_HIT_ERR_STS7__HIT_FLOC 0
#define RAB_HIT_ERR_STS7__HIT_FSIZ 32
#define RAB_HIT_ERR_STS7__HIT_FMSK 0xffffffff



// end submodule: p2012_rab_gp
// ==============================================================

// ==============================================================
// start submodule: p2012_rab_entry


#ifndef _rab_WORD_TYPE_
#define _rab_WORD_TYPE_
typedef uint32_t rab_word_t;
#endif
/**
\name BANK RAB_<reg_name>
<H1> Entry registers for Remapping Address Block </H1>
*/

#ifndef entry_RAB_DESC
#define entry_RAB_DESC "Entry registers for Remapping Address Block"
#endif
/**
\name RAB_CTRL

RAB ENTRY control register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_CTRL </TD><TD> 32 </TD><TD> 0x00000000 </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0x0000001f </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [0:0] </TD><TD> VALID </TD><TD> When this field is high, Remapping Address Block is activate. </TD></TR>
 <TR><TD> [2:1] </TD><TD> PROTECTION </TD><TD> Protection mode to apply when address hits source address. For instance, error is returned and transaction is not forwarded when store hits SRC_BASE with READ_ONLY protection. Protections are the following:   </TD></TR>
 <TR><TD> [4:3] </TD><TD> COHERENCY </TD><TD> Coherency mode to apply when address hits source address. For instance, when coherency is enabled, initiator port connected to coherency mechanism is used, otherwise default initiator is used. Coherency mode are the following: </TD></TR>
 </TABLE>
<H2> default enumeration for RAB_CTRL__VALID </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_CTRL__VALID_ON</TD><TD>1</TD><TD>on value</TD></TR>
 <TR><TD>RAB_CTRL__VALID_OFF</TD><TD>0</TD><TD>off value</TD></TR>
 </TABLE>
<H2> enumeration for RAB_CTRL__PROTECTION </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_CTRL__PROTECTION_RAB_READ_ONLY</TD><TD>0x00000001</TD><TD>0x1</TD></TR>
 <TR><TD>RAB_CTRL__PROTECTION_RAB_WRITE_ONLY</TD><TD>0x00000002</TD><TD>0x2</TD></TR>
 <TR><TD>RAB_CTRL__PROTECTION_RAB_READ_WRITE</TD><TD>0x00000003</TD><TD>0x3</TD></TR>
 </TABLE>
<H2> enumeration for RAB_CTRL__COHERENCY </H2>

 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>VALUE</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD>RAB_CTRL__COHERENCY_RAB_WITHOUT_COHERENCY</TD><TD>0x00000000</TD><TD>0x0</TD></TR>
 <TR><TD>RAB_CTRL__COHERENCY_RAB_WITH_COHERENCY</TD><TD>0x00000001</TD><TD>0x1</TD></TR>
 </TABLE>
*/
// defines for register RAB_CTRL
#define RAB_CTRL_RNAM "RAB_CTRL"
#define RAB_CTRL_RDES "RAB ENTRY control register"
#define RAB_CTRL_ROFF 0x0
#define RAB_CTRL_RSIZ 32
#define RAB_CTRL_RRWM IPU3_RW
#define RAB_CTRL_RRST 0x0
#define RAB_CTRL_RMSK 0x1f


// defines for field RAB_CTRL__VALID
#define RAB_CTRL__VALID_FNAM "RAB_CTRL__VALID"
#define RAB_CTRL__VALID_FDES "When this field is high, Remapping Address Block is activate."
#define RAB_CTRL__VALID_FLOC 0
#define RAB_CTRL__VALID_FSIZ 1
#define RAB_CTRL__VALID_FMSK 0x1


// default enum for RAB_CTRL__VALID
#define RAB_CTRL__VALID_ON  1
#define RAB_CTRL__VALID_OFF 0
// defines for field RAB_CTRL__PROTECTION
#define RAB_CTRL__PROTECTION_FNAM "RAB_CTRL__PROTECTION"
#define RAB_CTRL__PROTECTION_FDES "Protection mode to apply when address hits source address. For instance, error is returned and transaction is not forwarded when store hits SRC_BASE with READ_ONLY protection. Protections are the following:  "
#define RAB_CTRL__PROTECTION_FLOC 1
#define RAB_CTRL__PROTECTION_FSIZ 2
#define RAB_CTRL__PROTECTION_FMSK 0x6


// enum for RAB_CTRL__PROTECTION
typedef uint32_t rab_ctrl_protection_t;

#ifndef RAB_READ_ONLY
#define RAB_READ_ONLY 0x00000001 // 0x1
#endif

#ifndef RAB_WRITE_ONLY
#define RAB_WRITE_ONLY 0x00000002 // 0x2
#endif

#ifndef RAB_READ_WRITE
#define RAB_READ_WRITE 0x00000003 // 0x3
#endif

// defines for field RAB_CTRL__COHERENCY
#define RAB_CTRL__COHERENCY_FNAM "RAB_CTRL__COHERENCY"
#define RAB_CTRL__COHERENCY_FDES "Coherency mode to apply when address hits source address. For instance, when coherency is enabled, initiator port connected to coherency mechanism is used, otherwise default initiator is used. Coherency mode are the following:"
#define RAB_CTRL__COHERENCY_FLOC 3
#define RAB_CTRL__COHERENCY_FSIZ 2
#define RAB_CTRL__COHERENCY_FMSK 0x18


// enum for RAB_CTRL__COHERENCY
typedef uint32_t rab_ctrl_coherency_t;

#ifndef RAB_WITHOUT_COHERENCY
#define RAB_WITHOUT_COHERENCY 0x00000000 // 0x0
#endif

#ifndef RAB_WITH_COHERENCY
#define RAB_WITH_COHERENCY 0x00000001 // 0x1
#endif

/**
\name RAB_SRC_RANGE_MIN

Source address range minimum register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_SRC_RANGE_MIN </TD><TD> 32 </TD><TD> 0x00000004 </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0xfffff000 </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:12] </TD><TD> ADDRESS </TD><TD> Source range address minimum used to be compared with input address.   </TD></TR>
 </TABLE>
*/
// defines for register RAB_SRC_RANGE_MIN
#define RAB_SRC_RANGE_MIN_RNAM "RAB_SRC_RANGE_MIN"
#define RAB_SRC_RANGE_MIN_RDES "Source address range minimum register"
#define RAB_SRC_RANGE_MIN_ROFF 0x4
#define RAB_SRC_RANGE_MIN_RSIZ 32
#define RAB_SRC_RANGE_MIN_RRWM IPU3_RW
#define RAB_SRC_RANGE_MIN_RRST 0x0
#define RAB_SRC_RANGE_MIN_RMSK 0xfffff000


// defines for field RAB_SRC_RANGE_MIN__ADDRESS
#define RAB_SRC_RANGE_MIN__ADDRESS_FNAM "RAB_SRC_RANGE_MIN__ADDRESS"
#define RAB_SRC_RANGE_MIN__ADDRESS_FDES "Source range address minimum used to be compared with input address.  "
#define RAB_SRC_RANGE_MIN__ADDRESS_FLOC 12
#define RAB_SRC_RANGE_MIN__ADDRESS_FSIZ 20
#define RAB_SRC_RANGE_MIN__ADDRESS_FMSK 0xfffff000


/**
\name RAB_SRC_RANGE_MAX

Source range address maximum register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_SRC_RANGE_MAX </TD><TD> 32 </TD><TD> 0x00000008 </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0xfffff000 </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:12] </TD><TD> ADDRESS </TD><TD> Source range address maximum used to be compared with input address.   </TD></TR>
 </TABLE>
*/
// defines for register RAB_SRC_RANGE_MAX
#define RAB_SRC_RANGE_MAX_RNAM "RAB_SRC_RANGE_MAX"
#define RAB_SRC_RANGE_MAX_RDES "Source range address maximum register"
#define RAB_SRC_RANGE_MAX_ROFF 0x8
#define RAB_SRC_RANGE_MAX_RSIZ 32
#define RAB_SRC_RANGE_MAX_RRWM IPU3_RW
#define RAB_SRC_RANGE_MAX_RRST 0x0
#define RAB_SRC_RANGE_MAX_RMSK 0xfffff000


// defines for field RAB_SRC_RANGE_MAX__ADDRESS
#define RAB_SRC_RANGE_MAX__ADDRESS_FNAM "RAB_SRC_RANGE_MAX__ADDRESS"
#define RAB_SRC_RANGE_MAX__ADDRESS_FDES "Source range address maximum used to be compared with input address.  "
#define RAB_SRC_RANGE_MAX__ADDRESS_FLOC 12
#define RAB_SRC_RANGE_MAX__ADDRESS_FSIZ 20
#define RAB_SRC_RANGE_MAX__ADDRESS_FMSK 0xfffff000


/**
\name RAB_DST_REMAP

Destination remapping address register
 <TABLE>
 <TR><TD><B>NAME</B></TD><TD><B>SIZE</B></TD><TD><B>OFFSET</B></TD><TD><B>EXTERNAL MODE</B></TD><TD><B>INTERNAL MODE</B></TD><TD><B>RESET VALUE</B></TD><TD><B>FIELD MASK</B></TD></TR>
 <TR><TD> RAB_DST_REMAP </TD><TD> 32 </TD><TD> 0x0000000c </TD><TD> EXTERNAL_READ_WRITE </TD><TD> INTERNAL_READ_ONLY </TD><TD> 0x00000000 </TD><TD> 0xfffff000 </TD></TR>
 </TABLE>

 <B> field description</B>
 <TABLE>

 <TR><TD><B>FIELD POS</B></TD><TD><B>NAME</B></TD><TD><B>DESCRIPTION</B></TD></TR>
 <TR><TD> [31:12] </TD><TD> ADDRESS </TD><TD> Destination remapping address register to apply when input address hits ((SRC_RANGE_MIN &lt;= src_add) AND (src_add &lt;= SRC_MANGE_MAX)). When src address hits source range then destination address is calculated using the following expression: dst_add = (DST_REMAP + (src_add - SRC_RANGE_MIN)). </TD></TR>
 </TABLE>
*/
// defines for register RAB_DST_REMAP
#define RAB_DST_REMAP_RNAM "RAB_DST_REMAP"
#define RAB_DST_REMAP_RDES "Destination remapping address register"
#define RAB_DST_REMAP_ROFF 0xc
#define RAB_DST_REMAP_RSIZ 32
#define RAB_DST_REMAP_RRWM IPU3_RW
#define RAB_DST_REMAP_RRST 0x0
#define RAB_DST_REMAP_RMSK 0xfffff000


// defines for field RAB_DST_REMAP__ADDRESS
#define RAB_DST_REMAP__ADDRESS_FNAM "RAB_DST_REMAP__ADDRESS"
#define RAB_DST_REMAP__ADDRESS_FDES "Destination remapping address register to apply when input address hits ((SRC_RANGE_MIN &lt;= src_add) AND (src_add &lt;= SRC_MANGE_MAX)). When src address hits source range then destination address is calculated using the following expression: dst_add = (DST_REMAP + (src_add - SRC_RANGE_MIN))."
#define RAB_DST_REMAP__ADDRESS_FLOC 12
#define RAB_DST_REMAP__ADDRESS_FSIZ 20
#define RAB_DST_REMAP__ADDRESS_FMSK 0xfffff000



// end submodule: p2012_rab_entry
// ==============================================================

//@}
#endif
