/**
 * \file	dsmcc.h
 * \brief	DSM-CC Object Carousel parser
 * \author	Loic Lefort <llefort@wyplay.com>
 */

#ifndef LIBDSMCC_H
#define LIBDSMCC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** \defgroup logging Logging
 *  \{
 */

/** The different log levels */
enum
{
	DSMCC_LOG_DEBUG = 0,
	DSMCC_LOG_WARN,
	DSMCC_LOG_ERROR
};

/** The diffent carousel type */
enum
{
DSMCC_OBJECT_CAROUSEL,
DSMCC_DATA_CAROUSEL
};

/** \brief Callback called by the logging code
  * \param severity the severity of the message
  * \param message the message to be logged
  */
typedef void (dsmcc_logger_t)(int severity, const char *message);

/** \brief Set the logger callback
  * \param logger the callback
  * \param severity the minimum severity for which the callback will be called
  */
void dsmcc_set_logger(dsmcc_logger_t *logger, int severity);

/** \} */ // end of 'logging' group


/** \defgroup main Main DSM-CC API
 *  \{
 */

struct dsmcc_dvb_callbacks
{
	/** \brief Callback called when the library needs to find the PID for a given association tag
	  * \param arg opaque callback argument (passed as-is from field assoc_tag_callback_arg in struct dsmcc_dvb_callbacks)
	  * \param assoc_tag the Association Tag of the requested stream
	  * \param pid pointer to variable where the PID of the requested stream will be written
	  * \return 0 if successful, <0 otherwise
	  */
	int (*get_pid_for_assoc_tag)(void *arg, uint16_t assoc_tag, uint16_t *pid);
	/** argument for get_pid_for_assoc_tag callback */
	void *get_pid_for_assoc_tag_arg;

	/** \brief Callback called when the library needs to set a section filter on a PID
	  * \param arg opaque callback argument (passed as-is from field add_section_filter_arg  in struct dsmcc_dvb_callbacks)
	  * \param pid
	  * \param pattern
	  * \param equalmask
	  * \param notequalmask
	  * \param depth
	  * \return 0 if successful, <0 otherwise
	  */
	int (*add_section_filter)(void *arg, uint16_t pid, uint8_t *pattern, uint8_t *equalmask, uint8_t *notequalmask, uint16_t depth);
	/** argument for add_section_filter callback */
	void *add_section_filter_arg;
};

/** Opaque type containing the library state */
struct dsmcc_state;

/** \brief Initialize the DSM-CC parser
  * \param cachedir the cache directory that will be used. Will be created if not already existent. If NULL, it will default to /tmp/dsmcc-cache-XYZ (where XYZ is the PID of the current process)
  * \param keep_cache if 0 the cache files will be removed at close
  * \param callbacks the callbacks that will be called when the library needs to interract with DVB stack
  */
struct dsmcc_state *dsmcc_open(const char *cachedir, bool keep_cache, struct dsmcc_dvb_callbacks *callbacks);

/** \brief Add a MPEG section that will be processed by the parsing thread
  * \param state the library state
  * \param pid the PID of the stream from which the section originates
  * \param data the section data
  * \param data_length the total length of the data buffer
  * \return 1 if no error occured, 0 otherwise
  */
void dsmcc_add_section(struct dsmcc_state *state, uint16_t pid, uint8_t *data, int data_length);

/** \brief Free the memory used by the library (and the cache files if keep_state was 0 on dsmcc_init call)
  * \param state the library state
  */
void dsmcc_close(struct dsmcc_state *state);

/** \} */ // end of 'main' group

/** \defgroup control Download Control
 *  \{
 */

enum
{
	DSMCC_STATUS_PARTIAL,
	DSMCC_STATUS_DOWNLOADING,
	DSMCC_STATUS_TIMEDOUT,
	DSMCC_STATUS_DONE
};

struct dsmcc_carousel_callbacks
{
	/** \brief Callback called for each directory/file in the carousel to determine if it should be saved or not
	  * \param arg Opaque argument (passed as-is from the dentry_check_arg field of struct dsmcc_carousel_callbacks
	  * \param queue_id the queue ID that was returned by dsmcc_queue_carousel
	  * \param cid the carousel ID
	  * \param dir 0 if the dentry is a file, any other value indicate that the dentry is a directory
	  * \param path the directory/file path relative to the carousel root
	  * \param fullpath the directory/file path on disk
	  * \return return 0 if the directory/file should be skipped or any other value if it should be saved
	  */
	bool (*dentry_check)(void *arg, uint32_t queue_id, uint32_t cid, bool dir, const char *path, const char *fullpath);
	/** argument for dentry_check callback */
	void *dentry_check_arg;

	/** \brief Callback called after each directory/file in the carousel is saved to disk
	  * \param arg Opaque argument (passed as-is from the dentry_saved_arg field of struct dsmcc_carousel_callbacks
	  * \param queue_id the queue ID that was returned by dsmcc_queue_carousel
	  * \param cid the carousel ID
	  * \param dir 0 if the dentry is a file, any other value indicate that the dentry is a directory
	  * \param path the directory/file path relative to the carousel root
	  * \param fullpath the directory/file path on disk
	  */
	void (*dentry_saved)(void *arg, uint32_t queue_id, uint32_t cid, bool dir, const char *path, const char *fullpath);
	/** argument for dentry_saved callback */
	void  *dentry_saved_arg;

	/** \brief Callback called when the download progression changes
	  * \param arg Opaque argument (passed as-is from the download_progression_arg field of struct dsmcc_carousel_callbacks
	  * \param queue_id the queue ID that was returned by dsmcc_queue_carousel
	  * \param cid the carousel ID
	  * \param downloaded the amount of bytes downloaded so far
	  * \param total the total carousel size in bytes
	  */
	void (*download_progression)(void *arg, uint32_t queue_id, uint32_t cid, uint32_t downloaded, uint32_t total);
	/** argument for total_carousel_bytes callback */
	void  *download_progression_arg;

	/** \brief Callback called when the carousel status changes
	  * \param arg Opaque argument (passed as-is from the carousel_status_changed_arg field of struct dsmcc_carousel_callbacks
	  * \param queue_id the queue ID that was returned by dsmcc_queue_carousel
	  * \param cid the carousel ID
	  * \param newstatus the new carousel status
	  */
	void (*carousel_status_changed)(void *arg, uint32_t queue_id, uint32_t cid, int newstatus);
	/** argument for carousel_status_changed callback */
	void  *carousel_status_changed_arg;
};

/** \brief Add a carousel to the list of carousels to be downloaded
  * \param state the library state
  * \param DSMCC_DATA_CAROUSEL or DSMCC_OBJECT_CAROUSEL
  * \param pid the PID of the stream where the carousel DSI message is broadcasted
  * \param transaction_id the transaction ID of the carousel DSI message or 0xFFFFFFFF to use the first DSI message found on the stream
  * \param downloadpath the directory where the carousel files will be downloaded
  * \param callbacks the callback that will be called during/after carousel download
  * \return a carousel queue ID that will be used to remove the carousel
  */
uint32_t dsmcc_queue_carousel2(struct dsmcc_state *state, int type, 
		uint16_t pid, uint32_t transaction_id,	const char *downloadpath,
		struct dsmcc_carousel_callbacks *callbacks);

/** \brief deprecated API for compatibility */
uint32_t dsmcc_queue_carousel(struct dsmcc_state *state, uint16_t pid, uint32_t transaction_id,
		const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks);

/** \brief Remove a carousel from the list of carousels to be downloaded
  * \param state the library state
  * \param queue_id the queue ID that was returned by dsmcc_queue_carousel
  */
void dsmcc_dequeue_carousel(struct dsmcc_state *state, uint32_t queue_id);

/** \} */ // end of 'control' group

/** \defgroup control Cache Control
 *  \{
 */

/** \brief Remove all cached data
  * \param state the library state
  */
void dsmcc_cache_clear(struct dsmcc_state *state);

/** \brief Remove cached data for a given carousel. Does not remove data if carousel is currently queued
  * \param state the library state
  * \param carousel_id the carousel_id
  */
void dsmcc_cache_clear_carousel(struct dsmcc_state *state, uint32_t carousel_id);

/** \} */ // end of 'cache' group

#ifdef __cplusplus
}
#endif
#endif
