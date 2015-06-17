/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_list.h
   @brief  List Manipulation Functions

   This file contains functions for creating, freeing and manipulating lists.  These functions help
   manage the list, growing it as necessary, and working out where to put new items.

   Lists are not objects in the pti object manager sense.

 */

#ifndef _PTI_LIST_H_
#define _PTI_LIST_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

/* Lists have 12 bytes of overhead */
typedef struct {
	U16 MaxItems;			/**< List capacity */
	U16 UsedItems;			/**< Actual items in the list */
	U16 FirstFreeItem;		/**< First free space in the list */
	U16 StartOfFreeSpace;		/**< Next free space at the end of the list */
	void **Item_p;			/**< Pointer to the list itself */
} List_t;

/* Exported Function Prototypes -------------------------------------------- */

#define stptiOBJMAN_GetItemFromList(List_p, index)   (((index) < (List_p)->MaxItems) ? ((List_p)->Item_p[index]) : NULL )
#define stptiOBJMAN_GetNumberOfItemsInList(List_p)   ((List_p)->UsedItems)
#define stptiOBJMAN_GetListCapacity(List_p)          ((List_p)->MaxItems)

ST_ErrorCode_t stptiOBJMAN_AllocateList(List_t *List_p);
ST_ErrorCode_t stptiOBJMAN_AllocateSizedList(List_t *List_p, unsigned int size);

ST_ErrorCode_t stptiOBJMAN_ResizeList(List_t *List_p, unsigned int size);

ST_ErrorCode_t stptiOBJMAN_AddToList(List_t *List_p, void *ItemToAdd_p, BOOL Append, int *index_p);
ST_ErrorCode_t stptiOBJMAN_RemoveFromList(List_t *List_p, int index);

ST_ErrorCode_t stptiOBJMAN_ReplaceItemInList(List_t *List_p, int index, void *NewItem_p);

ST_ErrorCode_t stptiOBJMAN_DefragmentList(List_t *List_p);

ST_ErrorCode_t stptiOBJMAN_FirstInList(List_t *List_p, void **Item_p, int *index_p);
ST_ErrorCode_t stptiOBJMAN_NextInList(List_t *List_p, void **Item_p, int *index);

ST_ErrorCode_t stptiOBJMAN_SearchListForItem(List_t *List_p, void *Item_p, int *index_p);

ST_ErrorCode_t stptiOBJMAN_DeallocateList(List_t *List_p);

void stptiOBJMAN_PrintList(List_t *List_p);

#endif /* _PTI_LIST_H_ */
