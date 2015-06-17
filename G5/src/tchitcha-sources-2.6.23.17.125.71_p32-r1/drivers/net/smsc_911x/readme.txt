/***************************************************************************
 *
 * Copyright (c) 2004-2005, SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************
 * File: Readme.txt
 */

This is the users/programmers guide for the LAN9118 Linux Driver
The following sections can be found below
*  Revision History
*  Files
*  Features
*  Driver Parameters
*  Rx Performance Tuning
*  Tested Platforms
*  Rx Code Path
*  Tx Code Path
*  Platform Interface Descriptions

#######################################################
############### REVISION HISTORY ######################
#######################################################
06/04/2004 Bryan Whitehead. Version 0.50:
This is the first beta version

06/21/2004 Bryan Whitehead, Version 0.51:
added multicast
added private ioctls to set mac address and write eeprom (not tested)
removed timeout function, which served no purpose anyway.

07/09/2004 Bryan Whitehead, Version 0.52
Improved Flow Control
More Control on Link Management
   the driver parameter link_mode is now a bit wise field that
     specifies any combination of 4 or fewer link speeds
     1=10HD,2=10FD,4=100HD,8=100FD
     Still Autonegotiation is always used.

07/13/2004 Bryan Whitehead, Version 0.53
EEPROM has been fixed and tested.
   If there is an EEPROM connected and loaded correctly, its
     mac address will be loaded into the 118
Improved Flow Control again. Measured >10X improvement
    in UDP_STREAM test with small packets.

07/30/2004 Bryan Whitehead, Version 0.54
Added many IOCTL codes for use with the command app, cmd9118.
Added functions to the Platform interface
	Platform_RequestDmaChannel
	Platform_ReleaseDmaChannel
Added Macros to Platform interface
	PLATFORM_DEFAULT_RX_DMA
	PLATFORM_DEFAULT_TX_DMA

08/12/2004 Bryan Whitehead, Version 0.55
Updated driver documentation
Consolidated most driver source files into smsc9118.c
Minor changes to the platform interface

08/13/2004 Bryan Whitehead, Version 0.56
Added Ioctl COMMAND_GET_CONFIGURATION
  this command used with "cmd9118 -cGET_CONFIG"
  will display driver information, which is especially
  useful if trace points have been disabled.

08/24/2004 Bryan Whitehead, Version 0.57
Implemented software patch to resynchronize Rx fifos after an
overrun is detected.

08/24/2004 Bryan Whitehead, Version 0.58
Turned on LEDs which were accidentally turned off in version 0.57

08/25/2004 Bryan Whitehead, Version 0.59
Added extra driver context variables useful for debugging
They can be accessed with the following command
# cmd9118 -cGET_CONFIG

08/30/2004 Bryan Whitehead, Version 0.60
Fixed bug in software patch (0.57) that caused the receiver to be turned
off and not turned back on.
Added symple packet filter to detect bad packets. If a bad packet
is detected the driver will handle it like an overrun. That is, turn off
receiver, empty fifos, clear fifo pointers(RX_DUMP), and turn on receiver.
This method allows the driver to recover after a fifo synchronization fault.

09/01/2004 Bryan Whitehead, Version 0.61
This version was used for testing the patch. It was for internal purposes only

09/03/2004 Bryan Whitehead, Version 0.62
This version fixes some bugs in 0.60 and includes extra work around code
to improve patch reliability.

09/07/2004 Bryan Whitehead, Version 0.63
Added IOCTLs for reading and writing to general memory

09/13/2004 Bryan Whitehead, Version 0.64
As the hardware issue is now fully understood, this patch includes
a few changes to make the work around reliable in all situations.
This version also has a the beginnings of new flow control feature,
however this feature is disable for this version because it is
still in progress.
The build process has also changed. Now instead of building one
debug driver, you can build a release and debug driver by choosing
to include or not include the define USE_DEBUG. The release version
enables activity LEDs, and removes all trace, warning, and assertion
messages. The debug version enables GPIO signals, and enables trace,
warning, and assertion messages.

09/15/2004 Bryan Whitehead, Version 0.65
Removed packet filter from 0.64
Added non-Auto-Negotiate link modes. See description of the
  link_mode driver parameter below

09/28/2004 Bryan Whitehead, Version 0.66/0.67
Added Rx flow control which is only turned on when the driver
detects a lack of flow control in the higher layers.

10/07/2004 Bryan Whitehead, Version 0.68
Added support for Peakslite, and Polaris.
Implemented support for Phy management tools
  such as mii-diag and mii-tool.

10/13/2004 Bryan Whitehead, Version 0.69
Fixed some issues with Linux 2.6.

10/18/2004 Bryan Whitehead, Version 0.70
Added support for new chips 115, 116, 117.
	NOTE: 115, and 116 may still require bus timing
		adjustment.
Disabled workaround when rev b is detected.

10/20/2004 Bryan Whitehead, Version 0.71/0.72
Added support for tasklets, which allow the bulk of the
Rx processing to be done with out blocking interrupts for
the rest of the system.

10/27/2004 Bryan Whitehead, Version 1.00
Tuned the PLATFORM_INT_DEAS for sh3 platforms to get better
TCP performance. Bumped up version number to 1.00 because this
will be the first official release.

10/27/2004 Bryan Whitehead, Version 1.01
Changed sh3 POLARIS platform to use cycle steal DMA

11/11/2004 Bryan Whitehead, Version 1.02/1.03
Changed tasklets to default to off, (tasklets=0)
Testing has shown that tasklets are not stable yet. So changing the
default allows us to release this version. The user may still turn
tasklets on but does so at there own risk.

11/11/2004 Bryan Whitehead, Version 1.04
Fixed Tasklet Issue.
Changed tasklets to be ON by default, (tasklets=1)

11/18/2004 Bryan Whitehead, Version 1.05
Improved software flow control.
Updated platform code to handle all chips 118/117/116/115

11/19/2004 Bryan Whitehead, Version 1.06
Added support to provide separate flow control parameters
  for 118/117 and 116/115. And the driver will use which
  ever is appropriate.

11/23/2004 Bryan Whitehead, Version 1.07
Changed the driver so that PAUSE frames will be enable or disabled
based on state of the local and remote pause bits, as specified in
IEEE Standard 802.3, 2000 Edition, page 1360, Table 28B-3-Pause Resolution

12/01/2004 Bryan Whitehead, Version 1.08
Added support for external phy.
To enable use of external phy use the follow driver parameter setting
    ext_phy=1
other wise the driver will use the internal phy as it always has before.

12/01/2004 Bryan Whitehead, Version 1.09
replace the ext_phy parameter with phy_addr which works as follows
if(phy_addr==0xFFFFFFFF) {
	//use internal phy
} else if (phy_addr<32) {
	//use external phy at address specified by phy_addr
	//  if external phy can not be found use internal phy
} else {
	//attempt to auto detect external phy
	//  if external phy can not be found use internal phy
}

02/14/2005 Bryan Whitehead, Version 1.11
Automated the tuning procedure. See RX PERFORMANCE TUNING.
Changed all register access to use a Macro instead of
   a memory mapped structure. This should simplify porting,
   if register access is ever done through IO space.
Tuned all current platforms.
Increased time out of Rx_FastForward to 500.
Fixed faulty define TX_CMD_A_BUF_SIZE_
Fixed faulty define TX_CMD_B_PKT_BYTE_LENGTH_
Changed Phy reset to use PHY_BCR_RESET
Cleaned driver with Lint tool
Updated Rx_CountErrors
	Only count length error if length/type field is set
	If there is a CRC error don't count length error or
	  multicast
Changed access to Mac and Phy registers. Now requires only
	one lock, MacPhyLock. This replaces the MacAccessLock,
	PhyAccessLock, and RxSwitchLock.

02/28/2005 Bryan Whitehead, Version 1.12
Added date_code static constant

02/28/2005 Bryan Whitehead, Version 1.13
Added support for LAN9112

03/21/2005 Bryan Whitehead, Version 1.13
Updated Platforms ".h" file to include a version number in
   the PLATFORM_NAME macro. This version number is platform
   specific and independent of the common code version
   number.
Merged in STMICRO platform code to release tree

03/22/2005 Bryan Whitehead, Version 1.13
Updated platform code see st40.c for changes

03/23/2005 Bryan Whitehead, Version 1.13
Changed the format of the platform version to rN,
  where N is the number of the platform revision.
  This is visible in the PLATFORM_NAME macro.

04/11/2005 Bryan Whitehead, Version 1.14
Added Platform_CleanUp function, to be used in the event
	that a particular platform should release resources
	when the driver is unloaded from memory
Moved register access and data port access from common
	code to platform code. This is to support the
	possibility that a big endian system may need to do
	byte swapping.


Changed Lan_SetRegDW to use Platform_SetRegDW
Changed Lan_GetRegDW to use Platform_GetRegDW
Changed Lan_ClrBitsDW to use Platform_S/GetRegDW
Changed Lan_SetBitsDW to use Platform_S/GetRegDW
Changed Tx_WriteFifo to Platform_WriteFifo
Changed Rx_ReadFifo to Platform_ReadFifo
Changed the way platform files are included.
	Now the Macros PLATFORM_HEADER and PLATFORM_SOURCE
	will define a string that is the path to those
	header and source files.

04/15/2005 Bryan Whitehead, Version 1.15
Migrated source control to perforce.
Merged platform ".h" and ".c" file to a single
	platform ".c" file

05/26/2005 M David Gelbman, Version 1.16
Integrated Beacon PHY work-around into both Concord and
Beacon versions.
LED1 - 10/100 LED activity corrected to go OFF when link
state goes DOWN.

07/12/2005 Bryan Whitehead, Version 1.17
Fixed issue where driver parameter phy_addr was not used.

07/20/2005 Bryan Whitehead, Version 1.18
Preserve Platform Data from close to reopen.

07/26/2005 Phong Le
Version 1.19 Make delay time for EEPROM longer.
Fix bugs
Bug#1 external/internal problem. These are bugs introduced
when making LED work around and PHY work around.
Bug#2 Fix bug when bring down a driver, then bring up, driver crashed.

08/29/2005 - Phong Le
Version 1.20 Integrate fixes for false statistic error count report.

10/31/2005 - Phong Le
Version 1.21 Integrate fixes for Rx Multicast work around.

11/02/2005 - Phong Le
Still version 1.21 Fix bugs for version 1.21.

11/07/2005 - Phong Le
Still version 1.21 Fix bugs for version 1.21.

12/06/2005 Bryan Whitehead, Version 1.22
Added support for LAN9218, LAN9217, LAN9216, LAN9215

03/03/2006 Bryan Whitehead, Version 1.23
Fixed External Phy Support for LAN9217, LAN9215 (Bug 102)
Fixed Interrupt handler to only process enabled interrupts (Bug 96)
Fixed Multicast workaround to not rely on link status
   which may have been out dated (Bug 95)

03/22/2006 Bryan Whitehead, Version 1.24
Made driver future proof. Such that if the revision of a
chip increases it should still work.

03/23/2006 Bryan Whitehead, Version 1.25
Added print message to display date code on start up
Updated chip names to LAN9218, LAN9217, LAN9216, LAN9215

#######################################################
#################### FILES ############################
#######################################################
readme.txt: this file

smsc9118.c: Main driver source code file.
       This file contains all platform independent code
       It also includes platform dependent code based
       on the following defines
          USE_SH3
          USE_PEAKS
          USE_XSCALE
       One of these should be defined in the build batch file
        or makefile

peaks.h
xscale.h
sh3.h
      These files are platform dependent files. They define
      the platform dependent structure, PLATFORM_DATA.
      They also define some platform dependent constants for
      use by smsc9118.c. see PLATFORM INTERFACE DESCRIPTIONS
      below for more information

peaks.c
xscale.c
sh3.c
       These files implement the platform dependent functions
       used by smsc9118.c. see PLATFORM INTERFACE DESCRIPTIONS
       below for more information

ioctl_118.h
      defines ioctl codes common between the driver(smsc9118.c)
      and the helper application (cmd9118.c)

cmd9118.c
       This is the source code for the helper application.
       It provides the means by which one can send any ioctl code
       to the driver (smsc9118.c).

buildp: This batch file is used for building the driver for the
        peaks platform, and the helper application
        usage: bash buildp

builds: This batch file is used for building the driver for the
        sh3 platform, and the helper application
        usage: bash builds

buildx: This batch file is used for building the driver for the
        xscale platform, and the helper application
        usage: bash buildx

smsc9118.o: this is the driver binary obtained after building
        smsc9118.c

cmd9118: this is the helper application binary obtained after
        building cmd9118.c

lint.h:
	This is a header file for use only with the lint tool.
	It provides structure definitions and function declarations
	used by the driver so the lint tool does not need to
	scan the real header files. This is done because the real
	header files generate too many error messages.

cmd9118.Lnt:
smsc9118-*.Lnt:
	These are lint configuration files used with the lint tool

cmd9118-Lnt.txt
smsc9118-*-Lnt.txt:
	These are lint output files created by the lint tool.

COPYING:

	This is a copy of the GNU GENERAL PUBLIC LICENSE


############################################################
################### FEATURES ###############################
############################################################
transmit and receive for pio and dma
Automatic Flow control for Tx, and Rx
Multicast capable
Mac Address Management
External Phy support


############################################################
################## DRIVER PARAMETERS #######################
############################################################
The following are load time modifiable driver parameters.
They can be set when using insmod
Example:
# insmod smsc9118.o rx_dma=0 tx_dma=1 irq=5

lan_base
    specifies the physical base location in memory where the
    LAN9118 can be accessed. By default the location will be
    choosen by Platform_Initialize.

bus_width
    specifies the bus_width to be configured by
    Platform_Initialize. Must be set to either 16, or 32.
    Any other value or by default Platform_Initialize will
    attempt to autodetect the bus width


link_mode
    specifies the link mode used. Each bit has different meanings
      bit 0, 0x01, 10Mbps Half Duplex,
      bit 1, 0x02, 10Mbps Full Duplex,
      bit 2, 0x04, 100Mbps Half Duplex,
      bit 3, 0x08, 100Mbps Full Duplex,
      bit 4, 0x10, Symmetrical Pause,
      bit 5, 0x20, Asymmetrical Pause,
      bit 6, 0x40, Auto Negotiate,

    if bit 6 is set, then Auto Negotiation is used, and bits 0 through 5
      define what modes are advertised.

    if bit 6 is clear, then Auto Negotiation is not used, and bits 0
      through 3 select which speed and duplex setting to use. In this case,
      only the most significant set bit is used to set the speed and duplex.
      Example, the following are bits 6 to 0, and the resulting setting
      bits 6 5 4 3 2 1 0
           0 x x 0 0 0 x, 10Mbps Half Duplex
           0 x x 0 0 1 x, 10Mbps Full Duplex
           0 x x 0 1 x x, 100Mbps Half Duplex
           0 x x 1 x x x, 100Mbps Full Duplex

    by default link_mode=0x7F which uses Auto Negotiation and advertises
       all modes.

irq
    specifies the irq number to use. The default value is decided
    by the platform layer with the macro PLATFORM_IRQ

int_deas
    specifies the interrupt deassertion period to be written to
    INT_DEAS of INT_CFG register. The default value is decided by
    the platform layer with the macro PLATFORM_INT_DEAS

irq_pol
    specifies the value to be written to IRQ_POL of INT_CFG register.
    The default value is decided by the platform layer with the
    macro PLATFORM_INT_POL.

irq_type
    specifies the value to be written to IRQ_TYPE of INT_CFG register.
    The default value is decided by the platform layer with the
    macro PLATFORM_IRQ_TYPE

rx_dma
    specifies the dma channel to use for receiving packets. It may
    also be set to the following values
        256 = TRANSFER_PIO
            the driver will not use dma, it will use PIO.
        255 = TRANSFER_REQUEST_DMA
            the driver will call the Platform_RequestDmaChannel
            to get an available dma channel
    the default value is decided by the platform layer with the
    macro PLATFORM_RX_DMA

tx_dma
    specifies the dma channel to use for transmitting packets. It may
    also be set to the following values
        256 = TRANSFER_PIO
            the driver will not use dma, it will use PIO.
        255 = TRANSFER_REQUEST_DMA
            the driver will call the Platform_RequestDmaChannel
            to get an available dma channel
    the default value is decided by the platform layer with the
    macro PLATFORM_TX_DMA

dma_threshold
    specifies the minimum size a packet must be for using DMA.
    Otherwise the driver will use PIO. For small packets PIO may
    be faster than DMA because DMA requires a certain amount of set
    up time where as PIO can start almost immediately. Setting this
    value to 0 means dma will always be used where dma has been enabled.

    The default value is decided by the platform layer with the
    macro PLATFORM_DMA_THRESHOLD

mac_addr_hi16
    Specifies the high word of the mac address. If it was not
    specified then the driver will try to read the mac address
    from the eeprom. If that failes then the driver will set it
    to a valid value. Both mac_addr_hi16 and mac_addr_lo32 must be
    used together or not at all.

mac_addr_lo32
    Specifies the low dword of the mac address. If it was not
    specified then the driver will try to read the mac address
    from the eeprom. If that failes then the driver will set it to
    a valid value. Both mac_addr_hi16 and mac_addr_lo32 must be
    used together or not at all.

debug_mode
    specifies the debug mode
        0x01, bit 0 display trace messages
        0x02, bit 1 display warning messages
        0x04, bit 2 enable GPO signals

    Trace messages will only display if the driver was compiled
        with USE_TRACE defined.
    Warning message will only display if the driver was compiled
        with USE_WARNING defined.

tx_fifo_sz
    specifies the value to write to TX_FIFO_SZ of the
    HW_CFG register. It should be set to a value from 0 to 15
    that has been left shifted 16 bits.
    The default value is 0x00050000UL

afc_cfg
    specifies the value to write to write to the AFC_CFG register
    during initialization. By default the driver will choose a value
    that seems resonable for the tx_fifo_sz setting. However it
    should be noted that the driver has only be tested using the
    default setting for tx_fifo_sz

tasklets
	A non zero value specifies that most of the receive work
	should be done in a tasklet, thereby enabling other system interrupts.
	A zero value specifies that all receive work is done in the ISR,
	where interrupts are not enabled. A single burst of receive work has
	been seen to take as long as 3 mS. It would not be friendly for this
	driver to hold the CPU for that long, there for tasklets are enabled
	by default. However there are some tests that measure a mild performance
	drop when using tasklets, therefor this parameter is included so the
	end user can choose to disable tasklets.
	NOTE:10/29/2004: as of version 1.02 tasklets are disabled by default because
	testing has not confirmed their stability. The user may still turn on
	tasklets but does so at there own risk.
	NOTE:11/05/2004: as of version 1.04 tasklets are enabled by default because
	the issue observed before has been solved.

phy_addr
	The default value of 0xFFFFFFFF tells the driver to use the internal phy.
	A value less than or equal to 31 tells the driver to use the external phy
	    at that address. If the external phy is not found the internal phy
	    will be used.
	Any other value tells the driver to search for the external phy on all
	    addresses from 0 to 31. If the external phy is not found the internal
	    phy will be used.

max_throughput
    This is one of the software flow control parameters. It specifies the
    maximum throughput in a 100mS period that was measured during an rx
    TCP_STREAM test. It is used to help the driver decide when to activate
    software flow control and what work load to maintain when flow control
    is active. By default the driver will decide what value to use.
    See RX PERFORMANCE TUNING.

max_packet_count
	This is one of the software flow control parameters. It specifies the
	maximum packets counted in a 100mS period during an rx TCP_STREAM test.
	It is used to help the driver decide when to activate software flow
	control, and what work load to maintain when flow control is active.
	By default the driver will decide what value to use.
	See RX PERFORMANCE TUNING.

packet_cost
	This is one of the software flow control parameters. It specifies the
	ideal packet cost. This allows the driver achieve the best performance
	with both large and small packets, when flow control is active.
	By default the driver will decide what value to use.
	See RX PERFORMANCE TUNING.

burst_period
	This is one of the software flow control parameters. When flow control is active
	the driver will do work in bursts. This value specifies the length of
	the burst period in 100uS units. By default the driver will decide what value to
	use. See RX PERFORMANCE TUNING.

max_work_load
	This is one of the software flow control parameters. It specifies the amount
	of work the driver can do in a 100mS period. By default it is set as
	max_work_load = max_throughput + (max_packet_count*packet_cost);
	See RX PERFORMANCE TUNING.


###########################################################
################# RX PERFORMANCE TUNING ###################
###########################################################

Under most real world conditions traffic is flow controlled at
the upper layers of the protocol stack. The most common example is
TCP traffic. Even UDP traffic is usually flow controlled in the sense
that the transmitter does not normally send continuous wirespeed traffic.
When high level flow control is in use, it usually produces the
best performance on its own with no intervention from the driver.
But if high level flow control is not in use, then the driver will be
working as fast as it can to receive packets and pass them up to
the OS. In doing so it will hog CPU time and the OS may drop packets
due to lack of an ability to process them. It has been found that
during these heavy traffic conditions, throughput can be significantly
improved if the driver voluntarily releases the CPU. Yes, this will
cause more packets to be dropped on the wire but a greater number
of packets are spared because the OS has more time to process them.

As of version 1.05 and later, the driver implements a new flow control
detection method that operates like this. If the driver detects an
excessive work load in the period of 100mS, then the driver
assumes there is insufficient flow control in use. Therefor it will
turn on driver level flow control. Which significantly reduces the
amount of time the driver holds the CPU, and causes more packets to
be dropped on the wire. So a balancing/tuning, needs to be performed
on each system to get the best performance under these conditions.
If however the driver detects a tolerable work load in a period of
100mS then it assumes flow control is being managed well and does
not attempt to intervene by releasing the CPU. Under these conditions
the driver will naturally release the CPU to the OS, since the system
as a whole is keeping up with traffic.

Now that the background has been discussed, its necessary to talk
about how the driver implements flow control. The method used is a
work load model. This is similar but not exactly the same as
throughput. Work load is the sum of all the packet sizes plus a constant
packet cost for each packet. This provides more balance. For instance
a stream of max size packets will give a large throughput with fewer
packets, while a stream of min size packets will give a low throughput
with many packets. Adding a per packet cost to the work load allows these
two cases to both activate and manage flow control. If work load only
counted packet sizes then only a stream of max size packets would
activate flow control, while a stream of min size packets would not.

There are five primary parameters responsible for managing flow control.
They can be found in the FLOW_CONTROL_PARAMETERS structure.
They are
	MaxThroughput, Represents the maximum throughput measured in a
		100mS period, during an rx TCP_STREAM test.
	MaxPacketCount, Represents the maximum packet count measured in a
		100mS period, during an rx TCP_STREAM test.
	PacketCost, Represents the optimal per packet cost
	BurstPeriod, Represents the optimal burst period in 100uS units.
	IntDeas, This is the value for the interrupt deassertion period.
	    It is set to INT_DEAS field of the INT_CFG register.
The driver will call Platform_GetFlowControlParameters to get these

parameters for the specific platform and current configuration.
These parameters must be carefully choosen by the driver writer so
optimal performance can be achieved. Therefor it is necessary to
describe how the driver uses each parameter.

The first three parameters MaxThroughput, MaxPacketCount, and PacketCost,
are used to calculate the secondary parameter, MaxWorkLoad according
to the following formula.
	MaxWorkLoad=MaxThroughput+(MaxPacketCount*PacketCost);
MaxWorkLoad represents the maximum work load the system can handle during
a 100mS period.
	The driver will use MaxWorkLoad and BurstPeriod to determine
BurstPeriodMaxWorkLoad, which represents the maximum amount of work the
system can handle during a single burst period. It is calculated as follows
	BurstPeriodMaxWorkLoad=(MaxWorkLoad*BurstPeriod)/1000;

Every 100mS the driver measures the CurrentWorkLoad by the following
algorithm.

	At the beginning of a 100mS period
		CurrentWorkLoad=0;

	During a 100mS period CurrentWorkLoad is adjusted when a packet
	arrives according to the following formula.
		CurrentWorkLoad+=PacketSize+PacketCost;

	At the end of a 100mS period
		if(CurrentWorkLoad>((MaxWorkLoad*(100+ActivationMargin))/100))
		{
			if(!FlowControlActive) {
				FlowControlActive=TRUE;
				//Do other flow control initialization
				BurstPeriodCurrentWorkLoad=0;
			}
		}
		if(CurrentWorkLoad<((MaxWorkLoad*(100-DeactivationMargin))/100))
		{
			if(FlowControlActive) {
				FlowControlActive=FALSE;
				//Do other flow control clean up
				Enable Receiver interrupts
			}
		}

During periods where flow control is active, that is
FlowControlActive==TRUE, the driver will manage flow control by the
following algorithm.

	At the end/beginning of a burst period
		if(BurstPeriodCurrentWorkLoad>BurstPeriodMaxWorkLoad) {
			BurstPeriodCurrentWorkLoad-=BurstPeriodMaxWorkLoad;
		} else {
			BurstPeriodCurrentWorkLoad=0;
		}
		Enable Receiver Interrupts;

	When checking if a packet arrives
		if(BurstPeriodCurrentWorkLoad<BurstPeriodMaxWorkLoad) {
			//check for packet normally
			BurstPeriodCurrentWorkLoad+=PacketSize+PacketCost;
		} else {
			//Do not check for packet, but rather
			//  behave as though there is no new packet.
			Disable Receiver interrupts
		}

This algorithm will allow the driver to do a specified amount
of work and then give up the CPU until the next burst period. Doing this
allows the OS to process all the packets that have been sent to it.

So that is generally how the driver manages flow control. For more
detail you can refer to the source code. Now it is necessary to
describe the exact method for obtaining the optimal flow control
parameters.

When obtaining the optimal flow control parameters it is important
to note the configuration you are using. Generally there are 8
configurations for each platform. They involve the following options
	DMA or PIO
	16 bit or 32 bit
	118/117/112 or 116/115
Some platforms may only use 16 bit mode. While other platforms may
have a selectable clock rate. What ever the options are, every combination
should be identifiable, and Platform_GetFlowControlParameters should be
implemented to provide the correct flow control parameters. It is important
to be sure that the carefully selected parameters are applied to the
same configuration used during tuning.

Flow control tuning requires a publically available program called
netperf, and netserver, which are a client/server pair. These are built
with the same make file and can be found on the web.

Fortunately, as of version 1.10, smsc9118 and cmd9118 supports an automated
tuning mechanism. The process takes about one hour and can be initiated as
follows

AUTOMATED TUNING:

Choose the configuration you want to tune.
That is choose between
  DMA or PIO,
  16 bit or 32 bit,
  118/117/112 or 116/115,
Make sure there is a direct connection between the target platform
and the host platform. Do not use a hub, or switch. The target
platform is the platform that will run this driver. The host platform
should be a PC easily capable of sending wire speed traffic.

Install the driver on your target platform with your choosen configuration.
	insmod smsc9118d.o
	ifconfig eth1 192.1.1.118
load servers on target platform
	netserver
	cmd9118 -cSERVER
On host platform, make sure the netperf executable is
located in the same directory and the cmd9118 executable. While in that
directory run the following.
	cmd9118 -cTUNER -H192.1.1.118
This command, if successful, will begin the one hour tuning process.
At the end you will get a dump of the optimal flow control parameters.
The key parameters needed are
    MaxThroughput
    MaxPacketCount
    PacketCost
    BurstPeriod
    IntDeas
These value must be assigned in Platform_GetFlowControlParameters to
	flowControlParameters->MaxThroughput
	flowControlParameters->MaxPacketCount
	flowControlParameters->PacketCost
	flowControlParameters->BurstPeriod
	flowControlParameters->IntDeas
Make sure the Platform_GetFlowControlParameters checks the current configuration
and will only set those parameters if the current configuration matches the
configuration you tuned with.

Next start over but choose a configuration you haven't already tuned.


MANUAL TUNING:
In the off chance that the automated tuning fails to work properly, you may
use the following manual tuning procedure.

STEP 1:
	Select a configuration. That is choose between DMA or PIO, 16 bit or
	32 bit, 118/117/112 or 116/115.
	Make sure there is a direct connection between the target platform
	and the host platform. Do not use a hub, or switch. The target
	platform is the platform that will run this driver. The host platform
	should be a PC easily capable of sending wire speed traffic.

STEP 2:
	load the driver on the target platform with the following commands
		insmod smsc9118.o max_work_load=0 int_deas=ID
		ifconfig eth1 192.1.1.118
		netserver
	ID will be replated by the number you will be adjusting to obtain the
		best throughput score in STEP 3, initially a goog number to start
		with is 0.

STEP 3:
	On  the host platform run the following command
		netperf -H192.1.1.118
	Examine the output. The goal is to maximize the number on the
		Throughput column.
	If you are satisfied with the throughput remember the ID number
		you used and move on to STEP 4.
	If you would like to try improving the throughput
		unload the driver on the target with
			ifconfig eth1 down
			rmmod smsc9118
		goto STEP 2 and use a different value for ID


STEP 4:
	unload the driver with
		ifconfig eth1 down
		rmmod smsc9118
	load driver on the target platform with the following commands
		insmod smsc9118.o max_work_load=0 int_deas=ID
		ifconfig eth1 192.1.1.118
		netserver
	NOTE: the driver will be making traffic measurements. Therefor
		it is important not to insert any steps between 4 and 6.

STEP 5:
	run netperf on the host platform
		netperf -H192.1.1.118
	repeat two more times.

STEP 6:
	on target platform run the following
		cmd9118 -cGET_FLOW
	Many variables will be displayed. Two of them are measurements we need.
	You can set the following two parameters as follows.
		MaxThroughput  = RxFlowMeasuredMaxThroughput;
		MaxPacketCount = RxFlowMeasuredMaxPacketCount;

STEP 7:
	Unload the driver on target platform with
		ifconfig eth1 down
		rmmod smsc9118
	Apply the parameters obtained in STEP 6 and 2/3 to the appropriate location,
		given the configuration choosen in STEP 1, in
		Platform_GetFlowControlParameters. The parameters for your
		choosen configuration should be set as follows
			MaxThroughput = (RxFlowMeasuredMaxThroughput from step 6);
			MaxPacketCount = (RxFlowMeasuredMaxPacketCount from step 6);
			PacketCost=0; //temporarily set to 0
			BurstPeriod=100; //temporarily set to 100
			IntDeas= (ID from step 2/3).
	recompile driver.

STEP 8:
	Again make sure your still using the same configuration you selected
		in STEP 1.
	Load recompiled driver on target platform with the following commands
		insmod smsc9118.o burst_period=BP
		ifconfig eth1 192.1.1.118
	BP will be replaced by the number you will be adjusting to obtain the
		best throughput score in STEP 9, initially a good number to
		start with is 100.

STEP 9:
	On Host platform run the following command
		netperf -H192.1.1.118 -tUDP_STREAM -l10 -- -m1472
	Examine the output.	The goal is to maximize the lower
		number on the Throughput column.
	If you are satisfied with the throughput remember the BP number
		you used and move on to STEP 10.
	If you would like to try improving the throughput
		unload the driver on the target with
			ifconfig eth1 down
			rmmod smsc9118
		goto STEP 8 and use a different value for BP.

STEP 10:
	unload the driver on target platform
		ifconfig eth1 192.1.1.118 down
		rmmod smsc9118
	Again make sure your still using the same configuration you selected
		in STEP 1.
	Load the recompiled driver from STEP 7 on target platform with
		insmod smsc9118.o burst_period=BP packet_cost=PC
		ifconfig eth1 192.1.1.118
	BP will be replaced with the value you settled on in STEP 8/9.
	PC will be replaced by the number you will be adjusting to
		obtain the best throughput score in STEP 11, typically PC ends
		up somewhere between 100 and 200.

STEP 11:
	On Host platform run the following command
		netperf -H192.1.1.118 -tUDP_STREAM -l10 -- -m16
	Examine the output. The goal is to maximize the lower
		number on the Throughput column.
	If you are satisfied with the throughput remember the PC number
		you used and move on the STEP 12.
	If you would like to try improving the throughput
		unload the driver on the target with
			ifconfig eth1 down
			rmmod smsc9118
		goto STEP 10 and use a different value for PC

STEP 12:
	Apply the parameters to the appropriate location, given the
	configuration choosen in step 1, in Platform_GetFlowControlParameters.
		PacketCost= (PC from step 10/11);

		BurstPeriod= (BP from step 8/9);
	recompile driver.
	Now the values you applied will be the default values used when that
		same configuration is used again.
	Goto STEP 1 and choose a configuration you have not yet tuned.

###########################################################
################### TESTED PLATFORMS ######################
###########################################################
This driver has been tested on the following platforms.
The driver was loaded as a module with the following command
line.

	insmod smsc9118.o

===========================================================
Platform:
	SH3 SE01
Motherboard:
	Hitachi/Renesas, MS7727SE01/02
SMSC LAN9118 Board:
	LAN9118 FPGA DEV BOARD, ASSY 6337 REV A
LAN9118:
	LAN9118
Linux Kernel Version:
	2.4.18
Driver Resources:
	LAN_BASE=0xB4000000
	IRQ=8

===========================================================
Platform:
	XSCALE
Motherboard:
	Intel Corp, MAINSTONE II MAIN BOARD REV 2.1
SMSC LAN9118 Board:
	LAN9118 XSCALE FPGA DEV BOARD, ASSY 6343 REV A
LAN9118:
	LAN9118
Linux Kernel Version:
	2.4.21
Driver Resources:
	LAN_BASE=0xF2000000
	IRQ=198

###########################################################
##################### RX CODE PATH ########################
###########################################################
The purpose of this section is to describe how the driver
receives packets out of the LAN9118 and passes them to the
Linux OS. Most functions in the Rx code path start with Rx_

During initialization (Smsc9118_open) the function
Rx_Initialize is called. This call enables interrupts for
receiving packets.

When a packet is received the LAN9118 signals an interrupt,
which causes Smsc9118_ISR to be called. The Smsc9118_ISR function
is the main ISR which passes control to various handler routines.
One such handler routine that gets called is Rx_HandleInterrupt.

Rx_HandleInterrupt first checks to make sure the proper
interrupt has been signaled (INT_STS_RSFL_). If it has not
it returns immediately. If it has been signaled then it decides
if it should use PIO or DMA to read the data.

In both cases the process is like this
    An Rx status DWORD is read using Rx_PopRxStatus
    If there is an error
        the packet is purged from the LAN9118
            data fifo with Rx_FastForward
    If there is no error
        an sk_buff is allocated to receive the data
        The data is read into the sk_buff using PIO or DMA.
        After data is read the sk_buff is sent to linux using
            Rx_HandOffSkb
    The process continues until Rx_PopRxStatus returns 0

DMA is a little more complicated than PIO because it is written
to take advantage of concurrent processing. To get as much useful
work done as possible while a DMA operation is in progress.
Therefor DMA has the best performance boost when there are
several packets to service on a single call to Rx_HandleInterrupt

This is the purpose of the INT_DEAS field of INT_CFG. When that
field is set (using the driver parameter int_deas or platform macro
PLATFORM_INT_DEAS) it forces interrupts to be paced, so more packets
can arrive before a single call to the ISR.

The LAN9118 has been proven to receive at about 94Mbps on the
xscale platform. However other platforms run slower and require
some means of flow control. Rx flow control is built into the
LAN9118. It is configured by writing the AFC_CFG register. The
user can set this value at load time using the afc_cfg parameter.

Basically it works like this. If the Rx Data fifo fills up to
a level specified by AFC_HI in AFC_CFG then the LAN9118 will begin
flow control. If the link is half duplex then the LAN9118 will
exert back pressure by forcing a collision on the wire when a
packet is arriving. This will cause the sender to retransmit later.
If the link is full duplex then the LAN9118 will transmit a Pause
frame requesting the partner to stop sending packets. Then when
the Rx Data fifo drops to AFC_LO in AFC_CFG then the LAN9118 will
stop exerting back pressure in half duplex, or in full duplex it
will transmit another pause frame that lets the partner know it
can start sending packets again. See the LAN9118 specification
for more information. But all of this is done with out the help
of the driver.

Still the driver must participate in Rx flow control because of
platform speed limitations. Rx flow control in the driver is handled
in the functions Rx_HandOffSkb, and Rx_PopRxStatus. The function
Rx_HandOffSkb calls netif_rx which returns a congestion level.
If any congestion is detected then Rx_HandOffSkb will set the
RxCongested flag. The next time the driver calls Rx_PopRxStatus it
will see the RxCongested flag is set and will not Pop a new status
off the RX_STATUS_FIFO, but rather it will disable and re-enable
interrupts. This action will cause the interrupt deassertion interval
to begin. The ISR will return, and not be called again until the
deassertion interval has expired. This gives CPU time to linux so it
can handle the packets that have been sent to it and lower the
congestion level. In this case the driver voluntarily stops
servicing packets, which means the RX Data Fifo will fill up.
Eventually the hardware flow control will start up to slow down the
sender. Many times this will still result in an overflow of the
Rx Data fifo and packets will be lost under heavy traffic conditions.
But if the driver did not release the CPU to linux, then linux would

drop almost all the packets. So it is better to let the packets be
lost on the wire, rather than over consuming resources to service
them.


###########################################################
##################### TX CODE PATH ########################
###########################################################
When Linux wants to transmit a packet it will call
Smsc9118_hard_start_xmit. This function performs some checks
then calles Tx_SendSkb to send the packet.

Tx_SendSkb works with PIO or DMA depending on the selected
mode.

The basic sequence is this.
First Write TxCmdA and TxCmdB into the TX_DATA_FIFO.
Then Write the packet data into the TX_DATA_FIFO with
    adjustments for offset and end alignment.
Then free the skb using dev_kfree_skb

DMA is a little more complicated than PIO because it is written
to take advantage of concurrent processing. To get as much useful
work done as possible while a DMA operation is in progress.
Therefor DMA has the best performance boost when there are
a burst of large packets to transmit.

Tx Flow control works like this.
If the free space in the TX_DATA_FIFO after writing the data
is determined to be less than about the size of one full size
packet then the driver can't be sure it can transmit the next
packet without overflowing. Therefor the driver turns off the
Tx queue by calling Tx_StopQueue. Then it prepares an interrupt
to be signaled when the free space in the TX_DATA_FIFO has risen
to an acceptable level. When that level of free space has been
reached the interrupt is signaled and the handler will turn on
the Tx queue by calling Tx_WakeQueue

Statistic counters are updated after about every 30 packets or
when linux calls Smsc9118_get_stats

###########################################################
########## PLATFORM INTERFACE DESCRIPTIONS ################
###########################################################
The platform interface provides a platform independent
interface for the driver (smsc9118.c) This interface
simplifies the task of porting the driver to other platforms.

All functions that start with Platform_ are part of the
platform interface. All macros that start with PLATFORM_
are part of the platform interface.

Below are descriptions of PLATFORM_ macros.
for examples of usage see sh3.h, xscale.h or peaks.h

PLATFORM_CACHE_LINE_BYTES
      This macro is set to the size of a cache line in bytes.
      It is used with dma operations to insure cache line
          alignment which generally improves dma efficiency
      It is also used to insure that the IP header of received
          packets will be aligned on a cache line boundary

PLATFORM_IRQ_POL
      This macro is set to 0 or 1 and indicates the default
      value the driver will use when setting the IRQ_POL bit
      of the INT_CFG register. The user can override this
      value at load time by setting the driver parameter irq_pol.

PLATFORM_IRQ_TYPE
      This macro is set to 0 or 1 and indicates the default
      value the driver will use when setting the IRQ_TYPE bit
      of the INT_CFG register. The user can override this
      value at load time by setting the driver parameter irq_type.

PLATFORM_IRQ
      This macro indicates the default irq the driver will
      use when requesting an ISR.
      The user can override this value at load time by setting
      the driver parameter irq.

PLATFORM_RX_DMA
      This macro indicates the default dma channel to use for

      receiving packets. It may also be set to the following
      macros
         TRANSFER_PIO = 256
             the driver will not use dma. It will use PIO.
         TRANSFER_REQUEST_DMA = 255
             the driver will call the Platform_RequestDmaChannel
             to get an available dma channel
      The user can override this value at load time by setting
      the driver parameter rx_dma.

PLATFORM_TX_DMA
      This macro indicates the default dma channel to use for
      transmitting packets. It may also be set to the following
      macros
         TRANSFER_PIO = 256
             the driver will not use dma. It will use PIO.
         TRANSFER_REQUEST_DMA = 255
             the driver will call the Platform_RequestDmaChannel
             to get an available dma channel
      The user can override this value at load time by setting
      the driver parameter

PLATFORM_DMA_THRESHOLD
      This macro indicates the default value for dma_threshold.
      This value specifies the minimum size a packet must be
      for using DMA. Otherwise the driver will use PIO. For small
      packets PIO may be faster than DMA because DMA requires a
      certain amount of set up time where as PIO can start almost
      immediately. Setting this value to 0 means dma will always
      be used where dma has been enabled.

The platform specific header files (sh3.h, and xscale.h)
must also define the platform specific data structure
PLATFORM_DATA, and its pointer PPLATFORM_DATA.
this structure is passed to most Platform_ functions. It
allows the platform layer to maintain its own context information.

The following are descriptions of the Platform_ functions
for examples of usage see the files sh3.c, or xscale.c

/************************************************************
FUNCTION: Platform_Initialize
PARAMETERS:
	platformData, pointer to platform specified data structure
	dwLanBase,	user specified physical base address for the LAN9118
		= 0, let this function decide
	dwBusWidth,
		= 16, user specifies 16 bit mode operation, bypass autodetection
		= 32, user specifies 32 bit mode operation, bypass autodetection
		= any other value, driver should auto detect bus width
DESCRIPTION:
    This is the first Platform_xxx function that will be called
	This function will initialize the PLATFORM_DATA structure.
	This function will prepare the system to access the LAN9118.
	It will properly initialize the bus and return the virtual
	memory location where the LAN9118 can be access.
RETURN VALUE:
	0 = failed,
	any other value is the virtual base address where the LAN9118
	   can be accessed
************************************************************/
DWORD Platform_Initialize(
	PPLATFORM_DATA platformData,
	DWORD dwLanBase,
	DWORD dwBusWidth);

/***********************************************************
FUNCTION: Platform_CleanUp
PARAMETERS:
	platformData, pointer to platform specified data structure
DESCRIPTION:
	This function is called so the platform layer can clean up
	any resources that may have been acquired in Platform_Initialize.
	If Platform_Initialize returned 0, then this function will
	not be called.
***********************************************************/
void Platform_CleanUp(
	PPLATFORM_DATA platformData);

/************************************************************
FUNCTION: Platform_SetRegDW
PARAMETERS:
	DWORD dwLanBase, the virtual base address returned by
			Platform_Initialize
	DWORD dwOffset, the byte offset into the Lan address space
			where the register of interest is located.
			dwOffset should be DWORD aligned.
	DWORD dwVal, the value to write to the register of interest
DESCRIPTION:
	This function is used to provide a platform independent
		method of register access. It allows platform code
		for big endian systems to do byte swapping, if necessary.
	Because this function is very simple, it is recommended that
		it should be implemented as inline or as a macro.
RETURN VALUE: void
************************************************************/
inline void Platform_SetRegDW(
	DWORD dwLanBase,
	DWORD dwOffset,
	DWORD dwVal);

/************************************************************
FUNCTION: Platform_GetRegDW
PARAMETERS:
	DWORD dwLanBase, the virtual base address returned by
			Platform_Initialize
	DWORD dwOffset, the byte offset into the Lan address space
			where the register of interest is located.
			dwOffset should be DWORD aligned.
DESCRIPTION:
	This function is used to provide a platform independent
		method of register access. It allows platform code
		for big endian systems to do byte swapping if necessary.
	Because this function is very simple, it is recommended that
		it should be implemented as inline or as a macro.
RETURN VALUE:
	The DWORD value read from the register of interest;
************************************************************/
inline DWORD Platform_GetRegDW(
	DWORD dwLanBase,
	DWORD dwOffset);

/***********************************************************
FUNCTION: Platform_Is16BitMode
PARAMETER:
	platformData, pointer to platform specific data structure
RETURN VALUE:
	TRUE if the platform is configured for 16 bit mode
	FALSE if the platform is configured for 32 bit mode
NOTE: this function is not currently used.
***********************************************************/
BOOLEAN Platform_Is16BitMode(
	PPLATFORM_DATA platformData);

/************************************************************
FUNCTION: Platform_RequestIRQ
DESCRIPTION:
    Used to request and set up the ISR
PARAMETERS:
	platformData, pointer to platform specific data structure
	dwIrq,
	  = 0xFFFFFFFF, let this function decide what IRQ to use
	  = any other value is the IRQ requested by the user
	pIsr, pointer to the ISR, to be registered.
	dev_id, pointer to the driver specific structure
	   used when registering the ISR with Linux
RETURN VALUE:
    TRUE if successful
	FALSE is unsuccessful
************************************************************/
BOOLEAN Platform_RequestIRQ(

	PPLATFORM_DATA platformData,
	DWORD dwIrq,
	void (*pIsr)(int irq,void *dev_id,struct pt_regs *regs),
	void *dev_id);

/***********************************************************
FUNCTION: Platform_CurrentIRQ
PARAMETERS:
    platformData, pointer to platform specific data structure
RETURN VALUE:
	The IRQ number actually used by Platform_RequestIRQ
	  This may be different than the requested IRQ
***********************************************************/
DWORD Platform_CurrentIRQ(
	PPLATFORM_DATA platformData);

/**********************************************************
FUNCTION: Platform_FreeIRQ
PARAMETERS:
    platformData, pointer to platform specific data structure
DESCRIPTION:
    uninstalls the ISR installed from Platform_RequestIRQ
**********************************************************/
void Platform_FreeIRQ(
	PPLATFORM_DATA platformData);

/*********************************************************
FUNCTION: Platform_IsValidDmaChannel
PARAMETERS:
	dwDmaCh, the dma channel number to test for validity
DESCRIPTION:
    This function is used to test the validity of the
    channels request with parameters rx_dma, and tx_dma
RETURN VALUE:
	TRUE if the dma channel may be used
	FALSE if the dma channel may not be used
NOTE: this function does not use PLATFORM_DATA because
    it is called before the driver allocates memory for
    PLATFORM_DATA
*********************************************************/
BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh);

/********************************************************
FUNCTION: Platform_DmaInitialize
PARAMETERS:
    platformData, pointer to the platform specific data structure
	dwDmaCh, the Dma channel to initialize
RETURN VALUE:
	TRUE, on Success
	FALSE, on Failure
********************************************************/
BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh);

/********************************************************
FUNCTION: Platform_DmaDisable
PARAMETERS:
	platformData, pointer to the platform specific data structure
	dwDmaCh, the Dma channel to disable
RETURN VALUE:
	TRUE on success
	FALSE on failure
********************************************************/
BOOLEAN Platform_DmaDisable(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);

/*******************************************************
FUNCTION: Platform_CacheInvalidate
PARAMETERS:
	platformData, pointer to the platform specific data structure
	pStartAddress, the starting virtual address of the region
	dwLengthInBytes, the length in bytes of the region to invalidate
DESCRIPTION:
	Invalidates a specified memory region if it exists in cache.
	  Does not necessarily write the data back to memory.
*******************************************************/
void Platform_CacheInvalidate(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);

/*******************************************************
FUNCTION: Platform_CachePurge
PARAMETERS:
    platformData, pointer to the platform specific data structure
	pStartAddress, the starting virtual address of the region
	dwLengthInBytes, the length in bytes of the region to purge
DESCRIPTION:
    Writes back data to a specified memory region if it
      exists in cache.
*******************************************************/
void Platform_CachePurge(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);

/******************************************************
FUNCTION: Platform_RequestDmaChannel
PARAMETERS:
	platformData, pointer to the platform specific data structure
DESCRIPTION:
	If the OS supports Dma Channel allocation then this function
	  will use that support to reserve a dma channel.
	If the OS does not support Dma Channel allocation, or a
	  channel can not be reserved then this function
	  will return TRANSFER_PIO
RETURN VALUE:
	returns the DMA channel number that has been reserved
	if a channel can not be reserved then return TRANSFER_PIO
******************************************************/
DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData);

/*****************************************************
FUNCTION: Platform_ReleaseDmaChannel
PARAMETERS:
	platformData, pointer to the platform specific data structure
	dwDmaChannel, the Dma channel number to be released
DESCRIPTION:
	Releases the DMA channel specified by dwDmaChannel,
	  which was previously returned by Platform_RequestDmaChannel
    If the OS supports it this function will notify the OS
	  that the dma channel is free to be used by other devices
******************************************************/
void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel);

/*****************************************************
FUNCTION: Platform_DmaStartXfer
PARAMETERS:
    platformData, pointer to the platform specific data structure
    pDmaXfer, pointer to DMA_XFER structure
	    which describes the requested transfer
DESCRIPTION:
    Begins a new dma transfer,
	Should not be called if another transfer is in progress
RETURN VALUE:
    TRUE if dma has begun the transfer
	FALSE for any error
******************************************************/
BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer);

/*******************************************************
FUNCTION: Platform_DmaGetDwCnt
PARAMETERS:
    platformData, pointer to the platform specific data structure
	dwDmaCh, Dma channel number
RETURN VALUE:
    0, if the DMA channel is ready to handle another
	   request from Platform_DmaStartXfer
   non zero, is the number of DWORDS left for the
       dma channel to transfer
*******************************************************/
DWORD Platform_DmaGetDwCnt(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);

/******************************************************
FUNCTION: Platform_DmaComplete
PARAMETERS:
    platformData, pointer to platform specific data structure
	dwDmaCh, dma channel number
DESCRIPTION:
	Waits for the specified dma channel to finish
	transfering data. Upon return the dma channel should
	be ready to handle another request from
	Platform_DmaStartXfer
	This function should be the same as waiting for
	Platform_DmaGetDwCnt to return 0
******************************************************/
void Platform_DmaComplete(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);

/*****************************************************
FUNCTION: Platform_GetFlowControlParameters
PARAMETERS:
	platformData, pointer to platform specific data structure
	flowControlParameters, pointer to structure which receives
	     the flow control parameters.
    useDma, flag which specifies whether to get flow control
         parameters for PIO(FALSE) or DMA(TRUE).
DESCRIPTION:
	fills the structure pointed to by flowControlParameters,
	with the appropriate flow control parameters for the
	current combination of ((118/117/112) or (116/115)),
	(16 or 32 bit mode), and (PIO or DMA).
******************************************************/
void Platform_GetFlowControlParameters(
	PPLATFORM_DATA platformData,
	PFLOW_CONTROL_PARAMETERS flowControlParameters,
	BOOLEAN useDma);

/******************************************************
FUNCTION: Platform_WriteFifo
PARAMETERS:
	DWORD dwLanBase, the lan base address
	DWORD * pdwBuf, a pointer to and array of DWORDs to
		be written to the TX_DATA_FIFO
	DWORD dwDwordCount, the number of DWORDs in the dword
		array pointed to by pdwBuf
DESCRIPTION:
	This function is during PIO transfers to write packet
	data to the TX_DATA_FIFO. This function was made
	platform dependent to allow platforms to perform
	byte swapping on big endian systems, if necessary.
******************************************************/
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD * pdwBuf,
	DWORD dwDwordCount);

/******************************************************
FUNCTION: Platform_ReadFifo
PARAMETERS:
	DWORD dwLanBase, the lan base address
	DWORD * pdwBuf, a pointer to and array of DWORDs which
		will be read from the RX_DATA_FIFO
	DWORD dwDwordCount, the number of DWORDs in the dword
		array pointed to by pdwBuf
DESCRIPTION:
	This function is during PIO transfers to read packet
	data from the RX_DATA_FIFO. This function was made
	platform dependent to allow platforms to perform
	byte swapping on big endian systems, if necessary.
******************************************************/
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD * pdwBuf,
	DWORD dwDwordCount);
