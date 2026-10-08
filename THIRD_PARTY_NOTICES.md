# Third-party notices / source provenance

The project-level MIT license applies to the original Zig Saber-derived work and newly authored port/tools. It does not override component licenses below.

## Original Zig Saber

- Author: **ABA Games**, copyright 2026.
- License: **MIT**, preserved verbatim at `upstream/LICENSE`.
- Repository: https://github.com/abagames/agentic-gamedev-games
- Directory revision: `7aad21b8dde2a1b40188eebf29474ce4727c8eb6`.
- Included: untouched `core.js`, `main.js`, `psg.js`; exported terrain/event/music data and derived tile artwork. The original gate-core blade correction is included.

## devkitSMS

- Author/project: **sverx / devkitSMS** and the authors credited in its source.
- Repository: https://github.com/sverx/devkitSMS
- Pinned commit: `b172c7433ff690136fee47504a5fd427f62f2d16`.
- SMSlib: **public domain / Unlicense**, `vendor/devkitsms/LICENSE-SMSlib` and source headers.
- ihx2sms converter: **public domain / Unlicense**, `vendor/devkitsms/LICENSE-ihx2sms` and source header.
- Startup `crt0_sms.s`: copyright 2000 Michael Hope, SMS modifications by sverx; **GPL-2.0-or-later with the SDCC linking special exception** reproduced in its header. Full GPL text is at `vendor/devkitsms/COPYING.txt`.
- `LICENSES.txt` retains the upstream component license inventory and contributor notes. Some unused original SMSlib modules have their own source-header attributions.
- The release builds required library objects with the same compiler as the port. It does not rely on an incompatible precompiled library, and does not use PSGlib.

## Z80 player CPU

- Project: **superzazu/z80**; copyright 2019 **Nicolas Allemand**.
- Repository: https://github.com/superzazu/z80
- License: **MIT**, preserved at `vendor/z80/LICENSE`.
- `z80.c` and `z80.h` are the untouched CPU files supplied in the pinned hardware-reference snapshot. File digests are recorded in the release manifest.
- Used by the optional browser / native verification player, **not linked into the SMS cartridge**. `tools/web_sms.c`, HTML UI and the Python controller script runner are separately authored for this port.

## Hardware guide

- Reference: **Haroldo-OK**, https://github.com/haroldo-ok/sega-master-system-skill-for-claude
- Snapshot: `2a40a072693600fb18cb24fbddecfc99cc7afe1a`.
- Informed the SDCC, Sega mapper, tile/sprite and headless testing workflow.
- The upstream SMS wrapper / script harness was used during development, but no explicit root license was found in that snapshot or its cited reference project. **That wrapper is not included in the distributed source archive.** No MIT license is asserted for it. The release uses our own controller runner and player, plus the separately MIT-licensed CPU above.

## Compiler / external verification tools

SDCC, GCC, Node, Python, Pillow, Clang, Playwright and Emulicious are external development / verification tools; their installations are not bundled. The CRT and linked SMSlib sources are included with their notices. SDCC’s runtime licensing uses its own linking exception; consult your installed toolchain for its notices. Emulicious screenshots are verification evidence, not a distribution of that emulator.

## Naming

Sega and Master System identify the intended hardware. This is unofficial homebrew, not a Sega product. No endorsement or affiliation with Sega or ABA Games is claimed.
