/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutclkdivider.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_TVOUT_CLK_DIVIDER_H
#define _STM_TVOUT_CLK_DIVIDER_H

typedef enum {
  STM_CLK_PIX_SD,
  STM_CLK_PIX_ED,
  STM_CLK_PIX_HD,
  STM_CLK_DISP_SD,
  STM_CLK_DISP_ED,
  STM_CLK_DISP_HD,
  STM_CLK_DISP_PIP,
  STM_CLK_PIP,
  STM_CLK_656,
  STM_CLK_GDP1,
  STM_CLK_GDP2
} stm_clk_divider_output_name_t;


typedef enum {
  STM_CLK_SRC_0,
  STM_CLK_SRC_1
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
