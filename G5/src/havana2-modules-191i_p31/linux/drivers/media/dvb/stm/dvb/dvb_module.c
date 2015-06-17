/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_module.c
Author :           Julian

Implementation of the LinuxDVB interface to the DVB streamer

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-05   Created                                         Julian

************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/autoconf.h>
#include <asm/uaccess.h>

#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/version.h>

#include "dvb_demux.h"          /* provides kernel demux types */

#define USE_KERNEL_DEMUX

#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#include "dvb_dmux.h"
#include "dvb_dvr.h"
#include "dvb_ca.h"
#include "backend.h"

extern int __init avr_init(void);
extern int __init cap_init(void);
extern void linuxdvb_v4l2_init(void);

/*static*/ int  __init      StmLoadModule (void);
static void __exit      StmUnloadModule (void);

module_init             (StmLoadModule);
module_exit             (StmUnloadModule);

MODULE_DESCRIPTION      ("Linux DVB video and audio driver for STM streaming architecture.");
MODULE_AUTHOR           ("Julian Wilson");
MODULE_LICENSE          ("GPL");

#define MODULE_NAME     "STM Streamer"


struct DvbContext_s*     DvbContext;

long DvbGenericUnlockedIoctl(struct file *file, unsigned int foo, unsigned long bar)
{
    return dvb_generic_ioctl(NULL, file, foo, bar);
}

/*static*/ int __init StmLoadModule (void)
{
    int                         Result;
    int                         i;
    short int                   AdapterNumbers[] = { -1 };

    DvbContext  = kzalloc (sizeof (struct DvbContext_s),  GFP_KERNEL);
    if (DvbContext == NULL)
    {
        DVB_ERROR("Unable to allocate device memory\n");
        return -ENOMEM;
    }

#if defined (CONFIG_KERNELVERSION)
#if DVB_API_VERSION < 5
    Result      = dvb_register_adapter (&DvbContext->DvbAdapter, MODULE_NAME, THIS_MODULE, NULL);
#else   
    Result      = dvb_register_adapter (&DvbContext->DvbAdapter, MODULE_NAME, THIS_MODULE, NULL, AdapterNumbers);
#endif
#else /* STLinux 2.2 kernel */
    Result      = dvb_register_adapter (&DvbContext->DvbAdapter, MODULE_NAME, THIS_MODULE);
#endif
    if (Result < 0)
    {
        DVB_ERROR("Failed to register adapter (%d)\n", Result);
        kfree(DvbContext);
        DvbContext      = NULL;
        return -ENOMEM;
    }

    mutex_init (&(DvbContext->Lock));
    mutex_lock (&(DvbContext->Lock));
    /*{{{  Register devices*/
    for (i = 0; i < DVB_MAX_DEVICES_PER_ADAPTER; i++)
    {
        struct DeviceContext_s* DeviceContext   = &DvbContext->DeviceContext[i];
        struct dvb_demux*       DvbDemux        = &DeviceContext->DvbDemux;
        struct dmxdev*          DmxDevice       = &DeviceContext->DmxDevice;
        struct dvb_device*      DvrDevice;

        DeviceContext->DvbContext               = DvbContext;
    #if defined (USE_KERNEL_DEMUX)
        memset (DvbDemux, 0, sizeof (struct dvb_demux));
        DvbDemux->dmx.capabilities              = DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
        DvbDemux->priv                          = DeviceContext;
        DvbDemux->filternum                     = 32;
        DvbDemux->feednum                       = 32;
        DvbDemux->start_feed                    = StartFeed;
        DvbDemux->stop_feed                     = StopFeed;
        DvbDemux->write_to_decoder              = NULL;
        Result                                  = dvb_dmx_init (DvbDemux);
        if (Result < 0)
        {
            DVB_ERROR ("dvb_dmx_init failed (errno = %d)\n", Result);
            return Result;
        }

        memset (DmxDevice, 0, sizeof (struct dmxdev));
        DmxDevice->filternum                    = DvbDemux->filternum;
        DmxDevice->demux                        = &DvbDemux->dmx;
        DmxDevice->capabilities                 = 0;
        Result                                  = dvb_dmxdev_init (DmxDevice, &DvbContext->DvbAdapter);
        if (Result < 0)
        {
            DVB_ERROR("dvb_dmxdev_init failed (errno = %d)\n", Result);
            dvb_dmx_release (DvbDemux);
            return Result;
        }
        DvrDevice                               = DvrInit (DmxDevice->dvr_dvbdev->fops);
        /* Unregister the built-in dvr device and replace it with our own version */
        dvb_unregister_device  (DmxDevice->dvr_dvbdev);
        dvb_register_device (&DvbContext->DvbAdapter,
                             &DmxDevice->dvr_dvbdev,
                             DvrDevice,
                             DmxDevice,
                             DVB_DEVICE_DVR);


        DeviceContext->MemoryFrontend.source    = DMX_MEMORY_FE;
        Result                                  = DvbDemux->dmx.add_frontend (&DvbDemux->dmx, &DeviceContext->MemoryFrontend);
        if (Result < 0)
        {
            DVB_ERROR ("add_frontend failed (errno = %d)\n", Result);
            dvb_dmxdev_release (DmxDevice);
            dvb_dmx_release    (DvbDemux);
            return Result;
        }
    #else
        dvb_register_device (&DvbContext->DvbAdapter,
                             &DeviceContext->DemuxDevice,
                             DemuxInit (DeviceContext),
                             DeviceContext,
                             DVB_DEVICE_DEMUX);

        dvb_register_device (&DvbContext->DvbAdapter,
                             &DeviceContext->DvrDevice,
                             DvrInit (DeviceContext),
                             DeviceContext,
                             DVB_DEVICE_DVR);
        #endif

        dvb_register_device (&DvbContext->DvbAdapter,
                             &DeviceContext->AudioDevice,
                             AudioInit (DeviceContext),
                             DeviceContext,
                             DVB_DEVICE_AUDIO);

        dvb_register_device (&DvbContext->DvbAdapter,
                             &DeviceContext->CaDevice,
                             CaInit (DeviceContext),
                             DeviceContext,
                             DVB_DEVICE_CA);

        dvb_register_device (&DvbContext->DvbAdapter,
                             &DeviceContext->VideoDevice,
                             VideoInit (DeviceContext),
                             DeviceContext,
                             DVB_DEVICE_VIDEO);                             

        DeviceContext->Id                       = i;
        DeviceContext->DemuxContext             = DeviceContext;        /* wire directly to own demux by default */
        DeviceContext->SyncContext              = DeviceContext;        /* we are our own sync group by default */
        DeviceContext->Playback                 = NULL;
        DeviceContext->StreamType               = STREAM_TYPE_TRANSPORT;
        DeviceContext->DvbContext               = DvbContext;
        DeviceContext->DemuxStream              = NULL;
        DeviceContext->VideoStream              = NULL;
        DeviceContext->AudioStream              = NULL;
        DeviceContext->PlaySpeed                = DVB_SPEED_NORMAL_PLAY;
        DeviceContext->dvr_in                   = kmalloc(65536,GFP_KERNEL); // 128Kbytes is quite a lot per device.
        DeviceContext->dvr_out                  = kmalloc(65536,GFP_KERNEL); // However allocating on each write is expensive.
        DeviceContext->EncryptionOn             = 0;

    }

    mutex_unlock (&(DvbContext->Lock));

    DvbBackendInit ();

    /*}}}*/
#if defined (CONFIG_CPU_SUBTYPE_STX7200)
    avr_init();
#endif 

#if defined (CONFIG_CPU_SUBTYPE_STX7105) // || defined (CONFIG_CPU_SUBTYPE_STX7200)
    cap_init();
#endif  

    linuxdvb_v4l2_init();

    DVB_DEBUG("STM stream device loaded\n");

    return 0;
}

static void __exit StmUnloadModule (void)
{
    int i;

    DvbBackendDelete ();

    for (i = 0; i < DVB_MAX_DEVICES_PER_ADAPTER; i++)
    {
        struct DeviceContext_s* DeviceContext   = &DvbContext->DeviceContext[i];
        struct dvb_demux*       DvbDemux        = &DeviceContext->DvbDemux;
        struct dmxdev*          DmxDevice       = &DeviceContext->DmxDevice;

#if defined (USE_KERNEL_DEMUX)
        if (DmxDevice != NULL)
        {
            /* We don't need to unregister DmxDevice->dvr_dvbdev as this will be done by dvb_dmxdev_release */
            dvb_dmxdev_release (DmxDevice);
        }
        if (DvbDemux != NULL)
        {
            DvbDemux->dmx.remove_frontend (&DvbDemux->dmx, &DeviceContext->MemoryFrontend);
            dvb_dmx_release    (DvbDemux);
        }
#else
        dvb_unregister_device  (DeviceContext->DemuxDevice);
        dvb_unregister_device  (DeviceContext->DvrDevice);
#endif
        if (DeviceContext->AudioDevice != NULL)
            dvb_unregister_device  (DeviceContext->AudioDevice);
        if (DeviceContext->VideoDevice != NULL)
            dvb_unregister_device  (DeviceContext->VideoDevice);

        DvbPlaybackDelete (DeviceContext->Playback);
        DeviceContext->AudioStream              = NULL;
        DeviceContext->VideoStream              = NULL;
        DeviceContext->Playback                 = NULL;
        kfree(DeviceContext->dvr_in);
        kfree(DeviceContext->dvr_out);
    }


    if (DvbContext != NULL)
    {
        dvb_unregister_adapter (&DvbContext->DvbAdapter);
        kfree (DvbContext);
    }
    DvbContext  = NULL;

    DVB_DEBUG("STM stream device unloaded\n");

    return;
}

