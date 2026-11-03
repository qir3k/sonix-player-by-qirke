#ifndef SONIXLINK_STREAM_H
#define SONIXLINK_STREAM_H

#include <stdbool.h>
#include <stddef.h>

// What the phone's Streaming tab browses, as one list shape for every service.
//
// The services themselves are the ones the player already has -- YouTube Music,
// Tidal, Qobuz, Podcasts and Internet radio -- and every call here is a thin
// wrapper over their blocking API. The routes that use this run on the
// SonixLink worker, so a slow service delays only that request, never the
// player's audio.
//
// A list keeps the metadata it returned. Playing an entry sends back only its
// index, so the phone does not have to carry titles, ids and cover addresses
// for a service it cannot talk to directly, and the queue can be built from the
// same rows the list was drawn from.

#define SONIXLINK_STREAM_ROWS 60
#define SONIXLINK_STREAM_ID 192
#define SONIXLINK_STREAM_TITLE 200
#define SONIXLINK_STREAM_SUBTITLE 160
#define SONIXLINK_STREAM_ART 256
#define SONIXLINK_STREAM_MIME 40
#define SONIXLINK_STREAM_ERROR 192

typedef struct {
	// "track" (plays), "episode" (plays), "station" (plays), or one of the
	// containers the service opens: "container", "album", "playlist",
	// "artist", "feed", "term".
	char kind[16];
	char id[SONIXLINK_STREAM_ID];
	char title[SONIXLINK_STREAM_TITLE];
	char subtitle[SONIXLINK_STREAM_SUBTITLE];
	char album[SONIXLINK_STREAM_TITLE];
	char art[SONIXLINK_STREAM_ART];
	char mime[SONIXLINK_STREAM_MIME];
	int duration;
	int track_number;
	bool playable;
} sonixlink_stream_row_t;

typedef struct {
	char title[128];
	int count;
	bool more;
	char error[SONIXLINK_STREAM_ERROR];
	sonixlink_stream_row_t rows[SONIXLINK_STREAM_ROWS];
} sonixlink_stream_list_t;

typedef struct {
	char id[16];
	char name[40];
	bool available;
	bool signed_in;
	char detail[96];
} sonixlink_stream_service_t;

// The five services and whether they can be used right now. Returns the count.
int sonixlink_stream_services(sonixlink_stream_service_t *out, int max);

// One page of one service. `kind` and `id` are the values from a row, and `q`
// the search text; see sonixlink_stream.c for what each service accepts.
// Blocking: call from the request thread.
bool sonixlink_stream_list(const char *service, const char *kind, const char *id, const char *q,
						   int offset, sonixlink_stream_list_t *out);

// Plays entry `index` of the last list returned for `service`, queueing the
// rest of that list's playable rows with it. Returns false with `error` filled
// when the service cannot start it.
bool sonixlink_stream_play(const char *service, int index, char *error, size_t error_size);

#endif /* SONIXLINK_STREAM_H */
