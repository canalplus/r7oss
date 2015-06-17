/*
 * Runtime configuration structure
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *          Germain Haugou (germain.haugou@st.com)
 *
 */

#ifndef _P12_HOST_FABRIC_ABI_H_
#define _P12_HOST_FABRIC_ABI_H_

#include "rt_host_def.h"

/**
 * \defgroup hostAbi Host ABI
 * @{
 */

#ifndef P12_HOST_FABRIC_PACKED_DATA
#define P12_HOST_FABRIC_PACKED_DATA __attribute__((packed, aligned(4)))
#endif

/**
 * \brief Identifiers for the various parts of the system
 */
typedef enum p12_target {
  P12_TARGET_HOST = 0,
  P12_TARGET_FC = 1,
  P12_TARGET_CLUSTER0 = 2,
  P12_TARGET_CLUSTER1 = 3,
  P12_TARGET_CLUSTER2 = 4,
  P12_TARGET_CLUSTER3 = 5,
} p12_target_e;

/**
 * \brief List of functions supported by the base runtime
 *
 * These functions can be called through a message sent in the mailbox.
 * Warning : the order and values of this enum must match the baseRuntimeItf[]
 *           array indexes found in comete.c
 */
typedef enum p12_runtime_itf {
  P12_LOAD_BINARY = 0,
  P12_CLOSE_BINARY = 1,
  P12_CALLMAIN = 2,
  P12_CLUSTER_ALLOC = 3, // TODO: Not implemented yet !
} p12_runtime_itf_e;

/**
 * \brief Memory bank identifier
 *
 */
typedef enum {
  P12_MEMBANK_ENCMEM = 0, //!< L1: EnCore memory inside a cluster
  P12_MEMBANK_FABMEM = 1, //!< L2: Fabric memory shared between clusters
  P12_MEMBANK_SOCMEM = 2, //!< L3: SoC memory ouside of P2012 (host memory)
  P12_MEMBANK_TCDM = 3,   //!< Private memory: FC Tightly Coupled Data Memory
  P12_MEMBANK_INVALID,
} p12_memBank_e;

/**
 * \brief Pointer shared between host and fabric
 *
 * The STxP70 processors are 32bit in the P2012 fabric.
 * For the host we try to make no assumptions if the processor is 32 or 64 bits.
 */
typedef union P12_HOST_FABRIC_PACKED_DATA {
    uint64_t val64;
    uint32_t val32[2];
} p12_hostFabric_ptr;

/**
 * \brief Header for all message exchanged using the mailbox
 *
 * The payload of the message is assumed to follow the header in memory.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {

  uint32_t fabricHandler;   //!< Function pointer to the handler of the message on the STxP70
                            /*!< If set to NULL(0), the default handler of the base-runtime will be
                                 called. This field can be used to send a message to a programming model
                                 specific runtime layer which executes on top of the base-runtime.
                            */
  uint32_t requestID;       //!< Request ID used by the handler to know what action to take
                            /*!< For the base-runtime (when fabricHandler == 0), the value of this field
                                 must be initialized with the p12_runtime_itf_e enumeration.
                            */


  p12_hostFabric_ptr replyCallback; //!< function pointer set by the caller and returned as-is in the response
                                    /*!< This field can be used by the caller to take the proper
                                         action when the reply is received.
                                    */
  uint32_t replyID; //!< Unique reply identifier set by the caller and returned as-is in the response
                    /*!< The replyID will be the payload written in the mailbox when the reply is sent.
                         If the calling host CPU is 32-bit this can be the address of the reply
                         structure message in the host memory space for simplicity. If the
                         caller is a 64-bit CPU then he needs to manage unique IDs and their
                         corresponding reply structures because the FC mailbox support 32bit
                         message payload only.
                    */

  uint16_t sender;      //!< Sender ID as defined in p12_target_e
  uint16_t receiver;    //!< Destination ID as defined in p12_target_e

} p12_msgHeader_t;

/**
 * \brief Load binary request message
 *
 * The caller must fill all fields execpt 'binDesc' and 'result' which
 * are provided as a return values. The binary descriptor 'binDesc' is
 * valid only if result is 0.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  p12_msgHeader_t hdr;  //!< Request header, must always be the first field

  uint32_t buff;            //!< Address of the buffer containing the binary
  uint32_t buffSize;        //!< Size of the binary
  uint32_t parentDesc;      //!< Parent dynamic library that was already loaded
                            /*!< This parameter can be 0. It is usefull if loading
                                 a sequence of binaries that have dependencies, the
                                 dynamic linker will try to resolve undefined symbols
                                 in the parent binary.
                            */
  uint32_t errorAddr;       //!< Buffer to store error messages (can be 0/NULL)
  uint32_t errorSize;       //!< Mazimum size of the error message buffer (can be 0)
  uint32_t name;            //!< Pointer to a NULL terminated string containing the name of the binary
                            /*!< This parameter can be 0, but if it is not set the debugger will
                                 not be able to find the symbols it needs when GDB is connected
                                 to the STxP70
                             */
  uint16_t textMemBank;     //!< Memory to use for the code (.text) as defined in p12_memBank_e
  uint16_t dataMemBank;     //!< Memory to use for the data (.data, .bss) as defined in p12_memBank_e

  int32_t clusterId;       //!< Cluster on which the binary will execute

  uint32_t binDesc;         //!< Binary descriptor (handle) of the loaded code
  int32_t result;          //!< Status of the load binary request

} p12_loadBinaryMsg_t;

/**
 * \brief Close binary request command
 *
 * Ask the runtime to unload and free the resources associated to a binary
 * that was previously loaded.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  p12_msgHeader_t hdr;  //!< Request header, must always be the first field
  uint32_t binDesc;     //!< Binary descriptor (handle) on the fabric
   int32_t result;      //!< Return value, 0 means the operation succedded
} p12_closeBinaryMsg_t;

/**
 * \brief Call Main request command
 *
 * Call the main entry point of a binary that was previously loaded.
 * The main function name and signature is :
 *      int entry(void*)
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  p12_msgHeader_t hdr;  //!< Request header, must always be the first field
  uint32_t binDesc;     //!< Binary descriptor (handle) that contains the main function to execute
  uint32_t arg;         //!< Single argument received as a void* pointer by the main function
   int32_t result;      //!< Return value of the main function
} p12_callMainMsg_t;

/**@}*/

#endif /* _P12_HOST_FABRIC_ABI_H_ */
