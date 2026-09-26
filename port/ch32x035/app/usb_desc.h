/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.h
 * Description        : USB CDC device descriptor definitions, ported from the
 *                      WCH EVT SimulateCDC example.
 *******************************************************************************/

#ifndef USER_USB_DESC_H_
#define USER_USB_DESC_H_

#include "debug.h"

/* file version */
#define DEF_FILE_VERSION             0x11
/* usb device info define  */
#define DEF_USB_VID                  0x1A86
#define DEF_USB_PID                  0xFE0C
/* USB device descriptor, device serial number(bcdDevice) */
#define DEF_IC_PRG_VER               DEF_FILE_VERSION

/* usb device endpoint size define */
#define DEF_USBD_UEP0_SIZE           64     /* usb fs device end-point 0 size */
#define DEF_USBD_FS_PACK_SIZE        64     /* usb fs device max bulk/int pack size */
#define DEF_USBD_FS_ISO_PACK_SIZE    1023   /* usb fs device max iso pack size */
#define DEF_USBD_LS_UEP0_SIZE        8      /* usb ls device end-point 0 size */
#define DEF_USBD_LS_PACK_SIZE        64     /* usb ls device max int pack size */

#define DEF_USBD_ENDP1_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP2_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP3_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP4_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP5_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP6_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USBD_ENDP7_SIZE          DEF_USBD_FS_PACK_SIZE

/* descriptor lengths, 0 if the descriptor does not exist */
#define DEF_USBD_DEVICE_DESC_LEN     ((uint8_t)MyDevDescr[0])
#define DEF_USBD_CONFIG_DESC_LEN     ((uint16_t)MyCfgDescr[2] + (uint16_t)(MyCfgDescr[3] << 8))
#define DEF_USBD_REPORT_DESC_LEN     0
#define DEF_USBD_LANG_DESC_LEN       ((uint16_t)MyLangDescr[0])
#define DEF_USBD_MANU_DESC_LEN       ((uint16_t)MyManuInfo[0])
#define DEF_USBD_PROD_DESC_LEN       ((uint16_t)MyProdInfo[0])
#define DEF_USBD_SN_DESC_LEN         ((uint16_t)MySerNumInfo[0])

/* string descriptor indexes are provided by ch32x035_usb.h
 * (DEF_STRING_DESC_LANG/MANU/PROD/SERN) */

/* external variables */
extern const uint8_t MyDevDescr[];
extern const uint8_t MyCfgDescr[];
extern const uint8_t MyLangDescr[];
extern const uint8_t MyManuInfo[];
extern const uint8_t MyProdInfo[];
extern const uint8_t MySerNumInfo[];

#endif /* USER_USB_DESC_H_ */
