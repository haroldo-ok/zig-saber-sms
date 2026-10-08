#!/usr/bin/env python3
"""Create the source + ready-to-play release, excluding unlicensed dev tooling.
MIT, 2026 Zig Saber SMS port contributors.
"""
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED,ZipInfo

ROOT=Path(__file__).resolve().parents[1]
files=set()
def add(path):
    p=ROOT/path
    if not p.is_file():raise FileNotFoundError(p)
    files.add(p)
def tree(path,exclude=()):
    for p in (ROOT/path).rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and p.suffix not in exclude:files.add(p)
for p in ('README.md','LICENSE','THIRD_PARTY_NOTICES.md','Makefile','.gitignore','zig-saber.sms','zig-saber.noi','zig-saber.map','PLAY.html'):add(p)
for p in ('src','assets','upstream','vendor/z80'):tree(p)
for p in ('rules.c','reference_contract.c','reference_contract.js','run_tests.py','browser_test.py','controller-only.txt'):add('tests/'+p)
for p in ('export_original.js','generate.py','pack_rom.py','web_sms.c','player-template.html','make_player.py','check_rom.py','serve_player.py','package.py'):add('tools/'+p)
tree('tools/wasm_include')
for p in ('SMSlib.h','peep-rules.txt','crt0_sms.s','ihx2sms.c','LICENSE-SMSlib','LICENSE-ihx2sms','COPYING.txt','LICENSES.txt','VERSION.txt'):add('vendor/devkitsms/'+p)
tree('vendor/devkitsms/smslib-src',exclude=('.lib','.rel','.lst','.rst','.sym','.asm'))
for p in ('verification.json','browser-verification.json','controller-only.log','rom-checks.log','rules.log','reference-original.json','reference-sms.json','emulicious-verification.json'):
    if (ROOT/'evidence'/p).exists():add('evidence/'+p)
for p in ('title','game-over','multi-cut','laser-cut','pressure','cavern','fortress','gate','gate-open','gate-timeout','loop-2','second-gate-clear','all-clear','final-clear','browser-desktop','browser-mobile','emulicious-title-window','emulicious-pause-window','emulicious-resume-window'):
    if (ROOT/'evidence'/(p+'.png')).exists():add('evidence/'+p+'.png')
if (ROOT/'evidence/rebuild-verification.json').exists():add('evidence/rebuild-verification.json')
rom=(ROOT/'zig-saber.sms').read_bytes()
manifest={'release':'Zig Saber SMS 1.0','release_date':'2026-10-07','target':'Sega Master System, NTSC, Sega mapper','rom_bytes':len(rom),'rom_sha256':hashlib.sha256(rom).hexdigest(),'sega_header':rom[0x7ff0:0x8000].hex(),'sega_checksum':f'{int.from_bytes(rom[0x7ffa:0x7ffc],"little"):04X}','build_compiler':'SDCC 4.5.0','original_revision':'7aad21b8dde2a1b40188eebf29474ce4727c8eb6','devkitsms_revision':'b172c7433ff690136fee47504a5fd427f62f2d16','files':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}}
(ROOT/'RELEASE.json').write_text(json.dumps(manifest,indent=2)+'\n');add('RELEASE.json')
archive=ROOT/'zig-saber-sms-source.zip'
with ZipFile(archive,'w',compression=ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(files):
        info=ZipInfo('zig-saber-sms/'+str(p.relative_to(ROOT)),date_time=(2026,10,7,0,0,0));info.compress_type=ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,p.read_bytes(),compress_type=ZIP_DEFLATED,compresslevel=9)
print(archive.name,archive.stat().st_size,'bytes,',len(files),'files')
print('ROM SHA-256:',manifest['rom_sha256'])
print('Excluded: generated build objects, old diagnostics, stock unlicensed smstest wrapper, emulator installations.')
