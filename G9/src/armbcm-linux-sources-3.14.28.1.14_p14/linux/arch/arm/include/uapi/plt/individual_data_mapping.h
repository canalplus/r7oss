/* FIXME header */
#ifndef _INDIVIDUAL_DATA_MAPPING_H_
#define _INDIVIDUAL_DATA_MAPPING_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_INDIVIDUAL_DATA_MAC_NUM               (11)

#define FLASH_INDIVIDUAL_DATA_MB_SERIAL_NB          (0x00000000)
#define FLASH_INDIVIDUAL_DATA_BOX_SERIAL_NB         (0x00000040)
#define FLASH_INDIVIDUAL_DATA_TOCOM_NB              (0x00000080)
#define FLASH_INDIVIDUAL_DATA_MAC0                  (0x00000090)
#define FLASH_INDIVIDUAL_DATA_MAC1                  (0x000000A0)
#define FLASH_INDIVIDUAL_DATA_MAC2                  (0x000000B0)
#define FLASH_INDIVIDUAL_DATA_MAC3                  (0x000000C0)
#define FLASH_INDIVIDUAL_DATA_MAC4                  (0x000000D0)
#define FLASH_INDIVIDUAL_DATA_MAC5                  (0x000000E0)
#define FLASH_INDIVIDUAL_DATA_MAC6                  (0x000000F0)
#define FLASH_INDIVIDUAL_DATA_MAC7                  (0x00000100)
#define FLASH_INDIVIDUAL_DATA_MAC8                  (0x00000110)
#define FLASH_INDIVIDUAL_DATA_MAC9                  (0x00000120)
#define FLASH_INDIVIDUAL_DATA_MAC10                 (0x00000130)
#define FLASH_INDIVIDUAL_DATA_FACTORY_USED          (0x00000140)
#define FLASH_INDIVIDUAL_DATA_FACTORY_MODE          (0x00000170)
#define FLASH_INDIVIDUAL_DATA_PRODUCT_IDS           (0x00000180)
#define FLASH_INDIVIDUAL_DATA_HDCP_KEYS             (0x00000190)

#define FLASH_INDIVIDUAL_DATA_MB_SERIAL_NB_SIZE     (0x00000040) /* 64 bytes */
#define FLASH_INDIVIDUAL_DATA_BOX_SERIAL_NB_SIZE    (0x00000040) /* 64 bytes */
#define FLASH_INDIVIDUAL_DATA_TOCOM_NB_SIZE         (0x00000010) /* 16 bytes */
#define FLASH_INDIVIDUAL_DATA_MAC_SIZE              (0x00000010) /* 16 bytes */
#define FLASH_INDIVIDUAL_DATA_FACTORY_USED_SIZE     (0x00000030) /* 48 bytes */
#define FLASH_INDIVIDUAL_DATA_FACTORY_MODE_SIZE     (0x00000010) /* 16 bytes */
#define FLASH_INDIVIDUAL_DATA_PRODUCT_IDS_SIZE      (0x00000010) /* 16 bytes */

#ifndef FLASH_INDIVIDUAL_DATA_HDCP_KEYS_SIZE
#define FLASH_INDIVIDUAL_DATA_HDCP_KEYS_SIZE        (0x00000E70) /* 3696 bytes */
#warning You SHOULD define the size of the zone containing the HDCP keys in mapping.h prior to including this file!
#endif

#pragma pack(1)
typedef struct individual_data_s
{
    const unsigned char mb_serial_nb[FLASH_INDIVIDUAL_DATA_MB_SERIAL_NB_SIZE];
    const unsigned char box_serial_nb[FLASH_INDIVIDUAL_DATA_BOX_SERIAL_NB_SIZE];
    const unsigned char tocom_nb[FLASH_INDIVIDUAL_DATA_TOCOM_NB_SIZE];
    const unsigned char macs[FLASH_INDIVIDUAL_DATA_MAC_NUM][FLASH_INDIVIDUAL_DATA_MAC_SIZE];
    const unsigned char factory_area[FLASH_INDIVIDUAL_DATA_FACTORY_USED_SIZE];
    const unsigned char factory_mode[FLASH_INDIVIDUAL_DATA_FACTORY_MODE_SIZE];
    const unsigned char product_ids[FLASH_INDIVIDUAL_DATA_PRODUCT_IDS_SIZE];
    const unsigned char hdcp_keys[FLASH_INDIVIDUAL_DATA_HDCP_KEYS_SIZE];
} individual_data_t;

struct product_ids {
   const unsigned short pt;
   const unsigned short pv;
   const unsigned short pr;
   const unsigned int oui;
};
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INDIVIDUAL_DATA_MAPPING_H_ */


