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
   @file   pti_list.c
   @brief  List Manipulation Functions

   This file contains functions for creating, freeing and manipulating lists.  These functions help
   manage the list, growing it as necessary, and working out where to put new items.

   NOTE:  List only automatically grow.  They don't automatically shrink.

   Lists are not objects in the pti object manager sense.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"

/* Includes from the HAL / ObjMan level */
#include "pti_object.h"
#include "pti_list.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
#define STPTI_DEFAULT_LIST_SIZE                     8
#define STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD       1024

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
static ST_ErrorCode_t stptiOBJMAN_EnlargeList(List_t *List_p);

/* Functions --------------------------------------------------------------- */

/**
   @brief  Allocate memory for a List, using a default size.

   This function allocates memory for a list, and initialises it.

   @param  List_p             A pointer to the List

   @return                    A standard st error type...
                              - ST_NO_ERROR if no errors
                              - ST_ERROR_BAD_PARAMETER if the List_p is invalid
                              - ST_ERROR_NO_MEMORY if there is no memory
 */
ST_ErrorCode_t stptiOBJMAN_AllocateList(List_t *List_p)
{
	return (stptiOBJMAN_AllocateSizedList(List_p, STPTI_DEFAULT_LIST_SIZE));
}

/**
   @brief  Allocate memory for a List, and set it's initial size.

   This function allocates memory for a list (in terms of items), and initialises it.  The list will
   automatically grow, should the list become too small.

   @param  List_p             A pointer to the list
   @param  size               The initial capacity (in terms of items) for the list

   @return                    A standard st error type...
                              - ST_NO_ERROR if no errors
                              - ST_ERROR_BAD_PARAMETER if the List_p is invalid
                              - ST_ERROR_NO_MEMORY if there is no memory
 */
ST_ErrorCode_t stptiOBJMAN_AllocateSizedList(List_t *List_p, unsigned int size)
{
	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	/* create list */
	List_p->Item_p = stptiOBJMAN_AllocateMemory(size * sizeof(void *));
	if (List_p->Item_p == NULL) {
		return (ST_ERROR_NO_MEMORY);
	} else {
		stpti_printf(">> MALLOCed %ubytes (%p)", (unsigned)(size * sizeof(void *)), List_p->Item_p);
	}
	memset(List_p->Item_p, 0, size * sizeof(void *));
	List_p->MaxItems = size;
	List_p->UsedItems = 0;
	List_p->FirstFreeItem = 0;
	List_p->StartOfFreeSpace = 0;
	return (ST_NO_ERROR);
}

/**
   @brief  Resize an existing list to a new size.

   This function resizes a list to a smaller or larger size.  It does not check to see that
   the new list size is large enough for all the Items in the list (those items will just be
   "lost").  It also does not defragment the list.

   @param  List_p     A pointer to the List
   @param  Size       The new size.

   @return            A standard st error type...
                      - ST_NO_ERROR if no errors
                      - ST_ERROR_BAD_PARAMETER if the List_p is invalid
                      - ST_ERROR_NO_MEMORY if there is no memory
 */
ST_ErrorCode_t stptiOBJMAN_ResizeList(List_t *List_p, unsigned int size)
{
	if ((List_p == NULL) || (size < 1)) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if ((List_p->MaxItems == 0) || (List_p->Item_p == NULL)) {
		return (ST_ERROR_BAD_PARAMETER);
	} else if (size != List_p->MaxItems) {
		void **p;

		p = stptiOBJMAN_AllocateMemory(size * sizeof(void *));
		if (p == NULL) {
			return (ST_ERROR_NO_MEMORY);
		} else {
			stpti_printf(">> MALLOCed %ubytes (%p)", (unsigned)(size * sizeof(void *)), p);
		}
		if (size > List_p->MaxItems) {
			/* growing list */
			memcpy(p, List_p->Item_p, List_p->MaxItems * sizeof(void *));
			memset(p + List_p->MaxItems, 0, (size - List_p->MaxItems) * sizeof(void *));
		} else {
			/* reducing list */
			memcpy(p, List_p->Item_p, size * sizeof(void *));
		}
		stptiOBJMAN_DeallocateMemory((void *)List_p->Item_p, (List_p->MaxItems) * sizeof(void *));
		stpti_printf(">> FREEed (%p)", List_p->Item_p);
		List_p->Item_p = p;
		List_p->MaxItems = size;
		if (List_p->FirstFreeItem > List_p->MaxItems) {
			List_p->FirstFreeItem = List_p->MaxItems;
		}
		if (List_p->StartOfFreeSpace > List_p->MaxItems) {
			List_p->StartOfFreeSpace = List_p->MaxItems;
		}
	}
	return (ST_NO_ERROR);
}

/**
   @brief  Enlarge a List for adding new items (private function)

   This function enlarges a list by doubling it.  If the number of items is above a set threshold
   (STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD) it will then just increase it linearly.

   @param  List_p     A pointer to the List needing enlarging.

   @return            A standard st error type...
                      - ST_NO_ERROR if no errors
                      - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
static ST_ErrorCode_t stptiOBJMAN_EnlargeList(List_t *List_p)
{
	unsigned int new_size;

	if (List_p->MaxItems < STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD) {
		/* If less than STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD items, just double the length of the list */
		new_size = List_p->MaxItems * 2;
	} else {
		/* For longer lists, we just add STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD items */
		new_size = List_p->MaxItems + STPTI_DEFAULT_LIST_DOUBLING_THRESHOLD;
	}

	return (stptiOBJMAN_ResizeList(List_p, new_size));
}

/**
   @brief  Add an Item to a list.

   This function adds the specified Item to the list.

   @param  List_p         A pointer to the List
   @param  ItemToAdd_p    A pointer to the Item to add
   @param  Append         TRUE if it must be added at the end, otherwise it will add it to wherever
                          there is room
   @param  index_p        A pointer to an int to return the index of the item added.

   @return            A standard st error type...
                      - ST_NO_ERROR if no errors
                      - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_AddToList(List_t *List_p, void *ItemToAdd_p, BOOL Append, int *index_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int i;

	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	/* Check to make sure List is allocated */
	if ((List_p->MaxItems == 0) || (List_p->Item_p == NULL)) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (Append) {
		/* Need to add item to the end */

		/* Check there is space */
		if (List_p->StartOfFreeSpace >= List_p->MaxItems) {
			/* Need to grow list */
			Error = stptiOBJMAN_EnlargeList(List_p);
			if (Error != ST_NO_ERROR) {
				return (Error);
			}
		}

		if (index_p != NULL) {
			*index_p = List_p->StartOfFreeSpace;
		}

		List_p->Item_p[List_p->StartOfFreeSpace] = ItemToAdd_p;
		if (List_p->FirstFreeItem == List_p->StartOfFreeSpace) {
			/* We just filled a item into where FirstFreeItem was point too */
			/* need to increment it too. */
			List_p->FirstFreeItem++;
		}
		List_p->StartOfFreeSpace++;
	} else {
		/* Can add item anywhere */

		/* Check there is space */
		if (List_p->FirstFreeItem >= List_p->MaxItems) {
			/* Need to grow list */
			Error = stptiOBJMAN_EnlargeList(List_p);
			if (Error != ST_NO_ERROR) {
				return (Error);
			}
		}

		if (index_p != NULL) {
			*index_p = List_p->FirstFreeItem;
		}
		List_p->Item_p[List_p->FirstFreeItem] = ItemToAdd_p;
		for (i = List_p->FirstFreeItem; i < List_p->MaxItems; i++) {
			if (List_p->Item_p[i] == NULL)
				break;
		}

		if (List_p->FirstFreeItem == List_p->StartOfFreeSpace) {
			List_p->StartOfFreeSpace++;
		}

		List_p->FirstFreeItem = i;
	}

	List_p->UsedItems++;

	return (Error);
}

/**
   @brief  Replace an Item in a list.

   This function replaces the indexed Item in the list with another.

   @param  List_p     A pointer to the List
   @param  index      The index of the Item to remove
   @param  NewItem_p  The new item to replace the old one with.

   @return            A standard st error type...
                      - ST_NO_ERROR if no errors
                      - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */

ST_ErrorCode_t stptiOBJMAN_ReplaceItemInList(List_t * List_p, int index, void *NewItem_p)
{
	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (index >= List_p->MaxItems) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (List_p->Item_p[index] == NULL) {
		return (ST_ERROR_INVALID_HANDLE);
	} else {
		List_p->Item_p[index] = NewItem_p;
	}
	return (ST_NO_ERROR);
}

/**
   @brief  Remove an Item from a list.

   This function adds the indexed Item from the list.

   @param  List_p  A pointer to the List
   @param  index   The index of the Item to remove

   @return         A standard st error type...
                   - ST_NO_ERROR if no errors
                   - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_RemoveFromList(List_t * List_p, int index)
{
	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (index >= List_p->MaxItems) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (List_p->Item_p[index] == NULL) {
		return (ST_ERROR_INVALID_HANDLE);
	} else {
		List_p->Item_p[index] = NULL;
		List_p->UsedItems--;
	}

	if (index < List_p->FirstFreeItem) {
		List_p->FirstFreeItem = index;
	}

	if (index + 1 == List_p->StartOfFreeSpace) {
		/* Windback StartOfFreeSpace, and keep doing whilst we are finding NULLs */
		do {
			List_p->StartOfFreeSpace--;
		} while ((List_p->StartOfFreeSpace > 1) && (List_p->Item_p[List_p->StartOfFreeSpace - 1] == NULL));
	}
	return (ST_NO_ERROR);
}

/**
   @brief  Defragment a list (remove NULLs from it).

   This function moves up an Items after one or more NULLs.

   @param  List_p  A pointer to the List

   @return         A standard st error type...
                   - ST_NO_ERROR if no errors
                   - ST_ERROR_BAD_PARAMETER if the List_p is invalid

   @warning CURRENTLY NOT IMPLEMENTED
 */
ST_ErrorCode_t stptiOBJMAN_DefragmentList(List_t * List_p)
{
	/* Implement if needed */
	List_p = List_p;	/* Prevent Warning */
	return (ST_NO_ERROR);
}

/**
   @brief  Return the first Item in the list.

   This function return the first Item in the list and its index.

   @param  List_p    A pointer to the List
   @param  Item_p    A pointer to the Item pointer
   @param  index_p   A pointer to the index of the Item returned

   @return         A standard st error type...
                   - ST_NO_ERROR if no errors
                   - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_FirstInList(List_t * List_p, void **Item_p, int *index_p)
{
	*index_p = -1;
	return (stptiOBJMAN_NextInList(List_p, Item_p, index_p));
}

/**
   @brief  Return the next Item in the list.

   This function return the next Item in the list and its index.  The integer pointed to by index_p
   was the index of last item found.

   @param  List_p    A pointer to the List
   @param  Item_p    A pointer to the Item pointer
   @param  index_p   On entry, the int pointed to is the index of the last Item found.
                     On exit, the int pointed to is index of Item returned (or -1 if nothing found).

   @return         A standard st error type...
                   - ST_NO_ERROR if no errors
                   - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_NextInList(List_t * List_p, void **Item_p, int *index_p)
{
	int i, starting_index = *index_p;

	*index_p = -1;
	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if ((List_p->MaxItems == 0) || (List_p->Item_p == NULL)) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (starting_index < 0) {
		starting_index = -1;
	}

	starting_index++;

	for (i = starting_index; i < List_p->StartOfFreeSpace; i++) {
		if (List_p->Item_p[i] != NULL) {
			/* Found an item */
			*Item_p = List_p->Item_p[i];
			break;
		}
	}

	if (i < List_p->StartOfFreeSpace) {
		/* Found something */
		*index_p = i;
	} else {
		/* Hit end of list */
		*Item_p = NULL;
	}
	return (ST_NO_ERROR);
}

/**
   @brief  Search a List for a specified item, and return the index to it.

   This function search a list for a specified item, and return the index to it.  -1 is returned if
   the item is not found.

   @param  List_p    A pointer to the List
   @param  Item_p    The item to look for (a pointer)
   @param  index_p   On entry, the int pointed to is the index of the last Item found.
                     On exit, the int pointed to is index of Item returned (or -1 if nothing found).

   @return           A standard st error type...
                     - ST_NO_ERROR if no errors
                     - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_SearchListForItem(List_t * List_p, void *Item_p, int *index_p)
{
	int i, starting_index = *index_p;

	*index_p = -1;

	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if ((List_p->MaxItems == 0) || (List_p->Item_p == NULL)) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (starting_index < 0) {
		starting_index = -1;
	}

	starting_index++;

	for (i = starting_index; i < List_p->StartOfFreeSpace; i++) {
		if (List_p->Item_p[i] == Item_p) {
			*index_p = i;
			break;
		}
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Deallocates the list.

   This function deallocates the memory used for a list.

   @param  List_p    A pointer to the List

   @return         A standard st error type...
                   - ST_NO_ERROR if no errors
                   - ST_ERROR_BAD_PARAMETER if the List_p is invalid
 */
ST_ErrorCode_t stptiOBJMAN_DeallocateList(List_t * List_p)
{
	if (List_p == NULL) {
		return (ST_ERROR_BAD_PARAMETER);
	}
	if ((List_p->MaxItems > 0) && (List_p->Item_p != NULL)) {
		stptiOBJMAN_DeallocateMemory((void *)List_p->Item_p, List_p->MaxItems * sizeof(void *));
		stpti_printf(">> FREEed (%p)", List_p->Item_p);
	}
	List_p->Item_p = NULL;
	List_p->MaxItems = 0;
	return (ST_NO_ERROR);
}

/**
   @brief  A debug function for printing out a list.

   This function prints out a list's contents (including NULLs).  This function will be optimised
   away if STPTI_PRINT is not defined.

   @param  List_p  A pointer to the List to be displayed

   @return         nothing (to permit optimisation)

 */
void stptiOBJMAN_PrintList(List_t * List_p)
{
#if defined( STPTI_PRINT )
	int i, count;
	char string[256];
	const char len = sizeof(string);

	count =
	    scnprintf(string, len, "Items=%d/%d  FirstFree=%d  StartOfFreeSpace=%d", List_p->UsedItems,
		      List_p->MaxItems, List_p->FirstFreeItem, List_p->StartOfFreeSpace);
	for (i = 0; i < List_p->MaxItems; i++) {
		if (i % 8 == 0) {
			stpti_printf("%s", string);
			count = scnprintf(string, len, "%3d: ", i);
		}
		count += scnprintf(string + count, len - count, "%p ", List_p->Item_p[i]);
	}
	stpti_printf("%s", string);
#else
	List_p = List_p; /* Prevent Warning */
#endif
}
