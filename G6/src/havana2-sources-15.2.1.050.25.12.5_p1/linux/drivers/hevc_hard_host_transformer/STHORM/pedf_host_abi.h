/*
 * STHORM runtime ABI for the host
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

#ifndef __FC_PEDF_HOST_ABI_H__
#define __FC_PEDF_HOST_ABI_H__

#include <p2012_fabric_cfg.h> /* Memory map found with the HAL */
#include <rt_host_abi.h> /* Base-runtime host communication structures */

/**
 * \defgroup pedfAbi PEDF Host-Fabric communication ABI
 * @{
 */

/**
 * \brief PEDF runtime configuration arguments
 *
 * This strucutre is filled by the host and sent as argument to the
 * PEDF-runtime main entry point when the host calls it. The PEDF
 * runtime will finish to initialize the structure.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  uint32_t handler;     //!< PEDF-runtime handler, returned to to host once the runtime is ready
} pedf_runtime_info_t;

/**
 * \brief Lsit of commands that can be sent to the PEDF-runtime on the FC
 *
 * The requestID field of the command header p12_msgHeader_t
 * should be set to one of these values when sending a message to the
 * PEDF runtime
 */
typedef enum {
  PEDF_CMD_NEW_INSTANCE = 0,
  PEDF_CMD_EXEC = 1,
  PEDF_CMD_DESTROY = 2,
} pedf_commands_e;

/**
 * \brief PEDF new application instance command descriptor
 *
 * This structure is sent to create a new PEDF application instance.
 * This is a synchronous call.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  // Fields initialized by the caller
  p12_msgHeader_t hdr;     //!< Request header, must always be the first field
  uint32_t fcBinDesc;      //!< Handle of the FC binary descriptor that was previously loaded
  uint32_t fabricAddr;     //!< Physical address of this structure seen from the fabric controller
                           /*!< This field is used to link a run command (see pedf_run_t) with an
                                application instance. It is also used by the host to free the
                                instance struct when he is done.
                           */

  // Fields controller by he FC runtime
  uint32_t appDesc;        //!< Application descriptor, set and used by the pedf-runtime
  uint32_t pendingRuns;    //!< Used by the pedf-runtime to manage queued run commands
  uint32_t lastPendingRun; //!< Used by the pedf-runtime to manage queued run commands

  int32_t retval;         //!< Return value of the instantiate command, set by the pedf-runtime
} pedf_instance_t;

/**
 * \brief PEDF run command descriptor
 *
 * The run command is asynchronous and the return value is valid only once the
 * field 'ended' is set to 1 by the fabric controller.
 */
typedef struct P12_HOST_FABRIC_PACKED_DATA {
  // Fields initialized by the caller
  p12_msgHeader_t hdr;      //!< Request header, must always be the first field
  uint32_t instanceFabric;  //!< Address of the associated pedf_instance_t struct as seen in the fabric
  uint32_t fabricAddr;      //!< Physical address of this structure seen from the fabric (TODO: needed?)
  uint32_t attributes;      //!< Physical address pointing to the attributes of the top-controller
  int32_t callerWillWait;   //!< Indicates if the caller will wait on this run termination or not
  int16_t fabricDone;       //!< Must be initialize to 0 by the caller and will be set to 1 by the FC when done
  int16_t hostDone;         //!< Used by the host to log that this execution is completed when the interrupt is received

  // Fields controller by he FC runtime
  uint32_t next;            //!< Used by the PEDF-runtime to track the execution run queue
  int32_t retval;           //!< Return value of the executed PEDF application
} pedf_run_t;

/**@}*/

#endif /* __FC_PEDF_HOST_ABI_H__ */
