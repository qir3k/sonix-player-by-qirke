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
 * Built and run by run.sh. Covers base64, md5, sha1, sha256, json and config.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv) {
	const char *workdir = argc > 1 ? argv[1] : ".";

	test_base64();
	test_hashes();
	test_json();
	test_config(workdir);

	printf("\n%d checks, %d failure(s)\n", checks, failures);
	return failures ? 1 : 0;
}
