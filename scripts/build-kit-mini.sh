#!/usr/bin/env bash
set -euo pipefail

KIT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
KIT_JOBS=${NPROC:-6}
KIT_BUILD_TAG=${BUILD_TAG:-808p03}
if [[ ! "$KIT_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo 'NPROC must be a positive integer.' >&2
    exit 1
fi
if [[ -n ${TOOLCHAIN:-} ]]; then
    KIT_TOOLCHAIN_PREFIX=$TOOLCHAIN
elif [[ -x "$KIT_ROOT/agt/10.3/bin/arm-none-eabi-gcc" ]]; then
    KIT_TOOLCHAIN_PREFIX="$KIT_ROOT/agt/10.3/bin/arm-none-eabi-"
else
    KIT_TOOLCHAIN_PREFIX=arm-none-eabi-
fi
if ! KIT_COMPILER_VERSION=$("${KIT_TOOLCHAIN_PREFIX}gcc" --version); then
    echo 'Compiler missing. Run: bash scripts/bootstrap-toolchain.sh' >&2
    exit 1
fi
case "$KIT_COMPILER_VERSION" in
    *10.3-2021.10*) ;;
    *) echo 'Use the pinned GNU Arm Embedded 10.3-2021.10 toolchain.' >&2; exit 1 ;;
esac

make -C "$KIT_ROOT" mini NPROC="$KIT_JOBS" \
    TOOLCHAIN="$KIT_TOOLCHAIN_PREFIX" BUILD_TAG="$KIT_BUILD_TAG"
python3 "$KIT_ROOT/tools/drum-kit/package_release.py" \
    --firmware "$KIT_ROOT/build/TrackerMini_CE_1.0.0.ptf" \
    --out "$KIT_ROOT/dist" --build-tag "$KIT_BUILD_TAG"
echo 'Mini firmware and ZIP are ready in dist/. No device has been flashed.'
