/* Tile/sprite presentation. Everything before render_commit is RAM-only.
 * All VDP writes occur in VBlank, or with display explicitly disabled.
 * Incoming terrain columns use a circular nametable and hardware scrolling.
 */
#include "ui.h"
#include "SMSlib.h"
#include <string.h>

u8 display_off,patch_count;
static u16 shadow[768],column[22],hud[64],old_hud[64];
typedef struct {u16 addr,tile;} Patch;
static Patch patches[160];
static u8 patch_read,patch_write,edge_pending,hud_mask,title_blink,sprite_rotation;
static u16 last_col,incoming_col;
static const u32 powers[7]={1000000UL,100000UL,10000UL,1000UL,100UL,10UL,1UL};
static const char *const zone_names[3]={"CANYON","CAVERN","FORTRESS"};

static void disp_off(void) {SMS_displayOff();display_off=1;}
static void disp_on(void) {SMS_displayOn();display_off=0;}
static u16 font(char c) {return FONT_BASE+(u8)c-32;}
static void text(u16 *buf,u8 col,const char *s) {while(*s&&col<32)buf[col++]=font(*s++);}
static void number(u16 *buf,u8 col,u32 value) {
    u8 i,d;
    if(value>9999999UL)value=9999999UL;
    for(i=0;i<7;i++) {d=0;while(value>=powers[i]) {value-=powers[i];d++;}buf[col+i]=font('0'+d);}
}
static void two_digits(u16 *buf,u8 col,u16 n) {
    u8 tens=0;
    while(n>=10) {tens++;n-=10;}
    buf[col]=font('0'+tens);buf[col+1]=font('0'+n);
}
/* Classify exposed faces directly from four height boundaries. Calling a
 * generic solid predicate five times per cell costs dozens of frames during
 * a full-screen repaint on SDCC/Z80; these equivalent comparisons do not. */
static void make_column(u16 col) {
    const Geometry *g;
    u8 c,f,a,b,lc,lf,la,lb,rc,rf,ra,rb;
    u8 y,mask,z=col<ZONE0_END?0:col<ZONE1_END?1:2;
    u16 tile,base=ROCK_BASE+((u16)z<<5);
    u16 *out=shadow+64+(col&31);
    c=lc=rc=2;f=lf=rf=20;a=b=la=lb=ra=rb=0;
    if(col<COURSE_END) {g=GEO+col;c=g->c;f=22-g->f;a=g->a;b=g->b;}
    if(col&&col-1<COURSE_END) {g=GEO+col-1;lc=g->c;lf=22-g->f;la=g->a;lb=g->b;}
    if(col+1<COURSE_END) {g=GEO+col+1;rc=g->c;rf=22-g->f;ra=g->a;rb=g->b;}
    for(y=0;y<22;y++) {
        if(y<c||y>=f||(y>=a&&y<b)) {
            mask=0;
            if(y==f||(a<b&&y==a))mask|=1;
            if(y+1==c||(a<b&&y+1==b))mask|=2;
            if(!(y<lc||y>=lf||(y>=la&&y<lb)))mask|=4;
            if(!(y<rc||y>=rf||(y>=ra&&y<rb)))mask|=8;
            tile=base+((u16)mask<<1)+(((u8)col^y)&1);
        } else if(((((u8)col*13+y*7)^((u8)(col>>3)))&63)==0)tile=STAR_BASE+((col+y)&3);
        else tile=0;
        column[y]=tile;*out=tile;out+=32;
    }
}
static void scene_text(u8 x,u8 y,const char *s) {
    u8 col=((cam_x>>3)+x)&31;
    while(*s) {shadow[(u16)y*32+col]=font(*s++);col=(col+1)&31;}
}
static void center_text(u8 y,const char *s) {scene_text((32-strlen(s))>>1,y,s);}
static u16 boss_tile(u8 x,u8 y) {
    u8 i,c=12,state=255,hp_rows;
    s16 d;
    if(x>=28) {
        if(x==30&&y>=7&&y<15) {
            hp_rows=(boss_hp+2)/3;
            if(y>=15-hp_rows)return BOSS_TILE_BASE+(boss_flash?7:5);
            return BOSS_TILE_BASE+6;
        }
        return ROCK_BASE+64+(y&1);
    }
    if(x==26||x==27) {
        for(i=0;i<5;i++) {d=(s16)y*8+4-boss_row_y[i];if(d>=-13&&d<=13) {state=plate_state[i];break;}}
        if(boss_leaving)state=CLOSED;
        if(state==ARMING)state=(frame_count&8)?1:0;
        if(state!=255)return BOSS_TILE_BASE+state;
        return BOSS_TILE_BASE+(boss_over&&(frame_count&8)?8:4);
    }
    (void)c;return 0;
}
static void boss_to_shadow(u8 queue) {
    u8 x,y,phys;
    u16 idx,t;
    for(y=2;y<24;y++)for(x=26;x<32;x++) {
        phys=((cam_x>>3)+x)&31;idx=(u16)y*32+phys;t=boss_tile(x,y);
        if(shadow[idx]!=t) {
            shadow[idx]=t;
            if(queue&&patch_count<160) {patches[patch_write].addr=SMS_PNTAddress+(idx<<1);patches[patch_write].tile=t;patch_write++;if(patch_write==160)patch_write=0;patch_count++;}
        }
    }
}
static void prepare_hud(void) {
    u8 i,n;const char *name=is_demo?"CPU":pilot;
    for(i=0;i<64;i++)hud[i]=font(' ');
    text(hud,1,name);number(hud,5,score);text(hud,14,"HI");number(hud,17,hi_score);
    n=lives;if(game_state!=DEAD&&game_state!=OVER&&!(game_state==CLEAR&&loop_no==1&&final_clear)&&n)n--;
    if(n>6)n=6;for(i=0;i<n;i++)hud[31-i]=LIFE_TILE;
    if(paused) text(hud+32,3,"PAUSED - BUTTON TO RESUME");
    else if(game_state==READY) {text(hud+32,1,zone_names[zone]);text(hud+32,13,"READY");text(hud+32,19,pilot);}
    else if(game_state==DEAD)text(hud+32,11,"SHIP LOST");
    else if(game_state==OVER)text(hud+32,11,final_clear?"ALL CLEAR":"GAME OVER");
    else if(game_state==CLEAR)text(hud+32,7,final_clear?(loop_no==1?"ALL CLEAR - SHIP BONUS":"LOOP 1 CLEAR"):"THE GATE GOT AWAY");
    else {
        text(hud+32,1,"L");for(i=0;i<3;i++)hud[34+i]=GAUGE_BASE+(i<laser?2:1);
        hud[37]=laser?GAUGE_BASE+(laser_t<120&&(frame_count&8)?4:2):0;
        hud[39]=font('X');hud[40]=font('1'+chain);
        for(i=0;i<4;i++)hud[41+i]=GAUGE_BASE+(i<chain?5:0);
        if(boss_active) {
            text(hud+32,15,"HP");two_digits(hud+32,18,boss_hp);
            text(hud+32,22,"TIME");two_digits(hud+32,27,(boss_time+59)/60);
            if(boss_over)hud[62]=font('!');
        } else {
            text(hud+32,15,zone_names[zone]);hud[59]=font('L');hud[60]=font('1'+loop_no);
            if(is_demo)text(hud+32,26,"DEMO");else text(hud+32,29,shot_active?"OUT":"RDY");
        }
    }
    hud_mask=0;
    for(i=0;i<32;i++)if(hud[i]!=old_hud[i]) {hud_mask|=1;break;}
    for(i=32;i<64;i++)if(hud[i]!=old_hud[i]) {hud_mask|=2;break;}
    hud_dirty=0;
}
void render_boot(void) {
    u16 i;
    disp_off();SMS_setBGScrollX(0);SMS_setBGScrollY(0);SMS_useFirstHalfTilesforSprites(1);SMS_setSpriteMode(SPRITEMODE_TALL);
    SMS_mapROMBank(2);
    SMS_loadTiles((const void *)GAME_TILES_ADDR,0,GAME_TILES_BYTES);
    SMS_loadTiles((const void *)LOGO_TILES_ADDR,320,LOGO_TILES_BYTES);
    SMS_loadBGPalette((const void *)BG_PALETTE_ADDR);SMS_loadSpritePalette((const void *)SP_PALETTE_ADDR);
    SMS_setBackdropColor(0);SMS_mapROMBank(3);
    SMS_setNextTileatXY(0,0);for(i=0;i<896;i++)SMS_setTile(0);
    memset(old_hud,255,sizeof(old_hud));scene_dirty=1;last_col=65535;
}
void render_prepare(void) {
    if(game_state==TITLE||game_state==TABLE)return;
    if(!scene_dirty) {
        if((cam_x>>3)!=last_col) {incoming_col=(cam_x>>3)+32;make_column(incoming_col);edge_pending=1;}
        if(boss_active&&(boss_dirty||!(frame_count&7))) {boss_to_shadow(1);boss_dirty=0;}
    }
    if(hud_dirty||scene_dirty||(boss_active&&!(frame_count&31))||(laser&&laser_t<120&&!(frame_count&7)))prepare_hud();
}
static void sprite8(s16 x,s16 y,u8 tile) {if(x>=0&&x<256&&y>=16&&y<192)SMS_addSprite((u8)x,(u8)y,tile);}
static void actor(s16 x,u8 y,u8 tile) {sprite8(x-8,(s16)y-8,tile);sprite8(x,(s16)y-8,tile+2);}
static void tall32(s16 x,s16 y,u8 tile) {sprite8(x,y,tile);sprite8(x,y+16,tile+2);}
void render_sprites(void) {
    u8 i,j,tile,y,anim,slot,n;
    s16 x,y0,ym;
    SMS_initSprites();
    if(game_state==TABLE) {SMS_finalizeSprites();return;}
    if(game_state==TITLE) {
        actor(238,24,SPR_SHIP_UP_HOME);actor(40,104,SPR_DRONE0+((frame_count&8)?4:0));
        actor(28,120,SPR_TRAIN0);actor(46,120,SPR_TRAIN1);actor(64,120,SPR_TRAIN0);
        actor(214,120,(frame_count&8)?SPR_CAP:SPR_CAP_BLINK);actor(40,136,SPR_SHELL);sprite8(36,148,SPR_GEM);
        SMS_finalizeSprites();return;
    }
    y=ship_y>>8;
    if(game_state!=DEAD&&game_state!=OVER&&!(game_state==READY&&(frame_count&4))) {
        tile=ship_dir<0?SPR_SHIP_UP_HOME:SPR_SHIP_DOWN_HOME;
        if(laser)tile+=8;else if(shot_active)tile+=4;
        actor(SHIP_X,y,tile);
    }
    if(shot_active) {y0=(s16)shot_y+(shot_dir<0?-AHEAD:-BACK);tall32(shot_x-7,y0,SPR_WAVE);}
    /* Rotate enemies, with the ship and wave always pinned first. */
    if(++sprite_rotation>=enemy_count)sprite_rotation=0;slot=sprite_rotation;
    for(j=0;j<enemy_count;j++) {
        Enemy *e=&enemies[slot];slot++;if(slot>=enemy_count)slot=0;
        if(!e->type)continue;x=e->x>>4;
        if(e->warn) {if(frame_count&2)sprite8(246,(s16)e->y-8,SPR_MARK_RUSHER);continue;}
        anim=((frame_count+(u8)x)&8)?4:0;
        if(e->type==9) {tall32(x-4,(s16)e->y-13,SPR_PLATE);continue;}
        if(e->type==PLUG) {sprite8(x-4,(s16)e->y-24,SPR_SEAL);sprite8(x-4,(s16)e->y-8,SPR_SEAL+2);sprite8(x-4,(s16)e->y+8,SPR_SEAL+4);continue;}
        tile=e->type==SHELL?SPR_SHELL:e->type==RUSHER?SPR_RUSHER:e->type==TRAIN&&trains[e->train].carrier?SPR_TRAIN0+anim:SPR_DRONE0+anim;
        actor(x,e->y,tile);
    }
    for(i=0;i<item_count;i++)if(items[i].type)actor(items[i].x>>4,items[i].y,items[i].type==2?((frame_count&4)?SPR_CAP_BIG:SPR_CAP_BLINK):((frame_count&8)?SPR_CAP:SPR_CAP_BLINK));
    for(i=next_gem;i<GEM_COUNT;i++) {x=GEMS[i].wx-cam_x;if(x>264)break;if(!gem_taken[i])sprite8(x-4,(s16)GEMS[i].y-8,(frame_count&16)?SPR_GEM:SPR_GEM_BLINK);}
    for(i=0;i<pending_count;i++)if(pending[i].type&&pending[i].delay<=40&&(frame_count&4)) {
        tile=pending[i].type==SHELL||pending[i].type==CONVOY?SPR_MARK_SHELL:pending[i].type==RUSHER?SPR_MARK_RUSHER:pending[i].type==TRAIN&&pending[i].carrier?SPR_MARK_TRAIN:SPR_MARK_DRONE;
        sprite8(246,(s16)pending[i].y-8,tile);
    }
    if(game_state==PLAY&&!shot_active&&!blade_t) {
        y0=(s16)y+(ship_dir<0?-AHEAD:-BACK);
        tall32(NOSE_X+blade_reach[laser]-4,y0,laser?SPR_REACH_LASER:SPR_REACH);
    }
    if(blade_t) {
        ym=(s16)blade_y+(blade_dir<0?-MID:MID);
        tile=laser?SPR_BLADE_LASER:SPR_BLADE;
        n=blade_len>>4;if(n>6)n=6;if(!n)n=1;
        for(i=0;i<n;i++) {
            x=NOSE_X+((u16)i*blade_len)/(n?n:1);
            y0=ym-(i?12:5);sprite8(x,y0,tile);
            if(blade_n)sprite8(x,ym+(i?10:3),tile);
        }
        if(blade_n)tall32(NOSE_X+blade_len-4,ym-15,laser?SPR_WAVE_LASER:SPR_WAVE);
    }
    for(i=0;i<8;i++)if(bursts[i].t&&bursts[i].t&2)sprite8(bursts[i].x-4,(s16)bursts[i].y-4,SPR_BURST+bursts[i].kind*2);
    for(i=0;i<trail_count;i++)if(trails[i].x>8)sprite8(trails[i].x,trails[i].y,laser?SPR_TRAIL_LASER:chain?SPR_TRAIL_CHAIN:SPR_TRAIL);
    SMS_finalizeSprites();
}
static void draw_table(void) {
    u8 i;
    memset(shadow,0,sizeof(shadow));
    text(shadow+32*3,13,"BEST 5");text(shadow+32*5,6,"RANK    SCORE    PILOT");
    for(i=0;i<5;i++) {u16 *r=shadow+32*(8+i*2);r[6]=font('1'+i);number(r,11,best_scores[i]);text(r,22,best_names[i]);}
    text(shadow+32*21,10,"PUSH BUTTON");text(shadow+32*23,5,"ONE BUTTON - TWO LOOPS");
    SMS_loadTileMap(0,0,shadow,sizeof(shadow));
}
static void full_scene(void) {
    u8 i;
    disp_off();edge_pending=0;patch_count=patch_read=patch_write=0;
    SMS_VDPturnOffFeature(VDPFEATURE_HIDEFIRSTCOL|VDPFEATURE_LOCKHSCROLL);
    if(game_state==TITLE) {
        SMS_setBGScrollX(0);SMS_mapROMBank(2);SMS_loadTileMap(0,0,(const void *)TITLE_MAP_ADDR,1536);SMS_mapROMBank(3);
        title_blink=255;
    } else if(game_state==TABLE) {SMS_setBGScrollX(0);draw_table();}
    else {
        memset(shadow,0,sizeof(shadow));
        for(i=0;i<32;i++)make_column((cam_x>>3)+1+i);
        if(boss_active)boss_to_shadow(0);
        if(game_state==READY) {center_text(10,boss_active?"GATE":zone_names[zone]);center_text(12,"READY");}
        else if(game_state==DEAD)center_text(12,"SHIP LOST");
        else if(game_state==OVER)center_text(12,final_clear?"ALL CLEAR":"GAME OVER");
        else if(game_state==CLEAR) {center_text(10,final_clear?(loop_no==1?"ALL CLEAR":"LOOP 1 CLEAR"):"THE GATE GOT AWAY");center_text(12,final_clear?(loop_no==1?"SHIP BONUS 50000":"LOOP BONUS 20000"):"NO BONUS");}
        SMS_loadTileMap(0,0,shadow,sizeof(shadow));
        prepare_hud();SMS_loadTileMap(0,0,hud,sizeof(hud));memcpy(old_hud,hud,sizeof(hud));hud_mask=0;
        SMS_setBGScrollX(-(u8)cam_x);SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL|VDPFEATURE_LOCKHSCROLL);
        last_col=cam_x>>3;
    }
    SMS_copySpritestoSAT();scene_dirty=0;boss_dirty=0;disp_on();
}
void render_commit(void) {
    u8 n;
    SMS_copySpritestoSAT();
    if(scene_dirty) {full_scene();return;}
    if(game_state==TITLE) {
        n=(frame_count>>5)&1;
        if(n!=title_blink) {SMS_setNextTileatXY(10,22);if(n)SMS_print("PUSH BUTTON");else {u8 i;for(i=0;i<11;i++)SMS_setTile(0);}title_blink=n;}
        return;
    }
    if(game_state==TABLE)return;
    SMS_setBGScrollX(-(u8)cam_x);
    if(edge_pending) {SMS_loadTileMapColumn(incoming_col&31,2,column,22);last_col=cam_x>>3;edge_pending=0;}
    if(hud_mask&1) {SMS_loadTileMap(0,0,hud,64);memcpy(old_hud,hud,64);}
    if(hud_mask&2) {SMS_loadTileMap(0,1,hud+32,64);memcpy(old_hud+32,hud+32,64);}
    hud_mask=0;
    n=24;while(patch_count&&n--) {SMS_setAddr(patches[patch_read].addr);SMS_setTile(patches[patch_read].tile);patch_read++;if(patch_read==160)patch_read=0;patch_count--;}
}
