/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#ifndef __STM_CONTROL_DATA_HELPER_H
#define __STM_CONTROL_DATA_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif


// Market type definition
#define STM_CONTROL_DATA_CMD_TIMING_REQUEST             0x00    // Request for a time alarm (PTS based) from the stream player
#define STM_CONTROL_DATA_BREAK_FORWARD                  0x01    // Forward injection break
#define STM_CONTROL_DATA_BREAK_BACKWARD                 0x02    // Backward injection break
#define STM_CONTROL_DATA_BREAK_BACKWARD_SMOOTH_REVERSE  0x03    // Backward injection break (allowing smooth reverse)
#define STM_CONTROL_DATA_BREAK_END_OF_STREAM                0x04    // Break because of end of stream.

// Additionnal tag "OR'ed" to previous control datas
#define STM_CONTROL_DATA_ORedTAG_LAST_FRAME_COMPLETE        0x80    // Tag indicating the last fram is complete and is to be decoded


// Control data size
#define STM_CONTROL_DATA_TS_SIZE                            188     // Size of basic MPEG2-TS packet.
#define STM_CONTROL_DATA_PES_SIZE                            26     // Size of basic MPEG2-PES packet.

#define STM_CONTROL_DATA_USER_DATA_SIZE                 10      // Maximum user data size.


// ---------------------------
// Data structure definitions.

/* PES Packet */
typedef struct stm_pes_control_data_s
{
    uint8_t     packet_start_code_prefix[3];    /* Normalized standard value :              (00 00 01h) */
    uint8_t     stream_id;                      /* ST's PES control data stream_id              (FBh)       */
    uint8_t     PES_packet_length[2];           /* ST's PES control data length                 (00 14h)    */
    uint8_t     byte7;                          /* ST's PES control data 7th byte with:                     */
    /*  byte7.7 and .6 = '01'                   (01)        */
    /*  byte7.5 and .4 PES_scrambling_control   (00)        */
    /*  byte7.3 = PES_priority                  (0)         */
    /*  byte7.2 = data_alignment_indicator      (0)         */
    /*  byte7.1 = copyright                     (0)         */
    /*  byte7.0 = original_or_copy              (0)         */
    uint8_t     byte8;                          /* ST's PES control data 8th byte with:                     */
    /*  byte7.7 and .6 = PTS_DTS_flags          (00)        */
    /*  byte7.5 = ESCR_flag                     (0)         */
    /*  byte7.4 = ES_rate_flag                  (0)         */
    /*  byte7.3 = DSM_trick_mode_flag           (0)         */
    /*  byte7.2 = additional_copy_info_flag     (0)         */
    /*  byte7.1 = PES_CRC_flag                  (0)         */
    /*  byte7.0 = PES_extension_flag            (1)         */
    uint8_t     PES_header_data_length;         /* ST's PES control data data length                (11h)       */
    uint8_t     byte10;                         /* ST's PES control data 10th byte with:                        */
    /*  byte10.7 = PES_private_data_flag        (1)         */
    /*  byte10.6 = pack_header_field_flag       (0)         */
    /*  byte10.5 = program_packet_seq_cnt_flag  (0)         */
    /*  byte10.4 = PSTD_buffer_flag             (0)         */
    /*  byte10.3 to .1 = reserved               (000)       */
    /*  byte10.0 = PES_extension_flag_2         (0)         */
    uint8_t     PES_private_data[16];               /* ST's PES control data pattern and user data with:            */
    /*  PES_private_data[0..3] = control data pattern   ("STMM")    */
    /*  PES_private_data[4] = control data type     (...)       */
    /*  PES_private_data[5..14] = user data     (...)       */
    /*  PES_private_data[15] = user data size   (...)       */
} stm_pes_control_data_t;


/* Adaptation field */
typedef struct stm_ts_adaptation_field_s
{
    uint8_t                     adaptation_field_length;        /* ST's control data value:                                     (0x9D)                  */
    uint8_t                     byte2;                          /* ST's control data adaptation_field second byte with:                             */
    /*  byte2.7 = discontinuity_indicator                   (1)                     */
    /*  byte2.6 = random_access_indicator                   (0)                     */
    /*  byte2.5 = elementary__stream_priority_indicator     (0)                     */
    /*  byte2.4 = PCR_flag                                  (0)                     */
    /*  byte2.3 = OPCR_flag                                 (0)                     */
    /*  byte2.2 = splicing_point_flag                       (0)                     */
    /*  byte2.1 = transport_private_data_flag               (1)                     */
    /*  byte2.0 = adaptation_field_extension_flag           (0)                     */
    uint8_t                     transport_private_data_length;  /* ST's control data private data length                        (10h)                   */
    uint8_t                     private_data_byte[16];          /* ST's control data pattern                                    ("STM FakeTSPacket")    */
    uint8_t                     Stuffing_byte[139];             /* ST's control data stuffing bytes                             (0xff)) */
} stm_ts_adaptation_field_t;

/* MPEG2-TS Control data packet. */
typedef struct stm_ts_control_data_s
{
    uint8_t                     sync_byte;          /* Normalized standard value : 47h                                      */
    uint8_t                     pid_h;              /* User defined pid (msb) of channel to be considered, but with:        */
    /*  pid_h.7 = transport_error_indicator                                 */
    /*  pid_h.6 = payload_unit_start_indicator                              */
    /*  pid_h.5 = transport_priority                                        */
    uint8_t                     pid_l;              /* User defined pid (msb) of channel to be considered.                  */
    uint8_t                     byte4;              /* Fourth byte of TS control data packet with:                              */
    /*  ts_byte_4.7 and .6 = adaptation_field_control                       */
    /*  ts_byte_4.5 and .4 = transport_scrambling_indicator                 */
    /*  ts_byte_4.3 to .0  = continuity_counter                             */
    stm_ts_adaptation_field_t   adaptation_field;   /* MPEG2-System TS adaptation_field                                     */
    stm_pes_control_data_t          PES_packet;         /* MPEG2-system PES_packet.                                             */
} stm_ts_control_data_t;


// Local static variables.

// Default TS control data content
static uint8_t static_ts_control_data[STM_CONTROL_DATA_TS_SIZE] =
{
    /*                  |<--- adatpation  S    T    M        F     a    k    e   T  */
    0x47, 0x40, 0x00, 0x30, 0x9D, 0x82, 0x10, 0x53, 0x54, 0x4D, 0x20, 0x46, 0x61, 0x6B, 0x65, 0x54,
    /* S    P   a    c   k    e    t <---------------- stuffing bytes--------------*/
    0x53, 0x50, 0x61, 0x63, 0x6B, 0x65, 0x74, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*-----------------------------------------------------------------------------*/
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /*----- >|<-------------------- PES Packets -----------------------------------*/
    0xFF, 0xFF, 0x00, 0x00, 0x01, 0xFB, 0x00, 0x14, 0x80, 0x01, 0x11, 0x80, 0x53, 0x54, 0x4D, 0x4D,
    /*-------------------------------------------------------->*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



/*
/// \fn int             int stm_ts_control_data_create (
                                uint8_t         control_data_type,
                                void            *address,
                                uint16_t        size,
                                uint16_t        *control_data_size,
                                const uint16_t  pid,
                                const uint8_t   *user_data,
                                const uint8_t   user_data_size);
///
/// \Description This function is creating a ST propriatary MPEG2-TS control data.
///
/// \param control_data_type:   Kind of control data to be created.
/// \param address:         Address in memory the control data pattern should be copied into.
/// \param size:            Size in memory available for the pattern copy (min is STM_CONTROL_DATA_TS_SIZE)
/// \param control_data_size:   Pointer to return the size effectively written at address.
/// \param pid:             PID of the TS channel the TS control data should be inserted into.
/// \param user_data:       Pointer to user Application data to be associated to the control data.
/// \param user_data_size:  Size of user Application data (max is STM_CONTROL_DATA_USER_DATA_SIZE)
///
/// \retval -1      Error when creating the control data
/// \retval 0       Success.
*/
static inline int stm_ts_control_data_create(
    uint8_t         control_data_type,
    void            *address,
    uint16_t        size,
    uint16_t        *control_data_size,
    const uint16_t  pid,
    const uint8_t   *user_data,
    const uint8_t   user_data_size)
{
    // Locally used variables.
    stm_ts_control_data_t *ts_control_data_p = (stm_ts_control_data_t *)static_ts_control_data;

    // Check input parameters
    if (address == NULL || control_data_size == NULL || user_data == NULL)
    {
        return -1;
    }

    if (size < STM_CONTROL_DATA_TS_SIZE || user_data_size > STM_CONTROL_DATA_USER_DATA_SIZE)
    {
        return -1;
    }

    // Ok, now build the control data.
    // Set the PID of the TS channel
    ts_control_data_p->pid_h = (ts_control_data_p->pid_h & 0xC0) | ((pid & 0x1f00) >> 8);
    ts_control_data_p->pid_l = pid & 0x00ff;
    // Set-up the control_data type.
    ts_control_data_p->PES_packet.PES_private_data[4]       = control_data_type;
    // Set-up the size of user data.
    ts_control_data_p->PES_packet.PES_private_data[15]  = user_data_size;

    if (user_data_size != 0)
        // Copy user data if relevant (position 15 in field private_data)
    {
        memcpy(&ts_control_data_p->PES_packet.PES_private_data[5], user_data, user_data_size);
    }

    // Finally, copy back the TS control data pattern into caller's buffer.
    memcpy(address, ts_control_data_p, STM_CONTROL_DATA_TS_SIZE);
    // Update control data effective size.
    *control_data_size = STM_CONTROL_DATA_TS_SIZE;
    return 0;
} /* End of stm_ts_control_data_create() function */


/*
/// \fn int             int stm_pes_control_data_create (
                                uint8_t         control_data_type,
                                void            *address,
                                uint16_t        size,
                                uint16_t        *control_data_size,
                                const uint8_t   *user_data,
                                const uint8_t   user_data_size);
///
/// \Description This function is creating a ST propriatary MPEG2-PES control data.
///
/// \param control_data_type:   Kind of control data to be created.
/// \param address:         Address in memory the control data pattern should be copied into.
/// \param size:            Size in memory available for the pattern copy (min is STM_CONTROL_DATA_PES_SIZE)
/// \param control_data_size:   Pointer to return the size effectively written at address.
/// \param user_data:       Pointer to user Application data to be associated to the control data.
/// \param user_data_size:  Size of user Application data (max is STM_CONTROL_DATA_USER_DATA_SIZE)
///
/// \retval -1      Error when creating the control data
/// \retval 0       Success.
*/
static inline int stm_pes_control_data_create(
    uint8_t         control_data_type,
    void            *address,
    uint16_t        size,
    uint16_t        *control_data_size,
    const uint8_t   *user_data,
    const uint8_t   user_data_size)
{
    // Catch-up directly the position of the PES packet within the static ts control data.
    stm_pes_control_data_t *pes_control_data_p = (stm_pes_control_data_t *) & (((stm_ts_control_data_t *)(static_ts_control_data))->PES_packet);

    // Check input parameters
    if (address == NULL || control_data_size == NULL || user_data == NULL)
    {
        return -1;
    }

    if (size < STM_CONTROL_DATA_PES_SIZE || user_data_size > STM_CONTROL_DATA_USER_DATA_SIZE)
    {
        return -1;
    }

    // Set the control data type.
    pes_control_data_p->PES_private_data[4]     = control_data_type;
    // Set-up the size of user data.
    pes_control_data_p->PES_private_data[15]        = user_data_size;

    if (user_data_size != 0)
        // Copy user data if relevant (position 15 in field private_data)
    {
        memcpy(&pes_control_data_p->PES_private_data[5], user_data, user_data_size);
    }

    // Finally, copy back the TS control data pattern into caller's buffer.
    memcpy(address, pes_control_data_p, STM_CONTROL_DATA_PES_SIZE);
    // Update control data effective size.
    *control_data_size = STM_CONTROL_DATA_PES_SIZE;
    return 0;
} /* End of stm_pes_control_data_create() function */

#ifdef __cplusplus
}
#endif

#endif  // __STM_CONTROL_DATA_HELPER_H
