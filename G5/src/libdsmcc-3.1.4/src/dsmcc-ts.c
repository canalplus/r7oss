#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dsmcc/dsmcc-tsparser.h>
#include "dsmcc.h"
#include "dsmcc-debug.h"
#include "dsmcc-util.h"
#include "dsmcc-ts.h"

#define SYNC_BYTE       0x47
#define TRANSPORT_ERROR 0x80
#define START_INDICATOR 0x40

void dsmcc_tsparser_add_pid(struct dsmcc_tsparser_buffer **buffers, uint16_t pid)
{
	struct dsmcc_tsparser_buffer *buf;

	/* check that we do not already track this PID */
	for (buf = *buffers; buf != NULL; buf = buf->next)
	{
		if (buf->pid == pid)
			return;
	}

	/* create and register PID buffer */
	buf = malloc(sizeof(struct dsmcc_tsparser_buffer));
	buf->pid = pid;
	buf->si_seen = 0;
	buf->in_section = 0;
	buf->cont = -1;
	buf->next = *buffers;
	*buffers = buf;
	DSMCC_DEBUG("Created buffer for PID 0x%x", pid);
}

void dsmcc_tsparser_free_buffers(struct dsmcc_tsparser_buffer **buffers)
{
	struct dsmcc_tsparser_buffer *buffer = *buffers;

	while (buffer)
	{
		struct dsmcc_tsparser_buffer *bufnext = buffer->next;
		free(buffer);
		buffer = bufnext;
	}

	*buffers = NULL;
}

void dsmcc_tsparser_parse_packet(struct dsmcc_state *state, struct dsmcc_tsparser_buffer **buffers, uint8_t *packet, int packet_length)
{
	struct dsmcc_tsparser_buffer *buf;
	uint16_t pid;
	int cont;

	if (packet_length <= 0 || packet_length != 188)
	{
		DSMCC_WARN("Skipping packet: Invalid packet size (got %d, expected 188)", packet_length);
		return;
	}

	if (!packet)
	{
		DSMCC_WARN("Skipping NULL packet");
		return;
	}

	if (packet[0] != SYNC_BYTE)
	{
		DSMCC_WARN("Skipping packet: Invalid sync byte: got 0x%02hhx, expected 0x%02hhx", *packet, SYNC_BYTE);
		return;
	}

	/* Test if error bit is set */
	if (packet[1] & TRANSPORT_ERROR)
	{
		DSMCC_WARN("Skipping packet: Error bit is set");
		return;
	}

	pid = ((packet[1] & 0x1F) << 8) | packet[2];

	/* Find correct buffer for stream */
	for (buf = *buffers; buf != NULL; buf = buf->next)
	{
		if (buf->pid == pid)
			break;
	}
	if (buf == NULL)
		return;

	/* Test if start on new dsmcc_section */
	cont = packet[3] & 0x0F;

	if (buf->cont == 0xF && cont == 0)
	{
		buf->cont = 0;
	}
	else if (buf->cont + 1 == cont)
	{
		buf->cont++;
	}
	else if (buf->cont == -1)
	{
		buf->cont = cont;
	}
	else
	{
		/* Out of sequence packet, drop current section */
		DSMCC_WARN("Packet out of sequence (cont=%d, buf->cont=%d), resetting", cont, buf->cont);
		buf->in_section = 0;
		memset(buf->data, 0xFF, DSMCC_TSPARSER_BUFFER_SIZE);
	}

	if (packet[1] & START_INDICATOR)
	{
		DSMCC_DEBUG("New section");
		if (buf->in_section)
		{
			uint8_t pointer_field = packet[4];
			if (pointer_field <= 183)
			{
				if (pointer_field)
				{
					if (buf->in_section + pointer_field > DSMCC_TSPARSER_BUFFER_SIZE)
					{
						DSMCC_ERROR("Section buffer overflow, no room for %d bytes (buffer is already at %d bytes) (table ID is 0x%02hhx)", pointer_field, buf->in_section, buf->data[0]);
						memcpy(buf->data + buf->in_section, packet + 5, DSMCC_TSPARSER_BUFFER_SIZE - buf->in_section);
						buf->in_section = DSMCC_TSPARSER_BUFFER_SIZE;
					}
					else
					{
						memcpy(buf->data + buf->in_section, packet + 5, pointer_field);
						buf->in_section += pointer_field;
					}
				}

				if (buf->si_seen)
				{
					DSMCC_DEBUG("Processing section data: PID 0x%hx, table ID 0x%02hhx, buffer length %d", buf->pid, buf->data[0], buf->in_section);
					dsmcc_add_section(state, pid, buf->data, buf->in_section);
				}
				else
				{
					DSMCC_DEBUG("Ignoring section data with no start indicator: PID 0x%hx, buffer length %d", buf->pid, buf->in_section);
				}
				
				/* read data upto this and copy into buf */
				buf->si_seen = 1;
				buf->in_section = 183 - pointer_field;
				buf->cont = -1;
				memset(buf->data, 0xFF, DSMCC_TSPARSER_BUFFER_SIZE);
				memcpy(buf->data, packet + 5 + pointer_field, buf->in_section);
			}
			else
			{
				/* corrupted ? */
				DSMCC_ERROR("Invalid pointer field %d", pointer_field);
			}
		}
		else
		{
			buf->si_seen = 1;
			buf->in_section = 183;
			memcpy(buf->data, packet + 5, 183);
		}
	}
	else
	{
		/* append data to buf */
		if (buf->in_section + 184 > DSMCC_TSPARSER_BUFFER_SIZE)
		{
			DSMCC_ERROR("Section buffer overflow, no room for %d bytes (buffer is already at %d bytes) (table ID is 0x%02hhx)", 184, buf->in_section, buf->data[0]);
			memcpy(buf->data + buf->in_section, packet + 4, DSMCC_TSPARSER_BUFFER_SIZE - buf->in_section);
			buf->in_section = DSMCC_TSPARSER_BUFFER_SIZE;
		}
		else
		{
			memcpy(buf->data + buf->in_section, packet + 4, 184);
			buf->in_section += 184;
		}
	}
}

void dsmcc_tsparser_parse_buffered_sections(struct dsmcc_state *state, struct dsmcc_tsparser_buffer *buffers)
{
	while (buffers)
	{
		DSMCC_DEBUG("Processing section data PID 0x%hx, buffer length %d", buffers->pid, buffers->in_section);
		dsmcc_add_section(state, buffers->pid, buffers->data, buffers->in_section);
		buffers->si_seen = 0;
		buffers->in_section = 0;
		buffers->cont = -1;
		memset(buffers->data, 0xFF, DSMCC_TSPARSER_BUFFER_SIZE);
		buffers = buffers->next;
	}
}
