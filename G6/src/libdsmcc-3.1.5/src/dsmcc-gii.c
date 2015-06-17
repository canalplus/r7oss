#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "dsmcc.h"
#include "dsmcc-util.h"
#include "dsmcc-gii.h"


void dsmcc_group_info_indication_free(struct dsmcc_group_list *groups)
{
	struct dsmcc_group_list *pg;
	while(groups)
	{
		pg = groups->next;
		free(groups);
		groups = pg;
	}

}

static int gii_parse_group(struct dsmcc_group_list *group, uint8_t *data, int data_lenght)
{
	uint16_t compat_descriptor_lenght;
	uint16_t group_info_length;
	uint16_t private_lenght;
	int off = 0;
	if(!dsmcc_getlong(&group->id, data, 0, data_lenght))
		return -1;
	if(!dsmcc_getlong(&group->size, data, 4, data_lenght))
		return -1;

	if(!dsmcc_getshort(&compat_descriptor_lenght, data, 8, data_lenght))
		return -1;
	if(compat_descriptor_lenght != 0)
	{
		DSMCC_WARN("group contains a compatibility descriptor but it's unhandled");
	}
	off += 10 + compat_descriptor_lenght;

	if(!dsmcc_getshort(&group_info_length, data, off, data_lenght))
		return -1;
	if(group_info_length != 0)
	{
		DSMCC_WARN("group contains group info but it's unhandled");
	}
	off += 2 + group_info_length;

	if(!dsmcc_getshort(&private_lenght, data, off, data_lenght))
		return -1;

	return off + 2 + private_lenght;
}

int dsmcc_group_info_indication_parse(struct dsmcc_group_list **groups, uint8_t *data, int data_length)
{
	uint16_t num_groups;
	int off = 0, res, i;
	struct dsmcc_group_list **current = groups;

	if(!dsmcc_getshort(&num_groups, data, 0, data_length))
		return -1;
	off += 2;

	for(i = 0; i < num_groups; i++)
	{
		*current = malloc(sizeof(struct dsmcc_group_list));
		memset(*current, 0, sizeof(struct dsmcc_group_list));
		res = gii_parse_group(*current, data+off, data_length-off);
		if(res < 0)
		{
			DSMCC_ERROR("GroupInfoIndication parse error");
			dsmcc_group_info_indication_free(*groups);
			return -1;
		}
		current = &(*current)->next;
		off += res;
	}

	return 0;
}

