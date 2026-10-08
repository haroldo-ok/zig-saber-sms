# Native SMS cartridge. Tested with SDCC 4.5; gcc, make and Python 3.
# Rebuild SMSlib with the same SDCC as the game: never mix calling conventions.
# Node + Pillow are needed only for asset regeneration / optional evidence.
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
CC := sdcc
HOSTCC := gcc
CLANG ?= clang
CFLAGS := -mz80 --std-c99 --opt-code-speed --max-allocs-per-node 20000 -Ivendor/devkitsms -Isrc --peep-file vendor/devkitsms/peep-rules.txt
LIBFLAGS := -mz80 --max-allocs-per-node 100000 --peep-file vendor/devkitsms/peep-rules.txt
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x0200 --data-loc 0xC000 -Wl-u
OBJS := build/main.rel build/game.rel build/render.rel build/sound.rel
LIBNAMES := SMSlib SMSlib_sprite SMSlib_textrenderer SMSlib_string SMSlib_VRAMmemcpy SMSlib_loadTileMapColumn SMSlib_deprecated
LIBOBJS := $(addprefix build/,$(addsuffix .rel,$(LIBNAMES)))
LIBHEADERS := vendor/devkitsms/smslib-src/SMSlib.h vendor/devkitsms/smslib-src/SMSlib_common.c vendor/devkitsms/peep-rules.txt

all: zig-saber.sms

build:
	mkdir -p build

build/ihx2sms: vendor/devkitsms/ihx2sms.c | build
	$(HOSTCC) -O2 -w $< -o $@

build/crt0_sms.rel: vendor/devkitsms/crt0_sms.s | build
	sdasz80 -g -l -o $@ $<

build/SMSlib.rel: vendor/devkitsms/smslib-src/SMSlib.c $(LIBHEADERS) | build
	$(CC) $(LIBFLAGS) -c $< -o $@

build/SMSlib_%.rel: vendor/devkitsms/smslib-src/SMSlib_%.c $(LIBHEADERS) | build
	$(CC) $(LIBFLAGS) -c $< -o $@

build/SMSlib.lib: $(LIBOBJS)
	sdar -rc $@ $^

build/%.rel: src/%.c src/game.h src/ui.h src/assets.h src/sound_ids.h src/sound_data.h vendor/devkitsms/SMSlib.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/zig-saber.ihx: $(OBJS) build/SMSlib.lib build/crt0_sms.rel
	$(CC) $(LDFLAGS) build/crt0_sms.rel $(OBJS) build/SMSlib.lib -o $@ 2>&1 | tee build/link.log
	! grep -E 'Undefined Global|ASlink-(Warning|Error)|error:' build/link.log

zig-saber.sms: build/zig-saber.ihx build/ihx2sms assets/graphics.bin assets/course.bin tools/pack_rom.py
	build/ihx2sms build/zig-saber.ihx build/fixed.sms
	python3 tools/pack_rom.py build/fixed.sms assets/graphics.bin assets/course.bin $@
	cp build/zig-saber.noi zig-saber.noi
	cp build/zig-saber.map zig-saber.map

assets:
	node tools/export_original.js
	python3 tools/generate.py

build/rules: tests/rules.c src/game.c src/game.h src/assets.h | build
	$(HOSTCC) -O2 -std=c99 -DHOST_TEST -Isrc tests/rules.c src/game.c -o $@

rules: build/rules
	build/rules

build/sms-player.so: tools/web_sms.c vendor/z80/z80.c vendor/z80/z80.h | build
	$(HOSTCC) -O2 -shared -fPIC -Ivendor/z80 tools/web_sms.c vendor/z80/z80.c -o $@

test: all build/sms-player.so rules
	python3 tests/run_tests.py

player: all assets/sms-player.wasm tools/player-template.html tools/make_player.py
	python3 tools/make_player.py

# Optional: prebuilt WASM is included, so normal builds do not require clang.
wasm:
	$(CLANG) --target=wasm32 -O3 -ffreestanding -fno-builtin -nostdlib -Itools/wasm_include -Ivendor/z80 tools/web_sms.c vendor/z80/z80.c -Wl,--no-entry -Wl,--export-all -Wl,--export-memory -Wl,--initial-memory=2097152 -Wl,--max-memory=4194304 -o assets/sms-player.wasm

serve: player
	python3 tools/serve_player.py

clean:
	rm -rf build
	rm -f zig-saber.sms zig-saber.noi zig-saber.map

.PHONY: all assets rules test player wasm serve clean
