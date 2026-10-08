#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
u8 host_course[16384];
extern u8 gift_t;
void sound_effect(u8 id){(void)id;}
void sound_init(void){}
static void reset(void){game_new(0);game_place(0);game_state=PLAY;ship_y=104<<8;ship_dir=1;next_event=EVENT0_COUNT;next_gem=GEM_COUNT;gift_t=0;}
static void gate(void){boss_begin();boss_intro=0;boss_launch=boss_emit=200;}
static const char *phases[]={"title","ready","play","dead","over","clear","table"};
static void out(const char *name,int last){printf("{\"name\":\"%s\",\"dir\":%d,\"shot\":%d,\"laser\":%d,\"chain\":%d,\"score\":%lu,\"lives\":%d,\"cuts\":%d,\"fars\":%d,\"caps\":%d,\"gems\":%d,\"guard\":%d,\"items\":%d,\"enemies\":%d,\"hp\":%d,\"hits\":%d,\"phase\":\"%s\",\"laserT\":%d,\"chainT\":%d}%s\n",name,ship_dir,shot_active,laser,chain,(unsigned long)score,lives,cut_kills,far_kills,cap_count,gem_count,guard_t,item_count,enemy_count,boss_hp,boss_hits,phases[game_state],laser_t,chain_t,last?"":",");}
int main(void){FILE *f=fopen("assets/course.bin","rb");if(!f)return 2;fread(host_course,1,16384,f);fclose(f);puts("[");
reset();game_step(1);out("empty press",0);
reset();shot_active=1;shot_x=116;shot_y=104;shot_dir=1;game_step(1);out("dry turn",0);
reset();spawn_enemy(DRONE,70*16,104,0,0,0);game_step(1);out("point blank",0);
reset();spawn_wave(TRAIN,104,3,1);for(u8 i=0;i<3;i++)enemies[i].x=(83+i*18)*16;game_step(1);out("carrier multi cut",0);
reset();spawn_enemy(SHELL,94*16,104,0,0,0);game_step(1);out("shell cut",0);
reset();spawn_enemy(SHELL,143*16,110,0,0,0);shot_active=1;shot_x=130;shot_y=104;shot_dir=1;game_step(0);out("armour deflection",0);
reset();laser=3;laser_t=400;spawn_enemy(DRONE,44*16,105,0,0,0);game_step(0);out("laser armour",0);
reset();laser=3;laser_t=1;chain=4;chain_t=1;game_step(0);out("decay",0);
reset();item_count=1;items[0].x=44*16;items[0].y=104;items[0].type=1;game_step(0);out("capsule",0);
reset();item_count=1;items[0].x=44*16;items[0].y=104;items[0].type=2;game_step(0);out("sealed lane capsule",0);
reset();gate();plate_state[2]=FLYING;spawn_enemy(9,100*16,104,0,1,2);game_step(1);out("gate plate cut",0);
reset();gate();plate_state[2]=OPEN;plate_t[2]=270;laser=3;laser_t=420;game_step(1);out("wave into open core",0);
reset();gate();shot_active=1;shot_x=202;shot_y=104;shot_dir=1;game_step(0);out("closed core deflects",0);
reset();gate();plate_state[2]=OPEN;plate_t[2]=270;boss_hp=1;boss_time=3600;shot_active=1;shot_x=202;shot_y=104;shot_dir=1;game_step(0);out("gate victory and extend",1);
puts("]");return 0;}
