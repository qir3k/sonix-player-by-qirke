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
text="$app/src/system/text"
library="$app/src/system/library"
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
-Dconfig_store_save=c_config_store_save -Dconfig_store_move_section=c_config_store_move_section
-Dutils_isqrt32=c_utils_isqrt32 -Dutils_isqrt64=c_utils_isqrt64 -Dhas_extension=c_has_extension
-DformatDoubleSeconds=c_formatDoubleSeconds -DformatDoubleProgress=c_formatDoubleProgress
-Dformat_row_duration=c_format_row_duration
-Dtextfile_decode=c_textfile_decode -Dtextfile_read=c_textfile_read -Dtextfile_write=c_textfile_write
-Dtagdate_parse=c_tagdate_parse -Dtagdate_better=c_tagdate_better -Dtagdate_to_time=c_tagdate_to_time
-Did3chap_read=c_id3chap_read -Did3chap_present=c_id3chap_present
-Daddons_count=c_addons_count -Daddons_at=c_addons_at -Daddons_find=c_addons_find
-Daddons_enabled=c_addons_enabled -Daddons_set=c_addons_set
-Daddons_apply_defaults=c_addons_apply_defaults -Daddons_any_game_enabled=c_addons_any_game_enabled
-Dnotes_dir=c_notes_dir -Dnotes_path=c_notes_path -Dnotes_is_note_name=c_notes_is_note_name
-Dnotes_list=c_notes_list -Dnotes_read=c_notes_read -Dnotes_fits_editor=c_notes_fits_editor
-Dnotes_file_name=c_notes_file_name -Dnotes_write=c_notes_write -Dnotes_name_ok=c_notes_name_ok
-Dnotes_display_name=c_notes_display_name -Dnotes_default_name=c_notes_default_name"

# textfile.c flushes with POSIX `fsync`; the Windows CRT calls that `_commit`.
# It is the same call, so the comparison stays a fair one.
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) textfile_defs="-Dfsync=_commit" ;;
*) textfile_defs="" ;;
esac

# notesstore.c makes its folders with the two-argument mkdir() and asks for the
# date with localtime_r, which MinGW has neither of. Its part of the harness is
# left out on Windows rather than testing a hand-written stand-in.
notes_defs=""
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) ;;
*)
	notes_defs="-DHAVE_NOTESSTORE=1"
	# shellcheck disable=SC2086
	"$CC" -O1 -w -std=gnu99 -I"$app" $RENAMES \
		-c "$app/src/system/notes/notesstore.c" -o "$build/notesstore.o"
	;;
esac

echo "== compiling the C originals with renamed symbols"
for unit in base64 md5 sha1 sha256 json config; do
	# shellcheck disable=SC2086
	"$CC" -O1 -w -std=gnu99 -I"$core" $RENAMES \
		-c "$core/$unit.c" -o "$build/$unit.o"
done

# shellcheck disable=SC2086
"$CC" -O1 -w -std=gnu99 -I"$text" $RENAMES $textfile_defs \
	-c "$text/textfile.c" -o "$build/textfile.o"
"$CC" -O1 -w -std=gnu99 -I"$library" $RENAMES \
	-c "$library/tagdate.c" -o "$build/tagdate.o"
"$CC" -O1 -w -std=gnu99 -I"$app" -I"$library" $RENAMES \
	-c "$library/id3chap.c" -o "$build/id3chap.o"

# GB_CORE is what the device Makefile passes, and it decides whether the list of
# add-ons has a Gearboy row. The two sides have to be built with the same answer
# or the comparison is meaningless.
"$CC" -O1 -w -std=gnu99 -DGB_CORE=1 -I"$app" -I"$app/src/system/addons" $RENAMES \
	-c "$app/src/system/addons/addons.c" -o "$build/addons.o"

# utils.c is a mixed file: the pure half can be compiled here, the other half
# (pthreads, mmap, sched, prctl) cannot leave the device. Taking the prefix of
# the real file rather than a hand-made copy of these functions is what keeps
# this an honest comparison: there is no second copy to drift. The cut is the
# comment that opens the platform section, so moving code above it changes what
# is tested, and deleting that line makes this fail to compile loudly.
pure_utils="$build/utils_pure.c"
{
	echo "/* Generated by run.sh from src/system/core/utils.c: everything above the"
	echo " * platform section, verbatim. Do not edit; the edit goes in utils.c. */"
	awk '/^\/\/ See utils\.h\. SCHED_IDLE is per-thread/ { exit } { print }' "$core/utils.c" \
		| grep -v '^#include <sys/mman.h>'
} > "$pure_utils"
[ -s "$pure_utils" ] || { echo "run.sh: the utils.c extract came out empty" >&2; exit 2; }
"$CC" -O1 -w -std=gnu99 -I"$core" $RENAMES \
	-c "$pure_utils" -o "$build/utils.o"

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
# shellcheck disable=SC2086
"$CC" -O1 -w -std=gnu99 -I"$core" $notes_defs \
	"$here/harness.c" \
	"$build"/*.o \
	"$archive" \
	$sys_libs \
	-o "$build/differential"

echo "== running"
"$build/differential" "$build"
