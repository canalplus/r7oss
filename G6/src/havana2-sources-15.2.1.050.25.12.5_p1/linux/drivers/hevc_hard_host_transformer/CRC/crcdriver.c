/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.
************************************************************************/

//
// CRC driver
//
// Manages comms with user space & decoder/preprocessor
//
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>

#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/cdev.h>

#include "osdev_mem.h"

#include "checker.h"
#include "crc.h"
#include "crcdriver.h"

#define MAX_CRC_LINE 500 // should be enough to hold any line of a CRC file

#define MAX_STREAMS 20 // number of minor numbers for the CRC device

//
// API between User space <=> Linux Kernel
//

static int CRCOpen(struct inode *inode, struct file *filep);
static int CRCRelease(struct inode *inode, struct file *filep);
static ssize_t CRCReadComputed(struct file *filep, char *buff, size_t count, loff_t *offp);
static ssize_t CRCWriteRefs(struct file *filep, const char *buff, size_t count, loff_t *offp);

static struct file_operations FileOps =
{
        owner:          THIS_MODULE,
        open:           CRCOpen,
        release:        CRCRelease,
        read:           CRCReadComputed,
        write:          CRCWriteRefs,
};

typedef struct
{
	CheckerHandle_t my_checker;
	char            CRCParseLine[MAX_CRC_LINE];
	int             parseSize;
	char            CRCOutputLine[MAX_CRC_LINE];
	int 		outputSize;
} CRCInstance;

//
// Helper functions to convert between CRC binary format and CRC text format
//

static int ParseCRCLine (char* line, int max, int* is_comment, FrameCRC_t* crc);
static int FormatCRCLine(char* line, int max,            const FrameCRC_t* crc);

//
// character device internals
//
static dev_t           device;
static struct cdev     chardev;
static CheckerHandle_t checker[MAX_STREAMS];

//
// /////////////////////////////////////////
//

int CRCDriver_Init(const char* device_name)
{
	int Result;
	int minor;

	// Allocate major/minor numbers
	Result  = alloc_chrdev_region (&device, 0, MAX_STREAMS, device_name);
	if (Result < 0)
	{
	        pr_err("Error: CRCDriver: unable to allocate device numbers\n");
	        return -ENODEV;
	}
        pr_info("CRCDriver: major %d, minor %d to %d\n", MAJOR(device), MINOR(device), MINOR(device) + MAX_STREAMS - 1);

	// Create device
	cdev_init (&chardev, &FileOps);
	chardev.owner = THIS_MODULE;
	kobject_set_name (& chardev.kobj, "%s%d", device_name, 0);

	// Add device
	Result = cdev_add (&chardev, device, MAX_STREAMS);
	if (Result < 0)
	{
	        pr_err("Error: CRCDriver: unable to add device\n");
	        return -ENODEV;
	}

	// Initialize checkers array
	for (minor = 0; minor < MAX_STREAMS; minor ++)
		checker[minor] = NULL;

	return 0;
}

int CRCDriver_Term(void)
{
	cdev_del(&chardev);
	unregister_chrdev_region (device, MAX_STREAMS);
	return 0;
}

int CRCDriver_GetChecker(int minor, CheckerHandle_t* handle)
{
	if (minor < 0 || minor >= MAX_STREAMS)
		return -EINVAL;
	if (checker[minor] != NULL)
		CHK_Take(checker[minor]);
	*handle = checker[minor];
	return 0;
}

int CRCDriver_ReleaseChecker(int minor)
{
	CheckerStatus_t status;

	if (minor < 0 || minor >= MAX_STREAMS)
		return -EINVAL;
	if (checker[minor] == NULL)
		return -EIO;
	status = CHK_Release(checker[minor]);
	if (status == CHECKER_STOPPED)
		checker[minor] = NULL;
	return 0;
}


int CRCOpen(struct inode *inode, struct file *filep)
{
	CheckerStatus_t status;
	CRCInstance*    instance;
	int             minor = iminor(inode);

	if (filep->f_mode & FMODE_WRITE)
	{
		if (checker[minor] != NULL)
		{
			pr_err("Error: CRC: device %d: already checking\n", minor);
			return -EBUSY;
		}
		status = CHK_Alloc(&checker[minor]);
		if (status == CHECKER_OUT_OF_MEMORY)
			return -ENOMEM;
		if(status != CHECKER_NO_ERROR)
			return -EIO;
		pr_info("CRC: now checking stream %d..\n", minor);
	}
	else // read mode
	{
		if (checker[minor] == NULL)
		{
			pr_err("Error: CRC: device %d: no ongoing check\n", minor);
			return -ENODEV;
		}
		if (CHK_Take(checker[minor]) != CHECKER_NO_ERROR)
			return -EIO;
	}

	filep->private_data = OSDEV_Malloc(sizeof(CRCInstance));
	instance = (CRCInstance*) filep->private_data;
	instance->my_checker = checker[minor];
	instance->parseSize = 0;
	instance->outputSize = 0;
	return 0;
}

int CRCRelease(struct inode *inode, struct file *filep)
{
	CheckerStatus_t status;
	CRCInstance*    instance;

	if (filep->private_data == NULL)
		return -EIO; // should not happen
	instance = (CRCInstance*) (filep->private_data);

	if (filep->f_mode & FMODE_WRITE)
	{
		// pr_info("CRC: shutting down check for stream %d..\n", iminor(inode));
		status = CHK_ShutdownReferences(instance->my_checker); // no more refs to come
		if (status != CHECKER_NO_ERROR)
			return -EIO;
	}
	// pr_info("CRC: releasing CRC device %d..\n", iminor(inode));
	status = CHK_Release(instance->my_checker);
	if (status == CHECKER_STOPPED)
		checker[iminor(inode)] = NULL;
	OSDEV_Free(filep->private_data);
	return 0;
}

ssize_t CRCWriteRefs(struct file *filep, const char *buff, size_t count, loff_t *offp)
{
	CRCInstance* instance = (CRCInstance*) filep->private_data;
	int total_written = 0;

	while (count > 0)
	{
		int size;
		int remaining;
		FrameCRC_t crc;
		int parsed;
		int is_comment;

		size = MAX_CRC_LINE - instance->parseSize;
		if (size > count)
			size = count;
		remaining = copy_from_user(instance->CRCParseLine + instance->parseSize, buff + total_written, size);
		if (remaining != 0)
			return -EFAULT;
		total_written += size;
		count -= size;
		instance->parseSize += size;

		parsed = ParseCRCLine(instance->CRCParseLine, instance->parseSize, &is_comment, &crc);
		if (parsed < 0)
			return -EIO;
		if (parsed != 0)
		{
			if (! is_comment)
			{
				CheckerStatus_t status = CHK_AddRefCRC(instance->my_checker, &crc);
				if (status == CHECKER_STOPPED)
					return -ENOSPC;
				if (status != CHECKER_NO_ERROR)
					return -EIO;
			}
			memmove(instance->CRCParseLine, instance->CRCParseLine + parsed, instance->parseSize - parsed);
			instance->parseSize -= parsed;
		}
	}
	(*offp) += total_written;
	return total_written;
}

ssize_t CRCReadComputed(struct file *filep, char *buff, size_t count, loff_t *offp)
{
	CRCInstance* instance = (CRCInstance*) filep->private_data;
	CheckerStatus_t status;
	int total_read = 0;

	while (count > 0)
	{
		int remaining;
		int size;
		FrameCRC_t crc;
		int formatted;

		// first try to write remaining stored output
		if (instance->outputSize != 0)
		{
			size = instance->outputSize;
			if (size > count)
				size = count;
			remaining = copy_to_user(buff + total_read, instance->CRCOutputLine, size);
			// pr_info("CRCDriver: read: sent %d bytes to user space, remaining %d\n", size, remaining);
			if (remaining != 0)
				return -EFAULT;
			count -= size;
			total_read += size;
			memmove(instance->CRCOutputLine, instance->CRCOutputLine + size, instance->outputSize - size);
			instance->outputSize -= size;
			break; // debug: for line by line output, FIXME should be an option ? TODO(pht) check if still required ?
			if (instance->outputSize != 0)
				break; // waiting for an empty buffer before writing into it
		}

		// retrieve a CRC
		// pr_info("CRCDriver: read: waiting for computed CRC..\n");
		status = CHK_GetComputedCRC(instance->my_checker, &crc);
		if (status == CHECKER_STOPPED)
			break;
		if (status != CHECKER_NO_ERROR)
			return -EIO;

		// Format the CRC
		formatted = FormatCRCLine(instance->CRCOutputLine, MAX_CRC_LINE, &crc);
		if (formatted <= 0)
			return -EIO;
		instance->outputSize += formatted;
	}
	(*offp) += total_read;
	return total_read;
}

// Parse a CRC field from reference files
// (*parsed) advances to next field (or to end of string)
static bool ParseNumber(char* line, int radix, bool is_signed, int* parsed, long* number)
{
	char* index = line + *parsed; // scans the line
	char* field;                  // first digit
	char* end_of_field;           // after last digit
	char* next_field;             // after the comma, or at terminating null char
	int ret;

	// Find beginning of field
	while (*index != 0 && (*index == ' ' || *index == '\t'))
		++ index;
	field = index;

	if (*index == ',' || *index == 0)
	{
		if (*index == ',')
			++ index;
		*parsed = index - line;
		return false; // empty field
	}

	// Find end of field
	while (*index != 0 && *index != ' ' && *index != '\t' && *index != ',')
		++ index;
	end_of_field = index;

	// Find next field
	while (*index != 0 && *index != ',')
		++ index;
	if (*index == ',')
		++ index;
	next_field = index;

	// Parse number
	*end_of_field = 0; // kstrtoint expects a null terminated string
	if (is_signed)
		ret = kstrtoint(field, radix, (int*)number);
	else
		ret = kstrtouint(field, radix, (int*)number);

	if (ret != 0)
	{
		*parsed = -1;
		pr_err("Error: CRCDriver: cannot read base-%d %s number from %s\n", radix, is_signed ? "signed" : "unsigned", field);
		return false;
	}

	*parsed = next_field - line;
	return true;
}

// Parse a CRC line from references file
// returns: 0   = incomplete line (no \n)
//          <0  = error
//          >0  = number of parsed bytes (including final \n)
static int ParseCRCLine(char* line, int max, int* comment, FrameCRC_t* crc)
{
	char* newline;
	int parsed;
	bool not_empty;

	for (newline = line; newline < line + max; newline++)
		if (*newline == '\n')
			break;
	if (newline >= line + max)
		return 0;

	if (line[0] == '#')
	{
		*comment = 1;
		return newline - line + 1;
	}

	*newline = 0;
	parsed = 0;
	crc->mask = 0;
	*comment = 0;

//
#define PARSE(field, radix, is_signed, bitfield) \
	not_empty = ParseNumber(line, radix, is_signed, &parsed, (long*)&(crc->field));		\
	if (parsed < 0)										\
	{											\
		pr_err("Error: CRCDriver: cannot read field " #field " in reference CRCs\n");	\
		pr_err("Error: CRCDriver: line is %s\n", line);					\
		return -1;									\
	}											\
	if (not_empty)										\
		crc->mask |= 1llu << CRC_BIT_##bitfield;
//

	PARSE(decode_index, 10, false, DECODE_INDEX);
	PARSE(idr         , 10, false, IDR);
	PARSE(poc         , 10, true , POC);

	PARSE(bitstream, 16, false, BITSTREAM);

	PARSE(sli_table,16, false, SLI_TABLE);
	PARSE(ctb_table,16, false,CTB_TABLE);
	PARSE(sli_header,16, false,SLI_HEADER);
	PARSE(commands,16, false,COMMANDS);
	PARSE(residuals,16, false,RESIDUALS);

	PARSE(hwc_ctb_rx,16, false,HWC_CTB_RX);
	PARSE(hwc_sli_rx,16, false,HWC_SLI_RX);
	PARSE(hwc_mvp_tx,16, false,HWC_MVP_TX);
	PARSE(hwc_red_tx,16, false,HWC_RED_TX);
	PARSE(mvp_ppb_rx,16, false,MVP_PPB_RX);
	PARSE(mvp_ppb_tx,16, false,MVP_PPB_TX);
	PARSE(mvp_cmd_tx,16, false,MVP_CMD_TX);
	PARSE(mac_cmd_tx,16, false,MAC_CMD_TX);
	PARSE(mac_dat_tx,16, false,MAC_DAT_TX);
	PARSE(xop_cmd_tx,16, false,XOP_CMD_TX);
	PARSE(xop_dat_tx,16, false,XOP_DAT_TX);
	PARSE(red_dat_rx,16, false,RED_DAT_RX);
	PARSE(red_cmd_tx,16, false,RED_CMD_TX);
	PARSE(red_dat_tx,16, false,RED_DAT_TX);
	PARSE(pip_cmd_tx,16, false,PIP_CMD_TX);
	PARSE(pip_dat_tx,16, false,PIP_DAT_TX);
	PARSE(ipr_cmd_tx,16, false,IPR_CMD_TX);
	PARSE(ipr_dat_tx,16, false,IPR_DAT_TX);
	PARSE(dbk_cmd_tx,16, false,DBK_CMD_TX);
	PARSE(dbk_dat_tx,16, false,DBK_DAT_TX);
	PARSE(sao_cmd_tx,16, false,SAO_CMD_TX);
	PARSE(rsz_cmd_tx,16, false,RSZ_CMD_TX);
	PARSE(os_o4_tx,16, false,OS_O4_TX);
	PARSE(os_r2b_tx,16, false,OS_R2B_TX);
	PARSE(os_rsz_tx,16, false,OS_RSZ_TX);

	PARSE(omega_luma, 16, false, OMEGA_LUMA);
	PARSE(omega_chroma, 16, false, OMEGA_CHROMA);
	PARSE(raster_luma, 16, false, RASTER_LUMA);
	PARSE(raster_chroma, 16, false, RASTER_CHROMA);
	PARSE(raster_decimated_luma, 16, false, RASTER_DECIMATED_LUMA);
	PARSE(raster_decimated_chroma, 16, false, RASTER_DECIMATED_CHROMA);

	return newline - line + 1;
}

// Format a CRC into a string (for output to computed CRC file)
// returns: <=0 => error
//          >0  => number of formatted bytes (including final \n)
static int FormatCRCLine(char* line, int max, const FrameCRC_t* crc)
{
	char* index = line;

//
#define DUMP(field, header, zero, digits, format, bitfield) \
        if (crc->mask & (1llu << CRC_BIT_##bitfield))                                                                                           \
                index += snprintf(index, max - (index - line), "%.*s%" zero #digits format ",", strlen(header) - digits, "          ", crc->field);     \
        else                                                                                                                                    \
                index += snprintf(index, max - (index - line), "%.*s,", strlen(header), "          ");                                          \
        if (*index != 0) /* overflow */                                                                                                         \
                return -1;
//

        DUMP(decode_index, "# DECODE",   "",  8, "u", DECODE_INDEX);
        DUMP(idr         , "IDR",        "",  1, "u", IDR);
        DUMP(poc         , "     POC",   "",  8, "d", POC);

        DUMP(bitstream,    "BITSTREAM",  "0", 8, "x", BITSTREAM);
        DUMP(sli_table,    "SLI_TABLE",  "0", 8, "x", SLI_TABLE);
        DUMP(ctb_table,    "CTB_TABLE",  "0", 8, "x", CTB_TABLE);
        DUMP(sli_header,   "SLI_HEADER", "0", 8, "x", SLI_HEADER);
        DUMP(commands,     "COMMANDS",   "0", 8, "x", COMMANDS);
        DUMP(residuals,    "RESIDUALS",  "0", 8, "x", RESIDUALS);

        DUMP(hwc_ctb_rx,   "HWC_CTB_RX", "0", 8, "x", HWC_CTB_RX);
        DUMP(hwc_sli_rx,   "HWC_SLI_RX", "0", 8, "x", HWC_SLI_RX);
        DUMP(hwc_mvp_tx,   "HWC_MVP_TX", "0", 8, "x", HWC_MVP_TX);
        DUMP(hwc_red_tx,   "HWC_RED_TX", "0", 8, "x", HWC_RED_TX);
        DUMP(mvp_ppb_rx,   "MVP_PPB_RX", "0", 8, "x", MVP_PPB_RX);
        DUMP(mvp_ppb_tx,   "MVP_PPB_TX", "0", 8, "x", MVP_PPB_TX);
        DUMP(mvp_cmd_tx,   "MVP_CMD_TX", "0", 8, "x", MVP_CMD_TX);
        DUMP(mac_cmd_tx,   "MAC_CMD_TX", "0", 8, "x", MAC_CMD_TX);
        DUMP(mac_dat_tx,   "MAC_DAT_TX", "0", 8, "x", MAC_DAT_TX);
        DUMP(xop_cmd_tx,   "XOP_CMD_TX", "0", 8, "x", XOP_CMD_TX);
        DUMP(xop_dat_tx,   "XOP_DAT_TX", "0", 8, "x", XOP_DAT_TX);
        DUMP(red_dat_rx,   "RED_DAT_RX", "0", 8, "x", RED_DAT_RX);
        DUMP(red_cmd_tx,   "RED_CMD_TX", "0", 8, "x", RED_CMD_TX);
        DUMP(red_dat_tx,   "RED_DAT_TX", "0", 8, "x", RED_DAT_TX);
        DUMP(pip_cmd_tx,   "PIP_CMD_TX", "0", 8, "x", PIP_CMD_TX);
        DUMP(pip_dat_tx,   "PIP_DAT_TX", "0", 8, "x", PIP_DAT_TX);
        DUMP(ipr_cmd_tx,   "IPR_CMD_TX", "0", 8, "x", IPR_CMD_TX);
        DUMP(ipr_dat_tx,   "IPR_DAT_TX", "0", 8, "x", IPR_DAT_TX);
        DUMP(dbk_cmd_tx,   "DBK_CMD_TX", "0", 8, "x", DBK_CMD_TX);
        DUMP(dbk_dat_tx,   "DBK_DAT_TX", "0", 8, "x", DBK_DAT_TX);
        DUMP(sao_cmd_tx,   "SAO_CMD_TX", "0", 8, "x", SAO_CMD_TX);
        DUMP(rsz_cmd_tx,   "RSZ_CMD_TX", "0", 8, "x", RSZ_CMD_TX);
        DUMP(os_o4_tx,     "OS_O4_TX",   "0", 8, "x", OS_O4_TX);
        DUMP(os_r2b_tx,    "OS_R2B_TX",  "0", 8, "x", OS_R2B_TX);
        DUMP(os_rsz_tx,    "OS_RSZ_TX",  "0", 8, "x", OS_RSZ_TX);

        DUMP(omega_luma,   "OMEGA4-L",   "0", 8, "x", OMEGA_LUMA);
        DUMP(omega_chroma, "OMEGA4-C",   "0", 8, "x", OMEGA_CHROMA);
        DUMP(raster_luma,  "RASTER-L",   "0", 8, "x", RASTER_LUMA);
        DUMP(raster_chroma,"RASTER-C",   "0", 8, "x", RASTER_CHROMA);
        DUMP(raster_decimated_luma,   "RESIZE-L", "0", 8, "x", RASTER_DECIMATED_LUMA);
        DUMP(raster_decimated_chroma, "RESIZE-C", "0", 8, "x", RASTER_DECIMATED_CHROMA);

        index --; // remove last comma

	*index = '\n';
	return index - line + 1;
}
