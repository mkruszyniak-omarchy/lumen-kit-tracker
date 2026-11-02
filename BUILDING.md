# Build Lumen KIT 0.1

The complete Tracker CE source tree and the original binary libraries are kept in
this fork. Build the **Mini** target to obtain firmware for a Tracker Mini.

## 1. Prerequisites

The documented, locally verified path is **Linux x86_64** with:

- `git`, `make`, `python3`, `curl`, `tar`, `bzip2`, and `sha256sum`;
- **GNU Arm Embedded Toolchain 10.3-2021.10**, matching upstream;
- optionally a C++20-capable host `g++` for the desktop DSP checks.

No global installation or administrator access is required by our scripts.

## 2. Clone and install the pinned compiler

```bash
git clone https://github.com/mkruszyniak-omarchy/lumen-kit-tracker.git
cd lumen-kit-tracker
bash scripts/bootstrap-toolchain.sh
agt/10.3/bin/arm-none-eabi-gcc --version
```

The bootstrap downloads the official Arm Linux x86_64 archive, verifies its
pinned SHA256, and extracts it to ignored directory `agt/10.3/`. It stops rather
than replacing an existing incompatible compiler. Other hosts can use the
[upstream toolchain links](https://developer.arm.com/downloads/-/gnu-rm), but the
bootstrap itself is intentionally Linux x86_64 only.

## 3. Build and package Mini firmware

```bash
bash scripts/build-kit-mini.sh
```

The default job count is 6. Override it with `NPROC=4` if needed.
An existing compiler can be supplied as a prefix:

```bash
TOOLCHAIN=/path/to/bin/arm-none-eabi- NPROC=4 bash scripts/build-kit-mini.sh
```

The script runs the normal upstream Mini build with `BUILD_TAG=808p03`, then
validates the Intel HEX records and Mini flash address range before packaging:

```text
dist/
├── TrackerMini_KIT_0.1.0.ptf
├── Lumen_KIT_0.1.0_Mini.zip
└── SHA256SUMS
```

The ZIP includes `Firmware/`, install notes, authorship, license notices and two
short audio demos. Nothing in these scripts writes to your microSD card or flashes
a device. Version `0.1.0` identifies KIT; the CE base version remains `1.0.0`.
A local source build is not asserted to be bit-identical on every host: paths,
compiler details and build environment can affect the output.

## 4. Equivalent manual commands

```bash
make mini NPROC=6 BUILD_TAG=808p03
python3 tools/drum-kit/package_release.py \
  --firmware build/TrackerMini_CE_1.0.0.ptf --out dist
(cd dist && sha256sum -c SHA256SUMS)
```

Do not rename Plus or Original firmware to look like Mini firmware. The packaging
validator checks the expected Mini link address but is not a substitute for
choosing the correct build target.

To build the other targets for compilation checks:

```bash
make all NPROC=6 BUILD_TAG=808p03
```

`make plus` can compile KIT for Tracker+, but that image has not been tested on
physical hardware here. `make original` builds the Original Tracker base without
KIT, because its platform does not enable this synth SDK.

## 5. Desktop DSP and routing checks

```bash
bash tools/drum-kit/verify.sh
```

Output is written to `build/drum-kit-808/`, including the verifier and WAV renders.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled. Leak detection is
disabled by default because the original development sandbox cannot run LSan's
thread enumeration. On a suitable host it can be enabled explicitly:

```bash
ASAN_OPTIONS=detect_leaks=1 bash tools/drum-kit/verify.sh
```

The actual KIT voice/DSP is compiled against public SDK headers. A small test Patch
object supplies storage. The proprietary ARM PolySynth allocator/serializer is not
linked into the desktop test, so successful tests do not prove all on-device behavior.

## 6. Install

Back up the card, copy the **Mini** `.ptf` to `/Firmware/`, and select it in
**Config → Firmware → Firmware update**. Then use a new test project and choose
**SYN 1 → KIT → Edit Patch → Init**, with **1 voice**. See the
[English quick start](README.md#install-on-tracker-mini) or
[Polish guide](README.pl.md#instalacja-na-tracker-mini).

The downloadable file in `firmware/mini/` is the exact image whose basic operation
KRUChY confirmed, with the filename changed for the 0.1.0 release. Build display:
**808p03**. Keep the upstream license notices with redistributed firmware.
