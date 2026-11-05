# Convenience targets for the Rust port. The C firmware has its own Makefile one
# directory up and is not touched by anything here.

CARGO ?= cargo

.PHONY: all test differential check staticlib target fmt clippy clean

## Everything that can be checked without a device.
all: test differential

## Unit tests, against the vectors the C headers cite and against the behaviour
## documented in each module.
test:
	$(CARGO) test --workspace

## The differential test: the original C modules, symbols renamed, linked
## beside the Rust ones and fed identical inputs. Needs a C compiler; on the
## device build there is already one in the rockbox toolchain.
##
##     make differential CC=mipsel-rockbox-linux-gnu-gcc
differential: staticlib
	sh tests/differential/run.sh

## Compile without linking. Fast, and enough to know the crates still build.
check:
	$(CARGO) check --workspace

## The archive the firmware links against.
staticlib:
	$(CARGO) build --release -p sonix-ffi
	cp target/release/libsonix_player_core.a libsonix_player_core.a
	@echo "libsonix_player_core.a ready ($(shell wc -c < libsonix_player_core.a) bytes)"

## Cross build for the TempoTec Variations V1.
##
## mipsel-unknown-linux-gnu is a Tier 3 Rust target: rustup cannot install std
## for it and there is no prebuilt one, so this needs nightly and -Z build-std.
target:
	$(CARGO) +nightly build --release -p sonix-ffi \
		--target mipsel-unknown-linux-gnu \
		-Z build-std=std,panic_abort \
		-Z build-std-features=panic_immediate_abort
	cp target/mipsel-unknown-linux-gnu/release/libsonix_player_core.a .
	@echo "cross-built libsonix_player_core.a ready"

fmt:
	$(CARGO) fmt --all

clippy:
	$(CARGO) clippy --workspace --all-targets -- -D warnings

clean:
	$(CARGO) clean
	rm -rf tests/differential/build libsonix_player_core.a
