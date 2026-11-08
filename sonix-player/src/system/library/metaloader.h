#ifndef METALOADER_H
#define METALOADER_H

#include <stdbool.h>
#include "metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

// Starts the lightweight background metadata extraction worker thread. Safe to call multiple times.
void metaloader_init(void);

// Enqueues an asynchronous request to extract metadata from filepath.
void metaloader_request(const char *filepath);

// Non-blocking query called from UI event loop / timer.
// If metadata for filepath is ready, fills out with the extracted metadata and returns true.
// If still pending or worker is running, returns false.
bool metaloader_take(const char *filepath, song_metadata_t *out);

// Fills out with fast in-memory placeholder metadata (no disk I/O) to display in UI immediately.
void metaloader_get_placeholder(const char *filepath, song_metadata_t *out);

#ifdef __cplusplus
}
#endif

#endif // METALOADER_H
