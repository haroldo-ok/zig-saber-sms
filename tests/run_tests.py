#!/usr/bin/env python3
"""Real-ROM tests, kept separate from host rule contracts.
Controller-only scenarios do not write RAM. Fixtures explicitly seed RAM and
then exercise actual Z80 code with ordinary controller input. All addresses
are regenerated from this build's NoICE symbols, never hand-copied.
"""
import ctypes as C, hashlib, json, re, struct, subprocess
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
E=ROOT/'evidence';E.mkdir(exist_ok=True)
S={m[1]:int(m[2],16) for m in re.finditer(r'DEF\s+_(\w+)\s+0x([0-9A-Fa-f]+)',(ROOT/'zig-saber.noi').read_text())}
ROM=(ROOT/'zig-saber.sms').read_bytes()
checks=0;log=[]
def check(ok,label):
    global checks
    checks+=1
    if not ok:raise AssertionError(label)
    log.append('PASS '+label)
    print('PASS',label)

def run(cmd):subprocess.run(cmd,cwd=ROOT,check=True)
# Controller-only script: no fixture writes and no injected gameplay calls.
run(['make','build/sms-player.so'])
controller='''symbols zig-saber.noi
run 180
expect game_state == 0 w1
expectvdp display 1
expectcolors 8
screenshot evidence/title.ppm
hold 1
waituntil cam_x > 316 300
expect game_state == 2 w1
expect presses == 0
expect lives == 3 w1
hold none
run 2
tap 1
waituntil presses == 1 20
tap 2
waituntil presses == 2 20
expect dry_turns >= 1
pause
waituntil paused == 1 60 w1
run 12
expect paused == 1 w1
pause
waituntil paused == 0 60 w1
hold none
waituntil game_state == 4 2000 w1
expect lives == 0 w1
expect deaths == 3
run 30
expectvdp display 1
screenshot evidence/game-over.ppm
run 50
tap 1
waituntil game_state == 1 100 w1
expect lives == 3 w1
expect presses == 0
expect loop_no == 0 w1
run 30
expectvdp display 1
'''
(ROOT/'tests/controller-only.txt').write_text(controller)
with (E/'controller-only.log').open('w') as f:
    subprocess.run([sys.executable,'tools/check_rom.py','zig-saber.sms','tests/controller-only.txt'],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
check(True,'unmodified cartridge: title, held-button edge, turn, dry turn, PAUSE/NMI, three deaths, game over, restart (our controller-only harness)')
# Native build of the same lightweight emulator as the offline browser player.
run(['make','build/sms-player.so'])
L=C.CDLL(str(ROOT/'build/sms-player.so'))
for name in ('sms_rom','sms_ram','sms_sram','sms_video'):getattr(L,name).restype=C.POINTER(C.c_ubyte)
L.sms_audio.restype=C.POINTER(C.c_float)
L.sms_read.argtypes=[C.c_uint16]
C.memmove(L.sms_rom(),ROM,len(ROM));RAM=L.sms_ram()
byte_names='game_state is_demo loop_no zone lives laser chain guard_t paused cam_sub shot_active shot_y freeze_t blade_t blade_y blade_len blade_n boss_active boss_hp boss_intro boss_leaving boss_over boss_flash scene_dirty boss_dirty hud_dirty flash_t flash_n zone_lost final_clear next_event next_gem gem_chain extends enemy_count pending_count item_count trail_count gift_t emit_n trail_next tally_done tally_wait press_queue ship_dir shot_dir blade_dir display_off patch_count'.split()
W={n:1 for n in byte_names};W.update(score=4,hi_score=4)
def read(n,width=None):
    w=width or W.get(n,2);a=S[n];return sum(L.sms_read(a+i)<<(8*i) for i in range(w))
def poke(n,value,width=None):
    w=width or W.get(n,2);a=S[n]&8191
    for i in range(w):RAM[a+i]=(value>>(8*i))&255

def put(n,data,offset=0):
    a=(S[n]&8191)+offset
    for i,b in enumerate(data):RAM[a+i]=b

def frames(n,button=0):
    for _ in range(n):L.sms_frame(button)
def wait(n,value,limit=500):
    for i in range(limit):
        if read(n)==value:return i
        L.sms_frame(0)
    raise AssertionError('timeout '+n+' '+str(value)+'; actual '+str(read(n)))

def action():frames(2,16);frames(1)
def shot(n):
    try:
        from PIL import Image
        im=Image.frombytes('RGBA',(256,192),C.string_at(L.sms_video(),256*192*4)).convert('RGB')
        im.save(E/(n+'.png'))
    except ImportError:pass

def boot_play():
    C.memset(L.sms_sram(),0,16384);L.sms_init();frames(180);action();wait('game_state',2)
    for _ in range(80):
        if read('cam_x')>316:break
        frames(1)
    check(read('cam_x')>316,'native player core boots and starts the real cartridge')

boot_play()
# Controller-only pause: observe stable physics, without poking anything.
L.sms_pause();wait('paused',1);frames(3);cam=read('cam_x');py=read('ship_y');frames(30)
check(read('cam_x')==cam and read('ship_y')==py,'controller-only PAUSE stops physics')
L.sms_pause();wait('paused',0);frames(2)
check(read('cam_x')>cam,'controller-only NMI resumes physics')
# Press rhythm keeps the first section alive using input alone.
for i in range(150):L.sms_frame(16 if i%20==0 else 0)
check(read('presses')>=3 and read('deaths')<=1,'controller-only repeated press edges reach real play')
check(L.sms_active_writes()==0,'controller-only VDP writes stay outside active display')
# Full attract demo is the actual game, not a video.
L.sms_init();frames(750)
check(read('is_demo')==1 and read('game_state') in (1,2,3),'controller-only attract demo runs the same ROM rules')
action();wait('game_state',1)
check(read('is_demo')==0 and read('lives')==3,'controller input takes over attract and starts a fresh game')
wait('game_state',2);frames(25)

# Packing invariants of the Z80 image. Next symbol confirms SDCC array size.
check(S['pending']-S['enemies']==24*9,'Z80 Enemy layout matches generated fixture offsets')
check(S['trains']-S['pending']==12*5,'Z80 Pending layout matches generated fixture offsets')

def fixture(**override):
    # Pause via the console before writing a explicitly labelled fixture.
    if not read('paused'):L.sms_pause();wait('paused',1)
    frames(3)
    base=dict(game_state=2,is_demo=0,loop_no=0,zone=0,lives=3,score=0,extends=0,cam_x=0,cam_sub=0,ship_y=104<<8,ship_dir=1,shot_dir=1,
              laser=0,laser_t=0,chain=0,chain_t=0,guard_t=0,shot_active=0,shot_x=0,shot_y=104,freeze_t=0,blade_t=0,blade_n=0,
              boss_active=0,boss_intro=0,boss_hp=0,boss_time=0,boss_leaving=0,boss_over=0,boss_flash=0,boss_launch=200,boss_emit=200,
              final_clear=0,zone_lost=0,next_event=98,next_gem=57,gem_chain=0,phase_t=0,gift_t=0,emit_n=0,
              enemy_count=0,pending_count=0,item_count=0,trail_count=0,trail_next=0,tally_done=0,tally_wait=0,press_queue=0,flash_t=0,
              presses=0,dry_turns=0,cut_kills=0,far_kills=0,cap_count=0,gem_count=0,deaths=0,boss_hits=0,boss_kills=0,pool_drops=0)
    base.update(override)
    for n,v in base.items():poke(n,v)
    for n,size in [('enemies',216),('pending',60),('trains',60),('items',32),('trails',36),('bursts',40),('gem_taken',57),('plate_state',5),('plate_t',10)]:put(n,bytes(size))
    poke('scene_dirty',1);poke('hud_dirty',1);frames(35)
    check(read('display_off')==0,'fixture scene completed its display-off rebuild')

def enemy(i,type,x,y=104,group=0,gate=0,row=0,v=None):
    speed={1:38,2:32,3:26,4:90,7:19,9:38}[type] if v is None else v
    put('enemies',struct.pack('<h7B',x*16,y,type,speed,0,group,row,gate),i*9)
def item(i,x,y=104,big=False):put('items',struct.pack('<hBB',x*16,y,2 if big else 1),i*4)
def unpause():poke('paused',0);frames(1)

def gate_fixture(**kw):
    d=dict(boss_active=1,boss_hp=24,boss_time=4500,boss_intro=0,cam_x=10624,zone=2)
    d.update(kw);fixture(**d)

fixture(enemy_count=3)
put('trains',bytes([3,0,0,1,3]),5)
for i,x in enumerate((83,101,119)):enemy(i,2,x,group=1)
poke('paused',0);action();wait('cut_kills',3,40)
check(read('score')==2100 and read('item_count')==1,'ROM fixture: carrier train multi-cut, doubling score, capsule drop')
check(read('chain')==1 and read('shot_active')==0,'ROM fixture: cut returns energy immediately and raises chain')
frames(2);shot('multi-cut')
# Buffer a turn while hit-stop is active; it must not disappear.
frames(2,32);frames(12)
check(read('presses')==2,'ROM fixture: press buffered through cut hit-stop')

fixture(enemy_count=1,shot_active=1,shot_x=130,shot_y=104,shot_dir=1)
enemy(0,3,143,110);unpause();frames(3)
check(read('shot_active')==0 and read('enemy_count')==1 and read('score')==0,'ROM fixture: shell stops the far wave')

fixture(enemy_count=1,laser=3,laser_t=420)
enemy(0,1,198);poke('paused',0);action();wait('cut_kills',1,40)
check(read('blade_len')==160 and read('score')==300,'ROM fixture: level-3 laser lengthens the same blade')
frames(2);shot('laser-cut')

fixture(item_count=1);item(0,45);unpause();frames(3)
check(read('laser')==1 and read('cap_count')==1 and read('laser_t')>800,'ROM fixture: capsule pickup loads level 1 and clock')
fixture(item_count=1);item(0,45,big=True);unpause();frames(3)
check(read('laser')==3 and read('chain')==4,'ROM fixture: sealed-lane capsule grants top laser and X5 chain')

fixture(enemy_count=1,laser=3,laser_t=400);enemy(0,1,46,105);unpause();frames(3)
check(read('laser')==0 and read('guard_t')>=27 and read('lives')==3 and read('enemy_count')==0,'ROM fixture: laser armour burns every level, kills contact enemy, shields ship')
fixture(laser=3,guard_t=20,ship_y=25<<8,ship_dir=-1);unpause();wait('game_state',3,40)
check(read('lives')==2,'ROM fixture: rock is fatal even with armour')

fixture(laser=3,laser_t=1,chain=4,chain_t=1);unpause();frames(3)
check(read('laser')==2 and read('chain')==3,'ROM fixture: timed laser and chain decay one step, not all at once')

# Explicit high-pool pressure scene. Inputs are only one hardware frame long.
fixture(enemy_count=24,pending_count=12,item_count=8)
put('trains',bytes([5,0,0,1,5]),5)
for i in range(24):enemy(i,(1,2,3,4,7)[i%5],210+(i%5)*8,52+(i%6)*20,group=1 if i%5==1 else 0,v=19)
for i in range(12):put('pending',bytes([200,1,104,1,0]),i*5)
for i in range(8):item(i,210+i*4,60+i*14)
unpause()
for i in range(36):frames(1,16 if i in (1,9,17) else 0)
check(read('presses')==3 and read('game_state')==2,'ROM pressure fixture: three one-frame taps captured by VBlank edge latch')
check(read('pool_drops')==0 and read('enemy_count')<=24 and read('item_count')<=8,'ROM pressure fixture: dense pools and sprite pressure remain bounded')
check(L.sms_active_writes()==0,'ROM pressure fixture: zero active-display VDP data writes')
shot('pressure')

course=json.loads((ROOT/'assets/course.json').read_text());g=course['gems'][0]
fixture(cam_x=g['wx']-44,ship_y=g['y']<<8,ship_dir=-1,next_gem=0,laser=1,laser_t=10,chain=2,chain_t=100)
unpause();frames(2)
check(read('gem_count')==1 and read('score')==300 and read('laser_t')>890,'ROM fixture: gem chain score multiplied by cut chain; laser clock refilled')

fixture(cam_x=458*8-45);unpause();frames(5)
check(read('zone')==1 and read('score')==50000,'ROM fixture: canyon exit pays no-miss bonus once')
shot('cavern')
fixture(cam_x=876*8-45,zone=1);unpause();frames(5)
check(read('zone')==2 and read('score')==50000,'ROM fixture: cavern exit reaches fortress')
shot('fortress')
fixture(cam_x=1331*8-45,zone=2);unpause();wait('boss_active',1);frames(30)
check(read('boss_hp')==24,'ROM fixture: course end starts the 24-hit gate')
shot('gate')

gate_fixture(enemy_count=1)
put('plate_state',bytes([0,0,2,0,0]));enemy(0,9,100,gate=1,row=2)
poke('paused',0);action();wait('cut_kills',1,40)
check(read('plate_state')==0 and L.sms_read(S['plate_state']+2)==3 and read('score')==500,'ROM fixture: blade opens a thrown plate, at plain gate score')
frames(12);action();frames(16)
check(read('boss_hp')==23 and read('boss_hits')==1,'ROM fixture: ordinary second button press sends wave through the opened row')
shot('gate-open')

gate_fixture(shot_active=1,shot_x=202,shot_y=104,shot_dir=1)
unpause();frames(3)
check(read('boss_hp')==24 and read('boss_hits')==0 and read('shot_active')==0,'ROM fixture: closed core deflects wave')

gate_fixture(laser=3,laser_t=420)
put('plate_state',bytes([0,0,3,0,0]));put('plate_t',struct.pack('<5H',0,0,270,0,0))
poke('paused',0);action();frames(5)
check(read('boss_hp')==23 and read('boss_hits')==1,'ROM fixture: long blade itself never damages core; wave does')

gate_fixture(boss_hp=12);unpause();frames(3)
check(read('boss_over')==1,'ROM fixture: half-health gate enters overdrive')
gate_fixture(boss_time=1);unpause();frames(3)
check(read('boss_leaving')>0 and read('boss_hp')==24,'ROM fixture: gate timeout closes and leaves, not a free victory')
shot('gate-timeout')

gate_fixture(boss_hp=1,boss_time=60,shot_active=1,shot_x=202,shot_y=104,shot_dir=1)
put('plate_state',bytes([0,0,3,0,0]));put('plate_t',struct.pack('<5H',0,0,270,0,0))
unpause();wait('game_state',5)
check(read('boss_kills')==1 and read('score')==40200,'ROM fixture: wave destroys gate; gate and loop bonuses paid')
wait('loop_no',1,500)
wait('game_state',1,80)
check(read('game_state')==1 and read('final_clear')==0 and read('laser')==0,'ROM fixture: first victory starts harder loop 2 without a false ALL CLEAR')
frames(25);shot('loop-2')

gate_fixture(loop_no=1,next_event=118,boss_hp=1,boss_time=60,shot_active=1,shot_x=202,shot_y=104,shot_dir=1,lives=2)
put('plate_state',bytes([0,0,3,0,0]));put('plate_t',struct.pack('<5H',0,0,270,0,0))
unpause();wait('game_state',5);frames(2)
check(read('boss_kills')==1 and read('score')==80200 and read('final_clear')==1,'ROM fixture: second gate is destroyed by a real wave, with doubled loop bonus')
wait('game_state',4,700)
check(read('score')==180200 and read('lives')==0 and read('final_clear')==1,'ROM fixture: second gate victory counts ships and finishes ALL CLEAR')
shot('second-gate-clear')

fixture(loop_no=1,game_state=5,final_clear=1,phase_t=1,score=190000,lives=2,extends=0)
unpause();frames(2)
check(read('score')==240000 and read('lives')==1 and read('extends')==0,'ROM fixture: ALL CLEAR pays ship bonus without awarding extra ships')
wait('game_state',4,700)
check(read('score')==290000 and read('lives')==0 and read('final_clear')==1,'ROM fixture: final tally and ALL CLEAR reach game over')
shot('all-clear')
check(read('patch_count')<160,'ROM fixture: VBlank patch queue remains bounded')
check(L.sms_active_writes()==0,'ROM fixtures: no VRAM/CRAM data writes during active display')

# SRAM window and actual game-over ranking code, on a fresh cartridge boot.
boot_play();fixture(game_state=4,phase_t=170,score=300000,lives=0)
unpause();frames(60)
raw=C.string_at(L.sms_sram(),41)
check(raw[:4]==b'ZSB1' and int.from_bytes(raw[39:41],'little')==(sum(raw[:39])&65535),'ROM fixture: BEST 5 written to SRAM with valid checksum')
L.sms_init();frames(180)
check(sum(L.sms_read(S['best_scores']+4+i)<<(8*i) for i in range(4))==300000,'ROM fixture: SRAM ranking survives an emulator reset')

# Check actual sound output without claiming that a human auditioned it.
boot_play();samples=[]
for i in range(90):
    L.sms_frame(16 if i%20==0 else 0)
    samples.extend(L.sms_audio()[j] for j in range(L.sms_audio_count()))
peak=max(map(abs,samples));check(.01<peak<1.0,'actual PSG output is non-silent and not digitally clipped')

# These next checks are host contracts, not cartridge execution.
rom_checks=checks
# Original mechanical contracts (Node is optional for rebuild-only users).
upstream_count=0
try:
    original=json.loads(subprocess.check_output(['node','tests/reference_contract.js'],cwd=ROOT))
    run(['gcc','-O2','-std=c99','-DHOST_TEST','-Isrc','tests/reference_contract.c','src/game.c','-o','build/reference-contract'])
    port=json.loads(subprocess.check_output(['build/reference-contract'],cwd=ROOT))
    upstream_count=len(original)
    check(original==port,'14 mechanical contracts match untouched upstream/core.js (normalized vertical coordinates)')
    (E/'reference-original.json').write_text(json.dumps(original,indent=2));(E/'reference-sms.json').write_text(json.dumps(port,indent=2))
except FileNotFoundError:print('Node not found: upstream comparison skipped; ROM and C rules still tested.')

# Run the actual host rule suite, including when this script is invoked directly.
host_output=subprocess.check_output(['make','rules'],cwd=ROOT,text=True)
match=re.search(r'RULES: (\d+) checks, 0 failed',host_output)
if not match:raise AssertionError('host rules did not report success')
host_count=int(match[1]);(E/'rules.log').write_text(host_output)

# Convert genuine harness screenshots, never fabricate game screenshots.
try:
    from PIL import Image
    for p in E.glob('*.ppm'):Image.open(p).save(p.with_suffix('.png'))
except ImportError:pass
report={'rom_sha256':hashlib.sha256(ROM).hexdigest(),'rom_bytes':len(ROM),'real_rom_checks':rom_checks,'host_rule_checks':host_count,'upstream_contract_scenarios':upstream_count,'active_display_vdp_writes':L.sms_active_writes(),'audio_peak':round(peak,6),'controller_only':'title / play / edge inputs / PAUSE / deaths / game over / restart / attract','ram_fixtures':'weapons, trains, capsules, gems, armour, decay, zones, gate, both-loop state transitions, tally, SRAM','not_verified':['physical SMS hardware','PAL 50 Hz timing','Power Base Converter','full human campaign playthrough','human audio audition','general emulator compatibility outside tested cores']}
(E/'verification.json').write_text(json.dumps(report,indent=2))
(E/'rom-checks.log').write_text('\n'.join(log)+'\n')
print('\nREAL ROM:',rom_checks,'checks passed. SHA-256:',report['rom_sha256'])
