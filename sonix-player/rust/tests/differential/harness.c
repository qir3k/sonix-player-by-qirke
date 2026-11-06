/*
 * Differential test: the original C core beside the Rust port.
 *
 * Both implementations are linked into one program — the C ones with their
 * symbols renamed by -D (see run.sh), the Rust ones under their canonical
 * names — and fed identical inputs. Any disagreement is printed and counted,
 * and the program exits non-zero.
 *
 * This is the test that matters for a port. Unit tests check the Rust code
 * against what the header *says*; this checks it against what the C code
 * *does*, including the parts nobody wrote down.
 *
 * Built and run by run.sh. Covers base64, md5, sha1, sha256, json, config, the
 * pure half of utils, textfile and tagdate.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------------ */
/* Prototypes, declared by hand on both sides                                */
/*                                                                          */
/* The C headers are deliberately not included: including one while the C   */
/* sources are compiled with renamed symbols would declare the renamed      */
/* functions, and the Rust ones would then be implicit. Declaring both here */
/* is also what makes the harness a test of the *ABI* — if the struct       */
/* layouts below did not match the C headers, this would fail.              */
/* ------------------------------------------------------------------------ */

typedef struct {
	int type;
	int start;
	int end;
	int size;
	int parent;
} tok_t;

typedef struct {
	const char *text;
	tok_t *toks;
	int count;
} doc_t;

/* The C originals, renamed by run.sh. */
extern unsigned char *c_base64_decode(const char *text, size_t text_len, size_t max_bytes, size_t *out_size);
extern void c_md5(const void *data, size_t len, unsigned char out[16]);
extern void c_md5_hex(const void *data, size_t len, char out[33]);
extern void c_sha1(const void *data, size_t len, unsigned char out[20]);
extern void c_sha1_hex(const void *data, size_t len, char out[41]);
extern void c_sha256_init(void *ctx);
extern void c_sha256_update(void *ctx, const void *data, size_t len);
extern void c_sha256_final_hex(void *ctx, char out[65]);

extern bool c_json_parse(const char *text, doc_t *doc);
extern bool c_json_parse_limited(const char *text, doc_t *doc, int max_tokens);
extern void c_json_free(doc_t *doc);
extern int c_json_root(const doc_t *doc);
extern int c_json_get(const doc_t *doc, int obj, const char *key);
extern int c_json_at(const doc_t *doc, int arr, int index);
extern int c_json_len(const doc_t *doc, int tok);
extern int c_json_path(const doc_t *doc, int from, const char *path);
extern bool c_json_str(const doc_t *doc, int tok, char *out, size_t size);
extern bool c_json_str_exact(const doc_t *doc, int tok, char *out, size_t size);
extern long c_json_long(const doc_t *doc, int tok, long fallback);
extern long long c_json_llong(const doc_t *doc, int tok, long long fallback);
extern double c_json_double(const doc_t *doc, int tok, double fallback);
extern bool c_json_bool(const doc_t *doc, int tok, bool fallback);
extern bool c_json_obj_str(const doc_t *doc, int obj, const char *key, char *out, size_t size);
extern long c_json_obj_long(const doc_t *doc, int obj, const char *key, long fallback);
extern long long c_json_obj_llong(const doc_t *doc, int obj, const char *key, long long fallback);
extern bool c_json_obj_bool(const doc_t *doc, int obj, const char *key, bool fallback);

extern bool c_config_init(const char *path);
extern const char *c_config_get(const char *section, const char *key, const char *fallback);
extern long c_config_get_int(const char *section, const char *key, long fallback);
extern bool c_config_get_bool(const char *section, const char *key, bool fallback);
extern void c_config_set(const char *section, const char *key, const char *value);
extern void c_config_set_int(const char *section, const char *key, long value);
extern void c_config_set_bool(const char *section, const char *key, bool value);
extern bool c_config_save(void);
extern const char *c_config_path(void);

/* The Rust port, canonical names. */
extern unsigned char *base64_decode(const char *text, size_t text_len, size_t max_bytes, size_t *out_size);
extern void md5(const void *data, size_t len, unsigned char out[16]);
extern void md5_hex(const void *data, size_t len, char out[33]);
extern void sha1(const void *data, size_t len, unsigned char out[20]);
extern void sha1_hex(const void *data, size_t len, char out[41]);
extern void sha256_init(void *ctx);
extern void sha256_update(void *ctx, const void *data, size_t len);
extern void sha256_final_hex(void *ctx, char out[65]);

extern bool json_parse(const char *text, doc_t *doc);
extern bool json_parse_limited(const char *text, doc_t *doc, int max_tokens);
extern void json_free(doc_t *doc);
extern int json_root(const doc_t *doc);
extern int json_get(const doc_t *doc, int obj, const char *key);
extern int json_at(const doc_t *doc, int arr, int index);
extern int json_len(const doc_t *doc, int tok);
extern int json_path(const doc_t *doc, int from, const char *path);
extern bool json_str(const doc_t *doc, int tok, char *out, size_t size);
extern bool json_str_exact(const doc_t *doc, int tok, char *out, size_t size);
extern long json_long(const doc_t *doc, int tok, long fallback);
extern long long json_llong(const doc_t *doc, int tok, long long fallback);
extern double json_double(const doc_t *doc, int tok, double fallback);
extern bool json_bool(const doc_t *doc, int tok, bool fallback);
extern bool json_obj_str(const doc_t *doc, int obj, const char *key, char *out, size_t size);
extern long json_obj_long(const doc_t *doc, int obj, const char *key, long fallback);
extern long long json_obj_llong(const doc_t *doc, int obj, const char *key, long long fallback);
extern bool json_obj_bool(const doc_t *doc, int obj, const char *key, bool fallback);

extern bool config_init(const char *path);
extern const char *config_get(const char *section, const char *key, const char *fallback);
extern long config_get_int(const char *section, const char *key, long fallback);
extern bool config_get_bool(const char *section, const char *key, bool fallback);
extern void config_set(const char *section, const char *key, const char *value);
extern void config_set_int(const char *section, const char *key, long value);
extern void config_set_bool(const char *section, const char *key, bool value);
extern bool config_save(void);
extern const char *config_path(void);

/* utils: the pure prefix of utils.c, extracted by run.sh and compiled with the
 * same -D renames as the rest. The thread, mmap and breadcrumb half of that
 * file is not here because it cannot be compiled off the device. */
extern uint32_t c_utils_isqrt32(uint32_t v);
extern uint64_t c_utils_isqrt64(uint64_t v);
extern bool c_has_extension(const char *name, const char *ext);
extern int c_formatDoubleSeconds(double total_seconds, char *buffer, size_t max_len);
extern void c_formatDoubleProgress(double current_secs, double total_secs, char *buffer, size_t max_len);
extern void c_format_row_duration(int seconds, char *out, size_t size);

extern uint32_t utils_isqrt32(uint32_t v);
extern uint64_t utils_isqrt64(uint64_t v);
extern bool has_extension(const char *name, const char *ext);
extern int formatDoubleSeconds(double total_seconds, char *buffer, size_t max_len);
extern void formatDoubleProgress(double current_secs, double total_secs, char *buffer, size_t max_len);
extern void format_row_duration(int seconds, char *out, size_t size);

/* textfile */
extern char *c_textfile_decode(const unsigned char *raw, size_t len);
extern bool c_textfile_read(const char *path, size_t max, unsigned char **raw, size_t *bytes, bool *truncated);
extern bool c_textfile_write(const char *path, const char *text);

extern char *textfile_decode(const unsigned char *raw, size_t len);
extern bool textfile_read(const char *path, size_t max, unsigned char **raw, size_t *bytes, bool *truncated);
extern bool textfile_write(const char *path, const char *text);

/* tagdate */
typedef struct {
	int year;
	int month;
	int day;
} date_fields;

extern bool c_tagdate_parse(const char *text, date_fields *out);
extern bool c_tagdate_better(const date_fields *a, const date_fields *b);
extern time_t c_tagdate_to_time(const date_fields *d);

extern bool tagdate_parse(const char *text, date_fields *out);
extern bool tagdate_better(const date_fields *a, const date_fields *b);
extern time_t tagdate_to_time(const date_fields *d);

/* id3chap */
typedef struct {
	double start;
	char title[128];
} id3chap_fields;

extern int c_id3chap_read(const char *path, id3chap_fields *out, int max);
extern bool c_id3chap_present(const char *path);

extern int id3chap_read(const char *path, id3chap_fields *out, int max);
extern bool id3chap_present(const char *path);

/* addons */
typedef struct {
	const char *id;
	const char *label;
	const char *note;
	const char *section;
} addon_row;

extern int c_addons_count(void);
extern const addon_row *c_addons_at(int index);
extern const addon_row *c_addons_find(const char *id);
extern bool c_addons_enabled(const char *id);
extern bool c_addons_set(const char *id, bool on);
extern void c_addons_apply_defaults(bool (*is_default)(const char *id));
extern bool c_addons_any_game_enabled(void);

extern int addons_count(void);
extern const addon_row *addons_at(int index);
extern const addon_row *addons_find(const char *id);
extern bool addons_enabled(const char *id);
extern bool addons_set(const char *id, bool on);
extern void addons_apply_defaults(bool (*is_default)(const char *id));
extern bool addons_any_game_enabled(void);

/* ------------------------------------------------------------------------ */
/* Reporting                                                                 */
/* ------------------------------------------------------------------------ */

static int checks;
static int failures;
static int shown;

static void fail(const char *what, const char *detail) {
	failures++;
	if (shown < 25) {
		shown++;
		printf("FAIL  %s: %s\n", what, detail);
	} else if (shown == 25) {
		shown++;
		printf("FAIL  (further failures suppressed)\n");
	}
}

#define CHECK(cond, what, detail)  \
	do {                           \
		checks++;                  \
		if (!(cond)) {             \
			fail(what, detail);    \
		}                          \
	} while (0)

/* ------------------------------------------------------------------------ */
/* A tiny deterministic PRNG, so a failing run is reproducible              */
/* ------------------------------------------------------------------------ */

static uint32_t rng_state = 0x12345678u;

static uint32_t rnd(void) {
	uint32_t x = rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng_state = x;
	return x;
}

static uint32_t rnd_below(uint32_t n) { return n ? rnd() % n : 0; }

/* ------------------------------------------------------------------------ */
/* base64                                                                   */
/* ------------------------------------------------------------------------ */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t encode_b64(const unsigned char *src, size_t len, char *out) {
	size_t w = 0;
	for (size_t i = 0; i < len; i += 3) {
		unsigned char b0 = src[i];
		unsigned char b1 = i + 1 < len ? src[i + 1] : 0;
		unsigned char b2 = i + 2 < len ? src[i + 2] : 0;
		uint32_t n = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b2;
		out[w++] = B64[(n >> 18) & 63];
		out[w++] = B64[(n >> 12) & 63];
		out[w++] = i + 1 < len ? B64[(n >> 6) & 63] : '=';
		out[w++] = i + 2 < len ? B64[n & 63] : '=';
	}
	out[w] = 0;
	return w;
}

static void compare_b64(const char *text, size_t len, size_t cap, const char *label) {
	size_t c_size = 0;
	size_t r_size = 0;
	unsigned char *c_out = c_base64_decode(text, len, cap, &c_size);
	unsigned char *r_out = base64_decode(text, len, cap, &r_size);

	bool both_null = c_out == NULL && r_out == NULL;
	CHECK(both_null || (c_out != NULL && r_out != NULL), label, "one side decoded, the other did not");

	if (c_out && r_out) {
		if (c_size != r_size) {
			CHECK(false, label, "decoded lengths differ");
		} else if (memcmp(c_out, r_out, c_size) != 0) {
			CHECK(false, label, "decoded bytes differ");
		} else {
			checks++;
		}
	}

	free(c_out);
	free(r_out);
}

static void test_base64(void) {
	unsigned char bytes[300];
	char encoded[520];

	for (int iter = 0; iter < 400; iter++) {
		size_t len = rnd_below(sizeof bytes);
		for (size_t i = 0; i < len; i++) {
			bytes[i] = (unsigned char)rnd();
		}

		char label[96];
		snprintf(label, sizeof label, "base64 len=%zu (#%d)", len, iter);

		size_t enc_len = encode_b64(bytes, len, encoded);
		compare_b64(encoded, enc_len, 4096, label);

		/* Whitespace and padding must be skipped the same way. */
		char wrapped[700];
		size_t w = 0;
		for (size_t i = 0; i < enc_len && w + 1 < sizeof wrapped; i++) {
			if (i && i % 7 == 0) {
				wrapped[w++] = (i % 2) ? '\n' : ' ';
			}
			wrapped[w++] = encoded[i];
		}
		wrapped[w] = 0;
		compare_b64(wrapped, w, 4096, label);

		/* Unpadded input. */
		size_t bare = enc_len;
		while (bare && encoded[bare - 1] == '=') {
			bare--;
		}
		compare_b64(encoded, bare, 4096, label);

		/* A cap that may or may not be enough: this is the max_bytes rule. */
		compare_b64(encoded, enc_len, len, label);
		compare_b64(encoded, enc_len, len > 0 ? len - 1 : 0, label);

		/* URL-safe spelling of the same bytes. */
		char url[520];
		memcpy(url, encoded, enc_len + 1);
		for (size_t i = 0; i < enc_len; i++) {
			if (url[i] == '+') {
				url[i] = '-';
			} else if (url[i] == '/') {
				url[i] = '_';
			}
		}
		compare_b64(url, enc_len, 4096, label);
	}

	/* Garbage and the degenerate cases. */
	compare_b64("", 0, 64, "base64 empty");
	compare_b64("====", 4, 64, "base64 pad only");
	compare_b64(" \r\n\t", 4, 64, "base64 whitespace only");
	compare_b64("!!!!*&^%", 8, 64, "base64 garbage");
	compare_b64("aGVsbG8=", 8, 4, "base64 cap too small");
	compare_b64("aGVsbG8=", 8, 5, "base64 cap exact");
}

/* ------------------------------------------------------------------------ */
/* Hashes                                                                   */
/* ------------------------------------------------------------------------ */

static void compare_hash(const unsigned char *data, size_t len, int iter) {
	char c_md5_out[33];
	char r_md5_out[33];
	char c_sha1_out[41];
	char r_sha1_out[41];
	unsigned char c_digest[20];
	unsigned char r_digest[20];
	char label[96];

	snprintf(label, sizeof label, "md5 len=%zu (#%d)", len, iter);
	c_md5_hex(data, len, c_md5_out);
	md5_hex(data, len, r_md5_out);
	CHECK(strcmp(c_md5_out, r_md5_out) == 0, label, "hex differs");

	c_md5(data, len, c_digest);
	md5(data, len, r_digest);
	CHECK(memcmp(c_digest, r_digest, 16) == 0, label, "digest differs");

	snprintf(label, sizeof label, "sha1 len=%zu (#%d)", len, iter);
	c_sha1_hex(data, len, c_sha1_out);
	sha1_hex(data, len, r_sha1_out);
	CHECK(strcmp(c_sha1_out, r_sha1_out) == 0, label, "hex differs");

	c_sha1(data, len, c_digest);
	sha1(data, len, r_digest);
	CHECK(memcmp(c_digest, r_digest, 20) == 0, label, "digest differs");

	/* SHA-256 is incremental on both sides; feed it in uneven pieces. */
	snprintf(label, sizeof label, "sha256 len=%zu (#%d)", len, iter);
	unsigned char c_ctx[256];
	unsigned char r_ctx[256];
	c_sha256_init(c_ctx);
	sha256_init(r_ctx);

	size_t at = 0;
	while (at < len) {
		size_t take = 1 + rnd_below(64);
		if (take > len - at) {
			take = len - at;
		}
		c_sha256_update(c_ctx, data + at, take);
		sha256_update(r_ctx, data + at, take);
		at += take;
	}
	if (len == 0) {
		c_sha256_update(c_ctx, data, 0);
		sha256_update(r_ctx, data, 0);
	}

	char c_sha256_out[65];
	char r_sha256_out[65];
	c_sha256_final_hex(c_ctx, c_sha256_out);
	sha256_final_hex(r_ctx, r_sha256_out);
	CHECK(strcmp(c_sha256_out, r_sha256_out) == 0, label, "hex differs");
}

static void test_hashes(void) {
	unsigned char bytes[1200];

	for (int iter = 0; iter < 300; iter++) {
		size_t len = rnd_below(sizeof bytes);
		for (size_t i = 0; i < len; i++) {
			bytes[i] = (unsigned char)rnd();
		}
		compare_hash(bytes, len, iter);
	}

	/* The padding boundaries, which are where these implementations break. */
	for (size_t len = 0; len <= 130; len++) {
		memset(bytes, 'x', len);
		compare_hash(bytes, len, -1);
	}
}

/* ------------------------------------------------------------------------ */
/* JSON                                                                     */
/* ------------------------------------------------------------------------ */

static void compare_doc(const char *text, const char *label) {
	doc_t c_doc;
	doc_t r_doc;
	memset(&c_doc, 0, sizeof c_doc);
	memset(&r_doc, 0, sizeof r_doc);

	bool c_ok = c_json_parse(text, &c_doc);
	bool r_ok = json_parse(text, &r_doc);

	CHECK(c_ok == r_ok, label, "one parsed, the other did not");
	if (c_ok != r_ok) {
		c_json_free(&c_doc);
		json_free(&r_doc);
		return;
	}

	CHECK(c_doc.count == r_doc.count, label, "token counts differ");
	if (c_doc.count != r_doc.count) {
		c_json_free(&c_doc);
		json_free(&r_doc);
		return;
	}

	for (int i = 0; i < c_doc.count; i++) {
		if (memcmp(&c_doc.toks[i], &r_doc.toks[i], sizeof(tok_t)) != 0) {
			char detail[192];
			snprintf(
				detail,
				sizeof detail,
				"token %d differs: C{%d,%d,%d,%d,%d} R{%d,%d,%d,%d,%d}",
				i,
				c_doc.toks[i].type, c_doc.toks[i].start, c_doc.toks[i].end,
				c_doc.toks[i].size, c_doc.toks[i].parent,
				r_doc.toks[i].type, r_doc.toks[i].start, r_doc.toks[i].end,
				r_doc.toks[i].size, r_doc.toks[i].parent);
			CHECK(false, label, detail);
			break;
		}
	}

	/* Navigation, over every token and a handful of keys and indices. */
	static const char *keys[] = {"a", "id", "title", "small", "items", "tracks",
	                            "album", "image", "performer", "nope", "", "a.b"};
	static const char *paths[] = {"a", "a.b", "tracks.items", "tracks.items.0",
	                              "tracks.items.0.title", "items.0", "0.1", "a.0.1",
	                              "a.b.c.d", "id", ".", "a.", ""};

	for (int tok = -1; tok < c_doc.count; tok++) {
		CHECK(c_json_len(&c_doc, tok) == json_len(&r_doc, tok), label, "json_len differs");

		for (size_t k = 0; k < sizeof keys / sizeof *keys; k++) {
			int c_at = c_json_get(&c_doc, tok, keys[k]);
			int r_at = json_get(&r_doc, tok, keys[k]);
			CHECK(c_at == r_at, label, "json_get differs");
		}

		for (int index = -1; index < 5; index++) {
			CHECK(c_json_at(&c_doc, tok, index) == json_at(&r_doc, tok, index),
			      label, "json_at differs");
		}

		for (size_t p = 0; p < sizeof paths / sizeof *paths; p++) {
			CHECK(c_json_path(&c_doc, tok, paths[p]) == json_path(&r_doc, tok, paths[p]),
			      label, "json_path differs");
		}

		/* Extraction, on every token, at several buffer sizes. */
		static const size_t sizes[] = {0, 1, 2, 3, 5, 64, 1024};
		for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++) {
			char c_out[1024];
			char r_out[1024];
			memset(c_out, 0x7F, sizeof c_out);
			memset(r_out, 0x7F, sizeof r_out);
			size_t size = sizes[s];

			bool c_str = c_json_str(&c_doc, tok, c_out, size);
			bool r_str = json_str(&r_doc, tok, r_out, size);
			CHECK(c_str == r_str, label, "json_str result differs");
			if (c_str == r_str && size > 0) {
				CHECK(strcmp(c_out, r_out) == 0, label, "json_str output differs");
			}

			memset(c_out, 0x7F, sizeof c_out);
			memset(r_out, 0x7F, sizeof r_out);
			bool c_exact = c_json_str_exact(&c_doc, tok, c_out, size);
			bool r_exact = json_str_exact(&r_doc, tok, r_out, size);
			CHECK(c_exact == r_exact, label, "json_str_exact result differs");
			if (c_exact == r_exact && size > 0) {
				checks++;
				if (strcmp(c_out, r_out) != 0) {
					char detail[320];
					snprintf(detail, sizeof detail,
					         "tok=%d size=%zu C=\"%s\" R=\"%s\"",
					         tok, size, c_out, r_out);
					fail(label, detail);
				}
			}
		}

		CHECK(c_json_long(&c_doc, tok, -7) == json_long(&r_doc, tok, -7),
		      label, "json_long differs");
		CHECK(c_json_llong(&c_doc, tok, -7) == json_llong(&r_doc, tok, -7),
		      label, "json_llong differs");

		double c_d = c_json_double(&c_doc, tok, -7.5);
		double r_d = json_double(&r_doc, tok, -7.5);
		CHECK(memcmp(&c_d, &r_d, sizeof c_d) == 0, label, "json_double differs");

		CHECK(c_json_bool(&c_doc, tok, true) == json_bool(&r_doc, tok, true),
		      label, "json_bool differs");
		CHECK(c_json_bool(&c_doc, tok, false) == json_bool(&r_doc, tok, false),
		      label, "json_bool differs");

		for (size_t k = 0; k < sizeof keys / sizeof *keys; k++) {
			char c_out[256];
			char r_out[256];
			bool c_s = c_json_obj_str(&c_doc, tok, keys[k], c_out, sizeof c_out);
			bool r_s = json_obj_str(&r_doc, tok, keys[k], r_out, sizeof r_out);
			CHECK(c_s == r_s, label, "json_obj_str result differs");
			if (c_s == r_s) {
				CHECK(strcmp(c_out, r_out) == 0, label, "json_obj_str output differs");
			}
			CHECK(c_json_obj_long(&c_doc, tok, keys[k], -9) == json_obj_long(&r_doc, tok, keys[k], -9),
			      label, "json_obj_long differs");
			CHECK(c_json_obj_llong(&c_doc, tok, keys[k], -9) == json_obj_llong(&r_doc, tok, keys[k], -9),
			      label, "json_obj_llong differs");
			CHECK(c_json_obj_bool(&c_doc, tok, keys[k], true) == json_obj_bool(&r_doc, tok, keys[k], true),
			      label, "json_obj_bool differs");
		}
	}

	/* The token cap, at every size that matters for this document. */
	for (int cap = 0; cap <= c_doc.count + 1; cap++) {
		doc_t c_lim;
		doc_t r_lim;
		memset(&c_lim, 0, sizeof c_lim);
		memset(&r_lim, 0, sizeof r_lim);
		bool c_l = c_json_parse_limited(text, &c_lim, cap);
		bool r_l = json_parse_limited(text, &r_lim, cap);
		CHECK(c_l == r_l, label, "json_parse_limited result differs");
		if (c_l && r_l) {
			CHECK(c_lim.count == r_lim.count, label, "limited token counts differ");
		}
		c_json_free(&c_lim);
		json_free(&r_lim);
	}

	/* root() */
	CHECK(c_json_root(&c_doc) == json_root(&r_doc), label, "json_root differs");

	c_json_free(&c_doc);
	json_free(&r_doc);
}

/* A small recursive generator, so the corpus is wide rather than hand-picked. */
static void gen_ws(char **out, char *end) {
	const char *ws[] = {"", " ", "\n", "\t ", " \r\n "};
	const char *pick = ws[rnd_below(5)];
	while (*pick && *out < end) {
		*(*out)++ = *pick++;
	}
}

static void gen_string(char **out, char *end) {
	const char *pieces[] = {
		"So What", "Blue in Green", "a", "id", "title", "small", "items", "tracks",
		"hello \\\"quoted\\\"", "line\\nbreak", "tab\\there", "back\\\\slash",
		"unicode \\u00e9", "cjk \\u4e16\\u754c", "emoji \\ud83c\\udfb5", "half \\ud83c",
		"slash \\/ here", "nul \\u0000 inside", "  padded  ", "", "0123456789",
		"\\u0041\\u0042", "a.b", "then,comma", "brace}]", "quote'",
	};
	const char *pick = pieces[rnd_below(sizeof pieces / sizeof *pieces)];
	*(*out)++ = '"';
	while (*pick && *out < end - 2) {
		*(*out)++ = *pick++;
	}
	*(*out)++ = '"';
}

static void gen_value(char **out, char *end, int depth);

static void gen_number(char **out, char *end) {
	const char *numbers[] = {
		"0", "1", "-1", "42", "12345", "2147483647", "2147483648", "-2147483649",
		"99999999999999999999", "3.5", "-0.25", "1e10", "1.5E-3", "+7", "0.75",
		"12345678901234", " 5", "-", "1e", "1.2.3", "--1",
	};
	const char *pick = numbers[rnd_below(sizeof numbers / sizeof *numbers)];
	while (*pick && *out < end - 1) {
		*(*out)++ = *pick++;
	}
}

static void gen_value(char **out, char *end, int depth) {
	if (*out >= end - 8) {
		return;
	}
	uint32_t kind = rnd_below(depth > 3 ? 5 : 7);
	switch (kind) {
	case 0:
	case 1:
		gen_string(out, end);
		break;
	case 2:
		gen_number(out, end);
		break;
	case 3: {
		const char *lits[] = {"true", "false", "null"};
		const char *pick = lits[rnd_below(3)];
		while (*pick && *out < end - 1) {
			*(*out)++ = *pick++;
		}
		break;
	}
	case 4:
		*(*out)++ = '"';
		*(*out)++ = '"';
		break;
	case 5: {
		*(*out)++ = '{';
		int pairs = (int)rnd_below(4);
		for (int i = 0; i < pairs; i++) {
			if (i) {
				*(*out)++ = ',';
			}
			gen_ws(out, end);
			gen_string(out, end);
			gen_ws(out, end);
			*(*out)++ = ':';
			gen_ws(out, end);
			gen_value(out, end, depth + 1);
			gen_ws(out, end);
		}
		*(*out)++ = '}';
		break;
	}
	default: {
		*(*out)++ = '[';
		int elements = (int)rnd_below(4);
		for (int i = 0; i < elements; i++) {
			if (i) {
				*(*out)++ = ',';
			}
			gen_ws(out, end);
			gen_value(out, end, depth + 1);
			gen_ws(out, end);
		}
		*(*out)++ = ']';
		break;
	}
	}
}

static void test_json(void) {
	/* Hand-picked, including the malformed shapes a service sometimes sends. */
	static const char *corpus[] = {
		"{}", "[]", "[[]]", "{\"a\":1}", "{\"a\":{\"b\":{\"c\":{\"d\":1}}}}",
		"{\"tracks\":{\"total\":2,\"items\":[{\"id\":1,\"title\":\"x\"},{\"id\":2}]}}",
		"{\"a\":1,}", "[1,2,]", "{\"a\" 1}", "{a:1}", "{\"a\":1} {\"b\":2}",
		"{\"a\":}", "\"unterminated", "", " ", "null", "true", "false", "0", "-1.5e3",
		"\"\\u00e9\\u4e16\\ud83c\\udfb5\"", "{\"a\":\"\\ud83c\"}",
		"{\"a\":\"\\u0000\"}", "{\"a\":\"x\\\"y\"}", "{\"\\u0061\":1}",
		"{\"a\":[1,[2,[3,[4]]]]}", "{\"a\":1,\"a\":2}",
		"[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]",
		"{\"a\":\"\\\"}", "{\"a\":\"\\\\\"}", "{\"a\":\"\\q\"}",
		"{\"big\":99999999999999999999,\"small\":-99999999999999999999}",
		"{\"id\":\"12345\",\"flag\":0,\"on\":true}",
		"\xEF\xBB\xBF{\"bom\":1}",
		"{\"a\":1}\n\t ",
		"{\"a\":\"tab\tinside\"}",
	};
	for (size_t i = 0; i < sizeof corpus / sizeof *corpus; i++) {
		char label[64];
		snprintf(label, sizeof label, "json corpus #%zu", i);
		compare_doc(corpus[i], label);
	}

	/* Generated documents. */
	for (int iter = 0; iter < 250; iter++) {
		static char text[4096];
		char *out = text;
		char *end = text + sizeof text - 1;
		gen_value(&out, end, 1);
		*out = 0;
		char label[64];
		snprintf(label, sizeof label, "json generated #%d", iter);
		compare_doc(text, label);
	}

	/* Deep nesting, to exercise the depth limit on both sides. */
	for (int depth = 1; depth <= 70; depth++) {
		static char text[256];
		int w = 0;
		for (int i = 0; i < depth && w < 200; i++) {
			text[w++] = '[';
		}
		text[w++] = '1';
		for (int i = 0; i < depth && w < 250; i++) {
			text[w++] = ']';
		}
		text[w] = 0;
		char label[64];
		snprintf(label, sizeof label, "json depth %d", depth);
		compare_doc(text, label);
	}
}

/* ------------------------------------------------------------------------ */
/* Config                                                                   */
/* ------------------------------------------------------------------------ */

static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buffer = calloc(1, (size_t)size + 1);
	if (buffer) {
		fread(buffer, 1, (size_t)size, f);
	}
	fclose(f);
	return buffer;
}

static void test_config(const char *workdir) {
	static const char *ini =
		"# a comment\n"
		"; another comment\n"
		"\n"
		"[ui]\n"
		"theme = dark\n"
		"layout = poster   \n"
		"  spacing   =   wide  \n"
		"\n"
		"[eq]\n"
		"band0 = -3\n"
		"band1 = 0\n"
		"\n"
		"[clock]\n"
		"configured = 1\n"
		"last_seen = 1786000000\n"
		"flag = true\n"
		"yes = y\n"
		"no = n\n"
		"\n"
		"orphan = 5\n"
		"garbage line without equals\n"
		"[unclosed\n"
		"after = 7\n";

	char c_path[512];
	char r_path[512];
	char text[8192];
	snprintf(text, sizeof text, "[a]\nlongvalue = %s\n", "");
	for (int i = 0; i < 200; i++) {
		strncat(text, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", sizeof text - strlen(text) - 1);
	}

	snprintf(c_path, sizeof c_path, "%s/c-core.ini", workdir);
	snprintf(r_path, sizeof r_path, "%s/r-core.ini", workdir);

	FILE *f = fopen(c_path, "wb");
	fputs(ini, f);
	fclose(f);
	f = fopen(r_path, "wb");
	fputs(ini, f);
	fclose(f);

	bool c_ok = c_config_init(c_path);
	bool r_ok = config_init(r_path);
	CHECK(c_ok == r_ok, "config init", "one loaded, the other did not");

	static const char *sections[] = {"ui", "eq", "clock", "", "missing"};
	static const char *keys[] = {"theme", "layout", "spacing", "band0", "band1",
	                             "configured", "last_seen", "flag", "yes", "no",
	                             "orphan", "after", "nope"};
	static const char *fallback = "<fallback>";

	for (size_t s = 0; s < sizeof sections / sizeof *sections; s++) {
		for (size_t k = 0; k < sizeof keys / sizeof *keys; k++) {
			const char *c_v = c_config_get(sections[s], keys[k], fallback);
			const char *r_v = config_get(sections[s], keys[k], fallback);
			CHECK(strcmp(c_v, r_v) == 0, "config get", "values differ");

			long c_i = c_config_get_int(sections[s], keys[k], -11);
			long r_i = config_get_int(sections[s], keys[k], -11);
			CHECK(c_i == r_i, "config get_int", "values differ");

			CHECK(c_config_get_bool(sections[s], keys[k], true) ==
			          config_get_bool(sections[s], keys[k], true),
			      "config get_bool(true)", "values differ");
			CHECK(c_config_get_bool(sections[s], keys[k], false) ==
			          config_get_bool(sections[s], keys[k], false),
			      "config get_bool(false)", "values differ");
		}
	}

	CHECK(strcmp(c_config_path(), c_path) == 0, "config path (C)", "path not reported");
	CHECK(strcmp(config_path(), r_path) == 0, "config path (Rust)", "path not reported");

	/* The same setters, in the same order, on both sides. */
	const char *set_values[] = {"", "value", "0", "1", "-42", "true",
	                            "a value with spaces", "0123456789"};
	for (size_t i = 0; i < sizeof set_values / sizeof *set_values; i++) {
		c_config_set("set", "k", set_values[i]);
		config_set("set", "k", set_values[i]);
		c_config_set("set", "v", set_values[i]);
		config_set("set", "v", set_values[i]);
	}
	c_config_set_int("set", "n", -12345);
	config_set_int("set", "n", -12345);
	c_config_set_bool("set", "b", true);
	config_set_bool("set", "b", true);
	c_config_set_bool("set", "c", false);
	config_set_bool("set", "c", false);
	/* Updates in place, and a key too long for the 31-character field. */
	c_config_set("ui", "theme", "light");
	config_set("ui", "theme", "light");
	c_config_set("s1234567890123456789012345678901234567890",
	             "k1234567890123456789012345678901234567890",
	             "v1234567890123456789012345678901234567890123456789012345678901234567890");
	config_set("s1234567890123456789012345678901234567890",
	           "k1234567890123456789012345678901234567890",
	           "v1234567890123456789012345678901234567890123456789012345678901234567890");

	bool c_saved = c_config_save();
	bool r_saved = config_save();

#ifdef _WIN32
	/* POSIX rename() replaces the destination; Windows' does not. The C file's
	 * atomic save therefore fails here on the second save (the file already
	 * exists) while the Rust one succeeds, because std's rename uses the
	 * replacing API. That is a portability bug in the C code and not a
	 * behavioural difference on the device, so the comparison is made on Linux
	 * (CI) instead of being forced to match a broken Windows path. */
	(void)c_saved;
	(void)r_saved;
	checks++;
	printf("NOTE  config save: skipped on Windows (the C original cannot replace an "
	       "existing file); it runs on a POSIX host\n");
#else
	CHECK(c_saved == r_saved, "config save", "one saved, the other did not");

	char *c_file = read_file(c_path);
	char *r_file = read_file(r_path);
	CHECK(c_file != NULL && r_file != NULL, "config file", "a side wrote nothing");
	if (c_file && r_file) {
		if (strcmp(c_file, r_file) != 0) {
			printf("--- C wrote ---\n%s\n--- Rust wrote ---\n%s\n", c_file, r_file);
			CHECK(false, "config file bytes", "written files differ");
		} else {
			checks++;
		}
	}
	free(c_file);
	free(r_file);
#endif

	/* A long line, to check the 255-byte fgets split. */
	f = fopen(c_path, "wb");
	fputs(text, f);
	fclose(f);
	f = fopen(r_path, "wb");
	fputs(text, f);
	fclose(f);
	c_config_init(c_path);
	config_init(r_path);
	CHECK(strcmp(c_config_get("a", "longvalue", "-"), config_get("a", "longvalue", "-")) == 0,
	      "config long value", "values differ");

	remove(c_path);
	remove(r_path);
	char tmp[600];
	snprintf(tmp, sizeof tmp, "%s.tmp", c_path);
	remove(tmp);
	snprintf(tmp, sizeof tmp, "%s.tmp", r_path);
	remove(tmp);
}

/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* utils                                                                    */
/* ------------------------------------------------------------------------ */

static void compare_isqrt32(uint32_t v) {
	uint32_t c = c_utils_isqrt32(v);
	uint32_t r = utils_isqrt32(v);
	checks++;
	if (c != r) {
		char detail[96];
		snprintf(detail, sizeof detail, "v=%lu: C %lu, Rust %lu", (unsigned long)v,
		         (unsigned long)c, (unsigned long)r);
		fail("utils_isqrt32", detail);
	}
}

static void compare_isqrt64(uint64_t v) {
	uint64_t c = c_utils_isqrt64(v);
	uint64_t r = utils_isqrt64(v);
	checks++;
	if (c != r) {
		char detail[128];
		snprintf(detail, sizeof detail, "v=%llu: C %llu, Rust %llu",
		         (unsigned long long)v, (unsigned long long)c, (unsigned long long)r);
		fail("utils_isqrt64", detail);
	}
}

/* The formatters are compared on the whole buffer, not just the text: a
 * terminator written one byte too far is exactly the kind of bug that shows up
 * as a corrupted widget three screens later. FMT_MAX - 1 is the largest size
 * passed, so the sentinel at the end survives every call and can be printed. */
#define FMT_MAX 64

static void buffer_reset(char *buf) {
	memset(buf, 0xA5, FMT_MAX);
	buf[FMT_MAX - 1] = 0;
}

static void compare_seconds(double v, size_t cap) {
	char cbuf[FMT_MAX], rbuf[FMT_MAX];
	buffer_reset(cbuf);
	buffer_reset(rbuf);

	int cw = c_formatDoubleSeconds(v, cbuf, cap);
	int rw = formatDoubleSeconds(v, rbuf, cap);

	checks++;
	if (cw != rw || memcmp(cbuf, rbuf, sizeof cbuf) != 0) {
		char detail[220];
		snprintf(detail, sizeof detail, "%.17g cap=%lu: C \"%s\"(%d), Rust \"%s\"(%d)", v,
		         (unsigned long)cap, cbuf, cw, rbuf, rw);
		fail("formatDoubleSeconds", detail);
	}
}

static void compare_progress(double cur, double total, size_t cap) {
	char cbuf[FMT_MAX], rbuf[FMT_MAX];
	buffer_reset(cbuf);
	buffer_reset(rbuf);

	c_formatDoubleProgress(cur, total, cbuf, cap);
	formatDoubleProgress(cur, total, rbuf, cap);

	checks++;
	if (memcmp(cbuf, rbuf, sizeof cbuf) != 0) {
		char detail[220];
		snprintf(detail, sizeof detail, "%.17g/%.17g cap=%lu: C \"%s\", Rust \"%s\"", cur, total,
		         (unsigned long)cap, cbuf, rbuf);
		fail("formatDoubleProgress", detail);
	}
}

static void compare_row(int seconds, size_t size) {
	char cbuf[FMT_MAX], rbuf[FMT_MAX];
	buffer_reset(cbuf);
	buffer_reset(rbuf);

	c_format_row_duration(seconds, cbuf, size);
	format_row_duration(seconds, rbuf, size);

	checks++;
	if (memcmp(cbuf, rbuf, sizeof cbuf) != 0) {
		char detail[220];
		snprintf(detail, sizeof detail, "%d size=%lu: C \"%s\", Rust \"%s\"", seconds,
		         (unsigned long)size, cbuf, rbuf);
		fail("format_row_duration", detail);
	}
}

static void compare_ext(const char *name, const char *ext) {
	bool c = c_has_extension(name, ext);
	bool r = has_extension(name, ext);
	checks++;
	if (c != r) {
		char detail[200];
		snprintf(detail, sizeof detail, "name=\"%s\" ext=\"%s\": C %d, Rust %d", name, ext, (int)c, (int)r);
		fail("has_extension", detail);
	}
}

static void test_utils(void) {
	/* Integer square roots. The low range, the top, and every power of two on
	 * either side of itself — the edges are where a one-bit shift goes wrong. */
	for (uint32_t v = 0; v < 60000; v++) {
		compare_isqrt32(v);
	}
	for (uint32_t k = 0; k < 10000; k++) {
		compare_isqrt32(UINT32_MAX - k);
	}
	for (int b = 0; b < 32; b++) {
		uint32_t base = (uint32_t)1 << b;
		compare_isqrt32(base - 1);
		compare_isqrt32(base);
		if (base < UINT32_MAX) {
			compare_isqrt32(base + 1);
		}
	}
	for (int i = 0; i < 40000; i++) {
		compare_isqrt32(rnd());
	}

	static const uint64_t wide[] = {
	    0, 1, 2, 3, 4, 15, 16, 17, 255, 256, 65535, 65536, 4294967295ULL, 4294967296ULL,
	    4294967297ULL, 1ULL << 62, (1ULL << 62) + 1, UINT64_MAX - 1, UINT64_MAX};
	for (size_t i = 0; i < sizeof wide / sizeof wide[0]; i++) {
		compare_isqrt64(wide[i]);
	}
	for (int i = 0; i < 40000; i++) {
		compare_isqrt64(((uint64_t)rnd() << 32) | rnd());
	}
	for (int b = 0; b < 63; b++) {
		uint64_t base = 1ULL << b;
		compare_isqrt64(base - 1);
		compare_isqrt64(base);
	}

	/* Extensions, including the near-misses: a name that is exactly the
	 * extension, a name shorter than it, and an empty extension. */
	static const char *names[] = {"a.flac", "a.FLAC",  "a.Flac", "flac", ".flac",   "track.flac",
	                              "x.wav",  "noext",   "",      "a.b.c.mp3", "long.name.with.dots.OGG",
	                              "track.oga", "a.flac ", "flac .flac"};
	static const char *exts[] = {".flac", ".FLAC", ".ogg", ".wav", ".mp3", ".oga",
	                             "flac",  "",      ".flacx", ".f",  ".c"};
	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
		for (size_t j = 0; j < sizeof exts / sizeof exts[0]; j++) {
			compare_ext(names[i], exts[j]);
		}
	}

	/* Durations. Nothing at or past 2^32 seconds: C's double-to-uint32_t cast is
	 * undefined there and Rust saturates, so the two are allowed to differ on a
	 * number no track can have. */
	static const double secs[] = {0.0,      -0.0,      0.4,       0.5,       0.999, 1.0,
	                              5.9,      59.9,      60.0,      65.0,      3599.0, 3600.0,
	                              3601.0,   3661.5,    86399.0,   86400.0,   359999.0, 360000.0,
	                              4294967000.0, -1.0,  -3600.0,   -1e9,      1e-9,   12345.678};
	for (size_t i = 0; i < sizeof secs / sizeof secs[0]; i++) {
		for (size_t cap = 0; cap < FMT_MAX - 1; cap++) {
			compare_seconds(secs[i], cap);
		}
	}
	/* A negative time and a NaN both have to come out as zero. */
	for (size_t cap = 0; cap < FMT_MAX - 1; cap++) {
		compare_seconds(nan(""), cap);
		compare_seconds(-1.0 / 0.0, cap);
	}
	for (int i = 0; i < 3000; i++) {
		double v = (double)rnd() / 7.0;
		compare_seconds(v, rnd_below(FMT_MAX - 1));
		compare_seconds(-v, rnd_below(FMT_MAX - 1));
	}

	for (size_t i = 0; i < sizeof secs / sizeof secs[0]; i++) {
		for (size_t j = 0; j < sizeof secs / sizeof secs[0]; j += 3) {
			for (size_t cap = 0; cap < FMT_MAX - 1; cap += 3) {
				compare_progress(secs[i], secs[j], cap);
			}
		}
	}
	for (int i = 0; i < 2000; i++) {
		compare_progress((double)rnd() / 7.0, (double)rnd() / 3.0, rnd_below(FMT_MAX - 1));
	}

	for (int s = -8; s < 40000; s++) {
		for (size_t size = 0; size < FMT_MAX; size += 5) {
			compare_row(s, size);
		}
	}
	for (int i = 0; i < 2000; i++) {
		compare_row((int)rnd(), rnd_below(FMT_MAX));
	}
}

/* ------------------------------------------------------------------------ */
/* textfile                                                                 */
/* ------------------------------------------------------------------------ */

static void hex_into(char *dst, size_t dstsz, const unsigned char *p, size_t n) {
	size_t w = 0;
	dst[0] = 0;
	for (size_t i = 0; i < n && w + 3 < dstsz; i++) {
		w += (size_t)snprintf(dst + w, dstsz - w, "%02X ", p[i]);
	}
}

/* The decoded text never holds a NUL — every code point below 0x20 is dropped —
 * so strlen is a safe length here on both sides. */
static void compare_decode(const unsigned char *raw, size_t len, const char *label) {
	char *c = c_textfile_decode(raw, len);
	char *r = textfile_decode(raw, len);

	checks++;
	if (!c || !r) {
		free(c);
		free(r);
		fail(label, "one side decoded nothing");
		return;
	}

	size_t cl = strlen(c);
	size_t rl = strlen(r);
	checks++;
	if (cl != rl || (cl && memcmp(c, r, cl) != 0)) {
		char detail[400];
		char chex[180];
		char rhex[180];
		hex_into(chex, sizeof chex, (const unsigned char *)c, cl < 50 ? cl : 50);
		hex_into(rhex, sizeof rhex, (const unsigned char *)r, rl < 50 ? rl : 50);
		snprintf(detail, sizeof detail, "C %lu [%s] vs Rust %lu [%s]", (unsigned long)cl, chex,
		         (unsigned long)rl, rhex);
		fail(label, detail);
	}

	free(c);
	free(r);
}

static size_t utf8_of(uint32_t cp, unsigned char *out) {
	if (cp < 0x80) {
		out[0] = (unsigned char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (unsigned char)(0xC0 | (cp >> 6));
		out[1] = (unsigned char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (unsigned char)(0xE0 | (cp >> 12));
		out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (unsigned char)(0x80 | (cp & 0x3F));
		return 3;
	}
	out[0] = (unsigned char)(0xF0 | (cp >> 18));
	out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
	out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
	out[3] = (unsigned char)(0x80 | (cp & 0x3F));
	return 4;
}

static size_t utf16_of(uint32_t cp, unsigned char *out, bool le) {
	uint32_t units[2];
	int n;
	if (cp >= 0x10000) {
		uint32_t v = cp - 0x10000;
		units[0] = 0xD800 + (v >> 10);
		units[1] = 0xDC00 + (v & 0x3FF);
		n = 2;
	} else {
		units[0] = cp;
		n = 1;
	}
	for (int i = 0; i < n; i++) {
		if (le) {
			out[i * 2] = (unsigned char)(units[i] & 0xFF);
			out[i * 2 + 1] = (unsigned char)(units[i] >> 8);
		} else {
			out[i * 2] = (unsigned char)(units[i] >> 8);
			out[i * 2 + 1] = (unsigned char)(units[i] & 0xFF);
		}
	}
	return (size_t)n * 2;
}

static uint32_t pick_codepoint(void) {
	uint32_t r = rnd();
	switch (r % 6) {
	case 0:
		return r % 0x80; /* ASCII, controls included */
	case 1:
		return 0x80 + r % 0x780; /* two bytes */
	case 2:
		return 0x800 + r % 0xD000; /* three bytes, below the surrogates */
	case 3:
		return 0xE000 + r % 0x2000; /* three bytes, above them */
	case 4:
		return 0x10000 + r % 0x100000; /* four bytes */
	default:
		return r % 0x110000; /* anywhere, surrogates and all */
	}
}

#define FIX(s, label) {(const unsigned char *)(s), sizeof(s) - 1, label}

static void test_textfile(const char *workdir) {
	static const struct {
		const unsigned char *bytes;
		size_t len;
		const char *label;
	} fixed[] = {
	    FIX("", "empty"),
	    FIX("hello", "plain"),
	    FIX("a\r\nb", "crlf"),
	    FIX("a\rb", "lone cr"),
	    FIX("a\r\r\nb", "double cr"),
	    FIX("a\tb", "tab"),
	    FIX("a\0b\033c\177d", "controls"),
	    FIX("\xEF\xBB\xBFhello", "utf8 bom"),
	    FIX("a\xEF\xBB\xBFb", "utf8 bom inside"),
	    FIX("h\xC3\xA9llo", "utf8 e acute"),
	    FIX("\xE4\xB8\x96\xE7\x95\x8C", "utf8 cjk"),
	    FIX("\xF0\x9F\x8E\xB5 note", "utf8 emoji"),
	    FIX("\xFF\xFEh\0e\0l\0l\0o\0", "utf16le"),
	    FIX("\xFE\xFF\0h\0e\0l\0l\0o", "utf16be"),
	    FIX("\xFF\xFE\x3D\xD8\xB5\xDF", "utf16le pair"),
	    FIX("\xFF\xFE\x3C\xD8a\0", "utf16le unpaired"),
	    FIX("\xFF\xFE\r\0\n\0", "utf16le crlf"),
	    FIX("\x93quoted\x94", "1252 quotes"),
	    FIX("caf\xE9 x", "1252 e acute"),
	    FIX("a\x81b", "1252 hole"),
	    FIX("\x80", "1252 euro"),
	    FIX("caf\xE9", "1252 high byte last"),
	    FIX("\xC0\xAF", "overlong"),
	    FIX("ab\xE4\xB8", "clipped utf8 at the end"),
	};
	for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
		compare_decode(fixed[i].bytes, fixed[i].len, fixed[i].label);
	}

	/* A null pointer with no length is an empty file, and the C version allocates
	 * an empty string for it rather than failing. */
	compare_decode(NULL, 0, "null and empty");

	/* Random inputs of four kinds: raw bytes (which land on the Windows-1252
	 * path as soon as one sequence is invalid), ASCII, generated UTF-8 and
	 * generated UTF-16. */
	unsigned char buf[520];
	for (int round = 0; round < 3000; round++) {
		size_t want = rnd_below(400);
		int kind = (int)rnd_below(4);
		size_t w = 0;

		if (kind == 3 && want >= 2) {
			bool le = (rnd() & 1) != 0;
			buf[w++] = le ? 0xFF : 0xFE;
			buf[w++] = le ? 0xFE : 0xFF;
			while (w < want) {
				unsigned char enc[4];
				uint32_t cp = pick_codepoint();
				/* A surrogate is a single unit on this side and is dropped
				 * by both readers, which is the case worth covering. */
				size_t n = cp >= 0x10000 ? 4 : 2;
				if (w + n > want) {
					break;
				}
				size_t put = cp >= 0x10000 ? utf16_of(cp, buf + w, le)
				                             : utf16_of(cp, enc, le);
				if (cp < 0x10000) {
					memcpy(buf + w, enc, put);
				}
				w += put;
			}
		} else {
			while (w < want) {
				if (kind == 0) {
					buf[w++] = (unsigned char)rnd();
				} else if (kind == 1) {
					buf[w++] = (unsigned char)(rnd() & 0x7F);
				} else {
					unsigned char enc[4];
					size_t n = utf8_of(pick_codepoint(), enc);
					if (w + n > want) {
						break;
					}
					memcpy(buf + w, enc, n);
					w += n;
				}
			}
		}
		compare_decode(buf, w, "fuzz");
	}

	/* The file side: one file, read at every interesting limit. */
	char path[600];
	snprintf(path, sizeof path, "%s/diff_textfile_src.txt", workdir);
	FILE *f = fopen(path, "wb");
	if (!f) {
		fail("textfile", "could not create the test file");
		return;
	}
	for (int i = 0; i < 300; i++) {
		fputc('a' + (i % 26), f);
		if (i % 40 == 0) {
			fputs("\r\n", f);
		}
	}
	fclose(f);

	static const size_t limits[] = {0, 1, 2, 7, 255, 299, 300, 301, 4096, 65536};
	for (size_t i = 0; i < sizeof limits / sizeof limits[0]; i++) {
		unsigned char *c_raw = NULL;
		unsigned char *r_raw = NULL;
		size_t c_bytes = 12345;
		size_t r_bytes = 12345;
		bool c_trunc = true;
		bool r_trunc = true;

		bool c_ok = c_textfile_read(path, limits[i], &c_raw, &c_bytes, &c_trunc);
		bool r_ok = textfile_read(path, limits[i], &r_raw, &r_bytes, &r_trunc);

		checks++;
		if (c_ok != r_ok) {
			fail("textfile_read", "one side opened the file and the other did not");
		} else if (c_ok) {
			checks++;
			if (c_bytes != r_bytes || c_trunc != r_trunc ||
			    (c_bytes && memcmp(c_raw, r_raw, c_bytes) != 0)) {
				char detail[200];
				snprintf(detail, sizeof detail, "max=%lu: C %lu bytes%s, Rust %lu bytes%s",
				         (unsigned long)limits[i], (unsigned long)c_bytes,
				         c_trunc ? " truncated" : "", (unsigned long)r_bytes,
				         r_trunc ? " truncated" : "");
				fail("textfile_read", detail);
			}
		}
		free(c_raw);
		free(r_raw);
	}
	remove(path);

	/* A file that is not there. */
	{
		unsigned char *c_raw = NULL;
		unsigned char *r_raw = NULL;
		size_t c_bytes = 0;
		size_t r_bytes = 0;
		bool c_trunc = false;
		bool r_trunc = false;
		snprintf(path, sizeof path, "%s/diff_textfile_missing.txt", workdir);
		checks++;
		if (c_textfile_read(path, 64, &c_raw, &c_bytes, &c_trunc) !=
		    textfile_read(path, 64, &r_raw, &r_bytes, &r_trunc)) {
			fail("textfile_read", "a missing file was reported differently");
		}
		free(c_raw);
		free(r_raw);
	}

	/* Writes, through both sides, compared byte for byte. The C version appends
	 * the newline a note is supposed to end with, so the missing case matters. */
	static const char *writes[] = {"hello", "hello\n", "", "one\ntwo", "trailing \n\n",
	                              "\xC3\xA9t\xC3\xA9", "a\tb"};
	char c_path[600];
	char r_path[600];
	for (size_t i = 0; i < sizeof writes / sizeof writes[0]; i++) {
		snprintf(c_path, sizeof c_path, "%s/diff_write_c%lu.txt", workdir, (unsigned long)i);
		snprintf(r_path, sizeof r_path, "%s/diff_write_r%lu.txt", workdir, (unsigned long)i);

		bool c_ok = c_textfile_write(c_path, writes[i]);
		bool r_ok = textfile_write(r_path, writes[i]);

		checks++;
		if (c_ok != r_ok) {
			fail("textfile_write", "one side wrote and the other did not");
		} else if (c_ok) {
			char *cf = read_file(c_path);
			char *rf = read_file(r_path);
			checks++;
			if (!cf || !rf) {
				fail("textfile_write", "a side left no file behind");
			} else if (strcmp(cf, rf) != 0) {
				char detail[300];
				snprintf(detail, sizeof detail, "C \"%s\" vs Rust \"%s\"", cf, rf);
				fail("textfile_write", detail);
			}
			free(cf);
			free(rf);
		}
		remove(c_path);
		remove(r_path);
	}
}

/* ------------------------------------------------------------------------ */
/* tagdate                                                                  */
/* ------------------------------------------------------------------------ */

static void compare_tagdate(const char *text) {
	date_fields c = {-7, -7, -7};
	date_fields r = {-7, -7, -7};
	bool c_ok = c_tagdate_parse(text, &c);
	bool r_ok = tagdate_parse(text, &r);

	checks++;
	if (c_ok != r_ok || c.year != r.year || c.month != r.month || c.day != r.day) {
		char detail[260];
		snprintf(detail, sizeof detail,
		         "\"%s\": C %d %d-%d-%d, Rust %d %d-%d-%d", text, (int)c_ok, c.year, c.month,
		         c.day, (int)r_ok, r.year, r.month, r.day);
		fail("tagdate_parse", detail);
		return;
	}

	/* Only a date that was really read has a moment; the failure case is
	 * compared too, and both must answer zero for it. */
	time_t c_time = c_tagdate_to_time(&c);
	time_t r_time = tagdate_to_time(&r);
	checks++;
	if (c_time != r_time) {
		char detail[260];
		snprintf(detail, sizeof detail, "\"%s\": C %lld, Rust %lld", text, (long long)c_time,
		         (long long)r_time);
		fail("tagdate_to_time", detail);
	}
}

static void test_tagdate(void) {
	static const char *corpus[] = {
	    "",
	    " ",
	    "\t\t",
	    "n/a",
	    "2026",
	    "2026-03",
	    "2026-03-12",
	    "2026/03/12",
	    "20260312",
	    "2026-3-2",
	    "2026/3/12",
	    "2026-03-2",
	    "2026-3-12",
	    "2026-03-12T08:00:00",
	    "2026-03-12 08:00",
	    "2026-03-12T08:00:00Z",
	    "2026-03-12T08:00:00+02:00",
	    "2026-03-12/2026-03-13",
	    "  2026-03-12",
	    "\t20260312",
	    "0000",
	    "0000-01-01",
	    "9999",
	    "9999-12-31",
	    "10000-01-01",
	    "2026-13",
	    "2026-00",
	    "2026-00-10",
	    "2026-02-99",
	    "2026-02-31",
	    "2026-02-00",
	    "2026-03-32",
	    "2026-",
	    "2026-x",
	    "2026-03x",
	    "2026/1",
	    "202603121",
	    "2026031",
	    "202603",
	    "12/03/2026",
	    "-2026-03-12",
	    "2026--03",
	    "2026-03-12abc",
	    "1970-01-01",
	    "2037-12-31",
	    "1900-01-01",
	    "0001-01-01",
	};

	for (size_t i = 0; i < sizeof corpus / sizeof corpus[0]; i++) {
		compare_tagdate(corpus[i]);
	}

	/* Every pair, including the nulls: the tie rule is where a sort would go
	 * wrong silently. */
	date_fields dates[sizeof corpus / sizeof corpus[0]];
	bool have[sizeof corpus / sizeof corpus[0]];
	for (size_t i = 0; i < sizeof corpus / sizeof corpus[0]; i++) {
		have[i] = c_tagdate_parse(corpus[i], &dates[i]);
	}
	for (size_t i = 0; i < sizeof corpus / sizeof corpus[0]; i++) {
		for (size_t j = 0; j < sizeof corpus / sizeof corpus[0]; j++) {
			bool c = c_tagdate_better(have[i] ? &dates[i] : NULL, have[j] ? &dates[j] : NULL);
			bool r = tagdate_better(have[i] ? &dates[i] : NULL, have[j] ? &dates[j] : NULL);
			checks++;
			if (c != r) {
				fail("tagdate_better", "the fuller date did not win the same way on both sides");
			}
		}
	}

	/* Random shapes, built from the parts a tagger writes. */
	static const char *seps[] = {"-", "/", "", "--", "."};
	for (int round = 0; round < 2000; round++) {
		char text[64];
		int year = (int)rnd_below(12000);
		int month = (int)rnd_below(20);
		int day = (int)rnd_below(40);
		const char *s1 = seps[rnd_below(5)];
		const char *s2 = seps[rnd_below(5)];
		int form = (int)rnd_below(4);

		if (form == 0) {
			snprintf(text, sizeof text, "%04d%s%02d%s%02d", year, s1, month, s2, day);
		} else if (form == 1) {
			snprintf(text, sizeof text, "%04d%s%d%s%dT08:00:00", year, s1, month, s2, day);
		} else if (form == 2) {
			snprintf(text, sizeof text, "%04d%02d%02d", year, month, day);
		} else {
			snprintf(text, sizeof text, "%04d%s%02d", year, s1, month);
		}
		compare_tagdate(text);
	}
}

/* ------------------------------------------------------------------------ */
/* id3chap                                                                  */
/* ------------------------------------------------------------------------ */

static void syncsafe_into(uint32_t v, unsigned char *out) {
	out[0] = (unsigned char)((v >> 21) & 0x7F);
	out[1] = (unsigned char)((v >> 14) & 0x7F);
	out[2] = (unsigned char)((v >> 7) & 0x7F);
	out[3] = (unsigned char)(v & 0x7F);
}

static void be32_into(uint32_t v, unsigned char *out) {
	out[0] = (unsigned char)(v >> 24);
	out[1] = (unsigned char)(v >> 16);
	out[2] = (unsigned char)(v >> 8);
	out[3] = (unsigned char)v;
}

static size_t put_frame(unsigned char *buf, bool syncsafe_frames, const char *id,
                        const unsigned char *payload, size_t payload_len) {
	memcpy(buf, id, 4);
	if (syncsafe_frames) {
		syncsafe_into((uint32_t)payload_len, buf + 4);
	} else {
		be32_into((uint32_t)payload_len, buf + 4);
	}
	buf[8] = 0;
	buf[9] = 0;
	memcpy(buf + 10, payload, payload_len);
	return 10 + payload_len;
}

/* A CHAP payload: element id, four 32-bit fields, then a TIT2 sub-frame. */
static size_t build_chap(unsigned char *out, bool syncsafe_frames, int index, uint32_t start_ms,
                         const char *title) {
	unsigned char payload[1024];
	size_t p = 0;

	p += (size_t)snprintf((char *)payload, sizeof payload, "ch%03d", index) + 1; /* the NUL */
	be32_into(start_ms, payload + p);
	p += 4;
	be32_into(start_ms + 60000, payload + p);
	p += 4;
	be32_into(0xFFFFFFFFu, payload + p);
	p += 4;
	be32_into(0xFFFFFFFFu, payload + p);
	p += 4;

	unsigned char text[256];
	size_t n = strlen(title);
	if (n > sizeof text - 2) {
		n = sizeof text - 2;
	}
	text[0] = 0; /* encoding 0 */
	memcpy(text + 1, title, n);
	p += put_frame(payload + p, syncsafe_frames, "TIT2", text, n + 1);

	return put_frame(out, syncsafe_frames, "CHAP", payload, p);
}

/* Returns the tag length. `extended` adds a header, `padding` zeroes after the
 * frames (which is how a tag written once and edited later looks). */
static size_t build_id3_tag(unsigned char *out, size_t cap, int version, int chapters, bool extended,
                            bool padding) {
	unsigned char frames[16384];
	size_t f = 0;
	bool syncsafe_frames = version >= 4;

	/* A frame that is not a chapter, to make sure it is skipped by size. */
	{
		unsigned char text[8] = {0, 'A', 'l', 'b', 'u', 'm', 0, 0};
		f += put_frame(frames + f, syncsafe_frames, "TIT2", text, 6);
	}

	for (int i = 0; i < chapters; i++) {
		char title[300];
		if (i == 0) {
			snprintf(title, sizeof title, "Opening");
		} else if (i == 1) {
			/* Longer than the 128-byte field, so the truncation is compared. */
			snprintf(title, sizeof title,
			         "A Very Long Chapter Title That Will Not Fit In One Hundred And Twenty Eight "
			         "Bytes Because It Keeps Going On And On And On And On And On");
		} else if (i == 2) {
			snprintf(title, sizeof title, "Trailing space   ");
		} else if (i == 3) {
			snprintf(title, sizeof title, "");
		} else {
			snprintf(title, sizeof title, "Chapter %d", i + 1);
		}

		unsigned char chap[1600];
		size_t cl = build_chap(chap, syncsafe_frames, i, (uint32_t)i * 60000u, title);
		if (f + cl + 10 > sizeof frames) {
			break;
		}
		f += put_frame(frames + f, syncsafe_frames, "CHAP", chap, cl);
	}

	if (padding) {
		memset(frames + f, 0, 32);
		f += 32;
	}

	unsigned char ext[16];
	size_t ext_len = 0;
	if (extended) {
		if (version >= 4) {
			syncsafe_into(6, ext);
			memset(ext + 4, 0, 2);
			ext_len = 6;
		} else {
			be32_into(6, ext);
			memset(ext + 4, 0, 6);
			ext_len = 10;
		}
	}

	size_t at = 0;
	memcpy(out, "ID3", 3);
	at = 3;
	out[at++] = (unsigned char)version;
	out[at++] = 0;
	out[at++] = extended ? 0x40 : 0;
	syncsafe_into((uint32_t)(ext_len + f), out + at);
	at += 4;
	if (ext_len) {
		memcpy(out + at, ext, ext_len);
		at += ext_len;
	}
	memcpy(out + at, frames, f);
	at += f;

	return at < cap ? at : cap;
}

static void write_bytes(const char *path, const unsigned char *bytes, size_t len) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		fail("id3chap", "could not write the test file");
		return;
	}
	fwrite(bytes, 1, len, f);
	fclose(f);
}

static void compare_id3chap(const char *path, int max, const char *label) {
	id3chap_fields c[16];
	id3chap_fields r[16];
	memset(c, 0x5A, sizeof c);
	memset(r, 0x5A, sizeof r);

	int cc = c_id3chap_read(path, c, max);
	int rc = id3chap_read(path, r, max);

	checks++;
	if (cc != rc) {
		char detail[200];
		snprintf(detail, sizeof detail, "%s: C found %d, Rust found %d", label, cc, rc);
		fail("id3chap_read", detail);
	} else {
		int shown_here = cc < max ? cc : max;
		for (int i = 0; i < shown_here && i < 16; i++) {
			checks++;
			if (c[i].start != r[i].start || strcmp(c[i].title, r[i].title) != 0) {
				char detail[320];
				snprintf(detail, sizeof detail, "%s chapter %d: C %.3f \"%s\", Rust %.3f \"%s\"",
				         label, i, c[i].start, c[i].title, r[i].start, r[i].title);
				fail("id3chap_read", detail);
			}
		}
	}

	checks++;
	if (c_id3chap_present(path) != id3chap_present(path)) {
		char detail[200];
		snprintf(detail, sizeof detail, "%s: the two disagree on whether chapters are present", label);
		fail("id3chap_present", detail);
	}
}

static void test_id3chap(const char *workdir) {
	unsigned char tag[16384];
	char path[600];
	snprintf(path, sizeof path, "%s/diff_id3chap.mp3", workdir);

	for (int version = 3; version <= 4; version++) {
		for (int chapters = 0; chapters <= 6; chapters++) {
			for (int variant = 0; variant < 4; variant++) {
				bool extended = (variant & 1) != 0;
				bool padding = (variant & 2) != 0;
				size_t len = build_id3_tag(tag, sizeof tag, version, chapters, extended, padding);
				write_bytes(path, tag, len);

				char label[96];
				snprintf(label, sizeof label, "v2.%d, %d chapters, ext=%d pad=%d", version, chapters,
				         (int)extended, (int)padding);

				/* Every limit matters: a caller asking for fewer than there are
				 * must still be told the real count. */
				static const int limits[] = {0, 1, 2, 4, 8, 16};
				for (size_t i = 0; i < sizeof limits / sizeof limits[0]; i++) {
					compare_id3chap(path, limits[i], label);
				}
			}
		}
	}

	/* Truncations of a valid tag: the reader must stop at the same place on
	 * both sides rather than reading past what it got. */
	{
		size_t full = build_id3_tag(tag, sizeof tag, 4, 4, false, true);
		for (size_t cut = 0; cut <= full; cut += 7) {
			write_bytes(path, tag, cut);
			compare_id3chap(path, 8, "truncated tag");
		}
	}

	/* Corrupted frame sizes, which is what a half-written file looks like. */
	for (int round = 0; round < 300; round++) {
		size_t full = build_id3_tag(tag, sizeof tag, 3 + (int)rnd_below(2), 4, (rnd() & 1) != 0, true);
		/* Corrupt one byte somewhere in the frames, skipping the tag header. */
		size_t at = 10 + rnd_below((uint32_t)(full > 10 ? full - 10 : 1));
		if (at < full) {
			tag[at] = (unsigned char)rnd();
		}
		write_bytes(path, tag, full);
		compare_id3chap(path, 8, "corrupted tag");
	}

	/* Not a tag at all, and a tag that says it is enormous. */
	{
		static const unsigned char junk[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
		write_bytes(path, junk, sizeof junk);
		compare_id3chap(path, 8, "not an ID3 file");

		unsigned char huge[10] = {'I', 'D', '3', 4, 0, 0, 0x7F, 0x7F, 0x7F, 0x7F};
		write_bytes(path, huge, sizeof huge);
		compare_id3chap(path, 8, "a tag bigger than MAX_TAG");

		write_bytes(path, tag, 0);
		compare_id3chap(path, 8, "an empty file");
	}

	remove(path);

	/* A missing file. */
	snprintf(path, sizeof path, "%s/diff_id3chap_missing.mp3", workdir);
	compare_id3chap(path, 8, "a missing file");
}

/* ------------------------------------------------------------------------ */
/* addons                                                                   */
/* ------------------------------------------------------------------------ */

static void compare_addons_state(const char *label) {
	int n = c_addons_count();
	for (int i = 0; i < n; i++) {
		const addon_row *row = c_addons_at(i);
		checks++;
		if (c_addons_enabled(row->id) != addons_enabled(row->id)) {
			char detail[200];
			snprintf(detail, sizeof detail, "%s: the switch for \"%s\" differs", label, row->id);
			fail("addons_enabled", detail);
		}
	}
	checks++;
	if (c_addons_any_game_enabled() != addons_any_game_enabled()) {
		char detail[200];
		snprintf(detail, sizeof detail, "%s: the two disagree on whether a game is on", label);
		fail("addons_any_game_enabled", detail);
	}
}

/* Deliberately not the compiled-in default rule: the point is that the caller's
 * callback is what decides. */
static bool addon_l_or_eq(const char *id) {
	return id[0] == 'l' || strstr(id, "eq") != NULL;
}

/* The C settings writer opens the file with fopen(..., "w"), which the Windows
 * CRT turns into CRLF; the Rust one writes the bytes it means. That is a text
 * mode artefact of the C and not behaviour to copy — the device has no text
 * mode — so line endings are ignored here and everything else is compared. On a
 * POSIX host there are no CRs and this is strcmp. */
static bool same_ignoring_cr(const char *a, const char *b) {
	for (;;) {
		while (*a == '\r') {
			a++;
		}
		while (*b == '\r') {
			b++;
		}
		if (*a != *b) {
			return false;
		}
		if (*a == '\0') {
			return true;
		}
		a++;
		b++;
	}
}

static void compare_config_files(const char *c_path, const char *r_path, const char *label) {
	char *cf = read_file(c_path);
	char *rf = read_file(r_path);
	checks++;
	if (!cf || !rf) {
		fail(label, "a side wrote no settings file");
	} else if (!same_ignoring_cr(cf, rf)) {
		char detail[400];
		snprintf(detail, sizeof detail, "C:\n%s\nRust:\n%s", cf, rf);
		fail(label, detail);
	}
	free(cf);
	free(rf);
}

static void test_addons(const char *workdir) {
	char c_path[600];
	char r_path[600];
	char tmp[600];
	snprintf(c_path, sizeof c_path, "%s/diff_addons_c.ini", workdir);
	snprintf(r_path, sizeof r_path, "%s/diff_addons_r.ini", workdir);

	/* The list itself, row by row, including the order the page depends on. */
	checks++;
	if (c_addons_count() != addons_count()) {
		char detail[120];
		snprintf(detail, sizeof detail, "C has %d rows, Rust has %d", c_addons_count(), addons_count());
		fail("addons_count", detail);
	}
	for (int i = 0; i < c_addons_count(); i++) {
		const addon_row *c = c_addons_at(i);
		const addon_row *r = addons_at(i);
		checks++;
		if (!c || !r) {
			fail("addons_at", "one side returned nothing for a valid index");
			continue;
		}
		checks++;
		if (strcmp(c->id, r->id) != 0 || strcmp(c->label, r->label) != 0 ||
		    strcmp(c->note, r->note) != 0 || strcmp(c->section, r->section) != 0) {
			char detail[400];
			snprintf(detail, sizeof detail, "row %d: C {%s, %s, %s, %s} vs Rust {%s, %s, %s, %s}", i, c->id,
			         c->label, c->note, c->section, r->id, r->label, r->note, r->section);
			fail("addons_at", detail);
		}
	}
	checks++;
	if ((c_addons_at(-1) == NULL) != (addons_at(-1) == NULL) ||
	    (c_addons_at(c_addons_count()) == NULL) != (addons_at(addons_count()) == NULL)) {
		fail("addons_at", "out-of-range indices are handled differently");
	}

	/* find, by exact id only. */
	{
		static const char *ids[] = {"tidal", "", "nope", "TIDAL", "Tida", "gearboy",
		                            "pebble_deluxe", "layout_poster", "addons", "tidal "};
		for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
			const addon_row *c = c_addons_find(ids[i]);
			const addon_row *r = addons_find(ids[i]);
			checks++;
			if ((c == NULL) != (r == NULL) || (c && strcmp(c->id, r->id) != 0)) {
				char detail[200];
				snprintf(detail, sizeof detail, "\"%s\": C %s, Rust %s", ids[i], c ? c->id : "none",
				         r ? r->id : "none");
				fail("addons_find", detail);
			}
		}
		checks++;
		if (c_addons_find(NULL) != NULL || addons_find(NULL) != NULL) {
			fail("addons_find", "a null id found something");
		}
	}

	/* The defaults, with no settings file at all. */
	remove(c_path);
	remove(r_path);
	c_config_init(c_path);
	config_init(r_path);
	compare_addons_state("fresh install");

	/* One set per fresh file: the C config save cannot replace an existing file
	 * on Windows, and this test is about the switches, not about rename(). */
	{
		static const char *set_ids[] = {"tidal", "eq", "gearboy", "notes"};
		for (size_t i = 0; i < sizeof set_ids / sizeof set_ids[0]; i++) {
			for (int on = 0; on <= 1; on++) {
				remove(c_path);
				remove(r_path);
				c_config_init(c_path);
				config_init(r_path);

				bool c_ok = c_addons_set(set_ids[i], on != 0);
				bool r_ok = addons_set(set_ids[i], on != 0);
				checks++;
				if (c_ok != r_ok) {
					char detail[200];
					snprintf(detail, sizeof detail, "setting \"%s\" to %d: C %d, Rust %d", set_ids[i], on,
					         (int)c_ok, (int)r_ok);
					fail("addons_set", detail);
				}
				checks++;
				if (c_addons_enabled(set_ids[i]) != addons_enabled(set_ids[i])) {
					fail("addons_set", "the two disagree on the state after setting it");
				}
				compare_config_files(c_path, r_path, "addons settings file");
			}
		}

		/* An id that is not a module is refused, and nothing is written. */
		remove(c_path);
		remove(r_path);
		c_config_init(c_path);
		config_init(r_path);
		checks++;
		if (c_addons_set("nope", true) != addons_set("nope", true)) {
			fail("addons_set", "the two disagree on an unknown id");
		}
		checks++;
		if (c_addons_set(NULL, true) != addons_set(NULL, true)) {
			fail("addons_set", "the two disagree on a null id");
		}
	}

	/* Defaults applied through a callback of the caller's own. */
	remove(c_path);
	remove(r_path);
	c_config_init(c_path);
	config_init(r_path);
	c_addons_apply_defaults(addon_l_or_eq);
	addons_apply_defaults(addon_l_or_eq);
	compare_addons_state("applied defaults");
	compare_config_files(c_path, r_path, "applied defaults file");

	remove(c_path);
	remove(r_path);
	snprintf(tmp, sizeof tmp, "%s.tmp", c_path);
	remove(tmp);
	snprintf(tmp, sizeof tmp, "%s.tmp", r_path);
	remove(tmp);
}

int main(int argc, char **argv) {
	const char *workdir = argc > 1 ? argv[1] : ".";

	test_base64();
	test_hashes();
	test_json();
	test_config(workdir);
	test_utils();
	test_textfile(workdir);
	test_tagdate();
	test_id3chap(workdir);
	test_addons(workdir);

	printf("\n%d checks, %d failure(s)\n", checks, failures);
	return failures ? 1 : 0;
}
