/* Small, offline SMS cartridge player. MIT, 2026 Zig Saber SMS contributors.
 * Z80 instruction execution: superzazu/z80 (MIT), bundled separately.
 * This player supports the SMS mode-4 features used by this ROM. It is not a
 * general replacement for an accurate emulator such as Emulicious.
 */
#include <stdint.h>
#include <stdbool.h>
#include "z80.h"
#ifdef __wasm__
#include <stddef.h>
void *memset(void *p,int v,size_t n){uint8_t *d=p;while(n--)*d++=(uint8_t)v;return p;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
void *stderr;
int printf(const char *f,...){(void)f;return 0;}
int fprintf(void *s,const char *f,...){(void)s;(void)f;return 0;}
#else
#include <string.h>
#endif
#define CLOCK 3579545.0
#define RATE 48000.0
static uint8_t cart[65536],ram[8192],sram[16384],vram[16384],cram[32],reg[16];
static uint8_t bank[4],pad,first,latch,code,buffer,status,line_irq,line_count;
static uint16_t address;
static z80 cpu;
static uint32_t target_cycle,frame_no,active_writes,sram_changed;
static int line;
static uint32_t screen[256*192];
static float audio[8192];
static uint32_t audio_n,audio_cycle;
static double audio_acc,chip_acc;
static uint16_t period[3],counter[4],rng;
static uint8_t attenuation[4],output[4],noise,latch_channel,latch_volume;
static const float levels[16]={1.0f,.794328f,.630957f,.501187f,.398107f,.316228f,.251189f,.199526f,.158489f,.125893f,.1f,.079433f,.063096f,.050119f,.039811f,0.0f};
static uint16_t noise_period(void){return (noise&3)==3?(period[2]?period[2]:1024)*2:(uint16_t)(32u<<(noise&3));}
static void chip_tick(void){
    for(int i=0;i<3;i++)if(!--counter[i]){counter[i]=period[i]?period[i]:1024;output[i]^=1;}
    if(!--counter[3]){counter[3]=noise_period();uint16_t feedback=(noise&4)?((rng^(rng>>3))&1):(rng&1);rng=(rng>>1)|(feedback<<15);output[3]=rng&1;}
}
static float chip_level(void){float s=0;for(int i=0;i<4;i++)s+=(output[i]?1.0f:-1.0f)*levels[attenuation[i]];return s*.2f;}
static void audio_sync(uint32_t cycle){
    uint32_t delta=cycle-audio_cycle;audio_cycle=cycle;audio_acc+=(double)delta*RATE/CLOCK;
    while(audio_acc>=1.0){audio_acc-=1.0;chip_acc+=CLOCK/(16.0*RATE);float s=0;int n=0;
        while(chip_acc>=1.0){chip_acc-=1.0;chip_tick();s+=chip_level();n++;}
        if(audio_n<8192)audio[audio_n++]=n?s/n:chip_level();
    }
}
static void psg_write(uint8_t v){
    audio_sync((uint32_t)cpu.cyc);
    if(v&128){latch_channel=(v>>5)&3;latch_volume=(v>>4)&1;
        if(latch_volume)attenuation[latch_channel]=v&15;
        else if(latch_channel<3)period[latch_channel]=(period[latch_channel]&1008)|(v&15);
        else{noise=v&7;rng=0x8000;counter[3]=noise_period();}
    }else if(latch_volume)attenuation[latch_channel]=v&15;
    else if(latch_channel<3)period[latch_channel]=(period[latch_channel]&15)|((v&63)<<4);
}
static uint8_t read_mem(void *unused,uint16_t a){
    (void)unused;if(a>=0xc000)return ram[a&8191];
    if(a<0x400)return cart[a];
    if(a>=0x8000&&(bank[0]&8))return sram[a&16383];
    int slot=a>>14;return cart[((uint32_t)(bank[slot+1]&3)<<14)|(a&16383)];
}
static void write_mem(void *unused,uint16_t a,uint8_t v){
    (void)unused;if(a>=0xfffc)bank[a-0xfffc]=v;
    if(a>=0xc000)ram[a&8191]=v;
    else if(a>=0x8000&&(bank[0]&8)){sram[a&16383]=v;sram_changed=1;}
}
static uint8_t read_port(z80 *z,uint8_t p){
    (void)z;if(p<0x40)return 255;
    if(p<0x80)return (p&1)?255:(uint8_t)(line<=218?line:line-6);
    if(p<0xc0){first=0;if(p&1){uint8_t v=status;status=0;line_irq=0;cpu.int_pending=false;return v;}uint8_t v=buffer;buffer=vram[address&16383];address=(address+1)&16383;return v;}
    return (p&1)?255:(uint8_t)(~pad)|0xc0;
}
static void write_port(z80 *z,uint8_t p,uint8_t v){
    (void)z;if(p>=0x40&&p<0x80){psg_write(v);return;}
    if(p<0x80||p>=0xc0)return;
    if(p&1){
        if(!first){latch=v;first=1;return;}first=0;code=v>>6;address=((v&63)<<8)|latch;
        if(code==2)reg[v&15]=latch;
        else if(code==0){buffer=vram[address&16383];address=(address+1)&16383;}
    }else{
        first=0;if((reg[1]&64)&&line<192)active_writes++;
        if(code==3)cram[address&31]=v;else vram[address&16383]=v;
        buffer=v;address=(address+1)&16383;
    }
}
static uint8_t tile_pixel(uint16_t t,uint8_t x,uint8_t y){
    uint16_t a=((t&511)<<5)+(y<<2);uint8_t bit=7-x;
    return ((vram[a]>>bit)&1)|(((vram[a+1]>>bit)&1)<<1)|(((vram[a+2]>>bit)&1)<<2)|(((vram[a+3]>>bit)&1)<<3);
}
static uint32_t color(uint8_t c){return 0xff000000u|((uint32_t)((c>>4)&3)*85<<16)|((uint32_t)((c>>2)&3)*85<<8)|(c&3)*85;}
static void render(void){
    uint16_t nt=(reg[2]&14)<<10,sat=(reg[5]&126)<<7,spr=(reg[6]&4)?256:0;
    uint32_t colors[32];for(int i=0;i<32;i++)colors[i]=color(cram[i]);
    uint32_t backdrop=colors[16+(reg[7]&15)];
    for(int y=0;y<192;y++){
        uint8_t priority[256],occupied[256];memset(priority,0,256);memset(occupied,0,256);
        for(int x=0;x<256;x++){
            if(!(reg[1]&64)){screen[y*256+x]=backdrop;continue;}
            int sx=(x-((reg[0]&64)&&y<16?0:reg[8]))&255;
            int sy=(y+((reg[0]&128)&&x>=192?0:reg[9]))%224;
            uint16_t a=nt+(((sy>>3)<<5)+(sx>>3))*2,e=vram[a]|((uint16_t)vram[a+1]<<8);
            uint8_t px=(e&512)?7-(sx&7):sx&7,py=(e&1024)?7-(sy&7):sy&7;
            uint8_t ci=tile_pixel(e&511,px,py);screen[y*256+x]=colors[(e&2048?16:0)+ci];priority[x]=(e&4096)&&ci;
        }
        if(reg[1]&64){
            int found=0,height=(reg[1]&2)?16:8;
            for(int i=0;i<64;i++){
                uint8_t sy0=vram[sat+i];if(sy0==208)break;
                int sy=(uint8_t)(sy0+1),ty=y-sy;
                if(ty<0||ty>=height)continue;if(found++>=8)break;
                int sx=vram[sat+128+i*2]-(reg[0]&8?8:0);uint16_t tile=vram[sat+129+i*2];
                if(height==16)tile&=254;tile+=spr+(ty>>3);
                for(int xx=0;xx<8;xx++){
                    int x=sx+xx;if(x<0||x>=256)continue;uint8_t ci=tile_pixel(tile,xx,ty&7);
                    if(ci&&!occupied[x]){occupied[x]=1;if(!priority[x])screen[y*256+x]=colors[16+ci];}
                }
            }
            if(reg[0]&32)for(int x=0;x<8;x++)screen[y*256+x]=backdrop;
        }
    }
}
uint8_t *sms_rom(void){return cart;}
uint8_t *sms_sram(void){return sram;}
uint8_t *sms_ram(void){return ram;}
uint32_t *sms_video(void){return screen;}
float *sms_audio(void){return audio;}
uint32_t sms_audio_count(void){return audio_n;}
uint32_t sms_sram_changed(void){uint32_t v=sram_changed;sram_changed=0;return v;}
uint32_t sms_active_writes(void){return active_writes;}
uint32_t sms_frames(void){return frame_no;}
uint32_t sms_vdp_reg(uint32_t i){return reg[i&15];}
uint32_t sms_read(uint16_t a){return read_mem(0,a);}
void sms_pause(void){z80_gen_nmi(&cpu);}
void sms_init(void){
    memset(ram,0,8192);memset(vram,0,16384);memset(cram,0,32);memset(reg,0,16);
    bank[0]=0;bank[1]=0;bank[2]=1;bank[3]=2;pad=first=status=line_irq=0;address=0;code=buffer=0;
    frame_no=target_cycle=active_writes=sram_changed=0;line_count=255;
    z80_init(&cpu);cpu.read_byte=read_mem;cpu.write_byte=write_mem;cpu.port_in=read_port;cpu.port_out=write_port;
    audio_cycle=audio_n=0;audio_acc=chip_acc=0.0;rng=0x8000;noise=0;latch_channel=latch_volume=0;
    for(int i=0;i<4;i++){attenuation[i]=15;counter[i]=1;output[i]=0;}
    period[0]=period[1]=period[2]=1024;render();
}
void sms_frame(uint32_t buttons){
    pad=buttons&63;audio_n=0;
    for(line=0;line<262;line++){
        if(line<=192){if(line==192)status|=128;if(!line_count){line_irq=1;line_count=reg[10];}else line_count--;}
        else line_count=reg[10];
        target_cycle+=228;
        while((int32_t)((uint32_t)cpu.cyc-target_cycle)<0){if(cpu.iff1&&(((status&128)&&(reg[1]&32))||(line_irq&&(reg[0]&16))))cpu.int_pending=true;z80_step(&cpu);}
    }
    audio_sync((uint32_t)cpu.cyc);frame_no++;render();
}
