/*******************************************************************************

File name   : vcd_print.c

Description : Value Change Dump - IEEE 1364-2005 section 18.2.1
This is a copy of all relevant defines, enums, structures, and functions from
fvdp/mpe42/fvdp_vcd.h which involve printing to an output stream in the vcd
file format.

COPYRIGHT (C) STMicroelectronics 2013

*******************************************************************************/

//******************************************************************************
//  I N C L U D E    F I L E S
//******************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include "vcd_print.h"

//******************************************************************************
//  D E F I N E S
//******************************************************************************
#define VCD_VAR_ASCII_START 33      // The identifier code is a code composed of the printable
                                    // characters, which are in the ASCII character set
                                    // from ! to ~ (decimal 33 to 126).
#define VCD_VAR_ASCII_STOP  126     // Max ASCII code before increasing number of characters

//******************************************************************************
//  G L O B A L S
//******************************************************************************
static FILE*                vcd_output_file = NULL;


//******************************************************************************
// S T A T I C   F U N C T I O N   H E A D E R
//******************************************************************************
static void VCD_PrintEvent(uint32_t timeref, VCDEvent_t event, uint32_t value, uint8_t size);
static void VCD_PrintBinary(uint32_t val, uint8_t size);


//******************************************************************************
//  S O U R C E     C O D E
//******************************************************************************

#define VDC_PRINTF(...)     fprintf(vcd_output_file, __VA_ARGS__)

//=============================================================================
//Method:       VCD_SetOutputFile
//Description:  Set the output file descriptor
//
//In Param:     FILE* output_file - output file descriptor (already opened)
//Out Param:
//Return:       void
//=============================================================================
void VCD_SetOutputFile(FILE* output_file)
{
    if (output_file != NULL)
        vcd_output_file = output_file;
}

//=============================================================================
//Method:       vcd_GetIdentifierName
//Description:  Determine the VCD identifier name
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
static void vcd_GetIdentifierName (uint8_t var_id, char* name)
{
    uint8_t base = VCD_VAR_ASCII_STOP - VCD_VAR_ASCII_START + 1;
    uint8_t chr2 = var_id / base;
    uint8_t chr1 = var_id - (chr2 * base);

    if (chr2 > 0)  *(name++) = VCD_VAR_ASCII_START + chr2;
    *(name++) = VCD_VAR_ASCII_START + chr1;
    *(name++) = '\0';
}

//=============================================================================
//Method:       VCD_PrintHeader
//Description:  Print the VCD header information.  Should only be called by user
//              if VCD_LOG_TO_BUFFER is set to FALSE
//
//In Param:     vcd_log_ptr - pointer to type VCDLog_t
//Out Param:
//Return:       void
//=============================================================================
void VCD_PrintHeader(VCDLog_t* vcd_log_ptr)
{
    uint8_t      var_id;
    VCDModule_t  module = VCD_MODULE_DEFAULT;
    char         var_id_name[3];

    VDC_PRINTF("$version VCD FVDP/FBM debugger $end\n");

    VDC_PRINTF("$timescale");
    switch(vcd_log_ptr->timescale_num)
    {
        case VCD_TIMESCALE_NUM_1:
        {
            VDC_PRINTF(" 1");
            break;
        }
        case VCD_TIMESCALE_NUM_10:
        {
            VDC_PRINTF(" 10");
            break;
        }
        case VCD_TIMESCALE_NUM_100:
        {
            VDC_PRINTF(" 100");
            break;
        }
    }
    switch(vcd_log_ptr->timescale_unit)
    {
        case VCD_TIMESCALE_UNIT_s:
        {
            VDC_PRINTF(" s ");
            break;
        }
        case VCD_TIMESCALE_UNIT_ms:
        {
            VDC_PRINTF(" ms ");
            break;
        }
        case VCD_TIMESCALE_UNIT_us:
        {
            VDC_PRINTF(" us ");
            break;
        }
        case VCD_TIMESCALE_UNIT_ps:
        {
            VDC_PRINTF(" ps ");
            break;
        }
        case VCD_TIMESCALE_UNIT_fs:
        {
            VDC_PRINTF(" fs ");
            break;
        }
    }
    VDC_PRINTF("$end\n\n");

    //VDC_PRINTF("$scope module top $end\n");

    for (var_id = 0; var_id < vcd_log_ptr->num_events; var_id++)
    {

        if (vcd_log_ptr->vcd_values[var_id].module != module)
        {
            if (module != VCD_MODULE_DEFAULT)
            {
                VDC_PRINTF("$upscope $end\n");
            }
            VDC_PRINTF("$scope module %s $end\n", vcd_log_ptr->module_str[vcd_log_ptr->vcd_values[var_id].module]);
            module = vcd_log_ptr->vcd_values[var_id].module;
        }

        if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_INTEGER)
        {
            VDC_PRINTF("$var integer ");
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_EVENT)
        {
            VDC_PRINTF("$var event ");
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_WIRE)
        {
            VDC_PRINTF("$var wire ");
        }

        vcd_GetIdentifierName(var_id, var_id_name);
        VDC_PRINTF("%d %s %s $end\n", vcd_log_ptr->vcd_values[var_id].size, var_id_name, vcd_log_ptr->vcd_values[var_id].name);
    }

    if (module != VCD_MODULE_DEFAULT)
    {
        VDC_PRINTF("$upscope $end\n");
    }

    VDC_PRINTF("$enddefinitions $end\n\n");
}

//=============================================================================
//Method:       VCD_PrintLogBuffer
//Description:  Prints the events logged in the buffer in VCD format as per
//              IEEE 1364-2005 section 18.2.1
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
void VCD_PrintLogBuffer(VCDLog_t* vcd_log_ptr)
{
    uint16_t index, print_stop_index;
    uint32_t initial_time_ref;

    VCD_PrintHeader(vcd_log_ptr);

    if (vcd_log_ptr->buf_length == 0)
    {
        VDC_PRINTF("empty log.\n");
        return;
    }

    if (vcd_log_ptr->wraparound == TRUE)
    {
        for (index = 0; index < vcd_log_ptr->num_events; index ++)
        {
            if (CHECKBIT_VECT(vcd_log_ptr->initialized,index))
            {
                VCD_PrintEvent(0,
                               index,
                               vcd_log_ptr->initial_values[index],
                               vcd_log_ptr->vcd_values[index].size);
            }
        }
    }

    index = (vcd_log_ptr->wraparound == TRUE) ? vcd_log_ptr->write_index : 0;
    print_stop_index = (index + vcd_log_ptr->buf_length) % vcd_log_ptr->buf_length;
    initial_time_ref = vcd_log_ptr->buffer[index].timeref;

    do
    {
        VCD_PrintEvent(vcd_log_ptr->buffer[index].timeref - initial_time_ref,
                       vcd_log_ptr->buffer[index].event,
                       vcd_log_ptr->buffer[index].value,
                       vcd_log_ptr->vcd_values[vcd_log_ptr->buffer[index].event].size);

        index = (index + 1) % vcd_log_ptr->buf_length;
    } while (index != print_stop_index);
}

//=============================================================================
//Method:       VCD_PrintEvent
//Description:  static function for printing the event in VCD format as per
//              IEEE 1364-2005 section 18.2.1
//
//In Param:     uint32_t   timeref - the time reference for the event
//              VCDEvent_t event   - the event ID
//              uint32_t   value   - the value of the event
//              uint8_t    size    - size of value (in number of bits)
//Out Param:
//Return:       void
//=============================================================================
static void VCD_PrintEvent(uint32_t timeref, VCDEvent_t event, uint32_t value, uint8_t size)
{
    char var_id_name[3];

    VDC_PRINTF("#%u\n", timeref);

    vcd_GetIdentifierName(event, var_id_name);

    if (size > 1)
    {
        VCD_PrintBinary(value, size);
        VDC_PRINTF(" %s\n", var_id_name);
    }
    else
    {
        VDC_PRINTF("%u%s\n", (value & 0x1), var_id_name);
    }
}

//=============================================================================
//Method:       VCD_PrintBinary
//Description:  static function for printing in binary format (with a 'b' prefix)
//
//In Param:     uint32_t val - value that should be printed in binary format
//              uint8_t size - size of value (in number of bits)
//Out Param:
//Return:       void
//=============================================================================
static void VCD_PrintBinary(uint32_t val, uint8_t size)
{
    uint8_t i;

    VDC_PRINTF("b");
    for (i = 1; i <= size; i++)
    {
        if (((val >> (size-i)) & 1) == 1)
            VDC_PRINTF("1");
        else
            VDC_PRINTF("0");
    }

}

//=============================================================================
//Method:       VCD_PrintModulesEvents
//Description:  Print all modules and events to stdout
//
//In Param:     vcd_log_ptr - pointer to type VCDLog_t
//Out Param:
//Return:       void
//=============================================================================
void VCD_PrintModulesEvents(VCDLog_t* vcd_log_ptr)
{
    uint8_t      var_id;

    printf ("%5s      %25s    %7s    %4s    %s\n",
            "EVENT",
            "NAME",
            "TYPE",
            "SIZE",
            "MODULE");

    printf ("%5s      %25s    %7s    %4s    %s\n",
            "-----",
            "-------------------------",
            "-------",
            "----",
            "----------------------");

    for (var_id = 0; var_id < vcd_log_ptr->num_events; var_id++)
    {
        char* module_name = "unknown";
        char* type = "unknown";
        uint8_t module = vcd_log_ptr->vcd_values[var_id].module;

        if (module < vcd_log_ptr->num_modules)
            module_name = vcd_log_ptr->module_str[module];

        if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_INTEGER)
        {
            type = "integer";
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_EVENT)
        {
            type = "event";
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_WIRE)
        {
            type = "wire";
        }

        printf ("%5d  :   %25s    %7s    %4d    %s\n",
                var_id,
                vcd_log_ptr->vcd_values[var_id].name,
                type,
                vcd_log_ptr->vcd_values[var_id].size,
                module_name);
    }
}

