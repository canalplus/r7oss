/**
 *****************************************************************************
 * @file mapping.h
 * @authors amouriauxj rivierem frigierep
 *
 * @brief this file contains all the mapping definitions
 *
 * Copyright (C) 2012 Technicolor
 * All Rights Reserved
 *
 * This program contains proprietary information which is a trade
 * secret of Technicolor and/or its affiliates and also is protected as
 * an unpublished work under applicable Copyright laws. Recipient is
 * to retain this program in confidence and is not permitted to use or
 * make copies thereof other than as permitted in a written agreement
 * with Technicolor, unless otherwise expressly allowed by applicable laws
 *
 ******************************************************************************/

#ifndef MAPPING_H
#define MAPPING_H

/* Mapping with SNOR */

//#define __FACTORY__   yes

/* Base address and size for every types of memory device */
#define FLASH_NAND_START_ADDRESS                        (0x00000000)  /* accessible thru EBI1 */
#define FLASH_SNOR_START_ADDRESS                        (0x00000000)  /* TBD */
#define FLASH_SNOR_SIZE                                 (0x01000000)  /* N25Q128A 16MB */
#define FLASH_NAND_SIZE                                 (0xFFFFFFFF)  /* 4GB for API, 8GB whole size accessible thru EBI1 */
#define RAM_DDR_START_ADDRESS                           (0x00000000)  /* TBD ArmV7 */
#define RAM_DDR_SIZE                                    (0x80000000)  /* 2GB */

#define FLASH_BOOT_DEVICE_NAME                          "SNOR"
#define FLASH_NAND_DEVICE_NAME                          "EMMC"

/*
_/_/_/_/_/_/_/_/_/_/
_/ SNOR MAPPING  _/
_/_/_/_/_/_/_/_/_/
*/

/*******************************/
/* SNOR Boot area        */
/*******************************/

#define FLASH_BOOT_OFFSET                               (0x00000000)
#define FLASH_BOOT_SIZE                                 (0x001C0000)    /* 1792KB */

/* SNOR Individual Data area */
#define FLASH_SCS_FW_OFFSET                             (0x001C0000)
#define FLASH_SCS_FW_SIZE                               (0x00040000)    /* 256KB */

/* SNOR Individual Data area */
#define FLASH_INDIVIDUAL_DATA_OFFSET                    (0x00200000)
#define FLASH_INDIVIDUAL_DATA_SIZE                      (0x00020000)    /* 128KB */

/* SNOR Individual Data area */
#define FLASH_SPLASH_OFFSET                             (0x00220000)
#define FLASH_SPLASH_SIZE                               (0x00020000)    /* 128KB */

/* SNOR Individual Data area */
#define FLASH_BOLT_NVRAM_OFFSET                         (0x00240000)
#define FLASH_BOLT_NVRAM_SIZE                           (0x00010000)    /* 64KB */

/* SNOR Backip Loader area */
#define FLASH_LOADER_BACKUP_OFFSET                      (0x00250000)
#define FLASH_LOADER_BACKUP_SIZE                        (0x00DB0000)    /* 14016KB */

/*
_/_/_/_/_/_/_/_/_/_/
_/ eMMC MAPPING  _/
_/_/_/_/_/_/_/_/_/
*/

/*******************************/
/* eMMC /dev/mmcblk0        */
/*******************************/
#define FLASH_MMC_USER_OFFSET                           (0x00000000)
#define FLASH_MMC_USER_SIZE                             (0xFFA00000)

/* NAND Nagra DB2 main area */
#define FLASH_NAGRA_DB2_OFFSET                          (0x00002000) 
#define FLASH_NAGRA_DB2_SIZE                            (0x00000400)

/* NAND Nagra DB3 main area */
#define FLASH_NAGRA_DB3_OFFSET                          (0x00002400) 
#define FLASH_NAGRA_DB3_SIZE                            (0x0000E000)

/* NAND Nagra DB2 backup area */
#define FLASH_NAGRA_DB2_BACKUP_OFFSET                   (0x00012000) 
#define FLASH_NAGRA_DB2_BACKUP_SIZE                     (0x00000400)

/* NAND Nagra DB3 backup area */
#define FLASH_NAGRA_DB3_BACKUP_OFFSET                   (0x00012400) 
#define FLASH_NAGRA_DB3_BACKUP_SIZE                     (0x0000E000)

/* NAND SAV Reserved Area */
#define FLASH_SAV_RESERVED_AREA_OFFSET                  (0x00020400) 
#define FLASH_SAV_RESERVED_AREA_SIZE                    (0x00001C00)


/*******************************/
/* eMMC /dev/mmcblk0p1         */
/*******************************/
/* eMMC Loader area */
#define FLASH_LOADER_OFFSET                             (0x00026000)
#define FLASH_LOADER_SIZE                               (0x00FFA000)    /* 16MB - 8K */

/*******************************/
/* eMMC /dev/mmcblk0p5         */
/*******************************/
/*  Loader/Bootloader temp image storage area */
#define FLASH_APP_SLOT_1_OFFSET                         (0x01022000) 
#define FLASH_APP_SLOT_1_SIZE                           (0x063FE000)    /* 100M - 8K */

/*******************************/
/* eMMC /dev/mmcblk0p6         */
/*******************************/
/* eMMC FW NVRAM area */
#define FLASH_NVRAM_OFFSET                              (0x07422000)
#define FLASH_NVRAM_SIZE                                (0x0000E000)    /* 64KB - 8K */

/*******************************/
/* eMMC /dev/mmcblk0p7         */
/*******************************/
/* eMMC App Slot 0 area */
#define FLASH_APP_SLOT_0_OFFSET                         (0x07432000)
#define FLASH_APP_SLOT_0_SIZE                           (0x063FE000)    /* 100M - 8K */

/*******************************/
/* eMMC /dev/mmcblk0p8         */
/*******************************/
/* eMMC Nagra RW data area */
#define FLASH_NAGRA_RW_DATA_OFFSET                      (0x0D832000)
#define FLASH_NAGRA_RW_DATA_SIZE                        (0x007FE000)    /* 8MB - 8K */

/*******************************/
/* eMMC /dev/mmcblk0p9         */
/*******************************/
/* eMMC Application RW  file system area */
#define FLASH_APP_RW_FS_OFFSET                          (0x0E032000)
#define FLASH_APP_RW_FS_SIZE                            (0x001AE000)    /* 1720KB */

/*******************************/
/* eMMC /dev/mmcblk0p10         */
/*******************************/
/* eMMC Audio/Video buffers/records */
#define FLASH_AV_BUFFERS_OFFSET                         (0x0E1E2000)
#define FLASH_AV_BUFFERS_SIZE                           (0xF181E000)   


/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/_/ Individiual data detail
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/

/* Offsets relative to the Individual Data partition, generic ones */

#define FLASH_INDIVIDUAL_DATA_HDCP_KEYS_OFFSET          (0x00000190)    
#define FLASH_INDIVIDUAL_DATA_HDCP_1_4_KEYS_SIZE        (0x0000047C)    /* 1148B */
#define FLASH_INDIVIDUAL_DATA_HDCP_2_2_KEYS_SIZE        (0x00000440)    /* 1088B */
#define FLASH_INDIVIDUAL_DATA_HDCP_KEYS_SIZE            (FLASH_INDIVIDUAL_DATA_HDCP_1_4_KEYS_SIZE + FLASH_INDIVIDUAL_DATA_HDCP_2_2_KEYS_SIZE)
#include <plt/individual_data_mapping.h>
#define FLASH_INDIVIDUAL_DATA_MAC_ETH0                  (FLASH_INDIVIDUAL_DATA_MAC0)
#define FLASH_INDIVIDUAL_DATA_MAC_WIFI                  (FLASH_INDIVIDUAL_DATA_MAC1)
#define FLASH_INDIVIDUAL_DATA_MAC_BLUETOOTH             (FLASH_INDIVIDUAL_DATA_MAC2)

/* Offsets relative to the Individual Data partition, project specific ones */
#define FLASH_INDIVIDUAL_DATA_LOADER_DATA               (FLASH_INDIVIDUAL_DATA_PRODUCT_IDS)    /* 0x180, PT PV PR OUI */
#define FLASH_INDIVIDUAL_DATA_LOADER_DATA_SIZE          (0x00000010)
#define FLASH_INDIVIDUAL_DATA_LOADER_RESCUE             (0x00001000)    /* Rescue / Forced Loader mode parameters if none are valid */
#define FLASH_INDIVIDUAL_DATA_LOADER_RESCUE_SIZE        (0x00001000)

#define FLASH_INDIVIDUAL_DATA_SECURITY_ID               (0x00002000)
#define FLASH_INDIVIDUAL_DATA_SECURITY_ID_SIZE          (0x00000008)
#define FLASH_INDIVIDUAL_DATA_PERSONALIZATION           (0x00002008)
#define FLASH_INDIVIDUAL_DATA_PERSONALIZATION_SIZE      (0x00000800)

#define FLASH_INDIVIDUAL_DATA_CUSTOMER_IDS              (0x00002A00)
#define FLASH_INDIVIDUAL_DATA_CUSTOMER_IDS_SIZE         (0x00000010)     /* 16B   */

#define FLASH_INDIVIDUAL_DATA_GLOBAL_KEY_STORE          (0x00002C00)
#define FLASH_INDIVIDUAL_DATA_GLOBAL_KEY_STORE_SIZE     (0x00000050)
#define FLASH_INDIVIDUAL_DATA_NAGRA_PAIRING_KEYS        (0x00002E00)
#define FLASH_INDIVIDUAL_DATA_NAGRA_PAIRING_KEYS_SIZE   (0x00000404)     /* 1028 */
#define FLASH_INDIVIDUAL_DATA_NAGRA_CSCD                (0x00003204)
#define FLASH_INDIVIDUAL_DATA_NAGRA_CSCD_SIZE           (0x00000BB8)     /* 3000 */
#define FLASH_INDIVIDUAL_DATA_NAGRA_DB2_FACTORY         (0x00003DBC)
#define FLASH_INDIVIDUAL_DATA_NAGRA_DB2_FACTORY_SIZE    (0x00000400)     /* 1K   */
#define FLASH_INDIVIDUAL_DATA_NAGRA_DB3_FACTORY         (0x000041BC)
#define FLASH_INDIVIDUAL_DATA_NAGRA_DB3_FACTORY_SIZE    (0x0000E000)     /* 1K   */

/*
_/_/_/_/_/_/_/_/_/

_/_/_/_/_/_/_/_/
*/

/* Offsets relative to the SAV reserved area */
#define FLASH_SAV_BSSID_OFFSET                          (0x00000000) 
#define FLASH_SAV_DISPLAYTIMEON                         (0x00000020)

/*
_/_/_/_/_/_/_/_/_/
_/ RAM MAPPING _/
_/_/_/_/_/_/_/_/
*/

/* RAM Bootstrap (stage2) software */
#define RAM_BOOTSTRAP_OFFSET                            (0x07008000)
#define RAM_BOOTSTRAP_SIZE                              (0x00100000)  /* 1MB */

/* RAM Bootloader (stage3) software */
#define RAM_BOOTLOADER_OFFSET                           (0x27008000)
#define RAM_BOOTLOADER_SIZE                             (0x00100000)  /* 1MB */

/* RAM App SSV zone area */
#define RAM_APP_SSV_ZONE_OFFSET                         (0x28000000)  
#define RAM_APP_SSV_ZONE_SIZE                           (0x12C00000) /* 383M for both authentication and descryption*/

/* RAM Loader area */
#define RAM_LOADER_OFFSET                               (RAM_APP_SSV_ZONE_OFFSET + RAM_APP_SSV_ZONE_SIZE) /* 0x50000000 */
#define RAM_LOADER_SIZE                                 (FLASH_LOADER_SIZE)  /* 16MB */

/* RAM SSV DB2 */
#define RAM_NAGRA_SSV_DB2_OFFSET                        (0x3FFF1C00)
#define RAM_NAGRA_SSV_DB2_SIZE                          (0x00000400)

/* RAM SSV DB3 */
#define RAM_NAGRA_SSV_DB3_OFFSET                        (0x3FFF2000)
#define RAM_NAGRA_SSV_DB3_SIZE                          (0x0000E000)

/* Shared RAM area: corresponds to bmem.1, contains following areas such as PK, CSC, Exchange areas */ 
#define RAM_SHARED_MEMORY_OFFSET                        (0xBFF00000)
#define RAM_SHARED_MEMORY_SIZE                          (0x00100000)  /* 1MB */

/* RAM Exchange area */
#define RAM_EXCHANGE_OFFSET                             (0xBFFFF000)
#define RAM_EXCHANGE_SIZE                               (0x00001000)  /* 4KB */

/* RAM Nagra Pairing area */
#define RAM_NAGRA_PAIRING_OFFSET                        (0xBFFFE020)  /* skip 32 Bytes for alignement */
#define RAM_NAGRA_PAIRING_SIZE                          (0x00000FE0)  /* 4KB - 32B */

/* RAM Nagra CSC DATA */
#define RAM_NAGRA_CSC_DATA_OFFSET                       (0xBFFFD000)
#define RAM_NAGRA_CSC_DATA_SIZE                         (0x00001000)  /* 4KB */

/* RAM Loader Secure Downloading buffer */
#define RAM_SSD_BUFFER_OFFSET                           (RAM_APP_SSV_ZONE_OFFSET)
#define RAM_SSD_BUFFER_SIZE                             (FLASH_APP_SLOT_0_SIZE)

/*
_/_/_/_/_/_/_/_/_/_/_/_/
_/ AREA DESCRIPTIONS _/
_/_/_/_/_/_/_/_/_/_/_/
*/

#define MGC_BOOT          0x544F4F42    /* BOOT */
#define MGC_LAUNCHER      0x4E55414C    /* LAUN */
#define MGC_BL_STAGE4			0x34534C42		/* BLS4 */
#define MGC_LOADER        0x44414F4C    /* LOAD */
#define MGC_MAIN_APP      0x4E49414D    /* MAIN */
#define MGC_AFTER_SALES   0x41534641    /* AFSA */
#define MGC_CUST_TRACES   0x43415254    /* TRAC */
#define MGC_DLD           0x444C5744    /* DWLD */
#define MGC_SPLASH        0x414C5053    /* SPLA */

/** convenience macro for test programs */
#define MGC_TEXT(s) \
    (((s)==MGC_BOOT)        ? "Boot"              : \
     ((s)==MGC_LAUNCHER)    ? "Launcher"          : \
     ((s)==MGC_BL_STAGE4)   ? "BootLoader Stage4" : \
     ((s)==MGC_LOADER)      ? "Loader"            : \
     ((s)==MGC_MAIN_APP)    ? "Main Application"  : \
     ((s)==MGC_AFTER_SALES) ? "After Sales"       : \
     ((s)==MGC_CUST_TRACES) ? "Customer Trace"    : \
     ((s)==MGC_DLD)         ? "DLD"               : \
     ((s)==MGC_SPLASH)      ? "Splash"            : \
    "<Unknown>")

typedef enum {
  SIGN_KEY_NONE,
  SIGN_KEY_NAGRA_K3,
  SIGN_KEY_NAGRA_K4,
  SIGN_KEY_CUST_K5,
  SIGN_KEY_MAX,
} IMG_Compress_T;
/** convenience macro for test programs */
#define SIGN_KEY_TEXT(s) \
    (((s)==SIGN_KEY_NONE)     ? "None"        : \
     ((s)==SIGN_KEY_NAGRA_K3) ? "Nagra K3"    : \
     ((s)==SIGN_KEY_NAGRA_K4) ? "Nagra K4"    : \
     ((s)==SIGN_KEY_CUST_K5)  ? "Customer K5" : \
    "<Unknown>")

#pragma pack(1)
typedef struct module_header_s
{
	unsigned int    magic_number;
	unsigned int    version_id;
	unsigned int    binary_id;
	unsigned int    signing_key;
	unsigned char   reserved[10];
	unsigned short  crc;
} module_header_t;
#pragma pack()

/* Image header description */
#pragma pack(1)
typedef struct image_header_s
{
    unsigned int    magic_number;
    unsigned short  header_size;
    unsigned short  us_reserved;
    unsigned int    binary_id;        /* 6 digits: [(Year mod 100) * 10000 ] + [Week * 100] + [Increment_of_the_week] */
    unsigned int    copy_addr;
    unsigned int    entry_addr;
    unsigned int    image_size;
    unsigned int    body_size;
    unsigned int    init_ramfs_size;
    unsigned int    root_fs_size;
    unsigned int    cmdline_size;
    unsigned int    dtb_size;
    unsigned char   reserved[18];
    unsigned short  crc;
} image_header_t;
#pragma pack()

/*
_/_/_/_/_/_/_/_/_/_/_/_/
_/ MEMORY PARTITIONS _/
_/_/_/_/_/_/_/_/_/_/_/
*/
/* Partition table description */
typedef struct partition_table_s
{
    char*           name;
    char*           type;
    char*           dev_name;
    unsigned int    offset;
    unsigned int    size;
    unsigned int    flags;
} partition_table_t;

/* Former definitions, kept for convenience, should be removed in future and replaced by the definitions above in order to have a consistent naming */
#define FLASH_NVRAM                     (FLASH_NAND_START_ADDRESS + FLASH_NVRAM_OFFSET)

#define ADR_FLASH_INDIV_DATA            (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET)
#define ADR_FLASH_LDR_DATA              (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_LOADER_DATA)     /* OUI PT PV PR */
#define ADR_FLASH_CUST_DATA             (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_CUSTOMER_IDS)    /* Customer IDs */
#define ADR_FLASH_LDR_RESCUE            (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_LOADER_RESCUE)   /* OTP Rescue List */

/*SNOR flash mapping*/
#define ADR_FLASH_MACS                  (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_MAC_ETH0)
#define ADR_FLASH_NORFLAGS              (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_FACTORY_MODE)
#define ADR_FLASH_SSV_PK                (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_NAGRA_PAIRING_KEYS)
#define ADR_FLASH_SSV_CSCD              (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_NAGRA_CSCD)
#define ADR_FLASH_SSV_DB2FACTORY        (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_NAGRA_DB2_FACTORY)
#define ADR_FLASH_SSV_DB3FACTORY        (FLASH_SNOR_START_ADDRESS + FLASH_INDIVIDUAL_DATA_OFFSET + FLASH_INDIVIDUAL_DATA_NAGRA_DB3_FACTORY)
#define ADR_FLASH_LAUNCHER_VERS         (FLASH_SNOR_START_ADDRESS + 0x00100000)
#define ADR_FLASH_LOADER_BACKUP         (FLASH_SNOR_START_ADDRESS + FLASH_LOADER_BACKUP_OFFSET)
#define ADR_FLASH_SPLASH                (FLASH_SNOR_START_ADDRESS + FLASH_SPLASH_OFFSET)

/*Nand Flash mapping*/
#define ADR_FLASH_LOADER                (FLASH_NAND_START_ADDRESS + FLASH_LOADER_OFFSET)
#define ADR_FLASH_MAIN_APP              (FLASH_NAND_START_ADDRESS + FLASH_APP_SLOT_0_OFFSET)
#define ADR_FLASH_SLOT1_AFTERSALES      (FLASH_NAND_START_ADDRESS + FLASH_APP_SLOT_1_OFFSET)
#define ADR_FLASH_SSV_DB2               (FLASH_NAND_START_ADDRESS + FLASH_NAGRA_DB2_OFFSET)
#define ADR_FLASH_SSV_DB3               (FLASH_NAND_START_ADDRESS + FLASH_NAGRA_DB3_OFFSET)
#define ADR_FLASH_SSV_DB2ALTERNATE      (FLASH_NAND_START_ADDRESS + FLASH_NAGRA_DB2_BACKUP_OFFSET)
#define ADR_FLASH_SSV_DB3ALTERNATE      (FLASH_NAND_START_ADDRESS + FLASH_NAGRA_DB3_BACKUP_OFFSET)

/* RAM mapping */
#define ADR_RAM_EXCHANGE_AREA           (RAM_DDR_START_ADDRESS + RAM_EXCHANGE_OFFSET)
#define ADR_RAM_SHARE_SLOT_HEADER       (RAM_DDR_START_ADDRESS + RAM_SHARE_SLOT_HEADER_OFFSET)
#define ADR_RAM_SSV_PK                  (RAM_DDR_START_ADDRESS + RAM_NAGRA_PAIRING_OFFSET)
#define ADR_RAM_SSV_CSCD                (RAM_DDR_START_ADDRESS + RAM_NAGRA_CSC_DATA_OFFSET)
#define ADR_RAM_SSV_DB2                 (RAM_DDR_START_ADDRESS + RAM_NAGRA_SSV_DB2_OFFSET)
#define ADR_RAM_SSV_DB3                 (RAM_DDR_START_ADDRESS + RAM_NAGRA_SSV_DB3_OFFSET)
#define ADR_RAM_AUTHENTICATION          (RAM_DDR_START_ADDRESS + RAM_APP_SSV_ZONE_OFFSET)
#define ADR_RAM_DECRYPTION              (RAM_DDR_START_ADDRESS + RAM_APP_SSV_ZONE_OFFSET + RESERV_SIZE_SSV_DECRYPTION + 1024 - 4) /* Make cacheline match */

/*Area size*/
#define RESERV_SIZE_EXCHANGE_AREA       (RAM_EXCHANGE_SIZE) 
#define RESERV_SIZE_SSV_PK              (FLASH_INDIVIDUAL_DATA_NAGRA_PAIRING_KEYS_SIZE) /*768B*/
#define RESERV_SIZE_SSV_CSCD            (FLASH_INDIVIDUAL_DATA_NAGRA_CSCD_SIZE)
#define RESERV_SIZE_SSV_DB2             (FLASH_NAGRA_DB2_SIZE)
#define RESERV_SIZE_SSV_DB3             (FLASH_NAGRA_DB3_SIZE)
#define RESERV_SIZE_SW_STAGE3           (0x00060000)

#define RESERV_SIZE_SSV_DECRYPTION      (FLASH_APP_SLOT_0_SIZE)
#define RESERV_SIZE_SSV_AUTHENTICATION  (RESERV_SIZE_SSV_DECRYPTION)

#define RESERV_SIZE_LOADER              (FLASH_LOADER_SIZE)
#define RESERV_SIZE_LOADER_BK           (FLASH_LOADER_BACKUP_SIZE)
#define RESERV_SIZE_APP_SLOT            (FLASH_APP_SLOT_0_SIZE)        

/* Partition flag definitions; in order to make a partition appear, set the corresponding flag in the partition table below */
/* Partition flags for programs */ 
#define PARTITION_FLAG_BOOT                 (1 << 0)  /* partition dedicated to Boot */
#define PARTITION_FLAG_KERNEL               (1 << 1)  /* partition dedicated to KERNEL */
#define PARTITION_FLAG_USER                 (1 << 2)  /* partition dedicated to User App */
#define PARTITION_FLAG_LOADER               (1 << 3)  /* partition dedicated to User App */

/* Partition flags for environments */ 
#define PARTITION_FLAG_DEV                  (1 << 29)  /* partition in SW development context */
#define PARTITION_FLAG_PROD                 (1 << 30)  /* partition in production SW context */
#define PARTITION_FLAG_FACTORY              (1 << 31)  /* partition in factory SW context */

/* Partition flag group definitions */
#define PARTITION_FLAG_ALL_PROGS            (PARTITION_FLAG_BOOT | PARTITION_FLAG_KERNEL | PARTITION_FLAG_USER | PARTITION_FLAG_LOADER)
#define PARTITION_FLAG_ALL_ENVS             (PARTITION_FLAG_DEV | PARTITION_FLAG_PROD | PARTITION_FLAG_FACTORY)
#define PARTITION_FLAG_ALL                  (PARTITION_FLAG_ALL_PROGS | PARTITION_FLAG_ALL_ENVS)
#define PARTITION_FLAG_LAUNCHER             (PARTITION_FLAG_BOOT | PARTITION_FLAG_PROD)
#define FLASH_DB_AREA_OFFSET                (FLASH_NAGRA_DB2_OFFSET)
#define FLASH_DB_AREA_SIZE                  (FLASH_NAGRA_DB3_BACKUP_OFFSET - FLASH_NAGRA_DB2_OFFSET + FLASH_NAGRA_DB3_BACKUP_SIZE)

#if defined(_PART_C) || defined(MAPPING_C)
static partition_table_t partition[] = {
    { /* launcher      */ "boot",     	"SNOR", "/dev/mtdblock5", FLASH_BOOT_OFFSET,               FLASH_BOOT_SIZE,               PARTITION_FLAG_ALL },
    { /* RW SCS FW     */ "ext_fw", 	"SNOR", "/dev/mtdblock0", FLASH_SCS_FW_OFFSET,             FLASH_SCS_FW_SIZE,             PARTITION_FLAG_ALL },
    { /* rom data      */ "permdata", 	"SNOR", "/dev/mtdblock1", FLASH_INDIVIDUAL_DATA_OFFSET,    FLASH_INDIVIDUAL_DATA_SIZE,    PARTITION_FLAG_ALL },
    { /* splash screen */ "splash", 	"SNOR", "/dev/mtdblock2", FLASH_SPLASH_OFFSET,             FLASH_SPLASH_SIZE,             PARTITION_FLAG_ALL },
    { /* nvram for bolt*/ "bolt_nvram",	"SNOR", "/dev/mtdblock3", FLASH_BOLT_NVRAM_OFFSET,         FLASH_BOLT_NVRAM_SIZE,         PARTITION_FLAG_ALL },
    { /* loader backup */ "ldr_backup",	"SNOR", "/dev/mtdblock4", FLASH_LOADER_BACKUP_OFFSET,      FLASH_LOADER_BACKUP_SIZE,      PARTITION_FLAG_ALL },
    { /* DBx Area      */ "ssv_db",   	"EMMC", "",               FLASH_DB_AREA_OFFSET,            FLASH_DB_AREA_SIZE,            PARTITION_FLAG_ALL },
    { /* loader main   */ "loader",   	"EMMC", "/dev/mmcblk0p1", FLASH_LOADER_OFFSET,             FLASH_LOADER_SIZE,             PARTITION_FLAG_ALL },
    { /* application   */ "slot0" ,   	"EMMC", "/dev/mmcblk0p7", FLASH_APP_SLOT_0_OFFSET,         FLASH_APP_SLOT_0_SIZE,         PARTITION_FLAG_ALL },
    { /* fw nvram      */ "fw_nvram", 	"EMMC", "/dev/mmcblk0p6", FLASH_NVRAM_OFFSET,              FLASH_NVRAM_SIZE,              PARTITION_FLAG_ALL },
    { /* bg dld        */ "slot1" ,   	"EMMC", "/dev/mmcblk0p5", FLASH_APP_SLOT_1_OFFSET,         FLASH_APP_SLOT_1_SIZE,         PARTITION_FLAG_ALL }
};
#endif
#define FLASH_BOOT_DEVICE_PARTITION "/dev/mtdblock6"
#define FLASH_NAND_DEVICE_PARTITION "/dev/mmcblk0"
#endif /* MAPPING_H */

