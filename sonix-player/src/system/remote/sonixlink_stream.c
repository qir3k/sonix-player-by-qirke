#include "sonixlink_stream.h"
#include "src/system/core/utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/gui/shell/gui.h"
#include "src/system/addons/addons.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/streaming/ytmusicpage.h"
#include "src/system/playback/device_state.h"
#include "src/system/streaming/podcast.h"
#include "src/system/streaming/podcastcache.h"
#include "src/system/streaming/podcastplayed.h"
#include "src/system/streaming/podcastsubs.h"
#include "src/system/streaming/qobuz.h"
#include "src/system/streaming/qobuzcache.h"
#include "src/system/streaming/radio.h"
#include "src/system/streaming/streamturn.h"
#include "src/system/streaming/tidal.h"
#include "src/system/streaming/tidalcache.h"
#include "src/system/streaming/ytmusic.h"
#include "src/system/streaming/ytpodcastplayed.h"

// How many rows one request asks a service for. The pages use their own,
// smaller, limits; the phone can draw more in one go.
#define STREAM_PAGE 50

// The last list each service returned. A play request names an index into
// exactly this list, so the metadata the phone does not carry is still here.
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
static sonixlink_stream_list_t last_list;
static char last_service[16];

// Reverses a list that the phone asked for the other way round (see the list
// entry point at the bottom of this file).
static void stream_rows_order(sonixlink_stream_list_t *out, const char *order);

// ---------------------------------------------------------------------------
// rows
// ---------------------------------------------------------------------------

static void row_set(sonixlink_stream_row_t *row, const char *kind, const char *id, const char *title,
					const char *subtitle) {
	memset(row, 0, sizeof(*row));
	snprintf(row->kind, sizeof(row->kind), "%s", kind ? kind : "container");
	snprintf(row->id, sizeof(row->id), "%s", id ? id : "");
	snprintf(row->title, sizeof(row->title), "%s", title ? title : "");
	snprintf(row->subtitle, sizeof(row->subtitle), "%s", subtitle ? subtitle : "");
	row->playable = strcmp(row->kind, "track") == 0 || strcmp(row->kind, "episode") == 0 ||
					strcmp(row->kind, "station") == 0;
}

static long id_number(const char *text) {
	return text && text[0] ? strtol(text, NULL, 10) : 0;
}

// ---------------------------------------------------------------------------
// services
// ---------------------------------------------------------------------------

static const char *service_addon(const char *service) {
	if (!service) return NULL;
	if (strcmp(service, "youtube") == 0) return "ytmusic";
	if (strcmp(service, "tidal") == 0) return "tidal";
	if (strcmp(service, "qobuz") == 0) return "qobuz";
	if (strcmp(service, "podcasts") == 0) return "podcasts";
	if (strcmp(service, "radio") == 0) return "radio";
	return NULL;
}

int sonixlink_stream_services(sonixlink_stream_service_t *out, int max) {
	if (!out || max <= 0) {
		return 0;
	}
	int n = 0;
	struct {
		const char *id;
		const char *name;
		bool available;
		bool signed_in;
		const char *detail;
	} list[] = {
		{"youtube", "YouTube Music", ytmusic_configured(), ytmusic_session_cookie_attached(),
		 !ytmusic_configured() ? "not_configured" : (ytmusic_session_cookie_attached() ? "" : "signed_out")},
		{"tidal", "Tidal", tidal_configured(), tidal_logged_in(),
		 !tidal_configured() ? "not_configured" : (tidal_logged_in() ? "" : "signed_out")},
		{"qobuz", "Qobuz", qobuz_configured(), qobuz_logged_in(),
		 !qobuz_configured() ? "not_configured" : (qobuz_logged_in() ? "" : "signed_out")},
		{"podcasts", "Podcasts", podcast_configured(), podcast_configured(),
		 podcast_configured() ? "" : "not_configured"},
		{"radio", "Internet radio", true, true, ""},
	};
	for (size_t i = 0; i < sizeof(list) / sizeof(list[0]) && n < max; i++) {
		// Expose all streaming services (YouTube Music, Tidal, Qobuz, Podcasts, Radio)
		// so mobile and desktop clients can browse, configure, and stream them
		memset(&out[n], 0, sizeof(out[n]));
		snprintf(out[n].id, sizeof(out[n].id), "%s", list[i].id);
		snprintf(out[n].name, sizeof(out[n].name), "%s", list[i].name);
		out[n].available = list[i].available;
		out[n].signed_in = list[i].signed_in;
		snprintf(out[n].detail, sizeof(out[n].detail), "%s", list[i].detail);
		n++;
	}
	return n;
}

// ---------------------------------------------------------------------------
// YouTube Music
// ---------------------------------------------------------------------------

static void yt_row(const ytmusic_item_t *item, sonixlink_stream_row_t *row) {
	if (item->kind == YTMUSIC_ITEM_CONTAINER) {
		row_set(row, "container", item->browse_id, item->title, item->subtitle);
	} else if (item->podcast_episode) {
		// An episode is not a song: the phone draws it differently, offers it
		// the episode speed, and ticks it once it has been heard. The mark is
		// the player's own, from the store its podcast pages tick rows with.
		row_set(row, "episode", item->id, item->title, item->subtitle);
		row->played = ytpodcastplayed_get(item->id);
	} else {
		row_set(row, "track", item->id, item->title, item->subtitle);
	}
	snprintf(row->album, sizeof(row->album), "%s", item->album);
	snprintf(row->art, sizeof(row->art), "%s", item->thumbnail);
	row->duration = item->duration;
}

static void list_youtube(const char *kind, const char *id, const char *q, const char *order,
						 const char *parent, sonixlink_stream_list_t *out) {
	ytmusic_item_t items[STREAM_PAGE];
	int count;
	char error[192] = "";
	const char *mode = kind && kind[0] ? kind : "explore";

	if (strcmp(mode, "explore") == 0) {
		ytmusic_section_t sections[YTMUSIC_SECTIONS_MAX];
		int n = ytmusic_explore_sections(sections, YTMUSIC_SECTIONS_MAX, error, sizeof(error));
		snprintf(out->title, sizeof(out->title), "%s", "Explore");
		if (n < 0) {
			snprintf(out->error, sizeof(out->error), "%s", error[0] ? error : "request_failed");
			return;
		}
		for (int s = 0; s < n && out->count < SONIXLINK_STREAM_ROWS; s++) {
			for (int i = 0; i < sections[s].count && out->count < SONIXLINK_STREAM_ROWS; i++) {
				yt_row(&sections[s].items[i], &out->rows[out->count++]);
			}
		}
		return;
	}
	if (strcmp(mode, "search") == 0) {
		if (!q || !q[0]) {
			snprintf(out->error, sizeof(out->error), "empty_search");
			return;
		}
		count = ytmusic_search_items(q, YTMUSIC_FILTER_SONGS, items, STREAM_PAGE, error, sizeof(error));
		snprintf(out->title, sizeof(out->title), "%s", q);
	} else if (strcmp(mode, "charts") == 0) {
		count = ytmusic_browse_items("FEmusic_charts", items, STREAM_PAGE, error, sizeof(error));
		snprintf(out->title, sizeof(out->title), "%s", "Charts");
	} else if (strcmp(mode, "liked") == 0) {
		count = ytmusic_browse_items("VLLM", items, STREAM_PAGE, error, sizeof(error));
		snprintf(out->title, sizeof(out->title), "%s", "Liked songs");
	} else if (strcmp(mode, "open") == 0) {
		if (!id || !id[0]) {
			snprintf(out->error, sizeof(out->error), "missing_id");
			return;
		}
		count = ytmusic_browse_items(id, items, STREAM_PAGE, error, sizeof(error));
		if (strncmp(id, "MPSP", 4) == 0) {
			for (int i = 0; i < count; i++) {
				if (items[i].kind == YTMUSIC_ITEM_TRACK) {
					items[i].podcast_episode = true;
				}
			}
		}
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_kind");
		return;
	}
	if (count < 0) {
		snprintf(out->error, sizeof(out->error), "%s", error[0] ? error : "request_failed");
		return;
	}
	for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
		yt_row(&items[i], &out->rows[out->count++]);
		if (items[i].podcast_episode && parent && parent[0]) {
			// A show's own page names the show in its header and nowhere on
			// its rows, so this is where the creator comes from.
			snprintf(out->rows[out->count - 1].subtitle, sizeof(out->rows[0].subtitle), "%s", parent);
		}
		if (!out->rows[out->count - 1].album[0] && parent && parent[0] && !items[i].podcast_episode) {
			snprintf(out->rows[out->count - 1].album, sizeof(out->rows[0].album), "%s", parent);
		}
	}
	out->more = count >= STREAM_PAGE;
	stream_rows_order(out, order);
}

// ---------------------------------------------------------------------------
// Tidal and Qobuz
// ---------------------------------------------------------------------------

static void tidal_track_row(const tidal_track_t *t, sonixlink_stream_row_t *row) {
	row_set(row, "track", "", t->title, t->artist);
	snprintf(row->id, sizeof(row->id), "%ld", t->id);
	snprintf(row->album, sizeof(row->album), "%s", t->album);
	snprintf(row->art, sizeof(row->art), "%s", t->cover);
	row->duration = t->duration;
	row->track_number = t->track_number;
	row->playable = t->streamable;
}

static void tidal_album_row(const tidal_album_t *a, sonixlink_stream_row_t *row) {
	row_set(row, "album", a->id_text, a->title, a->artist);
	snprintf(row->art, sizeof(row->art), "%s", a->cover);
	row->track_number = a->track_count;
}


static void qobuz_track_row(const qobuz_track_t *t, sonixlink_stream_row_t *row) {
	row_set(row, "track", "", t->title, t->artist);
	snprintf(row->id, sizeof(row->id), "%ld", t->id);
	snprintf(row->album, sizeof(row->album), "%s", t->album);
	snprintf(row->art, sizeof(row->art), "%s", t->cover);
	row->duration = t->duration;
	row->track_number = t->track_number;
	row->playable = t->streamable;
}

static void qobuz_album_row(const qobuz_album_t *a, sonixlink_stream_row_t *row) {
	row_set(row, "album", a->id_text, a->title, a->artist);
	snprintf(row->art, sizeof(row->art), "%s", a->cover);
	row->track_number = a->track_count;
}


// ---------------------------------------------------------------------------
// Podcasts
// ---------------------------------------------------------------------------

static void list_podcasts(const char *kind, const char *id, const char *q, const char *order,
						  sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "trending";
	podcast_feed_t feeds[PODCAST_PAGE_LIMIT];
	podcast_episode_t episodes[PODCAST_PAGE_LIMIT];
	int count = 0;
	// Whose show the episodes belong to, when the page is one show's.
	podcast_feed_t feed;
	bool have_feed = false;
	memset(&feed, 0, sizeof(feed));

	if (strcmp(mode, "search") == 0) {
		if (!q || !q[0]) {
			snprintf(out->error, sizeof(out->error), "empty_search");
			return;
		}
		count = podcast_search(q, feeds, PODCAST_PAGE_LIMIT);
		snprintf(out->title, sizeof(out->title), "%s", q);
	} else if (strcmp(mode, "followed") == 0) {
		count = podcastsubs_list(feeds, PODCAST_PAGE_LIMIT);
		snprintf(out->title, sizeof(out->title), "%s", "Followed");
	} else if (strcmp(mode, "feed") == 0) {
		count = podcast_episodes((long long)atoll(id), episodes, PODCAST_PAGE_LIMIT);
		have_feed = podcast_feed((long long)atoll(id), &feed) && feed.author[0];
	} else if (strcmp(mode, "trending") == 0) {
		count = podcast_trending(feeds, PODCAST_PAGE_LIMIT);
		snprintf(out->title, sizeof(out->title), "%s", "Trending");
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_kind");
		return;
	}

	if (count < 0) {
		const char *why = podcast_last_error();
		snprintf(out->error, sizeof(out->error), "%s", why && why[0] ? why : "request_failed");
		return;
	}
	if (strcmp(mode, "feed") == 0) {
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			podcast_episode_t *e = &episodes[i];
			sonixlink_stream_row_t *row = &out->rows[out->count++];
			// The creator is the feed's author: an episode carries only the
			// feed's title. One request for the feed says who made it, and it
			// is asked once per page rather than once per row.
			row_set(row, "episode", "", e->title, have_feed ? feed.author : e->feed_title);
			snprintf(row->id, sizeof(row->id), "%lld", e->id);
			snprintf(row->art, sizeof(row->art), "%s", e->image);
			snprintf(row->mime, sizeof(row->mime), "%s", e->mime[0] ? e->mime : "audio/mpeg");
			row->duration = e->duration_secs;
			row->playable = e->enclosure[0] != '\0';
			row->played = podcastplayed_get(e->id);
		}
	} else {
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			podcast_feed_t *f = &feeds[i];
			sonixlink_stream_row_t *row = &out->rows[out->count++];
			row_set(row, "feed", "", f->title, f->author);
			snprintf(row->id, sizeof(row->id), "%lld", f->id);
			snprintf(row->art, sizeof(row->art), "%s", f->image);
			row->track_number = f->episode_count;
		}
	}
	out->more = count >= PODCAST_PAGE_LIMIT;
	stream_rows_order(out, order);
}

// ---------------------------------------------------------------------------
// Internet radio
// ---------------------------------------------------------------------------

static bool radio_wait(unsigned serial) {
	for (int i = 0; i < 40; i++) {
		radio_job_t state = radio_job_state();
		if (radio_job_serial() != serial && (state == RADIO_JOB_OK || state == RADIO_JOB_FAILED)) {
			return state == RADIO_JOB_OK;
		}
		usleep(200 * 1000);
	}
	return false;
}

static radio_browse_t radio_kind_of(const char *kind) {
	if (kind && strcmp(kind, "genres") == 0) {
		return RADIO_BROWSE_GENRES;
	}
	if (kind && strcmp(kind, "languages") == 0) {
		return RADIO_BROWSE_LANGUAGES;
	}
	return RADIO_BROWSE_COUNTRIES;
}

static void radio_station_row(const radio_station_t *s, sonixlink_stream_row_t *row) {
	// After row_set(), which clears the whole row: a station's URL is what the
	// phone plays when the guard advances the queue.
	row_set(row, "station", s->uuid, s->name, s->country);
	snprintf(row->url, sizeof(row->url), "%s", s->url);
	snprintf(row->art, sizeof(row->art), "%s", s->favicon);
	snprintf(row->mime, sizeof(row->mime), "%s", s->codec);
	row->track_number = s->bitrate;
	row->playable = radio_station_problem(s) == NULL;
}

static void list_radio(const char *kind, const char *id, const char *q, sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "recommended";

	if (strcmp(mode, "terms") == 0) {
		radio_browse_t which = radio_kind_of(id);
		unsigned serial = radio_job_serial();
		radio_request_terms(which);
		if (!radio_wait(serial)) {
			snprintf(out->error, sizeof(out->error), "radio_unavailable");
			return;
		}
		radio_term_t terms[200];
		int count = radio_get_terms(terms, 200);
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			sonixlink_stream_row_t *row = &out->rows[out->count++];
			row_set(row, "term", terms[i].name, terms[i].label, "");
			snprintf(row->album, sizeof(row->album), "%s", id ? id : "countries");
			row->track_number = terms[i].stationcount;
		}
		snprintf(out->title, sizeof(out->title), "%s", "Browse");
		return;
	}
	if (strcmp(mode, "favorites") == 0) {
		int count = radio_fav_count();
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			radio_station_t station;
			if (radio_fav_get(i, &station)) {
				radio_station_row(&station, &out->rows[out->count++]);
			}
		}
		snprintf(out->title, sizeof(out->title), "%s", "Favourites");
		return;
	}
	if (strcmp(mode, "custom") == 0) {
		int count = radio_custom_count();
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			radio_station_t station;
			if (radio_custom_get(i, &station)) {
				radio_station_row(&station, &out->rows[out->count++]);
			}
		}
		snprintf(out->title, sizeof(out->title), "%s", "My stations");
		return;
	}

	unsigned serial = radio_job_serial();
	if (strcmp(mode, "search") == 0) {
		if (!q || !q[0]) {
			snprintf(out->error, sizeof(out->error), "empty_search");
			return;
		}
		radio_request_search(q, 0);
		snprintf(out->title, sizeof(out->title), "%s", q);
	} else if (strcmp(mode, "stations") == 0) {
		radio_request_stations(radio_kind_of(q), id, 0);
	} else if (strcmp(mode, "recommended") == 0) {
		radio_request_recommended(0);
		snprintf(out->title, sizeof(out->title), "%s", "Popular stations");
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_kind");
		return;
	}
	if (!radio_wait(serial)) {
		snprintf(out->error, sizeof(out->error), "radio_unavailable");
		return;
	}
	radio_station_t stations[RADIO_PAGE_SIZE];
	int count = radio_get_stations(stations, RADIO_PAGE_SIZE);
	for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
		radio_station_row(&stations[i], &out->rows[out->count++]);
	}
}

// ---------------------------------------------------------------------------
// one list
// ---------------------------------------------------------------------------

#include "sonixlink_stream_menu.inc"

// Episode pages are the one place a list can be wanted the other way round, and
// it is the same rows either way -- the phone plays by index, so the order it
// draws has to be the order this list was kept in.
static void stream_rows_order(sonixlink_stream_list_t *out, const char *order) {
	if (!order || strcmp(order, "oldest") != 0 || out->count < 2) {
		return;
	}
	for (int i = 0, j = out->count - 1; i < j; i++, j--) {
		sonixlink_stream_row_t swap = out->rows[i];
		out->rows[i] = out->rows[j];
		out->rows[j] = swap;
	}
}

bool sonixlink_stream_list(const char *service, const char *kind, const char *id, const char *q, int offset,
						   const char *order, const char *parent, sonixlink_stream_list_t *out) {
	if (!service || !service[0] || !out) {
		return false;
	}
	memset(out, 0, sizeof(*out));

	const char *addon = service_addon(service);
	if (addon && !addons_enabled(addon)) {
		snprintf(out->error, sizeof(out->error), "service_disabled");
		return false;
	}

	if (kind && strcmp(kind, "menu") == 0) {
		return sonixlink_stream_menu(service, out);
	}

	if (strcmp(service, "youtube") == 0) {
		list_youtube_full(kind, id, q, order, parent, out);
	} else if (strcmp(service, "tidal") == 0) {
		list_tidal_full(kind, id, q, offset, out);
	} else if (strcmp(service, "qobuz") == 0) {
		list_qobuz_full(kind, id, q, offset, out);
	} else if (strcmp(service, "podcasts") == 0) {
		list_podcasts(kind, id, q, order, out);
	} else if (strcmp(service, "radio") == 0) {
		list_radio_full(kind, id, q, out);
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_service");
		return false;
	}
	if (out->error[0]) {
		return false;
	}
	if (out->count > 0) {
		pthread_mutex_lock(&list_lock);
		memcpy(&last_list, out, sizeof(*out));
		snprintf(last_service, sizeof(last_service), "%s", service);
		pthread_mutex_unlock(&list_lock);
	}
	return true;
}

// ---------------------------------------------------------------------------
// playback
// ---------------------------------------------------------------------------

typedef struct {
	char service[16];
	int row_count;
	int start_row;
	int start_slot;
	int built;
	// The track playing when the request arrived, so a job whose download
	// outlasted the user's patience can tell it has been overtaken.
	char then_playing[512];
	char (*paths)[512];
	const char **pointers;
	ytmusic_track_t yt_tracks[YTMUSIC_QUEUE_MAX];
	sonixlink_stream_row_t rows[SONIXLINK_STREAM_ROWS];
} play_job_t;

typedef struct {
	play_job_t *job;
	bool queued;
} youtube_playback_handoff_t;

static void job_free(play_job_t *job) {
	free(job->paths);
	free(job->pointers);
	free(job);
}

static void playback_failed(void *user) {
	gui_notify_popup(user);
	free(user);
}

static void notify_playback_failed(const char *service) {
	const char *key = "download_failed";
	if (strcmp(service, "youtube") == 0) {
		key = "ytmusic_playback_failed";
	} else if (strcmp(service, "podcasts") == 0) {
		key = "podcast_download_failed";
	}
	char *message = strdup(key);
	if (!message) {
		fprintf(stderr, "sonixlink: cannot allocate a %s playback error notice\n", service);
		return;
	}
	if (!gui_post(playback_failed, message)) {
		fprintf(stderr, "sonixlink: cannot show the %s playback error notice\n", service);
		free(message);
	}
}

static void queue_start(void *user) {
	play_job_t *job = user;
	// This runs on the GUI thread long after the request: the download may have
	// taken minutes on a slow network. If the player has moved on in the
	// meantime -- the user started something else, local or not -- the request
	// is stale, and starting it now would cut off what they chose to hear.
	device_state_t now;
	device_state_get(&now);
	if (strcmp(now.current_file, job->then_playing) != 0) {
		fprintf(stderr, "sonixlink: dropping %s play: the player moved from '%s' to '%s'\n",
				job->service, job->then_playing, now.current_file);
		job_free(job);
		return;
	}
	if (job->built > 0 && job->start_slot >= 0) {
		if (strcmp(job->service, "youtube") == 0 &&
			!ytmusicpage_set_external_queue(job->yt_tracks, job->pointers, job->built, job->start_slot)) {
			fprintf(stderr, "sonixlink: cannot register YouTube Music queue\n");
			notify_playback_failed(job->service);
			job_free(job);
			return;
		}
		if (strcmp(job->service, "podcasts") == 0) {
			device_state_play_list_ordered(job->pointers, job->built, job->start_slot);
		} else {
			device_state_play_list(job->pointers, job->built, job->start_slot);
		}
		player_refresh_now_playing();
	}
	job_free(job);
}

static void youtube_playback_ready(const char *path, void *user) {
	youtube_playback_handoff_t *handoff = user;
	play_job_t *job = handoff->job;
	if (job->start_slot >= 0) {
		snprintf(job->paths[job->start_slot], sizeof(job->paths[0]), "%s", path);
	}
	handoff->queued = gui_post(queue_start, job);
}

static void radio_start(void *user) {
	play_job_t *job = user;
	radio_station_t stations[SONIXLINK_STREAM_ROWS];
	int count = 0;
	int start = -1;
	for (int i = 0; i < job->row_count; i++) {
		const sonixlink_stream_row_t *row = &job->rows[i];
		if (strcmp(row->kind, "station") != 0 || !row->id[0]) {
			continue;
		}
		radio_station_t *s = &stations[count];
		memset(s, 0, sizeof(*s));
		snprintf(s->uuid, sizeof(s->uuid), "%s", row->id);
		snprintf(s->url, sizeof(s->url), "%s", row->url);
		snprintf(s->name, sizeof(s->name), "%s", row->title);
		snprintf(s->country, sizeof(s->country), "%s", row->subtitle);
		snprintf(s->favicon, sizeof(s->favicon), "%s", row->art);
		snprintf(s->codec, sizeof(s->codec), "%s", row->mime);
		s->bitrate = row->track_number;
		if (i == job->start_row) {
			start = count;
		}
		count++;
	}
	if (start >= 0 && count > 0) {
		radio_set_list(stations, count);
		radio_play(&stations[start]);
		player_refresh_now_playing();
	}
	job_free(job);
}

// Builds the cache paths the queue holds. The service's own cache names the
// files, and the sidecars give every entry a title before it has arrived.
static bool prepare_queue(play_job_t *job) {
	job->paths = calloc(SONIXLINK_STREAM_ROWS, sizeof(*job->paths));
	job->pointers = calloc(SONIXLINK_STREAM_ROWS, sizeof(*job->pointers));
	if (!job->paths || !job->pointers) {
		return false;
	}

	for (int i = 0; i < job->row_count; i++) {
		sonixlink_stream_row_t *row = &job->rows[i];
		if (strcmp(row->kind, "track") != 0 && strcmp(row->kind, "episode") != 0) {
			continue;
		}
		char path[512] = "";
		if (strcmp(job->service, "tidal") == 0) {
			long track_id = id_number(row->id);
			if (!tidalcache_find(track_id, path, sizeof(path))) {
				tidalcache_path(track_id, tidal_expected_mime(), path, sizeof(path));
			}
			if (!path[0]) continue;
			tidalcache_write_sidecars_at(path, row->title, row->subtitle, row->album, "", row->track_number,
										 row->duration, row->art);
		} else if (strcmp(job->service, "qobuz") == 0) {
			long id = id_number(row->id);
			// A cached track keeps the extension it was downloaded with, and
			// that need not be the one this build expects. The guessed name is
			// only for a track that is not there yet.
			if (!qobuzcache_find(id, path, sizeof(path))) {
				qobuzcache_path(id, qobuz_expected_mime(), path, sizeof(path));
			}
			if (!path[0]) continue;
			qobuzcache_write_sidecars(id, qobuz_expected_mime(), row->title, row->subtitle, row->album, "",
									  row->track_number, row->art);
		} else if (strcmp(job->service, "youtube") == 0) {
			if (!ytmusic_cache_path_for(row->id, path, sizeof(path))) continue;
			ytmusic_track_t *track = &job->yt_tracks[job->built];
			memset(track, 0, sizeof(*track));
			snprintf(track->id, sizeof(track->id), "%s", row->id);
			snprintf(track->title, sizeof(track->title), "%s", row->title);
			snprintf(track->artist, sizeof(track->artist), "%s", row->subtitle);
			snprintf(track->album, sizeof(track->album), "%s", row->album);
			snprintf(track->thumbnail, sizeof(track->thumbnail), "%s", row->art);
			track->duration = row->duration;
			track->podcast_episode = strcmp(row->kind, "episode") == 0;
			if (!ytmusic_cache_write_sidecars(track, path)) {
				fprintf(stderr, "sonixlink: cannot write YouTube Music queue metadata for %s\n", row->id);
				return false;
			}
		} else if (strcmp(job->service, "podcasts") == 0) {
			const char *mime = row->mime[0] ? row->mime : "audio/mpeg";
			long long episode = (long long)atoll(row->id);
			if (!podcastcache_find(episode, path, sizeof(path))) {
				podcastcache_path(episode, mime, path, sizeof(path));
			}
			if (!path[0]) continue;
			podcastcache_write_sidecars((long long)atoll(row->id), mime, row->title, row->subtitle, 0, "", "",
										row->art, false);
		}
		if (i == job->start_row) {
			job->start_slot = job->built;
		}
		snprintf(job->paths[job->built], sizeof(job->paths[0]), "%s", path);
		job->pointers[job->built] = job->paths[job->built];
		job->built++;
	}
	return job->built > 0 && job->start_slot >= 0;
}

static void *play_worker(void *arg) {
	thread_be_low_priority("sonixlink play");
	play_job_t *job = arg;
	if (strcmp(job->service, "radio") == 0) {
		if (!gui_post(radio_start, job)) {
			fprintf(stderr, "sonixlink: cannot start radio playback: GUI queue is full\n");
			job_free(job);
		}
		return NULL;
	}
	if (!prepare_queue(job)) {
		fprintf(stderr, "sonixlink: cannot prepare %s playback queue\n", job->service);
		notify_playback_failed(job->service);
		job_free(job);
		return NULL;
	}

	sonixlink_stream_row_t *start = &job->rows[job->start_row];
	char service[sizeof(job->service)];
	snprintf(service, sizeof(service), "%s", job->service);
	bool downloaded = false;
	if (strcmp(job->service, "tidal") == 0) {
		long id = id_number(start->id);
		tidal_stream_t *stream = calloc(1, sizeof(*stream));
		if (stream && tidal_track_stream(id, stream)) {
			char path[512] = "";
			if (tidalcache_start(id, stream, start->duration, true, path, sizeof(path)) && path[0]) {
				downloaded = true;
				if (job->start_slot >= 0) {
					snprintf(job->paths[job->start_slot], sizeof(job->paths[0]), "%s", path);
				}
				tidalcache_write_sidecars_at(path, start->title, start->subtitle, start->album, "",
											 start->track_number, start->duration, start->art);
			}
		}
		free(stream);
	} else if (strcmp(job->service, "qobuz") == 0) {
		long id = id_number(start->id);
		char url[QOBUZ_URL_MAX];
		char mime[64];
		if (qobuz_track_url(id, url, sizeof(url), mime, sizeof(mime), NULL)) {
			char path[512] = "";
			if (qobuzcache_start(id, url, mime, start->duration, path, sizeof(path)) && path[0]) {
				downloaded = true;
				if (job->start_slot >= 0) {
					snprintf(job->paths[job->start_slot], sizeof(job->paths[0]), "%s", path);
				}
				qobuzcache_write_sidecars(id, mime, start->title, start->subtitle, start->album, "",
										  start->track_number, start->art);
			}
		}
	} else if (strcmp(job->service, "podcasts") == 0) {
		podcast_episode_t episode;
		if (podcast_episode((long long)atoll(start->id), &episode)) {
			char path[512] = "";
			if (podcastcache_start(episode.id, episode.enclosure, episode.mime, episode.duration_secs, path,
								   sizeof(path)) &&
				path[0]) {
				downloaded = true;
				if (job->start_slot >= 0) {
					snprintf(job->paths[job->start_slot], sizeof(job->paths[0]), "%s", path);
				}
				// The tags go beside the file that actually arrived: an episode
				// served as mp3 when the feed said m4a would otherwise have them
				// in a sidecar nothing reads, and the row would show a file name.
				podcastcache_write_sidecars(episode.id, episode.mime, start->title, start->subtitle, 0, "", "",
											start->art, false);
			}
		}
	} else if (strcmp(job->service, "youtube") == 0) {
		ytmusic_track_t track = job->yt_tracks[job->start_slot];
		youtube_playback_handoff_t handoff = {.job = job, .queued = false};
		char path[512] = "";
		char error[192] = "";
		downloaded = ytmusic_stream_for_playback(&track, path, sizeof(path), youtube_playback_ready, &handoff,
												 error, sizeof(error));
		if (handoff.queued) {
			if (!downloaded) {
				fprintf(stderr, "sonixlink: YouTube Music stream failed after playback began: %s\n",
						error[0] ? error : "request_failed");
				notify_playback_failed(service);
			}
			return NULL;
		}
		if (downloaded && path[0]) {
			if (job->start_slot >= 0) {
				snprintf(job->paths[job->start_slot], sizeof(job->paths[0]), "%s", path);
			}
		} else {
			fprintf(stderr, "sonixlink: YouTube Music playback download failed: %s\n",
					error[0] ? error : "request_failed");
		}
	}

	if (!downloaded) {
		fprintf(stderr, "sonixlink: %s playback could not prepare the selected track\n", job->service);
		notify_playback_failed(service);
		job_free(job);
		return NULL;
	}
	if (!gui_post(queue_start, job)) {
		fprintf(stderr, "sonixlink: cannot queue %s playback: GUI queue is full\n", job->service);
		job_free(job);
	}
	return NULL;
}

bool sonixlink_stream_play(const char *service, int index, char *error, size_t error_size) {
	if (error && error_size) {
		error[0] = '\0';
	}
	if (!service || !service[0] || index < 0) {
		if (error && error_size) snprintf(error, error_size, "bad_request");
		return false;
	}
	const char *addon = service_addon(service);
	if (addon && !addons_enabled(addon)) {
		if (error && error_size) snprintf(error, error_size, "service_disabled");
		return false;
	}

	pthread_mutex_lock(&list_lock);
	if (strcmp(last_service, service) != 0 || index >= last_list.count) {
		pthread_mutex_unlock(&list_lock);
		if (error && error_size) snprintf(error, error_size, "stale_list");
		return false;
	}
	play_job_t *job = calloc(1, sizeof(*job));
	if (!job) {
		pthread_mutex_unlock(&list_lock);
		if (error && error_size) snprintf(error, error_size, "out_of_memory");
		return false;
	}
	snprintf(job->service, sizeof(job->service), "%s", service);
	job->row_count = last_list.count;
	job->start_row = index;
	job->start_slot = -1;
	memcpy(job->rows, last_list.rows, sizeof(job->rows));
	pthread_mutex_unlock(&list_lock);

	// Taken after the unlock: device_state has its own lock, and holding this
	// one across it invites a deadlock for a value nobody else wants.
	device_state_t now;
	device_state_get(&now);
	snprintf(job->then_playing, sizeof(job->then_playing), "%s", now.current_file);

	streamturn_abandon_all();
	pthread_t worker;
	if (pthread_create(&worker, NULL, play_worker, job) != 0) {
		job_free(job);
		if (error && error_size) snprintf(error, error_size, "no_worker");
		return false;
	}
	pthread_detach(worker);
	return true;
}

void sonixlink_stream_cleanup(void) {
	pthread_mutex_lock(&list_lock);
	memset(&last_list, 0, sizeof(last_list));
	last_service[0] = '\0';
	pthread_mutex_unlock(&list_lock);
}
