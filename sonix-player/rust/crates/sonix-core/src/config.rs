//! The settings store, ported from `src/system/core/config.c`.
//!
//! On the device this lives at `/usr/data/device_config.ini` — the UBIFS
//! partition, which is the only writable place that is always there (the
//! rootfs is read-only and the card may not be inserted). The format is a
//! plain INI so it can be read and edited over ADB without any tooling:
//!
//! ```ini
//! [ui]
//! theme = dark
//!
//! [clock]
//! configured = 1
//! last_seen = 1786000000
//! ```
//!
//! ## Why the entries are fixed-size
//!
//! The C version keeps a static array of `config_entry_t`, each holding three
//! `char` arrays, so a player's settings cost a fixed ~2 KB of the binary's
//! `.bss` and no allocation at runtime. More importantly for the C API:
//! `config_get()` hands back a pointer *into* that array, and the caller keeps
//! it until the next set. Rebuilding the value as a `String` on every set would
//! free the buffer the caller is holding.
//!
//! This port keeps the same shape for the same reason: [`Entry`] is three byte
//! arrays, and the entry array is sized once and never grows, so
//! [`Store::value_ptr`] can hand C a pointer that stays valid exactly as long
//! as the C one did.
//!
//! The C code truncates with `snprintf` into these arrays, so a section longer
//! than 31 characters is cut rather than rejected. That is kept.

/// The settings file. A player's settings are a handful of short strings, so a
/// fixed table keeps the whole thing allocation-free, which matters more on a
/// device with ten megabytes free than an unbounded number of entries would.
///
/// A full table is not an error anybody sees: [`Store::set`] drops every
/// further key with only a line in the log, and the symptom is a setting that
/// will not stick -- the switch moves, the value is never stored, and the
/// getter keeps returning its default. The C version counts 126 literal keys
/// plus `band0..9` for the equaliser and `v0..9` for MSEB, about 146 fixed
/// entries; device-specific settings such as saved wireless names can grow
/// that, so the margin is wide.
pub const MAX_ENTRIES: usize = 512;

/// The reader's own file. Its entries are one per book ever opened, so it grows
/// with the shelf rather than with the player, which is the whole reason it is
/// not in the table above.
pub const EBOOK_MAX_ENTRIES: usize = 512;

const SECTION_LEN: usize = 32;
const KEY_LEN: usize = 32;
const VALUE_LEN: usize = 128;

/// One stored key, laid out as the C `config_entry_t`.
#[derive(Clone, Copy)]
pub struct Entry {
    section: [u8; SECTION_LEN],
    key: [u8; KEY_LEN],
    value: [u8; VALUE_LEN],
}

impl Entry {
    const EMPTY: Entry = Entry {
        section: [0; SECTION_LEN],
        key: [0; KEY_LEN],
        value: [0; VALUE_LEN],
    };

    #[inline]
    fn section(&self) -> &str {
        cstr(&self.section)
    }

    #[inline]
    fn key(&self) -> &str {
        cstr(&self.key)
    }

    #[inline]
    fn value(&self) -> &str {
        cstr(&self.value)
    }

    #[inline]
    fn matches(&self, section: &str, key: &str) -> bool {
        self.section() == section && self.key() == key
    }
}

/// The bytes of a NUL-terminated field.
fn cstr(buf: &[u8]) -> &str {
    let end = buf.iter().position(|&b| b == 0).unwrap_or(buf.len());
    // The store only ever writes UTF-8 it was handed as &str, so this holds;
    // lossy is the safe answer if a hand-edited file broke that.
    core::str::from_utf8(&buf[..end]).unwrap_or("")
}

/// `snprintf(dst, dst.len(), "%s", src)`: copy what fits, terminate, drop the
/// rest.
fn sized_write(dst: &mut [u8], src: &str) {
    let bytes = src.as_bytes();
    let n = bytes.len().min(dst.len() - 1);
    dst[..n].copy_from_slice(&bytes[..n]);
    dst[n] = 0;
}

/// One settings file, held in memory.
pub struct Store {
    entries: Vec<Entry>,
    count: usize,
    path: [u8; 512],
    dirty: bool,
}

impl Store {
    /// A store holding at most `capacity` keys, and no path yet.
    pub fn new(capacity: usize) -> Store {
        Store {
            // Sized once and never pushed to, so an entry's address never
            // moves. `value_ptr()` depends on that.
            entries: vec![Entry::EMPTY; capacity],
            count: 0,
            path: [0; 512],
            dirty: false,
        }
    }

    /// Where the settings are being kept, or `None` if never set.
    pub fn path(&self) -> Option<&str> {
        if self.path[0] == 0 {
            None
        } else {
            Some(cstr(&self.path))
        }
    }

    /// Points the store at a file. Does not read it: see [`Store::load`].
    pub fn set_path(&mut self, path: &str) -> bool {
        self.path = [0; 512];
        if path.is_empty() {
            return false;
        }
        sized_write(&mut self.path, path);
        true
    }

    /// How many keys are stored.
    #[inline]
    pub fn count(&self) -> usize {
        self.count
    }

    /// How many could be.
    #[inline]
    pub fn capacity(&self) -> usize {
        self.entries.len()
    }

    /// Whether anything has changed since the last save.
    #[inline]
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    #[inline]
    pub fn mark_clean(&mut self) {
        self.dirty = false;
    }

    /// Clears the store, the way `config_store_init` starts.
    pub fn clear(&mut self) {
        self.count = 0;
        self.dirty = false;
        self.path = [0; 512];
        for entry in &mut self.entries {
            *entry = Entry::EMPTY;
        }
    }

    /// Reads an INI file's contents into the store, returning how many keys
    /// were loaded. A missing or unreadable file is not an error: the caller
    /// simply has nothing to load and the defaults passed to the getters are
    /// used.
    ///
    /// Blanks and `#`/`;` comments are skipped, a line without an `=` is
    /// ignored rather than failing the load, and keys before the first
    /// `[section]` land in the empty section.
    pub fn load(&mut self, text: &[u8]) -> usize {
        let mut section = String::new();
        let mut rest = text;

        // The C version reads with `fgets(line, 256, f)`, so a line longer than
        // 255 bytes arrives as more than one line. Long values are truncated
        // to 127 anyway, but reproducing the split keeps a hand-edited file
        // that happens to be wide parsing exactly as it used to.
        while !rest.is_empty() {
            let take = rest.len().min(255);
            let mut len = take;
            if let Some(nl) = rest[..take].iter().position(|&b| b == b'\n') {
                len = nl + 1;
            }
            let line = &rest[..len];
            rest = &rest[len..];

            let line = trim(line);
            if line.is_empty() || line[0] == b'#' || line[0] == b';' {
                continue; // blank or comment
            }

            if line[0] == b'[' {
                if let Some(close) = line.iter().position(|&b| b == b']') {
                    let name = trim(&line[1..close]);
                    section = String::from_utf8_lossy(name).into_owned();
                }
                continue;
            }

            let Some(equals) = line.iter().position(|&b| b == b'=') else {
                continue; // not a key/value line; ignore rather than fail the load
            };
            let key = trim(&line[..equals]);
            let value = trim(&line[equals + 1..]);
            self.set(
                &section,
                &String::from_utf8_lossy(key),
                &String::from_utf8_lossy(value),
            );
        }

        // What was just read is by definition already on disk.
        self.dirty = false;
        self.count
    }

    fn find(&self, section: &str, key: &str) -> Option<usize> {
        (0..self.count).find(|&i| self.entries[i].matches(section, key))
    }

    /// The value of `key` in `section`, if it is there and non-empty.
    pub fn get(&self, section: &str, key: &str) -> Option<&str> {
        self.find(section, key).map(|i| self.entries[i].value())
    }

    /// A pointer to the value's bytes, for handing to C.
    ///
    /// Valid until the same key is set again, exactly like the C version: the
    /// entry array never moves and the value lives inside the entry.
    pub fn value_ptr(&self, section: &str, key: &str) -> Option<*const u8> {
        self.find(section, key)
            .map(|i| self.entries[i].value.as_ptr())
    }

    /// A pointer straight at the whole entry array, for `sonix-ffi`.
    pub fn entries_ptr(&self) -> *const Entry {
        self.entries.as_ptr()
    }

    pub fn get_int(&self, section: &str, key: &str) -> Option<i32> {
        let value = self.get(section, key)?;
        if value.is_empty() {
            return None;
        }
        // strtol: an optional sign and digits, stopping at the first other
        // character. Saturation at the target's 32-bit `long` on overflow.
        let bytes = value.as_bytes();
        let mut i = 0usize;
        let mut negative = false;
        if i < bytes.len() && (bytes[i] == b'+' || bytes[i] == b'-') {
            negative = bytes[i] == b'-';
            i += 1;
        }
        let start = i;
        let mut acc: i64 = 0;
        while i < bytes.len() && bytes[i].is_ascii_digit() {
            acc = (acc * 10 + i64::from(bytes[i] - b'0')).min(i64::from(i32::MAX) + 1);
            i += 1;
        }
        if i == start {
            return None;
        }
        let value = if negative { -acc } else { acc };
        Some(value.clamp(i64::from(i32::MIN), i64::from(i32::MAX)) as i32)
    }

    pub fn get_bool(&self, section: &str, key: &str) -> Option<bool> {
        let value = self.get(section, key)?;
        let first = *value.as_bytes().first()?;
        Some(matches!(first, b'1' | b't' | b'T' | b'y' | b'Y'))
    }

    /// Stores a value. Only memory is touched: call [`Store::render`] and write
    /// the result to put it on disk.
    ///
    /// Returns false when the table is full and the value was dropped, which is
    /// the one failure worth surfacing.
    pub fn set(&mut self, section: &str, key: &str, value: &str) -> bool {
        if let Some(i) = self.find(section, key) {
            if self.entries[i].value() == value {
                return true; // unchanged: not worth a write
            }
            sized_write(&mut self.entries[i].value, value);
            self.dirty = true;
            return true;
        }

        if self.count >= self.entries.len() {
            return false;
        }

        let entry = &mut self.entries[self.count];
        entry.section = [0; SECTION_LEN];
        entry.key = [0; KEY_LEN];
        entry.value = [0; VALUE_LEN];
        sized_write(&mut entry.section, section);
        sized_write(&mut entry.key, key);
        sized_write(&mut entry.value, value);
        self.count += 1;
        self.dirty = true;
        true
    }

    pub fn set_int(&mut self, section: &str, key: &str, value: i32) -> bool {
        let mut text = String::new();
        let _ = core::fmt::Write::write_fmt(&mut text, format_args!("{value}"));
        self.set(section, key, &text)
    }

    pub fn set_bool(&mut self, section: &str, key: &str, value: bool) -> bool {
        self.set(section, key, if value { "1" } else { "0" })
    }

    /// The file's contents, or an empty vector when nothing is stored.
    ///
    /// Grouped by section, in the order the sections first appear. The C
    /// version writes beside the real file and renames over it, so a power cut
    /// leaves either the old settings or the new ones and never half a file
    /// which the next boot would read as no settings at all; the caller keeps
    /// that half of the contract.
    pub fn render(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(64 + self.count * 48);
        out.extend_from_slice(
            b"# sonix_player settings. Rewritten by the player; hand edits are read back.\n",
        );

        for i in 0..self.count {
            let section = self.entries[i].section();
            if (0..i).any(|j| self.entries[j].section() == section) {
                continue;
            }

            out.push(b'\n');
            out.push(b'[');
            out.extend_from_slice(section.as_bytes());
            out.extend_from_slice(b"]\n");

            for j in i..self.count {
                let entry = &self.entries[j];
                if entry.section() != section {
                    continue;
                }
                out.extend_from_slice(entry.key().as_bytes());
                out.extend_from_slice(b" = ");
                out.extend_from_slice(entry.value().as_bytes());
                out.push(b'\n');
            }
        }

        out
    }

    /// Moves every key of `section` from this store to `to` and returns how
    /// many moved.
    ///
    /// One use: the reader's settings that a config written by an older build
    /// still holds in `device_config.ini`, which would otherwise be lost now
    /// that the reader has a file of its own.
    pub fn move_section(&mut self, to: &mut Store, section: &str) -> usize {
        let mut moved = 0usize;
        let mut kept = 0usize;

        for i in 0..self.count {
            if self.entries[i].section() == section {
                // The C version counts a move even when the destination is
                // full; the key is then dropped rather than staying put.
                to.set(section, self.entries[i].key(), self.entries[i].value());
                moved += 1;
                continue;
            }
            if kept != i {
                self.entries[kept] = self.entries[i];
            }
            kept += 1;
        }

        if moved > 0 {
            self.count = kept;
            self.dirty = true;
        }
        moved
    }
}

/// Trims spaces, tabs and CR/LF from both ends.
fn trim(s: &[u8]) -> &[u8] {
    let is_space = |b: u8| matches!(b, b' ' | b'\t' | b'\r' | b'\n');
    let mut start = 0usize;
    while start < s.len() && is_space(s[start]) {
        start += 1;
    }
    let mut end = s.len();
    while end > start && is_space(s[end - 1]) {
        end -= 1;
    }
    &s[start..end]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_sections_and_keys() {
        let text = b"\n# a comment\n; another\n\n[ui]\ntheme = dark\n\n[clock]\nconfigured = 1\nlast_seen = 1786000000\n";
        let mut store = Store::new(MAX_ENTRIES);
        store.load(text);

        assert_eq!(store.get("ui", "theme"), Some("dark"));
        assert_eq!(store.get("clock", "last_seen"), Some("1786000000"));
        assert_eq!(store.get_int("clock", "configured"), Some(1));
        assert_eq!(store.get_bool("clock", "configured"), Some(true));
        assert_eq!(store.get("ui", "missing"), None);
        assert_eq!(
            store.get_int("ui", "theme"),
            None,
            "\"dark\" is not a number"
        );
        assert_eq!(store.get_int("ui", "missing"), None);
        assert_eq!(store.get_bool("ui", "missing"), None);
    }

    #[test]
    fn load_leaves_the_store_clean() {
        let mut store = Store::new(MAX_ENTRIES);
        store.load(b"[a]\nb = 1\n");
        assert!(!store.is_dirty(), "what was read is already on disk");
    }

    #[test]
    fn setting_an_unchanged_value_is_not_a_write() {
        let mut store = Store::new(MAX_ENTRIES);
        store.load(b"[a]\nb = 1\n");
        assert!(!store.is_dirty());
        store.set("a", "b", "1");
        assert!(!store.is_dirty(), "same value: nothing to save");
        store.set("a", "b", "2");
        assert!(store.is_dirty());
        assert_eq!(store.get("a", "b"), Some("2"));
        assert_eq!(store.count(), 1, "an update must not add a row");
    }

    #[test]
    fn render_round_trips_through_load() {
        let mut store = Store::new(MAX_ENTRIES);
        store.set("ui", "theme", "dark");
        store.set("eq", "band0", "-3");
        store.set("ui", "layout", "poster");
        store.set("clock", "configured", "1");

        let text = store.render();

        // Grouped by section in order of first appearance, with a blank line
        // before each header and the comment at the top.
        let rendered = String::from_utf8(text.clone()).unwrap();
        assert!(rendered.starts_with("# sonix_player settings."));
        assert_eq!(
            rendered,
            "# sonix_player settings. Rewritten by the player; hand edits are read back.\n\
             \n[ui]\ntheme = dark\nlayout = poster\n\
             \n[eq]\nband0 = -3\n\
             \n[clock]\nconfigured = 1\n"
        );

        let mut again = Store::new(MAX_ENTRIES);
        again.load(&text);
        assert_eq!(again.count(), 4);
        assert_eq!(again.get("ui", "layout"), Some("poster"));
        assert_eq!(again.get("eq", "band0"), Some("-3"));
        assert_eq!(again.render(), text);
    }

    #[test]
    fn values_are_truncated_like_snprintf() {
        let mut store = Store::new(MAX_ENTRIES);
        let long = "v".repeat(200);
        store.set("a", "b", &long);
        assert_eq!(store.get("a", "b").unwrap().len(), VALUE_LEN - 1);

        store.set(&"s".repeat(40), &"k".repeat(40), "x");
        assert_eq!(store.count(), 2);
        // 31 characters survive in each field.
        let rendered = String::from_utf8(store.render()).unwrap();
        assert!(rendered.contains(&format!("[{}]\n", "s".repeat(31))));
    }

    #[test]
    fn a_full_table_drops_further_keys() {
        let mut store = Store::new(2);
        assert!(store.set("a", "1", "x"));
        assert!(store.set("a", "2", "x"));
        assert!(!store.set("a", "3", "x"), "the third must be refused");
        assert_eq!(store.count(), 2);
        assert_eq!(store.get("a", "3"), None);
        // Updating an existing key still works when the table is full.
        assert!(store.set("a", "1", "y"));
        assert_eq!(store.get("a", "1"), Some("y"));
    }

    #[test]
    fn ints_saturate_and_reject_garbage() {
        let mut store = Store::new(16);
        store.set("a", "big", "99999999999");
        store.set("a", "small", "-99999999999");
        store.set("a", "junk", "12abc");
        store.set("a", "plus", "+7");
        store.set("a", "empty", "");

        assert_eq!(store.get_int("a", "big"), Some(i32::MAX));
        assert_eq!(store.get_int("a", "small"), Some(i32::MIN));
        assert_eq!(store.get_int("a", "junk"), Some(12), "strtol stops at 'a'");
        assert_eq!(store.get_int("a", "plus"), Some(7));
        assert_eq!(store.get_int("a", "empty"), None);
    }

    #[test]
    fn bools_accept_the_five_first_characters() {
        let mut store = Store::new(16);
        for (key, value) in [
            ("one", "1"),
            ("true", "true"),
            ("yes", "y"),
            ("zero", "0"),
            ("no", "n"),
            ("off", "false"),
        ] {
            store.set("a", key, value);
        }
        assert_eq!(store.get_bool("a", "one"), Some(true));
        assert_eq!(store.get_bool("a", "true"), Some(true));
        assert_eq!(store.get_bool("a", "yes"), Some(true));
        assert_eq!(store.get_bool("a", "zero"), Some(false));
        assert_eq!(store.get_bool("a", "no"), Some(false));
        assert_eq!(store.get_bool("a", "off"), Some(false));
    }

    #[test]
    fn move_section_takes_everything_and_leaves_nothing() {
        let mut main = Store::new(MAX_ENTRIES);
        let mut ebook = Store::new(EBOOK_MAX_ENTRIES);

        main.set("player", "volume", "40");
        main.set("ebook", "book1", "12");
        main.set("ebook", "book2", "34");
        main.set("ui", "theme", "dark");

        let moved = main.move_section(&mut ebook, "ebook");

        assert_eq!(moved, 2);
        assert_eq!(ebook.get("ebook", "book1"), Some("12"));
        assert_eq!(ebook.get("ebook", "book2"), Some("34"));
        assert_eq!(main.get("ebook", "book1"), None);
        assert_eq!(main.count(), 2, "the other sections stay");
        assert_eq!(main.get("player", "volume"), Some("40"));
        assert_eq!(main.get("ui", "theme"), Some("dark"));
        assert!(main.is_dirty(), "a move changes the file");
    }

    #[test]
    fn move_section_that_matches_nothing_changes_nothing() {
        let mut main = Store::new(MAX_ENTRIES);
        let mut ebook = Store::new(EBOOK_MAX_ENTRIES);
        main.set("ui", "theme", "dark");
        main.mark_clean();

        assert_eq!(main.move_section(&mut ebook, "ebook"), 0);
        assert_eq!(main.count(), 1);
        assert!(!main.is_dirty());
    }

    #[test]
    fn pointers_are_stable_and_null_terminated() {
        let mut store = Store::new(MAX_ENTRIES);
        store.set("a", "b", "value");

        let ptr = store.value_ptr("a", "b").unwrap();
        // Reading it back the way C would.
        let mut len = 0usize;
        unsafe {
            while *ptr.add(len) != 0 {
                len += 1;
            }
        }
        assert_eq!(len, 5);

        // Setting another key must not move it: that is the C contract.
        store.set("a", "c", "other");
        assert_eq!(store.value_ptr("a", "b"), Some(ptr));

        store.set("a", "b", "changed");
        assert_eq!(
            store.value_ptr("a", "b"),
            Some(ptr),
            "updating in place keeps the address too"
        );
    }

    #[test]
    fn keys_before_any_section_land_in_the_empty_section() {
        let mut store = Store::new(MAX_ENTRIES);
        store.load(b"orphan = 1\n[named]\nkey = 2\n");
        assert_eq!(store.get("", "orphan"), Some("1"));
        assert_eq!(store.get("named", "key"), Some("2"));
    }

    #[test]
    fn lines_without_an_equals_are_ignored() {
        let mut store = Store::new(MAX_ENTRIES);
        store.load(b"[a]\ngarbage\nb = 1\n[unclosed\nc = 2\n");
        assert_eq!(store.get("a", "b"), Some("1"));
        assert_eq!(store.get("a", "c"), Some("2"), "the bad header is ignored");
    }
}
