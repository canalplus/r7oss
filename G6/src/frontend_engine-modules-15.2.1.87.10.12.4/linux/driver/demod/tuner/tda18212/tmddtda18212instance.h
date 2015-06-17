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
#ifndef _TMDDTDA182I2_INSTANCE_H	/* ----------------- */
#define _TMDDTDA182I2_INSTANCE_H

tmErrorCode_t ddTDA182I2AllocInstance(tmUnitSelect_t tUnit,
				      pptmddTDA182I2Object_t ppDrvObject);
tmErrorCode_t ddTDA182I2DeAllocInstance(tmUnitSelect_t tUnit);
tmErrorCode_t ddTDA182I2GetInstance(tmUnitSelect_t tUnit,
				    pptmddTDA182I2Object_t ppDrvObject);

#endif /* _TMDDTDA182I2_INSTANCE_H //--------------- */
