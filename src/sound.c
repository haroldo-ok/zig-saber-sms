/* One-SN76489 arrangement of upstream/psg.js. Two music voices, one SFX
 * voice; percussion yields to effect noise. Original note/effect programs
 * are compiled offline, so there is no floating point or sample playback.
 */
#include "game.h"
#ifndef HOST_TEST
__sfr __at (0x7f) PSGPort;
#else
static u8 PSGPort;
#endif
typedef struct { u16 period;u8 att,noise,natt; } SoundFrame;
typedef struct { const SoundFrame *data;u8 length,priority; } Effect;
#include "sound_data.h"

u8 audio_effect,audio_effect_frame,audio_effect_active;
static u8 music_step,music_frame,last_cue,last_on,lead_age,lead_left,bass_age,bass_left;
static u8 drum_age,drum_left,drum_mode,drum_att,drum_decay,noise_mode;
static u16 lead_period,bass_period;
static void volume(u8 ch,u8 a) { PSGPort=0x90|(ch<<5)|a; }
static void tone(u8 ch,u16 p) { PSGPort=0x80|(ch<<5)|(p&15);PSGPort=(p>>4)&63; }
static void silence(void) { volume(0,15);volume(1,15);volume(2,15);volume(3,15); }
void sound_init(void) {
    silence();audio_effect_active=0;audio_effect_frame=0;audio_effect=0;
    last_cue=255;last_on=0;music_step=music_frame=0;lead_left=bass_left=drum_left=0;noise_mode=255;
}
void sound_effect(u8 id) {
    if(audio_effect_active&&effects[id].priority<effects[audio_effect].priority)return;
    audio_effect=id;audio_effect_frame=0;audio_effect_active=1;
}
void sound_tick(u8 on,u8 cue,u8 loop,u8 hot,u8 mute) {
    u8 frames=cue==3?(hot?3:4):5,note,len,i,att;
    s8 lead;
    const Effect *e;
    const SoundFrame *f;
    if(mute) {silence();return;}
    if(cue!=last_cue||on&&!last_on) {music_step=music_frame=0;lead_left=bass_left=drum_left=0;last_cue=cue;}
    last_on=on;
    if(on) {
        if(!music_frame) {
            lead=music_lead[cue][music_step];
            if(lead>0) {
                lead_period=midi_period[(u8)lead];lead_age=0;len=1;
                while(len<16&&music_lead[cue][(music_step+len)&127]==-1)len++;
                lead_left=len*frames-1;tone(0,lead_period);
            } else if(!lead)lead_left=0;
            note=music_bass[cue][music_step];
            if(note) {bass_period=midi_period[note];bass_age=0;bass_left=cue==3?frames-1:frames*2-2;tone(1,bass_period);}
            i=music_step&15;
            if(i==4||i==12||(cue==3&&(music_step>>4)==3&&i>=8&&!(i&1))) {drum_mode=5;drum_att=5;drum_left=5;drum_decay=2;drum_age=0;}
            else if((i&3)==2) {drum_mode=4;drum_att=9;drum_left=2;drum_decay=3;drum_age=0;}
            else if((cue==2||cue==3)&&(i&1)) {drum_mode=4;drum_att=11;drum_left=1;drum_decay=4;drum_age=0;}
        }
        if(lead_left) {att=5+(lead_age>>3);if(att>11)att=11;volume(0,att);lead_age++;lead_left--;}
        else volume(0,15);
        if(bass_left) {att=5+(bass_age>>1);if(att>13)att=13;volume(1,att);bass_age++;bass_left--;}
        else volume(1,15);
        if(++music_frame>=frames) {music_frame=0;music_step=(music_step+1)&(cue==3?63:127);}
    } else {volume(0,15);volume(1,15);}
    if(audio_effect_active) {
        e=&effects[audio_effect];f=e->data+audio_effect_frame;
        if(f->period)tone(2,f->period);
        volume(2,f->period?f->att:15);
        if(f->natt<15) {
            if(noise_mode!=f->noise) {PSGPort=0xe0|f->noise;noise_mode=f->noise;}
            volume(3,f->natt);
        } else if(on&&drum_left) {
            if(noise_mode!=drum_mode) {PSGPort=0xe0|drum_mode;noise_mode=drum_mode;}
            att=drum_att+drum_age*drum_decay;if(att>15)att=15;volume(3,att);
        } else volume(3,15);
        if(++audio_effect_frame>=e->length)audio_effect_active=0;
    } else {
        volume(2,15);
        if(on&&drum_left) {
            if(noise_mode!=drum_mode) {PSGPort=0xe0|drum_mode;noise_mode=drum_mode;}
            att=drum_att+drum_age*drum_decay;if(att>15)att=15;volume(3,att);
        } else volume(3,15);
    }
    if(drum_left) {drum_left--;drum_age++;}
    (void)loop;
}
