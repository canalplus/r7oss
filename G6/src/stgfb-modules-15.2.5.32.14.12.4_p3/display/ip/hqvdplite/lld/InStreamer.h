#ifndef _INSTREAMER_H_
#define _INSTREAMER_H_

/*------------------------------------------------------------------------------
 * Defines: TRADUCTION DIRECT DU DECODAGE INTERNE DU STREAMER
 *----------------------------------------------------------------------------*/

#define IN_STREAMER_REGS_OFFSET			0x0000
#define IN_STREAMER_RD_PLUG_OFFSET		0xA000
#define IN_STREAMER_WR_PLUG_OFFSET		0x8000


#define     IN_STREAMER_CONTROL_REG_OFFSET		      0x00001000	   /*  Read STbus plug control */
#define     IN_STREAMER_PAGE_SIZE_REG_OFFSET		      0x00001004	   /*  Read STbus plug page size */
#define     IN_STREAMER_MIN_OPC_REG_OFFSET		      0x00001008	   /*  Read STbus plug MINimum Opcode */
#define     IN_STREAMER_MAX_OPC_REG_OFFSET		      0x0000100c	   /*  Read STbus plug MAXimum Opcode */
#define     IN_STREAMER_MAX_CHK_REG_OFFSET		      0x00001010	   /*  Read STbus plug MAXimum Chunk size */
#define     IN_STREAMER_MAX_MSG_REG_OFFSET		      0x00001014	   /*  Read STbus plug MAXimum message size */
#define     IN_STREAMER_MIN_SPACE_REG_OFFSET		      0x00001018	   /*  Read STbus plug minimum space between reques */

/*  To be removed */
#define     HQR_CONTROL                    IN_STREAMER_CONTROL_REG_OFFSET          /*  Read STbus plug control */
#define     HQR_PAGE_SIZE                  IN_STREAMER_PAGE_SIZE_REG_OFFSET        /*  Read STbus plug page size */
#define     HQR_MIN_OPC                    IN_STREAMER_MIN_OPC_REG_OFFSET          /*  Read STbus plug MINimum Opcode */
#define     HQR_MAX_OPC                    IN_STREAMER_MAX_OPC_REG_OFFSET          /*  Read STbus plug MAXimum Opcode  */
#define     HQR_MAX_CHK                    IN_STREAMER_MAX_CHK_REG_OFFSET          /*  Read STbus plug MAXimum Chunk size */
#define     HQR_MAX_MSG                    IN_STREAMER_MAX_MSG_REG_OFFSET          /*  Read STbus plug MAXimum message size */
#define     HQR_MIN_SPACE                  IN_STREAMER_MIN_SPACE_REG_OFFSET        /*  Read STbus plug minimum space between reques */


#endif
