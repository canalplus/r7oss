/************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Debug macro translation.

************************************************************************/

#include "hpi.h"
#include "hpios.h"
#include "hpidebug.h"

/* Debug level; 0 quiet; 1 informative, 2 debug, 3 verbose debug.  */
int hpiDebugLevel = HPI_DEBUG_LEVEL_DEFAULT;

void HPI_DebugInit(
	void
)
{
	printk(DBG_TEXT("Debug Start\n"));
}

int HPI_DebugLevelSet(
	int level
)
{
	int old_level;

	old_level = hpiDebugLevel;
	hpiDebugLevel = level;
	return old_level;
}

int HPI_DebugLevelGet(
	void
)
{
	return (hpiDebugLevel);
}

#ifdef HPIOS_DEBUG_PRINT
/* implies OS has no printf-like function */
#include <stdarg.h>

void hpi_debug_printf(
	DBG_CHAR * fmt,
	...
)
{
	va_list arglist;
	DBG_CHAR buffer[128];

	va_start(arglist, fmt);

	if (buffer[0])
		HPIOS_DEBUG_PRINT(buffer);
	va_end(arglist);
}
#endif

struct treenode {
	void *array;
	unsigned int numElements;
};

#define make_treenode_from_array(nodename, array) \
static void *tmp_strarray_##nodename[] = array; \
static struct treenode nodename = { \
	&tmp_strarray_##nodename, \
	ARRAY_SIZE(tmp_strarray_##nodename) \
};

#define get_treenode_elem(node_ptr, idx, type)	\
	(&(*((type *)(node_ptr)->array)[idx]))

make_treenode_from_array(hpi_subsys_strings, HPI_SUBSYS_STRINGS)
	make_treenode_from_array(hpi_adapter_strings, HPI_ADAPTER_STRINGS)
	make_treenode_from_array(hpi_istream_strings, HPI_ISTREAM_STRINGS)
	make_treenode_from_array(hpi_ostream_strings, HPI_OSTREAM_STRINGS)
	make_treenode_from_array(hpi_mixer_strings, HPI_MIXER_STRINGS)
	make_treenode_from_array(hpi_node_strings,
	{
	DBG_TEXT("NODE is invalid object")})

	make_treenode_from_array(hpi_control_strings, HPI_CONTROL_STRINGS)
	make_treenode_from_array(hpi_nvmemory_strings, HPI_OBJ_STRINGS)
	make_treenode_from_array(hpi_digitalio_strings, HPI_DIGITALIO_STRINGS)
	make_treenode_from_array(hpi_watchdog_strings, HPI_WATCHDOG_STRINGS)
	make_treenode_from_array(hpi_clock_strings, HPI_CLOCK_STRINGS)
	make_treenode_from_array(hpi_profile_strings, HPI_PROFILE_STRINGS)
	make_treenode_from_array(hpi_asyncevent_strings, HPI_ASYNCEVENT_STRINGS)
#define HPI_FUNCTION_STRINGS \
{ \
  &hpi_subsys_strings,\
  &hpi_adapter_strings,\
  &hpi_ostream_strings,\
  &hpi_istream_strings,\
  &hpi_mixer_strings,\
  &hpi_node_strings,\
  &hpi_control_strings,\
  &hpi_nvmemory_strings,\
  &hpi_digitalio_strings,\
  &hpi_watchdog_strings,\
  &hpi_clock_strings,\
  &hpi_profile_strings,\
  &hpi_control_strings, \
  &hpi_asyncevent_strings \
};
	make_treenode_from_array(hpi_function_strings, HPI_FUNCTION_STRINGS)

	compile_time_assert(HPI_OBJ_MAXINDEX == 14, obj_list_doesnt_match);

static DBG_CHAR *hpi_function_string(
	unsigned int function
)
{
	unsigned int object;
	struct treenode *tmp;

	object = function / HPI_OBJ_FUNCTION_SPACING;
	function = function - object * HPI_OBJ_FUNCTION_SPACING;

	if (object == 0 || object == HPI_OBJ_NODE
		|| object > hpi_function_strings.numElements)
		return DBG_TEXT("Invalid object");

	tmp = get_treenode_elem(&hpi_function_strings, object - 1,
		struct treenode *);

	if (function == 0 || function > tmp->numElements)
		return DBG_TEXT("Invalid function");

	return get_treenode_elem(tmp, function - 1, DBG_CHAR *);
}

void hpi_debug_message(
	struct hpi_message *phm,
	DBG_CHAR * szFileline
)
{
	if (phm) {
		if ((phm->wObject <= HPI_OBJ_MAXINDEX) && phm->wObject) {
			u16 wIndex = 0;
			u16 wAttrib = 0;

			switch (phm->wObject) {
			case HPI_OBJ_ADAPTER:
			case HPI_OBJ_PROFILE:
				break;
			case HPI_OBJ_MIXER:
				if (phm->wFunction ==
					HPI_MIXER_GET_CONTROL_BY_INDEX)
					wIndex = phm->u.m.wControlIndex;
				break;
			case HPI_OBJ_OSTREAM:
			case HPI_OBJ_ISTREAM:
				wIndex = phm->u.d.wStreamIndex;
				break;
			case HPI_OBJ_CONTROLEX:
				if (phm->wFunction == HPI_CONTROL_GET_INFO)
					wIndex = phm->u.c.wControlIndex;
				else {
					wIndex = phm->u.cx.wControlIndex;
					wAttrib = phm->u.cx.wAttribute;
				}
				break;
			case HPI_OBJ_CONTROL:
				wIndex = phm->u.c.wControlIndex;
				wAttrib = phm->u.c.wAttribute;
				break;
			default:
				break;
			}
			printk(DBG_TEXT
				("%s Adapter %d %s param x%08x \n"),
				szFileline, phm->wAdapterIndex,
				hpi_function_string(phm->
					wFunction), (wAttrib << 16) + wIndex);
		} else {
			printk(DBG_TEXT
				("Adap=%d, Invalid Obj=%d, Func=%d\n"),
				phm->wAdapterIndex, phm->wObject,
				phm->wFunction);
		}
	} else
		printk(DBG_TEXT
			("NULL message pointer to hpi_debug_message!\n"));
}

void hpi_debug_data(
	u16 *pdata,
	u32 len
)
{
	u32 i;
	int j;
	int k;
	int lines;
	int cols = 8;

	lines = (len + cols - 1) / cols;
	if (lines > 8)
		lines = 8;

	for (i = 0, j = 0; j < lines; j++) {
		printk(DBG_TEXT("%p:"), (pdata + i));

		for (k = 0; k < cols && i < len; i++, k++)
			printk(DBG_TEXT("%s%04x"),
				k == 0 ? "" : " ", pdata[i]);

		printk(DBG_TEXT("\n"));
	}
}
