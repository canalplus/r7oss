/************************************************************************
COPYRIGHT (C) ST Microelectronics (R&D) 2005

Source file name : tccode.h
Author :           Automatically generated file

Contains the transport controller code packed as a word array

************************************************************************/


#define TRANSPORT_CONTROLLER_CODE_SIZE      103
#define TRANSPORT_CONTROLLER_LABLE_MAP_SIZE 35

static char const VERSION[] = "\nTC Version " __FILE__ " 0.0.0\n";


#define TRANSPORT_CONTROLLER_VERSION    2

#ifdef TCASM_LM_SUPPORT

#define TCASM_LM_CODE   (0)
#define TCASM_LM_DATA   (1)
#define TCASM_LM_CONST  (2)
#define TCASM_LM_UNKOWN (-1)

typedef struct TCASM_LM_s
{
    char           *Name;
    unsigned short  Value;
    unsigned short  Segment;
    unsigned char*  File;
    unsigned short  Line;
}TCASM_LM_t;

static TCASM_LM_t TCASM_LableMap[] =
{
    {"::__end_of_code__", 408, TCASM_LM_CODE, "<unknown>", 1 },
    {"::__css__", 0, TCASM_LM_CONST, "<unknown>", 1 },
    {"::__dss__", 32768, TCASM_LM_CONST, "<unknown>", 1 },
    {"::__StackPointer", 32768, TCASM_LM_DATA, "/tmp/lcc321201.asm", 3 },
    {"::_program_entry_point", 0, TCASM_LM_CODE, "/tmp/lcc321201.asm", 7 },
    {"::_main", 8, TCASM_LM_CODE, "pti_firmware.c", 24 },
    {"::_packet_size", 32772, TCASM_LM_DATA, "pti_firmware.c", 1 },
    {"::_thrown_away", 32774, TCASM_LM_DATA, "pti_firmware.c", 3 },
    {"::_header_size", 32776, TCASM_LM_DATA, "pti_firmware.c", 5 },
    {"::_10", 32778, TCASM_LM_DATA, "pti_firmware.c", 7 },
    {"::_11", 32778, TCASM_LM_DATA, "pti_firmware.c", 8 },
    {"::_22", 32778, TCASM_LM_DATA, "pti_firmware.c", 9 },
    {"::_25", 32778, TCASM_LM_DATA, "pti_firmware.c", 10 },
    {"::_26", 32778, TCASM_LM_DATA, "pti_firmware.c", 11 },
    {"::_pidsearch", 32954, TCASM_LM_DATA, "pti_firmware.c", 237 },
    {"::_pidtable", 32826, TCASM_LM_DATA, "pti_firmware.c", 172 },
    {"::_2", 24, TCASM_LM_CODE, "pti_firmware.c", 35 },
    {"::_pread", 32818, TCASM_LM_DATA, "pti_firmware.c", 166 },
    {"::_5", 64, TCASM_LM_CODE, "pti_firmware.c", 43 },
    {"::_pwrite", 32958, TCASM_LM_DATA, "pti_firmware.c", 240 },
    {"::_7", 72, TCASM_LM_CODE, "pti_firmware.c", 47 },
    {"::_8", 72, TCASM_LM_CODE, "pti_firmware.c", 47 },
    {"::_num_pids", 32810, TCASM_LM_DATA, "pti_firmware.c", 160 },
    {"::_buffer", 32778, TCASM_LM_DATA, "pti_firmware.c", 143 },
    {"::_12", 228, TCASM_LM_CODE, "pti_firmware.c", 94 },
    {"::_14", 188, TCASM_LM_CODE, "pti_firmware.c", 91 },
    {"::_15", 188, TCASM_LM_CODE, "pti_firmware.c", 91 },
    {"::_17", 228, TCASM_LM_CODE, "pti_firmware.c", 94 },
    {"::_21", 272, TCASM_LM_CODE, "pti_firmware.c", 107 },
    {"::_psize", 32814, TCASM_LM_DATA, "pti_firmware.c", 163 },
    {"::_19", 332, TCASM_LM_CODE, "pti_firmware.c", 112 },
    {"::_20", 392, TCASM_LM_CODE, "pti_firmware.c", 133 },
    {"::_discard", 32822, TCASM_LM_DATA, "pti_firmware.c", 169 },
    {"::_3", 392, TCASM_LM_CODE, "pti_firmware.c", 137 },
    {"::_1", 408, TCASM_LM_CODE, "pti_firmware.c", 141 },
};

#endif

static const unsigned int transport_controller_code[] = {
	0x51a00000, 0xc0000200, 0x2e800000, 0x40202ea8, 0x2ea00e80, 0x40002028, 
	0x52a00c80, 0x269fffe8, 0x03000028, 0x80001000, 0x52a02f80, 0x52200c80, 
	0x02c00128, 0x40202fa8, 0x2e800000, 0x40200ca8, 0x2e800040, 0x4000c128, 
	0x5280c100, 0x26800068, 0x03000028, 0x88001200, 0x52a00100, 0x4000c028, 
	0x52a00a80, 0x400020a8, 0x2f400180, 0x52a00204, 0x2ea00280, 0x2e400144, 
	0xe0400142, 0x44400532, 0x32800230, 0x2f400144, 0x2ec00182, 0x22c001a8, 
	0x2f400144, 0x2f800044, 0x52a00a80, 0x26bff828, 0x03000028, 0x80003900, 
	0x2ec00182, 0x2687ffe8, 0x400021a8, 0x2e800040, 0x40002128, 0x52802100, 
	0x26800068, 0x03000028, 0x88002f00, 0x52802200, 0x2e200000, 0x26c00128, 
	0x03000028, 0x80003900, 0x2f800004, 0x52a00100, 0x028000e8, 0x52200200, 
	0x02c0012c, 0x26800079, 0x03000028, 0x80004400, 0x52a02f80, 0x52200b80, 
	0x03400128, 0x98005300, 0x2ec00182, 0x40202ea8, 0x52a00180, 0x12800068, 
	0x402001a8, 0xe0400142, 0x2f400180, 0x52a02f80, 0x52200b80, 0x03400128, 
	0x98006200, 0x52a00d80, 0x12800068, 0x40200da8, 0xc0006200, 0x52a02f80, 
	0x12800068, 0x40202fa8, 0x52200204, 0x2ea00280, 0x2dc00144, 0xe0400102, 
	0x57c004c2, 0x2f8011c0, 0x32800631, 0x2fc00140, 0x2ec00182, 0x27803fe8, 
	0xe0400142, 0x2fc00180, 0xc0000600, 0x52a02f80, 0x12800068, 0x40202fa8, 
	0xc0006600 };

static const unsigned short int transport_controller_data[] = {
	0x199c, 0x0000, 0x00bc, 0x0000, 0x0006, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
	0x0000 };
