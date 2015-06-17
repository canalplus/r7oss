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
/**
     @file   tmNxCompId.h
     @brief  Brief description of this h file.

     @b  Component:  n/a

     This is a temporary workaround to assist integration of STB810 components
     into the STB225 project. STB810 uses the newer 'tmNxCompId.h' naming
     convention, while existing STB225 include 'tmCompId.h'.

     Set your editor for 4 space indentation.
  *//*

    Rev Date        Author      Comments
--------------------------------------------------------------------------------
    001 20060605    G.Lawrance  Original
--------------------------------------------------------------------------------
    For consistency and standardisation retain the Section Separators.
*/
#ifndef TMNXCOMPID_H
#define TMNXCOMPID_H

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------- */
/* Project include files:                                                     */
/* -------------------------------------------------------------------------- */
#include "tmcompid.h"

/* Map tmNxCompId.h onto tmCompId.h */
#define CID_STBSTREAMINGSYSTEM CID_COMP_STBSTREAMINGSYSTEM


#ifdef __cplusplus
}
#endif

#endif /* TM_NX_COMP_ID_H */
