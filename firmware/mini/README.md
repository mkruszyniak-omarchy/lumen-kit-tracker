# Tracker Mini firmware · Lumen KIT 0.1

**File:** [`TrackerMini_KIT_0.1.0.ptf`](TrackerMini_KIT_0.1.0.ptf)

- Hardware: **Polyend Tracker Mini only**.
- KIT release: **0.1.0**, experimental proof of concept.
- Firmware base: **Tracker CE 1.0.0**.
- Build shown on the device: **808p03**.
- Basic operation confirmed on a Mini by **KRUChY**.
- Authors of the KIT work: **KRUChY & Lumen**; original firmware by Polyend and CE contributors.

This is the exact validated image previously named `TrackerMini_808_p03.ptf`.
Only the filename has changed. SHA256:

```text
b44305f42db132b1c8c1476e7582a51f531fdb0a8b30f3d03b17a2014c43b074
```

From the repository root:

```bash
sha256sum -c firmware/mini/SHA256SUMS
```

Back up the card, copy the file to `/Firmware/`, then use
**Config → Firmware → Firmware update**. After updating, select
**SYN 1 → KIT → Edit Patch → Init**, with **1 voice**.

The full [LICENSE](../../LICENSE), [NOTICE.md](../../NOTICE.md), and
[PolySynth/PolyDSP binary license](../../synth/lib/LICENSE-PROPRIETARY.txt) apply.
This is not an official Polyend release. Further hardware testing is still needed.
