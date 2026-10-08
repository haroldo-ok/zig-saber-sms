/* ZIG SABER / Sega Master System. Original rules: ABA Games (MIT, 2026). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
#include "assets.h"
#include "sound_ids.h"
#define TITLE 0
#define READY 1
#define PLAY 2
#define DEAD 3
#define OVER 4
#define CLEAR 5
#define TABLE 6
#define DRONE 1
#define TRAIN 2
#define SHELL 3
#define RUSHER 4
#define CONVOY 5
#define AIMED 6
#define PLUG 7
#define CAPITEM 8
#define CLOSED 0
#define ARMING 1
#define FLYING 2
#define OPEN 3
#define MAX_ENEMIES 24
#define MAX_PENDING 12
#define MAX_ITEMS 8
#define MAX_TRAINS 12
#define SHIP_X 44
#define NOSE_X 52
#define FIELD_ROWS 22
#define AHEAD 26
#define BACK 5
#define MID 10
#define BOSS_FACE 208

typedef struct { u8 c,f,a,b; } Geometry;
typedef struct { u16 wx; u8 type,y,n,carrier,lo,hi,lo2,hi2; } CourseEvent;
typedef struct { u16 wx; u8 y,zone; } Gem;
/* SDCC/Z80 structs are packed; tests derive addresses from current .noi. */
typedef struct { s16 x; u8 y,type,v,warn,train,row,gate; } Enemy;
typedef struct { u8 delay,type,y,n,carrier; } Pending;
typedef struct { u8 n,killed,escaped,carrier,active; } TrainGroup;
typedef struct { s16 x; u8 y,type; } Item;
typedef struct { s16 x; u8 y,t,kind; } Burst;
typedef struct { s16 x; u8 y; } Trail;
#ifdef HOST_TEST
extern u8 host_course[16384];
#define ROM_PTR(a) (host_course+((a)-0x8000))
#else
#define ROM_PTR(a) ((const void *)(a))
#endif
#define GEO ((const Geometry *)ROM_PTR(GEO_ADDR))
#define GEMS ((const Gem *)ROM_PTR(GEM_ADDR))
extern u8 game_state,is_demo,loop_no,zone,lives,laser,chain,guard_t,paused;
extern s8 ship_dir,shot_dir,blade_dir;
extern u16 cam_x,ship_y,phase_t,sim_ticks,chain_t,laser_t;
extern u8 cam_sub,shot_active,shot_y,freeze_t,blade_t,blade_y,blade_len,blade_n;
extern u16 shot_x;
extern u8 boss_active,boss_hp,boss_intro,boss_leaving,boss_over,boss_flash;
extern u16 boss_time,boss_launch,boss_emit;
extern u8 plate_state[5];
extern u16 plate_t[5];
extern u8 scene_dirty,boss_dirty,hud_dirty,flash_t,flash_n,zone_lost,final_clear;
extern u16 flash_pts;
extern u8 next_event,next_gem,gem_chain,extends;
extern u8 gem_taken[GEM_COUNT];
extern u32 score,hi_score;
extern u16 presses,dry_turns,cut_kills,far_kills,cap_count,gem_count,deaths,boss_hits,boss_kills,pool_drops;
extern u8 enemy_count,pending_count,item_count,trail_count;
extern Enemy enemies[MAX_ENEMIES];
extern Pending pending[MAX_PENDING];
extern TrainGroup trains[MAX_TRAINS];
extern Item items[MAX_ITEMS];
extern Burst bursts[8];
extern Trail trails[12];
extern const u8 boss_row_y[5];
extern const u16 laser_decay[4],chain_decay[5];
extern const u8 blade_reach[4];
void game_new(u8 demo);
void game_place(u16 cam);
void game_step(u8 pressed);
void game_press(void);
u8 game_pilot(void);
u8 rock_at(u16 wx,s16 y);
u8 rock_hull(u16 wx,u8 y);
u8 rock_col(u16 col,s8 row);
u8 enemy_width(u8 type);
u8 enemy_halfheight(u8 type);
u8 enemy_speed(u8 type);
u8 spawn_enemy(u8 type,s16 x,u8 y,u8 group,u8 gate,u8 row);
void spawn_wave(u8 type,u8 y,u8 n,u8 carrier);
void boss_begin(void);
void boss_hurt(void);
void add_score(u32 pts);
void feedback_step(void);
void sound_init(void);
void sound_effect(u8 id);
void sound_tick(u8 on,u8 cue,u8 loop,u8 hot,u8 mute);
#endif
