/***********************************************************************
 *
 * File: display/ip/tvout/stmtvoutclkdivider.h
 * Copyright (c) 2009-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_TVOUT_CLK_DIVIDER_H
#define _STM_TVOUT_CLK_DIVIDER_H

typedef enum {
  /* STiH415 and later style TVOut clock names */
  STM_CLK_PIX_HDMI,
  STM_CLK_TMDS_HDMI,
  STM_CLK_HDMI_PHY_REJECTION,
  STM_CLK_PIX_DVO,
  STM_CLK_OUT_DVO,
  STM_CLK_PIX_HDDACS,
  STM_CLK_OUT_HDDACS,
  STM_CLK_DENC,
  STM_CLK_OUT_SDDACS,
  STM_CLK_PIX_MAIN,
  STM_CLK_PIX_AUX,
  STM_CLK_SPARE           /* means that this clock is not used */
} stm_clk_divider_output_name_t;


typedef enum {
  /*
   * Generic clock source names. Defined for up to the 16 cross-bar inputs
   * that are possible in configurations of the FlexClockGen IP.
   */
  STM_CLK_SRC_0,
  STM_CLK_SRC_1,
  STM_CLK_SRC_2,
  STM_CLK_SRC_3,
  STM_CLK_SRC_4,
  STM_CLK_SRC_5,
  STM_CLK_SRC_6,
  STM_CLK_SRC_7,
  STM_CLK_SRC_8,
  STM_CLK_SRC_9,
  STM_CLK_SRC_10,
  STM_CLK_SRC_11,
  STM_CLK_SRC_12,
  STM_CLK_SRC_13,
  STM_CLK_SRC_14,
  STM_CLK_SRC_15,
  /* STiH416/407 specific source names */
  STM_CLK_SRC_HD     = STM_CLK_SRC_0,
  STM_CLK_SRC_SD     = STM_CLK_SRC_1,
  STM_CLK_SRC_HD_EXT = STM_CLK_SRC_2,
  STM_CLK_SRC_SD_EXT = STM_CLK_SRC_3
} stm_clk_divider_output_source_t;


typedef enum {
  STM_CLK_DIV_1,
  STM_CLK_DIV_2,
  STM_CLK_DIV_4,
  STM_CLK_DIV_8
} stm_clk_divider_output_divide_t;


class CSTmTVOutClkDivider
{
public:
  CSTmTVOutClkDivider(const stm_clk_divider_output_name_t *outputmap,
                      int mapsize): m_outputMap(outputmap), m_nMapSize(mapsize) {}
  virtual ~CSTmTVOutClkDivider(void) {};

  virtual bool Enable(stm_clk_divider_output_name_t,
                      stm_clk_divider_output_source_t,
                      stm_clk_divider_output_divide_t) = 0;

  virtual bool Disable(stm_clk_divider_output_name_t) = 0;
  virtual bool isEnabled(stm_clk_divider_output_name_t) const = 0;
  virtual bool getDivide(stm_clk_divider_output_name_t, stm_clk_divider_output_divide_t *) const = 0;
  virtual bool getSource(stm_clk_divider_output_name_t, stm_clk_divider_output_source_t *) const = 0;

  inline bool isAvailable(stm_clk_divider_output_name_t name) const;
protected:
  const stm_clk_divider_output_name_t *m_outputMap;
  const int m_nMapSize;

  inline bool lookupOutput(stm_clk_divider_output_name_t name, int *output) const;

private:
  CSTmTVOutClkDivider(const CSTmTVOutClkDivider&);
  CSTmTVOutClkDivider& operator=(const CSTmTVOutClkDivider&);
};


inline bool CSTmTVOutClkDivider::isAvailable(stm_clk_divider_output_name_t name) const
{
  int dummy;
  return lookupOutput(name,&dummy);
}


inline bool CSTmTVOutClkDivider::lookupOutput(stm_clk_divider_output_name_t name, int *output) const
{
  for(int i=0;i<m_nMapSize;i++)
  {
    if(m_outputMap[i] == name)
    {
      *output = i;
      return true;
    }
  }

  return false;
}

#endif // _STM_TVOUT_CLK_DIVIDER_H
