//! The optional modules of the player, as a list rather than a heap of
//! `#ifdef`s, ported from `src/system/addons/addons.c`.
//!
//! The player is one binary: a module's code is always linked in. What "off"
//! means is that nothing of that module is *built* — its pages are never
//! created, its rows are not in the menus — so the RAM those pages would hold
//! stays free for the music. That is the memory that matters on a player with a
//! fixed heap; flash is trimmed with the build instead.
//!
//! Each module is identified by a short id, which is also the key its switch is
//! stored under in the config's `[addons]` section, so a switch survives updates
//! and a module added later starts on.
//!
//! Only the list lives here. Reading and writing the switches is
//! `config_get_bool`/`config_set_bool`, which is `sonix-ffi`'s side of the
//! boundary — this module never touches the settings store itself.

use core::ffi::c_char;
use std::ffi::CStr;
use std::sync::OnceLock;

/// One module. Laid out to match the C `addon_t`: four `const char *`.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct Addon {
    /// `"tidal"`, `"calculator"`: the config key and the gate.
    pub id: *const c_char,
    /// The row's name, a tag or a literal (`"YouTube Music"`).
    pub label: *const c_char,
    /// One line on what it adds, a tag.
    pub note: *const c_char,
    /// The Plugins page heading it belongs under, a tag.
    pub section: *const c_char,
}

// Every pointer in an `Addon` aims at a `c"..."` literal, which is `'static` and
// immutable, so sharing one between threads is sound. The C side shares these
// rows across threads for the same reason. `Send` is needed only because the
// table lives in a `OnceLock`; nothing here is ever moved out of it.
unsafe impl Sync for Addon {}
unsafe impl Send for Addon {}

const fn row(
    id: &'static CStr,
    label: &'static CStr,
    note: &'static CStr,
    section: &'static CStr,
) -> Addon {
    Addon {
        id: id.as_ptr(),
        label: label.as_ptr(),
        note: note.as_ptr(),
        section: section.as_ptr(),
    }
}

/// The list, and nothing else in this file knows what any of them do: the gate
/// is asked by the page that would build the module, and by the menu that would
/// offer it. Adding a module is a row here, a switch that never leaves the
/// player, and the two questions — "may I build?" — at the places that build.
///
/// Order is the Plugins page's: the services first, because they are the ones
/// people actually turn off, then what belongs beside the music, then the tools.
/// The section tag is on every row so the page never has to know which id is
/// what — a module added here lands under the right heading by itself.
fn build_table() -> Vec<Addon> {
    let mut v: Vec<Addon> = Vec::with_capacity(48);

    // Services.
    v.push(row(
        c"tidal",
        c"tidal",
        c"addon_tidal_note",
        c"plugins_section_services",
    ));
    v.push(row(
        c"qobuz",
        c"qobuz",
        c"addon_qobuz_note",
        c"plugins_section_services",
    ));
    v.push(row(
        c"ytmusic",
        c"YouTube Music",
        c"addon_ytmusic_note",
        c"plugins_section_services",
    ));
    v.push(row(
        c"podcasts",
        c"podcasts",
        c"addon_podcasts_note",
        c"plugins_section_services",
    ));
    v.push(row(
        c"radio",
        c"radio",
        c"addon_radio_note",
        c"plugins_section_services",
    ));
    v.push(row(
        c"subsonic",
        c"subsonic",
        c"addon_subsonic_note",
        c"plugins_section_services",
    ));

    // What talks to a pair of earbuds over the Bluetooth link rather than to a
    // service: the AirPods' battery and noise control through Apple's own
    // protocol, and the EarFun family's through GAIA. Off means the worker that
    // speaks it is never started and the row that leads to its page is not in
    // the Bluetooth page — the memory a socket and a parser hold stays free.
    v.push(row(
        c"airpods",
        c"airpods",
        c"addon_airpods_note",
        c"plugins_section_earbuds",
    ));
    v.push(row(
        c"earfun",
        c"earfun",
        c"addon_earfun_note",
        c"plugins_section_earbuds",
    ));
    v.push(row(
        c"speex_resampler",
        c"btaudio_speex_resampler",
        c"Speex sample-rate conversion for Bluetooth audio.",
        c"Bluetooth audio",
    ));

    // The shelves are a place of their own: a book is not a music feature, and
    // nobody would look for it under one.
    v.push(row(
        c"audiobooks",
        c"audiobooks",
        c"addon_audiobooks_note",
        c"plugins_section_library",
    ));
    v.push(row(
        c"ebook",
        c"books",
        c"addon_ebook_note",
        c"plugins_section_library",
    ));

    // Player arrangements share the player page; their switches decide which
    // choices and layout-specific resources are available. Poster is the user's
    // preferred default; the other layouts and Cover Flow stay opt-in.
    v.push(row(
        c"layout_standard",
        c"Standard",
        c"The classic now-playing screen.",
        c"Player layouts",
    ));
    v.push(row(
        c"layout_waveform",
        c"Waveform",
        c"Waveform display for local tracks.",
        c"Player layouts",
    ));
    v.push(row(
        c"layout_alternative",
        c"Alternative",
        c"Compact now-playing screen for any source.",
        c"Player layouts",
    ));
    v.push(row(
        c"layout_immersive",
        c"Immersive",
        c"Full-screen album artwork with overlaid controls.",
        c"Player layouts",
    ));
    v.push(row(
        c"layout_poster",
        c"Poster",
        c"Poster artwork with playback controls below.",
        c"Player layouts",
    ));
    v.push(row(
        c"layout_aam",
        c"Album Art Max",
        c"Full-screen artwork-focused layout for TempoTec V1.",
        c"Player layouts",
    ));
    v.push(row(
        c"coverflow",
        c"Cover Flow",
        c"Optional album carousel and cover cache.",
        c"Player layouts",
    ));

    // What the player does WITH the music: words on screen, a curve for a pair
    // of headphones, a line to two scrobblers. Four features, not four places,
    // which is why the heading says so.
    v.push(row(
        c"lyrics",
        c"lyrics_fetcher",
        c"addon_lyrics_note",
        c"plugins_section_features",
    ));
    // One artwork module, one switch: the search and the writing of what it
    // found into a file's tags are one act from where the user sits, and two
    // toggles for it only asked which of the two was holding the feature back.
    v.push(row(
        c"artwork",
        c"cover_art_fetcher",
        c"addon_artwork_note",
        c"plugins_section_features",
    ));
    // The three ways this player shapes the sound, each a module of its own:
    // the graphic equaliser, the parametric one and MSEB. Switched off, their
    // pages are never built, their rows leave Music settings and their buttons
    // leave the control centre.
    v.push(row(
        c"eq",
        c"equaliser",
        c"addon_eq_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"peq",
        c"peq",
        c"addon_peq_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"mseb",
        c"mseb",
        c"addon_mseb_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"mono",
        c"musicsettings_mono_audio",
        c"Downmix both channels into one.",
        c"plugins_section_features",
    ));
    v.push(row(
        c"soundfield",
        c"musicsettings_soundfield",
        c"Stereo width processing.",
        c"plugins_section_features",
    ));
    v.push(row(
        c"crossfeed",
        c"musicsettings_crossfeed",
        c"Headphone crossfeed processing.",
        c"plugins_section_features",
    ));
    v.push(row(
        c"channel_balance",
        c"musicsettings_channel_balance",
        c"Left/right channel level correction.",
        c"plugins_section_features",
    ));
    v.push(row(
        c"volume_limit",
        c"musicsettings_volume_limit",
        c"Per-output maximum volume safety limit.",
        c"plugins_section_features",
    ));
    // The sleep timers belong to the music transport rather than to a service,
    // so they are switched here like any other module: off, no timer is drawn
    // anywhere and nothing counts down.
    v.push(row(
        c"sleep_timer",
        c"sleep_timer",
        c"addon_sleep_timer_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"autoeq",
        c"autoeq",
        c"addon_autoeq_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"lastfm",
        c"last_fm",
        c"addon_lastfm_note",
        c"plugins_section_features",
    ));
    v.push(row(
        c"listenbrainz",
        c"listenbrainz",
        c"addon_listenbrainz_note",
        c"plugins_section_features",
    ));

    // Tools.
    v.push(row(
        c"calculator",
        c"calculator",
        c"addon_calculator_note",
        c"plugins_section_tools",
    ));
    v.push(row(
        c"notes",
        c"notes",
        c"addon_notes_note",
        c"plugins_section_tools",
    ));
    v.push(row(
        c"tagmanager",
        c"tag_manager",
        c"addon_tagmanager_note",
        c"plugins_section_tools",
    ));
    v.push(row(
        c"metronome",
        c"metronome",
        c"addon_metronome_note",
        c"plugins_section_tools",
    ));

    // Games. Gearboy is C++ and only in a build that carries it, which is what
    // `GB_CORE` means on the C side and the `gb-core` feature means here.
    #[cfg(feature = "gb-core")]
    v.push(row(
        c"gearboy",
        c"gearboy",
        c"addon_gearboy_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"flappy",
        c"flappy_bird",
        c"addon_flappy_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"game2048",
        c"2048",
        c"addon_2048_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"minesweeper",
        c"minesweeper",
        c"addon_minesweeper_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"sudoku",
        c"sudoku",
        c"addon_sudoku_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"guess_song",
        c"guess_song",
        c"addon_guess_song_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"stack_it",
        c"stack_it",
        c"addon_stack_it_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"brick_breaker",
        c"brick_breaker",
        c"addon_brick_breaker_note",
        c"plugins_section_games",
    ));
    v.push(row(
        c"pebble_deluxe",
        c"pebble_deluxe",
        c"addon_pebble_deluxe_note",
        c"plugins_section_games",
    ));

    v
}

/// The modules this build carries, in the order the Plugins page lists them.
///
/// Entries that share a section are drawn under one heading, so the list is
/// ordered by section first: a page that walked it unordered would print the
/// heading again at every change of group.
pub fn table() -> &'static [Addon] {
    static TABLE: OnceLock<Vec<Addon>> = OnceLock::new();
    TABLE.get_or_init(build_table).as_slice()
}

pub fn count() -> usize {
    table().len()
}

pub fn at(index: i32) -> Option<&'static Addon> {
    if index < 0 {
        return None;
    }
    table().get(index as usize)
}

pub fn find(id: &str) -> Option<&'static Addon> {
    table()
        .iter()
        .find(|a| unsafe { CStr::from_ptr(a.id) }.to_bytes() == id.as_bytes())
}

/// Fresh install defaults. These keep Music settings available and preserve
/// behavior for effects users could already enable in older builds.
///
/// The second group used to be available on every install; keeping them visible
/// by default leaves existing settings and behaviour unchanged on upgrade.
pub fn is_default_on(id: &str) -> bool {
    matches!(
        id,
        "audiobooks"
            | "subsonic"
            | "eq"
            | "peq"
            | "mseb"
            | "mono"
            | "soundfield"
            | "crossfeed"
            | "channel_balance"
            | "volume_limit"
            | "speex_resampler"
            | "layout_poster"
    )
}

/// The ids that count as a game, for the menu that hides the whole Arcade when
/// none of them is on.
pub fn game_ids() -> &'static [&'static str] {
    if cfg!(feature = "gb-core") {
        &[
            "gearboy",
            "flappy",
            "game2048",
            "minesweeper",
            "sudoku",
            "guess_song",
            "stack_it",
            "brick_breaker",
            "pebble_deluxe",
        ]
    } else {
        &[
            "flappy",
            "game2048",
            "minesweeper",
            "sudoku",
            "guess_song",
            "stack_it",
            "brick_breaker",
            "pebble_deluxe",
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn id_of(a: &Addon) -> String {
        unsafe { CStr::from_ptr(a.id) }
            .to_string_lossy()
            .into_owned()
    }

    fn section_of(a: &Addon) -> String {
        unsafe { CStr::from_ptr(a.section) }
            .to_string_lossy()
            .into_owned()
    }

    #[test]
    fn the_list_is_what_the_plugins_page_draws() {
        assert_eq!(count(), 45, "services, features, tools and games");
        assert_eq!(id_of(at(0).unwrap()), "tidal");
        assert_eq!(at(count() as i32).map(id_of), None);
        assert_eq!(at(-1).map(id_of), None);
    }

    #[test]
    fn every_id_is_unique() {
        let mut seen = std::collections::HashSet::new();
        for addon in table() {
            assert!(seen.insert(id_of(addon)), "duplicate id {:?}", id_of(addon));
        }
    }

    /// The page prints a heading when the section changes, so a list that
    /// returns to a section after leaving it would print it twice.
    #[test]
    fn sections_come_in_one_run_each() {
        let mut sections: Vec<String> = Vec::new();
        for addon in table() {
            let section = section_of(addon);
            if sections.last() != Some(&section) {
                assert!(
                    !sections.contains(&section),
                    "section {section:?} appears in two runs"
                );
                sections.push(section);
            }
        }
    }

    #[test]
    fn find_is_exact() {
        assert_eq!(find("tidal").map(id_of).as_deref(), Some("tidal"));
        assert_eq!(find("Tidal").map(id_of), None);
        assert_eq!(find("tida").map(id_of), None);
        assert_eq!(find("").map(id_of), None);
        assert_eq!(find("gearboy").map(id_of).as_deref(), Some("gearboy"));
    }

    #[test]
    fn the_defaults_are_the_ones_that_were_already_on() {
        assert!(is_default_on("audiobooks"));
        assert!(is_default_on("eq"));
        assert!(is_default_on("layout_poster"));
        assert!(!is_default_on("tidal"));
        assert!(!is_default_on("gearboy"));
        assert!(!is_default_on(""));
    }

    #[test]
    fn every_game_id_is_in_the_list() {
        for id in game_ids() {
            assert!(find(id).is_some(), "{id} is not a module");
        }
    }
}
