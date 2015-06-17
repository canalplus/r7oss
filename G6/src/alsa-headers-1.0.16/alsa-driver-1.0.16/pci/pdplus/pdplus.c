/*
 *  Driver for Sek'D/Marian Prodif Plus soundcard
 *  Copyright (c) by Henrik Theiling <henrik@theiling.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * NOTES (ht):
 *     If you got this as part of the ALSA driver, some parts you might want to have
 *     for improvement of this driver are missing.  These missing parts are a) the
 *     file README.pdplus, b) programs that generate the following files
 *     automatically:
 *
 *           Package name      Output file               Input file
 *           ---------------------------------------------------------
 *           bit2uchar         card-pdplus-pga-a.c       pdplusa.rbt
 *           -"-               card-pdplus-pga-d.c       pdplusd.rbt
 *           assert.pl         card-pdplus-failure.h     card-pdplus.c
 *
 *     Do not hesitate to contact henrik@theiling.de to get these programs, they are
 *     free software.  They will soon appear in the WWW (presumable at www.theiling.de).
 *
 * NOTES about this driver (ht):
 *     Some things were not clear from the docu:
 *         - The card occupies a piece of IO space.  I suppose those are the PLX's control
 *           registers mapped into IO space (additional to memory).  The driver registers
 *           that piece of IO space in order to detect collisions.
 *           (see pdplus_t::PLX_io)
 *
 *         - During local reset it seems to be necessary to wait a bit.  Obviously, it is ok
 *           to have a 100 usec reset pulse and then wait 5ms as specified for waiting after
 *           FPGA initialisation.
 *           (see pdplus_local_reset_ll)
 *
 *         - Directly after reset, either the DCO or the frequency scanner still seems to be
 *           a bit sleepy.  I added another nap of 5ms to give it its rest.
 *           (see pdplus_init_circuit_ll)
 *
 *         - When programming CS4222 registers, it is necessary to have a valid clock on the
 *           ADC/DAC.  Otherwise, values are not clocked into its registers.  In the
 *           initialisation phase, I used to set NoClk to all devices.  This was not
 *           correct.  Now, the CS4222 is initially clocked with a clock of 44100 Hz.
 *           (see pdplus_init_circuit_ll)
 *
 *         - It's a pity that the 4222 is in SLI mode instead of in I2C.  No register can
 *           be read. (especially the status register which contains valuable
 *           information: overrange bits).
 *
 *         - It seems that clock switching is done automatically when using monitoring.
 *           I wanted to know what will happen when setting the 4222 to 29000 hz and
 *           the cs8404 to 44100 hz and then chosing digital monitoring on the analog
 *           port.  The result was that the ADC/DAC sounded perfect.  It therefore
 *           seems to ignore the analog clock setting when monitoring.  Maybe this is
 *           the reason for the strange constraints on monitor switching.
 *           However, I bet the cs8404 has to be programmed to do the right thing for
 *           the selected clock.  I'll do that.
 *
 * Other notes:
 *         - FFG is the abbreviation for Fixed Frequency Generator
 *
 * BUGS (These are TODOs, too!):
 *
 * TODO (prio in parens):
 *     (6) Make volume linear.
 *     (6) Introduce de-emphasis mixer control (multiple choice)
 *         This is hard: pre_effect is neither implemented in mid-level, nor in
 *         e.g. gamix.
 *     (6) Controls for soft-ramping (attenuation control).
 *         This is hard for the same reasons.
 *     (5) SYNC GO
 *     (5) ADAT (!)
 *     (5) Enable playback at more than some fixed rates on the digital outputs.
 *         The transmitter CS8404 can do up to 108000 kHz (no minimum specified).
 *     (4) compress init circuits (currently largest part of driver)
 *     (2) It should be possible to select the DigRec Clock as the clock device.
 *         Currently, only FFG and DCO can be used.  Probably this will be implemented
 *         when sync go for digital rec and analog play (and vice versa) is implemented.
 *     (1) Fix all FIXMEs and __attribute__((__unused__))s
 *
 * DONE:
 *     - check why __inline__ and annotation about reason if non-obvious.
 *     (8) add hw_silence
 *     (8) use own data for checking need for interrupt
 *     (8) use snd_lib_set_buffer_size
 *     - Port to frame size protocol
 *     - Change the control logic for monitoring.  Currently, bad clocks may
 *       be supplied to the CS4222 and the CS8404 when selecting monitoring of the
 *       foreign device.
 *
 *     - All sorts of malclocking problems have been fixed.  See *_clock_change_prepare*
 *       and *_check_clock for details.
 *
 *     - Controls for Professional mode (including audio/non-audio bit etc.)
 *     - Controls for Auto CD mode (consumer)
 *       **
 *       Both done via digital.dig_status[] info.
 *
 *     - Enable cross-over output routes to capture device even if no capturing is
 *       done.  This requires in-interrupt tests for over-clocking (because digital
 *       input clock is not under control of driver).
 *
 *     (8) ADC
 *     (8) DigCapt
 *
 *     - For some reason, a_play and a_capt are exlusive.  No erros are generated,
 *       but the requesting program sleeps until both sub-channels are free again.
 *       This does not happen for d_play and d_rect.
 *       Bug was fixed by understanding that _DUPLEX and _DUPLEX_RATE are not
 *       exclusive...
 *
 * CANCELLED:
 *     (6) 16 bits signed and 8 bits signed should be supported for playing because
 *         the copy routine has to be slow and proprietary anyway.
 *     Why cancelled?:  This blows up code and the mid-level code does it anyway, it does
 *         not belong here.
 *
 *
 * Description of the board:
 *     The Prodif Plus is a sound card with both analog and digital input/output
 *     plugs.  The digital interface consists of coaxial and optical connectors.
 *     The board is full duplex on all channels and supports monitoring of any
 *     of the input channels to any of the output channels.  Furthermore, it
 *     supports the ADAT protocol for up to eight channels.
 *
 *     Chips:
 *         Crystal (Cirrus Logic) CS 4222 ADC/DAC
 *             max. 20 bits, max 50 KHz
 *         CS 8404/8414 digital transceivers
 *             max. 24 bits, max 108/100 kHz
 *             consumer and professional formats are supported (S/PDIF & AES/EBU)
 *         FPGA circuit as the controlling logic
 *         PLX 9050 PCI bridge.
 *
 */
/*
 * Changes:
 *
 * Ver.0.0.5	Takashi Iwai <tiwai@suse.de>
 *	- Rewritten the old PCM ioctl callbacks with h/w constraints.
 *	- Rewritten the SPDIF status setting with the standard controls.
 *	- Fixed __init and __exit prefixes.
 *	- Replaced __inline__ with inline.
 *	- Fixed the struct initialization in C99 style.
 *	- Fixed compile warnings.
 */


#define PDPLUS_VERSION    "0.0.5"

#include "adriver.h"
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#define SNDRV_GET_ID
#include <sound/initval.h>
#include <sound/info.h>
#include <sound/asoundef.h>

/* ********************************************************************** */

#ifndef DEBUG
#  ifdef CONFIG_SND_DEBUG
#    define DEBUG 1
#  else
#    define DEBUG 0
#  endif
#endif

#ifndef REG_DEBUG
#define REG_DEBUG 0
/* This makes noise in /var/log/messages... */
#endif

#ifndef CHECK
#define CHECK 1
#endif

#ifndef MANY_CHECKS
#  ifdef CONFIG_SND_DEBUG_DETECT
#    define MANY_CHECKS 1
#  else
#    define MANY_CHECKS 0
#  endif
#endif

#ifndef VERBOSE
#  ifdef CONFIG_SND_DEBUG
#    define VERBOSE 1
#  else
#    define VERBOSE 0
#  endif
#endif

#ifndef HELP_OPTIMISE
#define HELP_OPTIMISE  1
#endif
        /* This introduces local variables in all routines using the virtual
         * hardware addresses.  This makes GCC optimise better.  However,
         * only the initialisation routines profit a bit, so I might
         * remove this optimisation in order to improve readability. */

#ifndef OVERRIDE_DEBUG_IO_VIRT
#define OVERRIDE_DEBUG_IO_VIRT 0
        /* See below */
#endif

#ifndef ALLOW_NONE_INPUT
#define ALLOW_NONE_INPUT 0
#endif

#ifndef PDPLUS_MEMCPY_INT
#define PDPLUS_MEMCPY_INT 1
/* Whether the _memcpy function uses size_t (PDPLUS_MEMCPY_INT==0) or
 * int (PDPLUS_MEMCPY_INT=1). */
#endif

#ifndef IEC958_ALLOW_HIGH_FREQ
#define IEC958_ALLOW_HIGH_FREQ 1
        /* Define whether settings e.g. 96kHz on S/PDIF is allowed or not.
         * Note that if this is 1, professional format will still be
         * automatically selected, however, if consumer format is forced,
         * this is no error with 96kHz if this is 1. */
#endif

#ifndef PDPLUS_DEFAULT_CLOCK
#define PDPLUS_DEFAULT_CLOCK 44100
/* Default clock to use as initial clock and as a fall-back. */
#endif

/*
 * Include the FPGA circuits.  One for digital, one for ADAT */
#include "pdplus-pga-a.c"
#include "pdplus-pga-d.c"

/* ********************************************************************** */

#if !DEBUG && REG_DEBUG
#warning "Compiling with DEBUG == 0 and REG_DEBUG != 0"
#endif

#if !DEBUG && MANY_CHECKS
#warning "Compiling with DEBUG == 0 and MANY_CHECKS != 0"
#endif

/* ********************************************************************** */

#define VENDOR_NAME  "SekD-Marian"
        /* Sek'd builds the card, but the vendor ID is Marian's.  So I decided
         * to use Marian as the vendor name.  Correct me if I'm wrong. */

#define DEVICE_NAME  "Prodif Plus"
#define MODULE_NAME  "pdplus"
#define FULL_NAME    VENDOR_NAME " " DEVICE_NAME

/* ********************************************************************** */

/*
 * Debug macros */
#if DEBUG
#define DL(L,format,args...)                             \
        do {                                             \
                if (debug_level >= (L)) {                \
                        snd_printk ("[debug]: " format, ##args); \
                        printk ("\n");                   \
                }                                        \
        } while (0)

#else
#define DL(L,format,args...) /* nothing */
#endif

#define ENTER       DL(5, "Enter %s", __FUNCTION__)
#define LEAVE_VOID  DL(5, "Leave %s", __FUNCTION__)

#define LEAVE(result)                                                             \
        do {                                                                          \
                DL(5,"Leave %s (return "#result"==%ld)", __FUNCTION__, (long)result); \
                return result;                                                        \
        } while (0)

#if VERBOSE
#define Vprintk(format, args...) printk(format, ##args)
#else
#define Vprintk(format, args...) /* nothing */
#endif

#define RESOURCE_LEN(X,Y) (pci_resource_end(X,Y) - pci_resource_start(X,Y) + 1UL)

#define MILLI_SECONDS(X)  ((HZ * (X)) / 1000)
#define MICRO_SECONDS(X)  ((HZ * (X)) / 1000000)

/* ********************************************************************** */

#ifndef BOOL
#define BOOL u_char
#endif

#ifndef FALSE
#define FALSE ((BOOL)0)
#endif

#ifndef TRUE
#define TRUE ((BOOL)(!FALSE))
#endif

#define forever   while (1)
#define until(X)  while (!(X))
#define unless(X) if (!(X))

#define PDPLUS_KERN_ERR   KERN_ERR  MODULE_NAME ": "
#define PDPLUS_KERN_INFO  KERN_INFO MODULE_NAME ": "

/* ********************************************************************** */

static int   index[SNDRV_CARDS]  = SNDRV_DEFAULT_IDX;     /* Index 0-MAX */
static char *id[SNDRV_CARDS]     = SNDRV_DEFAULT_STR;     /* ID for this card */
static int   enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE;  /* Enable switches */

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Henrik Theiling <henrik@theiling.de>");
MODULE_DESCRIPTION("SekD/Marian Prodif Plus");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{SMarian,Prodif Plus}}");

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Prodif Plus soundcard.");

module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for Prodif Plus soundcard.");

module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable Prodif Plus soundcard.");

static int silent_exit = 0;
module_param(silent_exit, bool, 0444);
MODULE_PARM_DESC(silent_exit, "Do not reset when driver is unloaded.");

static int init_adat = 0;
module_param(init_adat, bool, 0444);
MODULE_PARM_DESC(init_adat, "Initialise the card in ADAT mode (instead of in digital stereo).");

#if DEBUG
static int debug_level = 0;

module_param(debug_level, bool, 0444);
MODULE_PARM_DESC(debug_level, "Debug level.");
#endif
/* ********************************************************************** */

#if OVERRIDE_DEBUG_IO_VIRT
/*
 * To test the code quality, we need to override the 2.4.*pre*-kernel debugging
 * for __io_virt.
 */
#undef  __io_virt
#define __io_virt(x) ((void *)(x))
#endif

/*
 * General macros for IO */
#if REG_DEBUG

inline static u32 readl_trace (u_long addr)
{
        u32 result;
        result = readl (addr);
        DL(9, "readl(0x%lx)= 0x%x", addr, result);
        return result;
}

inline static void writel_trace (u32 val, u_long addr)
{
        DL(9, "writel(0x%x,0x%lx)", val,addr);
        writel (val, addr);
}

inline static u32 inl_trace (u_long addr)
{
        u32 result;

        result = inl (addr);
        DL(9, "inl(0x%lx)= 0x%x", addr, result);
        return result;
}

#define IOMEM_READ(ADDR)       readl_trace ((ADDR))
#define IOMEM_WRITE(ADDR,VAL)  writel_trace ((VAL),(ADDR))
#define IO_READ(ADDR)          inl_trace((ADDR))

#else

#define IOMEM_READ(ADDR)       (u32) readl ((ADDR))
#define IOMEM_WRITE(ADDR,VAL)  writel ((VAL),(ADDR))
#define IO_READ(ADDR)          inl((ADDR))

#endif

/*
 * Concept of macros
 * -----------------
 * There are a lot of macros that will follow now.  These will simplify access to
 * the cards registers.  After the basic macros are set up, one can define access
 * to registers in the following way:
 *    a) The method of access to a register family can be set by defining the
 *       following macros:
 *            PDPLUS_ ## FAMILY ## _RAW_WRITE
 *            PDPLUS_ ## FAMILY ## _RAW_READ
 *
 *    b) Register numbers can be defined in the following way:
 *            PDPLUS_ ## FAMILY ## _REG_R_ ## NAME_OF_REGISTER
 *            PDPLUS_ ## FAMILY ## _REG_W_ ## NAME_OF_REGISTER
 *       Read/Write register need both defines.
 *       Example:
 *            PDPLUS_FPGA_REG_R_STATUS
 *
 *    c) Register parts (bit sequences) can be defined in the following ways:
 *            PDPLUS_ ## FAMILY ## _REG_ ## NAME_OF_REGISTER ## _ ## NAME_OF_PART
 *
 *       The value of this is a pair or a triple.  The pair is organised as follows:
 *            (FIRST_BIT, BIT_COUNT)
 *
 *       A triple is organised as follows:
 *            (FIRST_BIT, BIT_COUNT, XOR_VALUE)
 *
 *       The XOR_VALUE is xor'ed onto user values upon reading and writing.  Its main
 *       purpose is to define inverted bits.
 *
 * Macros for register access for the devices on the card. */

/*
 * How to access registers? */
#define PDPLUS_PLX_RAW_READ(SCARD,REG)         IOMEM_READ (PDPLUS_VADDR_READ (SCARD, PLX, REG))
#define PDPLUS_FPGA_RAW_READ(SCARD,REG)        IOMEM_READ (PDPLUS_VADDR_READ (SCARD, FPGA, REG))
#define PDPLUS_HW_RAW_READ(SCARD,REG)          IOMEM_READ (PDPLUS_VADDR_READ (SCARD, HW, REG))

/*
 * Most registers can be written directly.  CS4222 however, requires access via FPGA ADDA
 * register.  This is handled in pdplus_adda_write. */
#define PDPLUS_PLX_RAW_WRITE(SCARD,REG,VALUE)  IOMEM_WRITE (PDPLUS_VADDR_WRITE (SCARD, PLX, REG), VALUE)
#define PDPLUS_FPGA_RAW_WRITE(SCARD,REG,VALUE) IOMEM_WRITE (PDPLUS_VADDR_WRITE (SCARD, FPGA, REG), VALUE)
#define PDPLUS_HW_RAW_WRITE(SCARD,REG,VALUE)   IOMEM_WRITE (PDPLUS_VADDR_WRITE (SCARD, HW, REG), VALUE)
#define PDPLUS_CS4222_RAW_WRITE(SCARD,REG,VALUE) pdplus_adda_write_ll ((SCARD), PDPLUS_REG_W_NAME (CS4222, REG), (VALUE))

/*
 * The name of the register definition */
#define PDPLUS_REG_R_NAME(FAMILY,REG) PDPLUS_ ## FAMILY ## _REG_R_ ## REG
#define PDPLUS_REG_W_NAME(FAMILY,REG) PDPLUS_ ## FAMILY ## _REG_W_ ## REG

/*
 * Write a value into the cache */
#define PDPLUS_WRITE_CACHE(SCARD,FAMILY,REG,VAL) \
                (SCARD)->FAMILY ## _ ## REG ## _val = (VAL)

#define PDPLUS_READ_CACHE(SCARD,FAMILY,REG) \
        ((u32)((SCARD)->FAMILY ## _ ## REG ## _val))

#if HELP_OPTIMISE
#define PDPLUS_LOCAL_VADDR_FOR(SCARD,FAMILY)                                \
        vm_offset_t const FAMILY##_iomem_vaddr __attribute__((__unused__))= \
                ((SCARD)->FAMILY ## _iomem.vaddr)

#define PDPLUS_LOCAL_VADDR(SCARD)        \
        PDPLUS_LOCAL_VADDR_FOR(SCARD, PLX);  \
        PDPLUS_LOCAL_VADDR_FOR(SCARD, FPGA); \
        PDPLUS_LOCAL_VADDR_FOR(SCARD, HW);   \
        PDPLUS_LOCAL_VADDR_FOR(SCARD, MEM)

#define PDPLUS_VADDR_READ(SCARD,FAMILY,REG) \
        (FAMILY ## _iomem_vaddr + PDPLUS_REG_R_NAME (FAMILY, REG))

#define PDPLUS_VADDR_WRITE(SCARD,FAMILY,REG) \
        (FAMILY ## _iomem_vaddr + PDPLUS_REG_W_NAME (FAMILY, REG))

#else
#define PDPLUS_LOCAL_VADDR(SCARD)

#define PDPLUS_VADDR_READ(SCARD,FAMILY,REG) \
        ((SCARD)->FAMILY ## _iomem.vaddr+PDPLUS_REG_R_NAME (FAMILY, REG))

#define PDPLUS_VADDR_WRITE(SCARD,FAMILY,REG) \
        ((SCARD)->FAMILY ## _iomem.vaddr+PDPLUS_REG_W_NAME (FAMILY, REG))

#endif


#define PDPLUS_PART(FAMILY,REG,PART) \
        PDPLUS_ ## FAMILY ## _REG_ ## REG ##_## PART

#define PDPLUS_PART_CONST(FAMILY,REG,NAME) \
        (PDPLUS_ ## FAMILY ## _REG_ ## REG ##_## NAME)

#define PDPLUS_PART_CONSTVAL(FAMILY,REG,PART,NAME) \
        (PDPLUS_ ## FAMILY ## _REG_ ## REG ##_## PART ##_## NAME)

/*
 * *** Access to readable registers ***
 *
 * Read a value from a register */
#define PDPLUS_READ_HW(SCARD,FAMILY,REG) \
        PDPLUS_ ## FAMILY ## _RAW_READ (SCARD,REG)

/*
 * Read the initial value of a register into the cache.  This is for read/write
 * register, e.g. those of the PLX. */
#define PDPLUS_READ_INIT(SCARD,FAMILY,REG) \
        PDPLUS_WRITE_CACHE (SCARD, FAMILY, REG, PDPLUS_READ_HW (SCARD, FAMILY, REG))

/*
 * Read a value from a hardware register and extract a given part. */
#define PDPLUS_EXTRACT_HW(SCARD,FAMILY,REG,PART) \
        PDPLUS_CUT_BITS (PDPLUS_PART (FAMILY, REG, PART), PDPLUS_READ_HW (SCARD, FAMILY, REG))

/*
 * Read a value from a hardware register and extract a given bit given as a bitmask. */
#define PDPLUS_EXTRACT_HW_BIT(SCARD,FAMILY,REG,MASK) \
        ((PDPLUS_READ_HW (SCARD, FAMILY, REG) & (MASK)) ? 1 : 0)

/*
 * Read, update cache and extract value */
#define PDPLUS_EXTRACT_INIT(SCARD,FAMILY,REG,PART)                \
        ({                                                        \
                PDPLUS_READ_INIT (SCARD, FAMILY, REG);            \
                PDPLUS_CUT_BITS (PDPLUS_PART (FAMILY, REG, PART), \
                        PDPLUS_READ_CACHE (SCARD, FAMILY, REG));  \
        })


/*
 * *** Access to cached writable registers. ***
 *
 * Write a value into a register without caching its value. */
#define PDPLUS_WRITE_HW(SCARD,FAMILY,REG,VAL) \
        PDPLUS_ ## FAMILY ## _RAW_WRITE (SCARD, REG, (VAL))

/*
 * Write the cached value of a register into the hardware register. */
#define PDPLUS_REWRITE_HW(SCARD,FAMILY,REG)             \
        PDPLUS_WRITE_HW (                               \
                SCARD,                                  \
                FAMILY,                                 \
                REG,                                    \
                PDPLUS_READ_CACHE (SCARD, FAMILY, REG))

/*
 * Write a value into the cache and the hardware (a value for
 * which there is a #define) */
#define PDPLUS_WRITE(SCARD,FAMILY,REG,VAL)                      \
        do {                                                    \
                int qqval = (VAL);                               \
                PDPLUS_WRITE_CACHE (SCARD, FAMILY, REG, qqval); \
                PDPLUS_WRITE_HW    (SCARD, FAMILY, REG, qqval); \
        } while (0)

/*
 * Write a value into the cache and the hardware (arbitrary value) */
#define PDPLUS_WRITE_CONST_AUX(SCARD,FAMILY,REG,CONSTNAME)                                       \
        do {                                                                                     \
                PDPLUS_WRITE_CACHE (SCARD,FAMILY,REG, PDPLUS_PART_CONST (FAMILY,REG,CONSTNAME)); \
                PDPLUS_WRITE_HW    (SCARD,FAMILY,REG, PDPLUS_PART_CONST (FAMILY,REG,CONSTNAME)); \
        } while (0)

#define PDPLUS_WRITE_CONST(SCARD,FAMILY,REG,CONSTNAME) \
        PDPLUS_WRITE_CONST_AUX(SCARD,FAMILY,REG,CONSTNAME)

/*
 * Replace parts of a register value by a new value and write the new register
 * contents into the hardware register. */
#define PDPLUS_REPLACE(SCARD,FAMILY,REG,PART,VAL)                 \
        PDPLUS_WRITE (                                            \
                SCARD,                                            \
                FAMILY,                                           \
                REG,                                              \
                PDPLUS_SET_BITS (PDPLUS_PART (FAMILY, REG, PART), \
                        PDPLUS_READ_CACHE (SCARD, FAMILY, REG),   \
                        (VAL)))

/*
 * Replace a given bit defined by a bit mask and write the new register
 * contents into the hardware register. */
#define PDPLUS_REPLACE_BIT(SCARD,FAMILY,REG,MASK,VAL)                         \
        PDPLUS_WRITE (                                                        \
                SCARD,                                                        \
                FAMILY,                                                       \
                REG,                                                          \
                (VAL) ?   (PDPLUS_READ_CACHE (SCARD, FAMILY, REG) | (MASK))   \
                        : (PDPLUS_READ_CACHE (SCARD, FAMILY, REG) & ~(MASK)))

/*
 * Replace a part of a register by a named constant */
#define PDPLUS_REPLACE_CONST_AUX(SCARD,FAMILY,REG,PART,CONSTNAME)             \
        PDPLUS_WRITE (                                                        \
                SCARD,                                                        \
                FAMILY,                                                       \
                REG,                                                          \
                PDPLUS_SET_BITS (PDPLUS_PART (FAMILY, REG, PART),             \
                        PDPLUS_READ_CACHE (SCARD, FAMILY, REG),               \
                        PDPLUS_PART_CONSTVAL (FAMILY, REG, PART, CONSTNAME)))

#define PDPLUS_REPLACE_CONST(SCARD,FAMILY,REG,PART,CONSTNAME) \
        PDPLUS_REPLACE_CONST_AUX(SCARD,FAMILY,REG,PART,CONSTNAME)


/*
 * Only perform the things on the cache, not on the hardware register. */
#define PDPLUS_REPLACE_CACHE(SCARD,FAMILY,REG,PART,VAL)           \
        PDPLUS_WRITE_CACHE (                                      \
                SCARD,                                            \
                FAMILY,                                           \
                REG,                                              \
                PDPLUS_SET_BITS (PDPLUS_PART (FAMILY, REG, PART), \
                        PDPLUS_READ_CACHE (SCARD, FAMILY, REG),   \
                        (VAL)))

/*
 * Only perform the things on the cache, not on the hardware register. */
#define PDPLUS_REPLACE_CONST_CACHE_AUX(SCARD,FAMILY,REG,PART,CONSTNAME)       \
        PDPLUS_WRITE_CACHE (                                                  \
                SCARD,                                                        \
                FAMILY,                                                       \
                REG,                                                          \
                PDPLUS_SET_BITS (PDPLUS_PART (FAMILY, REG, PART),             \
                        PDPLUS_READ_CACHE (SCARD, FAMILY, REG),               \
                        PDPLUS_PART_CONSTVAL (FAMILY, REG, PART, CONSTNAME)))

#define PDPLUS_REPLACE_CONST_CACHE(SCARD,FAMILY,REG,PART,CONSTNAME) \
        PDPLUS_REPLACE_CONST_CACHE_AUX(SCARD,FAMILY,REG,PART,CONSTNAME)

/*
 * Check a bit in the cached value.  This is like _EXTRACT but for the cached value
 * of writable registers (most of which are not readable). */
#define PDPLUS_EXTRACT_CACHE(SCARD,FAMILY,REG,PART)     \
        PDPLUS_CUT_BITS (                               \
                PDPLUS_PART (FAMILY, REG, PART),        \
                PDPLUS_READ_CACHE (SCARD, FAMILY, REG))

#define PDPLUS_EXTRACT_CACHE_BIT(SCARD,FAMILY,REG,MASK)             \
        ((PDPLUS_READ_CACHE (SCARD, FAMILY, REG) & (MASK)) ? 1 : 0)


/*
 * To check that a given constant is in the cache. */
#define PDPLUS_CACHE_EQL(SCARD,FAMILY,REG,PART,CONSTNAME)           \
        (PDPLUS_EXTRACT_CACHE(SCARD,FAMILY,REG,PART) ==             \
                PDPLUS_PART_CONSTVAL(FAMILY,REG,PART,CONSTNAME))

/*
 * Register values (can be or'ed together to give a complete register value). */
#define PDPLUS_VALUE(FAMILY,REG,PART,VALUE)      \
        PDPLUS_SET_BITS (                        \
                PDPLUS_PART (FAMILY, REG, PART), \
                0,                               \
                VALUE)

#define PDPLUS_CONST(FAMILY,REG,PART,CONSTNAME)                      \
        PDPLUS_VALUE(                                                \
                FAMILY,                                              \
                REG,                                                 \
                PART,                                                \
                PDPLUS_PART_CONSTVAL (FAMILY, REG, PART, CONSTNAME))

/* ********************************************************************** */
/* Some other basic macros */

/*
 * Bit operations.
 *
 * Each register part is defined as two or three values.  Two values are (bitnum,bitwidth),
 * e.g. (4,3) for the bit mask 0x70.
 * If the user values are inverted, the definitions are triples: (bitnum, bitwidth, xorvalue).
 */
#define BIT_VAL(BITCNT)       (1U << (BITCNT))
#define PD_BIT_MASK(BITCNT)      (BIT_VAL (BITCNT) - 1)

#define GET_SHIFT_AUX(X,Y,Z...) X
#define GET_BIT_AUX(X,Y,Z...)   BIT_VAL(X)
#define GET_MASK_AUX(X,Y,Z...)  PD_BIT_MASK(Y)

#define GET_SHIFT(X)          (GET_SHIFT_AUX X)
#define GET_MASK(X)           (GET_MASK_AUX X)
#define GET_BIT(X)            (GET_BIT_AUX X)

#define ZERO_BITS(N,V)        ((V) & ~((GET_MASK_AUX N) << (GET_SHIFT_AUX N)))
#define SHIFT_VAL(N,X)        (((X) & (GET_MASK_AUX N)) << (GET_SHIFT_AUX N))
#define SHIFT_CONST(N,X)      SHIFT_VAL(N, N ## _ ## X)
#define OR_BITS(N,V,X)        ((V) | SHIFT_VAL (N, X))

/*
 * The XOR macros are a bit ugly.  If no xorvalue is given, Z will be empty.  If we
 * concatenate the empty argument with anything, the preprocessor will generate the
 * empty string.  E.g.
 *
 *      XOR_PERHAPS ((2,1),    5)  returns (5)
 * but  XOR_PERHAPS ((2,1,1),  5)  returns (5 ^ 1)
 *
 */
#define XOR_
#define XOR_0
#define XOR_1 ^ 1
#define XOR_PERHAPS_AUX(X,Y,Z...) XOR_ ## Z

#define XOR_PERHAPS(N,X) ((X) XOR_PERHAPS_AUX N)

/*
 * The following are for access to registers and therefore automatically adjust the value
 * if needed. */
#define PDPLUS_CUT_BITS(N,V)  \
        XOR_PERHAPS (N, (((V) >> (GET_SHIFT_AUX N)) & (GET_MASK_AUX N)))

#define PDPLUS_SET_BITS(N,V,X) \
        OR_BITS (N, ZERO_BITS (N, V), XOR_PERHAPS (N, X))

/*
 * NOTE:
 * The following value does not take into account the xor value! */
#define PDPLUS_GET_MASK(N) SHIFT_VAL(N,0xffffffffU)

#define PDPLUS_UPROUND(X,P)   ((X) < (P) ? (P) : (((X) + (P)) - (((X)-1) % (P)) - 1))

/* ********************************************************************** */

/*
 * The following register definitions are done in such a way that the macros
 * are suitable for use with the PDPLUS_ macros.  Therefore, parts of registers
 * are given as pairs.  The first entry is the start bit, the second the width.
 * It might be a bit strange to specify single bit values in such a way, but is
 * is conveniently consistent then.
 */

/*
 * PLX Register and bit definitions */
#define PDPLUS_PLX_REG_R_LAS0RR 0x00
#define PDPLUS_PLX_REG_W_LAS0RR 0x00

#define PDPLUS_PLX_REG_R_LAS0BA 0x14
#define PDPLUS_PLX_REG_W_LAS0BA 0x14
#define   PDPLUS_PLX_REG_LAS0BA_ENABLE (0, 1)
#define   PDPLUS_PLX_REG_LAS0BA_INITIAL                       \
                ( SHIFT_VAL (PDPLUS_PLX_REG_LAS0BA_ENABLE, 1) \
                | 0x00000000                                  \
                )

#define PDPLUS_PLX_REG_R_LAS1BA 0x18
#define PDPLUS_PLX_REG_W_LAS1BA 0x18
#define   PDPLUS_PLX_REG_LAS1BA_ENABLE (0, 1)
#define   PDPLUS_PLX_REG_LAS1BA_INITIAL                       \
                ( SHIFT_VAL (PDPLUS_PLX_REG_LAS1BA_ENABLE, 1) \
                | 0x00020000                                  \
                )

#define PDPLUS_PLX_REG_R_LAS2BA 0x1c
#define PDPLUS_PLX_REG_W_LAS2BA 0x1c
#define   PDPLUS_PLX_REG_LAS2BA_ENABLE (0, 1)
#define   PDPLUS_PLX_REG_LAS2BA_INITIAL                       \
                ( SHIFT_VAL (PDPLUS_PLX_REG_LAS2BA_ENABLE, 1) \
                | 0x00020400                                  \
                )

#define PDPLUS_PLX_REG_R_LAS3BA 0x20
#define PDPLUS_PLX_REG_W_LAS3BA 0x20
#define   PDPLUS_PLX_REG_LAS3BA_ENABLE (0, 1)
#define   PDPLUS_PLX_REG_LAS3BA_INITIAL                       \
                ( SHIFT_VAL (PDPLUS_PLX_REG_LAS3BA_ENABLE, 0) \
                | 0x00020600                                  \
                )

#define PDPLUS_PLX_REG_R_LAS0BRD 0x28
#define PDPLUS_PLX_REG_W_LAS0BRD 0x28

#define PDPLUS_PLX_REG_R_LAS1BRD 0x2c
#define PDPLUS_PLX_REG_W_LAS1BRD 0x2c

#define PDPLUS_PLX_REG_R_LAS2BRD 0x30
#define PDPLUS_PLX_REG_W_LAS2BRD 0x30

#define PDPLUS_PLX_REG_R_LAS3BRD 0x34
#define PDPLUS_PLX_REG_W_LAS3BRD 0x34

#define PDPLUS_PLX_REG_R_CS0BASE 0x3C
#define PDPLUS_PLX_REG_W_CS0BASE 0x3C

#define PDPLUS_PLX_REG_R_CS1BASE 0x40
#define PDPLUS_PLX_REG_W_CS1BASE 0x40

#define PDPLUS_PLX_REG_R_CS2BASE 0x44
#define PDPLUS_PLX_REG_W_CS2BASE 0x44

#define PDPLUS_PLX_REG_R_CS3BASE 0x48
#define PDPLUS_PLX_REG_W_CS3BASE 0x48

#define PDPLUS_PLX_REG_R_INTCSR  0x4c
#define PDPLUS_PLX_REG_W_INTCSR  0x4c
#define   PDPLUS_PLX_REG_INTCSR_ENABLE_1    (0, 1)
#define   PDPLUS_PLX_REG_INTCSR_POLARITY_1  (1, 1)
#define   PDPLUS_PLX_REG_INTCSR_STATUS_1    (2, 1)
#define   PDPLUS_PLX_REG_INTCSR_ENABLE_2    (3, 1)
#define   PDPLUS_PLX_REG_INTCSR_POLARITY_2  (4, 1)
#define   PDPLUS_PLX_REG_INTCSR_STATUS_2    (5, 1)
#define   PDPLUS_PLX_REG_INTCSR_ENABLE_PCI  (6, 1)
#define   PDPLUS_PLX_REG_INTCSR_SOFT_INT    (7, 1)

#define PDPLUS_PLX_REG_R_CTRL 0x50
#define PDPLUS_PLX_REG_W_CTRL 0x50
#define   PDPLUS_PLX_REG_CTRL_RESET (30, 1)

/* FPGA control register definitions */
#define PDPLUS_FPGA_REG_W_CTRL 0x00
#define   PDPLUS_FPGA_REG_CTRL_A_PLAY_ENABLE       (0, 1)
#define   PDPLUS_FPGA_REG_CTRL_A_CAPT_ENABLE       (1, 1)
#define   PDPLUS_FPGA_REG_CTRL_D_PLAY_ENABLE       (2, 1)
#define   PDPLUS_FPGA_REG_CTRL_D_CAPT_ENABLE       (3, 1)
#define   PDPLUS_FPGA_REG_CTRL_A_PLAY_CNT_CLEAR    (4, 1)
#define   PDPLUS_FPGA_REG_CTRL_A_CAPT_CNT_CLEAR    (5, 1)
#define   PDPLUS_FPGA_REG_CTRL_D_PLAY_CNT_CLEAR    (6, 1)
#define   PDPLUS_FPGA_REG_CTRL_D_CAPT_CNT_CLEAR    (7, 1)

#define   PDPLUS_FPGA_REG_CTRL_A_CLK_SEL                (8, 3)

#define     PDPLUS_CLK_SEL_NO_CLK       0
#define     PDPLUS_CLK_SEL_FFG          1
#define     PDPLUS_CLK_SEL_DCO          2
#define     PDPLUS_CLK_SEL_EXT1         3
#define     PDPLUS_CLK_SEL_EXT2         4
#define     PDPLUS_CLK_SEL_D_CAPT       5

#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_NO_CLK  PDPLUS_CLK_SEL_NO_CLK
#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_FFG     PDPLUS_CLK_SEL_FFG
#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_DCO     PDPLUS_CLK_SEL_DCO
#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_EXT1    PDPLUS_CLK_SEL_EXT1
#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_EXT2    PDPLUS_CLK_SEL_EXT2
#define     PDPLUS_FPGA_REG_CTRL_A_CLK_SEL_D_CAPT  PDPLUS_CLK_SEL_D_CAPT

#define   PDPLUS_FPGA_REG_CTRL_D_CLK_SEL                (14, 3)

#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_NO_CLK  PDPLUS_CLK_SEL_NO_CLK
#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_FFG     PDPLUS_CLK_SEL_FFG
#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_DCO     PDPLUS_CLK_SEL_DCO
#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_EXT1    PDPLUS_CLK_SEL_EXT1
#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_EXT2    PDPLUS_CLK_SEL_EXT2
#define     PDPLUS_FPGA_REG_CTRL_D_CLK_SEL_D_CAPT  PDPLUS_CLK_SEL_D_CAPT

#define   PDPLUS_FPGA_REG_CTRL_A_ROUTE             (18, 2)
#define     PDPLUS_FPGA_REG_CTRL_A_ROUTE_A_PLAY    0
#define     PDPLUS_FPGA_REG_CTRL_A_ROUTE_D_PLAY    1
#define     PDPLUS_FPGA_REG_CTRL_A_ROUTE_A_CAPT    2
#define     PDPLUS_FPGA_REG_CTRL_A_ROUTE_D_CAPT    3

#define   PDPLUS_FPGA_REG_CTRL_D_ROUTE             (20, 2)
#define     PDPLUS_FPGA_REG_CTRL_D_ROUTE_D_PLAY    0
#define     PDPLUS_FPGA_REG_CTRL_D_ROUTE_A_PLAY    1
#define     PDPLUS_FPGA_REG_CTRL_D_ROUTE_A_CAPT    2
#define     PDPLUS_FPGA_REG_CTRL_D_ROUTE_D_CAPT    3

#define   PDPLUS_FPGA_REG_CTRL_CLEAR_INT           (23, 1)

/*
 * Two initial values.  After reset, _1 is written into register, then _2.  _1
 * clears INT and resets counters.  _2 should be 0 (we probably could assert()
 * that). */
#define   PDPLUS_FPGA_REG_CTRL_INITIAL_1                              \
                ( PDPLUS_VALUE (FPGA, CTRL, A_PLAY_CNT_CLEAR, 1)      \
                | PDPLUS_VALUE (FPGA, CTRL, D_PLAY_CNT_CLEAR, 1)      \
                | PDPLUS_VALUE (FPGA, CTRL, A_CAPT_CNT_CLEAR, 1)      \
                | PDPLUS_VALUE (FPGA, CTRL, D_CAPT_CNT_CLEAR, 1)      \
                | PDPLUS_CONST (FPGA, CTRL, A_CLK_SEL,        NO_CLK) \
                | PDPLUS_CONST (FPGA, CTRL, D_CLK_SEL,        NO_CLK) \
                | PDPLUS_CONST (FPGA, CTRL, A_ROUTE,          A_PLAY) \
                | PDPLUS_CONST (FPGA, CTRL, D_ROUTE,          D_PLAY) \
                | PDPLUS_VALUE (FPGA, CTRL, CLEAR_INT,        1)      \
                )

#define   PDPLUS_FPGA_REG_CTRL_INITIAL_2                       \
                ( PDPLUS_CONST (FPGA, CTRL, A_CLK_SEL, NO_CLK) \
                | PDPLUS_CONST (FPGA, CTRL, D_CLK_SEL, NO_CLK) \
                | PDPLUS_CONST (FPGA, CTRL, A_ROUTE,   A_PLAY) \
                | PDPLUS_CONST (FPGA, CTRL, D_ROUTE,   D_PLAY) \
                )

/*
 * Fixed Frequency Generator */
#define PDPLUS_FPGA_REG_W_FFG 0x04
#define   PDPLUS_FPGA_REG_FFG_FREQ  (0, 3)
#define     PDPLUS_FPGA_REG_FFG_FREQ_NO_CLK 0
#define     PDPLUS_FPGA_REG_FFG_FREQ_96000  1
#define     PDPLUS_FPGA_REG_FFG_FREQ_48000  2
#define     PDPLUS_FPGA_REG_FFG_FREQ_44100  3
#define     PDPLUS_FPGA_REG_FFG_FREQ_32000  4
#define     PDPLUS_FPGA_REG_FFG_FREQ_22050  5
#define     PDPLUS_FPGA_REG_FFG_FREQ_11025  6

#define   PDPLUS_FPGA_REG_FFG_INITIAL                    \
                ( PDPLUS_CONST (FPGA, FFG, FREQ, NO_CLK) \
                )

/*
 * FPGA DCO Register */
#define PDPLUS_FPGA_REG_W_DCO 0x08
#define   PDPLUS_FPGA_REG_DCO_FREQ           (0, 16)
#define   PDPLUS_FPGA_REG_DCO_SYNC_TO_ADAT   (16, 1)
#define   PDPLUS_FPGA_REG_DCO_COARSE_DISABLE (17, 1)
#define   PDPLUS_FPGA_REG_DCO_ADAT_FINE      (18, 1)

#define PDPLUS_FPGA_REG_DCO_INITIAL 0

#define PDPLUS_DCO_MIN_RATE  29000 /* Hz */
        /* Well, the DCO my card minimally goes down to 29100.
         * But we can try to reach 29000 Hz anyway... */

#define PDPLUS_DCO_MAX_RATE 105000 /* Hz */

/*
 * FPGA CS4222 Control */
#define PDPLUS_FPGA_REG_W_CS4222 0x0c
#define   PDPLUS_FPGA_REG_CS4222_VALUE (0, 8)
#define   PDPLUS_FPGA_REG_CS4222_REG   (8, 8)
#define   PDPLUS_FPGA_REG_CS4222_REST  (16, 8)
#define   PDPLUS_FPGA_REG_CS4222_REST_VALUE  0x0020

/*
 * FPGA Digital Source Select */
#define PDPLUS_FPGA_REG_W_DIG_SRC 0x10
#define   PDPLUS_FPGA_REG_DIG_SRC_SRC     (6, 2)
#define     PDPLUS_FPGA_REG_DIG_SRC_SRC_NONE    0
#define     PDPLUS_FPGA_REG_DIG_SRC_SRC_OPTICAL 1
#define     PDPLUS_FPGA_REG_DIG_SRC_SRC_COAXIAL 2

#define PDPLUS_FPGA_REG_DIG_SRC_INITIAL                      \
                ( PDPLUS_CONST (FPGA, DIG_SRC, SRC, COAXIAL) \
                )

/*
 * FPGA ADAT Monitor Control */
#define PDPLUS_FPGA_REG_W_ADAT 0x14
#define   PDPLUS_FPGA_REG_ADAT_MON_ENABLE    (0, 1)
#define   PDPLUS_FPGA_REG_ADAT_CHANNEL_LEFT  (1, 3)
#define   PDPLUS_FPGA_REG_ADAT_CHANNEL_RIGHT (4, 3)

#define PDPLUS_FPGA_REG_ADAT_INITIAL                          \
                ( PDPLUS_VALUE (FPGA, ADAT, CHANNEL_LEFT,  0) \
                | PDPLUS_VALUE (FPGA, ADAT, CHANNEL_RIGHT, 1) \
                )

/*
 * FPGA RD Register (Memory Area 2, Read Only) */
#define PDPLUS_FPGA_REG_R_CNT 0x00
#define   PDPLUS_FPGA_REG_CNT_A_PLAY              (0, 6)
#define   PDPLUS_FPGA_REG_CNT_A_CAPT              (6, 6)
#define   PDPLUS_FPGA_REG_CNT_D_PLAY              (12, 6)
#define   PDPLUS_FPGA_REG_CNT_D_CAPT              (18, 6)

#define PDPLUS_FPGA_REG_R_FREQ_SCAN 0x04
#define   PDPLUS_FPGA_REG_FREQ_SCAN_FREQ         (0, 16)

#define PDPLUS_FPGA_REG_R_STATUS 0x08
#define   PDPLUS_FPGA_REG_STATUS_INT             (6, 1)
#define   PDPLUS_FPGA_REG_STATUS_FSCAN           (7, 1)
#define   PDPLUS_FPGA_REG_STATUS_A_PLAY_RUNNING  (8, 1)
#define   PDPLUS_FPGA_REG_STATUS_A_CAPT_RUNNING  (9, 1)
#define   PDPLUS_FPGA_REG_STATUS_D_PLAY_RUNNING  (10, 1)
#define   PDPLUS_FPGA_REG_STATUS_D_CAPT_RUNNING  (11, 1)

#define   PDPLUS_FSCAN_POLL_CNT     100000
#define   PDPLUS_RUNNING_POLL_CNT   100000

/*
 * HW Write Register (Memory Area 3, Write Access).  The ones with CON are for consumer mode,
 * the PRO are for professional mode. */
#define PDPLUS_HW_REG_W_WR   0x00
#define PDPLUS_HW_REG_WR_RESET_CS4222       (4, 1, 1)
#define PDPLUS_HW_REG_WR_PRO1               (5, 1, 1)
#define PDPLUS_HW_REG_WR_PRO2               (7, 1)

/* Bits 1,2,3 and 6 are different in consumer and professional mode: */

/* Consumer: */
#define PDPLUS_HW_REG_WR_CON_COPY_INHIBIT   (1, 1, 1)

#define PDPLUS_HW_REG_WR_CON_FS             (2, 2)
#define   PDPLUS_HW_REG_WR_CON_FS_32000     1
#define   PDPLUS_HW_REG_WR_CON_FS_44100     0
#define   PDPLUS_HW_REG_WR_CON_FS_48000     2
#define   PDPLUS_HW_REG_WR_CON_FS_44100CD   3

#define PDPLUS_HW_REG_WR_CON_PRE_EMPHASIS   (6, 1)

/* Professional: */
#define PDPLUS_HW_REG_WR_PRO_NON_AUDIO (3, 1, 1)
        /* inverted: the pin is /C1, and C1=/AUDIO.  */

/* The trnpt bit must be 0 in professional mode (see CS8404 docu) */
#define PDPLUS_HW_REG_WR_PRO_TRNPT (2, 1)

/* frequency select, bit c6 */
#define PDPLUS_HW_REG_WR_PRO_FSC6                  (1, 1)
#define   PDPLUS_HW_REG_WR_PRO_FSC6_NOT_INDICATED  0
#define   PDPLUS_HW_REG_WR_PRO_FSC6_32000          1
#define   PDPLUS_HW_REG_WR_PRO_FSC6_44100          1
#define   PDPLUS_HW_REG_WR_PRO_FSC6_48000          0

/* frequency select, bit c7 */
#define PDPLUS_HW_REG_WR_PRO_FSC7                  (6, 1)
#define   PDPLUS_HW_REG_WR_PRO_FSC7_NOT_INDICATED  0
#define   PDPLUS_HW_REG_WR_PRO_FSC7_32000          1
#define   PDPLUS_HW_REG_WR_PRO_FSC7_44100          0
#define   PDPLUS_HW_REG_WR_PRO_FSC7_48000          1

/* For reading only (DONT USE THIS FOR WRITING!) */
#define PDPLUS_HW_REG_WR_PRO_FS                  (1, 6)
#define   PDPLUS_HW_REG_WR_PRO_FS_MASK           33
#define   PDPLUS_HW_REG_WR_PRO_FS_NOT_INDICATED  0
#define   PDPLUS_HW_REG_WR_PRO_FS_32000          33
#define   PDPLUS_HW_REG_WR_PRO_FS_44100          1
#define   PDPLUS_HW_REG_WR_PRO_FS_48000          32

/* Initial values: */
#define PDPLUS_HW_REG_WR_INITIAL_1                           \
                ( PDPLUS_VALUE (HW, WR, PRO1,       0)       \
                | PDPLUS_VALUE (HW, WR, PRO2,       0)       \
                | PDPLUS_VALUE (HW, WR, RESET_CS4222, 1)     \
                | PDPLUS_VALUE (HW, WR, CON_COPY_INHIBIT, 0) \
                )

#define PDPLUS_HW_REG_WR_INITIAL_2                           \
                ( PDPLUS_VALUE (HW, WR, PRO1,       0)       \
                | PDPLUS_VALUE (HW, WR, PRO2,       0)       \
                | PDPLUS_VALUE (HW, WR, RESET_CS4222, 0)     \
                | PDPLUS_VALUE (HW, WR, CON_COPY_INHIBIT, 0) \
                )

#define PDPLUS_HW_REG_W_LOAD 0x00

/*
 * HW Read Register (Memory Area 3, Read Access).
 * These are some pins of the CS 8414. */
#define PDPLUS_HW_REG_R_RD 0x00
#define PDPLUS_HW_REG_RD_F1                (4, 1)
#define PDPLUS_HW_REG_RD_F0                (5, 1)
#define PDPLUS_HW_REG_RD_F2                (6, 1)
#define PDPLUS_HW_REG_RD_ERROR_USER        (7, 1)

/*
 * CS 4222 Registers (unfortunately, they cannot be read) */

#define PDPLUS_CS4222_REG_R_RESERVED  0x00
#define PDPLUS_CS4222_REG_W_RESERVED  0x00

/*
 * ADC control register */
#define PDPLUS_CS4222_REG_R_ADC_CTRL  0x01
#define PDPLUS_CS4222_REG_W_ADC_CTRL  0x01

/* Read only (thus: useless) */
#define   PDPLUS_CS4222_REG_ADC_CTRL_CLOCKING_ERROR          (0, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_CALIBRATION_IN_PROGRESS (1, 1)
/* Read/Write: */
#define   PDPLUS_CS4222_REG_ADC_CTRL_CALIBRATION      (2, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_MUTE_LEFT        (3, 1, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_MUTE_RIGHT       (4, 1, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_HIGH_PASS_LEFT   (5, 1, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_HIGH_PASS_RIGHT  (6, 1, 1)
#define   PDPLUS_CS4222_REG_ADC_CTRL_POWER_DOWN       (7, 1)

#define PDPLUS_CS4222_REG_ADC_CTRL_INITIAL 0


/*
 * Muting the ADC will definitely confuse people.  I will leave the register
 * bits out range of users.
 *       ( PDPLUS_VALUE (CS4222, ADC_CTRL, MUTE_LEFT,  1)
 *       | PDPLUS_VALUE (CS4222, ADC_CTRL, MUTE_RIGHT, 1)
 *       )
 */


/*
 * DAC control register */
#define PDPLUS_CS4222_REG_R_DAC_CTRL  0x02
#define PDPLUS_CS4222_REG_W_DAC_CTRL  0x02
#define   PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP              (0, 2)
#define     PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_8_LRCKS    0
#define     PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_4_LRCKS    1
#define     PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_16_LRCKS   2
#define     PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_32_LRCKS   3

#define   PDPLUS_CS4222_REG_DAC_CTRL_SOFT_MUTE     (3, 1, 1)
#define   PDPLUS_CS4222_REG_DAC_CTRL_MUTE_LEFT     (4, 1, 1)
#define   PDPLUS_CS4222_REG_DAC_CTRL_MUTE_RIGHT    (5, 1, 1)
#define   PDPLUS_CS4222_REG_DAC_CTRL_AUTO_MUTE_512 (6, 1, 1)
#define   PDPLUS_CS4222_REG_DAC_CTRL_POWER_DOWN    (7, 1)

#define PDPLUS_CS4222_REG_DAC_CTRL_INITIAL               \
        ( PDPLUS_VALUE (CS4222, DAC_CTRL, MUTE_LEFT,  0) \
        | PDPLUS_VALUE (CS4222, DAC_CTRL, MUTE_RIGHT, 0) \
        )
/* ``WARNING!!! The mixer channels for the ALSA driver are muted by default!!!'' */


/*
 * Attenuators */
#define PDPLUS_CS4222_REG_R_ATT_LEFT  0x03
#define PDPLUS_CS4222_REG_W_ATT_LEFT  0x03
#define PDPLUS_CS4222_REG_ATT_LEFT_INITIAL  PDPLUS_CS4222_ATTEN_MAX

#define PDPLUS_CS4222_REG_R_ATT_RIGHT 0x04
#define PDPLUS_CS4222_REG_W_ATT_RIGHT 0x04
#define PDPLUS_CS4222_REG_ATT_RIGHT_INITIAL PDPLUS_CS4222_ATTEN_MAX

/*
 * DSP control */
#define PDPLUS_CS4222_REG_R_DSP       0x05
#define PDPLUS_CS4222_REG_W_DSP       0x05
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT                 (0, 3)
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT_I2S             0
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT_LEFT_JUSTIFIED  1
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT_20_BITS         2
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT_18_BITS         3
#define   PDPLUS_CS4222_REG_DSP_INPUT_FORMAT_16_BITS         4

#define   PDPLUS_CS4222_REG_DSP_OUTPUT_FORMAT                (3, 1)
#define   PDPLUS_CS4222_REG_DSP_OUTPUT_FORMAT_I2S            0
#define   PDPLUS_CS4222_REG_DSP_OUTPUT_FORMAT_LEFT_JUSTIFIED 1

#define   PDPLUS_CS4222_REG_DSP_CLOCK_EDGE                   (4, 1)
#define     PDPLUS_CS4222_REG_DSP_CLOCK_EDGE_RISING          0
#define     PDPLUS_CS4222_REG_DSP_CLOCK_EDGE_FALLING         1

#define   PDPLUS_CS4222_REG_DSP_DE_EMPHASIS                  (5, 3)
#define     PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_PIN_CONTROL    0
#define     PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_44100          1
#define     PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_48000          2
#define     PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_32000          3
#define     PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_DISABLED       4

#define PDPLUS_CS4222_REG_DSP_INITIAL                               \
                ( PDPLUS_CONST (CS4222, DSP, DE_EMPHASIS, DISABLED) \
                )

/*
 * The status is a read-only register (Useless, so unfortunately, the
 * overrange bits cannot be used) */
#define PDPLUS_CS4222_REG_R_STATUS    0x06
#define   PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT              (0, 3)
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_NORMAL     0
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_6_DB 1
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_5_DB 2
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_4_DB 3
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_3_DB 4
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_2_DB 5
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_MINUS_1_DB 6
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_LEFT_CLIPPING   7

#define   PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT              (3, 3)
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_NORMAL     0
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_6_DB 1
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_5_DB 2
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_4_DB 3
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_3_DB 4
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_2_DB 5
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_MINUS_1_DB 6
#define     PDPLUS_CS4222_REG_STATUS_LEVEL_RIGHT_CLIPPING   7

#define   PDPLUS_CS4222_REG_STATUS_ACCEPTED_LEFT  (6, 2, 1)
#define   PDPLUS_CS4222_REG_STATUS_ACCEPTED_RIGHT (7, 2, 1)

/* ********************************************************************** */
/* Various General Settings (all in bytes) */

#define PDPLUS_BUFFER_SIZE_LOG 15
#define PDPLUS_BUFFER_SIZE     (1U << PDPLUS_BUFFER_SIZE_LOG)
        /* On-board memory per substream */

#define PDPLUS_A_PLAY_OFFSET 0x0
#define PDPLUS_A_CAPT_OFFSET 0x8000
#define PDPLUS_D_PLAY_OFFSET 0x10000
#define PDPLUS_D_CAPT_OFFSET 0x18000
        /* Byte offsets in memory area of card */

#define PDPLUS_COUNTER_RESOLUTION 6
#define PDPLUS_COUNTER_SHIFT      (PDPLUS_BUFFER_SIZE_LOG - PDPLUS_COUNTER_RESOLUTION)
        /* 6 bits are readable from the the counter register.  The
         * PDPLUS_BUFFER_SIZE address range is 15 bits. */

#define PDPLUS_LARGE_BLOCK   PDPLUS_BUFFER_SIZE
        /* We can run with only one single block now. */

#define PDPLUS_SMALL_BLOCK (BIT_VAL (PDPLUS_COUNTER_SHIFT))
        /* The notice the block overrun, this is the minimum. */

#define PDPLUS_Q1_HZ 24576000
#define PDPLUS_Q2_HZ 33868800
        /* Frequency of the frequency generators on the card. */

#define PDPLUS_INTERRUPTS_PER_SEC (PDPLUS_Q1_HZ / 32768)
        /* Exact number of interrupts per second */

#define PDPLUS_CS4222_ATTEN_MIN       0
#define PDPLUS_CS4222_ATTEN_MAX       227
#define PDPLUS_CS4222_DB_PER_POINT    0.5
        /* Maximal value of the attenuator data byte that does not mean full muting.
         * +1 means complete mute.     
         */

/* How to communicate about sizes with the ALSA kernel API: */
#define PDPLUS_FRAME_SHIFT(IS_ADAT) ((IS_ADAT) ? 5 : 3)
        /* A frame in stereo (the current unit the API uses) is
         * 32 bits * 2 channels = 8 bytes = (1 << 3)
         *
         * For ADAT:
         * 32 bits * 8 channels = 32 bytes = (1 << 5)
         *
         * NOTE: if PDPLUS_FRAME_SHIFT becomes greater than PDPLUS_COUNTER_SHIFT,
         *       the below macros need to be changed, too.
         */


static int const is_adat = 0;
        /* FIXME: this is non-constant */

#define PDPLUS_BYTES_FROM_FRAMES(X,A) ((X) << PDPLUS_FRAME_SHIFT(A))
#define PDPLUS_FRAMES_FROM_BYTES(X,A) ((X) >> PDPLUS_FRAME_SHIFT(A))
        /* Byte <-> frame conversion */

#define PDPLUS_HWPTR_FROM_FRAMES(X,A) ((X) >> (PDPLUS_COUNTER_SHIFT_FRAME(A)))
#define PDPLUS_FRAMES_FROM_HWPTR(X,A) ((X) << (PDPLUS_COUNTER_SHIFT_FRAME(A)))
        /* HW_Ptr <-> frame conversion */

#define PDPLUS_HWPTR_FROM_BYTES(X)  ((X) >> PDPLUS_COUNTER_SHIFT)
#define PDPLUS_BYTES_FROM_HWPTR(X)  ((X) << PDPLUS_COUNTER_SHIFT)
        /* HW_Ptr <-> byte conversion */

#define PDPLUS_COUNTER_SHIFT_FRAME(A) (PDPLUS_COUNTER_SHIFT - PDPLUS_FRAME_SHIFT(A))
#define PDPLUS_BUFFER_SIZE_FRAME(A)   PDPLUS_FRAMES_FROM_BYTES (PDPLUS_BUFFER_SIZE,A)

/* ********************************************************************** */

/*
 * Some convenient type definitions */
typedef struct pci_dev       pci_dev_t;
typedef struct pci_device_id pci_device_id_t;
typedef void __iomem        *vm_offset_t;


#define PDPLUS_MODE_NONE     0
        /* NONE means uninitialised.  This happens at startup (for a
         * very short time) or when the hardware refuses to work
         * after a mode switch (ADAT <=> Digital).
         */

#define PDPLUS_MODE_DIGITAL   1
        /* Digital modes.  IEC 958 (S/PDIF) and AES/EBU.  Both use the same circuit. */

#define PDPLUS_MODE_ADAT      2
        /* ADAT mode for max. 8 channels */

#define PDPLUS_ROUTE_A_PLAY   0
#define PDPLUS_ROUTE_A_CAPT   1
#define PDPLUS_ROUTE_D_PLAY   2
#define PDPLUS_ROUTE_D_CAPT   3
        /* Monitoring settings */

#define PDPLUS_ADAT_RATE -7192
        /* Arbitrary value for dco_target_rate to indicate that DCO is synchronised
         * and locked to ADAT clock. */

/*
 * Memory mapped IO data structure
 */
typedef struct snd_iomem_stru {
        u_long      start;
        u_long      size;
        vm_offset_t vaddr;
	struct resource *resource;
} snd_iomem_t;

typedef struct pdplus_ring_ptr_stru {
        u_int playing;
        int   shift;
        u_int mask;
        u_int last_hw_ptr;
} pdplus_ring_ptr_t;

        /* Move X to next possible hw pointer register value */
#define PDPLUS_NEXT_RING_PTR(P) ((void)((P).playing = ((P).playing+1) & ((P).mask)))

/*
 * The private control structure of the driver */
typedef struct pdplus_stru {
        /* Parents */
        pci_dev_t   *pci;
        struct snd_card *card;

        /* Locks */
        /* For writing struct data or hardware registers the following lock is
         * used.  Note that it is used in the interrupt routine, so either use
         * _irqsave or be absolutely sure no interrupt from the Prodif Plus
         * can occur. */
        rwlock_t lock;

        /* Resources */
        int irq;

        /* PCI Bridge control */
        snd_iomem_t  PLX_iomem;
	unsigned long PLX_ioport;
	struct resource *res_PLX_io;

        /* On-board memory (4 * 32k) */
        snd_iomem_t MEM_iomem;

        /* FPGA Control RD/WR */
        snd_iomem_t FPGA_iomem;

        /* FPGA Load (initialisation) and HW */
        snd_iomem_t HW_iomem;

        /*
         * Devices and channels.  Note that t_* and d_* cannot be used at the
         * same time.  Another circuit needs to be uploaded. */
        struct snd_pcm *analog;
        struct snd_pcm_substream *a_play;
        struct snd_pcm_substream *a_capt;

        struct snd_pcm *digital;
        struct snd_pcm_substream *d_play;
        struct snd_pcm_substream *d_capt;

        struct snd_pcm *adat;
        struct snd_pcm_substream *t_play;
        struct snd_pcm_substream *t_capt;

        /*
         * HW Pointer positions where we currently are (generate interrupt when HW pointer
         * is different).  This is in hw pointer units for possibility of direct comparison
         * with ring buffer pointer register. */
        pdplus_ring_ptr_t a_play_ring_ptr;
        pdplus_ring_ptr_t a_capt_ring_ptr;
        pdplus_ring_ptr_t d_play_ring_ptr;
        pdplus_ring_ptr_t d_capt_ring_ptr;

        /* Values of registers */
        u32 PLX_CTRL_val;
        u32 PLX_INTCSR_val;
        u32 FPGA_CTRL_val;
        u32 FPGA_FFG_val;
        u32 FPGA_DCO_val;

        /* Misc */
        int     dco_target_rate;  /* Hz */
        volatile u_long ticker;   /* at 2^32 bits: overrun after 66.28 days */

        /* Small data */
        u8 FPGA_DIG_SRC_val;
        u8 CS4222_ADC_CTRL_val;
        u8 CS4222_DAC_CTRL_val;
        u8 CS4222_ATT_LEFT_val;
        u8 CS4222_ATT_RIGHT_val;
        u8 CS4222_DSP_val;
        u8 FPGA_ADAT_val;
        u8 HW_WR_val;

        u_int  mode             : 2; /* PDLPUS_MODE_* */
        u_int  a_play_prepared  : 1; /* BOOL */
        u_int  a_capt_prepared  : 1; /* BOOL */
        u_int  d_play_prepared  : 1; /* BOOL */
        u_int  d_capt_prepared  : 1; /* BOOL */

        /* These may be changable by the user some time.  Currently, they are ignored
         * dig_valid is encountered. */
	struct snd_aes_iec958	dig_setup;
        u_int  auto_profi_mode  : 1; /* BOOL */
        u_int  profi_mode       : 1; /* BOOL mode to choose if auto_profi_mode == 0*/
        u_int  auto_cd_mode     : 1; /* BOOL */
        u_int  copy_inhibit     : 1; /* BOOL, consumer mode only */
        u_int  pre_emphasis     : 1; /* BOOL, consumer mode only */
        u_int  non_audio        : 1; /* BOOL, professional mode only */

        /* The output routes */
        u_int  a_out_route      : 2; /* PDPLUS_ROUTE_* what the DAC is to output */
        u_int  d_out_route      : 2; /* PDPLUS_ROUTE_* what the DTx is to output */
} pdplus_t;

/* ********************************************************************** */

static void  pdplus_sweep (struct snd_card *);

static int   pdplus_a_play_prepare (struct snd_pcm_substream *);
static int   pdplus_a_play_trigger (struct snd_pcm_substream *, int);
static snd_pcm_uframes_t pdplus_a_play_pointer (struct snd_pcm_substream *);

static int   pdplus_d_play_prepare (struct snd_pcm_substream *);
static int   pdplus_d_play_trigger (struct snd_pcm_substream *, int);
static snd_pcm_uframes_t pdplus_d_play_pointer (struct snd_pcm_substream *);

static int   pdplus_a_capt_prepare (struct snd_pcm_substream *);
static int   pdplus_a_capt_trigger (struct snd_pcm_substream *, int);
static snd_pcm_uframes_t pdplus_a_capt_pointer (struct snd_pcm_substream *);

static int   pdplus_d_capt_prepare (struct snd_pcm_substream *);
static int   pdplus_d_capt_trigger (struct snd_pcm_substream *, int);
static snd_pcm_uframes_t pdplus_d_capt_pointer (struct snd_pcm_substream *);

static BOOL pdplus_switch_circuit (pdplus_t *scard, int mode);
static int  pdplus_set_mode (pdplus_t *scard, int mode);

/* ********************************************************************** */

static vm_offset_t __devinit pdplus_remap_pci_mem (u_long base, u_long size)
{
        u_long page_base    = ((u_long) base) & PAGE_MASK;
        u_long page_offs    = ((u_long) base) - page_base;
        vm_offset_t page_remapped = ioremap_nocache (page_base, page_offs+size);

        Vprintk ("base=0x%lx, offs=0x%lx, remapped=0x%lx\n",
                page_base,
                page_offs,
		 (unsigned long)page_remapped);

        return (vm_offset_t) (page_remapped ? (page_remapped + page_offs) : NULL);
}

static void pdplus_unmap_pci_mem (snd_iomem_t *mem)
{
        if (mem) {
        	if (mem->vaddr)
	                iounmap((void __iomem *)((unsigned long)mem->vaddr & PAGE_MASK));
		if (mem->resource) {
			release_resource(mem->resource);
			kfree_nocheck(mem->resource);
		}
	}
}

/* ********************************************************************** */
/* Low-level routines.
 *
 * NOTE: Routine name endings: _ll* indicates that some kind of lock is
 *       necessary before calling the functions:
 *   _ll  : possibly writes data into *scard or registers.
 *   _llr : possibly reads data from *scard or registers.
 *   _lli : possibly reads data and needs uninterrupted execution (_irqsave required).
 *
 */

/*
 * This function takes a given block size, the number of channels and the
 * sample rate and calculates the smallest possible block size which is
 * possible on the hardware.  The number of interrupts per seconds is
 * constant, so for higher sampling frequencies, the minimal block size
 * is larger.
 *
 * Input unit:  frame
 * Output unit: frame
 */
static int pdplus_fit_period_size (
        unsigned int block_size,
        int is_adat,
        unsigned int sample_rate)
{
        unsigned int lauf;
        unsigned int frames_per_tick = sample_rate / PDPLUS_INTERRUPTS_PER_SEC;
        u_long hw_ptr_block = BIT_VAL (PDPLUS_COUNTER_SHIFT_FRAME (is_adat));
        u_long buffer_size_frame =  PDPLUS_BUFFER_SIZE_FRAME (is_adat);
        unsigned int minimum_block_size = PDPLUS_UPROUND (
                frames_per_tick,
                hw_ptr_block);

        if (block_size < minimum_block_size)
                block_size = minimum_block_size;

        if (block_size > buffer_size_frame)
                block_size = buffer_size_frame;

        lauf = hw_ptr_block;
        while (lauf < block_size)
                lauf+= lauf;

        return lauf;
}

/*
 * **** PLX **** */
/*
 * Program PLX to disallow interrupts */
static void pdplus_disable_interrupts_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_1,   1);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, STATUS_1,   0);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_2,   0);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, STATUS_2,   0);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_PCI, 0);
        PDPLUS_REWRITE_HW    (scard, PLX, INTCSR);
}

/*
 * Program PLX to allow interrupts */
static void pdplus_enable_interrupts_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_1,   1);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, STATUS_1,   0);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_2,   1);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, STATUS_2,   0);
        PDPLUS_REPLACE_CACHE (scard, PLX, INTCSR, ENABLE_PCI, 1);
        PDPLUS_REWRITE_HW    (scard, PLX, INTCSR);
}

/*
 * Read the contents of some PLX registers into the cache */
static void pdplus_read_initial_values_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        PDPLUS_READ_INIT (scard, PLX, CTRL);
        PDPLUS_READ_INIT (scard, PLX, INTCSR);
}

/*
 * Reset the components on the card. */
static void pdplus_local_reset_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        /*
         * Do a local reset. */
        PDPLUS_REPLACE (scard, PLX, CTRL, RESET, 1);
        udelay (100);

        PDPLUS_REPLACE (scard, PLX, CTRL, RESET, 0);
        mdelay (5);
}


/*
 * **** ADC/DAC **** */
static void pdplus_adda_write_ll (pdplus_t *scard, u_int reg, u_int value)
{
        u_int regval;
        PDPLUS_LOCAL_VADDR (scard);

        snd_assert (reg >= 1 && reg <= 6, return);
        snd_assert (value <= 255, return);

        regval = 0x200000 | (reg << 8) | value;
        PDPLUS_WRITE_HW (scard, FPGA, CS4222, regval);

        mdelay (1);
}

/*
 * **** FPGA **** */
/*
 * Clear interrupt status of cards */
inline static /* very short, should be fast */
void pdplus_clear_interrupt_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        PDPLUS_REPLACE (scard, FPGA, CTRL, CLEAR_INT, 1);
        PDPLUS_REPLACE (scard, FPGA, CTRL, CLEAR_INT, 0);
}

/*
 * Programmes a desired frequency rate into the DCO freq register.
 * NOTE: The caller must wait for the frequency to stabilise! */
static void pdplus_dco_programme_rate_ll (pdplus_t *scard, int rate)
{
        int dco_rate;
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (scard != NULL, return);

        if (rate < PDPLUS_DCO_MIN_RATE)
                rate = PDPLUS_DCO_MIN_RATE;

        if (rate > PDPLUS_DCO_MAX_RATE)
                rate = PDPLUS_DCO_MAX_RATE;

        /* NOTE:
         *     0.0853333etc equals 32/375
         *     105000 * 32 < 2^32, so:
         */
#if 1
        dco_rate = (rate * 32) / 375;
#else
        dco_rate = (rate * 0.085333333333333333333333333333L);
#endif

        if (scard->mode == PDPLUS_MODE_ADAT) {
                PDPLUS_REPLACE_CACHE (scard, FPGA, DCO, ADAT_FINE, 0);
                PDPLUS_REPLACE_CACHE (scard, FPGA, DCO, SYNC_TO_ADAT, 0);
        }

        PDPLUS_REPLACE_CACHE (scard, FPGA, DCO, FREQ, dco_rate);
        PDPLUS_REWRITE_HW (scard, FPGA, DCO);

        scard->dco_target_rate = rate;
}


/*
 * This waits for one frequency scan and returns that value.
 * NOTE: The caller must ensure stability to get a good frequency.
 * A return value of 0 indicates an error (timeout). */
static int pdplus_dco_scan_rate_lli (pdplus_t *scard)
{
        u_int x;
        int tend;
        PDPLUS_LOCAL_VADDR (scard);

        /* Wait for FSCAN high */
        tend = PDPLUS_FSCAN_POLL_CNT;
        while (PDPLUS_EXTRACT_HW (scard, FPGA, STATUS, FSCAN) == 0) {
                if (tend <= 0)
                        return 0;
                tend--;
        }

        /* Wait for FSCAN low */
        tend = PDPLUS_FSCAN_POLL_CNT;
        while (PDPLUS_EXTRACT_HW (scard, FPGA, STATUS, FSCAN) != 0) {
                if (tend <= 0)
                        return 0;
                tend--;
        }

        /* Read value */
        x = PDPLUS_EXTRACT_HW (scard, FPGA, FREQ_SCAN, FREQ);
        if (x == 0)
                return 0;

        /* x = (24576000 / x) * 32; */
        return (24576000 * 32) / x;  /* Why not this? */
}


/*
 * Wait for stabilisation of dco rate.  According to spec, stabilisation may take
 * as long as 25 ms. */
static int pdplus_dco_wait_stabilise_ll (pdplus_t *scard)
#define DCO_MAX_LOOP 500
{
        u_int tend = DCO_MAX_LOOP; /* we will wait 100us between stabilisation checks */
        u_int last_scan = pdplus_dco_scan_rate_lli (scard);
        u_int current_scan;

        while ((current_scan = pdplus_dco_scan_rate_lli (scard)) != last_scan) {
                if (tend == 0) {
                        printk (PDPLUS_KERN_ERR
                                "DCO clock did not stabilise (last=%u Hz, current=%u Hz).\n",
                                current_scan,
                                last_scan);
                        LEAVE (-EIO);
                }
                udelay (200);
                tend--;
                last_scan = current_scan;
        }

        Vprintk (PDPLUS_KERN_INFO
                "DCO clock needed %d loops to stabilise (freq=%u Hz)\n",
                DCO_MAX_LOOP - tend,
                current_scan);

        return 0;
}
#undef DCO_MAX_LOOP

#if 0
/*
 * Write 0 into the given memory area */
static void pdplus_clear_iomem_ll (snd_iomem_t *iomem)
{
        u_long lauf;
        u_long end;
        snd_assert (iomem != NULL, return);

        lauf = iomem->vaddr;
        end =  iomem->vaddr + iomem->size);

        while (lauf < end) {
                IOMEM_WRITE (lauf, 0);
                lauf+= 4;
        }
}
#endif

/*
 * Return a pointer to the channel status or NULL if none is available */
static u_char *pdplus_get_channel_status (pdplus_t *scard)
{
        if (scard != NULL)
        	return scard->dig_setup.status;

        return NULL;
}

/*
 * Set the rate on the CS8404 chip.  It depends in the mode how this is done, since the
 * bits have different meanings. */
static void pdplus_cs8404_prepare_registers_ll (pdplus_t *scard, int rate)
{
        u_char *cs;
        int profi;
        int non_audio;
        int pre_emphasis; /* 50/15 */
        int copy_inhibit;
        int auto_cd_mode;
        PDPLUS_LOCAL_VADDR (scard);

        cs = pdplus_get_channel_status (scard);
        if (cs != NULL) {
                profi =     cs[0] & IEC958_AES0_PROFESSIONAL;
                non_audio = cs[0] & IEC958_AES0_NONAUDIO;

                if (profi) {
                        switch (cs[0] & IEC958_AES0_PRO_FS) {
                        case IEC958_AES0_PRO_FS_NOTID: rate = 0;
                        case IEC958_AES0_PRO_FS_44100: rate = 44100;
                        case IEC958_AES0_PRO_FS_32000: rate = 32000;
                        case IEC958_AES0_PRO_FS_48000: rate = 48000;
                        }
                        copy_inhibit = 0;
                        auto_cd_mode = 0;
                        pre_emphasis = (cs[0] & IEC958_AES0_PRO_EMPHASIS) ==
                                IEC958_AES0_PRO_EMPHASIS_5015;
                }
                else {
                        switch (cs[3] & IEC958_AES3_CON_FS) {
                        case IEC958_AES3_CON_FS_32000: rate = 32000;
                        case IEC958_AES3_CON_FS_44100: rate = 44100;
                        case IEC958_AES3_CON_FS_48000: rate = 48000;
                        default: rate = 0;
                        }

                        copy_inhibit = cs[0] & IEC958_AES0_CON_NOT_COPYRIGHT;
                        pre_emphasis = (cs[0] & IEC958_AES0_CON_EMPHASIS) ==
                                IEC958_AES0_CON_EMPHASIS_5015;

                        auto_cd_mode = (cs[1] & IEC958_AES1_CON_CATEGORY) ==
                                IEC958_AES1_CON_IEC908_CD;
                                /* Bäh! should be IEC958_AES1_CON_CATEGORY_IEC908_CD!! */
                }

                /* Now, this information should be programmed best int othe 8404... */
        }
        else {
                if (scard->auto_profi_mode)
                        profi = rate > 48000;
                else
                        profi = scard->profi_mode;

                non_audio =    scard->non_audio;
                copy_inhibit = scard->copy_inhibit;
                pre_emphasis = scard->pre_emphasis;
                auto_cd_mode = scard->auto_cd_mode;
        }

#if 0
        profi= 1;
        non_audio= 1;
#endif

        if (profi) {
                /* professional mode */
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO1, 1);
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO2, 1);

                switch (rate) {
                case 48000:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC6, 48000);
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC7, 48000);
                        break;

                case 44100:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC6, 44100);
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC7, 44100);
                        break;

                case 32000:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC6, 32000);
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC7, 32000);
                        break;

                default:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC6, NOT_INDICATED);
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, PRO_FSC7, NOT_INDICATED);
                }
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO_NON_AUDIO, non_audio ? 1 : 0);
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO_TRNPT, 0); /* must always be 0 */
        }
        else {
                /* consumer mode */
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO1, 0);
                PDPLUS_REPLACE_CACHE (scard, HW, WR, PRO2, 0);

                switch (rate) {
                default:
                case 48000:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, CON_FS, 48000);
                        break;

                case 44100:
                        if (auto_cd_mode)
                                PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, CON_FS, 44100CD);
                        else
                                PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, CON_FS, 44100);
                        break;

                case 32000:
                        PDPLUS_REPLACE_CONST_CACHE (scard, HW, WR, CON_FS, 32000);
                        break;
                }
                PDPLUS_REPLACE_CACHE (scard, HW, WR, CON_PRE_EMPHASIS, pre_emphasis ? 1 : 0);
                PDPLUS_REPLACE_CACHE (scard, HW, WR, CON_COPY_INHIBIT, copy_inhibit ? 1 : 0);
        }

        /* Write the hardware register after all these changes (to those five bits...) */
        PDPLUS_REWRITE_HW (scard, HW, WR);
}

/*
 * **** More complex low level functions **** */
/*
 * The enable bits are cleared,  counters reset and interrupt status reset.
 * Only call after reset and uploading of circuit */
static void pdplus_init_circuit_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        /* Initial value for FPGA regs */
        PDPLUS_WRITE_CONST (scard, FPGA, CTRL, INITIAL_1); /* clears counters etc. */
        udelay (100);
        PDPLUS_WRITE_CONST (scard, FPGA, CTRL, INITIAL_2);
        mdelay (1);

        PDPLUS_WRITE_CONST (scard, FPGA, FFG,     INITIAL);
        PDPLUS_WRITE_CONST (scard, FPGA, DCO,     INITIAL);
        PDPLUS_WRITE_CONST (scard, FPGA, DIG_SRC, INITIAL);
        PDPLUS_WRITE_CONST (scard, FPGA, ADAT,    INITIAL);

        /* Reset CS4222 */
        PDPLUS_WRITE_CONST (scard, HW, WR, INITIAL_1);
        mdelay (10); /* To reset circuit, 10 ms are required. */
        PDPLUS_WRITE_CONST (scard, HW, WR, INITIAL_2);
        mdelay (50 - 10);
        /* OUCH: 50ms restauration time according to 4222 spec.
         * We will wait at least 10 ms until the next access to the 4222 now, so we
         * actually only wait 30ms now.
         */

        /* Set the frequencies of DCO and FFG to an initial 44100. */
        PDPLUS_REPLACE_CONST (scard, FPGA, FFG, FREQ, PDPLUS_DEFAULT_CLOCK);
        pdplus_dco_programme_rate_ll (scard, PDPLUS_DEFAULT_CLOCK);
        mdelay (10);
                /* Either the dco or the freq. scanner need more time to
                 * wake up here.  Therefore, we sleep another while. */
        pdplus_dco_wait_stabilise_ll (scard);

        /* CS4222 initialisation */
        /* I suppose the CS4222 needs a clock to receive its commands.
         * Take the DCO, it already runs.   Furthermore, it is nice for
         * the user to see the digital transmitter generates a signal.
         * So please them with a burning LED when the driver is loaded.
         * (Well, my amplifier has such an LED at least). */
        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, FFG);
        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, FFG);
        pdplus_cs8404_prepare_registers_ll (scard, PDPLUS_DEFAULT_CLOCK);
        udelay (10);

        /* Write initial values into CS4222 registers */
        PDPLUS_WRITE_CONST (scard, CS4222, ADC_CTRL,  INITIAL);
        PDPLUS_WRITE_CONST (scard, CS4222, DAC_CTRL,  INITIAL);
        PDPLUS_WRITE_CONST (scard, CS4222, ATT_LEFT,  INITIAL);
        PDPLUS_WRITE_CONST (scard, CS4222, ATT_RIGHT, INITIAL);
        PDPLUS_WRITE_CONST (scard, CS4222, DSP,       INITIAL);

        /* FIXME: TEST: Enable digital capture. */
        /* PDPLUS_REPLACE (scard, FPGA, CTRL, D_CAPT_ENABLE, 1); */
        /* PDPLUS_REPLACE (scard, FPGA, CTRL, A_CAPT_ENABLE, 1); */
}


/*
 * Shutdown the card via a local reset */
static void pdplus_shutdown_ll (pdplus_t *scard)
{
        if ((scard->PLX_iomem.vaddr) == 0)
                return;

        pdplus_local_reset_ll (scard);
}


/* Local defines */
#define PDPLUS_WRITE_CYCLES  4

/*
 * Copy data into the card.  This is a bit more complex than one might
 * think because according to the spec, after four write accesses a read
 * access must follow. */
static __inline__ /* only used twice */
void pdplus_copy_from_user_ll (
        vm_offset_t dst,
        const u32 __user *src,
        size_t byte_count)

/* FIXME: Maybe with a few kernel patches this would result in better code. */
#define ONE_CYCLE             \
        __get_user (x, src);  \
        IOMEM_WRITE (dst, x); \
        src++;                \
        dst+= 4;

/* Body */
{
        if (access_ok (VERIFY_READ, src, byte_count)) {
                int int_count =   byte_count / 4;
                int cycle_count = int_count / PDPLUS_WRITE_CYCLES;
                u32 x;

                /* initial read cycle on PCI bus to please hardware */
                readl (dst);

                /*
                 * Use that legendary switch/while construct to handle the loop
                 * without special cases. */
                switch (int_count % PDPLUS_WRITE_CYCLES) {
                        default:
                                snd_assert (0, return);
                        do {
                                cycle_count--;

                                readl (dst); /* read cycle in PCI bus to please hardware */
                                ONE_CYCLE;
                        case 3:
                                ONE_CYCLE;
                        case 2:
                                ONE_CYCLE;
                        case 1:
                                ONE_CYCLE;
                        case 0:
				;
                        } while (cycle_count);
                }
        }
        return;
}
#undef ONE_CYCLE


/*
 * Write zero data into the card. */
static void pdplus_write_silence_ll (
        vm_offset_t dst,
        size_t byte_count)

#define ONE_CYCLE                    \
        IOMEM_WRITE (dst, (u_int)0); \
        dst+= 4;

{
        int int_count =   byte_count / 4;
        int cycle_count = int_count / PDPLUS_WRITE_CYCLES;

        /* initial read cycle on PCI bus to please hardware */
        readl (dst);

        /*
         * Use that legendary switch/while construct to handle the loop
         * without special cases. */
        switch (int_count % PDPLUS_WRITE_CYCLES) {
                default:
                        snd_assert (0, return);
                do {
                        cycle_count--;

                        readl (dst); /* read cycle in PCI bus to please hardware */
                        ONE_CYCLE;
                case 3:
                        ONE_CYCLE;
                case 2:
                        ONE_CYCLE;
                case 1:
                        ONE_CYCLE;
                case 0:
			;
                } while (cycle_count);
        }
        return;
}
#undef ONE_CYCLE


/*
 * Copy data from user space to device's analog play memory */
static int pdplus_a_play_copy_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        void __user *src,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        /* pdplus doc: After after four writes at lastest, a read must be done.
         *             So we cannot memcpy_toio. */
        pdplus_copy_from_user_ll (
                scard->MEM_iomem.vaddr + PDPLUS_A_PLAY_OFFSET + bpos,
                src,
                bcount);

        return ucount;
}

/*
 * Fill analog play memory with zero. */
static int pdplus_a_play_silence_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        /* pdplus doc: After after four writes at lastest, a read must be done.
         *             So we cannot memcpy_toio. */
        pdplus_write_silence_ll (
                scard->MEM_iomem.vaddr + PDPLUS_A_PLAY_OFFSET + bpos,
                bcount);

        return ucount;
}

/*
 * Copy data to user space from device's analog capt memory */
static int pdplus_a_capt_copy_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        void __user *dst,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        /* No constraints about access here.  So we can use memcpy */
        copy_to_user_fromio (
                dst,
                scard->MEM_iomem.vaddr + PDPLUS_A_CAPT_OFFSET + bpos,
                bcount);

        return ucount;
}

/*
 * Copy data from user space to device's digital play memory */
static int pdplus_d_play_copy_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        void __user *src,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        pdplus_copy_from_user_ll (
                scard->MEM_iomem.vaddr + PDPLUS_D_PLAY_OFFSET + bpos,
                src,
                bcount);

        return ucount;
}

/*
 * Fill analog play memory with zero. */
static int pdplus_d_play_silence_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        pdplus_write_silence_ll (
                scard->MEM_iomem.vaddr + PDPLUS_D_PLAY_OFFSET + bpos,
                bcount);

        return ucount;
}

/*
 * Get the input rate from the 8414 on the digital input.
 * Return values:
 *     0 - out of range
 *     96000, 88200, 48000, 44100, 32000 - rate in Hertz +- 4%
 */
static unsigned int pdplus_d_capt_rate_llr (pdplus_t *scard)
{
        static unsigned int const code2rate[8]= { 0, 0, 0, 96000, 88200, 48000, 44100, 32000 };
        int rate_code = 0;
        int reg_value;
        PDPLUS_LOCAL_VADDR (scard);

        reg_value = PDPLUS_READ_HW (scard, HW, RD);
        if (PDPLUS_CUT_BITS (PDPLUS_HW_REG_RD_F0, reg_value) != 0) rate_code|= 1;
        if (PDPLUS_CUT_BITS (PDPLUS_HW_REG_RD_F1, reg_value) != 0) rate_code|= 2;
        if (PDPLUS_CUT_BITS (PDPLUS_HW_REG_RD_F2, reg_value) != 0) rate_code|= 4;

        snd_assert (rate_code >= 0, return 0);
        snd_assert (rate_code < 8, return 0);

        return code2rate[rate_code];
}

/*
 * Copy data to user space from device's digital capt memory */
static int pdplus_d_capt_copy_ll (
        struct snd_pcm_substream *substream,
        int channel, /* not used (interleaved data) */
        snd_pcm_uframes_t upos,
        void __user *dst,
        snd_pcm_uframes_t ucount
)
{
	pdplus_t *scard = snd_pcm_substream_chip(substream);
        u_int  bpos =     PDPLUS_BYTES_FROM_FRAMES (upos,   is_adat);
        size_t bcount =   PDPLUS_BYTES_FROM_FRAMES (ucount, is_adat);

        snd_assert (bpos < PDPLUS_BUFFER_SIZE, return -EINVAL);
        snd_assert (bpos + bcount - 1 < PDPLUS_BUFFER_SIZE, return -EINVAL);

        /* Check input frequency and possibly generate an error. */
        if (substream->runtime->rate != pdplus_d_capt_rate_llr (scard))
                return -EIO;

        /* No constraints about access here.  So we can use memcpy */
        copy_to_user_fromio (
                dst,
                scard->MEM_iomem.vaddr + PDPLUS_D_CAPT_OFFSET + bpos,
                bcount);

        return ucount;
}


/*
 * Stop playing by resetting the enable bit, waiting for
 * running bits to go to low, and clear the counters. */
static void pdplus_stop_device_ll (
        pdplus_t *scard,
        u_int     enable,
        u_int     running)
{
        u_int tend;
        PDPLUS_LOCAL_VADDR (scard);

        /* Is the device enabled?  If not, leave immediately. */
        if (PDPLUS_EXTRACT_CACHE_BIT (scard, FPGA, CTRL, enable) == 0)
                return;

        /* Clear enable bit (stop play) */
        PDPLUS_REPLACE_BIT (scard, FPGA, CTRL, enable, 0);

        /* Wait for running bit to drop to 0 */
        tend = 5000; /* max. wait 50ms */
        while (PDPLUS_EXTRACT_HW_BIT (scard, FPGA, STATUS, running) != 0) {
                if (tend == 0) {
                        printk (PDPLUS_KERN_ERR "Running bit did not drop to 0.\n");
                        return;
                }
                udelay (10);
                tend--;
        }
}


/*
 * Stop analog play */
inline static 
void pdplus_a_play_stop_ll (pdplus_t *scard)
{
        /* Stop playing */
        pdplus_stop_device_ll (
                scard,
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_CTRL_A_PLAY_ENABLE),
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_STATUS_A_PLAY_RUNNING));
}


/*
 * Stop analog capture */
inline static 
void pdplus_a_capt_stop_ll (pdplus_t *scard)
{
        /* Stop playing */
        pdplus_stop_device_ll (
                scard,
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_CTRL_A_CAPT_ENABLE),
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_STATUS_A_CAPT_RUNNING));
}


/*
 * Stop digital play */
inline static 
void pdplus_d_play_stop_ll (pdplus_t *scard)
{
        /* Stop playing */
        pdplus_stop_device_ll (
                scard,
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_CTRL_D_PLAY_ENABLE),
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_STATUS_D_PLAY_RUNNING));
}


/*
 * Stop digital capture */
inline static 
void pdplus_d_capt_stop_ll (pdplus_t *scard)
{
        /* Stop playing */
        pdplus_stop_device_ll (
                scard,
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_CTRL_D_CAPT_ENABLE),
                PDPLUS_GET_MASK (PDPLUS_FPGA_REG_STATUS_D_CAPT_RUNNING));
}


/*
 * If ADAT monitoring is running, disable it. */
inline static 
u_int pdplus_adat_mon_disable_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, ADAT, MON_ENABLE)) {
                PDPLUS_REPLACE (scard, FPGA, ADAT, MON_ENABLE, 0);
                pdplus_a_play_stop_ll (scard);
                return 1;
        }
        return 0;
}

/*
 * Set the flags to run adat monitoring. */
inline static 
void pdplus_adat_mon_enable_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);
        PDPLUS_REPLACE (scard, FPGA, ADAT, MON_ENABLE, 1);
        PDPLUS_REPLACE (scard, FPGA, CTRL, A_PLAY_ENABLE, 1);
}

/*
 * Sync the DCO to ADAT input freq. */
static int __attribute__((__unused__)) pdplus_dco_sync_to_adat_ll (pdplus_t *scard)
{
        int err;
        PDPLUS_LOCAL_VADDR (scard);

        if (scard->dco_target_rate == PDPLUS_ADAT_RATE)
                return 0;

        PDPLUS_REPLACE (scard, FPGA, DCO, SYNC_TO_ADAT, 1);
        if ((err = pdplus_dco_wait_stabilise_ll (scard)) < 0)
                return err;

        PDPLUS_REPLACE (scard, FPGA, DCO, ADAT_FINE, 1);

        scard->dco_target_rate = PDPLUS_ADAT_RATE;
        return 0;
}

/*
 * I'm not sure yet, whether this will be needed.  Why should one detach again?
 * This is simply re-programming of the DCO, so simply keep it synched is ok?  Is it?
 */
inline static 
void __attribute__((__unused__)) pdplus_dco_detach_from_adat_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        if (scard->dco_target_rate == PDPLUS_ADAT_RATE) {
                pdplus_dco_programme_rate_ll (scard, PDPLUS_DEFAULT_CLOCK);
                pdplus_dco_wait_stabilise_ll (scard);
        }
}

static int pdplus_get_a_route_llr (pdplus_t *scard)
{
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, ADAT, MON_ENABLE))
                return PDPLUS_FPGA_REG_CTRL_A_ROUTE_D_CAPT;
        else
                return PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_ROUTE);
}

/*
 * The following function starts ADAT monitoring (full procedure).
 * To restore ADAT monitoring that once worked, use
 * pdplus_adat_mon_enable_ll*/
inline static 
int pdplus_adat_mon_start_ll (pdplus_t *scard)
{
        int err;
        PDPLUS_LOCAL_VADDR (scard);

        if ((err = pdplus_dco_sync_to_adat_ll (scard)) < 0)
                return err;

        pdplus_adat_mon_enable_ll (scard);
        return 0;
}

/*
 * Stop adat monitoring (includes check for right mode) */
inline static 
void pdplus_adat_mon_stop_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        if (scard->mode == PDPLUS_MODE_ADAT)
                pdplus_adat_mon_disable_ll (scard);
}

/*
 * Find a valid frequency for analog play/record. */
static int pdplus_a_adjust_rate (int rate)
{
	if (rate < 11025)
		return rate;

	if (rate > 50000)
		return 50000;

	return rate;
}

/*
 * Valid frequencies for digital play/record.  Note that the digital rates are very
 * limited. */
static unsigned int d_rate_llr[5] = {
	32000, 44100, 48000, 88200, 96000
};

/*
 * Find a valid frequency for digital play/record.  Note that the digital rates are very
 * limited. */
static int pdplus_d_adjust_rate_llr (pdplus_t *scard, int rate, BOOL always_profi)
{
#if IEC958_ALLOW_HIGH_FREQ
	int const profi = 1;
#else
	int profi = (scard->profi_mode || scard->auto_profi_mode);
                
	if (!always_profi) {
		u_char *cs = pdplus_get_channel_status (scard);
		if (cs != NULL)
			profi = cs[0] & IEC958_AES0_PROFESSIONAL;
	}
#endif
	/* IEC958 & AES/EBU: */
	if (rate < 32050) return 32000;
	if (rate < 44150) return 44100;
	if (profi == 0)   return 48000;

	/* AES/EBU only: */
	if (rate < 48100) return 48000;
	if (rate < 88300) return 88200;
	return 96000;
}  

/*
 * Find the next higher frequency the FFG supports */
static int __attribute__((__unused__)) pdplus_find_ffg_rate (int rate)
{
        if (rate < 11050) return 11025;
        if (rate < 22100) return 22050;
        if (rate < 32050) return 32000;
        if (rate < 44150) return 44100;
        if (rate < 48100) return 48000;
        return 96000;
}

/*
 * Check which fix freq code a given frequency has */
static int pdplus_get_fix_freq_code (int rate)
{
        switch (rate) {
        case 0:     return PDPLUS_FPGA_REG_FFG_FREQ_NO_CLK;
        case 11025: return PDPLUS_FPGA_REG_FFG_FREQ_11025;
        case 22050: return PDPLUS_FPGA_REG_FFG_FREQ_22050;
        case 32000: return PDPLUS_FPGA_REG_FFG_FREQ_32000;
        case 44100: return PDPLUS_FPGA_REG_FFG_FREQ_44100;
        case 48000: return PDPLUS_FPGA_REG_FFG_FREQ_48000;
        case 96000: return PDPLUS_FPGA_REG_FFG_FREQ_96000;
        default:    return -1;
        }
        snd_BUG();
        return 0;
}
/*
 * Get the frequency from the FFG code */
static int pdplus_get_ffg_rate_llr (pdplus_t *scard)
{
        switch (PDPLUS_EXTRACT_CACHE (scard, FPGA, FFG, FREQ)) {
        case PDPLUS_FPGA_REG_FFG_FREQ_NO_CLK: return 0;
        case PDPLUS_FPGA_REG_FFG_FREQ_11025:  return 11025;
        case PDPLUS_FPGA_REG_FFG_FREQ_22050:  return 22050;
        case PDPLUS_FPGA_REG_FFG_FREQ_32000:  return 32000;
        case PDPLUS_FPGA_REG_FFG_FREQ_44100:  return 44100;
        case PDPLUS_FPGA_REG_FFG_FREQ_48000:  return 48000;
        case PDPLUS_FPGA_REG_FFG_FREQ_96000:  return 96000;
        }
        snd_BUG();
        return 0;
}

/*
 * Look up the input rate from the registers of the card.  However, for DCO,
 * use the idealised clock.
 *      0 means: no clock
 *     -1 means: unpredictable (e.g. external clock selected)
 * NOTE:
 *     If do_scan != FALSE, this function invokes pdplus_dco_scan_rate_lli which
 *     spin-locks!
 */
static int pdplus_lookup_clock_llr (pdplus_t *scard, int clk_sel)
{
        PDPLUS_LOCAL_VADDR (scard);

        switch (clk_sel) {
        case PDPLUS_CLK_SEL_NO_CLK:
                return 0;

        case PDPLUS_CLK_SEL_D_CAPT:
                return pdplus_d_capt_rate_llr (scard);

        case PDPLUS_CLK_SEL_FFG:
                return pdplus_get_ffg_rate_llr (scard);

        case PDPLUS_CLK_SEL_DCO:
                return scard->dco_target_rate; /* idealised */

        case PDPLUS_CLK_SEL_EXT1:
        case PDPLUS_CLK_SEL_EXT2:
                return -1; /* unpredictable */
        }

        snd_BUG();
        return -1;
}

/*
 * Look up the analog clock rate from the registers of the card. */
inline static 
int pdplus_a_clock_rate_llr (pdplus_t *scard)
{
        return pdplus_lookup_clock_llr (
                scard,
                PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CLK_SEL));
}

/*
 * Look up the digital clock rate from the registers of the card. */
inline static 
int pdplus_d_clock_rate_llr (pdplus_t *scard)
{
        return pdplus_lookup_clock_llr (
                scard,
                PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CLK_SEL));
}


/*
 * Switch back the digital output to normal mode */
inline static 
void pdplus_reset_d_route_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        pdplus_cs8404_prepare_registers_ll (
                scard,
                pdplus_d_clock_rate_llr (scard));

        if (!PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_ROUTE, D_PLAY))
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_ROUTE, D_PLAY);
}


/*
 * Switch back the analog output to normal mode */
inline static 
void pdplus_reset_a_route_ll (pdplus_t *scard)
{
        PDPLUS_LOCAL_VADDR (scard);

        pdplus_adat_mon_stop_ll (scard);

        if (!PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_ROUTE, A_PLAY))
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_ROUTE, A_PLAY);
}              


/*
 * Check that clock on 8404 is good. */
static void pdplus_check_d_clock_ll (pdplus_t *scard)
{
        int rate;
        PDPLUS_LOCAL_VADDR (scard);

        rate = pdplus_d_clock_rate_llr (scard);
        if (rate != pdplus_d_adjust_rate_llr (scard, rate, TRUE)) {
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CLK_SEL) !=
                        PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CLK_SEL))
                {
                        /* This is strange!  How come we are using different clocks and this
                         * one is overclocked? */
                        printk (PDPLUS_KERN_ERR
                                MODULE_NAME
                                "Emergency switch off of CS8404 clock.");
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, NO_CLK);
                        return;
                }

                /* Use other frequency generator */
                if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_CLK_SEL, DCO)) {
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, FFG);
                        PDPLUS_REPLACE_CONST (scard, FPGA, FFG,  FREQ, PDPLUS_DEFAULT_CLOCK);
                }
                else {
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, DCO);
                        pdplus_dco_programme_rate_ll (scard, PDPLUS_DEFAULT_CLOCK);
                }
                pdplus_cs8404_prepare_registers_ll (scard, PDPLUS_DEFAULT_CLOCK);
        }
}


/*
 * Check that clock on 4222 is good. */
static void pdplus_check_a_clock_ll (pdplus_t *scard)
{
        int rate;
        PDPLUS_LOCAL_VADDR (scard);

        rate = pdplus_a_clock_rate_llr (scard);
        if (rate != pdplus_a_adjust_rate (rate)) {
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CLK_SEL) !=
                        PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CLK_SEL))
                {
                        /* This is strange!  How come we are using different clocks and this
                         * one is overclocked? */
                        printk (PDPLUS_KERN_ERR
                                MODULE_NAME
                                "Emergency switch off of CS4222 clock.  "
                                "Registers will not be programmable.");
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, NO_CLK);
                        return;
                }

                /* Use other frequency generator */
                if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_CLK_SEL, DCO)) {
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, FFG);
                        PDPLUS_REPLACE_CONST (scard, FPGA, FFG,  FREQ, PDPLUS_DEFAULT_CLOCK);
                }
                else {
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, DCO);
                        pdplus_dco_programme_rate_ll (scard, PDPLUS_DEFAULT_CLOCK);
                }
        }
}


/*
 * Check that neither the analog nor the digital device are malclocked.
 * D_ROUTE must be safely set to D_PLAY and A_ROUTE to A_PLAY. */
inline static 
void pdplus_check_clocks_ll (pdplus_t *scard)
{
        pdplus_check_d_clock_ll (scard);
        pdplus_check_a_clock_ll (scard);
}

/*
 * Prepare for a change of the analog clock.  This might make it necessary to
 * switch back some other devices on the card.  Currently only the digital
 * monitoring might have to suffer. */
static void pdplus_a_clock_change_prepare_ll (pdplus_t *scard, int new_rate, BOOL dco)
{
        PDPLUS_LOCAL_VADDR (scard);

        /* Switch off adat monitoring if this is going to use the DCO */
        if (dco)
                pdplus_adat_mon_stop_ll (scard);

        /* Switch back digital output if monitoring */
        if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_ROUTE, A_CAPT)
        ||  PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_ROUTE, A_PLAY))
        {
                if (new_rate <= 0 || new_rate != pdplus_a_clock_rate_llr (scard))
                        pdplus_reset_d_route_ll (scard);
        }
}

/*
 * Prepare for a change of the analog clock.  This might make it necessary to
 * switch back some other devices on the card.  Currently only the digital
 * monitoring might have to suffer. */
static void pdplus_d_clock_change_prepare_ll (pdplus_t *scard, int new_rate, BOOL dco)
{
        PDPLUS_LOCAL_VADDR (scard);

        /* Switch off adat monitoring if this is going to use the DCO */
        if (dco)
                pdplus_adat_mon_stop_ll (scard);

        /* Switch back digital output if monitoring */
        if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_ROUTE, D_PLAY)) {
                if (new_rate <= 0 || new_rate != pdplus_d_clock_rate_llr (scard))
                        pdplus_reset_a_route_ll (scard);
        }
}

/*
 * Try to monitor a given device.  Only switch if the device is running with
 * acceptable settings, or it is the default device. */
static int pdplus_d_route_set_ll (pdplus_t *scard, int route)
{
        int rate;
        int result = -EINVAL;
        PDPLUS_LOCAL_VADDR (scard);

        /* Is this a mode switch or just a register adjustment */
        if (route == -1)
                route = scard->d_out_route;
        else
                scard->d_out_route = route;

        switch (route) {
        case PDPLUS_ROUTE_A_CAPT:
                if (scard->mode == PDPLUS_MODE_ADAT)
                        break; /* FIXME:CHECK: is this correct? */

                /* ,,If digital output is analog input, analog output may not
                 * be set to digital play or digital input.''
                 * We are about to set digital output to analog input, so check
                 * the constraints. */
                if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_ROUTE, D_PLAY)
                ||  PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_ROUTE, D_CAPT))
                        break; /* -EINVAL */

                rate = pdplus_a_clock_rate_llr (scard);
                if (rate > 0 && rate == pdplus_d_adjust_rate_llr (scard, rate, TRUE)) {
                        /* ok, acceptable */
                        pdplus_cs8404_prepare_registers_ll (scard, rate);
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_ROUTE, A_CAPT);
                        return 0;
                }
                result = 0;
                break;

        case PDPLUS_ROUTE_A_PLAY:
                if (scard->mode == PDPLUS_MODE_ADAT)
                        break; /* FIXME:CHECK: is this correct? */

                if (scard->a_play_prepared) {
                        rate = pdplus_a_clock_rate_llr (scard);
                        if (rate > 0 && rate == pdplus_d_adjust_rate_llr (scard, rate, TRUE)) {
                                /* ok, acceptable */
                                pdplus_cs8404_prepare_registers_ll (scard, rate);
                                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_ROUTE, A_PLAY);
                                return 0;
                        }
                }
                result = 0;
                break;

        case PDPLUS_ROUTE_D_CAPT:
                /* This always works.  The device accepts the same settings. */
                rate = pdplus_d_capt_rate_llr (scard);

                /* No check of rate.  Always allow monitoring even if the signal
                 * is invalid.  By this the user might be able to see the status
                 * of the digital input immediately. */
                pdplus_cs8404_prepare_registers_ll (scard, rate);
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_ROUTE, D_CAPT);
                return 0;

        case PDPLUS_ROUTE_D_PLAY:
                result = 0;
                break;

        default:
                snd_BUG();
        }

        /* The default and the fall-back is digital playback. */
        pdplus_reset_d_route_ll (scard);

        return result;
}

/*
 * Try to set analog output to digital capture.  This is called from an interrupt,
 * too, because the input clock might change and get invalid, so only one check
 * is not enough.
 * Return values:
 *    1 - success
 *    0 - clock is invalid
 *   <0 - bad setting or combination
 */
static int pdplus_set_a_route_to_d_capt_ll (pdplus_t *scard)
{
        int rate;
        PDPLUS_LOCAL_VADDR (scard);

        /* ,,If digital output is analog input, analog output may not
         * be set to digital play or digital input.  If analog record
         * is active, analog output may only be analog in or analog play.''
         * Check these constraints.   Actually, D_ROUTE_A_CAPT will only
         * be set of scard->a_capt_prepared. */
        if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_ROUTE, A_CAPT)
        || scard->a_capt_prepared)
                LEAVE (-EINVAL);

        if (scard->mode == PDPLUS_MODE_ADAT) {
                /* Another constraint: nothing must be played on analog play
                 * device because it is needed for ADAT monitoring. */
                if (scard->a_play_prepared)
                        LEAVE (-EINVAL);
        }

        rate = pdplus_d_capt_rate_llr (scard);
        if (rate > 0 && rate == pdplus_a_adjust_rate (rate)) { /* ok, acceptable */
                if (scard->mode == PDPLUS_MODE_ADAT)
                        pdplus_adat_mon_start_ll (scard);
                else
                        PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_ROUTE, D_CAPT);
                return 1;
        }

        return 0;
}

/*
 * Output route settings for the analog output */
static int pdplus_a_route_set_ll (pdplus_t *scard, int route)
{
        int rate;
        int result = -EINVAL;
        PDPLUS_LOCAL_VADDR (scard);

        /* Is this a mode switch or just a register adjustment */
        if (route == -1)
                route = scard->a_out_route;
        else
                scard->a_out_route = route;

        switch (route) {
        case PDPLUS_ROUTE_D_CAPT:
                result = pdplus_set_a_route_to_d_capt_ll (scard);
                if (result == 1)
                        return 0;
                break;

        case PDPLUS_ROUTE_D_PLAY:
                if (scard->mode == PDPLUS_MODE_ADAT)
                        break; /* FIXME:CHECK: impossible to monitor to ADAT output */

                if (scard->d_play_prepared) {
                        /* Pretty much the same as above. */
                        if (PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_ROUTE, A_CAPT))
                                break;

                        snd_assert (scard->a_capt_prepared == 0, break);

                        rate = pdplus_d_clock_rate_llr (scard);
                        if (rate > 0 && rate == pdplus_a_adjust_rate (rate)) {
                                /* ok, acceptable */
                                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_ROUTE, D_PLAY);
                                return 0;
                        }
                }
                result = 0;
                break;

        case PDPLUS_ROUTE_A_CAPT:
                /* Always works even without running capturing.  The device accepts
                 * the same settings. */
                pdplus_adat_mon_stop_ll (scard);

                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_ROUTE, A_CAPT);
                return 0;

        case PDPLUS_ROUTE_A_PLAY:
                result = 0;
                break;

        default:
                snd_BUG();
        }

        /* The default and the fall-back is digital playback. */
        pdplus_reset_a_route_ll (scard);

        return result;
}


/*
 * Update both analog and digital routes.  Should be called in every prepare
 * and close function. */
static void pdplus_routes_set_ll (pdplus_t *card)
{
        /* FIXME: Think about the logic and sort out the order.  It is
         *        definitely not too easy. */
        pdplus_a_route_set_ll (card, -1);
        pdplus_d_route_set_ll (card, -1);
        pdplus_a_route_set_ll (card, -1);
        pdplus_d_route_set_ll (card, -1);
}

/* ********************************************************************** */

inline static 
void pdplus_next_ring_ptr (pdplus_ring_ptr_t *ring_ptr)
{
        ring_ptr->playing = (ring_ptr->playing + 1) & ring_ptr->mask;
}

static
BOOL pdplus_is_next_period (
        pdplus_ring_ptr_t *ring_ptr,
        unsigned int hw_ptr)
{
        BOOL result = FALSE;
        if (ring_ptr->mask == 0) { /* only one period. */
                if (hw_ptr < ring_ptr->last_hw_ptr) {
                        result = TRUE;
                }
                ring_ptr->last_hw_ptr = hw_ptr;
        }
        else {
                /* with more than one period generate interrupt
                 * if the period is not equal to the last one. */
                unsigned int cur_pos = hw_ptr >> ring_ptr->shift;

                if (cur_pos != ring_ptr->playing) {
                        pdplus_next_ring_ptr (ring_ptr);
                        result = TRUE;
                }
        }
        return result;
}

static irqreturn_t pdplus_interrupt (
        int irq,
        void *dev_id)
{
        /*
         * Use checking cast, although this might generate thousands of printk
         * errors if something goes wrong.  This is still better than crashing.
         * On my machine some 20000 lines of kernel messages generated by the following
         * line (obviously there was a bug...) had no impact on system usability. */
        u_long transfers_done= 0;
        pdplus_t *scard= dev_id;
        PDPLUS_LOCAL_VADDR (scard);

        /* Did our card generate the interrupt? */
        if (PDPLUS_EXTRACT_HW (scard, PLX, INTCSR, STATUS_1) == 0)
                return IRQ_NONE;

        write_lock(&scard->lock);

        /* Clear interrupt flag */
        pdplus_clear_interrupt_ll (scard);

        /* Increment ticker */
        scard->ticker++;

        /* Check analog output mux if set to digital capture */
        if (scard->a_out_route == PDPLUS_ROUTE_D_CAPT
        && (scard->ticker % 16) == 0) /* Need not be done too often */
        {
                if (pdplus_set_a_route_to_d_capt_ll (scard) != 1)
                        pdplus_reset_a_route_ll (scard);
        }

        /* Check whether an analog play transfer is done */
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_PLAY_ENABLE) != 0
        &&  PDPLUS_EXTRACT_CACHE (scard, FPGA, ADAT, MON_ENABLE) == 0)
        {
                if (pdplus_is_next_period (
                        &scard->a_play_ring_ptr,
                        PDPLUS_EXTRACT_HW (scard, FPGA, CNT, A_PLAY)))
                {
                        transfers_done|= (1 << PDPLUS_ROUTE_A_PLAY);
                }
        }


        /* Check whether an analog capture transfer is done */
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CAPT_ENABLE) != 0)
        {
                if (pdplus_is_next_period (
                        &scard->a_capt_ring_ptr,
                        PDPLUS_EXTRACT_HW (scard, FPGA, CNT, A_CAPT)))
                {
                        transfers_done|= (1 << PDPLUS_ROUTE_A_CAPT);
                }
        }


        /* Check whether a digital play transfer is done */
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_PLAY_ENABLE) != 0)
        {
                if (pdplus_is_next_period (
                        &scard->d_play_ring_ptr,
                        PDPLUS_EXTRACT_HW (scard, FPGA, CNT, D_PLAY)))
                {
                        transfers_done|= (1 << PDPLUS_ROUTE_D_PLAY);
                }
        }

        /* Check whether a digital capture transfer is done */
        if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CAPT_ENABLE) != 0)
        {
                if (pdplus_is_next_period (
                        &scard->d_capt_ring_ptr,
                        PDPLUS_EXTRACT_HW (scard, FPGA, CNT, D_CAPT)))
                {
                        transfers_done|= (1 << PDPLUS_ROUTE_D_CAPT);
                }
        }

        /* Unlock */
        write_unlock (&scard->lock);


        /* Finally, report done transfers to mid-level. */
        if (transfers_done & (1 << PDPLUS_ROUTE_A_PLAY))
                snd_pcm_period_elapsed (scard->a_play);

        if (transfers_done & (1 << PDPLUS_ROUTE_A_CAPT))
                snd_pcm_period_elapsed (scard->a_capt);

        if (transfers_done & (1 << PDPLUS_ROUTE_D_PLAY))
                snd_pcm_period_elapsed (scard->d_play);

        if (transfers_done & (1 << PDPLUS_ROUTE_D_CAPT))
                snd_pcm_period_elapsed (scard->d_capt);

	return IRQ_HANDLED;
}

/* ********************************************************************** */

/*
 * Hardware constraints for period size
 */
static int pdplus_hw_rule_period_size(struct snd_pcm_hw_params *hw,
				      struct snd_pcm_hw_rule *rule)
{
	struct snd_interval *psize, *rate;
	struct snd_interval newi;

	psize = hw_param_interval(hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
	rate = hw_param_interval(hw, SNDRV_PCM_HW_PARAM_RATE);
	snd_interval_any(&newi);

	newi.min = pdplus_fit_period_size (psize->min, is_adat, rate->min);
	newi.max = pdplus_fit_period_size (psize->max, is_adat, rate->max);

	return snd_interval_refine(psize, &newi);
}


/*
 * hw constraint for rate on the digital playback
 */
static int pdplus_d_play_hw_rule_rate(struct snd_pcm_hw_params *hw,
				      struct snd_pcm_hw_rule *rule)
{
#if IEC958_ALLOW_HIGH_FREQ
	int const profi = 1;
#else
	pdplus_t *scard = rule->private;
	int profi = (scard->profi_mode || scard->auto_profi_mode);
	if (!always_profi) {
		u_char *cs = pdplus_get_channel_status (scard);
		if (cs != NULL)
			profi = cs[0] & IEC958_AES0_PROFESSIONAL;
	}
#endif
	return snd_interval_list(hw_param_interval(hw, SNDRV_PCM_HW_PARAM_RATE),
				 profi ? 5 : 3, d_rate_llr, 0);
}

/*
 * hw constraint for rate on the digital capture
 */
static int pdplus_d_capt_hw_rule_rate(struct snd_pcm_hw_params *hw,
				      struct snd_pcm_hw_rule *rule)
{
	pdplus_t *scard = rule->private;
	int rate;
	unsigned long flags;
	struct snd_interval newi;

	/* Force synchronised rate */
	read_lock_irqsave (&scard->lock, flags);
	rate = pdplus_d_capt_rate_llr (scard);
	read_unlock_irqrestore (&scard->lock, flags);
	if (rate <= 0)
		return 0; /* FIXME: should return the error? */
	snd_interval_any(&newi);
	newi.min = newi.max = rate;

	return snd_interval_refine(hw_param_interval(hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE), &newi);
}


/*
 * Get the buffer pointer for analog playback */
static snd_pcm_uframes_t pdplus_a_play_pointer (struct snd_pcm_substream *substream)
{
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);

        /* Note that the counter registers are very coarse (9 bits are missing). */
        return PDPLUS_FRAMES_FROM_HWPTR (PDPLUS_EXTRACT_HW (scard, FPGA, CNT, A_PLAY), is_adat);
}

/*
 * Get the buffer pointer for analog capture */
static snd_pcm_uframes_t pdplus_a_capt_pointer (struct snd_pcm_substream *substream)
{
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);

        return PDPLUS_FRAMES_FROM_HWPTR (PDPLUS_EXTRACT_HW (scard, FPGA, CNT, A_CAPT), is_adat);
}

/*
 * Get buffer pointer for digital playback. */
static snd_pcm_uframes_t pdplus_d_play_pointer (struct snd_pcm_substream *substream)
{
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);

        return PDPLUS_FRAMES_FROM_HWPTR (PDPLUS_EXTRACT_HW (scard, FPGA, CNT, D_PLAY), is_adat);
}

/*
 * Get buffer pointer for digital capture. */
static snd_pcm_uframes_t pdplus_d_capt_pointer (struct snd_pcm_substream *substream)
{
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);

        return PDPLUS_FRAMES_FROM_HWPTR (PDPLUS_EXTRACT_HW (scard, FPGA, CNT, D_CAPT), is_adat);
}


/*
 * Trigger playback of analog playback device. */
static int pdplus_a_play_trigger (struct snd_pcm_substream *substream, int cmd)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
                /* FIXME: implement sync */
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_PLAY_ENABLE)) {
                        if (substream != scard->a_play)
                                LEAVE (-EBUSY);

                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_PLAY_CNT_CLEAR, 0);
                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_PLAY_ENABLE,    1);
                }
                break;

        case SNDRV_PCM_TRIGGER_STOP:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_PLAY_ENABLE)) {
                        if (substream != scard->a_play)
                                LEAVE (-EBUSY);

                        pdplus_a_play_stop_ll (scard);
                }
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_PLAY_ENABLE))
                        pdplus_a_play_stop_ll (scard);
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_PLAY_ENABLE))
                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_PLAY_ENABLE, 1);
                break;

        default:
                LEAVE (-EINVAL);
        }

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Trigger playback of analog capture device. */
static int pdplus_a_capt_trigger (struct snd_pcm_substream *substream, int cmd)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CAPT_ENABLE)) {
                        if (substream != scard->a_capt)
                                LEAVE (-EBUSY);

                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_CAPT_CNT_CLEAR, 0);
                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_CAPT_ENABLE,    1);
                }
                break;

        case SNDRV_PCM_TRIGGER_STOP:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CAPT_ENABLE)) {
                        if (substream != scard->a_capt)
                                LEAVE (-EBUSY);

                        pdplus_a_capt_stop_ll (scard);
                }
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CAPT_ENABLE))
                        pdplus_a_capt_stop_ll (scard);
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, A_CAPT_ENABLE))
                        PDPLUS_REPLACE (scard, FPGA, CTRL, A_CAPT_ENABLE, 1);
                break;

        default:
                LEAVE (-EINVAL);
        }

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Trigger playback of digital device. */
static int pdplus_d_play_trigger (struct snd_pcm_substream *substream, int cmd)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
                /* FIXME: implement sync */
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_PLAY_ENABLE)) {
                        if (substream != scard->d_play)
                                LEAVE (-EBUSY);

                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_PLAY_CNT_CLEAR, 0);
                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_PLAY_ENABLE,    1);
                }
                break;

        case SNDRV_PCM_TRIGGER_STOP:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_PLAY_ENABLE)) {
                        if (substream != scard->d_play)
                                LEAVE (-EBUSY);

                        pdplus_d_play_stop_ll (scard);
                }
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_PLAY_ENABLE))
                        pdplus_d_play_stop_ll (scard);
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_PLAY_ENABLE))
                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_PLAY_ENABLE, 1);
                break;

        default:
                LEAVE (-EINVAL);
        }

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Trigger playback of digital capture device. */
static int pdplus_d_capt_trigger (struct snd_pcm_substream *substream, int cmd)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
                /* FIXME: implement sync */
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CAPT_ENABLE)) {
                        if (substream != scard->d_capt)
                                LEAVE (-EBUSY);

                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_CAPT_CNT_CLEAR, 0);
                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_CAPT_ENABLE,    1);
                }
                break;

        case SNDRV_PCM_TRIGGER_STOP:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CAPT_ENABLE)) {
                        if (substream != scard->d_capt)
                                LEAVE (-EBUSY);

                        pdplus_d_capt_stop_ll (scard);
                }
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
                if (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CAPT_ENABLE))
                        pdplus_d_capt_stop_ll (scard);
                break;

        case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
                unless (PDPLUS_EXTRACT_CACHE (scard, FPGA, CTRL, D_CAPT_ENABLE))
                        PDPLUS_REPLACE (scard, FPGA, CTRL, D_CAPT_ENABLE, 1);
                break;

        default:
                LEAVE (-EINVAL);
        }

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Find a free clock generator for a given device.  The two devices, digital and
 * analog, share two clock generators.  So a_play and a_capt can only go at
 * the same rate.  These constraints must be handled outside this code.  This
 * function searches for a free clock generator assuming that the one that
 * possibly runs on the given device is not suitable.  The output is < 0 for
 * an error, 0 if ok.  The rate will be programmed, and the device clock mux
 * will be switched. */
static int pdplus_analog_find_and_programme_clock_ll (
        pdplus_t *scard,
        struct snd_pcm_substream *substream,
        int rate)
{
        int  fixclock;
        BOOL d_prepared = FALSE;
        int  d_rate = -1;
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        /* Get the fixed frequency code of that rate. */
        fixclock = pdplus_get_fix_freq_code (rate);

        /* Is digital playback prepared?  Get the rate they are using. */
        if (scard->d_play_prepared) {
                snd_assert (scard != NULL, return -ENXIO);
                snd_assert (scard->d_play != NULL, return -ENXIO);
                snd_assert (scard->d_play->runtime != NULL, return -ENXIO);

                d_prepared = TRUE;
                d_rate = scard->d_play->runtime->rate;
        }

        /* Do not try to use digital capture clock because that rate might
         * change. */

        /* Can FFG be used? (<= either unused, or same rate) */
        if (fixclock >= 0
                && (!d_prepared
                        || !PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_CLK_SEL, FFG)
                        || d_rate == rate))
        {
                /* Notify others of following clock change */
                pdplus_a_clock_change_prepare_ll (scard, rate, FALSE);

                /*
                 * FFG is usable (do not programme FFG if frequency is matching.  Maybe
                 * this generates disturbances.  It is safe to check in any case. */
                if ((int)PDPLUS_EXTRACT_CACHE (scard, FPGA, FFG, FREQ) != fixclock)
                        PDPLUS_REPLACE (scard, FPGA, FFG, FREQ, fixclock);

                /* Set FFG clock for analog device */
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, FFG);

                /* Final checks for overclocking... */
                pdplus_check_clocks_ll (scard);

                return 0;
        }
        else
        if (!d_prepared
                || !PDPLUS_CACHE_EQL (scard, FPGA, CTRL, D_CLK_SEL, DCO)
                || d_rate == rate)
        {
                /* Notify others of following clock change */
                pdplus_a_clock_change_prepare_ll (scard, rate, TRUE);

                /* Possibly programme DCO rate */
                if (scard->dco_target_rate != rate) {
                        pdplus_dco_programme_rate_ll (scard, rate);
                        pdplus_dco_wait_stabilise_ll (scard);
                        /* substream->runtime->rate = scard->dco_target_rate; redundant. */
                }

                /* Set DCO clock for analog device */
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, A_CLK_SEL, DCO);

                /* Final checks for overclocking... */
                pdplus_check_clocks_ll (scard);

                return 0;
        }
        else
                LEAVE (-EBUSY);

        snd_BUG();
        LEAVE (-ENXIO);
}

/*
 * The same as pdplus_analog_... for digital devices. */
static int pdplus_digital_find_and_programme_clock_ll (
        pdplus_t *scard,
        struct snd_pcm_substream *substream,
        int rate)
{
        int  fixclock;
        BOOL a_prepared = FALSE;
        int  a_rate = -1;
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        /* Get the fixed frequency code of that rate. */
        fixclock = pdplus_get_fix_freq_code (rate);

        /* Is analog playback or capture prepared?  Get the rate they are using. */
        if (scard->a_play_prepared) {
                snd_assert (scard != NULL, return -ENXIO);
                snd_assert (scard->a_play != NULL, return -ENXIO);
                snd_assert (scard->a_play->runtime != NULL, return -ENXIO);

                a_prepared = TRUE;
                a_rate = scard->a_play->runtime->rate;
        }

        if (scard->a_capt_prepared) {
                snd_assert (scard != NULL, return -ENXIO);
                snd_assert (scard->a_capt != NULL, return -ENXIO);
                snd_assert (scard->a_capt->runtime != NULL, return -ENXIO);
                snd_assert (a_rate == -1 || a_rate == (int)scard->a_capt->runtime->rate,
			    return -ENXIO);

                a_prepared = TRUE;
                a_rate = scard->a_capt->runtime->rate;
        }

        /* Can FFG be used? (<= either unused, or same rate) */
        if (fixclock >= 0
                && (!a_prepared
                        || !PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_CLK_SEL, FFG)
                        || a_rate == rate))
        {
                /* Notify others of following clock change */
                pdplus_d_clock_change_prepare_ll (scard, rate, FALSE);

                /*
                 * FFG is usable (do not programme FFG if frequency is matching.  Maybe
                 * this generates disturbances.  It is safe to check in any case. */
                if ((int)PDPLUS_EXTRACT_CACHE (scard, FPGA, FFG, FREQ) != fixclock)
                        PDPLUS_REPLACE (scard, FPGA, FFG, FREQ, fixclock);

                /* Set FFG clock for digital device */
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, FFG);

                /* Final checks for overclocking... */
                pdplus_check_clocks_ll (scard);

                return 0;
        }
        else
        if (!a_prepared
                || !PDPLUS_CACHE_EQL (scard, FPGA, CTRL, A_CLK_SEL, DCO)
                || scard->dco_target_rate == rate)
        {
                /* Notify others of following clock change */
                pdplus_d_clock_change_prepare_ll (scard, rate, TRUE);

                /* Possibly programme DCO rate */
                if (scard->dco_target_rate != rate) {
                        pdplus_dco_programme_rate_ll (scard, rate);
                        pdplus_dco_wait_stabilise_ll (scard);
                        /* substream->runtime->rate = scard->dco_target_rate; redundant. */
                }

                /* Set DCO clock for digital device */
                PDPLUS_REPLACE_CONST (scard, FPGA, CTRL, D_CLK_SEL, DCO);

                /* Final checks for overclocking... */
                pdplus_check_clocks_ll (scard);

                return 0;
        }
        else
                LEAVE (-EBUSY);

        snd_BUG();
        LEAVE (-ENXIO);
}

/*
 * Get the shift count for the hw_pointer window defined by the period size
 * period_size is in frames.
 */
static void pdplus_init_ring_ptr (
        pdplus_ring_ptr_t *ring_ptr,
        u_int period_size,
        BOOL  is_adat)
{
        u_int period_size_bytes = PDPLUS_BYTES_FROM_FRAMES (period_size, is_adat);

        ring_ptr->playing = 0;
        ring_ptr->shift = 0;
        ring_ptr->last_hw_ptr = 0;
        while (BIT_VAL (PDPLUS_COUNTER_SHIFT + ring_ptr->shift) != period_size_bytes) {
                ring_ptr->shift++;
                if (BIT_VAL (PDPLUS_COUNTER_SHIFT + ring_ptr->shift) > PDPLUS_BUFFER_SIZE) {
                        printk (PDPLUS_KERN_ERR
                                "counter shift out of range.  period_size_bytes=%u\n",
                                period_size_bytes);
                        ring_ptr->shift = 0;
                        break;
                }
        }

        ring_ptr->mask = (PDPLUS_BUFFER_SIZE / period_size_bytes)-1;

        Vprintk ("ring_ptr: shift=%d, mask=%u, period_size=%u\n",
                ring_ptr->shift,
                ring_ptr->mask,
                period_size_bytes);
}

/*
 * Prepare analog play device by configuring the device settings. */
static int pdplus_a_play_prepare (struct snd_pcm_substream *substream)
{
        int err = -ENXIO; /* default error */
        u_long flags;
        int rate;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);
        ENTER;

        /* Get & check the sample rate */
        rate = substream->runtime->rate;
        snd_assert (rate == pdplus_a_adjust_rate (rate), return -ENXIO);

        /* Lock */
        write_lock_irqsave (&scard->lock, flags);

        /* Stop the card if it is playing. */
        pdplus_a_play_stop_ll (scard);

        /* Reset everything */
        PDPLUS_REPLACE (scard, FPGA, CTRL, A_PLAY_CNT_CLEAR, 1);
        pdplus_init_ring_ptr (&scard->a_play_ring_ptr, substream->runtime->period_size, is_adat);

        /*
         * Select a frequency source (either FFG or DCO) and set the target rate
         * Possibly, there is no free clock source.  In that case, return EBUSY.
         * Even if the dco has the correct frequency, we will probably want to
         * use the FFG whenever possible, because the DCO is a precious device
         * which might be needed for something else later. */
        if (scard->a_capt_prepared) {
                /* We have to use the same clock as analog rec */
                snd_assert (scard != NULL, return -ENXIO);
                snd_assert (scard->a_capt != NULL, return -ENXIO);
                snd_assert (scard->a_capt->runtime != NULL, return -ENXIO);

                if ((int)scard->a_capt->runtime->rate != rate) {
                        err = -EBUSY;
                        goto raus;
                }
        }
        else    /* Try to use FFG if possible */
        if ((err = pdplus_analog_find_and_programme_clock_ll (scard, substream, rate)) < 0)
                goto raus;

        /* Ok, everything is prepared now (this is read by the route_set_ll functions) */
        scard->a_play_prepared = TRUE;

        /* Set up routes correctly. */
        pdplus_routes_set_ll (scard);

        /* No error. */
        err = 0;

raus:
        /* unlock and leave */
        write_unlock_irqrestore (&scard->lock, flags);
        LEAVE (err);
}


/*
 * Prepare analog capture device by configuring the device settings. */
static int pdplus_a_capt_prepare (struct snd_pcm_substream *substream)
{
        int err = -ENXIO; /* default error */
        u_long flags;
        int rate;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);
        ENTER;

        /* Get & check the sample rate if necessary */
        rate = substream->runtime->rate;
        snd_assert (rate == pdplus_a_adjust_rate (rate), return -ENXIO);

        /* Lock */
        write_lock_irqsave (&scard->lock, flags);

        /* Stop the card if it is playing. */
        pdplus_a_capt_stop_ll (scard);

        /* Reset everything */
        PDPLUS_REPLACE (scard, FPGA, CTRL, A_CAPT_CNT_CLEAR, 1);
        pdplus_init_ring_ptr (&scard->a_capt_ring_ptr, substream->runtime->period_size, is_adat);

        /*
         * Select a frequency source (either FFG or DCO) and set the target rate
         * Possibly, there is no free clock source.  In that case, return EBUSY.
         * Even if the dco has the correct frequency, we will probably want to
         * use the FFG whenever possible, because the DCO is a precious device
         * which might be needed for something else later. */
        if (scard->a_play_prepared) {
                /* We have to use the same clock as analog play */
                snd_assert (scard != NULL, return -ENXIO);
                snd_assert (scard->a_play != NULL, return -ENXIO);
                snd_assert (scard->a_play->runtime != NULL, return -ENXIO);

                if ((int)scard->a_play->runtime->rate != rate) {
                        err = -EBUSY;
                        goto raus;
                }
        }
        else    /* Try to use FFG if possible */
        if ((err = pdplus_analog_find_and_programme_clock_ll (scard, substream, rate)) < 0)
                goto raus;

        /* Ok, everything is prepared now (this is read by the route_set_ll functions) */
        scard->a_capt_prepared = TRUE;

        /* Set up routes correctly (especially, disable digital monitoring). */
        pdplus_routes_set_ll (scard);

        err = 0;

raus:
        /* unlock and leave */
        write_unlock_irqrestore (&scard->lock, flags);
        LEAVE (err);
}


/*
 * Prepare digital play device by configuring the device settings.
 * This is quite similar to pdplus_a_play_prepare. */
static int pdplus_d_play_prepare (struct snd_pcm_substream *substream)
{
        int err = -ENXIO; /* default error */
        u_long flags;
        int rate;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        /* Lock */
        write_lock_irqsave (&scard->lock, flags);

        /* Check the sample rate if necessary */
        rate = substream->runtime->rate;
        snd_assert (rate == pdplus_d_adjust_rate_llr (scard, rate, FALSE), return -ENXIO);

        /* Stop the card if it is playing (for whatever reason). */
        pdplus_d_play_stop_ll (scard);

        /* Reset everything */
        PDPLUS_REPLACE (scard, FPGA, CTRL, D_PLAY_CNT_CLEAR, 1);
        pdplus_init_ring_ptr (&scard->d_play_ring_ptr, substream->runtime->period_size, is_adat);

        /*
         * Select a frequency source (either FFG or DCO) and set the target rate
         * Possibly, there is no free clock source.  In that case, return EBUSY. */
        if ((err = pdplus_digital_find_and_programme_clock_ll (scard, substream, rate)) < 0)
                goto raus;

        /* Ok, everything is prepared now (this is read by the route_set_ll functions) */
        scard->d_play_prepared = TRUE;

        /* Set up routes correctly. */
        pdplus_routes_set_ll (scard); /* Also does the cs8404 set-up */

        err = 0;

raus:
        /* unlock and leave */
        write_unlock_irqrestore (&scard->lock, flags);
        LEAVE (err);
}


/*
 * Prepare digital capture device by configuring the device settings. */
static int pdplus_d_capt_prepare (struct snd_pcm_substream *substream)
{
        u_long flags;
        int rate;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        /* Lock */
        write_lock_irqsave (&scard->lock, flags);

        /* Get the sample rate from the inpuAdjust the sample rate if necessary */
        rate = substream->runtime->rate;

        /* Stop the card if it is playing. */
        pdplus_d_capt_stop_ll (scard);

        /* Reset everything */
        PDPLUS_REPLACE (scard, FPGA, CTRL, D_CAPT_CNT_CLEAR, 1);
        pdplus_init_ring_ptr (&scard->d_capt_ring_ptr, substream->runtime->period_size, is_adat);

        /* Ok, everything is prepared now (this is read by the route_set_ll functions) */
        scard->d_capt_prepared = TRUE;

        /* Set up routes correctly. */
        pdplus_routes_set_ll (scard);

        /* unlock and leave */
        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/* ********************************************************************** */

static struct snd_pcm_hardware pdplus_a_play_info =
{
        .info =
                SNDRV_PCM_INFO_INTERLEAVED |
                SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,

        .formats =
                SNDRV_PCM_FMTBIT_S24_LE,

        .rates =  /* DCO Frequency Generator: */
                SNDRV_PCM_RATE_CONTINUOUS |
                /* Fixed Frequency Generator: */
                SNDRV_PCM_RATE_48000 |
                SNDRV_PCM_RATE_44100 |
                SNDRV_PCM_RATE_32000 |
                SNDRV_PCM_RATE_22050 |
                SNDRV_PCM_RATE_11025,

        .rate_min =   11025,  /* min: FFG only, DCO starts at 29000, ADC/DAC could do 4kHz */
        .rate_max =   50000,  /* max: 50kHz for ADC/DAC according to CS4222 specification. */
	
        .channels_min = 2,
        .channels_max = 2,

	.buffer_bytes_max =   PDPLUS_BUFFER_SIZE,

        .period_bytes_min =   PDPLUS_SMALL_BLOCK, /* this depends on the sampling rate. */
        .period_bytes_max =   PDPLUS_LARGE_BLOCK,

	.periods_min =	1,
	.periods_max =	1024,
        .fifo_size =      0,
};

/* ********************************************************************** */

static struct snd_pcm_hardware pdplus_a_capt_info =
{
        .info =
                SNDRV_PCM_INFO_INTERLEAVED |
                SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,

        .formats =
                SNDRV_PCM_FMTBIT_S24_LE,

        .rates =  /* DCO Frequency Generator: */
                SNDRV_PCM_RATE_CONTINUOUS |
                /* Fixed Frequency Generator: */
                SNDRV_PCM_RATE_48000 |
                SNDRV_PCM_RATE_44100 |
                SNDRV_PCM_RATE_32000 |
                SNDRV_PCM_RATE_22050 |
                SNDRV_PCM_RATE_11025,

        .rate_min =   11025,  /* min: FFG only, DCO starts at 29000, ADC/DAC could do 4kHz */
        .rate_max =   50000,  /* max: 50kHz for ADC/DAC according to CS4222 specification. */
	
        .channels_min = 2,
        .channels_max = 2,

	.buffer_bytes_max =   PDPLUS_BUFFER_SIZE,

        .period_bytes_min =   PDPLUS_SMALL_BLOCK, /* this depends on the sampling rate. */
        .period_bytes_max =   PDPLUS_LARGE_BLOCK,

	.periods_min =	1,
	.periods_max =	1024,
        .fifo_size =      0,
};

/* ********************************************************************** */

static struct snd_pcm_hardware pdplus_d_play_info =
{
        .info =
                SNDRV_PCM_INFO_INTERLEAVED |
                SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,

        .formats =
                SNDRV_PCM_FMTBIT_S24_LE,

        .rates =  /* Fixed Frequency Generator: */
                SNDRV_PCM_RATE_96000 |
                SNDRV_PCM_RATE_88200 |
                SNDRV_PCM_RATE_48000 |
                SNDRV_PCM_RATE_44100 |
                SNDRV_PCM_RATE_32000,

        .rate_min =   32000,
        .rate_max =   96000,

        .channels_min = 2,
        .channels_max = 2,

	.buffer_bytes_max =   PDPLUS_BUFFER_SIZE,

        .period_bytes_min =   PDPLUS_SMALL_BLOCK, /* this depends on the sampling rate. */
        .period_bytes_max =   PDPLUS_LARGE_BLOCK,

	.periods_min =	1,
	.periods_max =	1024,
        .fifo_size =      0,
};

/* ********************************************************************** */

static struct snd_pcm_hardware pdplus_d_capt_info =
{
        .info =
                SNDRV_PCM_INFO_INTERLEAVED |
                SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,

        .formats =
                SNDRV_PCM_FMTBIT_S24_LE,

        .rates =
                /* This can synchronise to those frequencies (this cannot be
                 * set, though): */
                SNDRV_PCM_RATE_96000 |
                SNDRV_PCM_RATE_88200 |
                SNDRV_PCM_RATE_48000 |
                SNDRV_PCM_RATE_44100 |
                SNDRV_PCM_RATE_32000,

        .rate_min =   32000,
        .rate_max =   96000,

        .channels_min = 2,
        .channels_max = 2,

	.buffer_bytes_max =   PDPLUS_BUFFER_SIZE,

        .period_bytes_min =   PDPLUS_SMALL_BLOCK, /* this depends on the sampling rate. */
        .period_bytes_max =   PDPLUS_LARGE_BLOCK,

	.periods_min =	1,
	.periods_max =	1024,
        .fifo_size =      0,
};

/* ********************************************************************** */

/*
 * Open the analog play device */
static int pdplus_a_play_open (struct snd_pcm_substream *substream)
{
        u_long flags;
	int err;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        ENTER;
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        substream->runtime->hw = pdplus_a_play_info;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				  pdplus_hw_rule_period_size, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

        write_lock_irqsave (&scard->lock, flags);

        scard->a_play = substream;

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}


/*
 * Close the analog play device */
static int pdplus_a_play_close (struct snd_pcm_substream *substream)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        scard->a_play = NULL;
        scard->a_play_prepared = FALSE;

        pdplus_a_play_stop_ll (scard);

        pdplus_routes_set_ll (scard);

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Open the analog capture device */
static int pdplus_a_capt_open (struct snd_pcm_substream *substream)
{
        u_long flags;
	int err;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        ENTER;
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        substream->runtime->hw = pdplus_a_capt_info;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				  pdplus_hw_rule_period_size, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

        write_lock_irqsave (&scard->lock, flags);

        scard->a_capt = substream;

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}


/*
 * Close the analog capture device */
static int pdplus_a_capt_close (struct snd_pcm_substream *substream)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        scard->a_capt = NULL;
        scard->a_capt_prepared = FALSE;

        pdplus_a_capt_stop_ll (scard);

        pdplus_routes_set_ll (scard);

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Possibly perform mode switch (upload new circuit, init, etc.) */
static int pdplus_set_mode (pdplus_t *scard, int mode)
{
        PDPLUS_LOCAL_VADDR (scard);

        if (scard->mode == mode)
                return 0;

        if (scard->a_play_prepared
        ||  scard->a_capt_prepared
        ||  scard->d_play_prepared
        ||  scard->d_capt_prepared)
                LEAVE (-EBUSY);

        unless (pdplus_switch_circuit (scard, mode))
                LEAVE (-EIO);

        return 0;
}

/*
 * Functions for digital playback */
static int pdplus_d_play_open (struct snd_pcm_substream *substream)
{
        u_long flags;
        int err;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        ENTER;
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        if ((err = pdplus_set_mode (scard, PDPLUS_MODE_DIGITAL)))
                LEAVE (err);

	substream->runtime->hw = pdplus_d_play_info;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_RATE,
				  pdplus_d_play_hw_rule_rate, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				  pdplus_hw_rule_period_size, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

        write_lock_irqsave (&scard->lock, flags);

        scard->d_play = substream;

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

static int pdplus_d_play_close (struct snd_pcm_substream *substream)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        scard->d_play = NULL;
        scard->d_play_prepared = FALSE;

        pdplus_d_play_stop_ll (scard);
        pdplus_routes_set_ll (scard);

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/*
 * Functions for digital capture */
static int pdplus_d_capt_open (struct snd_pcm_substream *substream)
{
        u_long flags;
        int err;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        ENTER;
        snd_assert (substream != NULL, return -ENXIO);
        snd_assert (substream->runtime != NULL, return -ENXIO);

        if ((err = pdplus_set_mode (scard, PDPLUS_MODE_DIGITAL)))
                LEAVE (err);

        substream->runtime->hw = pdplus_d_capt_info;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_RATE,
				  pdplus_d_capt_hw_rule_rate, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_rule_add(substream->runtime, 0,
				  SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				  pdplus_hw_rule_period_size, scard,
				  SNDRV_PCM_HW_PARAM_RATE, -1);
	if (err < 0)
		return err;
	err = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

        write_lock_irqsave (&scard->lock, flags);

        scard->d_capt = substream;

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

static int pdplus_d_capt_close (struct snd_pcm_substream *substream)
{
        u_long flags;
        pdplus_t *scard = snd_pcm_substream_chip(substream);
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        write_lock_irqsave (&scard->lock, flags);

        scard->d_capt = NULL;
        scard->d_capt_prepared = FALSE;

        pdplus_d_capt_stop_ll (scard);
        pdplus_routes_set_ll (scard);

        write_unlock_irqrestore (&scard->lock, flags);

        LEAVE (0);
}

/* ********************************************************************** */

static struct snd_pcm_ops pdplus_a_play_ops = {
	.open =	   pdplus_a_play_open,
	.close =   pdplus_a_play_close,
        .ioctl =   snd_pcm_lib_ioctl,
        .prepare = pdplus_a_play_prepare,
        .trigger = pdplus_a_play_trigger,
        .pointer = pdplus_a_play_pointer,
	.copy =	   pdplus_a_play_copy_ll,
	.silence = pdplus_a_play_silence_ll,
};

static struct snd_pcm_ops pdplus_a_capt_ops = {
	.open =	   pdplus_a_capt_open,
	.close =   pdplus_a_capt_close,
        .ioctl =   snd_pcm_lib_ioctl,
        .prepare = pdplus_a_capt_prepare,
        .trigger = pdplus_a_capt_trigger,
        .pointer = pdplus_a_capt_pointer,
	.copy =    pdplus_a_capt_copy_ll,
};

static struct snd_pcm_ops pdplus_d_play_ops = {
	.open =    pdplus_d_play_open,
	.close =   pdplus_d_play_close,
        .ioctl =   snd_pcm_lib_ioctl,
        .prepare = pdplus_d_play_prepare,
        .trigger = pdplus_d_play_trigger,
        .pointer = pdplus_d_play_pointer,
	.copy =    pdplus_d_play_copy_ll,
	.silence = pdplus_d_play_silence_ll,
};

static struct snd_pcm_ops pdplus_d_capt_ops = {
	.open =    pdplus_d_capt_open,
	.close =   pdplus_d_capt_close,
        .ioctl =   snd_pcm_lib_ioctl,
        .prepare = pdplus_d_capt_prepare,
        .trigger = pdplus_d_capt_trigger,
        .pointer = pdplus_d_capt_pointer,
	.copy =    pdplus_d_capt_copy_ll,
};

/* ********************************************************************** */

static void pdplus_a_pcm_free (struct snd_pcm *pcm)
{
        pdplus_t *scard = pcm->private_data;
        scard->analog = NULL;
}

static void pdplus_d_pcm_free (struct snd_pcm *pcm)
{
        pdplus_t *scard = pcm->private_data;
        scard->digital = NULL;
}

/* ********************************************************************** */

static __devinit int pdplus_a_pcm_new (
        struct snd_card *card,
        pdplus_t *scard,
        int device)
{
        struct snd_pcm *pcm;
        int err;
        ENTER;

        if ((err = snd_pcm_new (card, "CS4222", device, 1, 1, &pcm)) < 0)
                LEAVE (err);

        snd_assert (pcm != NULL, return -ENOMEM);

        pcm->private_data = scard;
        pcm->private_free = pdplus_a_pcm_free;

        pcm->info_flags = SNDRV_PCM_INFO_JOINT_DUPLEX;

        snd_assert (pcm->name != NULL, return -ENXIO);
        strcpy (pcm->name, "CS4222 DAC/ADC");

        snd_assert (pcm->streams != NULL, return -ENXIO);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &pdplus_a_play_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &pdplus_a_capt_ops);

        scard->analog = pcm;
        scard->a_play_prepared = FALSE;
        scard->a_capt_prepared = FALSE;

        LEAVE (0);
}

static __devinit int pdplus_d_pcm_new (
        struct snd_card *card,
        pdplus_t *scard,
        int device)
{
        struct snd_pcm *pcm;
        int err;
        ENTER;

        if ((err = snd_pcm_new (card, "CS84x4", device, 1, 1, &pcm)) < 0)
                LEAVE (err);
        snd_assert (pcm != NULL, return -ENOMEM);

        pcm->private_data = scard;
        pcm->private_free = pdplus_d_pcm_free;

        pcm->info_flags = 0;

        snd_assert (pcm->name != NULL, return -ENXIO);
        strcpy (pcm->name, "CS8404/CS8414 Digital Transceiver");

        snd_assert (pcm->streams != NULL, return -ENXIO);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &pdplus_d_play_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &pdplus_d_capt_ops);

        scard->digital = pcm;
        scard->d_play_prepared = 0;
        scard->d_capt_prepared = 0;

        LEAVE (0);
}

/* ********************************************************************** */
/* Mixer functions */

inline static int pdplus_adda_get_volume_left_llr (pdplus_t *scard)
{
        return PDPLUS_CS4222_ATTEN_MAX - PDPLUS_READ_CACHE (scard, CS4222, ATT_LEFT);
}

inline static int pdplus_adda_get_volume_right_llr (pdplus_t *scard)
{
        return PDPLUS_CS4222_ATTEN_MAX - PDPLUS_READ_CACHE (scard, CS4222, ATT_RIGHT);
}

inline static void pdplus_adda_set_volume_left_ll (pdplus_t *scard, int value)
{
        PDPLUS_WRITE (scard, CS4222, ATT_LEFT, PDPLUS_CS4222_ATTEN_MAX - value);
}

inline static void pdplus_adda_set_volume_right_ll (pdplus_t *scard, int value)
{
        PDPLUS_WRITE (scard, CS4222, ATT_RIGHT, PDPLUS_CS4222_ATTEN_MAX - value);
}

/* ********************************************************************** */

/*
 * For OSS Mixer compatibility, this is called `PCM Playback ....'
 * although `Analog Playback...' or `DAC Playback' would be more
 * appropriate (the digital output can output PCM samples, too...)
 */
#define PDPLUS_CONTROL_DAC_VOLUME               \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "PCM Playback Volume",   \
                .index = 0,                       \
                .info =  pdplus_dac_volume_info,  \
                .get =   pdplus_dac_volume_get,   \
                .put =   pdplus_dac_volume_set    \
        }

static int pdplus_dac_volume_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *uinfo)
{
        uinfo->type=   SNDRV_CTL_ELEM_TYPE_INTEGER;
        uinfo->count= 2;
        uinfo->value.integer.min= 0;
        uinfo->value.integer.max= PDPLUS_CS4222_ATTEN_MAX;
        return 0;
}

static int pdplus_dac_volume_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= pdplus_adda_get_volume_left_llr  (scard);
        u->value.integer.value[1]= pdplus_adda_get_volume_right_llr (scard);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_dac_volume_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int change= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        write_lock_irqsave (&scard->lock, flags);

        if (u->value.integer.value[0] != pdplus_adda_get_volume_left_llr (scard)) {
                pdplus_adda_set_volume_left_ll (scard, u->value.integer.value[0]);
                change= 1;
        }

        if (u->value.integer.value[1] != pdplus_adda_get_volume_right_llr (scard)) {
                pdplus_adda_set_volume_right_ll (scard, u->value.integer.value[1]);
                change= 1;
        }

        write_unlock_irqrestore (&scard->lock, flags);

        return change;
}

#define PDPLUS_CONTROL_DAC_SWITCH               \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "PCM Playback Switch",   \
                .index = 0,                       \
                .info =  pdplus_boolean2_info,    \
                .get =   pdplus_dac_switch_get,   \
                .put =   pdplus_dac_switch_set    \
        }

static int pdplus_dac_switch_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]=
                !!PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, MUTE_LEFT);
        u->value.integer.value[1]=
                !!PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, MUTE_RIGHT);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_dac_switch_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        write_lock_irqsave (&scard->lock, flags);
        if (PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, MUTE_LEFT) !=
                !!u->value.integer.value[0] ||
            PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, MUTE_RIGHT) !=
                !!u->value.integer.value[1])
        {
                PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, MUTE_LEFT,
                        !!u->value.integer.value[0]);
                PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, MUTE_RIGHT,
                        !!u->value.integer.value[1]);
                result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_ADC_SWITCH               \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "Analog Capture Switch", \
                .index = 0,                       \
                .info =  pdplus_boolean2_info,    \
                .get =   pdplus_adc_switch_get,   \
                .put =   pdplus_adc_switch_set    \
        }

static int pdplus_adc_switch_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]=
                !!PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, MUTE_LEFT);
        u->value.integer.value[1]=
                !!PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, MUTE_RIGHT);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_adc_switch_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        write_lock_irqsave (&scard->lock, flags);
        if (PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, MUTE_LEFT) !=
                !!u->value.integer.value[0] ||
            PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, MUTE_RIGHT) !=
                !!u->value.integer.value[1])
        {
                PDPLUS_REPLACE (scard, CS4222, ADC_CTRL, MUTE_LEFT,
                        !!u->value.integer.value[0]);
                PDPLUS_REPLACE (scard, CS4222, ADC_CTRL, MUTE_RIGHT,
                        !!u->value.integer.value[1]);
                result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_ADC_HIGH_PASS                      \
        {                                                 \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,           \
                .name =  "Analog Capture High Pass Filter", \
                .index = 0,                                 \
                .info =  pdplus_boolean2_info,              \
                .get =   pdplus_adc_high_pass_get,          \
                .put =   pdplus_adc_high_pass_set           \
        }

static int pdplus_boolean2_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *uinfo)
{
        uinfo->type=   SNDRV_CTL_ELEM_TYPE_BOOLEAN;
        uinfo->count= 2;
        uinfo->value.integer.min= 0;
        uinfo->value.integer.max= 1;
        return 0;
}

static int pdplus_adc_high_pass_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]=
                !PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, HIGH_PASS_LEFT);
        u->value.integer.value[1]=
                !PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, HIGH_PASS_RIGHT);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_adc_high_pass_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        write_lock_irqsave (&scard->lock, flags);
        if (PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, HIGH_PASS_LEFT) !=
                !u->value.integer.value[0] ||
            PDPLUS_EXTRACT_CACHE (scard, CS4222, ADC_CTRL, HIGH_PASS_RIGHT) !=
                !u->value.integer.value[1])
        {
                PDPLUS_REPLACE (scard, CS4222, ADC_CTRL, HIGH_PASS_LEFT,
                        !u->value.integer.value[0]);
                PDPLUS_REPLACE (scard, CS4222, ADC_CTRL, HIGH_PASS_RIGHT,
                        !u->value.integer.value[1]);
                result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DAC_AUTO_MUTE_512             \
        {                                            \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,      \
                .name =  "PCM Playback Auto Mute 512", \
                .index = 0,                            \
                .info =  pdplus_boolean1_info,         \
                .get =   pdplus_dac_auto_mute_512_get, \
                .put =   pdplus_dac_auto_mute_512_set  \
        }

static int pdplus_boolean1_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *uinfo)
{
        uinfo->type=   SNDRV_CTL_ELEM_TYPE_BOOLEAN;
        uinfo->count= 1;
        uinfo->value.integer.min= 0;
        uinfo->value.integer.max= 1;
        return 0;
}

static int pdplus_dac_auto_mute_512_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, AUTO_MUTE_512);
        read_unlock_irqrestore (&scard->lock, flags);
        return 0;
}

static int pdplus_dac_auto_mute_512_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int current_value= 0;
        int change= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR (scard);

        write_lock_irqsave (&scard->lock, flags);
        current_value = PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, AUTO_MUTE_512);
        if (u->value.integer.value[0] != current_value) {
                PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, AUTO_MUTE_512,
                        !!u->value.integer.value[0]);
                change= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return change;
}

#define PDPLUS_CONTROL_A_OUT_MUX                \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "Analog Output Select",  \
                .index = 0,                       \
                .info =  pdplus_out_mux_info,     \
                .get =   pdplus_a_out_mux_get,    \
                .put =   pdplus_a_out_mux_set     \
        }

static void pdplus_info_enumerated (
        struct snd_ctl_elem_info *u,
        int count,
        char const *const *text,
        unsigned int max)
{
        u->type=   SNDRV_CTL_ELEM_TYPE_ENUMERATED;
        u->count= count;

        u->value.enumerated.items= max;
        if (u->value.enumerated.item > max)
                u->value.enumerated.item= max-1;

        strncpy (
                u->value.enumerated.name,
                text[u->value.enumerated.item],
                sizeof (u->value.enumerated.name));

        u->value.enumerated.name[sizeof (u->value.enumerated.name)-1]= 0;
}

static char const *pdplus_out_mux_text[4]= {
        [PDPLUS_ROUTE_A_PLAY] = "Analog Playback",
        [PDPLUS_ROUTE_A_CAPT] = "Analog Capture",
        [PDPLUS_ROUTE_D_PLAY] = "Digital Playback",
        [PDPLUS_ROUTE_D_CAPT] = "Digital Capture"
};

static int pdplus_out_mux_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
        pdplus_info_enumerated (u, 1, pdplus_out_mux_text, 4);
        return 0;
}

static int pdplus_a_out_mux_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.enumerated.item[0]= scard->a_out_route;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_a_out_mux_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        if (((unsigned)u->value.enumerated.item[0]) >= 4)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.enumerated.item[0] != scard->a_out_route) {
            if ((result = pdplus_a_route_set_ll (scard, u->value.enumerated.item[0])) == 0)
                    result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_D_OUT_MUX                \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "Digital Output Select", \
                .index = 0,                       \
                .info =  pdplus_out_mux_info,     \
                .get =   pdplus_d_out_mux_get,    \
                .put =   pdplus_d_out_mux_set     \
        }

static int pdplus_d_out_mux_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.enumerated.item[0]= scard->d_out_route;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_d_out_mux_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        if ((unsigned)(u->value.enumerated.item[0]) > 4)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.enumerated.item[0] != scard->d_out_route) {
            if ((result= pdplus_d_route_set_ll (scard, u->value.enumerated.item[0])) == 0)
                    result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_D_IN_MUX                 \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "Digital Input Select",  \
                .index = 0,                       \
                .info =  pdplus_in_mux_info,      \
                .get =   pdplus_d_in_mux_get,     \
                .put =   pdplus_d_in_mux_set      \
        }

static char const *pdplus_in_mux_text[3]= {
        [PDPLUS_FPGA_REG_DIG_SRC_SRC_NONE]    = "None",
        [PDPLUS_FPGA_REG_DIG_SRC_SRC_OPTICAL] = "Optical",
        [PDPLUS_FPGA_REG_DIG_SRC_SRC_COAXIAL] = "Coaxial"
};

static int pdplus_in_mux_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
        pdplus_info_enumerated (u, 1, pdplus_in_mux_text, 3);
        return 0;
}


static int pdplus_d_in_mux_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.enumerated.item[0]= PDPLUS_EXTRACT_CACHE (scard, FPGA, DIG_SRC, SRC);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_d_in_mux_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR(scard);

        if (((unsigned)u->value.enumerated.item[0]) >= 3)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.enumerated.item[0] != PDPLUS_EXTRACT_CACHE (scard, FPGA, DIG_SRC, SRC)) {
            PDPLUS_REPLACE (scard, FPGA, DIG_SRC, SRC, u->value.enumerated.item[0]);
            result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DE_EMPH                    \
        {                                         \
                .iface = SNDRV_CTL_ELEM_IFACE_PCM,     \
                .name=  "Codec De-Emphasis 50/15", \
                .index = 0,                         \
                .info =  pdplus_de_emph_info,       \
                .get =   pdplus_de_emph_get,        \
                .put =   pdplus_de_emph_set         \
        }

static char const *pdplus_de_emph_text[4]= {
        [PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_44100-1]    = "44100 Hz",
        [PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_48000-1]    = "48000 Hz",
        [PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_32000-1]    = "32000 Hz",
        [PDPLUS_CS4222_REG_DSP_DE_EMPHASIS_DISABLED-1] = "Disabled"
};

static int pdplus_de_emph_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
        pdplus_info_enumerated (u, 1, pdplus_de_emph_text, 4);
        return 0;
}


static int pdplus_de_emph_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.enumerated.item[0]= PDPLUS_EXTRACT_CACHE (scard, CS4222, DSP, DE_EMPHASIS) - 1;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_de_emph_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR(scard);

        if (((unsigned)u->value.enumerated.item[0]) >= 4)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.enumerated.item[0] !=
                PDPLUS_EXTRACT_CACHE (scard, CS4222, DSP, DE_EMPHASIS) - 1)
        {
            PDPLUS_REPLACE (scard, CS4222, DSP, DE_EMPHASIS, u->value.enumerated.item[0] + 1);
            result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DAC_SOFT_RAMP                       \
        {                                                  \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,            \
                .name =  "PCM Playback Volume Soft Ramping", \
                .index = 0,                                  \
                .info =  pdplus_soft_ramp_info,              \
                .get =   pdplus_soft_ramp_get,               \
                .put =   pdplus_soft_ramp_set                \
        }

#define PDPLUS_SOFT_RAMP_OFF 4
static char const *pdplus_soft_ramp_text[5]= {
        [PDPLUS_SOFT_RAMP_OFF]                          = "On Zero Crossing",
        [PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_8_LRCKS]  = "During 8 Frames",
        [PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_4_LRCKS]  = "During 4 Frames",
        [PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_16_LRCKS] = "During 16 Frames",
        [PDPLUS_CS4222_REG_DAC_CTRL_SOFT_STEP_32_LRCKS] = "During 32 Frames"
};

static int pdplus_soft_ramp_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
        pdplus_info_enumerated (u, 1, pdplus_soft_ramp_text, 5);
        return 0;
}


static unsigned int pdplus_soft_ramp_get_raw_llr (pdplus_t *scard)
{
        if (PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, SOFT_MUTE))
                return PDPLUS_EXTRACT_CACHE (scard, CS4222, DAC_CTRL, SOFT_STEP);
        else
                return PDPLUS_SOFT_RAMP_OFF;
}

static int pdplus_soft_ramp_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.enumerated.item[0]= pdplus_soft_ramp_get_raw_llr (scard);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_soft_ramp_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);
        PDPLUS_LOCAL_VADDR(scard);

        if (((unsigned)u->value.enumerated.item[0]) >= 5)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.enumerated.item[0] != pdplus_soft_ramp_get_raw_llr (scard))
        {
                if (u->value.enumerated.item[0] == PDPLUS_SOFT_RAMP_OFF) {
                        PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, SOFT_MUTE, 0);
                }
                else {
                        PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, SOFT_MUTE, 1);
                        PDPLUS_REPLACE (scard, CS4222, DAC_CTRL, SOFT_STEP,
                                u->value.enumerated.item[0]);
                }
                result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_D_CAPT_RATE              \
        {                                       \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
                .name =  "Digital Input Clock",   \
                .index = 0,                       \
                .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
                .info = pdplus_d_capt_rate_info, \
                .get = pdplus_d_capt_rate_get,  \
                .put = NULL                     \
        }

static int pdplus_d_capt_rate_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *uinfo)
{
        uinfo->type=   SNDRV_CTL_ELEM_TYPE_INTEGER;
        uinfo->count= 1;
        uinfo->value.integer.min= 0;
        uinfo->value.integer.max= 96000;
        return 0;
}

static int pdplus_d_capt_rate_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= pdplus_d_capt_rate_llr  (scard);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

#define PDPLUS_CONTROL_DEF_PROFI_MODE              \
        {                                          \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,     \
                .name =  "Default Digital Format",   \
                .index = 0,                          \
                .info =  pdplus_def_profi_mode_info, \
                .get =   pdplus_def_profi_mode_get,  \
                .put =   pdplus_def_profi_mode_set,  \
        }

static int pdplus_def_profi_mode_info (struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
        char const *text[3]= {
                "S/PDIF (Consumer)",
                "AES/EBU (Professional)",
                "AES/EBU if rate > 48000"
        };
        pdplus_info_enumerated (u, 1, text, 3);
        return 0;
}

static int pdplus_def_profi_mode_get_raw_llr (pdplus_t *scard)
{
        if (scard->auto_profi_mode)
                return 2;
        else
        if (scard->profi_mode)
                return 1;
        else
                return 0;
}

static int pdplus_def_profi_mode_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= pdplus_def_profi_mode_get_raw_llr (scard);
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_def_profi_mode_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        if (((unsigned)u->value.integer.value[0]) >= 3)
                return -EINVAL;

        write_lock_irqsave (&scard->lock, flags);
        if (u->value.integer.value[0] != pdplus_def_profi_mode_get_raw_llr (scard)) {
                switch (u->value.integer.value[0]) {
                default:
                case 0:
                        scard->auto_profi_mode= 0;
                        scard->profi_mode= 0;
                        break;
                case 1:
                        scard->auto_profi_mode= 0;
                        scard->profi_mode= 1;
                        break;
                case 2:
                        scard->auto_profi_mode= 1;
                        scard->profi_mode= 0;
                        break;
                }
                result= 1;
        }
        write_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DEF_COPY_INHIBIT             \
        {                                           \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,      \
                .name =  "Default Copy Inhibit",      \
                .index = 0,                           \
                .info =  pdplus_boolean1_info,        \
                .get =   pdplus_def_copy_inhibit_get, \
                .put =   pdplus_def_copy_inhibit_set  \
        }

static int pdplus_def_copy_inhibit_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= !!scard->copy_inhibit;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_def_copy_inhibit_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        result= (u->value.integer.value[0] == scard->copy_inhibit) ? 0 : 1;
        scard->copy_inhibit= !!u->value.integer.value[0];
        read_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DEF_NON_AUDIO             \
        {                                        \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,   \
                .name =  "Default Non-Audio Bit",  \
                .index = 0,                        \
                .info =  pdplus_boolean1_info,     \
                .get =   pdplus_def_non_audio_get, \
                .put =   pdplus_def_non_audio_set  \
        }

static int pdplus_def_non_audio_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= !!scard->non_audio;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_def_non_audio_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        result= (u->value.integer.value[0] == scard->non_audio) ? 0 : 1;
        scard->non_audio= !!u->value.integer.value[0];
        read_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DEF_PRE_EMPHASIS             \
        {                                           \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,      \
                .name =  "Default Pre-Emphasis",      \
                .index = 0,                           \
                .info =  pdplus_boolean1_info,        \
                .get =   pdplus_def_pre_emphasis_get, \
                .put =   pdplus_def_pre_emphasis_set  \
        }

static int pdplus_def_pre_emphasis_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= !!scard->pre_emphasis;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_def_pre_emphasis_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        result= (u->value.integer.value[0] == scard->pre_emphasis) ? 0 : 1;
        scard->pre_emphasis= !!u->value.integer.value[0];
        read_unlock_irqrestore (&scard->lock, flags);

        return result;
}

#define PDPLUS_CONTROL_DEF_AUTO_CD_MODE             \
        {                                           \
                .iface = SNDRV_CTL_ELEM_IFACE_MIXER,      \
                .name =  "Default Auto CD Mode",      \
                .index = 0,                           \
                .info =  pdplus_boolean1_info,        \
                .get =   pdplus_def_auto_cd_mode_get, \
                .put =   pdplus_def_auto_cd_mode_set  \
        }

static int pdplus_def_auto_cd_mode_get (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        u->value.integer.value[0]= !!scard->auto_cd_mode;
        read_unlock_irqrestore (&scard->lock, flags);

        return 0;
}

static int pdplus_def_auto_cd_mode_set (struct snd_kcontrol *k, struct snd_ctl_elem_value *u)
{
        u_long flags;
        int result= 0;
        pdplus_t *scard= snd_kcontrol_chip(k);

        read_lock_irqsave (&scard->lock, flags);
        result= (u->value.integer.value[0] == scard->auto_cd_mode) ? 0 : 1;
        scard->auto_cd_mode= !!u->value.integer.value[0];
        read_unlock_irqrestore (&scard->lock, flags);

        return result;
}

/*
 * S/PDIF set up
 */
static int pdplus_spdif_mask_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}
                        
static int pdplus_spdif_cmask_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
		IEC958_AES0_NONAUDIO |
		IEC958_AES0_CON_NOT_COPYRIGHT |
		IEC958_AES0_CON_EMPHASIS;
	ucontrol->value.iec958.status[1] = IEC958_AES1_CON_CATEGORY;
	ucontrol->value.iec958.status[3] = IEC958_AES3_CON_FS;
	return 0;
}
                        
static int pdplus_spdif_pmask_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
		IEC958_AES0_NONAUDIO |
		IEC958_AES0_PRO_FS |
		IEC958_AES0_PRO_EMPHASIS;
	return 0;
}

static int pdplus_spdif_default_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pdplus_t *scard = snd_kcontrol_chip(kcontrol);
	unsigned long flags;

	read_lock_irqsave(&scard->lock, flags);
	ucontrol->value.iec958 = scard->dig_setup;
	read_unlock_irqrestore(&scard->lock, flags);
	return 0;
}
                        
static int pdplus_spdif_default_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pdplus_t *scard = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int change;

	write_lock_irqsave(&scard->lock, flags);
	if (memcmp(&ucontrol->value.iec958, &scard->dig_setup, sizeof(scard->dig_setup)) == 0)
		change = 0;
	else {
		scard->dig_setup = ucontrol->value.iec958;
		change = 1;
	}
	write_unlock_irqrestore(&scard->lock, flags);
	return change;
}

/*
 * entries
 */
static struct snd_kcontrol_new pdplus_control[] __devinitdata = {
        PDPLUS_CONTROL_DAC_VOLUME,
        PDPLUS_CONTROL_DAC_SWITCH,
        PDPLUS_CONTROL_DAC_AUTO_MUTE_512,
        PDPLUS_CONTROL_DAC_SOFT_RAMP,
        PDPLUS_CONTROL_ADC_SWITCH,
        PDPLUS_CONTROL_ADC_HIGH_PASS,
        PDPLUS_CONTROL_DE_EMPH,
        PDPLUS_CONTROL_A_OUT_MUX,
        PDPLUS_CONTROL_D_OUT_MUX,
        PDPLUS_CONTROL_D_IN_MUX,
        PDPLUS_CONTROL_D_CAPT_RATE,

        PDPLUS_CONTROL_DEF_PROFI_MODE,
        PDPLUS_CONTROL_DEF_COPY_INHIBIT,
        PDPLUS_CONTROL_DEF_NON_AUDIO,
        PDPLUS_CONTROL_DEF_AUTO_CD_MODE,
        PDPLUS_CONTROL_DEF_PRE_EMPHASIS,

	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,CON_MASK),
		.info = pdplus_spdif_mask_info,
		.get = pdplus_spdif_cmask_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,PRO_MASK),
		.info = pdplus_spdif_mask_info,
		.get = pdplus_spdif_pmask_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
		.info = pdplus_spdif_mask_info,
		.get = pdplus_spdif_default_get,
		.put = pdplus_spdif_default_put,
	},
};

#define PDPLUS_CONTROL_CNT ARRAY_SIZE(pdplus_control)

/* New mixer */
static int __devinit pdplus_mixer_new (pdplus_t *scard)
{
        struct snd_card *card;
        struct snd_kcontrol *k;
        unsigned int i;
        int err= 0;

        snd_assert (scard != NULL, return -ENXIO);

        card= scard->card;
        strcpy (card->mixername, DEVICE_NAME);

        for (i = 0; i < PDPLUS_CONTROL_CNT; i++) {
                k= snd_ctl_new1(&pdplus_control[i], scard);
		if (k && k->id.iface == SNDRV_CTL_ELEM_IFACE_PCM)
			k->id.device = scard->digital->device;
                if ((err = snd_ctl_add (card, k)) < 0)
                        return err;
        }
        return 0;
}


/* ********************************************************************** */
/* High-level functions.
 *
 * These functions handle the locks themselves.
 */

/*
 * Upload an FPGA circuit into the card's FPGA.  Without a circuit, the
 * hardware only does PCI negotiations, but nothing else.  To get any
 * function out of it, a circuit needs to be uploaded.  There are two modes
 * which are exclusive: ADAT and Digital. */
static BOOL pdplus_upload_circuit (
        pdplus_t *scard,
        u_char const *circuit,
        u_int len)
{
        u_char bit = 1;
        u_long flags;
        BOOL result = FALSE;
        PDPLUS_LOCAL_VADDR (scard);
        ENTER;

        snd_assert (scard != NULL, return FALSE);

#if DEBUG
        Vprintk ("jiffies=%ld\n", jiffies);
#endif

        /* Lock access to the hardware to clear interrupts. */
        write_lock_irqsave (&scard->lock, flags);
        pdplus_disable_interrupts_ll (scard);
        write_unlock_irqrestore (&scard->lock, flags);
                                
        /*
         * Enable interrupts.  The following will take some time,
         * so allow interrupts. */
        write_lock (&scard->lock);

        /* Reset the card, clear interrupts */
        pdplus_local_reset_ll (scard);

        /* Upload circuit */
        for (; len; len--) {
                if (*circuit & bit)
                        PDPLUS_WRITE_HW (scard, HW, LOAD, ~0U);
                else
                        PDPLUS_WRITE_HW (scard, HW, LOAD, ~1U);

                /* Dummy read to ensure uploading is not too fast */
                PDPLUS_READ_HW (scard, PLX, CTRL);

                bit <<= 1;
                if (bit == 0) {
                        circuit++;
                        bit = 1;
                }
        }

        /* Wait 5 milliseconds. */
        mdelay (5);

        /* Force sane state of board */
        pdplus_init_circuit_ll (scard);

#if DEBUG
        Vprintk ("jiffies=%ld\n", jiffies);
#endif

#if CHECK
        /*
         * Wait another 2 milliseconds.  If no interrupt occured during these 8ms
         * of waiting, there is a problem. */
        mdelay (2);

        /* Check whether interrupt status is set */
        if (PDPLUS_EXTRACT_INIT (scard, PLX, INTCSR, STATUS_1) == 0) {
                printk (PDPLUS_KERN_ERR
                        "Device could not be initialised (no interrupt was generated).\n");
                goto raus;
        }

        if (PDPLUS_EXTRACT_HW (scard, FPGA, STATUS, INT) == 0) {
                printk (PDPLUS_KERN_ERR
                        "Device could not be initialised (interrupt occurred but Prodif Plus "
                        "card says it did not generate it).\n");
                goto raus;
        }

        /* Check whether interrupt status can be cleared (we must do this at least twice
         * in case an interrupt is generated just after we cleared the interrupt status.
         * To prevent scheduling during this phase, we lock irqs on this cpu.
         */
        local_irq_save (flags);
        for (len = 0; len < 3; len++) {
                pdplus_clear_interrupt_ll (scard);
                if (PDPLUS_EXTRACT_HW (scard, FPGA, STATUS, INT) == 0) {
                        result = TRUE;
                        goto raus;
                }
        }
        local_irq_restore (flags);

        printk (PDPLUS_KERN_ERR
                "Device could not be initialised (interrupt status could not be cleared).\n");
#endif


raus:
        /* Unlock access to the hardware */
        write_unlock (&scard->lock);

        LEAVE (result);
}


/*
 * Switch modes (ADAT<->Digital).
 * NOTE: The caller should check whether it is wise to switch modes, i.e.,
 *       check that no recording or playing currently takes place, since
 *       a mode switch resets the card completely! */
static BOOL pdplus_switch_circuit (pdplus_t *scard, int mode)
{
        if (scard->mode != mode) {
                BOOL circuit_ok = FALSE;
                u_long flags;

                switch (mode) {
                case PDPLUS_MODE_DIGITAL:
                        circuit_ok = pdplus_upload_circuit (
                                scard,
                                pga_dig,
                                PGA_DIG_CNT);
                        break;

                case PDPLUS_MODE_ADAT:
                        circuit_ok = pdplus_upload_circuit (
                                scard,
                                pga_adat,
                                PGA_ADAT_CNT);
                        break;
                }

                write_lock_irqsave (&scard->lock, flags);
                if (circuit_ok) {
                        scard->mode = mode;
                        pdplus_routes_set_ll (scard);
                }
                else {
                        pdplus_local_reset_ll (scard);
                        scard->mode = PDPLUS_MODE_NONE;
                }

                if (scard->mode != PDPLUS_MODE_NONE)
                        pdplus_enable_interrupts_ll (scard);

                write_unlock_irqrestore (&scard->lock, flags);
                return scard->mode == mode;
        }

        return TRUE; /* nothing to do: success */
}

/* ********************************************************************** */

#if DEBUG
static void pdplus_print_iomem_ll (
        struct snd_info_buffer *buffer,
        snd_iomem_t *iomem)
{
        void __iomem *lauf;
        void __iomem *end;
        int i = 0;

        snd_assert (iomem != NULL, return);
        lauf = iomem->vaddr;
        end =  iomem->vaddr + iomem->size;

        while (lauf < end) {
                if (i == 8) {
                        snd_iprintf (buffer, "\n");
                        i = 0;
                }
                if (i == 0)
                        snd_iprintf (buffer, "base+%08lx: ", (unsigned long)lauf - (unsigned long)iomem->vaddr);
                snd_iprintf (buffer, " %08x", IOMEM_READ (lauf));
                lauf+= 4;
                i++;
        }
        snd_iprintf (buffer, "\n");
}
#endif

/* Translate a number of an analog output route to normalised route number. */
static int pdplus_xlat_a_route (int route)
{
        switch (route) {
        case PDPLUS_FPGA_REG_CTRL_A_ROUTE_A_PLAY: return PDPLUS_ROUTE_A_PLAY;
        case PDPLUS_FPGA_REG_CTRL_A_ROUTE_A_CAPT: return PDPLUS_ROUTE_A_CAPT;
        case PDPLUS_FPGA_REG_CTRL_A_ROUTE_D_PLAY: return PDPLUS_ROUTE_D_PLAY;
        case PDPLUS_FPGA_REG_CTRL_A_ROUTE_D_CAPT: return PDPLUS_ROUTE_D_CAPT;
        }
        snd_BUG();
        return 0;
}

/* Translate a number of an digital output route to normalised route number. */
static int pdplus_xlat_d_route (int route)
{
        switch (route) {
        case PDPLUS_FPGA_REG_CTRL_D_ROUTE_A_PLAY: return PDPLUS_ROUTE_A_PLAY;
        case PDPLUS_FPGA_REG_CTRL_D_ROUTE_A_CAPT: return PDPLUS_ROUTE_A_CAPT;
        case PDPLUS_FPGA_REG_CTRL_D_ROUTE_D_PLAY: return PDPLUS_ROUTE_D_PLAY;
        case PDPLUS_FPGA_REG_CTRL_D_ROUTE_D_CAPT: return PDPLUS_ROUTE_D_CAPT;
        }
        snd_BUG();
        return 0;
}

/* Translate a number of an output route to humam-readable string. */
static char const *pdplus_route_str (int route)
{
        switch (route) {
        case PDPLUS_ROUTE_A_PLAY: return "analog playback";
        case PDPLUS_ROUTE_A_CAPT: return "analog capture";
        case PDPLUS_ROUTE_D_PLAY: return "digital playback";
        case PDPLUS_ROUTE_D_CAPT: return "digital capture";
        default:
                return "invalid";
        }
        snd_BUG();
        return "**unreachable**";
}


/* Handle /proc reading
 * FIXME: This should lock correctly!  But it does so much... */
static void pdplus_proc_read (
        struct snd_info_entry *entry,
	struct snd_info_buffer *buffer)
{
        u_long flags;
        u_long ticker;
        u_long ticker_100sec;
        int dco_rate;
        int rate;
        int i;
        pdplus_t *scard = (pdplus_t*)entry->private_data;
        pdplus_t *lcard; /* only _llr function may be called with &lcard!! */
        PDPLUS_LOCAL_VADDR (scard);

        snd_assert (scard != NULL, return);
        snd_assert (scard->card != NULL, return);
        snd_assert (scard->card->longname != NULL, return);

	lcard = kmalloc(sizeof(*lcard), GFP_KERNEL);
	if (! lcard)
		return;

        snd_iprintf (buffer, scard->card->longname);
        snd_iprintf (buffer, " (index #%d)\n", scard->card->number + 1);

        read_lock_irqsave (&scard->lock, flags);
        dco_rate = pdplus_dco_scan_rate_lli (scard);
        *lcard = *scard;
        read_unlock_irqrestore (&scard->lock, flags);


#ifdef COMPILE_DATE
        snd_iprintf (buffer, "Compiled: "COMPILE_DATE"\n");
#endif

        ticker = lcard->ticker;
	i = (int)(ticker % PDPLUS_INTERRUPTS_PER_SEC);
	i = (i * 100) / PDPLUS_INTERRUPTS_PER_SEC;
        ticker_100sec = (ticker / PDPLUS_INTERRUPTS_PER_SEC) * 100;
	ticker_100sec += i;
        snd_iprintf (buffer,
                "\nTicker: %lu (%lu:%02d:%02d.%02d)\n",
                ticker,
                (long)(ticker_100sec / (100 * 60 * 60)),
                (int)((ticker_100sec / (100 * 60)) % 60),
                (int)((ticker_100sec / 100) % 60),
                (int)(ticker_100sec % 100));

        /* FFG */
        snd_iprintf (
                buffer,
                "\n"
                "Fixed Frequency Generator\n"
                "\tProgrammed rate: ");

        rate = pdplus_get_ffg_rate_llr (lcard);
        if (rate == 0)
                snd_iprintf (buffer, "no clock\n");
        else
        if (rate < 0)
                snd_iprintf (buffer, "undefined!\n");
        else
                snd_iprintf (buffer, "%d Hz\n", rate);

        /* DCO */
        snd_iprintf (
                buffer,
                "\n"
                "DCO Frequency Generator\n"
                "\tProgrammed rate: ");

        if (lcard->dco_target_rate == PDPLUS_ADAT_RATE)
                snd_iprintf (buffer, "ADAT input clock\n");
        else
                snd_iprintf (buffer, "%d Hz\n", lcard->dco_target_rate);

        snd_iprintf (
                buffer,
                "\tScanned rate:    ");

        if (dco_rate <= 0)
                snd_iprintf (buffer, "scan timeout\n");
        else
                snd_iprintf (buffer, "%d Hz\n", dco_rate);

        /* Display clocks at devices */
        snd_iprintf (
                buffer,
                "\n"
                "Clocks of Devices\n"
                "\tAnalog  In/Out: ");
        i = pdplus_a_clock_rate_llr (lcard);
        if (i == 0)
                snd_iprintf (buffer, "invalid\n");
        else
        if (i < 0)
                snd_iprintf (buffer, "unpredictable\n");
        else
                snd_iprintf (buffer, "%d Hz\n", i);

        snd_iprintf (
                buffer,
                "\tDigital Input:  ");
        i = pdplus_d_capt_rate_llr (lcard);
        if (i == 0)
                snd_iprintf (buffer, "invalid\n");
        else
        if (i < 0)
                snd_iprintf (buffer, "unpredictable\n");
        else
                snd_iprintf (buffer, "%d Hz\n", i);

        snd_iprintf (
                buffer,
                "\tDigital Output: ");
        i = pdplus_d_clock_rate_llr (lcard);
        if (i == 0)
                snd_iprintf (buffer, "invalid\n");
        else
        if (i < 0)
                snd_iprintf (buffer, "unpredictable\n");
        else
                snd_iprintf (buffer, "%d Hz\n", i);

        /* Digital output settings */
        snd_iprintf (
                buffer,
                "\nDigital Output Settings\n"
                "\tFormat: %s\n",
                PDPLUS_EXTRACT_CACHE (lcard, HW, WR, PRO1)
                        ? "AES/EBU (Professional)"
                        : "IEC958, S/PDIF (Consumer)");

        if (PDPLUS_EXTRACT_CACHE (lcard, HW, WR, PRO1)) {
                snd_iprintf (buffer,
                        "\tNon-Audio: %d\n"
                        "\tRate Indication: ",
                        PDPLUS_EXTRACT_CACHE (lcard, HW, WR, PRO_NON_AUDIO));
                switch (PDPLUS_EXTRACT_CACHE (lcard, HW, WR, PRO_FS) &
                        PDPLUS_HW_REG_WR_PRO_FS_MASK)
                {
                case PDPLUS_HW_REG_WR_PRO_FS_NOT_INDICATED: snd_iprintf (buffer, "none"); break;
                case PDPLUS_HW_REG_WR_PRO_FS_32000:         snd_iprintf (buffer, "32000"); break;
                case PDPLUS_HW_REG_WR_PRO_FS_44100:         snd_iprintf (buffer, "44100"); break;
                case PDPLUS_HW_REG_WR_PRO_FS_48000:         snd_iprintf (buffer, "48000"); break;
                }
        }
        else {
                snd_iprintf (buffer,
                        "\tCopy Inhibit: %d\n"
                        "\tPre-Emphasis: %d\n"
                        "\tRate Indication: ",
                        PDPLUS_EXTRACT_CACHE (lcard, HW, WR, CON_COPY_INHIBIT),
                        PDPLUS_EXTRACT_CACHE (lcard, HW, WR, CON_PRE_EMPHASIS));
                switch (PDPLUS_EXTRACT_CACHE (lcard, HW, WR, CON_FS)) {
                case PDPLUS_HW_REG_WR_CON_FS_32000:   snd_iprintf (buffer, "32000"); break;
                case PDPLUS_HW_REG_WR_CON_FS_44100:   snd_iprintf (buffer, "44100"); break;
                case PDPLUS_HW_REG_WR_CON_FS_48000:   snd_iprintf (buffer, "48000"); break;
                case PDPLUS_HW_REG_WR_CON_FS_44100CD: snd_iprintf (buffer, "44100 CD"); break;
                }
        }
        snd_iprintf (buffer, "\n");

        /* Display output route settings. */
        i = pdplus_xlat_a_route (pdplus_get_a_route_llr (lcard));
        snd_iprintf (
                buffer,
                "\n"
                "Output Route Settings\n"
                "\tAnalog  Output: %s",
                pdplus_route_str (i));  
        if (i != lcard->a_out_route)
                snd_iprintf (buffer, " (selected: %s)", pdplus_route_str (lcard->a_out_route));

        i = pdplus_xlat_d_route (PDPLUS_EXTRACT_CACHE (lcard, FPGA, CTRL, D_ROUTE));
        snd_iprintf (
                buffer,
                "\n"
                "\tDigital Output: %s",
                pdplus_route_str (i));
        if (i != lcard->d_out_route)
                snd_iprintf (buffer, " (selected: %s)", pdplus_route_str (lcard->d_out_route));
        snd_iprintf (buffer, "\n");


        /* Display running decives */
        snd_iprintf (
                buffer,
                "\n"
                "Running devices:");
        if (PDPLUS_EXTRACT_CACHE (lcard, FPGA, CTRL, A_PLAY_ENABLE))
                snd_iprintf (buffer, " a_play");
        if (PDPLUS_EXTRACT_CACHE (lcard, FPGA, CTRL, A_CAPT_ENABLE))
                snd_iprintf (buffer, " a_capt");
        if (PDPLUS_EXTRACT_CACHE (lcard, FPGA, CTRL, D_PLAY_ENABLE))
                snd_iprintf (buffer, " d_play");
        if (PDPLUS_EXTRACT_CACHE (lcard, FPGA, CTRL, D_CAPT_ENABLE))
                snd_iprintf (buffer, " d_capt");
        snd_iprintf (buffer, "\n");

#if DEBUG
        snd_iprintf (
                buffer,
                "\n"
                "Minimal period for digital output : %d\n"
                "Bytes per interrupt (digital output): %d\n"
                "Minimal period for analog output  : %d\n"
                "Bytes per interrupt (analog output) : %d\n",
                PDPLUS_BYTES_FROM_FRAMES(
                        pdplus_fit_period_size (1, is_adat, pdplus_d_clock_rate_llr (scard)),
                        is_adat),
                (pdplus_d_clock_rate_llr(scard) * 4 * 2) / PDPLUS_INTERRUPTS_PER_SEC,
                PDPLUS_BYTES_FROM_FRAMES(
                        pdplus_fit_period_size (1, is_adat, pdplus_a_clock_rate_llr (scard)),
                        is_adat),
                (pdplus_a_clock_rate_llr(scard) * 4 * 2) / PDPLUS_INTERRUPTS_PER_SEC);

        snd_iprintf (
                buffer,
                "\n"
                "Low-Level Read Registers\n"
                "\tFPGA counter:        0x%08x\n"
                "\tFPGA frequency scan: 0x%08x\n"
                "\tFPGA status:         0x%08x\n"
                "\tHW/RD:               0x%08x\n",
                PDPLUS_READ_HW (lcard, FPGA, CNT),
                PDPLUS_READ_HW (lcard, FPGA, FREQ_SCAN),
                PDPLUS_READ_HW (lcard, FPGA, STATUS),
                PDPLUS_READ_HW (lcard, HW,   RD));

        snd_iprintf (
                buffer,
                "\n"
                "Low-Level Write Registers (Cache Values)\n"
                "\tFPGA ctrl:           0x%08x\n"
                "\tFPGA ffg:            0x%08x\n"
                "\tFPGA dco:            0x%08x\n"
                "\tFPGA dig src:        0x%08x\n"
                "\tFPGA adat monitor:   0x%08x\n"
                "\tCS4222 adc ctrl:     0x%08x\n"
                "\tCS4222 dac ctrl:     0x%08x\n"
                "\tCS4222 left atten:   0x%08x\n"
                "\tCS4222 right atten:  0x%08x\n"
                "\tCS4222 dsp mode:     0x%08x\n"
                "\tHW/WR:               0x%08x\n",
                PDPLUS_READ_CACHE (lcard, FPGA,   CTRL),
                PDPLUS_READ_CACHE (lcard, FPGA,   FFG),
                PDPLUS_READ_CACHE (lcard, FPGA,   DCO),
                PDPLUS_READ_CACHE (lcard, FPGA,   DIG_SRC),
                PDPLUS_READ_CACHE (lcard, FPGA,   ADAT),
                PDPLUS_READ_CACHE (lcard, CS4222, ADC_CTRL),
                PDPLUS_READ_CACHE (lcard, CS4222, DAC_CTRL),
                PDPLUS_READ_CACHE (lcard, CS4222, ATT_LEFT),
                PDPLUS_READ_CACHE (lcard, CS4222, ATT_RIGHT),
                PDPLUS_READ_CACHE (lcard, CS4222, DSP),
                PDPLUS_READ_CACHE (lcard, HW,     WR));

        snd_iprintf (buffer, "\nPLX Regs\n");
        pdplus_print_iomem_ll (buffer, &scard->PLX_iomem);

#if 0
        snd_iprintf (buffer, "\nFPGA Regs\n");
        pdplus_print_iomem (buffer, &scard->FPGA_iomem);

        snd_iprintf (buffer, "\nHW Regs\n");
        pdplus_print_iomem (buffer, &scard->HW_iomem);
#endif

	kfree(lcard);
#endif
}


/*
 * Register the /proc entry */
static void __devinit pdplus_register_proc (pdplus_t *scard)
{
        struct snd_info_entry *entry;

        snd_assert (scard != NULL, return);
        snd_assert (scard->card != NULL, return);

        if (! snd_card_proc_new (scard->card, "prodif_plus", &entry))
		snd_info_set_text_ops(entry, scard, pdplus_proc_read);
}


/*
 * Check that the resource queries of the card are sane */
#if MANY_CHECKS
static int __devinit pdplus_check_resources (pci_dev_t *pci)
{
        u_long x;

        if ((x = RESOURCE_LEN (pci, 0)) != 0x80) {
                printk (PDPLUS_KERN_ERR "Expected 0x80 bytes of PCI resource 0, got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if ((x = RESOURCE_LEN (pci, 1)) != 0x80) {
                printk (PDPLUS_KERN_ERR "Expected 0x80 bytes of PCI resource 1, got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if ((x = RESOURCE_LEN (pci, 2)) != 0x20000) {
                printk (PDPLUS_KERN_ERR "Expected 0x20000 bytes of PCI resource 2, got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if ((x = RESOURCE_LEN (pci, 3)) != 0x400) {
                printk (PDPLUS_KERN_ERR "Expected 0x400 bytes of PCI resource 3, got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if ((x = RESOURCE_LEN (pci, 4)) != 0x200) {
                printk ("Expected 0x200 byte of PCI resource 4, got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if (((x = pci_resource_flags (pci, 0)) & IORESOURCE_MEM) == 0) {
                printk ("Expected IO Mem of PCI resource 0 got 0x%lx.\n", x);
                LEAVE (-EINVAL);                                                
        }

        if (((x = pci_resource_flags (pci, 1)) & IORESOURCE_IO) == 0) {
                printk ("Expected IO Mem of PCI resource 1 got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if (((x = pci_resource_flags (pci, 2)) & IORESOURCE_MEM) == 0) {
                printk ("Expected IO Mem of PCI resource 2 got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if (((x = pci_resource_flags (pci, 3)) & IORESOURCE_MEM) == 0) {
                printk ("Expected IO Mem of PCI resource 3 got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        if (((x = pci_resource_flags (pci, 4)) & IORESOURCE_MEM) == 0) {
                printk ("Expected IO Mem of PCI resource 4 got 0x%lx.\n", x);
                LEAVE (-EINVAL);
        }

        return 0;
}
#endif

/*
 * Register memory area */
static int __devinit pdplus_register_iomem (
        struct snd_card *card,
        pci_dev_t *pci,
        int num,
        u_long size,
        snd_iomem_t *piomem,
        const char *name)
{
        u_long start = pci_resource_start (pci, num);

        DL (1, "Trying to grab MMIO region at 0x%lx, size=0x%lx", start, size);

        snd_assert (piomem != NULL, return -ENXIO);
        
        if ((piomem->resource = request_mem_region(start, size, name)) == NULL) {
		snd_printk(KERN_ERR "unable to grab memory region 0x%lx-0x%lx\n",
			   start, start + size - 1);
		LEAVE(-EBUSY);
	}

        piomem->start = start;
        piomem->size = size;

        piomem->vaddr = pdplus_remap_pci_mem (start, size);
        if (!piomem->vaddr) {
                printk (PDPLUS_KERN_ERR
                        "Unable to grab MMIO region at 0x%lx, size=0x%lx.\n",
                        start,
                        size);
                LEAVE (-EBUSY);
        }

        return 0;
}

#if CHECK
/*
 * Check consistency of some aspects of the card. */
static int __devinit pdplus_check_consistency_ll (pdplus_t *scard)
{
        u32 x, y;
        PDPLUS_LOCAL_VADDR (scard);

        snd_assert (scard != NULL, return -ENXIO);
        snd_assert (scard->PLX_ioport != 0, return -ENXIO);

        /*
         * Some consistency checks to detect really weird errors
         * No 1: LAS0RR are the same when accessed via IO and MMIO.
         */
        x = IO_READ  (scard->PLX_ioport + PDPLUS_PLX_REG_R_LAS0RR);
        y = PDPLUS_READ_HW (scard, PLX, LAS0RR);
        if (x != y) {
                printk (PDPLUS_KERN_ERR
                        "IO and MMIO report different values for PLX reg LAS0RR: 0x%x, 0x%x.\n",
                        x, y);
                LEAVE (-ENODEV);
        }

        if (PDPLUS_READ_HW (scard, PLX, LAS0BA) != PDPLUS_PLX_REG_LAS0BA_INITIAL ||
                PDPLUS_READ_HW (scard, PLX, LAS1BA) != PDPLUS_PLX_REG_LAS1BA_INITIAL ||
                PDPLUS_READ_HW (scard, PLX, LAS2BA) != PDPLUS_PLX_REG_LAS2BA_INITIAL ||
                PDPLUS_READ_HW (scard, PLX, LAS3BA) != PDPLUS_PLX_REG_LAS3BA_INITIAL)
        {
                printk (PDPLUS_KERN_ERR
                        "PLX address space registers (LASxBA) contain wrong values.\n");
                LEAVE (-ENODEV);
        }

        return 0;
}
#endif


/*
 * Grab resources and initialise card.
 * NOTE: It is very important in this function to NOT use the PDPLUS_ macros for
 *       reading or writing the registers since local copies of the virtual addresses
 *       are not easy to have here.  Furthermore, check that all called functions
 *       only use the vaddrs known at the time of calling!! (ht)
 */
static int __devinit pdplus_init(
        pci_dev_t  *pci,
        struct snd_card *card)
{
        int err;
        u_long start;
        pdplus_t *scard;

        snd_assert (card != NULL, return -ENXIO);

        scard = (pdplus_t *)card->private_data;
        if (scard == NULL)
                LEAVE (-ENOMEM);

        scard->pci = pci;
        scard->card = card;
	scard->irq = -1;

        /* Set up variables which are possibly non-null at start-up. */
        /* *INIT* */
        scard->a_out_route =     PDPLUS_ROUTE_A_PLAY;
        scard->d_out_route =     PDPLUS_ROUTE_D_PLAY;
        rwlock_init(&scard->lock);
        scard->auto_cd_mode =    1;
        scard->auto_profi_mode = 1;

        /* ALSA should have kcallocked our private_data.  Check this. */
        snd_assert (scard->PLX_iomem.vaddr == NULL,  scard->PLX_iomem.vaddr = NULL);
        snd_assert (scard->MEM_iomem.vaddr == NULL,  scard->MEM_iomem.vaddr = NULL);
        snd_assert (scard->FPGA_iomem.vaddr == NULL, scard->FPGA_iomem.vaddr = NULL);
        snd_assert (scard->HW_iomem.vaddr == NULL,   scard->HW_iomem.vaddr = NULL);

#if MANY_CHECKS
        DL (1, "Checking PCI resources");
        if ((err = pdplus_check_resources (pci)) != 0)
                return err;
#endif

        /* Register communication area for PLX */
        if ((err = pdplus_register_iomem (card, pci, 0, 0x80, &scard->PLX_iomem, FULL_NAME " - PLX mem")) < 0)
                return err;

        scard->PLX_ioport = start = pci_resource_start (pci, 1);
        DL (1, "Trying to register IO region at 0x%lx", start);
	if ((scard->res_PLX_io = request_region(start, 0x80, FULL_NAME " - PLX io")) == NULL) {
                printk (PDPLUS_KERN_ERR "Unable to register IO region 0x%lx-0x%lx.\n", start, start + 0x80 - 1);
		LEAVE(-EBUSY);
	}

#if CHECK
        /*
         * Read initial values from the registers of the PLX we will use.
         * CHECK: This function only uses the PLX vaddr which is available already. */
        if ((err = pdplus_check_consistency_ll (scard)) < 0)
                return err;
#endif
        /*
         * Read initial values from the registers of the PLX we will use.
         * CHECK: This function only uses the PLX vaddr which is available already. */
        pdplus_read_initial_values_ll (scard);

        /*
         * Switch off interrupts in hardware register of PLX
         * CHECK: This function only uses the PLX vaddr which is available already. */
        pdplus_disable_interrupts_ll (scard);

        /* Try to grab an interrupt for the card. */
        DL (1, "Trying to register irq %d", pci->irq);
	if (request_irq(pci->irq, pdplus_interrupt, IRQF_SHARED, FULL_NAME, scard)) {
		snd_printk ("Unable to grab interrupt %d.\n", pci->irq);
		return -EBUSY;
	}
	scard->irq = pci->irq;

        /* Register io memory */
        if ((err = pdplus_register_iomem (card, pci, 2, 0x20000, &scard->MEM_iomem, FULL_NAME " - MEM")) < 0)
                return err;

        if ((err = pdplus_register_iomem (card, pci, 3, 0x400, &scard->FPGA_iomem, FULL_NAME " - FPGA")) < 0)
                return err;

        if ((err = pdplus_register_iomem (card, pci, 4, 0x200, &scard->HW_iomem, FULL_NAME " - HW")) < 0)
                return err;

        /* Generate PCM devices */
        Vprintk ("Generating PCM devices.\n");
        if ((err = pdplus_a_pcm_new (card, scard, 0)) < 0)
                return err;

        if ((err = pdplus_d_pcm_new (card, scard, 1)) < 0)
                return err;

        /* Generate mixer */
        Vprintk ("Generating mixer.\n");
        if ((err = pdplus_mixer_new (scard)) < 0)
                return err;

        Vprintk ("Generating /proc entry.\n");
        /* Generate /proc entry */
        pdplus_register_proc (scard);

        /* Wake up card */
        Vprintk ("Uploading circuit.\n");
        if (!pdplus_switch_circuit (scard, init_adat ? PDPLUS_MODE_ADAT : PDPLUS_MODE_DIGITAL))
                LEAVE (-ENODEV);
        /* **** FROM HERE, INTERRUPTS MAY BE RECEIVED SO THE SET-UP SHOULD BE DONE! **** */


        /* Finally register driver */
        Vprintk ("Finishing registration.\n");
        strcpy (card->driver, DEVICE_NAME);
        strcpy (card->shortname, FULL_NAME);
        sprintf(card->longname, FULL_NAME" at 0x%lx,0x%lx,0x%lx,0x%lx, irq %d",
                (unsigned long)pci_resource_start (scard->pci, 0),
                (unsigned long)pci_resource_start (scard->pci, 2),
                (unsigned long)pci_resource_start (scard->pci, 3),
                (unsigned long)pci_resource_start (scard->pci, 4),
                scard->irq);

        if ((err = snd_card_register(card)) >= 0) {
                pci_set_drvdata (pci, card);
#if MANY_CHECKS
                if (pci_get_drvdata (pci) != card) {
                        printk (KERN_ERR
                                "pci_get_drvdata(pci)=%p != card =%p\n",
                                pci_get_drvdata (pci),
                                card);
                        LEAVE (-EINVAL);
                }
#endif

                /* FIXME: continue: add 2*PCM (analog, digital) */
                DL (1, "No errors.");
                LEAVE (0);
        }

        LEAVE (err);
}

/*
 * Try to grab the PCI device */
static int __devinit pdplus_probe(
        pci_dev_t *pci,
        pci_device_id_t const *pci_id)
{
	static int dev = 0;
        int err;
        struct snd_card *card;
        ENTER;

        if (dev >= SNDRV_CARDS)
                LEAVE (-ENODEV);

        if (!enable[dev]) {
                dev++;
                LEAVE (-ENOENT);
        }

        card = snd_card_new (index[dev], id[dev], THIS_MODULE, 0);
        if (card == NULL)
                LEAVE (-ENOMEM);

	card->private_data = kzalloc(sizeof(pdplus_t), GFP_KERNEL);
	if (card->private_data == NULL) {
		snd_card_free(card);
                LEAVE (-ENOMEM);
	}
	card->private_free = pdplus_sweep;

	snd_card_set_dev(card, &pci->dev);

        err = pdplus_init (pci, card);

        if (err != 0) {
                snd_card_free (card);
                LEAVE (err);
        }

        dev++;
        LEAVE (0);
}

static void pdplus_unregister_iomem(snd_iomem_t *iomem)
{
        ENTER;

        pdplus_unmap_pci_mem (iomem);
        LEAVE_VOID;
}

/*
 * Release resources */
static void pdplus_sweep(struct snd_card *card)
{
        pdplus_t *scard;
        u_long flags;
        ENTER;

        snd_assert (card != NULL, return);

        scard = (pdplus_t *)card->private_data;
        snd_assert (scard != NULL, return);

        write_lock_irqsave (&scard->lock, flags);

        unless (silent_exit)
                pdplus_shutdown_ll (scard);

        pdplus_disable_interrupts_ll (scard);

        write_unlock_irqrestore (&scard->lock, flags);

        pdplus_unregister_iomem (&scard->PLX_iomem);
        pdplus_unregister_iomem (&scard->MEM_iomem);
        pdplus_unregister_iomem (&scard->FPGA_iomem);
        pdplus_unregister_iomem (&scard->HW_iomem);

	if (scard->res_PLX_io) {
		release_resource(scard->res_PLX_io);
		kfree_nocheck(scard->res_PLX_io);
	}
	if (scard->irq >= 0)
		free_irq(scard->irq, (void *)scard);

	kfree(scard);
        LEAVE_VOID;
}


/*
 * Release the PCI device */
static void __devexit pdplus_remove(pci_dev_t *pci)
{
        struct snd_card *card;
        ENTER;

        Vprintk ("MMIO 0= 0x%lx\n", (unsigned long)pci_resource_start (pci, 0));
        card = pci_get_drvdata (pci);

        if (card == 0)
                Vprintk ("Error: card == %p.\n", card);
        else
		snd_card_free (card);
        pci_set_drvdata (pci, NULL);

        LEAVE_VOID;
}

static struct pci_device_id pdplus_ids[] = {
        { PCI_DEVICE(PCI_VENDOR_ID_MARIAN, PCI_DEVICE_ID_MARIAN_PRODIF_PLUS) },
        { 0, }
};

MODULE_DEVICE_TABLE(pci, pdplus_ids);

/* ********************************************************************** */

static struct pci_driver driver = {
	.name =     FULL_NAME,
	.id_table = pdplus_ids,
	.probe =    pdplus_probe,
	.remove =   __devexit_p(pdplus_remove),
};

static int __init alsa_card_pdplus_init(void)
{
        /*
         * Check that our macros work. */
        snd_assert (PDPLUS_HW_REG_WR_INITIAL_1 == 0x22, return -ENXIO);
        snd_assert (PDPLUS_HW_REG_WR_INITIAL_2 == 0x32, return -ENXIO);

        printk (PDPLUS_KERN_INFO "version " PDPLUS_VERSION "\n");
	return pci_register_driver (&driver);
}

static void __exit alsa_card_pdplus_exit(void)
{
        ENTER;
        pci_unregister_driver (&driver);
        LEAVE_VOID;
}

module_init(alsa_card_pdplus_init)
module_exit(alsa_card_pdplus_exit)

#ifndef MODULE

/* format is: snd-pdplus=enable,index,id
              snd-pdplus-misc=init_adat */

static int __init alsa_card_pdplus_setup(char *str)
{
	static unsigned __initdata nr_dev = 0;

	if (nr_dev >= SNDRV_CARDS)
		return 0;
	(void)(get_option(&str,&enable[nr_dev]) == 2 &&
	       get_option(&str,&index[nr_dev]) == 2 &&
	       get_id(&str,&id[nr_dev]) == 2);
	nr_dev++;
	return 1;
}

static int __init alsa_card_pdplus_setup_misc(char *str)
{
	(void)get_option(&str,&init_adat);
	return 1;
}

__setup("snd-pdplus=", alsa_card_pdplus_setup);
__setup("snd-pdplus-misc=", alsa_card_pdplus_setup_misc);

#endif /* ifndef MODULE */

/*
 * Local variables:
 * tab-width: 8
 * End:
 */
