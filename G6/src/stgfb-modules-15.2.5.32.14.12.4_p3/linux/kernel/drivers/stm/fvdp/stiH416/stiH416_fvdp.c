/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/fvdp/stiH416/stiH416_fvdp.c
 * Copyright (c) 2011-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/stm/stih416.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/pm_runtime.h>

#include <fvdp/fvdp.h>

#include "fvdp_module_debug.h"
#include "fvdp_module.h"

#include <vibe_os.h>

#define STiH416_FVDP_REGISTER_BASE          0xFD000000
#define STiH416_FVDP_REG_ADDR_SIZE          0x000C0000

#define SYS_CFG7564_RST_N_FVDP_AUX_VID_PIPE      (1L<<5)
#define SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_BE  (1L<<6)
#define SYS_CFG7564_RST_N_FVDP_PIP_VID_PIPE      (1L<<7)
#define SYS_CFG7564_RST_N_FVDP_INPUT_VID_MUX     (1L<<9)
#define SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_FE  (1L<<12)
#define SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_EN  (1L<<14)

#define FVDP_ENC_INT    STIH416_IRQ(105)

typedef enum {
  /* FVDP clocks */
  NO_PARENT_CLOCK = -1,
  /* Clocks configured by this driver */
  CLOCK_MPE_FS0_VCO,
  CLOCK_MPE_VCPU,
  CLOCK_MPE_FS0_CH3,
  CLOCK_MPE_FVDP_PROC,
  CLOCK_MPE_PIX_MAIN_CRU,
  CLOCK_MPE_PIX_AUX_CRU,
  CLOCK_MPE_PIX_XFER_BE_COMPO,
  CLOCK_MPE_PIX_XFER_PIP_COMPO,
  CLOCK_MPE_PIX_XFER_AUX_COMPO,
  CLOCK_MPE_VSENS,
  /* Clocks used for indicating that they are parent clocks of previous one */
  CLOCK_MPE_F_VCC_HD,
  CLOCK_MPE_F_VCC_SD,
  /* Following clocks are required for uploading firmware into STxP70 TCDM/TCPM through DMA (streamer) transfer */
  CLOCK_M_TX_ICN_VDP_0,
  CLOCK_M_RX_ICN_VDP_0,
  NB_CLOCKS_FVDP,
} fvdp_clock_t;

enum _mem_res {
  /* Memory resources */
  MEM_FVDP,
  NB_MEM_RES
};

enum _sysconf {
  /* Sysconf resources */
  SYSCONF_RESET,
  NB_SYSCONF
};

struct fvdp_clock {
    const char    *name;
    unsigned long  rate;
    fvdp_clock_t   parent;
};

struct fvdp_sysconf {
    const char    *name;
    unsigned int   sys_reg;
    unsigned int   lsb;
    unsigned int   msb;
};

struct fvdp_platform_data {
    struct fvdp_clock      clock[NB_CLOCKS_FVDP];
    struct fvdp_sysconf    sysconf[NB_SYSCONF];
    struct stm_pad_config  gpio_config;
};

struct fvdp_gpio {
    /* To know if gpio should be used or not */
    bool           is_used;

    /* For enabling/disabling gpio interrupt and checking gpio state*/
    unsigned int   gpio_id;
    unsigned int   irq_num;

    /* FVDP interrupt to detect end of scaling */
    struct stm_pad_state *gpio_pad;
};

struct fvdp_driver_data {
    /* For accessing to FVDP registers and doing the fvdp_hw_power_up/fvdp_hw_power_down */
    uint32_t             *p_resource[NB_MEM_RES];
    struct sysconf_field *p_sysconf[NB_SYSCONF];

    /* For playing with FVDP clocks */
    struct clk           *clock[NB_CLOCKS_FVDP];

    /* For playing with GPIO used for simulating end of scaling */
    struct fvdp_gpio      gpio;

    /*Chip version*/
    unsigned int          chip_version_major;
    unsigned int          chip_version_minor;
};

const char* fvdp_driver_name = "stm-fvdp";

const char* mem_res_name[] = {
    [MEM_FVDP]    = "fvdp_registers",
};


/* Empty device release function to avoid WARNING from Linux Kernel at module unload */
static void stub_device_release(struct device *dev)
{
}

static struct fvdp_platform_data fvdp_data = {
    .clock = {
        /* Clocks configured by this driver */
        [CLOCK_MPE_FS0_VCO] = {
            .name    = "CLK_M_F_FS_VCO",
            .rate    = 600000000,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_MPE_VCPU] = {
            .name    = "CLK_M_FVDP_VCPU",
            .rate    = 350000000,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_MPE_FS0_CH3] = {
            .name    = "CLK_M_FVDP_PROC_FS",
            .rate    = 330000000,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_MPE_FVDP_PROC] = {
            .name    = "CLK_M_FVDP_PROC",
            .rate    = 0,
            .parent  = CLOCK_MPE_FS0_CH3,
        },
        [CLOCK_MPE_PIX_MAIN_CRU] = {
            .name    = "CLK_M_PIX_MAIN_CRU",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_HD,
        },
        [CLOCK_MPE_PIX_AUX_CRU] = {
            .name    = "CLK_M_PIX_AUX_CRU",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_HD,
        },
        [CLOCK_MPE_PIX_XFER_BE_COMPO] = {
            .name    = "CLK_M_XFER_BE_COMPO",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_HD,
        },
        [CLOCK_MPE_PIX_XFER_PIP_COMPO] = {
            .name    = "CLK_M_XFER_PIP_COMPO",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_HD,
        },
        [CLOCK_MPE_PIX_XFER_AUX_COMPO] = {
            .name    = "CLK_M_XFER_AUX_COMPO",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_SD,
        },
        [CLOCK_MPE_VSENS] = {
            .name    = "CLK_M_VSENS",
            .rate    = 0,
            .parent  = CLOCK_MPE_F_VCC_HD,
        },
        /* Clocks used for indicating that they are parent clocks of previous one */
        [CLOCK_MPE_F_VCC_HD] = {
            .name    = "CLK_M_F_VCC_HD",
            .rate    = 0,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_MPE_F_VCC_SD] = {
            .name    = "CLK_M_F_VCC_SD",
            .rate    = 0,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_M_TX_ICN_VDP_0] = {
            .name    = "CLK_M_TX_ICN_VDP_0",
            .rate    = 0,
            .parent  = NO_PARENT_CLOCK,
        },
        [CLOCK_M_RX_ICN_VDP_0] = {
            .name    = "CLK_M_RX_ICN_VDP_0",
            .rate    = 0,
            .parent  = NO_PARENT_CLOCK,
        },
    },
    .sysconf = {
        [SYSCONF_RESET] = {
            .name     = "FVDP-Reset",
            .sys_reg  = 7564,
            .lsb      = 0,
            .msb      = 31,
        },
    },
    .gpio_config = {
        .gpios_num = 1,
        .gpios = (struct stm_pad_gpio []) {
            /* Set GPIO104 pin2 in AlternateFunc 5 and also configure both SYSCONF registers:
             *  - STM_PAD_SYSCONF(SYSCONF(6001),  8, 10, 0x5) means Alternate function output control for PIO104 pin2 so PIO104_2_SELECTOR[10:8]=101b for AltFunc5
             *  - STM_PAD_SYSCONF(SYSCONF(6040), 10, 10, 0x1) means Output Enable pad control for PIO Alternate Functions so SYSCFG_PIO104_2_OE[10]=1b
             */
            STIH416_PAD_PIO_BIDIR_PULL_UP(104, 2, 5),
        },
        .sysconfs_num = 1,
        .sysconfs = (struct stm_pad_sysconf []) {
            /* In TV debug configuration register, set SEL_DBGBUS[2:0] = 110b */
            STM_PAD_SYSCONF(SYSCONF(8510),  0,  2, 0x6),
        },
        .custom_claim   = NULL,
        .custom_release = NULL,
    },
};

static struct platform_device fvdp_device[] = {
    [0] = {
            .name = "stm-fvdp",
            .id = 0,

            .dev.release = stub_device_release,

            .num_resources = NB_MEM_RES, /* type of resources= IO, MEM, IRQ, DMA */
            .resource = (struct resource[])
            {
                STM_PLAT_RESOURCE_MEM_NAMED("fvdp_registers",  STiH416_FVDP_REGISTER_BASE, STiH416_FVDP_REG_ADDR_SIZE),
            },
            .dev.platform_data = (void *)(uintptr_t)&fvdp_data,
          },
};

static struct platform_device *fvdp_devices[] = {
        &fvdp_device[0],
};

void fvdp_hw_power_up(struct platform_device *pdev)
{
    struct fvdp_driver_data *context;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);
    if(context)
    {
        unsigned long val;

        /*
         * Power On FVDP chip :
         * Now make sure all the fvdp related MPE IP blocks are out of reset
         */
        val = sysconf_read(context->p_sysconf[SYSCONF_RESET]);
        val |= (
            SYS_CFG7564_RST_N_FVDP_AUX_VID_PIPE      |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_BE  |
            SYS_CFG7564_RST_N_FVDP_PIP_VID_PIPE      |
            SYS_CFG7564_RST_N_FVDP_INPUT_VID_MUX     |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_FE  |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_EN
        );

        sysconf_write(context->p_sysconf[SYSCONF_RESET], val);

        stm_fvdp_printd(">>> power up MPE RST(31-0): %08x\n",
            (unsigned int)sysconf_read(context->p_sysconf[SYSCONF_RESET]) );
    }
}

void fvdp_hw_power_down(struct platform_device *pdev)
{
    struct fvdp_driver_data *context;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);
    if(context)
    {
        unsigned long val;

        /*
         * Power Down FVDP chip :
         * Set all the fvdp related MPE IP blocks in reset
         */
        val = sysconf_read(context->p_sysconf[SYSCONF_RESET]);
        val |= ~(
            SYS_CFG7564_RST_N_FVDP_AUX_VID_PIPE      |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_BE  |
            SYS_CFG7564_RST_N_FVDP_PIP_VID_PIPE      |
            SYS_CFG7564_RST_N_FVDP_INPUT_VID_MUX     |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_FE  |
            SYS_CFG7564_RST_N_FVDP_MAIN_VID_PIPE_EN
        );

        sysconf_write(context->p_sysconf[SYSCONF_RESET], val);

        stm_fvdp_printd(">>> power down MPE RST(31-0): %08x\n",
            (unsigned int)sysconf_read(context->p_sysconf[SYSCONF_RESET]) );
    }
}

irqreturn_t stih416_gpio104_int_handler(int irq, void* data)
{
    FVDP_Result_t error=0;

    /* FVDP treatment associated to the interrupt */
    error = FVDP_IntEndOfScaling();

    if(error != FVDP_OK)
    {
        stm_fvdp_printe("Failed, FVDP_IntEndOfScaling err=%d\n", error);
    }

  return IRQ_HANDLED;
}

irqreturn_t stih416_enc_ch_int_handler(int irq, void* data)
{
    FVDP_Result_t error;

    /* FVDP treatment associated to the interrupt */
    error = FVDP_IntEndOfScaling();
    if(error != FVDP_OK)
    {
        stm_fvdp_printe("Failed, FVDP_IntEndOfScaling err=%d\n", error);
    }

  return IRQ_HANDLED;
}

int fvdp_platform_device_register(void)
{
    int result;

    result = platform_add_devices(fvdp_devices, ARRAY_SIZE(fvdp_devices));

    return result;
}


void fvdp_platform_device_unregister(void)
{
    int i;
    for(i=0; i<ARRAY_SIZE(fvdp_devices); i++)
    {
        platform_device_unregister(fvdp_devices[i]);
    }
}

int fvdp_claim_sysconf(struct platform_device *pdev)
{
    int                        i;
    struct fvdp_driver_data   *context;
    struct fvdp_platform_data *platform_data = (struct fvdp_platform_data*)pdev->dev.platform_data;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    /* Claim sysconf registers */
    for(i=0; i<NB_SYSCONF;i++)
    {
        context->p_sysconf[i] = sysconf_claim(SYSCONF(platform_data->sysconf[i].sys_reg),
                                                    platform_data->sysconf[i].lsb,
                                                    platform_data->sysconf[i].msb,
                                                    platform_data->sysconf[i].name);
        if(context->p_sysconf[i] == 0)
        {
            stm_fvdp_printe("Failed, sysconf_claim returns error for register %d\n", platform_data->sysconf[i].sys_reg);
            return(-ENODEV);
        }
    }

    return(0);
}

void fvdp_release_sysconf(struct platform_device *pdev)
{
    int                        i;
    struct fvdp_driver_data   *context;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    /* Release sysconf registers */
    for(i=0; i<NB_SYSCONF;i++)
    {
        if(context->p_sysconf[i])
        {
            sysconf_release(context->p_sysconf[i]);
        }
    }
}

int get_io_resource(struct platform_device *pdev)
{
    struct fvdp_driver_data   *context;
    struct fvdp_platform_data *platform_data = (struct fvdp_platform_data*)pdev->dev.platform_data;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    /*if cut1.0 */
    if(((context->chip_version_major == 1) && (context->chip_version_minor == 0)))
    {
        /* MPE cut 1.0 */

        /* Configure GPIO resource for simulating end of FVDP activity on MPE cut 1.0 */
        /* MPE cut 1.1 or higher will use dedicated FVDP interrupt so no need of GPIO */

        /* Check that only one gpio config is defined */
        if(platform_data->gpio_config.gpios_num != 1)
        {
            stm_fvdp_printe("Failed, more than one gpio config in platform_data\n");
            return -EINVAL;
        }

        /* Initialize gpio sturcture */
        context->gpio.gpio_id = platform_data->gpio_config.gpios[0].gpio;
        context->gpio.irq_num = gpio_to_irq(context->gpio.gpio_id);
        context->gpio.gpio_pad = NULL;
        context->gpio.is_used = true;
    }

    return 0;
}

int fvdp_configure_pads(struct platform_device *pdev, bool enable)
{
    struct fvdp_driver_data   *context;
    struct fvdp_platform_data *platform_data = (struct fvdp_platform_data*)pdev->dev.platform_data;
    int result=0;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    if(enable)
    {
        if(((context->chip_version_major == 1) && (context->chip_version_minor == 0)))
        {
            /* MPE cut 1.0 */
            if(context->gpio.is_used)
            {
                /* Configure the Pad in Alternate function for detecting end of FVDP scaling by an interrupt */
                context->gpio.gpio_pad = stm_pad_claim(&(platform_data->gpio_config), dev_name(&pdev->dev));
                if(context->gpio.gpio_pad == NULL)
                {
                    stm_fvdp_printe("Failed to call stm_pad_claim\n");
                }

                /* Set gpio interrupt handler on low level trigerring (but request_irq() is also automatically enabling the interrupt). */
                /* As by default the gpio state is at low level so interrupt hnadler will be executed immediately. */
                if ((context->chip_version_major == 1) && (context->chip_version_minor == 0))
                {
                    result = request_irq(context->gpio.irq_num, stih416_gpio104_int_handler, IRQF_TRIGGER_LOW, fvdp_driver_name, NULL);
                }
                if (result)
                {
                    stm_fvdp_printe("Failed, can't subscribe to IRQ %i - result is:%d\n", context->gpio.irq_num, result);
                    return -ENODEV;
                }
                else
                {
                    stm_fvdp_printd("Succeed to subscribe to IRQ %i\n", context->gpio.irq_num);
                }
            }

        }
        else
        {
            /* MPE cut 1.1 or higher */

            /* Request IRQ, make sure that irq_num should be non-error value IRQF_DISABLED*/
            result = request_irq(FVDP_ENC_INT, stih416_enc_ch_int_handler,IRQF_DISABLED, fvdp_driver_name, NULL);
            if (result)
            {
                stm_fvdp_printe("Failed, can't subscribe to IRQ %i - result is:%d\n", FVDP_ENC_INT, result);
                return -ENODEV;
            }
            else
            {
                stm_fvdp_printd("Succeed to subscribe to IRQ %i\n", FVDP_ENC_INT);
            }
        }
    }
    else
    {
        if(((context->chip_version_major == 1) && (context->chip_version_minor == 0)))
        {
            /* MPE cut 1.0 */

            if(context->gpio.is_used)
            {
                /* Release the irq */
                free_irq(context->gpio.irq_num, NULL);

                /* Release Pad and disable interrupt for detecting end of FVDP scaling */
                stm_pad_release(context->gpio.gpio_pad);
                context->gpio.gpio_pad = 0;
            }
        }
        else
        {
            /* MPE cut 1.1 or higher */

            /* Release the irq */
            free_irq(FVDP_ENC_INT, NULL);
        }
    }

    return 0;
}

int fvdp_clocks_initialize(struct platform_device *pdev)
{
    int                        i;
    struct fvdp_driver_data   *context;
    struct fvdp_platform_data *platform_data = (struct fvdp_platform_data*)pdev->dev.platform_data;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    /* Retrieve clocks */
    for(i=0; i<NB_CLOCKS_FVDP;i++)
    {
        context->clock[i] = clk_get(&(pdev->dev), platform_data->clock[i].name);
        if(IS_ERR(context->clock[i]))
        {
            stm_fvdp_printe("Failed, %s: clock %s was not found\n", fvdp_driver_name, platform_data->clock[i].name);
            return(-ENODEV);
        }
    }

    /* Set clocks rate and/or parent clock */
    for(i=0; i<NB_CLOCKS_FVDP;i++)
    {
        if(platform_data->clock[i].parent != NO_PARENT_CLOCK)
        {
            /* Set parent clock if parent clock is defined */
            if(clk_set_parent(context->clock[i], context->clock[platform_data->clock[i].parent]))
            {
                stm_fvdp_printe("Failed, to set clock %s parent to %s\n", platform_data->clock[i].name, platform_data->clock[platform_data->clock[i].parent].name);
                return(-ENODEV);
            }
        }
        if(platform_data->clock[i].rate)
        {
            /* Set clock rate if not null */
            if(clk_set_rate(context->clock[i], platform_data->clock[i].rate))
            {
                stm_fvdp_printe("Failed, to set %s clock at rate %lu\n", platform_data->clock[i].name, platform_data->clock[i].rate);
                return(-ENODEV);
            }
        }
    }

    return(0);
}

void fvdp_clocks_uninitialize(struct platform_device *pdev)
{
    int                        i;
    struct fvdp_driver_data   *context;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    /* Release FVDP clock usage to linux kernel */
    for(i=0; i<NB_CLOCKS_FVDP;i++)
    {
        if(context->clock[i])
        {
            clk_put(context->clock[i]);
        }
    }
}

int fvdp_clocks_enable(struct platform_device *pdev, bool enable)
{
    int                        i;
    int                        result = 0;
    struct fvdp_driver_data   *context;
    struct fvdp_platform_data *platform_data = (struct fvdp_platform_data*)pdev->dev.platform_data;

    /* Get private driver data. */
    context = platform_get_drvdata(pdev);

    for(i=0; (i<NB_CLOCKS_FVDP) && (result == 0);i++)
    {
        if(context->clock[i] == NULL)
        {
            /* Clock not yet initialized! */
            continue;
        }

        if(enable)
        {
            /* Enable FVDP clock */
            if((result = clk_enable(context->clock[i])))
            {
                stm_fvdp_printe("Failed to enable %s clock\n", platform_data->clock[i].name);
            }
        }
        else
        {
            /* Disable FVDP clock */
            clk_disable(context->clock[i]);
        }
    }

    return result;
}


int __init fvdp_driver_probe(struct platform_device *pdev)
{
    int                      result = 0;
    struct device           *dev = &pdev->dev;
    struct fvdp_driver_data *context;
    struct resource         *resource;
    int                      i;
    FVDP_Init_t              init_params;
    uint32_t                 mpe_cut_maj;
    uint32_t                 mpe_cut_min;

    stm_fvdp_printd(">%s\n", __func__);
    pdev->dev.platform_data = (void *)(uintptr_t)&fvdp_data;

    context = kmalloc(sizeof(struct fvdp_driver_data), GFP_KERNEL);
    if(context == 0)
    {
        stm_fvdp_printe("Failed, to allocate memory for fvdp driver context\n");
        goto error;
    }
    memset(context, 0, sizeof(struct fvdp_driver_data));

    /* Save private driver data. */
    platform_set_drvdata(pdev, context);

    /* Check MPE cut version */
    if(!vibe_os_get_chip_version(&mpe_cut_maj, &mpe_cut_min))
    {
        stm_fvdp_printe("Failed, to get chip version\n");
        goto error;
    }
    else
    {
        context->chip_version_major =  mpe_cut_maj;
        context->chip_version_minor =  mpe_cut_min;

    }
    printk(KERN_INFO "MPE CUT %d.%d\n", mpe_cut_maj, mpe_cut_min);

    /* Retrieve IORESOURCE_MEM */
    for(i=0; i< NB_MEM_RES; i++)
    {
        resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, mem_res_name[i]);
        if(resource == 0)
        {
            stm_fvdp_printe("Failed, IORESOURCE_MEM %s missing\n", mem_res_name[i]);
            goto error;
        }
        context->p_resource[i] = (uint32_t*)ioremap(resource->start, (resource->end - resource->start +1));
        if(context->p_resource[i] == 0)
        {
            stm_fvdp_printe("Failed, ioremap returns invalid address for %s\n", mem_res_name[i]);
            goto error;
        }
        stm_fvdp_printd("Succeed, %s start=%08x end=%08x virt=%08x\n", mem_res_name[i], (uint32_t)resource->start, (uint32_t)resource->end, (uint32_t)context->p_resource[i]);
    }

    /* Claim sysconf registers */
    if(fvdp_claim_sysconf(pdev))
    {
        stm_fvdp_printe("Failed, fvdp_claim_sysconf\n");
        goto error;
    }

    /* Power Down FVDP chip */
    fvdp_hw_power_down(pdev);

    /* Power On FVDP chip */
    fvdp_hw_power_up(pdev);

    /* Get if and which gpio to used for detecting end of scaling */
    if(get_io_resource(pdev))
    {
        stm_fvdp_printe("Failed, to get_io_resource\n");
        goto error;
    }


    /* Initialize FDVP driver */
    init_params.pDeviceBaseAddress = context->p_resource[MEM_FVDP];
    if(FVDP_InitDriver(init_params)!= FVDP_OK)
    {
        stm_fvdp_printe("Failed, FVDP_InitDriver\n");
        goto error;
    }

    /* Initialize FVDP clocks */
    if(fvdp_clocks_initialize(pdev))
    {
        stm_fvdp_printe("Failed, fvdp_clocks_initialize\n");
        goto error;
    }

    /* Switch on FVDP clocks */
    if(fvdp_clocks_enable(pdev, true))
    {
        stm_fvdp_printe("Failed, to switch on FVDP clocks\n");
        goto error;
    }

    /* Configure gpio pads */
    if(fvdp_configure_pads(pdev, true))
    {
        stm_fvdp_printe("Failed, FVDP_InitDriver\n");
        goto error;
    }

    pm_runtime_set_active(dev);
    pm_runtime_enable(dev);
    pm_runtime_get(dev);


    stm_fvdp_printd("<%s succeed\n", __func__);

    return result;

error:
    stm_fvdp_printe("Failed %s \n", __func__);

    fvdp_driver_remove(pdev);

    return -ENODEV;
}

int __exit fvdp_driver_remove(struct platform_device *pdev)
{
    int                      i;
    struct fvdp_driver_data *context;

    stm_fvdp_printd(">%s\n", __func__);

    /* Get private driver data. */
    context = (struct fvdp_driver_data *) platform_get_drvdata(pdev);

    /* Unconfigure gpio pads */
    if(fvdp_configure_pads(pdev, false))
    {
        stm_fvdp_printe("Failed, FVDP_InitDriver\n");
    }

    /* Switch off FVDP clocks */
    if(fvdp_clocks_enable(pdev, false))
    {
        stm_fvdp_printe("Failed, to switch off FVDP clocks\n");
    }

    /* Uninitialize FVDP clocks */
    fvdp_clocks_uninitialize(pdev);

    /* Power Down FVDP chip */
    fvdp_hw_power_down(pdev);

    /* Release sysconf registers */
    fvdp_release_sysconf(pdev);

    /* Release IORESOURCE_MEM */
    for(i=0; i< NB_MEM_RES; i++)
    {
        if(context->p_resource[i])
        {
            iounmap(context->p_resource[i]);
            context->p_resource[i]=0;
        }
    }

    kfree(context);

    stm_fvdp_printd("<%s\n", __func__);

    return 0;
}
