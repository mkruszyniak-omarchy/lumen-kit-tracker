#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build/drum-kit-808
"${CXX:-g++}" -std=c++20 -O2 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DFEATURE_SYNTH -DPRODUCT=1 -DWAVE=1 -DFMT_HEADER_ONLY \
    -include cstdint -include array -include string \
    -Icore/config -Isynth/include -Iutil/include -isystem util/3rdparty \
    tools/drum-kit/render_test.cpp -o build/drum-kit-808/verify
# LeakSanitizer cannot enumerate threads under the Codex ptrace sandbox.
# AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build/drum-kit-808/verify build/drum-kit-808
