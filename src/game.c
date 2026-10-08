/* Fixed-point rules port of upstream/core.js, with a 22-row playfield.
 * Camera: unsigned pixels + 8-bit fraction. Ship: 8.8. Enemies: signed 12.4.
 * No heap, floats, video access, or platform input in this file.
 */
#include "game.h"
#include <string.h>

u8 game_state,is_demo,loop_no,zone,lives,laser,chain,guard_t,paused;
s8 ship_dir,shot_dir,blade_dir;
u16 cam_x,ship_y,phase_t,sim_ticks,chain_t,laser_t;
u8 cam_sub,shot_active,shot_y,freeze_t,blade_t,blade_y,blade_len,blade_n;
u16 shot_x;
u8 boss_active,boss_hp,boss_intro,boss_leaving,boss_over,boss_flash;
u16 boss_time,boss_launch,boss_emit;
u8 plate_state[5];
u16 plate_t[5];
u8 scene_dirty,boss_dirty,hud_dirty,flash_t,flash_n,zone_lost,final_clear;
u16 flash_pts;
u8 next_event,next_gem,gem_chain,extends;
u8 gem_taken[GEM_COUNT];
u32 score,hi_score;
u16 presses,dry_turns,cut_kills,far_kills,cap_count,gem_count,deaths,boss_hits,boss_kills,pool_drops;
u8 enemy_count,pending_count,item_count,trail_count;
Enemy enemies[MAX_ENEMIES];
Pending pending[MAX_PENDING];
TrainGroup trains[MAX_TRAINS];
Item items[MAX_ITEMS];
Burst bursts[8];
Trail trails[12];
const u8 boss_row_y[5]={44,74,104,134,164};
const u16 laser_decay[4]={0,900,600,420};
const u16 chain_decay[5]={0,150,120,90,60};
const u8 blade_reach[4]={64,96,128,160};
static const u32 extend_at[4]={200000UL,600000UL,1400000UL,3000000UL};
static const u8 speed_table[2][5][5]={
    {{38,41,44,47,50},{32,35,37,40,42},{26,28,30,32,34},{90,97,104,111,118},{38,41,45,48,51}},
    {{42,45,49,52,55},{35,38,41,44,46},{28,30,33,35,37},{99,107,115,122,130},{42,45,49,52,56}}
};
u8 gift_t,emit_n,trail_next,tally_done,tally_wait;

/* Keep horizontal distances 16-bit: offscreen trains can be >255px away. */
static u16 abs16(s16 a) { return a<0 ? (u16)-a : (u16)a; }
u8 enemy_width(u8 type) { return type==PLUG ? 8 : type==SHELL||type==RUSHER ? 14 : type==9 ? 10 : 12; }
u8 enemy_halfheight(u8 type) { return type==PLUG ? 24 : type==9 ? 13 : type==SHELL ? 5 : type==RUSHER ? 3 : 4; }
u8 enemy_speed(u8 type) {
    u8 k=type==TRAIN||type==CONVOY ? 1 : type==SHELL ? 2 : type==RUSHER ? 3 : type==9 ? 4 : 0;
    if(type==PLUG) return 19;
    return speed_table[loop_no?1:0][k][chain];
}
u8 rock_col(u16 col,s8 row) {
    const Geometry *g;
    if(row<0||row>=FIELD_ROWS) return 1;
    if(col>=COURSE_END) return row<2||row>=FIELD_ROWS-2;
    g=GEO+col;
    return (u8)row<g->c||(u8)row>=FIELD_ROWS-g->f||((u8)row>=g->a&&(u8)row<g->b);
}
u8 rock_at(u16 wx,s16 y) {
    if(y<16||y>=192) return 1;
    return rock_col(wx>>3,(s8)((y-16)>>3));
}
u8 rock_hull(u16 wx,u8 y) {
    return rock_at(wx+5,y)||rock_at(wx-4,(s16)y-2)||rock_at(wx-4,(s16)y+2)||rock_at(wx+1,(s16)y-2)||rock_at(wx+1,(s16)y+2);
}
static u8 open_mid(u16 col) {
    u8 r,best=0,mid=104,start=255;
    for(r=0;r<=FIELD_ROWS;r++) {
        if(r<FIELD_ROWS&&!rock_col(col,r)) { if(start==255) start=r; }
        else if(start!=255) { if(r-start>best) { best=r-start;mid=16+(start+r)*4; } start=255; }
    }
    return mid;
}
static u8 zone_at(u16 cam) {
    u16 c=(cam+SHIP_X)>>3;
    return c<ZONE0_END ? 0 : c<ZONE1_END ? 1 : 2;
}
void add_score(u32 pts) {
    score+=pts;
    if(!is_demo&&score>hi_score) hi_score=score;
    while(extends<4&&score>=extend_at[extends]) { extends++;lives++;sound_effect(SE_EXTEND); }
    hud_dirty=1;
}
static void burst_at(s16 x,u8 y,u8 kind) {
    u8 i,slot=0;
    for(i=0;i<8;i++) if(!bursts[i].t) {slot=i;break;}
    bursts[slot].x=x;bursts[slot].y=y;bursts[slot].kind=kind;bursts[slot].t=18;
}
static void add_item(s16 x,u8 y,u8 big) {
    if(item_count<MAX_ITEMS) {Item *it=&items[item_count++];it->x=x;it->y=y;it->type=big?2:1;sound_effect(SE_CAPDROP);}
    else pool_drops++;
}
u8 spawn_enemy(u8 type,s16 x,u8 y,u8 group,u8 gate,u8 row) {
    u8 i=enemy_count;
    if(i<MAX_ENEMIES) {
        Enemy *e=&enemies[enemy_count++];e->type=type;e->x=x;e->y=y;e->v=enemy_speed(type);e->train=group;e->gate=gate;e->row=row;e->warn=type==RUSHER?20:0;return i;
    }
    pool_drops++;return 255;
}
void spawn_wave(u8 type,u8 y,u8 n,u8 carrier) {
    u8 i,g=0,free=MAX_ENEMIES-enemy_count;
    if(type==CONVOY||type==TRAIN) {
        if(free<n+(type==CONVOY)) {pool_drops++;return;}
        for(i=1;i<MAX_TRAINS;i++) if(!trains[i].active) {g=i;break;}
        if(!g) {pool_drops++;return;}
        trains[g].active=n;trains[g].n=n;trains[g].killed=0;trains[g].escaped=0;trains[g].carrier=carrier;
        if(type==CONVOY) {i=spawn_enemy(SHELL,264*16,y,0,0,0);if(i!=255) enemies[i].v=enemy_speed(TRAIN);}
        for(i=0;i<n;i++) spawn_enemy(TRAIN,(264+(u16)i*18+(type==CONVOY?26:0))*16,y,g,0,0);
    } else spawn_enemy(type,264*16,y,0,0,0);
}
static void queue_wave(u8 type,u8 y,u8 n,u8 carrier,u8 delay) {
    if(pending_count<MAX_PENDING) {Pending *p=&pending[pending_count++];p->type=type;p->y=y;p->n=n;p->carrier=carrier;p->delay=delay;}
    else pool_drops++;
}
static void remove_enemy(u8 i,u8 escaped) {
    Enemy *e=&enemies[i];
    if(e->train) {
        TrainGroup *t=&trains[e->train];
        if(escaped) t->escaped=1;
        if(t->active) t->active--;
    }
    if(e->type==9&&boss_active&&escaped) {plate_state[e->row]=CLOSED;plate_t[e->row]=0;boss_dirty=1;}
    enemy_count--;
    if(i!=enemy_count)enemies[i]=enemies[enemy_count];
    enemies[enemy_count].type=0;
}
static void kill_enemy(u8 i,u8 how,u8 order,u8 mult,u8 near) {
    Enemy *e=&enemies[i];
    u16 pts=how==1 ? (e->type==SHELL||e->type==9||e->type==PLUG?500:e->type==RUSHER?600:300) : (e->type==RUSHER?200:e->type==9||e->type==PLUG||e->type==SHELL?200:100);
    if(!e->gate) {
        if(how==1) {pts*=mult;if(order>3) order=3;pts<<=order;if(near) pts<<=1;}
    }
    add_score(pts);
    if(how==1) cut_kills++;else if(how==0) far_kills++;
    burst_at(e->x>>4,e->y,how==1?0:1);
    if(e->type==9&&boss_active) {plate_state[e->row]=OPEN;plate_t[e->row]=270;boss_dirty=1;}
    if(e->train) {
        TrainGroup *t=&trains[e->train];t->killed++;
        if(t->killed==t->n&&!t->escaped&&t->carrier) add_item(e->x,e->y,0);
    }
    remove_enemy(i,0);
    flash_pts=pts;flash_t=24;
    if(how!=1) sound_effect(SE_FAR);
}
static u8 in_band(const Enemy *e,u8 y,s8 dir) {
    s16 d;
    if(!e->type||e->warn) return 0;
    if(e->type==PLUG) return abs16((s16)e->y-y)<=24;
    d=(s16)e->y-y;if(dir<0) d=-d;
    return d>=-(BACK+enemy_halfheight(e->type))&&d<=AHEAD+enemy_halfheight(e->type);
}
static u8 blade_length(u8 y,s8 dir,u8 max) {
    u8 d=0;
    s16 ym=(s16)y+(dir<0?-MID:MID);
    while(d<max) {if(rock_at(cam_x+NOSE_X+d,ym)) break;d+=2;}
    return d>max?max:d;
}
void game_press(void) {
    u8 y=ship_y>>8,len,max,i,best,k=0,near_any=0,mult=chain+1;
    s8 dir=ship_dir;
    s16 best_x;
    ship_dir=-ship_dir;presses++;
    if(shot_active) {dry_turns++;sound_effect(SE_TURN);return;}
    max=blade_reach[laser];len=blade_length(y,dir,max);
    blade_y=y;blade_dir=dir;blade_len=len;blade_t=7;blade_n=0;
    for(;;) {
        best=255;best_x=32767;
        for(i=0;i<enemy_count;i++) {
            Enemy *e=&enemies[i];s16 x=e->x>>4;u8 w=enemy_width(e->type)>>1;
            if(in_band(e,y,dir)&&x+w>=NOSE_X-4&&x-w<=NOSE_X+len&&e->x<best_x) {best=i;best_x=e->x;}
        }
        if(best==255) break;
        i=((enemies[best].x>>4)-(enemy_width(enemies[best].type)>>1)<=NOSE_X+24);
        if(i) near_any=1;
        kill_enemy(best,1,k,mult,i);k++;
    }
    if(k) {
        if(chain<4) chain++;
        chain_t=chain_decay[chain];if(near_any) chain_t+=chain_t>>1;
        blade_n=k;flash_n=k;freeze_t=2+k*2+(near_any?2:0);if(freeze_t>10) freeze_t=10;
        sound_effect(SE_CUT_0+(k+chain-2>4?4:k+chain-2));hud_dirty=1;
    } else {
        shot_active=1;shot_x=NOSE_X+len;shot_y=y;shot_dir=dir;
        sound_effect(SE_SHOT);
    }
}
static void zone_pay(u8 z) {
    u8 i,all=1,any=0;
    for(i=0;i<GEM_COUNT;i++) if(GEMS[i].zone==z) {any=1;if(!gem_taken[i]) all=0;}
    add_score((zone_lost?10000UL:50000UL)+(all&&any?100000UL:0));
    zone_lost=0;sound_effect(SE_ZONE);
}
static void end_loop(u8 won) {
    game_state=CLEAR;phase_t=200;final_clear=won;shot_active=0;freeze_t=10;blade_t=0;
    enemy_count=pending_count=item_count=0;
    memset(enemies,0,sizeof(enemies));memset(pending,0,sizeof(pending));memset(items,0,sizeof(items));
    if(won) {add_score(20000UL*(loop_no+1));sound_effect(SE_LOOP);}else sound_effect(SE_GAMEOVER);
    tally_done=0;tally_wait=0;scene_dirty=1;hud_dirty=1;
}
void boss_hurt(void) {
    u8 i,n=0;
    if(!boss_active||boss_leaving||boss_intro||!boss_hp) return;
    for(i=0;i<5;i++) if(plate_state[i]==OPEN) n++;
    if(!n) n=1;
    boss_hp--;boss_hits++;boss_flash=8;boss_dirty=1;add_score((u16)200*n);sound_effect(SE_HEAVY);
    burst_at(212,ship_y>>8,0);
    if(!boss_hp) {
        boss_kills++;add_score((20000UL+(u32)(boss_time/60)*5000UL)*(loop_no+1));
        boss_active=0;end_loop(1);
    }
}
void boss_begin(void) {
    boss_active=1;boss_hp=24;boss_time=4500;boss_intro=60;boss_leaving=0;boss_over=0;boss_flash=0;
    boss_launch=60;boss_emit=100;emit_n=0;
    memset(plate_state,0,sizeof(plate_state));memset(plate_t,0,sizeof(plate_t));
    enemy_count=pending_count=0;
    memset(pending,0,sizeof(pending));memset(enemies,0,sizeof(enemies));memset(trains,0,sizeof(trains));
    shot_active=0;cam_x&=0xfff8;cam_sub=0;scene_dirty=1;boss_dirty=1;hud_dirty=1;sound_effect(SE_WARN);
}
static u8 nearest_plate(u8 skip,u8 required) {
    u8 i,best=255,dist=255,d,y=ship_y>>8;
    for(i=0;i<5;i++) if(i!=skip&&plate_state[i]==required) {d=abs16((s16)boss_row_y[i]-y);if(d<dist) {dist=d;best=i;}}
    return best;
}
static void boss_step(void) {
    u8 i,j,k,busy=0,n,slot;
    if(boss_flash) boss_flash--;
    if(boss_intro) {boss_intro--;return;}
    if(boss_leaving) {if(!--boss_leaving) {boss_active=0;end_loop(0);}return;}
    if(boss_time) boss_time--;
    if(!boss_time) {
        boss_leaving=100;memset(plate_state,0,sizeof(plate_state));
        for(i=0;i<enemy_count;i++) if(enemies[i].type==9) {remove_enemy(i,1);i--;}
        boss_dirty=1;hud_dirty=1;sound_effect(SE_WARN);return;
    }
    if(boss_hp<=12&&!boss_over) {boss_over=1;boss_dirty=1;sound_effect(SE_WARN);}
    n=0;
    for(i=0;i<5;i++) {
        if(plate_state[i]==ARMING) {
            busy=1;
            if(plate_t[i]&&!--plate_t[i]) {
                slot=spawn_enemy(9,(BOSS_FACE-4)*16,boss_row_y[i],0,1,i);
                plate_state[i]=slot==255?CLOSED:FLYING;boss_dirty=1;
                if(!n&&slot!=255) {
                    /* One escort file per two-plate launch, from the next-nearest row. */
                    j=0;for(k=0;k<5;k++) if(k!=i&&(j==i||abs16((s16)boss_row_y[k]-(ship_y>>8))<abs16((s16)boss_row_y[j]-(ship_y>>8)))) j=k;
                    for(k=0;k<3+loop_no+boss_over;k++) spawn_enemy(DRONE,(BOSS_FACE+10+(u16)k*18)*16,boss_row_y[j],0,1,0);
                    n=1;
                }
            }
        } else if(plate_state[i]==FLYING) busy=1;
        else if(plate_state[i]==OPEN&&plate_t[i]&&!--plate_t[i]) {plate_state[i]=CLOSED;boss_dirty=1;}
    }
    if(!busy) {
        if(boss_launch) boss_launch--;
        if(!boss_launch) {
            i=nearest_plate(255,CLOSED);j=nearest_plate(i,CLOSED);
            if(i!=255) {plate_state[i]=ARMING;plate_t[i]=30;}
            if(j!=255) {plate_state[j]=ARMING;plate_t[j]=30;}
            boss_launch=boss_over?40:(loop_no?64:70);boss_dirty=1;
        }
    }
    if(boss_emit) boss_emit--;
    if(!boss_emit) {
        for(k=0;k<5;k++) {
            i=emit_n+k;if(i>=5) i-=5;
            if(plate_state[i]==OPEN) {for(j=0;j<2+loop_no;j++) spawn_enemy(DRONE,(BOSS_FACE+(u16)j*18)*16,boss_row_y[i],0,1,0);emit_n=i+1;if(emit_n>=5)emit_n=0;break;}
        }
        boss_emit=loop_no?90:100;
    }
}
static void die(void) {
    game_state=DEAD;phase_t=80;if(lives) lives--;zone_lost++;deaths++;guard_t=0;
    burst_at(SHIP_X,ship_y>>8,1);sound_effect(SE_DEATH);freeze_t=6;scene_dirty=1;hud_dirty=1;
}
void game_place(u16 cam) {
    const CourseEvent *ev;
    if(loop_no)ev=(const CourseEvent *)ROM_PTR(EVENT1_ADDR);else ev=(const CourseEvent *)ROM_PTR(EVENT0_ADDR);
    u8 total=loop_no?EVENT1_COUNT:EVENT0_COUNT;
    cam_x=cam;cam_sub=0;ship_y=(u16)open_mid((cam+SHIP_X)>>3)<<8;ship_dir=-1;
    enemy_count=pending_count=item_count=0;
    memset(enemies,0,sizeof(enemies));memset(pending,0,sizeof(pending));memset(items,0,sizeof(items));memset(trains,0,sizeof(trains));
    memset(bursts,0,sizeof(bursts));memset(trails,0,sizeof(trails));trail_next=trail_count=0;
    shot_active=0;laser=0;laser_t=0;chain=0;chain_t=0;guard_t=0;gem_chain=0;blade_t=0;freeze_t=0;flash_t=0;
    next_event=0;while(next_event<total&&ev[next_event].wx<cam_x+304) next_event++;
    next_gem=0;while(next_gem<GEM_COUNT&&GEMS[next_gem].wx<cam_x+SHIP_X-8) next_gem++;
    zone=zone_at(cam);scene_dirty=1;hud_dirty=1;gift_t=20;
}
void game_new(u8 demo) {
    is_demo=demo;loop_no=0;zone=0;zone_lost=0;boss_active=0;boss_leaving=0;boss_hp=0;boss_intro=0;
    lives=3;score=0;extends=0;game_state=READY;phase_t=demo?40:110;paused=0;sim_ticks=0;final_clear=0;
    presses=dry_turns=cut_kills=far_kills=cap_count=gem_count=deaths=boss_hits=boss_kills=pool_drops=0;
    memset(gem_taken,0,sizeof(gem_taken));game_place(START_CAM);sound_effect(SE_START);
}
static u8 roomy(u16 cam,u8 y) {
    u16 col,endcol=(cam+SHIP_X+80)>>3;
    s8 r;
    for(col=(cam+SHIP_X-12)>>3;col<=endcol;col++) for(r=((s16)y-30-16)>>3;r<=((s16)y+30-16)>>3;r++) if(rock_col(col,r)) return 0;
    return 1;
}
static void rewind_ship(void) {
    u16 first=boss_active ? (COURSE_END+4)*8-SHIP_X : START_CAM,cam;
    u8 old_zone=zone,i;
    cam=cam_x>first+144?(cam_x-144)&0xfff8:first;
    while(cam>first&&!roomy(cam,open_mid((cam+SHIP_X)>>3))) cam-=8;
    if(cam<first) cam=first;
    if(boss_active) for(i=0;i<5;i++) if(plate_state[i]!=OPEN) {plate_state[i]=CLOSED;plate_t[i]=0;}
    game_place(cam);if(old_zone>zone)zone=old_zone;
    if(boss_active) {cam_x&=0xfff8;gift_t=0;boss_dirty=1;}
    game_state=READY;phase_t=60;sound_effect(SE_START);
}
static u8 event_row(const CourseEvent *e,u8 target) {
    u8 a,b,d;
    if(!e->lo&&!e->lo2) return 255;
    a=target<e->lo?e->lo:target>e->hi?e->hi:target;
    if(!e->lo) a=255;
    if(e->lo2) {b=target<e->lo2?e->lo2:target>e->hi2?e->hi2:target;if(a==255||abs16((s16)b-target)<abs16((s16)a-target))a=b;}
    d=abs16((s16)a-target);return d<=41?a:255;
}
static void script_step(void) {
    const CourseEvent *ev;
    if(loop_no)ev=(const CourseEvent *)ROM_PTR(EVENT1_ADDR);else ev=(const CourseEvent *)ROM_PTR(EVENT0_ADDR);
    u8 total=loop_no?EVENT1_COUNT:EVENT0_COUNT,i,k,y;
    if(gift_t&&!--gift_t) {
        s16 yy=(ship_y>>8)+((ship_y>>8)>104?-34:34);
        if(yy<32)yy=32;if(yy>176)yy=176;
        queue_wave(TRAIN,(u8)yy,loop_no?4:3,1,40);
    }
    while(next_event<total&&ev[next_event].wx<=cam_x+304) {
        const CourseEvent *e=&ev[next_event++];
        if(e->type==PLUG) {spawn_enemy(PLUG,(e->wx-cam_x)*16,e->y,0,0,0);continue;}
        if(e->type==CAPITEM) {add_item((e->wx-cam_x)*16,e->y,1);continue;}
        y=e->type==AIMED||e->type==RUSHER?event_row(e,ship_y>>8):e->y;
        if(y==255)continue;
        if(e->type==AIMED||e->type==RUSHER) {for(k=0;k<e->n;k++)queue_wave(e->type==AIMED?DRONE:RUSHER,y,1,0,40+k*12);}
        else queue_wave(e->type,y,e->n,e->carrier,40);
        for(k=0;k<(chain>>1);k++) queue_wave(DRONE,y,1,0,54+k*12);
    }
    for(i=0;i<pending_count;i++) if(pending[i].type) {
        Pending *p=&pending[i];
        if(p->delay) p->delay--;
        if(!p->delay) {spawn_wave(p->type,p->y,p->n,p->carrier);pending_count--;pending[i]=pending[pending_count];pending[pending_count].type=0;i--;}
    }
}
static void shot_step(void) {
    u8 i,hit=255,open=0;
    u16 x0=shot_x;
    s16 closest=32767,d;
    shot_x+=14;
    for(i=0;i<enemy_count;i++) {
        Enemy *e=&enemies[i];s16 x=e->x>>4;u8 w=enemy_width(e->type)>>1;
        if(in_band(e,shot_y,shot_dir)&&x+w>=x0&&x-w<=shot_x&&e->x<closest) {hit=i;closest=e->x;}
    }
    if(hit!=255) {
        Enemy *e=&enemies[hit];shot_active=0;
        if(e->type==SHELL||e->type==9||e->type==PLUG) {sound_effect(SE_PING);burst_at(e->x>>4,shot_y,0);}
        else kill_enemy(hit,0,0,1,0);
    } else if(boss_active&&!boss_intro&&!boss_leaving&&shot_x>=BOSS_FACE) {
        shot_active=0;
        for(i=0;i<5;i++) {d=(s16)boss_row_y[i]-shot_y;if(shot_dir<0)d=-d;if(d>=-(BACK+13)&&d<=AHEAD+13&&plate_state[i]==OPEN)open=1;}
        if(open) boss_hurt();else sound_effect(SE_PING);
    } else if(shot_x>260) shot_active=0;
}
void game_step(u8 pressed) {
    u8 i,y,z,oldsub,scroll=19;
    sim_ticks++;
    if(game_state==READY) {if(phase_t&&!--phase_t) {game_state=PLAY;scene_dirty=1;hud_dirty=1;}return;}
    if(game_state==DEAD) {if(phase_t&&!--phase_t) {if(!lives) {game_state=OVER;phase_t=170;scene_dirty=1;}else rewind_ship();}return;}
    if(game_state==CLEAR) {
        if(phase_t&&!--phase_t) {
            if(loop_no==1) {
                if(final_clear&&lives) {lives--;score+=50000UL;if(!is_demo&&score>hi_score)hi_score=score;tally_done++;phase_t=26;sound_effect(SE_CAPDROP);hud_dirty=1;scene_dirty=1;return;}
                if(final_clear&&!tally_wait) {tally_wait=1;phase_t=110;return;}
                game_state=OVER;phase_t=170;scene_dirty=1;
            } else {loop_no=1;boss_active=0;zone_lost=0;final_clear=0;memset(gem_taken,0,sizeof(gem_taken));game_place(START_CAM);game_state=READY;phase_t=110;sound_effect(SE_START);}
        }
        return;
    }
    if(game_state!=PLAY)return;
    if(pressed)game_press();
    if(ship_dir<0)ship_y-=347;else ship_y+=347;
    y=ship_y>>8;
    if(!boss_active) {
        oldsub=cam_sub;cam_sub+=51;cam_x+=1;if(cam_sub<oldsub)cam_x++;
        scroll=16+(cam_sub<oldsub?16:0)+(cam_sub>>4)-(oldsub>>4);
        script_step();
    }
    if(guard_t)guard_t--;
    if(boss_active) {boss_step();if(game_state!=PLAY)return;}
    if(chain&&chain_t&&!--chain_t) {chain--;chain_t=chain_decay[chain];hud_dirty=1;}
    if(laser&&laser_t&&!--laser_t) {laser--;laser_t=laser_decay[laser];hud_dirty=1;}
    for(i=0;i<enemy_count;i++) if(enemies[i].type) {
        Enemy *e=&enemies[i];
        if(e->warn) {e->warn--;e->x=250*16;continue;}
        e->x-=e->type==PLUG?scroll:e->v;
        if(e->x<-12*16) {remove_enemy(i,1);i--;}
        else if(rock_at(cam_x+(e->x>>4)-(enemy_width(e->type)>>1),e->y)) {burst_at(e->x>>4,e->y,0);remove_enemy(i,1);i--;}
    }
    if(shot_active) {shot_step();if(game_state!=PLAY)return;}
    for(i=0;i<item_count;i++) if(items[i].type) {
        Item *it=&items[i];s16 x;
        it->x-=scroll;x=it->x>>4;
        if(abs16(x-SHIP_X)<14&&abs16((s16)it->y-y)<11) {
            u8 full=laser>=3,big=it->type==2;it->type=0;cap_count++;
            if(big) {laser=3;chain=4;chain_t=chain_decay[4];}else if(laser<3)laser++;
            laser_t=laser_decay[laser];add_score(full||big?1000:300);sound_effect(SE_CAP);burst_at(SHIP_X,y,0);item_count--;items[i]=items[item_count];items[item_count].type=0;i--;
        } else if(x<-10) {item_count--;items[i]=items[item_count];items[item_count].type=0;i--;}
    }
    while(next_gem<GEM_COUNT&&GEMS[next_gem].wx+8<cam_x) {if(!gem_taken[next_gem])gem_chain=0;next_gem++;}
    for(i=next_gem;i<GEM_COUNT;i++) {
        const Gem *g=&GEMS[i];s16 x=(s16)(g->wx-cam_x);
        if(x>SHIP_X+14)break;
        if(!gem_taken[i]&&abs16(x-SHIP_X)<12&&abs16((s16)g->y-y)<9) {
            gem_taken[i]=1;if(gem_chain<8)gem_chain++;gem_count++;add_score((u16)100*gem_chain*(1+chain));
            if(laser)laser_t=laser_decay[laser];sound_effect(SE_GEM_1+gem_chain-1);burst_at(x,g->y,2);
        }
    }
    if(rock_hull(cam_x+SHIP_X,y)) {die();return;}
    for(i=0;i<enemy_count;i++) if(enemies[i].type&&!enemies[i].warn) {
        Enemy *e=&enemies[i];s16 x=e->x>>4;
        if(abs16(x-SHIP_X)<(enemy_width(e->type)>>1)+3&&abs16((s16)e->y-y)<enemy_halfheight(e->type)+1) {
            if(!guard_t&&laser) {laser=0;guard_t=30;sound_effect(SE_GUARD);freeze_t=3;hud_dirty=1;}
            if(guard_t) {kill_enemy(i,2,0,1,0);i--;}else {die();return;}
        }
    }
    z=zone_at(cam_x);
    if(z>zone) {zone_pay(zone);zone=z;hud_dirty=1;}
    if(!boss_active&&((cam_x+SHIP_X)>>3)>=COURSE_END+6) {zone_pay(zone);boss_begin();}
    if((sim_ticks&3)==0) {trails[trail_next].x=SHIP_X-9;trails[trail_next].y=y;if(trail_count<12)trail_count++;trail_next++;if(trail_next>=12)trail_next=0;}
}
void feedback_step(void) {
    u8 i;
    if(blade_t)blade_t--;
    if(flash_t)flash_t--;
    for(i=0;i<8;i++)if(bursts[i].t) {bursts[i].t--;bursts[i].x--;if(bursts[i].y>18)bursts[i].y--;}
    if(game_state==PLAY)for(i=0;i<trail_count;i++)if(trails[i].x>0)trails[i].x--;
}
/* Small real-rules attract pilot. It steers around a 24-frame rock forecast
 * and waits for a safe cut. It has no power to teleport or bypass collisions.
 */
u8 game_pilot(void) {
    u8 i,y=ship_y>>8;
    s16 next=(s16)y+(ship_dir<0?-25:25),alternate=(s16)y+(ship_dir<0?16:-16);
    if(rock_hull(cam_x+SHIP_X+22,(u8)next)||rock_hull(cam_x+SHIP_X+8,(u8)((s16)y+(ship_dir<0?-12:12))))return 1;
    if(!shot_active)for(i=0;i<enemy_count;i++) {
        Enemy *e=&enemies[i];s16 x=e->x>>4;
        if(in_band(e,y,ship_dir)&&x>NOSE_X+12&&x<(s16)NOSE_X+blade_reach[laser]-4&&!rock_hull(cam_x+SHIP_X+16,(u8)alternate))return 1;
    }
    if(boss_active&&!shot_active&&!rock_hull(cam_x+SHIP_X+16,(u8)alternate)) {
        for(i=0;i<5;i++) {s16 d=(s16)boss_row_y[i]-y;if(ship_dir<0)d=-d;if(plate_state[i]==OPEN&&d>=-(BACK+13)&&d<=AHEAD+13)return 1;}
    }
    return 0;
}
