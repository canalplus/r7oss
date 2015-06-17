/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.



This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.



This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.



You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
/* --------------------------------------------------------------------------
 * FILE NAME:    tmbslTDA182I2Instance.h
 *
 * DESCRIPTION:  define the static Objects
 *
 * DOCUMENT REF: DVP Software Coding Guidelines v1.14
 * DVP Board Support Library Architecture Specification v0.5
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
#ifndef _TMBSLTDA182I2_INSTANCE_H	/* ----------------- */
#define _TMBSLTDA182I2_INSTANCE_H

tmErrorCode_t TDA182I2AllocInstance(tmUnitSelect_t tUnit,
				    pptmTDA182I2Object_t ppDrvObject);
tmErrorCode_t TDA182I2DeAllocInstance(tmUnitSelect_t tUnit);
tmErrorCode_t TDA182I2GetInstance(tmUnitSelect_t tUnit,
				  pptmTDA182I2Object_t ppDrvObject);

#endif /* _TMBSLTDA182I2_INSTANCE_H //--------------- */
