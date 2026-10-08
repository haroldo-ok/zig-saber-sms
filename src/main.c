/* Cabinet flow, joypad edge detection, console PAUSE/NMI and BEST 5 SRAM.
 * Build with SDCC >= 4.2 and the vendored, matched devkitSMS snapshot.
 */
#include "ui.h"
#include "SMSlib.h"
#include <string.h>

u16 frame_count,frame_work_max;
u8 press_queue,pilot_index;
char pilot[4];
u32 best_scores[5];
char best_names[5][4];
static u16 front_t;
/* VBlank latches edges even if a busy render spans more than one frame. */
static volatile u8 irq_previous_keys,irq_edges;
static u8 ranking_saved;
static const char callsigns[50][4]={
    "ACE","ARC","AXE","BOW","COG","DOT","EDG","FIN","FOX","GUN",
    "HEX","INK","ION","JAM","JET","KIT","LUX","MAX","NEO","NOV",
    "ORB","PIX","RAM","RAY","REX","SKY","SOL","TAU","VEX","VIC",
    "WIZ","ZAG","ZAP","ZED","ASH","BAY","COL","DAX","ELM","FAY",
    "GUS","HAL","IVY","JOE","KAI","LEO","MIA","NED","OTT","PAM"
};
static const u32 defaults[5]={500000UL,200000UL,80000UL,30000UL,10000UL};
static const char default_names[5][4]={"ZIG","SAB","ERR","CUT","ONE"};
static u8 save_record[41];

static void rank_load(void) {
    u8 i,j,p=4,valid=1;
    u16 sum=0;
    for(i=0;i<5;i++) {best_scores[i]=defaults[i];memcpy(best_names[i],default_names[i],4);}
    SMS_enableSRAM();
    if(SMS_SRAM[0]=='Z'&&SMS_SRAM[1]=='S'&&SMS_SRAM[2]=='B'&&SMS_SRAM[3]=='1') {
        for(i=0;i<41;i++)save_record[i]=SMS_SRAM[i];
        for(i=0;i<39;i++)sum+=save_record[i];
        if(sum==((u16)save_record[39]|((u16)save_record[40]<<8))) {
            for(i=0;i<5;i++) {
                u32 value=0;
                for(j=0;j<4;j++) value|=(u32)save_record[p+j]<<(j*8);
                if(value>9999999UL)valid=0;
                for(j=0;j<3;j++)if(save_record[p+4+j]<'A'||save_record[p+4+j]>'Z')valid=0;
                p+=7;
            }
            if(valid) {
                p=4;
                for(i=0;i<5;i++) {
                    best_scores[i]=0;for(j=0;j<4;j++)best_scores[i]|=(u32)save_record[p++]<<(j*8);
                    for(j=0;j<3;j++)best_names[i][j]=save_record[p++];best_names[i][3]=0;
                }
            }
        }
    }
    SMS_disableSRAM();SMS_mapROMBank(3);hi_score=best_scores[0];
}
static void rank_save(void) {
    u8 i,j,p=4;
    u16 sum=0;
    save_record[0]='Z';save_record[1]='S';save_record[2]='B';save_record[3]='1';
    for(i=0;i<5;i++) {
        for(j=0;j<4;j++)save_record[p++]=(u8)(best_scores[i]>>(j*8));
        for(j=0;j<3;j++)save_record[p++]=best_names[i][j];
    }
    for(i=0;i<39;i++)sum+=save_record[i];save_record[39]=sum;save_record[40]=sum>>8;
    SMS_enableSRAM();for(i=0;i<41;i++)SMS_SRAM[i]=save_record[i];SMS_disableSRAM();SMS_mapROMBank(3);
}
static void rank_insert(void) {
    u8 i,j;
    if(ranking_saved||is_demo)return;ranking_saved=1;
    if(!score)return;
    for(i=0;i<5;i++)if(score>=best_scores[i])break;
    if(i==5)return;
    for(j=4;j>i;j--) {best_scores[j]=best_scores[j-1];memcpy(best_names[j],best_names[j-1],4);}
    best_scores[i]=score;memcpy(best_names[i],pilot,4);hi_score=best_scores[0];rank_save();sound_effect(SE_ENTRY);
}
static void start_real(void) {
    u16 n=frame_count;
    while(n>=50)n-=50;pilot_index=n;memcpy(pilot,callsigns[pilot_index],4);
    sound_init();game_new(0);press_queue=0;front_t=0;ranking_saved=0;
}
static void to_title(void) {
    game_state=TITLE;is_demo=0;scene_dirty=1;front_t=0;paused=0;press_queue=0;shot_active=0;blade_t=0;
}
static void input_capture(void) {
    u8 keys=(u8)SMS_getKeysStatus()&0x33;
    if((keys&~irq_previous_keys)&&irq_edges<3)irq_edges++;
    irq_previous_keys=keys;
}
void main(void) {
    u8 edge,pressed,start_line,end_line,work;
    frame_count=frame_work_max=front_t=0;irq_previous_keys=irq_edges=0;ranking_saved=0;
    game_state=TITLE;is_demo=paused=0;score=0;lives=3;memcpy(pilot,"ACE",4);
    SMS_configureTextRenderer(FONT_BASE-32);
    rank_load();sound_init();render_boot();
    SMS_setFrameInterruptHandler(input_capture);
    for(;;) {
        frame_count++;start_line=SMS_getVCount();
        __critical {edge=irq_edges;irq_edges=0;}
        if(SMS_queryPauseRequested()) {
            SMS_resetPauseRequest();
            if(game_state!=TITLE&&game_state!=TABLE&&!is_demo) {paused=!paused;press_queue=0;hud_dirty=1;}
        }
        pressed=edge;
        if(paused&&edge) {paused=0;pressed=0;press_queue=0;hud_dirty=1;}
        if(!paused) {
            feedback_step();
            if(is_demo&&game_state!=TITLE&&game_state!=TABLE&&pressed)start_real();
            else if(game_state==TITLE) {
                if(pressed)start_real();
                else if(++front_t>=480) {game_new(1);front_t=0;}
            } else if(game_state==TABLE) {
                if(pressed)start_real();else if(++front_t>=420)to_title();
            } else if(game_state==OVER) {
                if(is_demo) {game_state=TABLE;scene_dirty=1;front_t=0;is_demo=0;}
                else {
                    rank_insert();
                    if(pressed&&phase_t<130)start_real();
                    else if(phase_t&&!--phase_t) {game_state=TABLE;scene_dirty=1;front_t=0;}
                }
            } else {
                if(is_demo) {if(++front_t>=1080) {game_state=TABLE;scene_dirty=1;is_demo=0;front_t=0;}}
                if(pressed&&game_state==PLAY&&!is_demo) {
                    if(press_queue+edge>3)press_queue=3;else press_queue+=edge;
                }
                if(freeze_t)freeze_t--;
                else {
                    u8 action=0;
                    if(game_state==PLAY) {if(is_demo)action=game_pilot();else if(press_queue) {action=1;press_queue--;}}
                    else press_queue=0;
                    game_step(action);
                }
            }
        }
        sound_tick(game_state==PLAY,boss_active?3:zone,loop_no,boss_over,paused||is_demo);
        render_prepare();render_sprites();
        end_line=SMS_getVCount();work=end_line-start_line;if(work>frame_work_max)frame_work_max=work;
        SMS_waitForVBlank();
        render_commit();
    }
}
SMS_EMBED_SEGA_ROM_HEADER(9999,1);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(1,0,"ABA Games / SMS port","ZIG SABER","One button. Turn + fire. Two loops.");
