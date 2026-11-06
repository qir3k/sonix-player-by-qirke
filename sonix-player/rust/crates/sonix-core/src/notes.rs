//! The notes on the card: one folder of text files, read, written and listed,
//! ported from `src/system/notes/notesstore.c`.
//!
//! The folder rules and the arithmetic of "the next free name" live here so
//! they can be tested without a screen; the page above this only draws what it
//! gets back.

use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use crate::text;

/// Where the notes live: `<card>/Notes`. A folder of the user's own, beside
/// Audiobooks, Ebook, Games and PEQ, rather than anything the player creates.
pub const FOLDER: &str = "Notes";

/// The longest note, in bytes, that the editor will read back.
pub const MAX_BYTES: usize = 64 * 1024;

/// What the editor will hold, in characters.
pub const MAX_CHARS: usize = MAX_BYTES / 4;

/// Room for a name on a 240 px list.
pub const NAME_MAX: usize = 96;

pub const MAX_ENTRIES: usize = 200;

/// The extensions that are notes, which is the set the file manager already
/// opens as text plus Markdown.
const EXTENSIONS: [&str; 5] = ["txt", "md", "lrc", "nfo", "ini"];

fn ends_with_name(name: &str, ext: &str) -> bool {
    let len = name.len();
    let ext_len = ext.len();
    if len == ext_len {
        return false; // a file called ".txt" is hidden, not a note
    }
    len > ext_len + 1
        && name.as_bytes()[len - ext_len - 1] == b'.'
        && name[len - ext_len..].eq_ignore_ascii_case(ext)
}

fn name_ends_with_known(name: &str) -> bool {
    EXTENSIONS.iter().any(|ext| ends_with_name(name, ext))
}

/// Whether a name is a note: a text file, not hidden.
///
/// Everything else in the folder — a cover picture, a leftover `.tmp` from a
/// write that was interrupted — is left alone rather than offered to the editor
/// to overwrite.
pub fn is_note_name(name: &str) -> bool {
    if name.is_empty() || name.starts_with('.') {
        return false;
    }
    if name.len() > NAME_MAX {
        return false; // more than the row or the store can hold
    }
    name_ends_with_known(name)
}

/// `<card_root>/Notes`.
pub fn dir(card_root: &str) -> Option<String> {
    if card_root.is_empty() {
        return None;
    }
    Some(format!("{card_root}/{FOLDER}"))
}

/// `<dir>/<name>`.
pub fn path(dir: &str, name: &str) -> Option<String> {
    if dir.is_empty() || name.is_empty() {
        return None;
    }
    Some(format!("{dir}/{name}"))
}

/// A-Z, case-insensitively, with a run of digits compared as the number it is:
/// a folder of dated notes reads in the order they were written rather than
/// note-1, note-10, note-2.
pub fn compare_names(a: &str, b: &str) -> std::cmp::Ordering {
    use std::cmp::Ordering;

    let a = a.as_bytes();
    let b = b.as_bytes();
    let mut i = 0usize;
    let mut j = 0usize;

    while i < a.len() && j < b.len() {
        if a[i].is_ascii_digit() && b[j].is_ascii_digit() {
            let (num_a, next_a) = number_at(a, i);
            let (num_b, next_b) = number_at(b, j);
            if num_a != num_b {
                return num_a.cmp(&num_b);
            }
            // Equal numbers written differently ("07" against "7") keep going:
            // the rest of the name decides, as it does for a letter.
            i = next_a;
            j = next_b;
            continue;
        }
        let la = a[i].to_ascii_lowercase();
        let lb = b[j].to_ascii_lowercase();
        if la != lb {
            return la.cmp(&lb);
        }
        i += 1;
        j += 1;
    }

    match (i < a.len(), j < b.len()) {
        (true, _) => Ordering::Greater,
        (_, true) => Ordering::Less,
        _ => Ordering::Equal,
    }
}

/// The number a run of digits names, saturating the way `strtoull` does.
fn number_at(s: &[u8], from: usize) -> (u64, usize) {
    let mut value: u64 = 0;
    let mut at = from;
    while at < s.len() && s[at].is_ascii_digit() {
        value = value
            .saturating_mul(10)
            .saturating_add(u64::from(s[at] - b'0'));
        at += 1;
    }
    (value, at)
}

/// The folder's notes, by name, A-Z and case-insensitively, with a run of
/// digits read as a number: note-2 comes before note-10. At most `max` entries
/// are returned; an empty list means the folder is not there.
pub fn list(dir: &Path, max: usize) -> Vec<String> {
    if max == 0 {
        return Vec::new();
    }
    let Ok(entries) = std::fs::read_dir(dir) else {
        return Vec::new();
    };

    let mut names: Vec<String> = Vec::new();
    for entry in entries.flatten() {
        let name = entry.file_name();
        let Some(name) = name.to_str() else { continue };
        if !is_note_name(name) {
            continue;
        }

        // A directory called "list.txt" is not a note, and offering it to the
        // editor would be offering to write over it.
        let Some(full) = path(&dir.to_string_lossy(), name) else {
            continue;
        };
        if !Path::new(&full).is_file() {
            continue;
        }

        names.push(name.to_owned());
        if names.len() == max {
            break;
        }
    }

    names.sort_by(|a, b| compare_names(a, b));
    names
}

/// The note's text, decoded to UTF-8 (see [`crate::text`]), and whether the
/// file was longer than the ceiling. `None` when the file cannot be read at
/// all — which the flag distinguishes from "it was long".
pub fn read(path: &Path) -> Option<(String, bool)> {
    let (raw, truncated) = read_raw(path)?;
    Some((
        String::from_utf8_lossy(&text::decode(&raw)).into_owned(),
        truncated,
    ))
}

/// The raw bytes of a note, up to [`MAX_BYTES`], and whether there were more.
pub fn read_raw(path: &Path) -> Option<(Vec<u8>, bool)> {
    use std::io::Read as _;

    let mut file = std::fs::File::open(path).ok()?;
    let mut raw = vec![0u8; MAX_BYTES.max(1)];
    let mut got = 0usize;
    while got < MAX_BYTES {
        match file.read(&mut raw[got..MAX_BYTES]) {
            Ok(0) => break,
            Ok(n) => got += n,
            Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(_) => return None,
        }
    }
    let mut probe = [0u8; 1];
    let more = matches!(file.read(&mut probe), Ok(1));
    raw.truncate(got);
    Some((raw, more))
}

fn make_dir(path: &Path) -> bool {
    match std::fs::create_dir(path) {
        Ok(()) => true,
        Err(_) => path.is_dir(),
    }
}

/// The file a name is written as.
///
/// "list" becomes "list.txt", "list.md" stays the Markdown file it says it is,
/// and a name that carries a dot but no extension the page knows — a date typed
/// as "2026.10.16", a name copied from somewhere else — keeps it and gets
/// ".txt" after it, rather than being refused for a reason the user cannot see.
pub fn file_name(name: &str) -> String {
    if name_ends_with_known(name) {
        name.to_owned()
    } else {
        format!("{name}.txt")
    }
}

/// `text` written into `dir` as `name`, as UTF-8.
///
/// The folder is made if it is not there, which is what lets the first note be
/// written on a card that has never had one.
pub fn write(dir: &Path, name: &str, text: &str) -> bool {
    if !name_ok(name) {
        return false;
    }
    if !make_dir(dir) {
        return false;
    }

    let file = file_name(name);
    if file.len() + 1 > NAME_MAX + 8 || !is_note_name(&file) {
        return false;
    }

    // Through the same writer the C used: a temporary file and a rename, with a
    // final newline added if the text lacks one.
    let full = dir.join(&file);
    crate::text::write_text_file(&full, text.as_bytes())
}

/// A name as a list or a title should show it: the note's extension taken off,
/// since every row of the page is a text file and the `.txt` is noise on a
/// 240 px line. The name on the card is untouched.
pub fn display_name(name: &str) -> String {
    let mut buf = vec![0u8; name.len() + 1];
    let n = display_name_into(name, &mut buf);
    String::from_utf8_lossy(&buf[..n]).into_owned()
}

/// [`display_name`] into a caller's buffer, in the order the C function did it:
/// the whole name is written (cut to whatever fits), and only then is the last
/// dot still in the buffer turned into a terminator.
///
/// The order matters when the name does not fit: `"a.b.txt"` in three bytes is
/// written as `"a."` and then cut to `"a"`, where cutting the full name first
/// would have produced `"a."`. Bytes past the terminator are left alone, as
/// `snprintf` leaves them. Returns the length of the string now in `buf`.
pub fn display_name_into(name: &str, buf: &mut [u8]) -> usize {
    if buf.is_empty() {
        return 0;
    }
    let n = name.len().min(buf.len() - 1);
    buf[..n].copy_from_slice(&name.as_bytes()[..n]);
    buf[n] = 0;

    if !name_ends_with_known(name) {
        return n;
    }
    if let Some(dot) = buf[..n].iter().rposition(|&b| b == b'.') {
        if dot != 0 {
            buf[dot] = 0;
            return dot;
        }
    }
    n
}

/// Whether a name can be written at all: something other than blank, with no
/// path separator and no control character in it.
pub fn name_ok(name: &str) -> bool {
    // A leading dot is refused on purpose: the file is written as "<name>.txt",
    // and a hidden file is one the notes list would never show back to the
    // person who wrote it.
    if name.is_empty() || name.starts_with('.') {
        return false;
    }
    for byte in name.bytes() {
        if byte == b'/' || byte == b'\\' || byte < 0x20 {
            return false;
        }
    }
    // Blank, or nothing but spaces: a file whose name a list cannot show.
    if !name.bytes().any(|b| !b.is_ascii_whitespace()) {
        return false;
    }
    name.len() <= NAME_MAX
}

/// A name for a new note that is free: the date, or the date and a number.
///
/// What the new-note dialog opens with, so the fastest way to write something
/// down does not begin with typing a name.
pub fn default_name(dir: Option<&Path>) -> String {
    let stamp = today();

    // The date, then the date and a number: a second note written the same day
    // does not land on top of the first.
    for extra in 0..100 {
        let candidate = match (stamp.as_str(), extra) {
            ("", 0) => "note".to_owned(),
            ("", n) => format!("note-{}", n + 1),
            (date, 0) => date.to_owned(),
            (date, n) => format!("{date}-{}", n + 1),
        };

        // Asked about the file the name would be written as, not the bare name
        // the dialog shows: a note is stored as "<name>.txt", so "2026-10-16"
        // is taken exactly when 2026-10-16.txt is on the card.
        let free = match dir {
            Some(dir) => !dir.join(file_name(&candidate)).exists(),
            None => true,
        };
        if free {
            return candidate;
        }
    }

    // A hundred notes under one name: the caller's dialog still gets something
    // it can write.
    if stamp.is_empty() {
        "note".to_owned()
    } else {
        stamp
    }
}

/// Today's date, `YYYY-MM-DD`, in the device's timezone.
///
/// Empty when the clock cannot be read, which the caller turns into "note".
pub fn today() -> String {
    let seconds = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs() as i64)
        .unwrap_or(0);
    // The date is local: a note written at 00:30 belongs to that day where the
    // person is, not to the previous one in UTC. `localtime_r` + `%Y-%m-%d`, in
    // libc because only libc knows the timezone.
    crate::c_time::local_date(seconds as crate::c_time::TimeT).unwrap_or_default()
}

/// Characters, not bytes: one UTF-8 sequence is one character however many
/// bytes it takes.
pub fn chars(text: &str) -> usize {
    text.bytes().filter(|&b| (b & 0xC0) != 0x80).count()
}

/// Whether the editor can hold `text`.
pub fn fits_editor(text: &str) -> bool {
    chars(text) <= MAX_CHARS
}

/// The folder as a path.
pub fn folder_path(card_root: &str) -> PathBuf {
    PathBuf::from(dir(card_root).unwrap_or_default())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_text_files_are_notes() {
        assert!(is_note_name("shopping.txt"));
        assert!(is_note_name("shopping.TXT"));
        assert!(is_note_name("a.md"));
        assert!(is_note_name("a.lrc"));
        assert!(is_note_name("a.nfo"));
        assert!(is_note_name("a.ini"));

        assert!(!is_note_name("cover.jpg"));
        assert!(!is_note_name("x.txt.tmp"));
        assert!(!is_note_name(".txt"), "a file called .txt is hidden");
        assert!(!is_note_name(".hidden.txt"));
        assert!(!is_note_name("txt"), "the extension needs its dot");
        assert!(!is_note_name(""));
        assert!(!is_note_name("a.txtx"));
    }

    #[test]
    fn a_name_longer_than_a_row_is_not_a_note() {
        let long = format!("{}.txt", "a".repeat(NAME_MAX));
        assert!(!is_note_name(&long));
        let fits = format!("{}.txt", "a".repeat(NAME_MAX - 4));
        assert!(is_note_name(&fits));
    }

    #[test]
    fn paths_are_joined_with_one_separator() {
        assert_eq!(dir("/mnt/sd").as_deref(), Some("/mnt/sd/Notes"));
        assert_eq!(dir("").as_deref(), None);
        assert_eq!(
            path("/mnt/sd/Notes", "a.txt").as_deref(),
            Some("/mnt/sd/Notes/a.txt")
        );
        assert_eq!(path("", "a.txt"), None);
        assert_eq!(path("/notes", ""), None);
    }

    #[test]
    fn names_sort_naturally_and_case_insensitively() {
        use std::cmp::Ordering;
        assert_eq!(compare_names("note-2.txt", "note-10.txt"), Ordering::Less);
        assert_eq!(
            compare_names("note-10.txt", "note-2.txt"),
            Ordering::Greater
        );
        assert_eq!(compare_names("Apple.txt", "banana.txt"), Ordering::Less);
        assert_eq!(compare_names("apple.txt", "Apple.txt"), Ordering::Equal);
        assert_eq!(compare_names("note-07.txt", "note-7.txt"), Ordering::Equal);
        // "note-07a" against "note-7b": equal numbers, so the rest decides.
        assert_eq!(compare_names("note-07a.txt", "note-7b.txt"), Ordering::Less);
        assert_eq!(compare_names("short", "shorter"), Ordering::Less);
        assert_eq!(compare_names("shorter", "short"), Ordering::Greater);
        assert_eq!(
            compare_names("2026-10-09.txt", "2026-10-16.txt"),
            Ordering::Less
        );
    }

    #[test]
    fn the_file_name_keeps_a_known_extension() {
        assert_eq!(file_name("list"), "list.txt");
        assert_eq!(file_name("list.md"), "list.md");
        assert_eq!(file_name("list.MD"), "list.MD");
        assert_eq!(file_name("2026.10.16"), "2026.10.16.txt");
        assert_eq!(file_name("a.b.jpg"), "a.b.jpg.txt");
    }

    #[test]
    fn display_name_drops_only_a_known_extension() {
        assert_eq!(display_name("shopping.txt"), "shopping");
        assert_eq!(display_name("a.b.txt"), "a.b");
        assert_eq!(display_name("photo.jpg"), "photo.jpg");
        assert_eq!(display_name("noext"), "noext");
        assert_eq!(display_name(""), "");
    }

    #[test]
    fn display_name_is_written_then_cut() {
        let mut buf = [0xA5u8; 8];
        let n = display_name_into("a.b.txt", &mut buf[..4]);
        assert_eq!(
            &buf[..n],
            b"a",
            "cut at the dot that survived the truncation"
        );
        assert_eq!(buf[4], 0xA5, "nothing past the terminator is touched");

        let n = display_name_into("a.b.txt", &mut buf);
        assert_eq!(&buf[..n], b"a.b");
    }

    #[test]
    fn a_name_that_cannot_be_written_is_refused() {
        assert!(name_ok("shopping"));
        assert!(name_ok("a b c"));
        assert!(!name_ok(""));
        assert!(!name_ok("   "), "nothing but spaces");
        assert!(!name_ok(".hidden"));
        assert!(!name_ok("a/b"));
        assert!(!name_ok("a\\b"));
        assert!(!name_ok("a\tb"), "a control character");
        assert!(!name_ok(&"a".repeat(NAME_MAX + 1)));
    }

    #[test]
    fn the_editor_counts_characters_not_bytes() {
        assert_eq!(chars("abc"), 3);
        assert_eq!(chars("héllo"), 5);
        assert_eq!(chars("世界"), 2);
        assert_eq!(chars("🎵"), 1);
        assert!(fits_editor(""));
        assert!(!fits_editor(&"a".repeat(MAX_CHARS + 1)));
        assert!(fits_editor(&"a".repeat(MAX_CHARS)));
    }

    #[test]
    fn a_note_round_trips_through_the_folder() {
        let dir = std::env::temp_dir().join(format!("sonix-notes-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);

        assert!(write(&dir, "first", "hello"));
        assert!(write(&dir, "second", "world\n"));

        let mut names = list(&dir, MAX_ENTRIES);
        names.sort();
        assert_eq!(names, vec!["first.txt", "second.txt"]);

        let (text, truncated) = read(&dir.join("first.txt")).unwrap();
        assert_eq!(text, "hello\n", "the writer leaves a final newline");
        assert!(!truncated);

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_default_name_is_free_before_it_is_returned() {
        let dir = std::env::temp_dir().join(format!("sonix-notes-default-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();

        let first = default_name(Some(&dir));
        assert!(name_ok(&first));
        // Whatever the date is, creating that note moves the next name on.
        write(&dir, &first, "x");
        let second = default_name(Some(&dir));
        assert_ne!(first, second);
        assert!(name_ok(&second));

        let _ = std::fs::remove_dir_all(&dir);
    }
}
