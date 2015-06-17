/***********************************************************************
 *
 * File: linux/tests/panel/panel.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/
#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <linux/fb.h>
#include <linux/kernel/drivers/video/stmfb.h>

#include "panel.h"
#include "paneldata.h"

static PanelParams_t const *pSelectedPanelParams = NULL;
static unsigned int CurrentPanelID = DEFAULT_PANEL;

PanelParams_t const *pPanelParams[] =
{
  &PanelParams_CMO_M216H1LO1,
  &PanelParams_CMO_M236H5_LOA,
   NULL,   /*  place holder for end of list */
};

void usage(void)
{
  printf("Usage: panel [options]\n");
  printf("\t-a,--list out the supported panel in the app\n");
  printf("\t-c,--color remap\n");
  printf("\t-d,--dither mode\n");
  printf("\t-e,--look up table [1][2] enable/disable\n");
  printf("\t-f,--flip\n");
  printf("\t-g,--lock method\n");
  printf("\t-h,--help\n");
  printf("\t-l,--look up [1][2] table write/read\n");
  printf("\t-n,--Flagline\n");
  printf("\t-p,--power control\n");
  printf("\t-s,--select panel\n");
}

int find_panel(int panelID);
int select_panel(int fbfd, int panelID);
int list_panel(void);

int main(int argc, char **argv)
{
  int fbfd;
  int req_change_panel = 0;
  int panel= 0;
  int req_list_panel = 0;

  char *devname       = "/dev/fb0";
  int option;

  static struct option long_options[] = {
          { "list-panel"             , 0, 0, 'a' },
          { "select-panel"           , 1, 0, 's' },
          { 0, 0, 0, 0 }
  };

  while((option = getopt_long (argc, argv, "as:", long_options, NULL)) != -1)
  {
    switch(option)
    {
      case 'a':
      {
        req_list_panel = 1;
        break;
      }

      case 's':
      {
        panel = atoi(optarg);
        if(panel<0 || panel>=MAX_NUM_SUPPORTED_PANEL)
        {
          fprintf(stderr,"Panel value out of range\n");
          exit(1);
        }
        else
        {
          req_change_panel = 1;
        }

        break;
      }

      default:
      {
        usage();
        exit(1);
      }
    }
  }

  if((fbfd = open(devname, O_RDWR)) < 0)
  {
    fprintf (stderr, "Unable to open %s: %d (%m)\n", devname, errno);
    exit(1);
  }

  if(req_list_panel)
  {
    list_panel();
  }

  if(req_change_panel)
  {
    printf("PanelID %d is selected.\n", panel);
    find_panel(panel);
    select_panel(fbfd, panel);
  }

  return 0;
}

int find_panel(int panelID)
{
  int PanelIndex;
  int PanelFound=0;

  if(panelID >= MAX_NUM_SUPPORTED_PANEL)
  {
    printf("Invalid panel select=%d\n", panelID); 
    return(1);
  }

    /* Look for the Panel form the list */        
    for(PanelIndex =0; pPanelParams[PanelIndex]!= NULL; PanelIndex++)
    {
        if (pPanelParams[PanelIndex]->panelID == panelID)
        {
            pSelectedPanelParams = pPanelParams[PanelIndex];
            PanelFound  = 1;
            break;
        }
    }

    if(!PanelFound)
    {
      printf("Invalid panel select=%d\n", panelID); 
      return (1);
    }

    if(!pSelectedPanelParams)
    {
      printf("Panel is not selected.\n"); 
      return (1);
    }
    CurrentPanelID = (unsigned int) panelID;

  return (0);
}

int select_panel(int fbfd, int panelID)
{
  struct stmfbio_panel_config panel_config;

  panel_config.lookup_table1_p = (unsigned int)pSelectedPanelParams->lookup_table1_p;
  panel_config.lookup_table2_p = (unsigned int)pSelectedPanelParams->lookup_table2_p;
  panel_config.linear_color_remap_table_p = \
  (unsigned int)pSelectedPanelParams->linear_color_remap_table_p;
  panel_config.pwr_to_de_delay_during_power_on = \
  pSelectedPanelParams->pwr_to_de_delay_during_power_on;
  panel_config.de_to_bklt_on_delay_during_power_on = \
   pSelectedPanelParams->de_to_bklt_on_delay_during_power_on;
  panel_config.de_to_pwr_delay_during_power_off = \
  pSelectedPanelParams->de_to_pwr_delay_during_power_off;
  panel_config.bklt_to_de_off_delay_during_power_off = \
  pSelectedPanelParams->bklt_to_de_off_delay_during_power_off;
  panel_config.enable_lut1 = pSelectedPanelParams->enable_lut1;
  panel_config.enable_lut2 = pSelectedPanelParams->enable_lut2;
  panel_config.afr_enable = pSelectedPanelParams->afr_enable;
  panel_config.is_half_display_clock = pSelectedPanelParams->is_half_display_clock;
  panel_config.dither = pSelectedPanelParams->dither;
  panel_config.lock_method = pSelectedPanelParams->lock_method ;
  panel_config.vertical_refresh_rate = pSelectedPanelParams->vertical_refresh_rate;
  panel_config.active_area_width = pSelectedPanelParams->active_area_width;
  panel_config.active_area_height = pSelectedPanelParams->active_area_height;
  panel_config.pixels_per_line = pSelectedPanelParams->pixels_per_line;
  panel_config.lines_per_frame = pSelectedPanelParams->lines_per_frame;
  panel_config.pixel_clock_freq = pSelectedPanelParams->pixel_clock_freq;
  panel_config.active_area_start_pixel = pSelectedPanelParams->active_area_start_pixel;
  panel_config.active_area_start_line = pSelectedPanelParams->active_area_start_line;
  panel_config.hsync_width = pSelectedPanelParams->hsync_width;
  panel_config.vsync_width = pSelectedPanelParams->vsync_width;
  panel_config.hsync_polarity = pSelectedPanelParams->hsync_polarity;
  panel_config.vsync_polarity = pSelectedPanelParams->vsync_polarity;


  ioctl(fbfd, STMFBIO_SET_PANEL_CONFIG, &panel_config);

  return (0);
}

int list_panel(void)
{
  int PanelIndex;
  printf("Supported Panel List\n");
  for(PanelIndex=0; PanelIndex<MAX_NUM_SUPPORTED_PANEL; PanelIndex++)
  {
    printf("\tPanel \tID=%d \tName=%s\n", \
    pPanelParams[PanelIndex]->panelID, pPanelParams[PanelIndex]->panelname);
  }

  return (0);
}
