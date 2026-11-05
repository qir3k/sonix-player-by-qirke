#!/bin/sh
# Differential test: the original C core against the Rust port.
#
# The six C files that have been ported are compiled with their exported
# symbols renamed (-Dmd5=c_md5 and so on), then linked beside the Rust archive,
# which exports the canonical names. One program then calls both and compares.
#
#     ./run.sh            # from this directory, or: make differential
#
# Exits non-zero on the first sign of a disagreement.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
rust=$(cd "$here/../.." && pwd)     # sonix-player/rust
app=$(cd "$rust/.." && pwd)         # sonix-player
core="$app/src/system/core"
build="$here/build"

CC=${CC:-gcc}
CARGO=${CARGO:-cargo}

if ! command -v "$CC" >/dev/null 2>&1; then
	echo "run.sh: no C compiler ($CC). Set CC= to one." >&2
	exit 2
fi

rm -rf "$build"
mkdir -p "$build"

echo "== building the Rust archive"
(cd "$rust" && "$CARGO" build --release -p sonix-ffi)
archive="$rust/target/release/libsonix_player_core.a"
[ -f "$archive" ] || { echo "run.sh: $archive is missing" >&2; exit 2; }

# Every name a ported module exports. Renaming the C side is what lets both be
# linked into one program.
RENAMES="-Dbase64_decode=c_base64_decode
-Dmd5=c_md5 -Dmd5_hex=c_md5_hex
-Dsha1=c_sha1 -Dsha1_hex=c_sha1_hex
-Dsha256_init=c_sha256_init -Dsha256_update=c_sha256_update -Dsha256_final_hex=c_sha256_final_hex
-Djson_parse=c_json_parse -Djson_parse_limited=c_json_parse_limited -Djson_free=c_json_free
-Djson_root=c_json_root -Djson_get=c_json_get -Djson_at=c_json_at -Djson_len=c_json_len
-Djson_path=c_json_path -Djson_str=c_json_str -Djson_str_exact=c_json_str_exact
-Djson_long=c_json_long -Djson_llong=c_json_llong -Djson_double=c_json_double
-Djson_bool=c_json_bool -Djson_obj_str=c_json_obj_str -Djson_obj_long=c_json_obj_long
-Djson_obj_llong=c_json_obj_llong -Djson_obj_bool=c_json_obj_bool
-Dconfig_init=c_config_init -Dconfig_get=c_config_get -Dconfig_get_int=c_config_get_int
-Dconfig_get_bool=c_config_get_bool -Dconfig_set=c_config_set -Dconfig_set_int=c_config_set_int
-Dconfig_set_bool=c_config_set_bool -Dconfig_save=c_config_save -Dconfig_path=c_config_path
-Dconfig_main_store=c_config_main_store -Dconfig_ebook_store=c_config_ebook_store
-Dconfig_store_init=c_config_store_init -Dconfig_store_path=c_config_store_path
-Dconfig_store_get=c_config_store_get -Dconfig_store_get_int=c_config_store_get_int
-Dconfig_store_set=c_config_store_set -Dconfig_store_set_int=c_config_store_set_int
-Dconfig_store_save=c_config_store_save -Dconfig_store_move_section=c_config_store_move_section"

echo "== compiling the C originals with renamed symbols"
for unit in base64 md5 sha1 sha256 json config; do
	# shellcheck disable=SC2086
	"$CC" -O1 -w -std=gnu99 -I"$core" $RENAMES \
		-c "$core/$unit.c" -o "$build/$unit.o"
done

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
	# What Rust's std needs from the Windows CRT when it is inside a staticlib.
	# lang=english keeps any further missing-symbol paragraph out of the way.
	sys_libs="-lws2_32 -lbcrypt -luserenv -ladvapi32 -lole32 -lntdll -lkernel32"
	;;
*)
	sys_libs="-lpthread -ldl -lm"
	;;
esac

echo "== linking the harness"
# shellcheck disable=SC2086
"$CC" -O1 -w -std=gnu99 -I"$core" \
	"$here/harness.c" \
	"$build"/*.o \
	"$archive" \
	$sys_libs \
	-o "$build/differential"

echo "== running"
"$build/differential" "$build"
