#include "addons.h"

#include <stddef.h>
#include <string.h>

#include "src/system/core/config.h"

// The list, and nothing else in this file knows what any of them do: the gate
// is asked by the page that would build the module, and by the menu that would
// offer it. Adding a module is a row here, a switch that never leaves the
// player, and the two questions -- "may I build?" -- at the places that build.
//
// Order is the Plugins page's: the services first, because they are the ones
// people actually turn off, then what belongs beside the music, then the tools.
// The section tag is on every row so the page never has to know which id is
// what -- a module added here lands under the right heading by itself.
static const addon_t ADDONS[] = {
	{"tidal", "tidal", "addon_tidal_note", "plugins_section_services"},
	{"qobuz", "qobuz", "addon_qobuz_note", "plugins_section_services"},
	{"ytmusic", "YouTube Music", "addon_ytmusic_note", "plugins_section_services"},
	{"podcasts", "podcasts", "addon_podcasts_note", "plugins_section_services"},
	{"radio", "radio", "addon_radio_note", "plugins_section_services"},
	{"subsonic", "subsonic", "addon_subsonic_note", "plugins_section_services"},
	// What talks to a pair of earbuds over the Bluetooth link rather than to a
	// service: the AirPods' battery and noise control through Apple's own
	// protocol, and the EarFun family's through GAIA. Off means the worker that
	// speaks it is never started and the row that leads to its page is not in
	// the Bluetooth page -- the memory a socket and a parser hold stays free.
	{"airpods", "airpods", "addon_airpods_note", "plugins_section_earbuds"},
	{"earfun", "earfun", "addon_earfun_note", "plugins_section_earbuds"},
	{"speex_resampler", "btaudio_speex_resampler", "Speex sample-rate conversion for Bluetooth audio.",
	 "Bluetooth audio"},
	// The shelves are a place of their own: a book is not a music feature, and
	// nobody would look for it under one.
	{"audiobooks", "audiobooks", "addon_audiobooks_note", "plugins_section_library"},
	{"ebook", "books", "addon_ebook_note", "plugins_section_library"},
	// Player arrangements share the player page; their switches decide which
	// choices and layout-specific resources are available. Poster is the user's
	// preferred default; the other layouts and Cover Flow stay opt-in.
	{"layout_standard", "Standard", "The classic now-playing screen.", "Player layouts"},
	{"layout_waveform", "Waveform", "Waveform display for local tracks.", "Player layouts"},
	{"layout_alternative", "Alternative", "Compact now-playing screen for any source.", "Player layouts"},
	{"layout_immersive", "Immersive", "Full-screen album artwork with overlaid controls.", "Player layouts"},
	{"layout_poster", "Poster", "Poster artwork with playback controls below.", "Player layouts"},
	{"layout_aam", "Album Art Max", "Full-screen artwork-focused layout for TempoTec V1.", "Player layouts"},
	{"layout_vinyl", "Vinyl", "Classic vinyl record sleeve layout.", "Player layouts"},
	{"coverflow", "Cover Flow", "Optional album carousel and cover cache.", "Player layouts"},
	// What the player does WITH the music: words on screen, a curve for a pair
	// of headphones, a line to two scrobblers. Four features, not four places,
	// which is why the heading says so.
	{"lyrics", "lyrics_fetcher", "addon_lyrics_note", "plugins_section_features"},
	// One artwork module, one switch: the search and the writing of what it
	// found into a file's tags are one act from where the user sits, and two
	// toggles for it only asked which of the two was holding the feature back.
	{"artwork", "cover_art_fetcher", "addon_artwork_note", "plugins_section_features"},
	{"visual_effects", "Visual Effects", "Frosted and album-tinted status bar.", "plugins_section_features"},
	// The three ways this player shapes the sound, each a module of its own:
	// the graphic equaliser, the parametric one and MSEB. Switched off, their
	// pages are never built, their rows leave Music settings and their buttons
	// leave the control centre.
	{"eq", "equaliser", "addon_eq_note", "plugins_section_features"},
	{"peq", "peq", "addon_peq_note", "plugins_section_features"},
	{"mseb", "mseb", "addon_mseb_note", "plugins_section_features"},
	{"mono", "musicsettings_mono_audio", "Downmix both channels into one.", "plugins_section_features"},
	{"soundfield", "musicsettings_soundfield", "Stereo width processing.", "plugins_section_features"},
	{"crossfeed", "musicsettings_crossfeed", "Headphone crossfeed processing.", "plugins_section_features"},
	{"channel_balance", "musicsettings_channel_balance", "Left/right channel level correction.", "plugins_section_features"},
	{"volume_limit", "musicsettings_volume_limit", "Per-output maximum volume safety limit.", "plugins_section_features"},
	// The sleep timers belong to the music transport rather than to a service,
	// so they are switched here like any other module: off, no timer is drawn
	// anywhere and nothing counts down.
	{"sleep_timer", "sleep_timer", "addon_sleep_timer_note", "plugins_section_features"},
	{"autoeq", "autoeq", "addon_autoeq_note", "plugins_section_features"},
	{"lastfm", "last_fm", "addon_lastfm_note", "plugins_section_features"},
	{"listenbrainz", "listenbrainz", "addon_listenbrainz_note", "plugins_section_features"},
	{"calculator", "calculator", "addon_calculator_note", "plugins_section_tools"},
	{"notes", "notes", "addon_notes_note", "plugins_section_tools"},
	{"tagmanager", "tag_manager", "addon_tagmanager_note", "plugins_section_tools"},
	{"metronome", "metronome", "addon_metronome_note", "plugins_section_tools"},
#if GB_CORE
	{"gearboy", "gearboy", "addon_gearboy_note", "plugins_section_games"},
#endif
	{"flappy", "flappy_bird", "addon_flappy_note", "plugins_section_games"},
	{"game2048", "2048", "addon_2048_note", "plugins_section_games"},
	{"minesweeper", "minesweeper", "addon_minesweeper_note", "plugins_section_games"},
	{"sudoku", "sudoku", "addon_sudoku_note", "plugins_section_games"},
	{"guess_song", "guess_song", "addon_guess_song_note", "plugins_section_games"},
	{"stack_it", "stack_it", "addon_stack_it_note", "plugins_section_games"},
	{"brick_breaker", "brick_breaker", "addon_brick_breaker_note", "plugins_section_games"},
	{"pebble_deluxe", "pebble_deluxe", "addon_pebble_deluxe_note", "plugins_section_games"},
};

int addons_count(void) { return (int)(sizeof(ADDONS) / sizeof(ADDONS[0])); }

const addon_t *addons_at(int index) {
	if (index < 0 || index >= addons_count()) {
		return NULL;
	}
	return &ADDONS[index];
}

const addon_t *addons_find(const char *id) {
	if (!id) {
		return NULL;
	}
	for (int i = 0; i < addons_count(); i++) {
		if (strcmp(ADDONS[i].id, id) == 0) {
			return &ADDONS[i];
		}
	}
	return NULL;
}

static bool addon_is_default_on(const char *id) {
	if (!id) {
		return false;
	}
	// Fresh install defaults. These defaults keep Music settings available and
	// preserve behavior for effects users could already enable in older builds.
	if (strcmp(id, "audiobooks") == 0) return true;
	if (strcmp(id, "subsonic") == 0) return true;
	if (strcmp(id, "eq") == 0) return true;
	if (strcmp(id, "peq") == 0) return true;
	if (strcmp(id, "mseb") == 0) return true;
	// These controls used to be available on every install. Keep them visible
	// by default so existing settings and behavior remain unchanged on upgrade.
	if (strcmp(id, "mono") == 0) return true;
	if (strcmp(id, "soundfield") == 0) return true;
	if (strcmp(id, "crossfeed") == 0) return true;
	if (strcmp(id, "channel_balance") == 0) return true;
	if (strcmp(id, "volume_limit") == 0) return true;
	if (strcmp(id, "speex_resampler") == 0) return true;
	if (strcmp(id, "layout_poster") == 0) return true;
	return false;
}

bool addons_enabled(const char *id) {
	if (!id) {
		return true;
	}
	return config_get_bool("addons", id, addon_is_default_on(id));
}

bool addons_set(const char *id, bool on) {
	if (!id || !addons_find(id)) {
		return false;
	}
	bool previous = addons_enabled(id);
	config_set_bool("addons", id, on);
	if (addons_enabled(id) != on || !config_save()) {
		config_set_bool("addons", id, previous);
		config_save();
		return false;
	}
	return true;
}

void addons_apply_defaults(bool (*is_default)(const char *id)) {
	if (!is_default) {
		return;
	}
	for (int i = 0; i < addons_count(); i++) {
		config_set_bool("addons", ADDONS[i].id, is_default(ADDONS[i].id));
	}
	config_save();
}

bool addons_any_streaming_enabled(void) {
	return addons_enabled("radio") ||
	       addons_enabled("podcasts") ||
	       addons_enabled("tidal") ||
	       addons_enabled("qobuz") ||
	       addons_enabled("subsonic") ||
	       addons_enabled("ytmusic");
}

bool addons_any_game_enabled(void) {
#if GB_CORE
	if (addons_enabled("gearboy")) return true;
#endif
	return addons_enabled("flappy") ||
	       addons_enabled("game2048") ||
	       addons_enabled("minesweeper") ||
	       addons_enabled("sudoku") ||
	       addons_enabled("guess_song") ||
	       addons_enabled("stack_it") ||
	       addons_enabled("brick_breaker") ||
	       addons_enabled("pebble_deluxe");
}
