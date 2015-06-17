/***********************************************************************
 *
 * File: display/ip/displaytiming/stmclocklla.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_CLOCK_LLA_H
#define _STM_CLOCK_LLA_H

typedef enum {
  /* TVOUT */
  STM_CLK_PIX_MAIN,
  STM_CLK_PIX_HDDACS,
  STM_CLK_OUT_HDDACS,
  STM_CLK_PIX_AUX,
  STM_CLK_DENC,
  STM_CLK_OUT_SDDACS,
  /* DVO */
  STM_CLK_PIX_DVO,
  STM_CLK_OUT_DVO,
  /* HDMI */
  STM_CLK_HDMI_PHY,
  STM_CLK_HDMI_PHY_REJECTION = STM_CLK_HDMI_PHY,
  STM_CLK_TMDS_HDMI,
  STM_CLK_PIX_HDMI,
  /* COMPO */
  STM_CLK_PIX_GDP1,
  STM_CLK_PIX_GDP2,
  STM_CLK_PIX_GDP3,
  STM_CLK_PIX_GDP4,
  STM_CLK_PIX_PIP,
  STM_CLK_PIX_ALP,
  /* SPARE */
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
  STM_CLK_SRC_SD_EXT = STM_CLK_SRC_3,
  STM_CLK_SRC_SPARE  = 1024
} stm_clk_divider_output_source_t;


typedef enum {
  STM_CLK_DIV_1,
  STM_CLK_DIV_2,
  STM_CLK_DIV_4,
  STM_CLK_DIV_8
} stm_clk_divider_output_divide_t;


class CSTmClockLLA
{
public:
  CSTmClockLLA( struct vibe_clk *clk_src_map, uint32_t clk_src_mapsize
                , struct vibe_clk *clk_out_map, uint32_t clk_out_mapsize);

  virtual ~CSTmClockLLA(void);

  bool Enable(stm_clk_divider_output_name_t,
              stm_clk_divider_output_source_t src = STM_CLK_SRC_SPARE,
              stm_clk_divider_output_divide_t div = STM_CLK_DIV_1);
  bool Disable(stm_clk_divider_output_name_t);

  bool SetRate(stm_clk_divider_output_name_t name, uint32_t rate);
  uint32_t GetRate(stm_clk_divider_output_name_t name);
  bool SetParent(stm_clk_divider_output_name_t name, stm_clk_divider_output_source_t src);
  stm_clk_divider_output_source_t GetParent(stm_clk_divider_output_name_t name);

  bool isEnabled(stm_clk_divider_output_name_t) const;
  bool getDivide(stm_clk_divider_output_name_t, stm_clk_divider_output_divide_t *) const;
  bool getSource(stm_clk_divider_output_name_t, stm_clk_divider_output_source_t *) const;
  
  int Suspend(void);
  int Freeze() { return Suspend(); }
  int Resume(void);

protected:
  struct vibe_clk *m_sourceMap;
  uint32_t m_nSrcMapSize;
  struct vibe_clk *m_outputMap;
  uint32_t m_nOutMapSize;

  bool m_bIsSuspended;

  inline bool isAvailable(stm_clk_divider_output_name_t name) const;
  inline bool lookupSource(stm_clk_divider_output_source_t src, uint32_t *source) const;
  inline bool lookupOutput(stm_clk_divider_output_name_t name, uint32_t *output) const;

private:
  CSTmClockLLA(const CSTmClockLLA&);
  CSTmClockLLA& operator=(const CSTmClockLLA&);
};


inline bool CSTmClockLLA::lookupSource(stm_clk_divider_output_source_t src, uint32_t *source) const
{
  for(uint32_t i=0;i<m_nSrcMapSize;i++)
  {
    if(m_sourceMap[i].id == src)
    {
      *source = i;
      return true;
    }
  }

  return false;
}

inline bool CSTmClockLLA::lookupOutput(stm_clk_divider_output_name_t name, uint32_t *output) const
{
  for(uint32_t i=0;i<m_nOutMapSize;i++)
  {
    if(m_outputMap[i].id == name)
    {
      *output = i;
      return true;
    }
  }

  return false;
}


inline bool CSTmClockLLA::isAvailable(stm_clk_divider_output_name_t name) const
{
  uint32_t dummy;
  return lookupOutput(name,&dummy);
}

#endif // _STM_CLOCK_LLA_H
