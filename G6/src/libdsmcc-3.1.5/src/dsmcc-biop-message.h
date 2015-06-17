#ifndef DSMCC_BIOP_MESSAGE_H
#define DSMCC_BIOP_MESSAGE_H

#include "dsmcc-cache.h"

/* a directory entry (either file or dir depending on 'dir' flag) */
struct biop_msg_dentry
{
	bool                   dir;
	struct dsmcc_object_id id;
	char                  *name;

	struct biop_msg_dentry *next;
};

/* a directory */
struct biop_msg_dir
{
	bool                   gateway;
	struct dsmcc_object_id id;

	struct biop_msg_dentry *first_dentry, *last_dentry;
};

/* a file */
struct biop_msg_file
{
	struct dsmcc_object_id id;

	char                  *data_file;
	int                    data_offset;
	int                    data_length;
};

enum
{
	BIOP_MSG_DIR = 0,
	BIOP_MSG_FILE
};

struct biop_msg
{
	int type;

	union
	{
		struct biop_msg_dir  dir;
		struct biop_msg_file file;
	} msg;

	struct biop_msg *next;
};

int dsmcc_biop_msg_parse_data(struct biop_msg **messages, struct dsmcc_module_id *module_id, const char *module_file, int length);
void dsmcc_biop_msg_free_all(struct biop_msg *messages);

#endif
