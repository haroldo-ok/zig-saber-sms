// Mechanical contracts from the untouched upstream JS core. This is not a
// tick-for-tick position oracle: the SMS has a quantized, 22-row playfield.
const ZS=require('../upstream/core.js');
function reset(){const s=ZS.newGame();s.phase='play';s.camX=0;s.ship.y=120;s.ship.dir=1;s.nextEv=s.course.events.length;s.nextGem=s.course.gems.length;return s;}
function enemy(s,type,x,y,extra={}){const k=ZS.KIND[type];s.enemies.push({type,x,y,v:k.v,w:k.w,h:k.h,warn:0,age:99,train:0,...extra});}
function gate(s){s.boss=ZS.newBoss(s);s.boss.x=208;s.boss.launchT=200;s.boss.emitT=200;}
function result(name,s){return {name,dir:s.ship.dir,shot:+!!s.shot,laser:s.laser,chain:s.chain,score:s.score,lives:s.lives,cuts:s.stats.cut,fars:s.stats.far,caps:s.stats.caps,gems:s.stats.gems,guard:s.guardT,items:s.items.length,enemies:s.enemies.length,hp:s.boss?s.boss.hp:0,hits:s.stats.bossHits,phase:s.phase,laserT:s.laserT,chainT:s.chainT};}
const out=[];let s;
s=reset();ZS.step(s,true);out.push(result('empty press',s));
s=reset();s.shot={x:116,y:120,d:1};ZS.step(s,true);out.push(result('dry turn',s));
s=reset();enemy(s,'drone',70,120);ZS.step(s,true);out.push(result('point blank',s));
s=reset();s.trains[1]={n:3,killed:0,escaped:false,carrier:true};[83,101,119].forEach(x=>enemy(s,'train',x,120,{train:1}));ZS.step(s,true);out.push(result('carrier multi cut',s));
s=reset();enemy(s,'shell',94,120);ZS.step(s,true);out.push(result('shell cut',s));
s=reset();enemy(s,'shell',143,126);s.shot={x:130,y:120,d:1};ZS.step(s,false);out.push(result('armour deflection',s));
s=reset();s.laser=3;s.laserT=400;enemy(s,'drone',44,121);ZS.step(s,false);out.push(result('laser armour',s));
s=reset();s.laser=3;s.laserT=1;s.chain=4;s.chainT=1;ZS.step(s,false);out.push(result('decay',s));
s=reset();s.items.push({type:'cap',x:44,y:120});ZS.step(s,false);out.push(result('capsule',s));
s=reset();s.items.push({type:'cap',x:44,y:120,big:true});ZS.step(s,false);out.push(result('sealed lane capsule',s));
s=reset();gate(s);s.boss.plates[2]={s:'fly',t:0};enemy(s,'plate',100,120,{plate:2,gate:true});ZS.step(s,true);out.push(result('gate plate cut',s));
s=reset();gate(s);s.boss.plates[2]={s:'open',t:270};s.laser=3;s.laserT=420;ZS.step(s,true);out.push(result('wave into open core',s));
s=reset();gate(s);s.shot={x:202,y:120,d:1};ZS.step(s,false);out.push(result('closed core deflects',s));
s=reset();gate(s);s.boss.plates[2]={s:'open',t:270};s.boss.hp=1;s.boss.left=3600;s.shot={x:202,y:120,d:1};ZS.step(s,false);out.push(result('gate victory and extend',s));
process.stdout.write(JSON.stringify(out,null,2));
