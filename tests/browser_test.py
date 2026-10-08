#!/usr/bin/env python3
"""Optional offline / Chromium UI checks. Requires Python playwright + Chromium.
RAM writes in the SRAM test are explicitly labelled fixtures, not playthroughs.
MIT, 2026 Zig Saber SMS port contributors.
"""
import argparse
import base64
import hashlib
import json
import re
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]
E=ROOT/'evidence'
S={m[1]:int(m[2],16) for m in re.finditer(r'DEF\s+_(\w+)\s+0x([0-9A-Fa-f]+)',(ROOT/'zig-saber.noi').read_text())}
ROM=(ROOT/'zig-saber.sms').read_bytes()
parser=argparse.ArgumentParser();parser.add_argument('--url',default=(ROOT/'PLAY.html').as_uri());args=parser.parse_args()
checks=[]
def check(ok,label):
    if not ok:raise AssertionError(label)
    checks.append(label);print('PASS',label)
def read(page,name,width=1):
    return page.evaluate('([a,w])=>{let n=0;for(let i=0;i<w;i++)n+=emu.sms_read(a+i)*2**(8*i);return n}',[S[name],width])
def wait(page,name,value,width=1):
    page.wait_for_function('([a,w,v])=>{if(!ready)return false;let n=0;for(let i=0;i<w;i++)n+=emu.sms_read(a+i)*2**(8*i);return n===v}',arg=[S[name],width,value],timeout=15000)

with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,args=['--no-sandbox'])
    context=browser.new_context(viewport={'width':1360,'height':1080},accept_downloads=True)
    page=context.new_page();errors=[];requests=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.on('request',lambda r:requests.append(r.url))
    page.goto(args.url);wait(page,'game_state',0)
    check(page.locator('#status').inner_text()=='TITLE','HTML boots the actual cartridge to TITLE')
    check(page.locator('#action').is_enabled(),'action button becomes available')
    check(hashlib.sha256(base64.b64decode(page.evaluate('ROM64'))).digest()==hashlib.sha256(ROM).digest(),'embedded ROM is byte-identical to native download')
    page.screenshot(path=str(E/'browser-desktop.png'),full_page=True)
    page.keyboard.down('Space')
    page.wait_for_function('(a)=>ready&&emu.sms_read(a)===2',arg=S['game_state'],timeout=15000)
    page.wait_for_function('(a)=>emu.sms_read(a)+256*emu.sms_read(a+1)>316',arg=S['cam_x'],timeout=15000)
    check(read(page,'presses',2)==0,'holding START does not repeat or fire at READY transition')
    page.keyboard.up('Space');page.wait_for_timeout(60)
    page.keyboard.press('Space');wait(page,'presses',1,2)
    check(read(page,'ship_dir')==1,'keyboard tap turns the native ship')
    check(page.evaluate('ac&&ac.state')=='running','user gesture starts WebAudio')
    page.keyboard.press('KeyP');wait(page,'paused',1)
    y=read(page,'ship_y',2);cam=read(page,'cam_x',2)
    page.wait_for_timeout(300)
    check(y==read(page,'ship_y',2) and cam==read(page,'cam_x',2),'P / console NMI pauses native physics')
    page.keyboard.press('Space');wait(page,'paused',0)
    check(read(page,'presses',2)==1,'resume button is consumed without firing')
    page.locator('#expand').click();check('expanded' in page.locator('#machine').get_attribute('class'),'expand control works')
    page.locator('#expand').click()
    page.locator('#sound').click();check('OFF' in page.locator('#sound').inner_text(),'sound toggle works')
    page.locator('#reset').click();wait(page,'game_state',0)
    check(read(page,'presses',2)==0,'reset reboots native ROM state')
    with page.expect_download() as download_info:page.locator('#download').click()
    download=download_info.value;download.save_as(str(E/'browser-download.sms'))
    check((E/'browser-download.sms').read_bytes()==ROM,'browser download returns the exact cartridge bytes')
    (E/'browser-download.sms').unlink()
    # Browser storage fixture: real cartridge game-over writes, not fake UI data.
    writes={S['game_state']:[4],S['phase_t']:[170,0],S['score']:list((300000).to_bytes(4,'little')),S['lives']:[0],S['paused']:[0],S['is_demo']:[0],S['hud_dirty']:[1],S['scene_dirty']:[1]}
    page.evaluate('(writes)=>{let ram=new Uint8Array(emu.memory.buffer,emu.sms_ram(),8192);for(const [a,v] of Object.entries(writes))ram.set(v,Number(a)&8191)}',{str(k):v for k,v in writes.items()})
    page.wait_for_function("()=>localStorage.getItem('zig-saber-sms-sram-v1')!==null",timeout=15000)
    saved=base64.b64decode(page.evaluate("localStorage.getItem('zig-saber-sms-sram-v1')"))
    check(saved[:4]==b'ZSB1' and int.from_bytes(saved[39:41],'little')==sum(saved[:39])&65535,'SRAM fixture: native ranking serialized to browser storage')
    page.reload();wait(page,'game_state',0)
    check(read(page,'best_scores',4)==500000 and page.evaluate('(a)=>emu.sms_read(a)+256*emu.sms_read(a+1)+65536*emu.sms_read(a+2)+16777216*emu.sms_read(a+3)',S['best_scores']+4)==300000,'SRAM fixture: ranking survives page reload')
    check(not errors,'no uncaught JavaScript errors')
    page.close()
    # A separate file:// session confirms no web server or asset fetch is needed.
    offline=context.new_page();offline_requests=[]
    offline.on('request',lambda r:offline_requests.append(r.url))
    offline.goto((ROOT/'PLAY.html').as_uri());wait(offline,'game_state',0)
    check(all(u.startswith('file:') for u in offline_requests),'offline file loads without external resources')
    offline.close()
    mobile_context=browser.new_context(viewport={'width':390,'height':844},device_scale_factor=2,is_mobile=True,has_touch=True)
    mobile=mobile_context.new_page();mobile.goto(args.url);wait(mobile,'game_state',0)
    check(mobile.evaluate('document.documentElement.scrollWidth<=innerWidth'),'mobile layout has no horizontal overflow')
    mobile.screenshot(path=str(E/'browser-mobile.png'),full_page=True)
    mobile.locator('#action').tap();wait(mobile,'game_state',1)
    check(mobile.locator('#action').inner_text()=='TURN + FIRE','touch control starts the actual cartridge')
    mobile_context.close()
    fallback=context.new_page()
    fallback.add_init_script("Object.defineProperty(window,'WebAssembly',{value:undefined})")
    fallback.goto(args.url);fallback.wait_for_function("()=>document.getElementById('status').textContent==='PLAYER UNAVAILABLE'")
    check(fallback.locator('#download').get_attribute('href').startswith('data:application/octet-stream;base64,'),'ROM download remains usable if WebAssembly fails')
    fallback.close()
    storage=context.new_page()
    storage.add_init_script("Object.defineProperty(window,'localStorage',{get(){throw new Error('storage blocked')}})")
    storage.goto(args.url);wait(storage,'game_state',0)
    check(storage.locator('#save-state').inner_text()=='Session only','blocked storage degrades to a playable session')
    browser.close()

report={'browser':'Chromium / Playwright','checks_passed':len(checks),'rom_sha256':hashlib.sha256(ROM).hexdigest(),'checks':checks,'save_checks_use_explicit_ram_fixtures':True}
(E/'browser-verification.json').write_text(json.dumps(report,indent=2))
print('BROWSER:',len(checks),'checks passed')
