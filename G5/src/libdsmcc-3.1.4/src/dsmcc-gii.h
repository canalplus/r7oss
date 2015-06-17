#ifndef DSMCC_GII
#define DSMCC_GII

struct dsmcc_group_list
{
	uint32_t id;
	uint32_t size;
	int parsed;

	struct dsmcc_group_list *next;
};

int dsmcc_group_info_indication_parse(struct dsmcc_group_list **groups, uint8_t *data, int data_length);
void dsmcc_group_info_indication_free(struct dsmcc_group_list *groups);

#endif

