#!/usr/bin/env python3
"""Compile upstream procedural graphics/courses into two 16 KiB SMS banks.
No downloaded art, runtime floating point or framebuffer is used by the ROM.
"""
import json, math, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
D=json.loads((ROOT/'assets/original.json').read_text())
A=ROOT/'assets'; S=ROOT/'src'; A.mkdir(exist_ok=True)
macros={}
def define(k,v): macros[k]=v

def sy(y): return 16+int((y-16)*11/13+0.5)
def sr(r): return (r*11+6)//13
co=D['courses'][0]; end=co['end']
geo=[(sr(co['ceil'][i]),22-sr(26-co['flo'][i]),sr(co['i0'][i]),sr(co['i1'][i])) for i in range(end)]
def solid(col,row):
    if row<0 or row>=22: return True
    c,f,a,b=geo[col] if 0<=col<end else (2,2,0,0)
    return row<c or row>=22-f or (a<=row<b)
def clear(x0,x1,y,h):
    return all(not solid(c,r) for c in range(x0//8,x1//8+1) for r in range((y-h-16)//8,(y+h-16)//8+1))
def ranges(ev):
    v={'aimed':2.4,'rusher':5.6}.get(ev['type'],2.4)*1.1*1.32
    span=int(280*(v-1.2)/v)+16
    ys=[y for y in range(24,184) if clear(ev['wx']-span-48,ev['wx']+16,y,6 if ev['type']=='rusher' else 7)]
    runs=[]
    for y in ys:
        if not runs or y>runs[-1][1]+1: runs.append([y,y])
        else: runs[-1][1]=y
    runs=sorted(runs,key=lambda r:r[1]-r[0],reverse=True)[:2]
    return sum(runs,[])+[0]*(4-len(runs)*2)
course=bytearray()
def put(k,data):
    define(k,hex(0x8000+len(course)));course.extend(data)
put('GEO_ADDR',bytes(v for g in geo for v in g))
types={'drone':1,'train':2,'shell':3,'rusher':4,'convoy':5,'aimed':6,'plug':7,'capitem':8}
for loop,c in enumerate(D['courses']):
    data=bytearray()
    for e in c['events']:
        lim=ranges(e) if e['type'] in ('aimed','rusher') else [0]*4
        data.extend(struct.pack('<H8B',e['wx'],types[e['type']],sy(e.get('y',120)),e.get('n',1),int(e.get('carrier',False)),*lim))
    put('EVENT%d_ADDR'%loop,data);define('EVENT%d_COUNT'%loop,len(c['events']))
put('GEM_ADDR',b''.join(struct.pack('<HBB',g['wx'],sy(g['y']),g['zone']) for g in co['gems']))
define('GEM_COUNT',len(co['gems']));define('COURSE_END',end)
define('ZONE0_END',co['zones'][0]['c1']);define('ZONE1_END',co['zones'][1]['c1'])
define('START_CAM',co['cps'][0]*8-52)
assert len(course)<=16384
(A/'course.bin').write_bytes(course+bytes([255])*(16384-len(course)))

# VDP planar tile encoder, and 8x16 hardware sprite allocator.
def encode(tile):
    return bytes(sum(((tile[y][x]>>p)&1)<<(7-x) for x in range(8)) for y in range(8) for p in range(4))
zero=[[0]*8 for _ in range(8)]
tiles=[encode(zero) for _ in range(320)]
next_sprite=2
pal={'w':1,'c':2,'e':2,'d':9,'r':5,'f':4}
def sprite(k,pixels,palette,width=16,height=16):
    global next_sprite
    base=next_sprite; define('SPR_'+k,base)
    for x0 in range(0,width,8):
        for y0 in range(0,height,16):
            for yoff in (0,8):
                t=[[pixels[y0+yoff+y][x0+x] if y0+yoff+y<len(pixels) and x0+x<len(pixels[0]) else 0 for x in range(8)] for y in range(8)]
                tiles[next_sprite]=encode(t);next_sprite+=1
    assert next_sprite<=128, (k,next_sprite)
    return base

def art(name,mapping,w=16,h=16,shear=0):
    rows=D['sprites'][name]; rh=len(rows); rw=len(rows[0]); img=[[0]*w for _ in range(h)]
    ox=(w-rw)//2; oy=(h-rh)//2
    for y,row in enumerate(rows):
        for x,ch in enumerate(row):
            yy=oy+y+int((x-rw/2)*shear+(0.5 if (x-rw/2)*shear>=0 else -0.5))
            if 0<=yy<h and 0<=ox+x<w: img[yy][ox+x]=mapping.get(ch,0)
    return img
for direction,shear in [('UP',-0.2),('DOWN',0.2)]:
    for status,nose in [('HOME',2),('OUT',9),('LASER',3)]:
        sprite('SHIP_'+direction+'_'+status,art('ship',dict(pal,e=nose),shear=shear),None)
for name,cols in [('DRONE',{'a':4,'b':3,'w':15}),('TRAIN',{'a':5,'b':4,'w':15})]:
    for i in range(2): sprite(name+str(i),art('drone2' if i else 'drone',cols),None)
sprite('SHELL',art('shell',{'a':7,'b':8,'w':1}),None)
sprite('RUSHER',art('rusher',{'a':6,'b':1}),None)
for k,cols in [('CAP',{'a':5,'b':3}),('CAP_BLINK',{'a':3,'b':5}),('CAP_BIG',{'a':1,'b':5})]: sprite(k,art('cap',cols),None)
for k,cols in [('GEM',{'a':6,'b':6,'w':1}),('GEM_BLINK',{'a':1,'b':6,'w':1})]: sprite(k,art('gem',cols,8,16),None,8,16)
for k,col in [('WAVE',2),('WAVE_LASER',3)]:
    img=[[0]*8 for _ in range(32)]
    for y in range(30):
        x=7-int(4*((y-14.5)/14.5)**2)
        for xx in range(max(0,x-3),x+1): img[y][xx]=1 if xx>x-2 else col
    sprite(k,img,None,8,32)
for k,col in [('MARK_DRONE',4),('MARK_TRAIN',5),('MARK_SHELL',7),('MARK_RUSHER',6)]:
    img=[[0]*8 for _ in range(16)]
    for y in (3,4,11,12):
        for x in range(2,7): img[y][x]=col
    for y in range(5,11): img[y][6]=col
    img[7][0]=img[7][1]=col
    sprite(k,img,None,8,16)
for k,col in [('BLADE',2),('BLADE_LASER',3)]:
    img=[[0]*8 for _ in range(16)]
    for x in range(8):
        img[1][x]=1;img[2][x]=col
        if x%2==0: img[0][x]=col
    sprite(k,img,None,8,16)
for k,h in [('PLATE',32),('SEAL',48)]:
    img=[[0]*8 for _ in range(h)]
    for y in range(h-2):
        for x in range(8): img[y][x]=1 if x<2 else 8 if y%8==7 else 7
    sprite(k,img,None,8,h)
for k,col in [('BURST',1),('BURST_RED',5),('BURST_PINK',6)]:
    img=[[0]*8 for _ in range(16)]
    for x,y in [(3,3),(4,3),(1,1),(6,1),(0,4),(7,4),(1,7),(6,7)]: img[y][x]=col
    sprite(k,img,None,8,16)
for k,col in [('TRAIL',5),('TRAIL_CHAIN',2),('TRAIL_LASER',3)]:
    img=[[0]*8 for _ in range(16)];img[0][0]=col;img[0][1]=col
    sprite(k,img,None,8,16)
for k,col in [('REACH',2),('REACH_LASER',3)]:
    img=[[0]*8 for _ in range(32)]
    for y in (0,1,28,29):
        for x in (3,4,5): img[y][x]=col
    sprite(k,img,None,8,32)
define('SPRITE_TILES',next_sprite)

# Font reuses the upstream 5x7 glyphs, one 8x8 tile per ASCII code.
for ch in range(32,96):
    gl=D['font'].get(chr(ch),'');img=[[0]*8 for _ in range(8)]
    for i,v in enumerate(gl[:35]):
        if v=='#': img[i//5][1+i%5]=1
    tiles[128+ch-32]=encode(img)
define('FONT_BASE',128)
# Three zones x 16 exposed-edge masks x two rock textures.
for zone in range(3):
    base,edge,dark=[(4,5,6),(7,8,9),(10,11,12)][zone]
    for mask in range(16):
        for tex in range(2):
            img=[[base]*8 for _ in range(8)]
            if zone==2:
                for x in range(8): img[7][x]=dark
                for y in range(7): img[y][3 if tex else 7]=dark
            else:
                for x,y in ([(2,2),(3,2),(2,3),(3,3),(6,6)] if tex else [(5,1),(5,2),(1,5)]): img[y][x]=dark
            for y in range(8):
                for x in range(8):
                    if (mask&1 and y<2) or (mask&2 and y>=6) or (mask&4 and x<2) or (mask&8 and x>=6): img[y][x]=edge
            tiles[192+zone*32+mask*2+tex]=encode(img)
define('ROCK_BASE',192)
for n in range(4):
    img=[[0]*8 for _ in range(8)];img[(n*3+1)%8][(n*5+2)%8]=[2,3,1,13][n]
    tiles[288+n]=encode(img)
define('STAR_BASE',288)
# HUD gauges and a little background ship for the life counter.
for n in range(8):
    img=[[0]*8 for _ in range(8)]
    if n<2:
        for x in range(7): img[2][x]=img[5][x]=2 if n==0 else 13
    else:
        for y in range(2,6):
            for x in range(7): img[y][x]=[13,3,14,2,5,15][n-2]
    tiles[292+n]=encode(img)
define('GAUGE_BASE',292)
for n in range(12):
    img=[[12]*8 for _ in range(8)]
    if n<4:  # closed, arming, flying, open
        for y in range(8):
            for x in range(8):
                if n==0: img[y][x]=1 if x<2 else 11 if y<7 else 12
                elif n==1: img[y][x]=1 if x<2 or y%4==0 else 11
                elif n==2: img[y][x]=12 if x<2 else 2
                else: img[y][x]=1 if 3<=x<5 and 2<=y<6 else 14 if x<6 else 13
    elif n==4:
        for y in range(8): img[y][0]=img[y][1]=11
    elif n==5:
        for y in range(8): img[y][3]=img[y][4]=14
    elif n==6:
        for y in range(8): img[y][3]=img[y][4]=2
    elif n==7:
        for y in range(8): img[y][3]=img[y][4]=1
    elif n==8:
        for y in range(8): img[y][0]=img[y][1]=14
    else:
        for x in range(8): img[7][x]=10
    tiles[300+n]=encode(img)
define('BOSS_TILE_BASE',300)
life=[[0]*8 for _ in range(8)]
for y,row in enumerate(D['sprites']['life']):
    for x,ch in enumerate(row): life[y+2][x]={'w':1,'r':14,'c':3}.get(ch,0)
tiles[312]=encode(life);define('LIFE_TILE',312)
# Palette bytes: the SMS stores 2 bits per RGB channel.
def rgb(r,g,b): return r|(g<<2)|(b<<4)
bg=[rgb(*x) for x in [(0,0,0),(3,3,3),(1,1,1),(0,2,3),(2,0,0),(3,2,0),(1,0,0),(0,1,0),(1,3,2),(0,0,0),(0,0,2),(0,2,3),(0,0,1),(3,3,2),(3,0,0),(3,1,3)]]
sp=[rgb(*x) for x in [(0,0,0),(3,3,3),(0,2,3),(3,3,2),(3,2,0),(3,0,0),(3,1,3),(1,2,3),(0,0,2),(2,2,2),(3,3,0),(2,0,0),(0,2,0),(1,0,0),(1,1,1),(0,0,0)]]
# Banded logo, faithful to the original procedural title, compiled offline.
canvas=[[0]*256 for _ in range(192)]
def pixel(x,y,c):
    if 0<=x<256 and 0<=y<192: canvas[y][x]=c

def glyph(ch,x,y,bands,cut=False):
    gl=D['font'][ch]
    for shadow in [True,False]:
        for i,v in enumerate(gl[:35]):
            if v!='#':continue
            px=x+(i%5)*4;py=y+(i//5)*4
            if cut and py+2>62-(px+2-100)*.27: px+=3;py+=2
            if shadow:px+=2;py+=2
            for yy in range(4):
                for xx in range(4):pixel(px+xx,py+yy,6 if shadow else bands[i//5])
for n,ch in enumerate('ZIG'):glyph(ch,24+n*24,24+(10 if n==1 else 0),[1,1,3,3,3,10,10])
for n,ch in enumerate('SABER'):glyph(ch,108+n*24,28,[1,1,13,13,5,5,14],True)
for n in range(34):pixel(round(20+n*2.4),round(66+abs(((n+6)%24)-12)*.9),5 if n>26 else 14)
for x in range(102,237):pixel(x,round(62-(x-100)*.27),3)
logo_tiles=[]; lookup={encode(zero):0}; titlemap=[0]*768
for ty in range(24):
    for tx in range(32):
        im=[canvas[ty*8+y][tx*8:tx*8+8] for y in range(8)]
        b=encode(im)
        if b not in lookup:
            lookup[b]=320+len(logo_tiles);logo_tiles.append(b)
        titlemap[ty*32+tx]=lookup[b]
assert len(logo_tiles)<=128, len(logo_tiles)
# Plain small-font text remains independent of the logo's tile dictionary.
def text(row,label,col=None):
    col=(32-len(label))//2 if col is None else col
    for i,ch in enumerate(label):
        if 0<=col+i<32:titlemap[row*32+col+i]=128+ord(ch)-32
text(0,'1UP 0000000 HI 0500000',1)
text(10,'ONE BUTTON = TURN + FIRE')
text(12,'FAR 100  CUT 300',8)
text(14,'ALL = LASER',10)
text(16,'FAR ---  CUT 500',8)
text(18,'GEMS REFILL YOUR LASER',8)
text(20,'LASER = BLADE + ARMOR')
text(22,'PUSH BUTTON')
text(23,'ABA GAMES / SMS PORT')
gfx=bytearray(b''.join(tiles));define('GAME_TILES_ADDR',hex(0x8000));define('GAME_TILES_BYTES',len(gfx))
def gput(k,data):define(k,hex(0x8000+len(gfx)));gfx.extend(data)
gput('LOGO_TILES_ADDR',b''.join(logo_tiles));define('LOGO_TILES_BYTES',len(logo_tiles)*32)
gput('TITLE_MAP_ADDR',b''.join(struct.pack('<H',v) for v in titlemap))
gput('BG_PALETTE_ADDR',bytes(bg));gput('SP_PALETTE_ADDR',bytes(sp))
assert len(gfx)<=16384,len(gfx)
(A/'graphics.bin').write_bytes(gfx+bytes([255])*(16384-len(gfx)))
(S/'assets.h').write_text('/* Generated by tools/generate.py; banked ROM addresses, not RAM. */\n#ifndef ASSETS_H\n#define ASSETS_H\n'+''.join('#define %s %s\n'%(k,v) for k,v in macros.items())+'#endif\n')

# A one-chip re-voicing of the original two-PSG score and effect programs.
# Lead + bass on channels 0/1; effects on 2; drums/effect noise share 3.
effect_lines=[];effect_defs=[];effect_names=[]
for idx,e in enumerate(D['effects']):
    e['noise'] = e.get('noise') or []
    e['tones'] = e.get('tones') or []
    name=e['name'].upper()+('_'+str(e['arg']) if e['name'] in ('cut','gem') else '')
    frames=max([len(t) for t in e.get('tones',[])]+[len(e.get('noise',[])),1])
    ff=[]
    for i in range(frames):
        f,a=e.get('tones',[[]])[0][i] if e.get('tones') and i<len(e['tones'][0]) else (0,15)
        period=max(1,min(1023,round(3579545/(32*f)))) if f else 0
        nm,na=e['noise'][i] if i<len(e.get('noise',[])) else (4,15)
        ff.append('{%d,%d,%d,%d}'%(period,a,nm,na))
    effect_lines.append('static const SoundFrame se_%d[] = {%s};'%(idx,','.join(ff)))
    effect_defs.append('{se_%d,%d,%d}'%(idx,frames,e['prio']));effect_names.append('#define SE_%s %d'%(name,idx))
periods=[max(1,min(1023,round(3579545/(32*(440*2**((m-69)/12)))))) for m in range(128)]
ml=[];mb=[]
for z in range(4):
    rows=D['music'][z]
    ml.append('{'+','.join(str(rows[i%len(rows)]['lead']) for i in range(128))+'}')
    mb.append('{'+','.join(str(rows[i%len(rows)]['bass']) for i in range(128))+'}')
(S/'sound_data.h').write_text('/* Original ABA Games PSG programs, re-voiced for one SN76489. */\n'+ '\n'.join(effect_names)+'\n'+ '\n'.join(effect_lines)+'\nstatic const Effect effects[] = {'+','.join(effect_defs)+'};\nstatic const unsigned int midi_period[] = {'+','.join(map(str,periods))+'};\nstatic const signed char music_lead[4][128] = {'+','.join(ml)+'};\nstatic const unsigned char music_bass[4][128] = {'+','.join(mb)+'};\n')
(S/'sound_ids.h').write_text('/* Generated effect IDs. */\n#ifndef SOUND_IDS_H\n#define SOUND_IDS_H\n'+'\n'.join(effect_names)+'\n#endif\n')
# Human/test metadata does not go into the ROM.
(A/'course.json').write_text(json.dumps({'geometry':geo,'events':D['courses'][0]['events'],'gems':[{'wx':g['wx'],'y':sy(g['y']),'zone':g['zone']} for g in co['gems']], 'end':end,'zones':co['zones'],'start_cam':co['cps'][0]*8-52},indent=2))
(A/'manifest.json').write_text(json.dumps({'graphics_used':len(gfx),'course_used':len(course),'sprite_tiles':next_sprite,'logo_tiles':len(logo_tiles),'game_tiles':320,'vertical_scale':'11/13','source_commit':(ROOT/'upstream/upstream-commit.txt').read_text().strip()},indent=2))
print('Graphics bank: %d/16384; course bank: %d/16384; sprite tiles: %d/128; logo: %d/128'%(len(gfx),len(course),next_sprite,len(logo_tiles)))
try:
    from PIL import Image
    im=Image.new('RGB',(256,192))
    colors=[((c&3)*85,((c>>2)&3)*85,((c>>4)&3)*85) for c in bg]
    for y in range(192):
        for x in range(256):im.putpixel((x,y),colors[canvas[y][x]])
    im.save(A/'logo.png')
except ImportError:pass
