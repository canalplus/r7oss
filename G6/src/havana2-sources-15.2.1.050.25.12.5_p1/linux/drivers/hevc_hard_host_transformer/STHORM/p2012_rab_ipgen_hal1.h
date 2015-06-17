/**
 * \brief Hardware Abstraction Layer
 *
 * Creation date: Fri Mar  1 16:37:54 2013
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

#ifndef _p2012_rab_ipgen_hal1_h_
#define _p2012_rab_ipgen_hal1_h_


/**
 * \brief memory map and register description
 */
#include "p2012_rab_ipgen_regmap.h"

/**
 * \brief register access
 */
#include "driverEmuHce.h"

/**
 * \brief used with doxygen for the documentation (doxygen can't support key words static inline)
 */
 #define P2012_RAB_IPGEN_STATIC_INLINE static inline


// ==============================================================
// start submodule: p2012_rab_gp

/**
 * \brief Read RAB_CFG and returns uint32_t
 *
 * register description: Configuration register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_cfg_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_CFG_ROFF));
}

/**
 * \brief Write RAB_CFG using type uint32_t
 *
 * register description: Configuration register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_gp_rab_cfg_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_CFG_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_CFG
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_cfg(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_gp_rab_cfg_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_cfg_uint32(base_) != (RAB_CFG_RMSK & 0x0)) { return(1); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_cfg_uint32(base_) != (RAB_CFG_RMSK & 0xffffffff)) { return(2); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_cfg_uint32(base_) != (RAB_CFG_RMSK & 0xa5a5a5a5)) { return(3); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_cfg_uint32(base_) != (RAB_CFG_RMSK & 0x5a5a5a5a)) { return(4); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_cfg_uint32(base_) != (RAB_CFG_RMSK & 0x89abcdef)) { return(5); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_gp_rab_cfg_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read RAB_IT_MASK and returns uint32_t
 *
 * register description: Interrupt mask register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_it_mask_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_IT_MASK_ROFF));
}

/**
 * \brief Write RAB_IT_MASK using type uint32_t
 *
 * register description: Interrupt mask register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_gp_rab_it_mask_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_IT_MASK_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_IT_MASK
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_it_mask(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_gp_rab_it_mask_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_it_mask_uint32(base_) != (RAB_IT_MASK_RMSK & 0x0)) { return(6); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_it_mask_uint32(base_) != (RAB_IT_MASK_RMSK & 0xffffffff)) { return(7); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_it_mask_uint32(base_) != (RAB_IT_MASK_RMSK & 0xa5a5a5a5)) { return(8); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_it_mask_uint32(base_) != (RAB_IT_MASK_RMSK & 0x5a5a5a5a)) { return(9); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_gp_rab_it_mask_uint32(base_) != (RAB_IT_MASK_RMSK & 0x89abcdef)) { return(10); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_gp_rab_it_mask_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read RAB_ERR_STS and returns uint32_t
 *
 * register description: Status error register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_err_sts_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_ERR_STS_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_ERR_STS
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_err_sts(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_err_sts_uint32(base_);
  if (data_read_tmp != RAB_ERR_STS_RRST) { return(11); }

  return(0);
}

#endif

/**
 * \brief Write RAB_ERR_BSET using type uint32_t
 *
 * register description: Bit set of Status error register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_gp_rab_err_bset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_ERR_BSET_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_ERR_BSET
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_err_bset(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal1_write_gp_rab_err_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal1_read_gp_rab_err_sts_uint32(base_) != RAB_ERR_STS_RMSK) { return(12); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal1_write_gp_rab_err_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal1_read_gp_rab_err_sts_uint32(base_) != RAB_ERR_STS_RMSK) { return(13); }

  return(0);
}

#endif

/**
 * \brief Write RAB_ERR_BCLR using type uint32_t
 *
 * register description: Bit clear of Status error register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_gp_rab_err_bclr_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_ERR_BCLR_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_ERR_BCLR
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_err_bclr(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal1_write_gp_rab_err_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal1_read_gp_rab_err_sts_uint32(base_) != 0x0) { return(14); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal1_write_gp_rab_err_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal1_read_gp_rab_err_sts_uint32(base_) != 0x0) { return(15); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS0 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts0_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS0_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS0
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts0(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts0_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS0_RRST) { return(16); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS1 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts1_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS1_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS1
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts1(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts1_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS1_RRST) { return(17); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS2 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts2_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS2_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS2
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts2(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts2_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS2_RRST) { return(18); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS3 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts3_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS3_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS3
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts3(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts3_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS3_RRST) { return(19); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS4 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts4_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS4_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS4
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts4(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts4_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS4_RRST) { return(20); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS5 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts5_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS5_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS5
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts5(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts5_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS5_RRST) { return(21); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS6 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts6_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS6_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS6
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts6(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts6_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS6_RRST) { return(22); }

  return(0);
}

#endif

/**
 * \brief Read RAB_HIT_ERR_STS7 and returns uint32_t
 *
 * register description: Status hit errors register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_gp_rab_hit_err_sts7_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_HIT_ERR_STS7_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_HIT_ERR_STS7
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp_rab_hit_err_sts7(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal1_read_gp_rab_hit_err_sts7_uint32(base_);
  if (data_read_tmp != RAB_HIT_ERR_STS7_RRST) { return(23); }

  return(0);
}

#endif



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test all registers of gp
 *
 * autotest sequence
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_gp(uint32_t base_) {
  uint32_t retval = 0;
  if ((retval = hal1_autotest_gp_rab_cfg(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_it_mask(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_err_sts(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_err_bset(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_err_bclr(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts0(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts1(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts2(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts3(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts4(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts5(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts6(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_gp_rab_hit_err_sts7(base_)) != 0) return(retval);
  return(retval);
}

#endif

// end submodule: p2012_rab_gp
// ==============================================================

// ==============================================================
// start submodule: p2012_rab_entry

/**
 * \brief Read RAB_CTRL and returns uint32_t
 *
 * register description: RAB ENTRY control register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_entry_rab_ctrl_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_CTRL_ROFF));
}

/**
 * \brief Write RAB_CTRL using type uint32_t
 *
 * register description: RAB ENTRY control register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_entry_rab_ctrl_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_CTRL_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_CTRL
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_entry_rab_ctrl(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_entry_rab_ctrl_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_ctrl_uint32(base_) != (RAB_CTRL_RMSK & 0x0)) { return(24); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_ctrl_uint32(base_) != (RAB_CTRL_RMSK & 0xffffffff)) { return(25); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_ctrl_uint32(base_) != (RAB_CTRL_RMSK & 0xa5a5a5a5)) { return(26); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_ctrl_uint32(base_) != (RAB_CTRL_RMSK & 0x5a5a5a5a)) { return(27); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_ctrl_uint32(base_) != (RAB_CTRL_RMSK & 0x89abcdef)) { return(28); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_entry_rab_ctrl_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read RAB_SRC_RANGE_MIN and returns uint32_t
 *
 * register description: Source address range minimum register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_entry_rab_src_range_min_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_SRC_RANGE_MIN_ROFF));
}

/**
 * \brief Write RAB_SRC_RANGE_MIN using type uint32_t
 *
 * register description: Source address range minimum register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_entry_rab_src_range_min_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_SRC_RANGE_MIN_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_SRC_RANGE_MIN
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_entry_rab_src_range_min(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_entry_rab_src_range_min_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_min_uint32(base_) != (RAB_SRC_RANGE_MIN_RMSK & 0x0)) { return(29); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_min_uint32(base_) != (RAB_SRC_RANGE_MIN_RMSK & 0xffffffff)) { return(30); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_min_uint32(base_) != (RAB_SRC_RANGE_MIN_RMSK & 0xa5a5a5a5)) { return(31); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_min_uint32(base_) != (RAB_SRC_RANGE_MIN_RMSK & 0x5a5a5a5a)) { return(32); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_min_uint32(base_) != (RAB_SRC_RANGE_MIN_RMSK & 0x89abcdef)) { return(33); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_entry_rab_src_range_min_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read RAB_SRC_RANGE_MAX and returns uint32_t
 *
 * register description: Source range address maximum register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_entry_rab_src_range_max_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_SRC_RANGE_MAX_ROFF));
}

/**
 * \brief Write RAB_SRC_RANGE_MAX using type uint32_t
 *
 * register description: Source range address maximum register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_entry_rab_src_range_max_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_SRC_RANGE_MAX_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_SRC_RANGE_MAX
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_entry_rab_src_range_max(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_entry_rab_src_range_max_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_max_uint32(base_) != (RAB_SRC_RANGE_MAX_RMSK & 0x0)) { return(34); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_max_uint32(base_) != (RAB_SRC_RANGE_MAX_RMSK & 0xffffffff)) { return(35); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_max_uint32(base_) != (RAB_SRC_RANGE_MAX_RMSK & 0xa5a5a5a5)) { return(36); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_max_uint32(base_) != (RAB_SRC_RANGE_MAX_RMSK & 0x5a5a5a5a)) { return(37); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_src_range_max_uint32(base_) != (RAB_SRC_RANGE_MAX_RMSK & 0x89abcdef)) { return(38); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_entry_rab_src_range_max_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read RAB_DST_REMAP and returns uint32_t
 *
 * register description: Destination remapping address register
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_read_entry_rab_dst_remap_uint32(uint32_t base_) {
  return(p2012_read(base_ + RAB_DST_REMAP_ROFF));
}

/**
 * \brief Write RAB_DST_REMAP using type uint32_t
 *
 * register description: Destination remapping address register
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE void hal1_write_entry_rab_dst_remap_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + RAB_DST_REMAP_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test RAB_DST_REMAP
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_entry_rab_dst_remap(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal1_read_entry_rab_dst_remap_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_dst_remap_uint32(base_) != (RAB_DST_REMAP_RMSK & 0x0)) { return(39); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_dst_remap_uint32(base_) != (RAB_DST_REMAP_RMSK & 0xffffffff)) { return(40); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_dst_remap_uint32(base_) != (RAB_DST_REMAP_RMSK & 0xa5a5a5a5)) { return(41); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_dst_remap_uint32(base_) != (RAB_DST_REMAP_RMSK & 0x5a5a5a5a)) { return(42); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal1_read_entry_rab_dst_remap_uint32(base_) != (RAB_DST_REMAP_RMSK & 0x89abcdef)) { return(43); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal1_write_entry_rab_dst_remap_uint32(base_, data_write_tmp);

  return(0);
}

#endif



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test all registers of entry
 *
 * autotest sequence
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_RAB_IPGEN_STATIC_INLINE uint32_t hal1_autotest_entry(uint32_t base_) {
  uint32_t retval = 0;
  if ((retval = hal1_autotest_entry_rab_ctrl(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_entry_rab_src_range_min(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_entry_rab_src_range_max(base_)) != 0) return(retval);
  if ((retval = hal1_autotest_entry_rab_dst_remap(base_)) != 0) return(retval);
  return(retval);
}

#endif

// end submodule: p2012_rab_entry
// ==============================================================
#endif
