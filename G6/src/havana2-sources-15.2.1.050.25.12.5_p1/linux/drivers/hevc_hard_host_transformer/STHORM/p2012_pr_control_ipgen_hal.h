/**
 * \brief Hardware Abstraction Layer for the PR control
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

#ifndef _p2012_pr_control_ipgen_hal_h_
#define _p2012_pr_control_ipgen_hal_h_


/**
 * \brief low level macro to read/write data
 */


/**
 * \brief memory map and register description
 */
#include "p2012_pr_control_ipgen_regmap.h"

/**
 * \brief used with doxygen for the documentation (doxygen can't support key words static inline)
 */
 #define P2012_PR_CONTROL_IPGEN_STATIC_INLINE static inline

/**
 * \brief Read BANK_PR_CTRL_FETCH_ENABLE and returns uint32_t
 *
 * register description: Processor fetch enable
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_fetch_enable_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_FETCH_ENABLE_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_FETCH_ENABLE using type uint32_t
 *
 * register description: Processor fetch enable
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_fetch_enable_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_FETCH_ENABLE_ROFF, val_);
}

/**
 * \brief Read BANK_PR_CTRL_IT_NMI_STS and returns uint32_t
 *
 * register description: Status for processor non maskable interrupt
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_it_nmi_sts_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_IT_NMI_STS_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_NMI_STS
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_nmi_sts(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_nmi_sts_uint32(base_);
  if (data_read_tmp != BANK_PR_CTRL_IT_NMI_STS_RRST) { return(1); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_CTRL_IT_NMI_BSET using type uint32_t
 *
 * register description: Set processor non maskable interrupt
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_it_nmi_bset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_IT_NMI_BSET_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_NMI_BSET
 *
 * autotest sequence:  W0
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_nmi_bset(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 0
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_it_nmi_bset_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_CTRL_IT_NMI_BCLR using type uint32_t
 *
 * register description: Clear processor non maskable interrupt
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_it_nmi_bclr_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_IT_NMI_BCLR_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_NMI_BCLR
 *
 * autotest sequence:  W0
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_nmi_bclr(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 0
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_it_nmi_bclr_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_CTRL_SOFT_RESET and returns uint32_t
 *
 * register description: Processor soft reset
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_soft_reset_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_SOFT_RESET_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_SOFT_RESET using type uint32_t
 *
 * register description: Processor soft reset
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_soft_reset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_SOFT_RESET_ROFF, val_);
}

/**
 * \brief Read BANK_PR_CTRL_BOOT_ADDRESS and returns uint32_t
 *
 * register description: Processor boot address
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_BOOT_ADDRESS_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_BOOT_ADDRESS using type uint32_t
 *
 * register description: Processor boot address
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_BOOT_ADDRESS_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_BOOT_ADDRESS
 *
 * autotest sequence:  RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_boot_address(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_) != (BANK_PR_CTRL_BOOT_ADDRESS_RMSK & 0x0)) { return(2); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_) != (BANK_PR_CTRL_BOOT_ADDRESS_RMSK & 0xffffffff)) { return(3); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_) != (BANK_PR_CTRL_BOOT_ADDRESS_RMSK & 0xa5a5a5a5)) { return(4); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_) != (BANK_PR_CTRL_BOOT_ADDRESS_RMSK & 0x5a5a5a5a)) { return(5); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_boot_address_uint32(base_) != (BANK_PR_CTRL_BOOT_ADDRESS_RMSK & 0x89abcdef)) { return(6); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_CTRL_IT_HOST and returns uint32_t
 *
 * register description: IT to host processor generation
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_it_host_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_IT_HOST_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_IT_HOST using type uint32_t
 *
 * register description: IT to host processor generation
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_it_host_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_IT_HOST_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_HOST
 *
 * autotest sequence:  RST-W0-R0
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_host(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_host_uint32(base_);
  if (data_read_tmp != BANK_PR_CTRL_IT_HOST_RRST) { return(7); }

  // test write 0
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_it_host_uint32(base_, data_write_tmp);

  // test read 0
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_host_uint32(base_);
  if (data_read_tmp != 0x0) { return(8); }

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_CTRL_ERROR_STS and returns uint32_t
 *
 * register description: Error status
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_ERROR_STS_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_ERROR_STS
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_error_sts(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(base_);
  if (data_read_tmp != BANK_PR_CTRL_ERROR_STS_RRST) { return(9); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_CTRL_ERROR_BSET using type uint32_t
 *
 * register description: Set bit in error status
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_error_bset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_ERROR_BSET_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_ERROR_BSET
 *
 * autotest sequence:  W1-STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_error_bset(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_error_bset_uint32(base_, data_write_tmp);

  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_error_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(base_) != BANK_PR_CTRL_ERROR_STS_RMSK) { return(10); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_error_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(base_) != BANK_PR_CTRL_ERROR_STS_RMSK) { return(11); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_CTRL_ERROR_BCLR using type uint32_t
 *
 * register description: Clear bit in error status
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_error_bclr_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_ERROR_BCLR_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_ERROR_BCLR
 *
 * autotest sequence:  W1-STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_error_bclr(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_error_bclr_uint32(base_, data_write_tmp);

  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_error_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(base_) != 0x0) { return(12); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_error_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_ctrl_error_sts_uint32(base_) != 0x0) { return(13); }

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_CTRL_IT_HOST_ERROR_MASK and returns uint32_t
 *
 * register description: Mask for IT error on host processor
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_IT_HOST_ERROR_MASK_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_IT_HOST_ERROR_MASK using type uint32_t
 *
 * register description: Mask for IT error on host processor
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_IT_HOST_ERROR_MASK_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_HOST_ERROR_MASK
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_host_error_mask(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_);
  if (data_read_tmp != BANK_PR_CTRL_IT_HOST_ERROR_MASK_RRST) { return(14); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_HOST_ERROR_MASK_RMSK & 0x0)) { return(15); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_HOST_ERROR_MASK_RMSK & 0xffffffff)) { return(16); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_HOST_ERROR_MASK_RMSK & 0xa5a5a5a5)) { return(17); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_HOST_ERROR_MASK_RMSK & 0x5a5a5a5a)) { return(18); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_HOST_ERROR_MASK_RMSK & 0x89abcdef)) { return(19); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_ctrl_it_host_error_mask_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_CTRL_IT_LOCAL_ERROR_MASK and returns uint32_t
 *
 * register description: Mask for IT error on local processor
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_ROFF));
}

/**
 * \brief Write BANK_PR_CTRL_IT_LOCAL_ERROR_MASK using type uint32_t
 *
 * register description: Mask for IT error on local processor
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_IT_LOCAL_ERROR_MASK
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_it_local_error_mask(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_);
  if (data_read_tmp != BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RRST) { return(20); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RMSK & 0x0)) { return(21); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RMSK & 0xffffffff)) { return(22); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RMSK & 0xa5a5a5a5)) { return(23); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RMSK & 0x5a5a5a5a)) { return(24); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_) != (BANK_PR_CTRL_IT_LOCAL_ERROR_MASK_RMSK & 0x89abcdef)) { return(25); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_ctrl_it_local_error_mask_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_CTRL_CFG using type uint32_t
 *
 * register description: Processor configuration
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_ctrl_cfg_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_CTRL_CFG_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_CTRL_CFG
 *
 * autotest sequence:  W0
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_ctrl_cfg(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 0
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_ctrl_cfg_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_STATUS_STS and returns uint32_t
 *
 * register description: Interrupt AND combining status
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_STATUS_STS_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_STATUS_STS
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_status_sts(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_STATUS_STS_RRST) { return(26); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_COMB_ITAND_STATUS_BSET using type uint32_t
 *
 * register description: Set bit into PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_status_bset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_STATUS_BSET_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_STATUS_BSET
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_status_bset(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_status_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(base_) != BANK_PR_COMB_ITAND_STATUS_STS_RMSK) { return(27); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_status_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(base_) != BANK_PR_COMB_ITAND_STATUS_STS_RMSK) { return(28); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_COMB_ITAND_STATUS_BCLR using type uint32_t
 *
 * register description: Clear bit into PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_status_bclr_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_STATUS_BCLR_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_STATUS_BCLR
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_status_bclr(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_status_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(base_) != 0x0) { return(29); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_status_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_comb_itand_status_sts_uint32(base_) != 0x0) { return(30); }

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK0 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK0_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK0 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK0_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK0
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask0(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK0_RRST) { return(31); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_) != (BANK_PR_COMB_ITAND_MASK0_RMSK & 0x0)) { return(32); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_) != (BANK_PR_COMB_ITAND_MASK0_RMSK & 0xffffffff)) { return(33); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_) != (BANK_PR_COMB_ITAND_MASK0_RMSK & 0xa5a5a5a5)) { return(34); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_) != (BANK_PR_COMB_ITAND_MASK0_RMSK & 0x5a5a5a5a)) { return(35); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask0_uint32(base_) != (BANK_PR_COMB_ITAND_MASK0_RMSK & 0x89abcdef)) { return(36); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask0_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK1 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK1_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK1 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK1_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK1
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask1(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK1_RRST) { return(37); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_) != (BANK_PR_COMB_ITAND_MASK1_RMSK & 0x0)) { return(38); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_) != (BANK_PR_COMB_ITAND_MASK1_RMSK & 0xffffffff)) { return(39); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_) != (BANK_PR_COMB_ITAND_MASK1_RMSK & 0xa5a5a5a5)) { return(40); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_) != (BANK_PR_COMB_ITAND_MASK1_RMSK & 0x5a5a5a5a)) { return(41); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask1_uint32(base_) != (BANK_PR_COMB_ITAND_MASK1_RMSK & 0x89abcdef)) { return(42); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask1_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK2 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK2_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK2 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK2_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK2
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask2(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK2_RRST) { return(43); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_) != (BANK_PR_COMB_ITAND_MASK2_RMSK & 0x0)) { return(44); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_) != (BANK_PR_COMB_ITAND_MASK2_RMSK & 0xffffffff)) { return(45); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_) != (BANK_PR_COMB_ITAND_MASK2_RMSK & 0xa5a5a5a5)) { return(46); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_) != (BANK_PR_COMB_ITAND_MASK2_RMSK & 0x5a5a5a5a)) { return(47); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask2_uint32(base_) != (BANK_PR_COMB_ITAND_MASK2_RMSK & 0x89abcdef)) { return(48); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask2_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK3 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK3_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK3 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK3_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK3
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask3(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK3_RRST) { return(49); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_) != (BANK_PR_COMB_ITAND_MASK3_RMSK & 0x0)) { return(50); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_) != (BANK_PR_COMB_ITAND_MASK3_RMSK & 0xffffffff)) { return(51); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_) != (BANK_PR_COMB_ITAND_MASK3_RMSK & 0xa5a5a5a5)) { return(52); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_) != (BANK_PR_COMB_ITAND_MASK3_RMSK & 0x5a5a5a5a)) { return(53); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask3_uint32(base_) != (BANK_PR_COMB_ITAND_MASK3_RMSK & 0x89abcdef)) { return(54); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask3_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK4 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK4_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK4 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK4_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK4
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask4(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK4_RRST) { return(55); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_) != (BANK_PR_COMB_ITAND_MASK4_RMSK & 0x0)) { return(56); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_) != (BANK_PR_COMB_ITAND_MASK4_RMSK & 0xffffffff)) { return(57); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_) != (BANK_PR_COMB_ITAND_MASK4_RMSK & 0xa5a5a5a5)) { return(58); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_) != (BANK_PR_COMB_ITAND_MASK4_RMSK & 0x5a5a5a5a)) { return(59); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask4_uint32(base_) != (BANK_PR_COMB_ITAND_MASK4_RMSK & 0x89abcdef)) { return(60); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask4_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK5 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK5_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK5 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK5_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK5
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask5(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK5_RRST) { return(61); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_) != (BANK_PR_COMB_ITAND_MASK5_RMSK & 0x0)) { return(62); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_) != (BANK_PR_COMB_ITAND_MASK5_RMSK & 0xffffffff)) { return(63); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_) != (BANK_PR_COMB_ITAND_MASK5_RMSK & 0xa5a5a5a5)) { return(64); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_) != (BANK_PR_COMB_ITAND_MASK5_RMSK & 0x5a5a5a5a)) { return(65); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask5_uint32(base_) != (BANK_PR_COMB_ITAND_MASK5_RMSK & 0x89abcdef)) { return(66); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask5_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK6 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK6_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK6 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK6_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK6
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask6(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK6_RRST) { return(67); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_) != (BANK_PR_COMB_ITAND_MASK6_RMSK & 0x0)) { return(68); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_) != (BANK_PR_COMB_ITAND_MASK6_RMSK & 0xffffffff)) { return(69); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_) != (BANK_PR_COMB_ITAND_MASK6_RMSK & 0xa5a5a5a5)) { return(70); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_) != (BANK_PR_COMB_ITAND_MASK6_RMSK & 0x5a5a5a5a)) { return(71); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask6_uint32(base_) != (BANK_PR_COMB_ITAND_MASK6_RMSK & 0x89abcdef)) { return(72); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask6_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MASK7 and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MASK7_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MASK7 using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITAND_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MASK7_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MASK7
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mask7(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MASK7_RRST) { return(73); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_) != (BANK_PR_COMB_ITAND_MASK7_RMSK & 0x0)) { return(74); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_) != (BANK_PR_COMB_ITAND_MASK7_RMSK & 0xffffffff)) { return(75); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_) != (BANK_PR_COMB_ITAND_MASK7_RMSK & 0xa5a5a5a5)) { return(76); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_) != (BANK_PR_COMB_ITAND_MASK7_RMSK & 0x5a5a5a5a)) { return(77); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mask7_uint32(base_) != (BANK_PR_COMB_ITAND_MASK7_RMSK & 0x89abcdef)) { return(78); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mask7_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITAND_MODE and returns uint32_t
 *
 * register description: Interrupt mode
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itand_mode_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITAND_MODE_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITAND_MODE using type uint32_t
 *
 * register description: Interrupt mode
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itand_mode_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITAND_MODE_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITAND_MODE
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itand_mode(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITAND_MODE_RRST) { return(79); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_) != (BANK_PR_COMB_ITAND_MODE_RMSK & 0x0)) { return(80); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_) != (BANK_PR_COMB_ITAND_MODE_RMSK & 0xffffffff)) { return(81); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_) != (BANK_PR_COMB_ITAND_MODE_RMSK & 0xa5a5a5a5)) { return(82); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_) != (BANK_PR_COMB_ITAND_MODE_RMSK & 0x5a5a5a5a)) { return(83); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itand_mode_uint32(base_) != (BANK_PR_COMB_ITAND_MODE_RMSK & 0x89abcdef)) { return(84); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itand_mode_uint32(base_, data_write_tmp);

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITOR_STATUS_STS and returns uint32_t
 *
 * register description: Interrupt OR combining status
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITOR_STATUS_STS_ROFF));
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITOR_STATUS_STS
 *
 * autotest sequence:  RST
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itor_status_sts(uint32_t base_) {
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITOR_STATUS_STS_RRST) { return(85); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_COMB_ITOR_STATUS_BSET using type uint32_t
 *
 * register description: Set bit into PR_COMB_ITOR_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itor_status_bset_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITOR_STATUS_BSET_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITOR_STATUS_BSET
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itor_status_bset(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itor_status_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(base_) != BANK_PR_COMB_ITOR_STATUS_STS_RMSK) { return(86); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itor_status_bset_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 1
  if (hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(base_) != BANK_PR_COMB_ITOR_STATUS_STS_RMSK) { return(87); }

  return(0);
}

#endif

/**
 * \brief Write BANK_PR_COMB_ITOR_STATUS_BCLR using type uint32_t
 *
 * register description: Clear bit into PR_COMB_ITOR_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itor_status_bclr_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITOR_STATUS_BCLR_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITOR_STATUS_BCLR
 *
 * autotest sequence:  STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itor_status_bclr(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  // test write 1
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itor_status_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(base_) != 0x0) { return(88); }

  // test write 0 (no effects)
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itor_status_bclr_uint32(base_, data_write_tmp);

  // test read STS for significant bits at 0
  if (hal_read_pr_control_bank_pr_comb_itor_status_sts_uint32(base_) != 0x0) { return(89); }

  return(0);
}

#endif

/**
 * \brief Read BANK_PR_COMB_ITOR_MASK and returns uint32_t
 *
 * register description: Mask bits in PR_COMB_ITOR_STATUS_STS
 *
 * \param[in]  base_     base address
 *
 * \return     returns  uint32_t
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_read_pr_control_bank_pr_comb_itor_mask_uint32(uint32_t base_) {
  return(p2012_read(base_ + BANK_PR_COMB_ITOR_MASK_ROFF));
}

/**
 * \brief Write BANK_PR_COMB_ITOR_MASK using type uint32_t
 *
 * register description: Mask bits in PR_COMB_ITOR_STATUS_STS
 *
 * \param[in]  base_     base address
 * \param[in]  val_      register value using base type uint32_t
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE void hal_write_pr_control_bank_pr_comb_itor_mask_uint32(uint32_t base_, uint32_t val_) {
  p2012_write(base_ + BANK_PR_COMB_ITOR_MASK_ROFF, val_);
}



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test BANK_PR_COMB_ITOR_MASK
 *
 * autotest sequence:  RST-RW
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control_bank_pr_comb_itor_mask(uint32_t base_) {
  uint32_t data_write_tmp = 0xffffffff;
  uint32_t data_read_tmp = 0x0;
  // test reset value
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_);
  if (data_read_tmp != BANK_PR_COMB_ITOR_MASK_RRST) { return(90); }

  // test read data
  data_read_tmp = hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_);
  // test write some bits
  data_write_tmp = 0x0;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_) != (BANK_PR_COMB_ITOR_MASK_RMSK & 0x0)) { return(91); }

  // test write some bits
  data_write_tmp = 0xffffffff;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_) != (BANK_PR_COMB_ITOR_MASK_RMSK & 0xffffffff)) { return(92); }

  // test write some bits
  data_write_tmp = 0xa5a5a5a5;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_) != (BANK_PR_COMB_ITOR_MASK_RMSK & 0xa5a5a5a5)) { return(93); }

  // test write some bits
  data_write_tmp = 0x5a5a5a5a;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_) != (BANK_PR_COMB_ITOR_MASK_RMSK & 0x5a5a5a5a)) { return(94); }

  // test write some bits
  data_write_tmp = 0x89abcdef;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  // test read 1 for significant bits
  if (hal_read_pr_control_bank_pr_comb_itor_mask_uint32(base_) != (BANK_PR_COMB_ITOR_MASK_RMSK & 0x89abcdef)) { return(95); }

  // restore initial value
  data_write_tmp = data_read_tmp;
  hal_write_pr_control_bank_pr_comb_itor_mask_uint32(base_, data_write_tmp);

  return(0);
}

#endif



#ifdef HAL_ACTIVATE_AUTOTEST

/**
 * \brief Auto test all registers of pr_control
 *
 * autotest sequence
 *
 * \param[in]  base_     base address
 *
 * \return     returns   0 if success, error value otherwise
 *
 */
P2012_PR_CONTROL_IPGEN_STATIC_INLINE uint32_t hal_autotest_pr_control(uint32_t base_) {
  uint32_t retval = 0;
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_nmi_sts(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_nmi_bset(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_nmi_bclr(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_boot_address(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_host(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_error_sts(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_error_bset(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_error_bclr(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_host_error_mask(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_it_local_error_mask(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_ctrl_cfg(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_status_sts(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_status_bset(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_status_bclr(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask0(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask1(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask2(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask3(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask4(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask5(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask6(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mask7(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itand_mode(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itor_status_sts(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itor_status_bset(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itor_status_bclr(base_)) != 0) return(retval);
  if ((retval = hal_autotest_pr_control_bank_pr_comb_itor_mask(base_)) != 0) return(retval);
  return(retval);
}

#endif
#endif
