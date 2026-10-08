#ifndef UI_H
#define UI_H
#include "game.h"
extern u16 frame_count;
extern u8 display_off,press_queue,pilot_index;
extern char pilot[4];
extern u32 best_scores[5];
extern char best_names[5][4];
extern u8 patch_count;
extern u16 frame_work_max;
void render_boot(void);
void render_prepare(void);
void render_sprites(void);
void render_commit(void);
#endif
