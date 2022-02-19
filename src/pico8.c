#include "pico8.h"

#include <gba_sprites.h>
#include <gba_video.h>
#include <maxmod.h>
#include <stdlib.h>

#include "soundbank.h"
#include "soundbank_bin.h"

//-- Graphics --
//--------------
static OBJATTR obj_buffer[128] = { 0 };

static const u16 palette[16] = {
	RGB8(0x00,0x00,0x00),	//black
	RGB8(0x1D,0x2B,0x53),	//dark blue
	RGB8(0x7E,0x25,0x53),	//dark purple
	RGB8(0x00,0x87,0x51),	//dark green
	RGB8(0xAB,0x52,0x36),	//brown
	RGB8(0x5F,0x57,0x4F),	//dark gray
	RGB8(0xC2,0xC3,0xC7),	//light gray
	RGB8(0xFF,0xF1,0xE8),	//white
	RGB8(0xFF,0x00,0x4D),	//red
	RGB8(0xFF,0xA3,0x00),	//orange
	RGB8(0xFF,0xEC,0x27),	//yellow
	RGB8(0x00,0xE4,0x36),	//green
	RGB8(0x29,0xAD,0xFF),	//blue
	RGB8(0x83,0x76,0x9C),	//indigo
	RGB8(0xFF,0x77,0xA8),	//pink
	RGB8(0xFF,0xCC,0xAA)	//peach
};

static s16 camx = 0;
static s16 camy = 0;
static s16 borderx = 0;
static s16 bordery = 0;

void camera(s16 x, s16 y)
{
	camx = x - 56;
	camy = y - 16;

	borderx = x + 8;
	bordery = y + 8;
}

void pal(u8 c0, u8 c1, u8 p)
{
	if (c0 == 0 && c1 == 0)
	{
		//reset palette
		for (int i = 0; i < 16; i++)
		{
			if (p == 0 || p & PAL_BG)
				BG_PALETTE[i] = palette[i];

			if (p == 0 || p & PAL_OVERLAY)
				BG_PALETTE[16 + i] = palette[i];

			if (p == 0 || p & PAL_SPRITES)
				SPRITE_PALETTE[i] = palette[i];

			if (p == 0 || p & PAL_PLAYER)	
				SPRITE_PALETTE[16 + i] = palette[i];

			if (p == 0 || p & PAL_TEXT)
				SPRITE_PALETTE[32 + i] = palette[i];

			if (p == 0 || p & PAL_OVERLAY)
				SPRITE_PALETTE[48 + i] = palette[i];
		}
	}
	else
	{
		if (p & PAL_BG)
			BG_PALETTE[c0] = palette[c1];

		if (p & PAL_OVERLAY)
			BG_PALETTE[16 + c0] = palette[c1];

		if (p & PAL_SPRITES)
			SPRITE_PALETTE[c0] = palette[c1];

		if (p & PAL_PLAYER)
			SPRITE_PALETTE[16 + c0] = palette[c1];

		if (p & PAL_TEXT)
			SPRITE_PALETTE[32 + c0] = palette[c1];

		if (p & PAL_OVERLAY)
			SPRITE_PALETTE[48 + c0] = palette[c1];
	}
}

void print(char const* str, u8 x, u8 y, u8 col)
{
	x += 4;

	u8 xstart = x;
	
	pal(7,col,PAL_TEXT);

	u16 i = 0;
	while (str[i] != '\0')
	{
		if (str[i] == '#')
		{
			x = xstart;
			y += 8;
		}
		else
		{
			spr(256+str[i]-' ', x, y, 0, PAL_TEXT, 0, 0);
			x += 4;
		}
		i++;
	}
}

static bool rect_drawn = false;

void rectfill(u8 x, u8 y, u8 w, u8 h, s8 col)
{
	if (col >= 0)
	{
		rect_drawn = true;
		pal(10,col,PAL_BG);
	}
	else
	{
		//clear drawn rectangles
		if (rect_drawn == false)
			return;
		rect_drawn = false;
	}

	u16* map_adr = MAP_BASE_ADR(3);

	for (u8 iy = 3+y; iy < 3+y+h; iy++)
	{
		for (u8 ix = 8+x; ix < 8+x+w; ix++)
			map_adr[ix+(iy*32)] = (col >= 0)? 138: 0;
	}
}

static u8 sprite_index = 0;

void spr(u16 n, s16 x, s16 y, u8 layer, u8 palette, bool flip_x, bool flip_y)
{
	OBJATTR* obj = &(obj_buffer[sprite_index]);
	obj->attr0 = OBJ_Y(y-camy) | ATTR0_COLOR_16 | ATTR0_SQUARE;
	obj->attr1 = OBJ_X(x-camx) | ATTR1_SIZE_8;
	obj->attr2 = OBJ_CHAR(n) | OBJ_PRIORITY(layer) | OBJ_SQUARE;

	//palette
	if (palette == 0 || palette & PAL_SPRITES)
		obj->attr2 |= ATTR2_PALETTE(0);
	else if (palette & PAL_PLAYER)
		obj->attr2 |= ATTR2_PALETTE(1);
	else if (palette & PAL_TEXT)
		obj->attr2 |= ATTR2_PALETTE(2);
	else if (palette & PAL_OVERLAY)
		obj->attr2 |= ATTR2_PALETTE(3);

	//flip
	if (flip_x)
		obj->attr1 |= ATTR1_FLIP_X;

	if (flip_y)
		obj->attr1 |= ATTR1_FLIP_Y;

	sprite_index += 1;
	if (sprite_index >= 128)
		sprite_index = 0;
}

void update_screen()
{
	//game screen position
	REG_BG1HOFS = camx;
	REG_BG1VOFS = camy;

	//black border position
	REG_BG0HOFS = borderx;
	REG_BG0VOFS = bordery;

	//upload OAM copy to real OAM
	CpuFastSet(obj_buffer, OAM, ((sizeof(OBJATTR)*128)/4) | COPY32);

	//wipe OAM copy
	for (u8 i = 0; i < sprite_index; i++)
		obj_buffer[i].attr0 = OBJ_DISABLE;

	sprite_index = 0;
}


//-- Audio --
//-----------
void music(s8 n, u16 fade_len, u8 channel_mask)
{
	mmStop();

	switch (n)
	{
		case  0: mmStart(MOD_MUS0,  MM_PLAY_LOOP); break;
		case 10: mmStart(MOD_MUS10, MM_PLAY_LOOP); break;
		case 20: mmStart(MOD_MUS20, MM_PLAY_LOOP); break;
		case 30: mmStart(MOD_MUS30, MM_PLAY_LOOP); break;
		case 40: mmStart(MOD_MUS40, MM_PLAY_LOOP); break;
	}
}

void sfx(u8 n)
{
	switch (n)
	{
		case  0: mmEffect(SFX_SND0);  break;
		case  1: mmEffect(SFX_SND1);  break;
		case  2: mmEffect(SFX_SND2);  break;
		case  3: mmEffect(SFX_SND3);  break;
		case  4: mmEffect(SFX_SND4);  break;
		case  5: mmEffect(SFX_SND5);  break;
		case  6: mmEffect(SFX_SND6);  break;
		case  7: mmEffect(SFX_SND7);  break;
		case  8: mmEffect(SFX_SND8);  break;
		case  9: mmEffect(SFX_SND9);  break;
		case 13: mmEffect(SFX_SND13); break;
		case 14: mmEffect(SFX_SND14); break;
		case 15: mmEffect(SFX_SND15); break;
		case 16: mmEffect(SFX_SND16); break;
		case 23: mmEffect(SFX_SND23); break;
		case 35: mmEffect(SFX_SND35); break;
		case 37: mmEffect(SFX_SND37); break;
		case 38: mmEffect(SFX_SND38); break;
		case 40: mmEffect(SFX_SND40); break;
		case 51: mmEffect(SFX_SND51); break;
		case 54: mmEffect(SFX_SND54); break;
		case 55: mmEffect(SFX_SND55); break;
	}
}


//-- Math --
//----------
int rndi(int x)
{
	return (rand() % x);
}

float rnd(float x)
{
	int v = x * 100.f;
	if (v == 0)
		return 0;

	return ((rand() % v) / 100.f);
}