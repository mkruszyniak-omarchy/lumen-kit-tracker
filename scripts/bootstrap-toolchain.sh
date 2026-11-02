#!/usr/bin/env bash
set -euo pipefail

KIT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
KIT_TOOLCHAIN_DIR="$KIT_ROOT/agt/10.3"
KIT_ARCHIVE_NAME=gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
KIT_ARCHIVE_SHA=97dbb4f019ad1650b732faffcc881689cedc14e2b7ee863d390e0a41ef16c9a3
KIT_ARCHIVE_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/$KIT_ARCHIVE_NAME"

if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
    echo 'This bootstrap supports Linux x86_64. See BUILDING.md for other hosts.' >&2
    exit 1
fi

if [[ -x "$KIT_TOOLCHAIN_DIR/bin/arm-none-eabi-gcc" ]]; then
    KIT_COMPILER_VERSION=$("$KIT_TOOLCHAIN_DIR/bin/arm-none-eabi-gcc" --version)
    case "$KIT_COMPILER_VERSION" in
        *10.3-2021.10*) echo 'Pinned GNU Arm toolchain is already installed.'; exit 0 ;;
        *) echo 'Existing agt/10.3 compiler is not the pinned version; leaving it untouched.' >&2; exit 1 ;;
    esac
fi
if [[ -e "$KIT_TOOLCHAIN_DIR" ]]; then
    echo 'agt/10.3 exists but is incomplete; inspect it before retrying.' >&2
    exit 1
fi

mkdir -p "$KIT_ROOT/agt/cache"
KIT_ARCHIVE="$KIT_ROOT/agt/cache/$KIT_ARCHIVE_NAME"
if [[ ! -f "$KIT_ARCHIVE" ]]; then
    curl --fail --location --retry 3 "$KIT_ARCHIVE_URL" -o "$KIT_ARCHIVE.downloading"
    mv "$KIT_ARCHIVE.downloading" "$KIT_ARCHIVE"
fi
printf '%s  %s\n' "$KIT_ARCHIVE_SHA" "$KIT_ARCHIVE" | sha256sum --check --status

KIT_TOOLCHAIN_TMP=$(mktemp -d "$KIT_ROOT/agt/.toolchain-10.3.XXXXXX")
trap 'rm -rf "$KIT_TOOLCHAIN_TMP"' EXIT
tar -xjf "$KIT_ARCHIVE" -C "$KIT_TOOLCHAIN_TMP" --strip-components=1
KIT_COMPILER_VERSION=$("$KIT_TOOLCHAIN_TMP/bin/arm-none-eabi-gcc" --version)
case "$KIT_COMPILER_VERSION" in
    *10.3-2021.10*) ;;
    *) echo 'Unexpected compiler in archive.' >&2; exit 1 ;;
esac
mv "$KIT_TOOLCHAIN_TMP" "$KIT_TOOLCHAIN_DIR"
trap - EXIT
echo 'GNU Arm Embedded 10.3-2021.10 installed in agt/10.3.'
