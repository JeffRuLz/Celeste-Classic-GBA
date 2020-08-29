#ifndef PICO8_H
#define PICO8_H

#include <gba_input.h>
#include <gba_types.h>
#include <gba_systemcalls.h>


#define FLAGS_SIZE 256
extern const unsigned char FLAGS_DATA[FLAGS_SIZE];

#define MAP_SIZE 8192
extern const unsigned short MAP_DATA[MAP_SIZE];


//-- Graphics --
//--------------
#define PAL_BG 		(1 << 0)
#define PAL_SPRITES (1 << 1)
#define PAL_PLAYER 	(1 << 2)
#define PAL_TEXT  	(1 << 3)
#define PAL_OVERLAY (1 << 4)

#define fget(n,f) ((FLAGS_DATA[(n)] >> (f)) & 1)

void camera(s16 x, s16 y);
void pal(u8 c0, u8 c1, u8 p);
void print(char const* str, u8 x, u8 y, u8 col);
void rectfill(u8 x, u8 y, u8 w, u8 h, s8 col);
void spr(u16 n, s16 x, s16 y, u8 layer, u8 palette, bool flip_x, bool flip_y);
void update_screen();


//-- Audio --
//-----------
void music(s8 n, u16 fade_len, u8 channel_mask);
void sfx(u8 n);


//-- Math --
//----------
#define max(X,Y) (((X) > (Y))? (X): (Y))
#define min(X,Y) (((X) < (Y))? (X): (Y))

#define flr(X) ((s16)(X))

int rndi(int x);	//integer rand (fast)
float rnd(float x);	//float rand (slow)

#endif
