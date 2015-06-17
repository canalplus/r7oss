/**
 * \file	dsmcc-tsparser.h
 * \brief	DSM-CC Object Carousel parser
 * \author	Loic Lefort <llefort@wyplay.com>
 */

#ifndef LIBDSMCC_TSPARSER_H
#define LIBDSMCC_TSPARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** \defgroup tsparser TS Parser
 *  \{
 */

/** from dsmcc.h */
struct dsmcc_state;

/** Opaque structure used by the TS Parser */
struct dsmcc_tsparser_buffer;

/** \brief Allocate a buffer for a PID in the list of section buffers used by the TS Parser
  * \param buffers a pointer to the list of buffers
  * \param pid the PID
  */
void dsmcc_tsparser_add_pid(struct dsmcc_tsparser_buffer **buffers, uint16_t pid);

/** \brief Free all the buffers used by the TS Parser
  * \param buffers a pointer to the list of buffers
  */
void dsmcc_tsparser_free_buffers(struct dsmcc_tsparser_buffer **buffers);

/** \brief Parse a single TS packet. It will be added to current sections buffers and in case of complete section, dsmcc_parse_section will be called.
  * \param state the library state
  * \param buffers a pointer to the list of buffers
  * \param packet the packet data
  * \param packet_length the packet length (should be 188)
  */
void dsmcc_tsparser_parse_packet(struct dsmcc_state *state, struct dsmcc_tsparser_buffer **buffers, uint8_t *packet, int packet_length);

/** \brief Call dsmcc_parse_section on all current section buffers.
  * \param state the library state
  * \param buffers a pointer to the list of buffers
  */
void dsmcc_tsparser_parse_buffered_sections(struct dsmcc_state *state, struct dsmcc_tsparser_buffer *buffers);

/** \} */ // end of 'tsparser' group

#ifdef __cplusplus
}
#endif
#endif
