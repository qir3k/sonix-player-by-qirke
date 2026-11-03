// The local menus of the streaming pages.
//
// Included by sonixlink_stream.c after the list helpers and before the
// dispatcher. Every row here is the player's own: the order and the hidden
// flags are read from the same configuration keys the pages write, so the
// phone shows what the DAP shows.
//
// These are local reads. A menu never touches the network.

#include "src/system/core/config.h"
#include "src/system/streaming/ytlikes.h"

static void menu_add(sonixlink_stream_list_t *out, const char *kind, const char *id, const char *title) {
	if (out->count >= SONIXLINK_STREAM_ROWS) {
		return;
	}
	sonixlink_stream_row_t *row = &out->rows[out->count++];
	row_set(row, kind, id, title, "");
}

// ---------------------------------------------------------------------------
// Tidal
// ---------------------------------------------------------------------------

static const char *const TIDAL_MENU_KINDS[] = {
	"favorite_tracks", "favorite_albums", "favorite_artists", "playlists",
	"new", "top", "recommended",
};
static const char *const TIDAL_MENU_LABELS[] = {
	"Favourite tracks", "Favourite albums", "Favourite artists", "My playlists",
	"New releases", "Most streamed", "Recommended for you",
};
#define TIDAL_MENU_ROWS ((int)(sizeof(TIDAL_MENU_KINDS) / sizeof(TIDAL_MENU_KINDS[0])))

static void menu_tidal(sonixlink_stream_list_t *out) {
	int order[TIDAL_MENU_ROWS];
	bool seen[TIDAL_MENU_ROWS] = {false};
	int n = 0;
	const char *saved = config_get("tidal", "home_order", "0,1,2,3,4,5,6");
	for (const char *p = saved; p && *p && n < TIDAL_MENU_ROWS; ) {
		if (*p >= '0' && *p < '0' + TIDAL_MENU_ROWS) {
			int id = *p - '0';
			if (!seen[id]) {
				seen[id] = true;
				order[n++] = id;
			}
			p++;
		} else {
			p++;
		}
	}
	if (n == 0) {
		for (int id = 0; id < TIDAL_MENU_ROWS; id++) {
			order[n++] = id;
		}
	} else {
		for (int id = 0; id < TIDAL_MENU_ROWS; id++) {
			if (!seen[id] && n < TIDAL_MENU_ROWS) {
				order[n++] = id;
			}
		}
	}
	long hidden = config_get_int("tidal", "home_hidden", 0);
	for (int slot = 0; slot < n; slot++) {
		int id = order[slot];
		if ((hidden >> id) & 1) {
			continue;
		}
		menu_add(out, TIDAL_MENU_KINDS[id], "", TIDAL_MENU_LABELS[id]);
	}
	snprintf(out->title, sizeof(out->title), "%s", "Tidal");
}

static void list_tidal_full(const char *kind, const char *id, const char *q, int offset, sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "new";
	tidal_track_t tracks[STREAM_PAGE];
	tidal_album_t albums[STREAM_PAGE];
	tidal_artist_t artists[STREAM_PAGE];
	tidal_playlist_t playlists[STREAM_PAGE];
	int count = 0;
	bool album_mode = false;
	bool artist_mode = false;
	bool playlist_mode = false;

	if (strcmp(mode, "search") == 0) {
		if (!q || !q[0]) {
			snprintf(out->error, sizeof(out->error), "empty_search");
			return;
		}
		count = tidal_search_tracks(q, offset, tracks, STREAM_PAGE);
		snprintf(out->title, sizeof(out->title), "%s", q);
	} else if (strcmp(mode, "favorite_tracks") == 0 || strcmp(mode, "favorites") == 0) {
		count = tidal_favorite_tracks(offset, tracks, STREAM_PAGE);
		snprintf(out->title, sizeof(out->title), "%s", "Favourite tracks");
	} else if (strcmp(mode, "favorite_albums") == 0) {
		count = tidal_favorite_albums(offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "Favourite albums");
	} else if (strcmp(mode, "favorite_artists") == 0) {
		count = tidal_favorite_artists(offset, artists, STREAM_PAGE);
		artist_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "Favourite artists");
	} else if (strcmp(mode, "playlists") == 0) {
		count = tidal_user_playlists(offset, playlists, STREAM_PAGE);
		playlist_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "My playlists");
	} else if (strcmp(mode, "album") == 0) {
		count = tidal_album_tracks(id, offset, tracks, STREAM_PAGE);
	} else if (strcmp(mode, "playlist") == 0) {
		count = tidal_playlist_tracks(id, offset, tracks, STREAM_PAGE);
	} else if (strcmp(mode, "artist") == 0) {
		count = tidal_artist_albums(id_number(id), offset, albums, STREAM_PAGE);
		album_mode = true;
	} else if (strcmp(mode, "new") == 0 || strcmp(mode, "featured") == 0) {
		count = tidal_featured_albums("new", offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "New releases");
	} else if (strcmp(mode, "top") == 0) {
		count = tidal_featured_albums("top", offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "Most streamed");
	} else if (strcmp(mode, "recommended") == 0) {
		count = tidal_featured_albums("recommended", offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "Recommended for you");
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_kind");
		return;
	}

	if (count < 0) {
		const char *why = tidal_last_error();
		snprintf(out->error, sizeof(out->error), "%s", why && why[0] ? why : "request_failed");
		return;
	}
	for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
		sonixlink_stream_row_t *row = &out->rows[out->count++];
		if (album_mode) {
			tidal_album_row(&albums[i], row);
		} else if (artist_mode) {
			const tidal_artist_t *a = &artists[i];
			char who[32];
			snprintf(who, sizeof(who), "%d", a->album_count);
			row_set(row, "artist", "", a->name, who);
			snprintf(row->id, sizeof(row->id), "%ld", a->id);
			snprintf(row->art, sizeof(row->art), "%s", a->image);
			row->track_number = a->album_count;
		} else if (playlist_mode) {
			const tidal_playlist_t *p = &playlists[i];
			row_set(row, "playlist", p->id, p->name, p->owner);
			snprintf(row->art, sizeof(row->art), "%s", p->image);
			row->track_number = p->track_count;
		} else {
			tidal_track_row(&tracks[i], row);
		}
	}
	out->more = count >= STREAM_PAGE;
}

// ---------------------------------------------------------------------------
// Qobuz
// ---------------------------------------------------------------------------

static void menu_qobuz(sonixlink_stream_list_t *out) {
	menu_add(out, "favorite_tracks", "", "Favourite tracks");
	menu_add(out, "favorite_albums", "", "Favourite albums");
	menu_add(out, "playlists", "", "My playlists");
	menu_add(out, "new", "", "New releases");
	snprintf(out->title, sizeof(out->title), "%s", "Qobuz");
}

static void list_qobuz_full(const char *kind, const char *id, const char *q, int offset, sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "new";
	qobuz_track_t tracks[STREAM_PAGE];
	qobuz_album_t albums[STREAM_PAGE];
	qobuz_playlist_t playlists[STREAM_PAGE];
	int count = 0;
	bool album_mode = false;
	bool playlist_mode = false;

	if (strcmp(mode, "search") == 0) {
		if (!q || !q[0]) {
			snprintf(out->error, sizeof(out->error), "empty_search");
			return;
		}
		count = qobuz_search_tracks(q, offset, tracks, STREAM_PAGE);
		snprintf(out->title, sizeof(out->title), "%s", q);
	} else if (strcmp(mode, "favorite_tracks") == 0 || strcmp(mode, "favorites") == 0) {
		count = qobuz_favorite_tracks(offset, tracks, STREAM_PAGE);
		snprintf(out->title, sizeof(out->title), "%s", "Favourite tracks");
	} else if (strcmp(mode, "favorite_albums") == 0) {
		count = qobuz_favorite_albums(offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "Favourite albums");
	} else if (strcmp(mode, "playlists") == 0) {
		count = qobuz_user_playlists(offset, playlists, STREAM_PAGE);
		playlist_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "My playlists");
	} else if (strcmp(mode, "album") == 0) {
		count = qobuz_album_tracks(id, offset, tracks, STREAM_PAGE);
	} else if (strcmp(mode, "playlist") == 0) {
		count = qobuz_playlist_tracks(id_number(id), offset, tracks, STREAM_PAGE);
	} else if (strcmp(mode, "artist") == 0) {
		count = qobuz_artist_albums(id_number(id), offset, albums, STREAM_PAGE);
		album_mode = true;
	} else if (strcmp(mode, "new") == 0 || strcmp(mode, "featured") == 0) {
		count = qobuz_featured_albums("new-releases", offset, albums, STREAM_PAGE);
		album_mode = true;
		snprintf(out->title, sizeof(out->title), "%s", "New releases");
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_kind");
		return;
	}

	if (count < 0) {
		const char *why = qobuz_last_error();
		snprintf(out->error, sizeof(out->error), "%s", why && why[0] ? why : "request_failed");
		return;
	}
	for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
		sonixlink_stream_row_t *row = &out->rows[out->count++];
		if (album_mode) {
			qobuz_album_row(&albums[i], row);
		} else if (playlist_mode) {
			const qobuz_playlist_t *p = &playlists[i];
			char id_text[32];
			snprintf(id_text, sizeof(id_text), "%ld", p->id);
			row_set(row, "playlist", id_text, p->name, p->owner);
			snprintf(row->art, sizeof(row->art), "%s", p->image);
			row->track_number = p->track_count;
		} else {
			qobuz_track_row(&tracks[i], row);
		}
	}
	out->more = count >= STREAM_PAGE;
}

// ---------------------------------------------------------------------------
// YouTube Music
// ---------------------------------------------------------------------------

static const char *const YT_MENU_KINDS[] = {"explore", "charts", "favourites", "podcasts"};
static const char *const YT_MENU_LABELS[] = {"Explore", "Top songs", "Favourites", "Podcasts"};
#define YT_MENU_ROWS ((int)(sizeof(YT_MENU_KINDS) / sizeof(YT_MENU_KINDS[0])))

static void menu_youtube(sonixlink_stream_list_t *out) {
	int order[YT_MENU_ROWS];
	bool seen[YT_MENU_ROWS] = {false};
	int n = 0;
	// The page's arrangement used to count from one, with Search as zero, and
	// yt_home_migrate() rewrites it the first time that page is drawn. A phone
	// asking before then reads the old numbers, so they are shifted here the
	// same way, without writing the config from a request thread.
	int shift = config_get_int("ytmusic", "home_rows_version", 0) >= 1 ? 0 : 1;
	const char *saved = config_get("ytmusic", "home_order", "");
	if (saved && saved[0]) {
		for (const char *p = saved; *p && n < YT_MENU_ROWS; ) {
			if (*p >= '0' && *p <= '9') {
				int id = atoi(p) - shift;
				while (*p >= '0' && *p <= '9') p++;
				if (id >= 0 && id < YT_MENU_ROWS && !seen[id]) {
					seen[id] = true;
					order[n++] = id;
				}
			} else {
				p++;
			}
		}
	}
	if (n == 0) {
		for (int id = 0; id < YT_MENU_ROWS; id++) {
			order[n++] = id;
		}
	} else {
		for (int id = 0; id < YT_MENU_ROWS; id++) {
			if (!seen[id] && n < YT_MENU_ROWS) {
				order[n++] = id;
			}
		}
	}

	bool hidden[YT_MENU_ROWS] = {false};
	const char *off = config_get("ytmusic", "home_hidden", "");
	for (const char *p = off; p && *p; ) {
		if (*p >= '0' && *p <= '9') {
			int id = atoi(p) - shift;
			while (*p >= '0' && *p <= '9') p++;
			if (id >= 0 && id < YT_MENU_ROWS) {
				hidden[id] = true;
			}
		} else {
			p++;
		}
	}
	for (int slot = 0; slot < n; slot++) {
		int id = order[slot];
		if (!hidden[id]) {
			menu_add(out, YT_MENU_KINDS[id], "", YT_MENU_LABELS[id]);
		}
	}
	snprintf(out->title, sizeof(out->title), "%s", "YouTube Music");
}

static void list_youtube_full(const char *kind, const char *id, const char *q, sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "explore";
	if (strcmp(mode, "favourites") == 0) {
		snprintf(out->title, sizeof(out->title), "%s", "Favourites");
		if (ytmusic_session_cookie_attached()) {
			ytmusic_item_t items[STREAM_PAGE];
			char error[192] = "";
			int count = ytmusic_browse_items("VLLM", items, STREAM_PAGE, error, sizeof(error));
			if (count < 0) {
				snprintf(out->error, sizeof(out->error), "%s", error[0] ? error : "request_failed");
				return;
			}
			for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
				yt_row(&items[i], &out->rows[out->count++]);
			}
			out->more = count >= STREAM_PAGE;
			return;
		}
		int count = ytlikes_count();
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			const ytlike_t *like = ytlikes_at(i);
			if (!like || !like->id[0]) {
				continue;
			}
			ytmusic_item_t item;
			memset(&item, 0, sizeof(item));
			item.kind = YTMUSIC_ITEM_TRACK;
			snprintf(item.id, sizeof(item.id), "%s", like->id);
			snprintf(item.title, sizeof(item.title), "%s", like->title);
			snprintf(item.subtitle, sizeof(item.subtitle), "%s", like->artist);
			snprintf(item.album, sizeof(item.album), "%s", like->album);
			item.duration = like->duration;
			yt_row(&item, &out->rows[out->count++]);
		}
		return;
	}
	if (strcmp(mode, "podcasts") == 0) {
		ytmusic_item_t items[STREAM_PAGE];
		char error[192] = "";
		int count = ytmusic_search_items("podcast", YTMUSIC_FILTER_PODCASTS, items, STREAM_PAGE, error,
									 sizeof(error));
		if (count < 0) {
			snprintf(out->error, sizeof(out->error), "%s", error[0] ? error : "request_failed");
			return;
		}
		snprintf(out->title, sizeof(out->title), "%s", "Podcasts");
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			yt_row(&items[i], &out->rows[out->count++]);
		}
		out->more = count >= STREAM_PAGE;
		return;
	}
	list_youtube(kind, id, q, out);
}

// ---------------------------------------------------------------------------
// Podcasts
// ---------------------------------------------------------------------------

static void menu_podcasts(sonixlink_stream_list_t *out) {
	menu_add(out, "followed", "", "My podcasts");
	menu_add(out, "trending", "", "Trending");
	snprintf(out->title, sizeof(out->title), "%s", "Podcasts");
}

// ---------------------------------------------------------------------------
// Internet radio
// ---------------------------------------------------------------------------

static const char *const RADIO_SLUGS[] = {
	"recent", "language", "country", "genre", "custom", "recommended", "favourites", "flylo",
};
static const char *const RADIO_LABELS[] = {
	"Recently played", "Language", "Country", "Genre", "My stations", "Recommended", "Favourites",
	"Flylo favs",
};
static const char *const RADIO_KINDS[] = {
	"recent", "terms", "terms", "terms", "custom", "recommended", "favorites", "flylo",
};
static const char *const RADIO_IDS[] = {"", "language", "country", "genre", "", "", "", ""};
#define RADIO_MENU_ROWS 8
static const int RADIO_DEFAULT_ORDER[RADIO_MENU_ROWS] = {0, 6, 7, 5, 1, 2, 3, 4};

static void menu_radio(sonixlink_stream_list_t *out) {
	int order[RADIO_MENU_ROWS];
	bool seen[RADIO_MENU_ROWS] = {false};
	int n = 0;
	const char *saved = config_get("radio", "hub_order", "");
	if (saved && saved[0]) {
		const char *p = saved;
		while (*p && n < RADIO_MENU_ROWS) {
			while (*p == ',' || *p == ' ') p++;
			const char *start = p;
			while (*p && *p != ',' && *p != ' ') p++;
			size_t len = (size_t)(p - start);
			for (int id = 0; id < RADIO_MENU_ROWS; id++) {
				if (!seen[id] && strlen(RADIO_SLUGS[id]) == len && strncmp(RADIO_SLUGS[id], start, len) == 0) {
					seen[id] = true;
					order[n++] = id;
					break;
				}
			}
		}
	}
	if (n == 0) {
		for (int i = 0; i < RADIO_MENU_ROWS; i++) {
			order[n++] = RADIO_DEFAULT_ORDER[i];
			seen[RADIO_DEFAULT_ORDER[i]] = true;
		}
	} else {
		for (int id = 0; id < RADIO_MENU_ROWS; id++) {
			if (!seen[id] && n < RADIO_MENU_ROWS) {
				order[n++] = id;
			}
		}
	}
	long hidden = config_get_int("radio", "hub_hidden", 0);
	for (int slot = 0; slot < n; slot++) {
		int id = order[slot];
		if ((hidden >> id) & 1) {
			continue;
		}
		menu_add(out, RADIO_KINDS[id], RADIO_IDS[id], RADIO_LABELS[id]);
	}
	snprintf(out->title, sizeof(out->title), "%s", "Internet radio");
}

typedef struct {
	const char *id;
	const char *name;
	const char *url;
	const char *icon;
} flylo_station_t;

static const flylo_station_t FLYLO_STATIONS[] = {
	{"ilove-chillhop", "I Love Chillhop", "https://streams.ilovemusic.de/iloveradio17.mp3",
	 "https://ilovemusic.de/favicon.png"},
	{"ilove-chillhop-2", "ChillHop", "http://stream01.iloveradio.de/iloveradio17.mp3",
	 "https://ilovemusic.de/favicon.png"},
	{"fluxfm-chillhop", "Flux Fm Chillhop", "https://streams.fluxfm.de/Chillhop/mp3-320/streams.fluxfm.de/",
	 "https://www.fluxfm.de/assets/favicons/apple-icon-180x180.png"},
	{"le-mellotron", "Le Mellotron", "https://listen.radioking.com/radio/477719/stream/534044",
	 "https://cdn.prod.website-files.com/62288d6c9e6c33efb4e67d74/622895c09e6c33d606e6e822_Icone-webclip.png"},
	{"zeno-lofi", "Lo-Fi Hip-Hop Radio", "https://stream.zeno.fm/0r0xa792kwzuv",
	 "https://zeno.fm/icon-192x192.png"},
};
#define FLYLO_STATION_COUNT ((int)(sizeof(FLYLO_STATIONS) / sizeof(FLYLO_STATIONS[0])))

static void list_radio_full(const char *kind, const char *id, const char *q, sonixlink_stream_list_t *out) {
	const char *mode = kind && kind[0] ? kind : "recommended";
	if (strcmp(mode, "recent") == 0) {
		int count = radio_recent_count();
		for (int i = 0; i < count && out->count < SONIXLINK_STREAM_ROWS; i++) {
			radio_station_t station;
			if (!radio_recent_get(i, &station)) {
				continue;
			}
			radio_station_row(&station, &out->rows[out->count++]);
		}
		snprintf(out->title, sizeof(out->title), "%s", "Recently played");
		return;
	}
	if (strcmp(mode, "flylo") == 0) {
		for (int i = 0; i < FLYLO_STATION_COUNT && out->count < SONIXLINK_STREAM_ROWS; i++) {
			const flylo_station_t *f = &FLYLO_STATIONS[i];
			radio_station_t station;
			memset(&station, 0, sizeof(station));
			snprintf(station.uuid, sizeof(station.uuid), "flylo:%s", f->id);
			snprintf(station.name, sizeof(station.name), "%s", f->name);
			snprintf(station.url, sizeof(station.url), "%s", f->url);
			snprintf(station.favicon, sizeof(station.favicon), "%s", f->icon);
			snprintf(station.codec, sizeof(station.codec), "%s", "MP3");
			radio_station_row(&station, &out->rows[out->count++]);
		}
		snprintf(out->title, sizeof(out->title), "%s", "Flylo favs");
		return;
	}
	list_radio(kind, id, q, out);
}

// ---------------------------------------------------------------------------
// The dispatcher
// ---------------------------------------------------------------------------

bool sonixlink_stream_menu(const char *service, sonixlink_stream_list_t *out) {
	if (!service || !service[0] || !out) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	if (strcmp(service, "youtube") == 0) {
		menu_youtube(out);
	} else if (strcmp(service, "tidal") == 0) {
		menu_tidal(out);
	} else if (strcmp(service, "qobuz") == 0) {
		menu_qobuz(out);
	} else if (strcmp(service, "podcasts") == 0) {
		menu_podcasts(out);
	} else if (strcmp(service, "radio") == 0) {
		menu_radio(out);
	} else {
		snprintf(out->error, sizeof(out->error), "unknown_service");
		return false;
	}
	return true;
}
