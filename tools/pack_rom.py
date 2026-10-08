#!/usr/bin/env python3
"""Append bank 2 (graphics) and bank 3 (course) to the fixed 32 KiB engine.
The Sega mapper switches only slot 2. Code and sound tables never leave slots
0/1. Update the export 64 KiB header/checksum after appending the banks.
"""
import sys, hashlib, re, os
from datetime import date, datetime, timezone
from pathlib import Path
fixed,graphics,course,dest=map(Path,sys.argv[1:])
code=fixed.read_bytes()
assert len(code)==32768, 'Fixed engine must fit 32 KiB; got %d'%len(code)
assert code[0x7ff0:0x7ff8]==b'TMR SEGA', 'SEGA header missing'
banks=[p.read_bytes() for p in (graphics,course)]
assert all(len(b)==16384 for b in banks)
rom=bytearray(code+b''.join(banks))
assert rom[0x7fe0:0x7fe4]==b'SDSC', 'SDSC header missing'
# ihx2sms writes today's date. Pin the release day for reproducible binaries.
epoch=os.environ.get('SOURCE_DATE_EPOCH')
day=datetime.fromtimestamp(int(epoch),timezone.utc).date() if epoch else date(2026,10,7)
def bcd(n):return (n//10)*16+n%10
rom[0x7fe6:0x7fea]=bytes((bcd(day.day),bcd(day.month),bcd(day.year%100),bcd(day.year//100)))
rom[0x7fff]=0x4e  # export SMS, 64 KiB
checksum=(sum(rom[:0x7ff0])+sum(rom[0x8000:]))&65535
rom[0x7ffa:0x7ffc]=checksum.to_bytes(2,'little')
mapfile=Path('build/zig-saber.map').read_text()
for area,limit in [('_CODE',0x7d00),('_DATA',0xde00)]:
    m=re.search(r'^'+area+r'\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)',mapfile,re.M)
    assert m, 'Missing '+area+' in linker map'
    if m:
        start,size=map(lambda x:int(x,16),m.groups())
        assert start+size<limit,(area,hex(start),hex(size))
        print('%s: %s..%s (%d bytes)'%(area,hex(start),hex(start+size-1),size))
dest.write_bytes(rom)
print('SEGA header found, 64 KiB checksum updated: %04X'%checksum)
print('ROM: %s, %d bytes, SHA-256 %s'%(dest,len(rom),hashlib.sha256(rom).hexdigest()))
