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
#ifndef _TM_UNIT_PARAMS_H
#define _TM_UNIT_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Types and defines:                                                         */
/*============================================================================*/


/******************************************************************************/
/** \brief "These macros map to tmUnitSelect_t variables parts"
*
******************************************************************************/

#define UNIT_VALID(_tUnIt)                      (((_tUnIt)&0x80000000) == 0)

#define UNIT_PATH_INDEX_MASK                    (0x0000001F)
#define UNIT_PATH_INDEX_POS                     (0)

#define UNIT_PATH_TYPE_MASK                     (0x000003E0)
#define UNIT_PATH_TYPE_POS                      (5)

#define UNIT_PATH_CONFIG_MASK                   (0x0003FC00)
#define UNIT_PATH_CONFIG_POS                    (10)

#define UNIT_SYSTEM_INDEX_MASK                  (0x007C0000)
#define UNIT_SYSTEM_INDEX_POS                   (18)

#define UNIT_SYSTEM_CONFIG_MASK                 (0x7F800000)
#define UNIT_SYSTEM_CONFIG_POS                  (23)




#define UNIT_PATH_INDEX_GET(_tUnIt) ((_tUnIt)&UNIT_PATH_INDEX_MASK)
#define UNIT_PATH_INDEX_VAL(_val) (((_val) << UNIT_PATH_INDEX_POS) \
							& UNIT_PATH_INDEX_MASK)
#define UNIT_PATH_INDEX_SET(_tUnIt, _val) (((_tUnIt) & ~UNIT_PATH_INDEX_MASK) \
						| UNIT_PATH_INDEX_VAL(_val))
#define UNIT_PATH_INDEX_VAL_GET(_val) (UNIT_PATH_INDEX_VAL( \
						UNIT_PATH_INDEX_GET(_val)))

#define UNIT_PATH_TYPE_GET(_tUnIt) (((_tUnIt) & UNIT_PATH_TYPE_MASK) >> \
							UNIT_PATH_TYPE_POS)
#define UNIT_PATH_TYPE_VAL(_val) (((_val) << UNIT_PATH_TYPE_POS) & \
							UNIT_PATH_TYPE_MASK)
#define UNIT_PATH_TYPE_SET(_tUnIt, _val) (((_tUnIt) & ~UNIT_PATH_TYPE_MASK) | \
						UNIT_PATH_TYPE_VAL(_val))
#define UNIT_PATH_TYPE_VAL_GET(_val) (UNIT_PATH_TYPE_VAL( \
						UNIT_PATH_TYPE_GET(_val)))


#define UNIT_PATH_CONFIG_GET(_tUnIt) (((_tUnIt) & UNIT_PATH_CONFIG_MASK) >> \
							UNIT_PATH_CONFIG_POS)
#define UNIT_PATH_CONFIG_VAL(_val) (((_val) << UNIT_PATH_CONFIG_POS) & \
							UNIT_PATH_CONFIG_MASK)
#define UNIT_PATH_CONFIG_SET(_tUnIt, _val) (((_tUnIt) & ~UNIT_PATH_CONFIG_MASK)\
						| UNIT_PATH_CONFIG_VAL(_val))
#define UNIT_PATH_CONFIG_VAL_GET(_val) (UNIT_PATH_CONFIG_VAL( \
						UNIT_PATH_CONFIG_GET(_val)))

#define UNIT_SYSTEM_INDEX_GET(_tUnIt) (((_tUnIt) & UNIT_SYSTEM_INDEX_MASK) >> \
							UNIT_SYSTEM_INDEX_POS)
#define UNIT_SYSTEM_INDEX_VAL(_val) (((_val) << UNIT_SYSTEM_INDEX_POS) & \
							UNIT_SYSTEM_INDEX_MASK)
#define UNIT_SYSTEM_INDEX_SET(_tUnIt, _val) (((_tUnIt) & \
						~UNIT_SYSTEM_INDEX_MASK) | \
						UNIT_SYSTEM_INDEX_VAL(_val))
#define UNIT_SYSTEM_INDEX_VAL_GET(_val) (UNIT_SYSTEM_INDEX_VAL( \
						UNIT_SYSTEM_INDEX_GET(_val)))

#define UNIT_SYSTEM_CONFIG_GET(_tUnIt) (((_tUnIt) & UNIT_SYSTEM_CONFIG_MASK) \
						>> UNIT_SYSTEM_CONFIG_POS)
#define UNIT_SYSTEM_CONFIG_VAL(_val) (((_val) << UNIT_SYSTEM_CONFIG_POS) & \
							UNIT_SYSTEM_CONFIG_MASK)
#define UNIT_SYSTEM_CONFIG_SET(_tUnIt, _val) (((_tUnIt) & \
						~UNIT_SYSTEM_CONFIG_MASK) | \
						UNIT_SYSTEM_CONFIG_POS(_val))
#define UNIT_SYSTEM_CONFIG_VAL_GET(_val) (UNIT_SYSTEM_CONFIG_VAL( \
						UNIT_SYSTEM_CONFIG_GET(_val)))


#define GET_WHOLE_SYSTEM_TUNIT(_tUnIt) (UNIT_SYSTEM_CONFIG_VAL_GET(_tUnIt) | \
					UNIT_SYSTEM_INDEX_VAL_GET(_tUnIt))

#define GET_SYSTEM_TUNIT(_tUnIt) (UNIT_SYSTEM_CONFIG_VAL_GET(_tUnIt) | \
					UNIT_SYSTEM_INDEX_VAL_GET(_tUnIt) | \
					UNIT_PATH_INDEX_VAL_GET(_tUnIt))

#define GET_INDEX_TUNIT(_tUnIt) (UNIT_SYSTEM_INDEX_VAL_GET(_tUnIt) | \
						UNIT_PATH_INDEX_VAL_GET(_tUnIt))

#define GET_INDEX_TYPE_TUNIT(_tUnIt) (UNIT_SYSTEM_INDEX_VAL_GET(_tUnIt) | \
					UNIT_PATH_INDEX_VAL_GET(_tUnIt) | \
					UNIT_PATH_TYPE_VAL_GET(_tUnIt))

#define XFER_DISABLED_FLAG                      (UNIT_PATH_CONFIG_VAL(0x80))
#define GET_XFER_DISABLED_FLAG_TUNIT(_tUnIt) (((_tUnIt)&XFER_DISABLED_FLAG) \
									!= 0)


/*============================================================================*/


#ifdef __cplusplus
}
#endif

#endif /* _TM_UNIT_PARAMS_H */
