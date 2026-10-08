#!/usr/bin/env python3
"""Small controller-only ROM script runner, MIT, 2026 port contributors.
Uses our own SMS player and the separately MIT-licensed superzazu Z80 core.
Only reads RAM to assert results; all game actions are controller or console NMI.
Not a general SMS emulator or a substitute for hardware verification.
"""
import ctypes as C
import re
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def main():
    if len(sys.argv)!=3:raise SystemExit('Usage: python3 tools/check_rom.py ROM.sms SCRIPT.txt')
    rom_path,script=map(Path,sys.argv[1:])
    emu=C.CDLL(str(ROOT/'build/sms-player.so'))
    for n in ('sms_rom','sms_video'):getattr(emu,n).restype=C.POINTER(C.c_ubyte)
    emu.sms_read.argtypes=[C.c_uint16]
    rom=rom_path.read_bytes()
    if len(rom)!=65536:raise AssertionError('This player expects a 64 KiB ROM')
    C.memmove(emu.sms_rom(),rom,len(rom));emu.sms_init()
    pad=0;symbols={}
    ops={'==':lambda a,b:a==b,'!=':lambda a,b:a!=b,'>':lambda a,b:a>b,'<':lambda a,b:a<b,'>=':lambda a,b:a>=b,'<=':lambda a,b:a<=b}
    def buttons(s):
        result=0
        for part in re.split(r'[,+]',s.lower()):
            if part in ('none','0'):continue
            result|={'up':1,'down':2,'left':4,'right':8,'1':16,'2':32}[part]
        return result
    def run(n):
        for _ in range(n):emu.sms_frame(pad)
    def val(name,w=2):
        a=symbols[name];return sum(emu.sms_read(a+i)<<(8*i) for i in range(w))
    def pixels():return C.string_at(emu.sms_video(),256*192*4)
    for line_number,line in enumerate(script.read_text().splitlines(),1):
        line=line.split('#',1)[0].strip()
        if not line:continue
        tokens=line.split();command,args=tokens[0],tokens[1:]
        w=2
        if args and re.fullmatch(r'w[124]',args[-1]):w=int(args.pop()[1:])
        try:
            if command=='symbols':
                symbols={m[1]:int(m[2],16) for m in re.finditer(r'DEF\s+_(\w+)\s+0x([0-9A-Fa-f]+)',Path(args[0]).read_text())}
            elif command=='run':run(int(args[0]))
            elif command=='hold':pad=buttons(args[0])
            elif command=='tap':
                pad=buttons(args[0]);run(int(args[1]) if len(args)>1 else 1);pad=0;run(1)
            elif command=='pause':emu.sms_pause()
            elif command=='waituntil':
                name,op,expected=args[:3];expected=int(expected,0);limit=int(args[3]) if len(args)>3 else 600
                for _ in range(limit):
                    if ops[op](val(name,w),expected):break
                    run(1)
                else:raise AssertionError(f'timeout: {name}={val(name,w)}, expected {op} {expected}')
            elif command=='expect':
                name,op,expected=args;actual=val(name,w)
                if not ops[op](actual,int(expected,0)):raise AssertionError(f'{name}={actual}, expected {op} {expected}')
                print('PASS',line)
            elif command=='expectvdp':
                if args[0]!='display':raise ValueError('Only display VDP assertion is supported')
                actual=(emu.sms_vdp_reg(1)>>6)&1
                if actual!=int(args[1]):raise AssertionError(f'display={actual}, expected {args[1]}')
                print('PASS',line)
            elif command=='expectcolors':
                p=pixels();actual=len({p[i:i+3] for i in range(0,len(p),4)})
                if actual<int(args[0]):raise AssertionError(f'only {actual} distinct rendered colours')
                print('PASS',line,'actual',actual)
            elif command=='screenshot':
                p=pixels();rgb=bytearray(256*192*3)
                for i in range(256*192):rgb[i*3:i*3+3]=p[i*4:i*4+3]
                out=Path(args[0]);out.parent.mkdir(parents=True,exist_ok=True)
                out.write_bytes(b'P6\n256 192\n255\n'+rgb)
            elif command=='print':print(args[0],val(args[0],w))
            else:raise ValueError('Unsupported command: '+command)
        except Exception as exc:raise AssertionError(f'{script}:{line_number}: {line}: {exc}') from exc
    if emu.sms_active_writes():raise AssertionError('VDP data writes during active display')
    print('PASS controller-only script; zero active-display VDP data writes;',emu.sms_frames(),'emulated frames')

if __name__=='__main__':main()
