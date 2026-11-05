//! A small JSON parser for streaming-service responses, ported from
//! `src/system/core/json.c`.
//!
//! radio.c gets by without one: it scans for keys by hand in the raw text, and
//! for radio-browser.info — an array of flat objects, all strings and numbers —
//! that works in twenty lines.
//!
//! Qobuz and Tidal nest four or five levels deep
//!
//! ```text
//! {"tracks":{"items":[{"album":{"image":{"small":"..."}},"performer":{...}}]}}
//! ```
//!
//! where searching the raw text for `"small"` finds the first occurrence in
//! whatever object it happens to sit in. A real parser is the only way to read
//! the right field.
//!
//! One pass fills an array of tokens in document order, each with its span in
//! the text, its child count and its parent. No string is copied until it is
//! asked for — a Qobuz response is a hundred kilobytes or so, and copying it
//! twice on a device with 64 MB is not free. The document borrows the text; it
//! must stay alive and unmodified for as long as the document is read.
//!
//! ## Why this is not `serde_json`
//!
//! The token layout is the C one (`Tok` is `#[repr(C)]` and
//! `size_of::<Tok>() == 20`), because `sonix-ffi` hands these documents back to
//! C callers by pointer and the firmware's own structs must keep matching. A
//! generic JSON tree would also cost an allocation per node on a device where
//! the whole response is one buffer.

/// How deep nesting may go before the document is rejected. Without a limit a
/// document of nothing but opening brackets takes down the stack. Qobuz and
/// Tidal never pass the fifth level.
pub const MAX_DEPTH: usize = 64;

/// Token kinds, numbered as `json_type_t` is in C.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[repr(i32)]
pub enum Type {
    Undefined = 0,
    Object = 1,
    Array = 2,
    String = 3,
    Number = 4,
    Bool = 5,
    Null = 6,
}

impl Type {
    /// The value a C caller would see in `json_tok_t::type`.
    #[inline]
    pub fn as_i32(self) -> i32 {
        self as i32
    }

    #[inline]
    pub fn from_i32(v: i32) -> Type {
        match v {
            1 => Type::Object,
            2 => Type::Array,
            3 => Type::String,
            4 => Type::Number,
            5 => Type::Bool,
            6 => Type::Null,
            _ => Type::Undefined,
        }
    }
}

/// One token. Field order and types are the C `json_tok_t`: `start`/`end` are
/// byte offsets into the document text, `size` is the pair count of an object
/// or the element count of an array, and `parent` is the enclosing token
/// (`-1` for the root).
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct Tok {
    pub ty: i32,
    pub start: i32,
    pub end: i32,
    pub size: i32,
    pub parent: i32,
}

impl Tok {
    #[inline]
    pub fn kind(&self) -> Type {
        Type::from_i32(self.ty)
    }
}

#[inline]
fn is_number_char(c: u8) -> bool {
    c.is_ascii_digit() || c == b'-' || c == b'+' || c == b'.' || c == b'e' || c == b'E'
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

struct Parser<'a> {
    text: &'a [u8],
    pos: usize,
    toks: Vec<Tok>,
    depth: usize,
    max_tokens: usize,
}

impl<'a> Parser<'a> {
    /// The byte at `i`, or 0 past the end.
    ///
    /// The C parser is written against a NUL-terminated string and reads
    /// `text[pos]` freely, relying on the terminator to end every loop. Going
    /// through one accessor keeps that behaviour exactly, including a NUL
    /// *inside* the buffer, which the C build would have read as the end of
    /// the document.
    #[inline]
    fn at(&self, i: usize) -> u8 {
        self.text.get(i).copied().unwrap_or(0)
    }

    #[inline]
    fn cur(&self) -> u8 {
        self.at(self.pos)
    }

    fn starts_with(&self, prefix: &[u8]) -> bool {
        prefix
            .iter()
            .enumerate()
            .all(|(i, &b)| self.at(self.pos + i) == b)
    }

    fn tok_new(&mut self, ty: Type, start: usize, parent: i32) -> Option<i32> {
        if self.toks.len() >= self.max_tokens {
            return None;
        }
        let index = self.toks.len() as i32;
        self.toks.push(Tok {
            ty: ty as i32,
            start: start as i32,
            end: start as i32,
            size: 0,
            parent,
        });
        Some(index)
    }

    fn skip_space(&mut self) {
        loop {
            match self.cur() {
                b' ' | b'\t' | b'\n' | b'\r' => self.pos += 1,
                _ => break,
            }
        }
    }

    /// Skips a string body, leaving `pos` on the closing quote. Escapes are not
    /// decoded, only stepped over, since an escaped quote is not the end.
    fn skip_string(&mut self) -> bool {
        while self.cur() != 0 {
            match self.cur() {
                b'\\' => {
                    if self.at(self.pos + 1) == 0 {
                        return false;
                    }
                    self.pos += 2;
                }
                b'"' => return true,
                _ => self.pos += 1,
            }
        }
        false
    }

    fn parse_object(&mut self, me: i32) -> bool {
        self.pos += 1; // opening brace
        loop {
            self.skip_space();
            if self.cur() == b'}' {
                self.pos += 1;
                self.toks[me as usize].end = self.pos as i32;
                return true;
            }
            if self.toks[me as usize].size > 0 {
                if self.cur() != b',' {
                    return false;
                }
                self.pos += 1;
                self.skip_space();
                // A trailing comma before the closing brace is malformed, not
                // an empty object: rejecting it here avoids counting a pair
                // that is not there.
                if self.cur() == b'}' {
                    return false;
                }
            }

            if self.cur() != b'"' {
                return false; // a key is always a string
            }
            self.pos += 1;
            let key_start = self.pos;
            if !self.skip_string() {
                return false;
            }
            let Some(key) = self.tok_new(Type::String, key_start, me) else {
                return false;
            };
            self.toks[key as usize].end = self.pos as i32;
            self.pos += 1; // closing quote

            self.skip_space();
            if self.cur() != b':' {
                return false;
            }
            self.pos += 1;

            // The value's parent is the key, not the object: that is how
            // `get` tells keys from values while walking the object's
            // children.
            if self.parse_value(key).is_none() {
                return false;
            }
            self.toks[me as usize].size += 1;
        }
    }

    fn parse_array(&mut self, me: i32) -> bool {
        self.pos += 1; // opening bracket
        loop {
            self.skip_space();
            if self.cur() == b']' {
                self.pos += 1;
                self.toks[me as usize].end = self.pos as i32;
                return true;
            }
            if self.toks[me as usize].size > 0 {
                if self.cur() != b',' {
                    return false;
                }
                self.pos += 1;
                self.skip_space();
                if self.cur() == b']' {
                    return false;
                }
            }
            if self.parse_value(me).is_none() {
                return false;
            }
            self.toks[me as usize].size += 1;
        }
    }

    fn parse_value(&mut self, parent: i32) -> Option<i32> {
        if self.depth >= MAX_DEPTH {
            return None;
        }

        self.skip_space();
        let c = self.cur();

        if c == b'{' || c == b'[' {
            let ty = if c == b'{' { Type::Object } else { Type::Array };
            let me = self.tok_new(ty, self.pos, parent)?;
            self.depth += 1;
            let ok = if c == b'{' {
                self.parse_object(me)
            } else {
                self.parse_array(me)
            };
            self.depth -= 1;
            return if ok { Some(me) } else { None };
        }

        if c == b'"' {
            self.pos += 1;
            let start = self.pos;
            if !self.skip_string() {
                return None;
            }
            let me = self.tok_new(Type::String, start, parent)?;
            self.toks[me as usize].end = self.pos as i32;
            self.pos += 1;
            return Some(me);
        }

        if self.starts_with(b"true") || self.starts_with(b"false") {
            let len = if self.cur() == b't' { 4 } else { 5 };
            let me = self.tok_new(Type::Bool, self.pos, parent)?;
            self.pos += len;
            self.toks[me as usize].end = self.pos as i32;
            return Some(me);
        }

        if self.starts_with(b"null") {
            let me = self.tok_new(Type::Null, self.pos, parent)?;
            self.pos += 4;
            self.toks[me as usize].end = self.pos as i32;
            return Some(me);
        }

        if is_number_char(c) {
            let start = self.pos;
            while is_number_char(self.cur()) {
                self.pos += 1;
            }
            let me = self.tok_new(Type::Number, start, parent)?;
            self.toks[me as usize].end = self.pos as i32;
            return Some(me);
        }

        None
    }
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------

/// Where the token array lives.
///
/// A document parsed here owns its tokens. A document that came back from C
/// through `sonix-ffi` borrows the array the C caller allocated, so reading it
/// costs nothing and never copies a Qobuz response on a device with 64 MB.
#[derive(Clone, Debug)]
enum Toks<'a> {
    Owned(Vec<Tok>),
    Borrowed(&'a [Tok]),
}

/// A parsed document. Borrows the text it was parsed from.
#[derive(Clone, Debug)]
pub struct Doc<'a> {
    text: &'a [u8],
    toks: Toks<'a>,
}

impl<'a> Doc<'a> {
    /// A document over a token array somebody else owns — the C `json_doc_t`
    /// after `sonix-ffi` has built it.
    pub fn from_parts(text: &'a [u8], toks: &'a [Tok]) -> Doc<'a> {
        Doc {
            text,
            toks: Toks::Borrowed(toks),
        }
    }

    #[inline]
    fn store(&self) -> &[Tok] {
        match &self.toks {
            Toks::Owned(v) => v,
            Toks::Borrowed(s) => s,
        }
    }
    /// Parses `text`. `None` on malformed JSON, when nesting passes
    /// [`MAX_DEPTH`], or when the token cap is reached.
    pub fn parse(text: &'a [u8]) -> Option<Doc<'a>> {
        Doc::parse_limited(text, usize::MAX)
    }

    /// The same, for unpaginated endpoints on memory-limited devices: at most
    /// `max_tokens` tokens are used and a document needing more is rejected
    /// rather than grown.
    pub fn parse_limited(text: &'a [u8], max_tokens: usize) -> Option<Doc<'a>> {
        if max_tokens == 0 {
            return None;
        }

        let mut p = Parser {
            text,
            pos: 0,
            toks: Vec::new(),
            depth: 0,
            max_tokens,
        };

        if p.parse_value(-1).is_none() {
            return None;
        }

        // Only whitespace may follow the root value. A second document
        // appended behind it means the response arrived mangled, and silently
        // reading just the first part would be worse than failing.
        p.skip_space();
        if p.cur() != 0 {
            return None;
        }

        if p.toks.is_empty() {
            return None;
        }

        Some(Doc {
            text,
            toks: Toks::Owned(p.toks),
        })
    }

    /// The same from a `&str`, which is what a network buffer usually is.
    pub fn parse_str(text: &'a str) -> Option<Doc<'a>> {
        Doc::parse(text.as_bytes())
    }

    /// The token array, in document order.
    #[inline]
    pub fn tokens(&self) -> &[Tok] {
        self.store()
    }

    #[inline]
    pub fn token_count(&self) -> usize {
        self.store().len()
    }

    #[inline]
    fn valid(&self, tok: i32) -> bool {
        tok >= 0 && (tok as usize) < self.store().len()
    }

    #[inline]
    fn byte_at(&self, i: usize) -> u8 {
        self.text.get(i).copied().unwrap_or(0)
    }

    /// The root token, or -1 when the document is empty.
    #[inline]
    pub fn root(&self) -> i32 {
        if self.store().is_empty() {
            -1
        } else {
            0
        }
    }

    /// The raw text a token spans, before unescaping.
    pub fn raw(&self, tok: i32) -> &'a [u8] {
        if !self.valid(tok) {
            return &[];
        }
        let t = self.store()[tok as usize];
        let (start, end) = (t.start.max(0) as usize, t.end.max(0) as usize);
        self.text.get(start..end).unwrap_or(&[])
    }

    /// The kind of a token, or [`Type::Undefined`] for -1.
    pub fn kind(&self, tok: i32) -> Type {
        if !self.valid(tok) {
            return Type::Undefined;
        }
        self.store()[tok as usize].kind()
    }

    /// The value of `key` in object `obj`, or -1. Compares the unescaped
    /// string: these services use plain ASCII keys where raw text would do,
    /// but a key carrying a backslash must not slip through unnoticed.
    pub fn get(&self, obj: i32, key: &str) -> i32 {
        if !self.valid(obj) || self.kind(obj) != Type::Object {
            return -1;
        }

        let toks = self.store();
        let obj_end = toks[obj as usize].end;
        let mut i = obj as usize + 1;
        while i < toks.len() && toks[i].start < obj_end {
            // The object's direct children are its keys; a key's value has the
            // key as its parent.
            if toks[i].parent == obj
                && toks[i].kind() == Type::String
                && self.key_equals(i, key.as_bytes())
            {
                for v in (i + 1)..toks.len() {
                    if toks[v].parent == i as i32 {
                        return v as i32;
                    }
                }
                return -1;
            }
            i += 1;
        }
        -1
    }

    /// Compares a string token's raw text with `key`, unescaping only when an
    /// escape is present. Keys from these services never carry one, so the
    /// common case stays a byte comparison.
    fn key_equals(&self, tok: usize, key: &[u8]) -> bool {
        let span = self.raw(tok as i32);
        if !span.contains(&b'\\') {
            return span == key;
        }
        let mut buf = [0u8; 256];
        match self.string_into(tok as i32, &mut buf, false) {
            Some(n) => &buf[..n] == key,
            None => false,
        }
    }

    /// Element `index` of array `arr`, or -1.
    pub fn at(&self, arr: i32, index: i32) -> i32 {
        if !self.valid(arr) || index < 0 || self.kind(arr) != Type::Array {
            return -1;
        }

        let toks = self.store();
        let arr_end = toks[arr as usize].end;
        let mut seen = 0;
        let mut i = arr as usize + 1;
        while i < toks.len() && toks[i].start < arr_end {
            if toks[i].parent == arr {
                if seen == index {
                    return i as i32;
                }
                seen += 1;
            }
            i += 1;
        }
        -1
    }

    /// Element count of an array, or pair count of an object; 0 for anything
    /// else.
    #[inline]
    pub fn len(&self, tok: i32) -> usize {
        if !self.valid(tok) {
            return 0;
        }
        let t = &self.store()[tok as usize];
        match t.kind() {
            Type::Array | Type::Object => t.size.max(0) as usize,
            _ => 0,
        }
    }

    #[inline]
    pub fn is_empty_container(&self, tok: i32) -> bool {
        self.len(tok) == 0
    }

    /// Walks a dotted path: `path(root, "tracks.items")` is two `get` calls. A
    /// number between dots indexes an array: `"tracks.items.0.album.title"`.
    /// Returns -1 as soon as a step is missing.
    pub fn path(&self, from: i32, path: &str) -> i32 {
        if !self.valid(from) {
            return -1;
        }

        let mut at = from;
        let mut rest = path.as_bytes();

        while !rest.is_empty() && at >= 0 {
            let segment_end = rest.iter().position(|&b| b == b'.').unwrap_or(rest.len());
            // The C version copies at most 127 characters and discards the
            // remainder of a longer segment; keep that rather than failing.
            let piece = &rest[..segment_end.min(127)];
            rest = if segment_end < rest.len() {
                &rest[segment_end + 1..]
            } else {
                &rest[rest.len()..]
            };

            if piece.is_empty() {
                return -1;
            }

            // All digits on an array token means the segment is an index.
            let digits = piece.iter().all(u8::is_ascii_digit);
            at = if digits && self.kind(at) == Type::Array {
                self.at(at, parse_index(piece))
            } else {
                match core::str::from_utf8(piece) {
                    Ok(key) => self.get(at, key),
                    Err(_) => -1,
                }
            };
        }

        at
    }

    // -- Extraction ---------------------------------------------------------
    //
    // All of these are lenient: a -1 token, or one of the wrong type, yields
    // the fallback rather than an error to check on every line. Remote
    // responses have fields that are sometimes present and sometimes not, and
    // checking twenty times per screen helps nobody.

    /// Copies the string with escapes resolved (`\" \\ \/ \b \f \n \r \t` and
    /// `\uXXXX`, surrogate pairs included, converted to UTF-8), truncating to
    /// fit `out` including its NUL.
    ///
    /// Returns `Some(bytes_written)` (not counting the NUL), or `None` when the
    /// token is not a string or `out` is empty. `exact` turns truncation, an
    /// embedded NUL, a broken escape and an unpaired surrogate into failure —
    /// what an identity or credential needs.
    pub fn string_into(&self, tok: i32, out: &mut [u8], exact: bool) -> Option<usize> {
        if out.is_empty() {
            return None;
        }
        match self.string_into_inner(tok, out, exact) {
            Some(n) => Some(n),
            None => {
                // The C version's `invalid:` label clears the buffer, and a
                // caller that ignores the false can still print what it finds
                // there. A failure must never leave half a credential behind.
                out[0] = 0;
                None
            }
        }
    }

    fn string_into_inner(&self, tok: i32, out: &mut [u8], exact: bool) -> Option<usize> {
        out[0] = 0;
        if !self.valid(tok) || self.kind(tok) != Type::String {
            return None;
        }

        let src = self.raw(tok);
        let len = src.len();
        let size = out.len();
        let mut w = 0usize;
        let mut i = 0usize;

        while i < len {
            if w + 1 >= size {
                if exact {
                    return None;
                }
                break;
            }

            let c = src[i];
            if c != b'\\' {
                out[w] = c;
                w += 1;
                i += 1;
                continue;
            }

            if i + 1 >= len {
                if exact {
                    return None;
                }
                break;
            }
            i += 1;
            let esc = src[i];

            match esc {
                b'"' => {
                    out[w] = b'"';
                    w += 1;
                }
                b'\\' => {
                    out[w] = b'\\';
                    w += 1;
                }
                b'/' => {
                    out[w] = b'/';
                    w += 1;
                }
                b'b' => {
                    out[w] = 0x08;
                    w += 1;
                }
                b'f' => {
                    out[w] = 0x0C;
                    w += 1;
                }
                b'n' => {
                    out[w] = b'\n';
                    w += 1;
                }
                b'r' => {
                    out[w] = b'\r';
                    w += 1;
                }
                b't' => {
                    out[w] = b'\t';
                    w += 1;
                }
                b'u' => {
                    if i + 4 >= len {
                        if exact {
                            return None;
                        }
                        break;
                    }
                    let Some(mut cp) = hex4(&src[i + 1..i + 5]) else {
                        if exact {
                            return None;
                        }
                        break;
                    };
                    i += 4;

                    // Surrogate pair: Qobuz titles are full of characters
                    // outside the basic plane (emoji in playlist names), and
                    // decoding one half at a time produces mojibake.
                    if (0xD800..=0xDBFF).contains(&cp)
                        && i + 6 < len
                        && src[i + 1] == b'\\'
                        && src[i + 2] == b'u'
                    {
                        if let Some(low) = hex4(&src[i + 3..i + 7]) {
                            if (0xDC00..=0xDFFF).contains(&low) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                i += 6;
                            }
                        }
                    }

                    // An identity/credential must not be shortened at a NUL,
                    // cut at a multi-byte character, or accept an unpaired
                    // surrogate.
                    if exact && (cp == 0 || (0xD800..=0xDFFF).contains(&cp)) {
                        return None;
                    }

                    let bytes = utf8_put(&mut out[w..], size - 1 - w, cp);
                    if exact && bytes == 0 {
                        return None;
                    }
                    w += bytes;
                }
                _ => {
                    if exact {
                        return None;
                    }
                    out[w] = esc; // unknown escape: keep the character as it is
                    w += 1;
                }
            }

            i += 1;
        }

        out[w] = 0;
        Some(w)
    }

    /// `string_into` without the buffer: unescapes into a `String`.
    ///
    /// A UTF-16 escape is never longer than the characters it replaces, so the
    /// raw span plus one is always enough room.
    pub fn string(&self, tok: i32) -> Option<String> {
        if !self.valid(tok) || self.kind(tok) != Type::String {
            return None;
        }
        let mut out = vec![0u8; self.raw(tok).len() + 1];
        let n = self.string_into(tok, &mut out, false)?;
        Some(String::from_utf8_lossy(&out[..n]).into_owned())
    }

    /// Shorthand for `string` on a field of an object.
    pub fn obj_string(&self, obj: i32, key: &str) -> Option<String> {
        self.string(self.get(obj, key))
    }

    /// A number, or a string holding one.
    ///
    /// Qobuz sends ids sometimes as numbers and sometimes quoted, within the
    /// same response, so both are accepted. This is the 32-bit `long` of the
    /// target: a value past `i32` range saturates, exactly as `strtol` on the
    /// device did, rather than wrapping or returning the fallback.
    pub fn long(&self, tok: i32, fallback: i32) -> i32 {
        match self.integer(tok) {
            Some(v) => clamp_i32(v),
            None => fallback,
        }
    }

    /// The same at 64 bits. Use this for any id the service does not promise to
    /// keep small: Podcast Index episode ids are eleven digits, and collapsing
    /// them onto one number meant one cache file and the player playing the
    /// same episode under another episode's title.
    pub fn llong(&self, tok: i32, fallback: i64) -> i64 {
        self.integer(tok).unwrap_or(fallback)
    }

    fn integer(&self, tok: i32) -> Option<i64> {
        if !self.valid(tok) {
            return None;
        }
        let t = self.store()[tok as usize];
        if t.kind() == Type::Bool {
            return Some(if self.byte_at(t.start as usize) == b't' {
                1
            } else {
                0
            });
        }
        if t.kind() != Type::Number && t.kind() != Type::String {
            return None;
        }
        // The C version copies into a 32-byte buffer and gives up on anything
        // that does not fit, which is also a sane cap for an id or a duration.
        let raw = self.raw(tok);
        if raw.is_empty() || raw.len() >= 32 {
            return None;
        }
        parse_int_prefix(raw)
    }

    /// A number, or a string holding one, as `f64`. Like `strtod`, the longest
    /// valid prefix is used and anything unparseable yields the fallback.
    pub fn double(&self, tok: i32, fallback: f64) -> f64 {
        if !self.valid(tok) {
            return fallback;
        }
        let t = self.store()[tok as usize];
        if t.kind() != Type::Number && t.kind() != Type::String {
            return fallback;
        }
        let raw = self.raw(tok);
        if raw.is_empty() || raw.len() >= 64 {
            return fallback;
        }
        let n = float_prefix_len(raw);
        if n == 0 {
            return fallback;
        }
        match core::str::from_utf8(&raw[..n])
            .ok()
            .and_then(|s| s.parse::<f64>().ok())
        {
            Some(v) => v,
            None => fallback,
        }
    }

    /// A boolean. Qobuz sends its subscription flags as booleans in some
    /// responses and as `0`/`1` in others.
    pub fn boolean(&self, tok: i32, fallback: bool) -> bool {
        if !self.valid(tok) {
            return fallback;
        }
        let t = self.store()[tok as usize];
        match t.kind() {
            Type::Bool => self.byte_at(t.start as usize) == b't',
            Type::Number => self.long(tok, if fallback { 1 } else { 0 }) != 0,
            _ => fallback,
        }
    }

    // -- Shorthands for one field of an object ------------------------------

    pub fn obj_long(&self, obj: i32, key: &str, fallback: i32) -> i32 {
        self.long(self.get(obj, key), fallback)
    }

    pub fn obj_llong(&self, obj: i32, key: &str, fallback: i64) -> i64 {
        self.llong(self.get(obj, key), fallback)
    }

    pub fn obj_double(&self, obj: i32, key: &str, fallback: f64) -> f64 {
        self.double(self.get(obj, key), fallback)
    }

    pub fn obj_boolean(&self, obj: i32, key: &str, fallback: bool) -> bool {
        self.boolean(self.get(obj, key), fallback)
    }

    pub fn obj_len(&self, obj: i32, key: &str) -> usize {
        self.len(self.get(obj, key))
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Encodes one code point as UTF-8 into `out`, whose usable room is `room`
/// bytes. Returns the number of bytes written, or 0 when they do not fit.
fn utf8_put(out: &mut [u8], room: usize, cp: u32) -> usize {
    let need = if cp < 0x80 {
        1
    } else if cp < 0x800 {
        2
    } else if cp < 0x10000 {
        3
    } else {
        4
    };
    if room < need || out.len() < need {
        return 0;
    }
    match need {
        1 => out[0] = cp as u8,
        2 => {
            out[0] = 0xC0 | (cp >> 6) as u8;
            out[1] = 0x80 | (cp & 0x3F) as u8;
        }
        3 => {
            out[0] = 0xE0 | (cp >> 12) as u8;
            out[1] = 0x80 | ((cp >> 6) & 0x3F) as u8;
            out[2] = 0x80 | (cp & 0x3F) as u8;
        }
        _ => {
            out[0] = 0xF0 | (cp >> 18) as u8;
            out[1] = 0x80 | ((cp >> 12) & 0x3F) as u8;
            out[2] = 0x80 | ((cp >> 6) & 0x3F) as u8;
            out[3] = 0x80 | (cp & 0x3F) as u8;
        }
    }
    need
}

fn hex4(s: &[u8]) -> Option<u32> {
    if s.len() < 4 {
        return None;
    }
    let mut value = 0u32;
    for &c in &s[..4] {
        let digit = match c {
            b'0'..=b'9' => u32::from(c - b'0'),
            b'a'..=b'f' => u32::from(c - b'a') + 10,
            b'A'..=b'F' => u32::from(c - b'A') + 10,
            _ => return None,
        };
        value = value * 16 + digit;
    }
    Some(value)
}

/// `strtol`'s value for `s`: an optional sign then decimal digits, stopping at
/// the first other character, saturating on overflow. `None` when no digit was
/// consumed, which is when `strtol` leaves `endptr == start`.
fn parse_int_prefix(s: &[u8]) -> Option<i64> {
    let mut i = 0usize;
    let mut negative = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        negative = s[i] == b'-';
        i += 1;
    }

    let digits_start = i;
    let mut value: i64 = 0;
    let mut overflowed = false;

    while i < s.len() && s[i].is_ascii_digit() {
        let digit = i64::from(s[i] - b'0');
        match value.checked_mul(10).and_then(|v| v.checked_add(digit)) {
            Some(v) => value = v,
            None => {
                overflowed = true;
                break;
            }
        }
        i += 1;
    }

    if i == digits_start {
        return None;
    }
    if overflowed {
        return Some(if negative { i64::MIN } else { i64::MAX });
    }
    Some(if negative { -value } else { value })
}

#[inline]
fn clamp_i32(v: i64) -> i32 {
    if v > i64::from(i32::MAX) {
        i32::MAX
    } else if v < i64::from(i32::MIN) {
        i32::MIN
    } else {
        v as i32
    }
}

/// `atoi` on an all-digit slice: the C path walker uses it for array indices.
fn parse_index(digits: &[u8]) -> i32 {
    let mut value: i64 = 0;
    for &c in digits {
        value = value * 10 + i64::from(c - b'0');
        if value > i64::from(i32::MAX) {
            return i32::MAX;
        }
    }
    value as i32
}

/// The length of the prefix of `s` that `strtod` would consume.
fn float_prefix_len(s: &[u8]) -> usize {
    let mut i = 0usize;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        i += 1;
    }

    let mut digits = 0usize;
    while i < s.len() && s[i].is_ascii_digit() {
        i += 1;
        digits += 1;
    }

    let mut end = i;
    if i < s.len() && s[i] == b'.' {
        let mut j = i + 1;
        let mut frac = 0usize;
        while j < s.len() && s[j].is_ascii_digit() {
            j += 1;
            frac += 1;
        }
        if digits + frac > 0 {
            end = j;
            digits += frac;
        }
    }

    if digits == 0 {
        return 0;
    }

    if end < s.len() && (s[end] == b'e' || s[end] == b'E') {
        let mut j = end + 1;
        if j < s.len() && (s[j] == b'+' || s[j] == b'-') {
            j += 1;
        }
        let exp_start = j;
        while j < s.len() && s[j].is_ascii_digit() {
            j += 1;
        }
        if j > exp_start {
            end = j;
        }
    }

    end
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The shape Qobuz actually sends, cut down: nested objects, an array of
    /// objects, a number, a bool.
    const QOBUZ: &str = r#"{
        "tracks": {
            "total": 2,
            "items": [
                {
                    "id": 12345,
                    "title": "So What",
                    "duration": 545,
                    "album": { "image": { "small": "https://img/q/1_s.jpg" } },
                    "performer": { "name": "Miles Davis" }
                },
                {
                    "id": 67890,
                    "title": "Blue in Green",
                    "album": { "image": { "small": "https://img/q/2_s.jpg" } }
                }
            ]
        },
        "hires": true
    }"#;

    #[test]
    fn navigates_the_right_field() {
        let doc = Doc::parse_str(QOBUZ).unwrap();
        let root = doc.root();
        assert_eq!(doc.kind(root), Type::Object);

        let items = doc.path(root, "tracks.items");
        assert_eq!(doc.len(items), 2);

        let second = doc.at(items, 1);
        assert_eq!(
            doc.obj_string(second, "title").as_deref(),
            Some("Blue in Green")
        );
        assert_eq!(doc.obj_long(second, "id", -1), 67890);
        assert_eq!(
            doc.obj_string(second, "album.image.small").as_deref(),
            None,
            "a dotted path is not a key: it must not match anything"
        );
        assert_eq!(
            doc.string(doc.path(second, "album.image.small")).as_deref(),
            Some("https://img/q/2_s.jpg")
        );
        assert_eq!(doc.obj_boolean(root, "hires", false), true);
        assert_eq!(doc.obj_long(root, "tracks.total", -1), -1);
        assert_eq!(doc.obj_long(doc.path(root, "tracks"), "total", -1), 2);
    }

    #[test]
    fn out_of_range_indexes_are_minus_one() {
        let doc = Doc::parse_str(QOBUZ).unwrap();
        let items = doc.path(0, "tracks.items");
        assert_eq!(doc.at(items, 2), -1);
        assert_eq!(doc.at(items, -1), -1);
        assert_eq!(doc.at(0, 0), -1, "an object is not an array");
        assert_eq!(doc.get(0, "nope"), -1);
        assert_eq!(doc.path(0, "tracks.nope.deeper"), -1);
        assert_eq!(doc.len(-1), 0);
    }

    #[test]
    fn escapes_and_surrogates() {
        let text = r#"{"a":"line\nbreak\ttab \"q\" \\ \/ \u00e9 \u4e16","b":"\ud83c\udfb5 note"}"#;
        let doc = Doc::parse_str(text).unwrap();
        assert_eq!(
            doc.obj_string(0, "a").unwrap(),
            "line\nbreak\ttab \"q\" \\ / é 世"
        );
        assert_eq!(doc.obj_string(0, "b").unwrap(), "🎵 note");
    }

    #[test]
    fn escaped_keys_are_matched() {
        let doc = Doc::parse_str(r#"{"a\u0062c":1}"#).unwrap();
        assert_eq!(doc.get(0, "abc"), doc.get(0, "abc"));
        assert_ne!(doc.get(0, "abc"), -1, "an escaped key must still be found");
        assert_eq!(doc.obj_long(0, "abc", -1), 1);
    }

    #[test]
    fn malformed_documents_are_rejected() {
        for bad in [
            r#"{"a":1,}"#,        // trailing comma in an object
            r#"[1,2,]"#,          // trailing comma in an array
            r#"{"a" 1}"#,         // missing colon
            r#"{a:1}"#,           // unquoted key
            r#"{"a":1} {"b":2}"#, // a second document behind the first
            r#"{"a":}"#,          // missing value
            r#""unterminated"#,   // unterminated string
            r#""#,                // nothing at all
        ] {
            assert!(
                Doc::parse_str(bad).is_none(),
                "should have been rejected: {bad}"
            );
        }
    }

    #[test]
    fn nesting_limit_is_enforced() {
        let deep = format!("{}1{}", "[".repeat(MAX_DEPTH), "]".repeat(MAX_DEPTH));
        assert!(Doc::parse_str(&deep).is_none(), "past the limit");

        let ok = format!(
            "{}1{}",
            "[".repeat(MAX_DEPTH - 1),
            "]".repeat(MAX_DEPTH - 1)
        );
        assert!(Doc::parse_str(&ok).is_some(), "just inside the limit");
    }

    #[test]
    fn token_cap_is_honoured() {
        let text = r#"{"a":1,"b":2,"c":3}"#;
        assert_eq!(Doc::parse_str(text).unwrap().token_count(), 7);
        assert!(Doc::parse_limited(text.as_bytes(), 6).is_none());
        assert!(Doc::parse_limited(text.as_bytes(), 7).is_some());
        assert!(Doc::parse_limited(text.as_bytes(), 0).is_none());
    }

    #[test]
    fn numbers_that_arrive_as_strings() {
        let doc = Doc::parse_str(r#"{"id":"42","ratio":0.75,"big":2147483648}"#).unwrap();
        assert_eq!(doc.obj_long(0, "id", -1), 42);
        assert_eq!(doc.obj_double(0, "ratio", 0.0), 0.75);
        // 32-bit long on the target: past the range it saturates, the way
        // strtol did, instead of wrapping to a negative id.
        assert_eq!(doc.obj_long(0, "big", -1), i32::MAX);
        assert_eq!(doc.obj_llong(0, "big", -1), 2_147_483_648);
        assert_eq!(doc.obj_long(0, "missing", 7), 7);
    }

    #[test]
    fn eleven_digit_ids_survive_at_64_bits() {
        let doc = Doc::parse_str(r#"{"episodeId":12345678901}"#).unwrap();
        assert_eq!(doc.obj_llong(0, "episodeId", -1), 12_345_678_901);
        assert_eq!(
            doc.obj_long(0, "episodeId", -1),
            i32::MAX,
            "the 32-bit read is why json_llong exists"
        );
    }

    #[test]
    fn bools_as_bools_and_as_zero_one() {
        let doc = Doc::parse_str(r#"{"a":true,"b":false,"c":1,"d":0}"#).unwrap();
        assert!(doc.obj_boolean(0, "a", false));
        assert!(!doc.obj_boolean(0, "b", true));
        assert!(doc.obj_boolean(0, "c", false));
        assert!(!doc.obj_boolean(0, "d", true));
        assert!(doc.obj_boolean(0, "missing", true));
        assert!(!doc.obj_boolean(0, "missing", false));
    }

    #[test]
    fn truncation_is_lenient_but_exact_is_not() {
        let doc = Doc::parse_str(r#"{"a":"abcdefghij"}"#).unwrap();
        let tok = doc.get(0, "a");

        let mut buf = [0u8; 5];
        let n = doc.string_into(tok, &mut buf, false).unwrap();
        assert_eq!(n, 4);
        assert_eq!(&buf[..5], b"abcd\0");

        assert!(
            doc.string_into(tok, &mut buf, true).is_none(),
            "a credential must not be silently shortened"
        );

        let mut big = [0u8; 32];
        assert_eq!(doc.string_into(tok, &mut big, true), Some(10));
        assert_eq!(&big[..11], b"abcdefghij\0");
    }

    #[test]
    fn exact_rejects_embedded_nul_and_unpaired_surrogate() {
        let doc = Doc::parse_str(r#"{"nul":"a\u0000b","half":"\ud83c"}"#).unwrap();
        let mut big = [0u8; 64];
        assert!(doc.string_into(doc.get(0, "nul"), &mut big, true).is_none());
        assert!(doc
            .string_into(doc.get(0, "half"), &mut big, true)
            .is_none());

        // The lenient path still produces something, which is what a title
        // wants: a stray half-surrogate should show as a box, not blank the row.
        assert!(doc
            .string_into(doc.get(0, "nul"), &mut big, false)
            .is_some());
    }

    #[test]
    fn wrong_types_yield_the_fallback() {
        let doc = Doc::parse_str(r#"{"s":"text","n":5,"o":{}}"#).unwrap();
        assert_eq!(doc.obj_long(0, "s", -1), -1, "\"text\" is not a number");
        assert_eq!(doc.obj_string(0, "n").as_deref(), None);
        assert_eq!(doc.len(doc.get(0, "o")), 0);
        assert_eq!(doc.string(-1).as_deref(), None);
        assert_eq!(doc.string_into(-1, &mut [0u8; 8], false), None);
    }

    #[test]
    fn token_layout_matches_the_c_struct() {
        // json_tok_t is five ints; the C side reads these documents directly.
        assert_eq!(core::mem::size_of::<Tok>(), 20);
        assert_eq!(core::mem::align_of::<Tok>(), 4);
        let doc = Doc::parse_str(r#"{"k":"v"}"#).unwrap();
        // root, key, value — in document order, the key parented to the object
        let toks = doc.tokens();
        assert_eq!(toks.len(), 3);
        assert_eq!(toks[0].kind(), Type::Object);
        assert_eq!(toks[0].parent, -1);
        assert_eq!(toks[1].kind(), Type::String);
        assert_eq!(toks[1].parent, 0);
        assert_eq!(toks[2].parent, 1);
        assert_eq!(&doc.raw(1), b"k");
        assert_eq!(&doc.raw(2), b"v");
    }

    #[test]
    fn empty_containers() {
        let doc = Doc::parse_str(r#"{"a":[],"b":{}}"#).unwrap();
        assert_eq!(doc.len(doc.get(0, "a")), 0);
        assert_eq!(doc.len(doc.get(0, "b")), 0);
        assert_eq!(doc.at(doc.get(0, "a"), 0), -1);
    }
}
