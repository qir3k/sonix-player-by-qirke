#!/usr/bin/env bash
# Wrapper forwarding to sonix_firmware_packer.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
exec "$SCRIPT_DIR/sonix_firmware_packer.sh" "$@"
