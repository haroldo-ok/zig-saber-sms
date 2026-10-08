# Zig Saber — Sega Master System

An unofficial, native Z80 homebrew port of [ABA Games’ Zig Saber](https://github.com/abagames/agentic-gamedev-games/tree/main/docs/zig-saber). **A real 64 KiB `.sms` cartridge**, not a web game dressed up as an emulator.

**Release:** 1.0 · **Target:** NTSC Sega Master System · **ROM:** 64 KiB, standard Sega mapper.

ROM SHA-256: `2a3e6d233d785481fc8b5f8afcb8bb5a70e72e80b4f3f8a839beb5a29eaf034a`.

## Downloads and source archive

- [Native Master System ROM](zig-saber.sms)
- [Complete source ZIP](zig-saber-sms-source.zip)
- [Self-contained offline ROM player](PLAY.html)

The source ZIP extracts into `zig-saber-sms/` and includes the C sources, Makefile, asset generators and required prebuilt assets, pinned original JavaScript, matched hardware-library sources, tests, verification evidence and component licenses. The ready-to-play ROM and HTML are included for convenience. Temporary build objects, caches, emulator installations and the unlicensed upstream test wrapper are excluded.

### Quick start from the ZIP

With SDCC, GCC, GNU make and Python 3 installed:

```sh
unzip zig-saber-sms-source.zip
cd zig-saber-sms
make -j2       # Build the native cartridge
make player    # Generate PLAY.html using the included WASM
make test      # Run host-rule and actual-ROM checks
```

The verified compiler is **SDCC 4.5.0**. Node.js enables the original-JavaScript comparisons; Pillow enables screenshot export. Neither Clang nor Playwright is required for an ordinary cartridge build. See the detailed build and optional-test instructions below.

## Play

- **Native cartridge:** open `zig-saber.sms` in a Master System emulator, with the console set to **NTSC / 60 Hz**. The cartridge uses the standard Sega mapper. An emulator / cartridge with Sega-mapper SRAM can retain BEST 5.
- **Offline browser:** open `PLAY.html` in a modern browser. ROM, WebAssembly emulator, graphics and scripts are all embedded; no installation, CDN or server is required. Press **Space** or click/tap **START / TURN + FIRE**. Sound starts after your first gesture. `P` pauses, `M` toggles sound. The player also has touch, reset and expand controls.
- Optional local web server: `python3 tools/serve_player.py`, then open `http://localhost:8080`. This can help browsers that restrict local-file storage.

The lightweight browser player is a convenience, not a general-purpose accuracy reference. For normal native play, use the cartridge in an SMS emulator.

### Controls

| Master System | Offline player | Action |
|---|---|---|
| Controller 1: button **1 or 2** | **Space, Z, X, Enter**, on-screen button or canvas tap | Start; turn and attack |
| Console **PAUSE** button | **P** or PAUSE control | Pause / resume |
| Action while paused | Action key / tap | Resume without attacking |

Up / Down on controller 1 are also action aliases, **not direct steering**. Holding any action does not repeat. Left / Right do nothing. Configure your emulator’s keyboard bindings as needed.

### The important rule

Every press reverses your vertical motion and attacks in the **previous** direction. A close blade cut returns your energy immediately. A miss sends out your **one reusable wave**; while that wave is out, another press only turns you.

- Wait for enemies to reach the blade. Close cuts, multi-cuts and chains pay much more than distant shots.
- Destroy an entire red train to receive a laser capsule. Laser extends blade reach; a sealed-lane capsule gives maximum laser and chain. Gems refill the laser clock.
- Blue armour stops the wave. Cut it with the blade. Enemy contact burns all laser levels for a brief guard; **rock is fatal even with laser / guard**.
- At the gate, cut a thrown plate, then send a wave through the open row. **The blade itself never damages the gate core.**

Three fixed zones — **Canyon, Cavern, Fortress** — lead to the gate. Complete the first course to enter the harder second loop. Destroy the second gate for ALL CLEAR and the remaining-ship tally. The gate can time out and retreat; that is not a boss kill.

## Included

| File / directory | Contents |
|---|---|
| `zig-saber.sms` | Native 65,536-byte cartridge |
| `PLAY.html` | Self-contained offline ROM player |
| `src/` | Gameplay, SMS rendering, cabinet flow, input, SRAM and PSG audio |
| `assets/` | Prebuilt tile/course banks, original export and player WASM |
| `upstream/` | Pinned, untouched original JavaScript and MIT license |
| `vendor/devkitsms/` | Matched SMSlib / CRT / converter sources and licenses |
| `vendor/z80/` | MIT-licensed CPU core used only for the test / browser player |
| `tools/` | Asset exporter/generator, ROM packer, player and packaging tools |
| `tests/` | Shared-core rules, original-rule comparisons, cartridge and browser tests |
| `evidence/` | Actual test logs, fixture screenshots and verification reports |

## Build the cartridge

Tested with **SDCC 4.5.0**, GNU make, GCC and Python 3 on Linux. No network downloads or third-party game ROMs are needed to rebuild. SMSlib is rebuilt with the same SDCC as the game, avoiding precompiled-library ABI mismatches.

```sh
# Install the tools using your system's package manager, for example:
sudo apt-get install sdcc build-essential python3

# In the extracted source directory:
make -j2
# Output: zig-saber.sms (+ .noi symbols and .map)
```

Other SDCC versions can generate different code and are not part of the verified configuration. The build checks its code/RAM limits, linker diagnostics, cartridge size, Sega header and final 64 KiB checksum. Startup assembly and required SMSlib modules are assembled/compiled from their included sources.

### Reproducibility

The extracted source archive was clean-built and tested: the ROM is **byte-identical**, and optional asset regeneration reproduces the tile/course banks and generated C headers. See `evidence/rebuild-verification.json`.

The distribution’s tile/course banks are included. With the same SDCC version, these files and the provided Makefile, a clean rebuild reproduces the cartridge. The packer fixes the SDSC release date to **2026-10-07**, instead of inheriting the converter’s wall-clock date. Set `SOURCE_DATE_EPOCH` to intentionally use a different release date.

```sh
make clean
make -j2
make test
make player
```

`make player` embeds the newly built ROM and the included, precompiled WASM. **Clang is not needed for ordinary ROM or HTML builds.**

### Repackage the sources

After changing the project, rebuild/test the cartridge and regenerate the player before packaging:

```sh
make -j2 test player
python3 tools/package.py
```

This writes `zig-saber-sms-source.zip` and refreshes `RELEASE.json`, which records the cartridge checksum and SHA-256 digests of the packaged files. The archive uses a fixed release timestamp and an explicit file selection; it does not collect arbitrary files from your working directory.

### Optional regeneration

```sh
# Regenerate graphics, terrain, scripted waves and the C audio tables:
# Requires Node.js and Python Pillow in addition to the normal tools.
sudo apt-get install nodejs python3-pil
make assets
make -j2

# Rebuild the optional emulator WASM (tested with clang/lld 19):
make wasm CLANG=clang-19
make player
```

Optional browser regression checks require Playwright and Chromium (not needed to build or play):

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install playwright
python3 -m playwright install --with-deps chromium
make player
python3 tests/browser_test.py
```

No boot BIOS is distributed or required. The cartridge starts through its included CRT.

## What was preserved / adapted

- One-button turn-and-attack, one wave, immediate return on cuts, directional blade, rock clipping and wave-through-rock.
- Laser levels/timers, multi-cut and near bonuses, X1–X5 chain, chain heat, carrier capsules, sealed lanes, gems, extends, deaths and checkpoint rewind.
- Fixed authored course geometry and two event scripts; the 24-hit, five-row gate, opening clock, overdrive, retreat, both loop transitions and final tally.
- Title, actual gameplay attract mode, 50 callsigns and BEST 5, with a checksummed SRAM record.
- Original PSG composition data revoiced for the SMS’s **one SN76489**: lead, bass, priority effects and percussion/noise. The second-chip arpeggio / secondary jingle voice are omitted.

The original 256×224 presentation is adapted to **256×192**: a 16-pixel HUD and 22 terrain rows. Vertical positions are mapped into the shorter field; movement uses fixed point and enemies use bounded, dense pools. Graphics are tile/sprite artwork with the SMS palette, 8×16 sprites and rotated enemy draw priority. Some particles, trails and UI decoration are simplified. It is a mechanic-preserving hardware adaptation, **not a pixel- or cycle-exact conversion**.

Code/audio stay in the fixed first 32 KiB. Bank 2 holds graphics; bank 3 holds the course. Terrain uses a circular name-table cache. Scene rebuilds blank the display; normal VRAM updates are queued for VBlank. Action edges are captured in the VBlank interrupt, with a three-press pending limit, so short presses are not lost during a busy render or hit-stop.

## Verification and limits

The recorded release verification is summarized below. Updating this README or repackaging does not imply that previously untested platforms have been verified.

| Validation | Recorded result | Evidence in the ZIP |
|---|---|---|
| Shared gameplay C | 139 assertions passed | `evidence/rules.log` |
| Native cartridge execution | 68 assertions passed | `evidence/verification.json`, `rom-checks.log` |
| Original JavaScript comparison | 14 normalized state contracts matched | `evidence/reference-original.json`, `reference-sms.json` |
| Chromium / offline player | 20 checks passed | `evidence/browser-verification.json` |
| Independent Emulicious smoke check | Boot, attract, real play and console pause/resume observed | `evidence/emulicious-verification.json` |
| Extracted-source rebuild | Byte-identical ROM; asset regeneration matched | `evidence/rebuild-verification.json` |

Run `make test` for the repeatable checks. Node.js enables the upstream comparison; Pillow enables screenshot export. Tests execute the **actual cartridge instructions** in our native player, as well as compiling the shared gameplay C for host-rule checks.

- **139 host-rule assertions**, zero failures.
- **68 cartridge / emulator assertions**, including controller-only title/start, no hold repeat, pause/resume, three deaths, game over, restart and attract takeover.
- Explicitly labelled **RAM fixtures** exercise cuts, a complete train, wave deflection, capsules, armour vs. rock, clocks, gems, zone bonuses, gate rows, both actual gate kills, loop transitions, ALL CLEAR, tally and SRAM. These are targeted tests, **not a claimed end-to-end campaign playthrough**.
- A maximum-pool pressure fixture checks one-frame input capture, bounded pools and no active-display VRAM writes.
- **14 state contracts** agree with untouched original `core.js`, after normalizing vertical coordinates.
- **20 Chromium browser checks**, including keyboard, touch, audio-context startup, pause, reset, byte-identical ROM download, offline loading, SRAM reload and WebAssembly/storage fallbacks. Browser SRAM tests also use an explicitly labelled fixture.
- Actual PSG output is non-silent and does not digitally clip in the tested sequence. This is not a human music audition.

An independent **Emulicious** smoke check also boots the unmodified ROM, runs attract, starts real play, and verifies console PAUSE/resume via keyboard input. Its window reports 100% emulation speed at 60 fps; this is not proof that every game-logic frame meets its budget. The tested default keyboard bindings were **A / S** for buttons 1 / 2 and **Enter** for console PAUSE.

The reports are `evidence/verification.json`, `browser-verification.json`, `emulicious-verification.json`, `rom-checks.log`, `rules.log` and the two `reference-*.json` files. Counts describe assertions, including setup/bounds checks, not dozens of complete campaigns.

**Not yet verified:** physical SMS / flash cartridge, PAL 50 Hz timing, Power Base Converter, full human campaign playthrough or human audio audition. The core targets a nominal 60 Hz cadence; saturated/heavy scenes can take more than one hardware frame and slow gameplay. Sprite flicker is an SMS hardware limit. Zero active-display writes in the tested scenes is not a universal timing guarantee.

## Provenance / licensing

Original directory revision: `7aad21b8dde2a1b40188eebf29474ce4727c8eb6` — ABA Games, MIT, 2026. This includes the correction that blade cuts do not damage the gate core.

Hardware/build reference: [Haroldo-OK’s Master System skill](https://github.com/haroldo-ok/sega-master-system-skill-for-claude), snapshot `2a40a072693600fb18cb24fbddecfc99cc7afe1a`. The guide informed the SDCC/devkitSMS workflow. Its unlicensed SMS test-wrapper source is **not included in the release**; the shipped script runner and SMS player are separately authored.

Matched [devkitSMS](https://github.com/sverx/devkitSMS) snapshot: `b172c7433ff690136fee47504a5fd427f62f2d16`. SMSlib and converter are public domain / Unlicense; CRT is GPL-2.0-or-later with its SDCC linking exception. The player’s [superzazu Z80 core](https://github.com/superzazu/z80) is MIT, © 2019 Nicolas Allemand.

See `LICENSE`, `THIRD_PARTY_NOTICES.md` and the preserved component licenses. The original game and new port code are MIT; this does **not** relabel the entire vendor tree as MIT. Unofficial homebrew; no affiliation with Sega or endorsement by the original author is implied.
