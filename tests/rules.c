/* Host-side rule scenarios exercise the same C core compiled into the ROM.
 * The ROM's Z80 execution is tested separately by run_tests.py.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
u8 host_course[16384];
static int checks,failed;
void sound_effect(u8 id) {(void)id;}
void sound_init(void) {}
static void check(int yes,const char *what) {checks++;if(!yes){printf("FAIL: %s\n",what);failed++;}}
#define CHECK(a) check((a),#a)
static void reset(void) {
    FILE *f=fopen("assets/course.bin","rb");if(!f){perror("course");exit(2);}fread(host_course,1,16384,f);fclose(f);
    game_new(0);game_place(0);game_state=PLAY;ship_y=104<<8;ship_dir=1;hi_score=500000;
}
static void center_step(u8 p) {ship_y=104<<8;game_step(p);}
int main(void) {
    u8 i,g;u16 start;u32 before;
    reset();CHECK(lives==3);CHECK(laser==0);CHECK(chain==0);CHECK(!shot_active);CHECK(sizeof(CourseEvent)==10);CHECK(sizeof(Gem)==4);
    game_state=READY;phase_t=2;game_step(1);CHECK(presses==0);CHECK(ship_dir==1);game_step(1);CHECK(game_state==PLAY);
    reset();game_press();CHECK(ship_dir==-1);CHECK(shot_active);CHECK(shot_x==116);CHECK(presses==1);
    game_press();CHECK(ship_dir==1);CHECK(dry_turns==1);CHECK(shot_active);CHECK(shot_x==116);
    reset();spawn_enemy(DRONE,100*16,129,0,0,0);game_press();CHECK(enemy_count==0);CHECK(cut_kills==1);CHECK(score==300);CHECK(chain==1);CHECK(chain_t==150);CHECK(!shot_active);
    reset();spawn_enemy(DRONE,100*16,94,0,0,0);game_press();CHECK(enemy_count==1);CHECK(cut_kills==0);CHECK(shot_active);
    reset();spawn_enemy(DRONE,70*16,104,0,0,0);game_press();CHECK(score==600);CHECK(chain_t==225);
    reset();spawn_wave(TRAIN,104,3,1);CHECK(enemy_count==3);g=enemies[0].train;
    for(i=0;i<3;i++)enemies[i].x=(83+i*18)*16;
    game_press();CHECK(cut_kills==3);CHECK(score==2100);CHECK(item_count==1);CHECK(trains[g].killed==3);CHECK(!trains[g].active);CHECK(blade_n==3);CHECK(!shot_active);CHECK(chain==1);
    reset();spawn_wave(TRAIN,104,3,0);for(i=0;i<3;i++)enemies[i].x=(83+i*18)*16;game_press();CHECK(item_count==0);
    reset();spawn_enemy(SHELL,94*16,104,0,0,0);game_press();CHECK(score==500);CHECK(!enemy_count);CHECK(chain==1);
    reset();spawn_enemy(SHELL,143*16,110,0,0,0);shot_active=1;shot_x=130;shot_y=104;shot_dir=1;game_step(0);CHECK(!shot_active);CHECK(enemy_count==1);CHECK(score==0);
    reset();shot_active=1;shot_x=130;shot_y=20;shot_dir=1;game_step(0);CHECK(shot_active);CHECK(shot_x==144);CHECK(game_state==PLAY);
    reset();for(i=7;i<34;i++)((Geometry*)host_course)[i].f=10;
    spawn_enemy(DRONE,90*16,104,0,0,0);game_press();CHECK(enemy_count==1);CHECK(shot_active);CHECK(shot_x<70);CHECK(score==0);
    for(i=0;i<4;i++) {reset();laser=i;spawn_enemy(DRONE,(NOSE_X+blade_reach[i]-2)*16,104,0,0,0);game_press();CHECK(cut_kills==1);CHECK(!shot_active);}
    reset();items[0].type=1;items[0].x=44*16;items[0].y=104;item_count=1;game_step(0);CHECK(laser==1);CHECK(laser_t==900);CHECK(cap_count==1);CHECK(score==300);CHECK(item_count==0);
    reset();items[0].type=2;items[0].x=44*16;items[0].y=104;item_count=1;game_step(0);CHECK(laser==3);CHECK(chain==4);CHECK(laser_t==420);CHECK(score==1000);
    reset();laser=3;laser_t=400;spawn_enemy(DRONE,44*16,105,0,0,0);game_step(0);CHECK(laser==0);CHECK(guard_t==30);CHECK(enemy_count==0);CHECK(lives==3);CHECK(game_state==PLAY);
    reset();laser=3;guard_t=20;ship_y=25<<8;ship_dir=-1;game_step(0);CHECK(game_state==DEAD);CHECK(lives==2);
    reset();laser=3;laser_t=1;chain=4;chain_t=1;game_step(0);CHECK(laser==2);CHECK(laser_t==600);CHECK(chain==3);CHECK(chain_t==90);
    reset();chain=0;i=enemy_speed(TRAIN);chain=4;CHECK(enemy_speed(TRAIN)>i);CHECK(enemy_speed(TRAIN)==42);loop_no=1;CHECK(enemy_speed(TRAIN)==46);
    reset();spawn_enemy(DRONE,300*16,105,0,0,0);items[0].type=1;items[0].x=300*16;items[0].y=104;item_count=1;game_step(0);CHECK(game_state==PLAY);CHECK(cap_count==0);CHECK(item_count==1);
    reset();start=ship_y;for(i=0;i<10;i++)game_step(0);CHECK(ship_y==start+3470);CHECK(cam_x==11);CHECK(cam_sub==254);
    reset();game_place(GEMS[0].wx-SHIP_X);ship_y=(u16)GEMS[0].y<<8;ship_dir=-1;laser=1;laser_t=10;chain=2;chain_t=100;game_step(0);CHECK(gem_count==1);CHECK(score==300);CHECK(laser_t==900);
    reset();cam_x=ZONE0_END*8-SHIP_X-1;game_step(0);CHECK(zone==1);CHECK(score==50000);before=score;game_step(0);CHECK(score==before);
    reset();memset(gem_taken,1,sizeof(gem_taken));cam_x=ZONE0_END*8-SHIP_X-1;game_step(0);CHECK(score==150000);
    reset();boss_begin();boss_intro=0;CHECK(boss_hp==24);CHECK(boss_time==4500);
    spawn_enemy(9,100*16,104,0,1,2);plate_state[2]=FLYING;game_press();CHECK(plate_state[2]==OPEN);CHECK(plate_t[2]==270);CHECK(score==500);CHECK(boss_hp==24);
    shot_active=0;laser=3;ship_dir=1;game_press();CHECK(boss_hp==24);CHECK(shot_active);CHECK(shot_x==212);game_step(0);CHECK(boss_hp==23);CHECK(boss_hits==1);CHECK(score==700);
    reset();boss_begin();boss_intro=0;shot_active=1;shot_x=202;shot_y=104;shot_dir=1;game_step(0);CHECK(boss_hp==24);CHECK(!shot_active);
    reset();boss_begin();boss_intro=0;plate_state[2]=OPEN;plate_state[3]=OPEN;boss_hurt();CHECK(score==400);CHECK(boss_hp==23);
    reset();boss_begin();boss_intro=0;plate_state[2]=FLYING;spawn_enemy(9,-13*16,104,0,1,2);game_step(0);CHECK(plate_state[2]==CLOSED);
    reset();boss_begin();boss_intro=0;boss_hp=12;game_step(0);CHECK(boss_over==1);
    reset();boss_begin();boss_intro=0;boss_hp=1;boss_time=3600;for(i=0;i<5;i++)plate_state[i]=OPEN;
    boss_hurt();CHECK(boss_kills==1);CHECK(score==341000);CHECK(lives==4);CHECK(game_state==CLEAR);CHECK(final_clear==1);
    phase_t=1;game_step(0);CHECK(loop_no==1);CHECK(game_state==READY);CHECK(final_clear==0);CHECK(laser==0);
    reset();boss_begin();boss_intro=0;boss_time=1;game_step(0);CHECK(boss_leaving==100);before=score;boss_hurt();CHECK(score==before);
    for(i=0;i<100;i++)center_step(0);CHECK(game_state==CLEAR);CHECK(final_clear==0);CHECK(score==0);phase_t=1;game_step(0);CHECK(loop_no==1);
    reset();loop_no=1;game_state=CLEAR;final_clear=1;phase_t=1;lives=2;score=190000;extends=0;
    game_step(0);CHECK(score==240000);CHECK(lives==1);CHECK(extends==0);
    for(i=0;i<26;i++)game_step(0);CHECK(score==290000);CHECK(lives==0);CHECK(extends==0);
    for(i=0;i<140;i++)game_step(0);CHECK(game_state==OVER);CHECK(final_clear==1);
    reset();enemy_count=0;for(i=0;i<MAX_ENEMIES;i++)spawn_enemy(DRONE,(100+i)*16,104,0,0,0);CHECK(enemy_count==MAX_ENEMIES);CHECK(spawn_enemy(DRONE,120*16,104,0,0,0)==255);CHECK(enemy_count==MAX_ENEMIES);
    printf("RULES: %d checks, %d failed\n",checks,failed);return failed?1:0;
}
