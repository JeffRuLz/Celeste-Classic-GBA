//-- ~celeste~
//-- matt thorson + noel berry

#include <gba_interrupt.h>
#include <gba_video.h>
#include <gba_sprites.h>
#include <maxmod.h>

#include "pico8.h"
#include <string.h>

#include "fnt.h"
#include "gfx.h"
#include "sine.h"

#include "soundbank_bin.h"


//-- types --
//-----------
typedef struct
{
	s8 x, y;
} ALIGN(4) Point;

typedef struct
{
	float x, y;
} ALIGN(4) Pointf;

typedef struct
{
	bool x, y;
} ALIGN(4) Flip;

typedef struct
{
	s16 x, y;
	u8 w, h;
} ALIGN(4) Rectangle;


//-- object types --
//------------------
enum {
	type_player_spawn = 1,
	type_player,
	type_platform,
	type_key = 8,
	type_spring = 18,
	type_chest = 20,
	type_balloon = 22,
	type_fall_floor = 23,
	type_fruit = 26,
	type_fly_fruit = 28,
	type_fake_wall = 64,
	type_message = 86,
	type_big_chest = 96,
	type_flag = 118,
	type_smoke,
	type_orb,
	type_lifeup
};

typedef struct
{
	bool active;
	u8 type;
	bool collideable;
	bool solids;
	u8 spr;
	Flip flip;
	float x, y;
	Rectangle hitbox;
	Pointf spd;
	Pointf rem;
} ALIGN(4) Object;

typedef struct
{
	Object obj;
	float offset;
	float start;
	u8 timer;
} ALIGN(4) Balloon;

typedef struct
{
	bool active;
	s16 x, y;
	u8 h;
	u8 spd;
} Big_Chest_Particle;

typedef struct
{
	Object obj;
	u8 state;
	u8 timer;
	Big_Chest_Particle particles[50];
} ALIGN(4) Big_Chest;

typedef struct
{
	Object obj;
	u8 start;
	u8 timer;
} ALIGN(4) Chest;

typedef struct
{
	bool active;
	float x, y;
	u8 t;
	Pointf spd;
} ALIGN(4) Dead_Particle;

typedef struct
{
	Object obj;
	u8 state;
	bool solid;
	u8 delay;
	u16* map_adr;
} ALIGN(4) Fall_Floor;

typedef struct
{
	Object obj;
	u8 score;
	bool show;
} ALIGN(4) Flag;

typedef struct
{
	Object obj;
	u8 start;
	bool fly;
	float step;
	u8 sfx_delay;
} ALIGN(4) Fly_Fruit;

typedef struct
{
	Object obj;
	u8 start;
	u8 off;
} ALIGN(4) Fruit;

typedef struct
{
	float x, y;
	u8 size;
} ALIGN(4) Hair;

typedef struct
{
	Object obj;
	u8 duration;
	u8 flash;
} ALIGN(4) Lifeup;

typedef struct
{
	Object obj;
	u8 index;
	u8 lines;
	u8 len;
} ALIGN(4) Message;

typedef struct
{
	Object obj;	
} ALIGN(4) Orb;

typedef struct
{
	float x, y;
	u8 s;
	float spd;
	float off;
	u8 c;
} ALIGN(4) Particle;

typedef struct
{
	Object obj;
	float last;
} ALIGN(4) Platform;

typedef struct
{
	Object obj;
	bool p_jump;
	bool p_dash;
	u8 grace;
	u8 jbuffer;
	u8 djump;
	u8 dash_time;
	u8 dash_effect_time;
	Pointf dash_target;
	Pointf dash_accel;
	Rectangle hitbox;
	u8 spr_off;
	bool was_on_ground;
	void (*is_ice)(u8, u8, Rectangle);
} ALIGN(4) Player;

typedef struct
{
	bool active;
	s16 x, y;
	u8 spr;
	Point target;
	Pointf spd;
	u8 state;
	u8 delay;
	bool solids;
	Flip flip;
} ALIGN(4) Player_Spawn;

typedef struct
{
	bool active;
	s8 delay;
} ALIGN(4) Room_Title;

typedef struct
{
	Object obj;
	u8 hide_in;
	u8 hide_for;
	u8 delay;
} ALIGN(4) Spring;


#define SAVE_DATA_MAGIC 0xAAAA


typedef struct
{
        int magic;
        u8 seconds;
        u8 minutes;
        u16 deaths;
        Point room;
} ALIGN(4) SaveData;

//-- object lists --
//------------------
Big_Chest big_chest = { 0 };
Chest chest = { 0 };
Object fake_wall = { 0 };
Flag flag = { 0 };
Fly_Fruit fly_fruit = { 0 };
Fruit fruit = { 0 };
Object key = { 0 };
Lifeup lifeup = { 0 };
Message message = { 0 };
Orb orb = { 0 };
Player player = { 0 };
Player_Spawn player_spawn = { 0 };
Room_Title room_title = { 0 };

#define MAX_BALLOONS 6
Balloon balloons[MAX_BALLOONS] = { 0 };

#define MAX_DEAD_PARTICLES 8
Dead_Particle dead_particles[MAX_DEAD_PARTICLES] = { 0 };

#define MAX_FALL_FLOORS 12
Fall_Floor fall_floors[MAX_FALL_FLOORS] = { 0 };

#define MAX_HAIR 5
Hair hair[MAX_HAIR] = { 0 };

#define MAX_PARTICLES 24
Particle particles[MAX_PARTICLES] = { 0 };

#define MAX_PLATFORMS 10
Platform platforms[MAX_PLATFORMS] = { 0 };

#define MAX_SMOKE 10
Object smoke[MAX_SMOKE] = { 0 };

#define MAX_SPRINGS 5
Spring springs[MAX_SPRINGS] = { 0 };


//-- globals --
//-------------
Point room = { 0, 0 };
u8 freeze = 0;
u8 shake = 0;
bool cheated = false;
bool can_shake = true;
bool will_restart = false;
u8 delay_restart = 0;
bool got_fruit[32] = { false };
bool has_dashed = false;
u8 sfx_timer = 0;
bool has_key = false;
bool pause_player = false;
bool flash_bg = false;
u8 music_timer = 0;

#define k_left KEY_LEFT
#define k_right KEY_RIGHT
#define k_up KEY_UP
#define k_down KEY_DOWN
#define k_jump KEY_A
#define k_dash KEY_B

static u16 kheld = 0;
static u16 kdown = 0;

bool start_game = false;

u8 frames = 0;
u8 seconds = 0;
u8 minutes = 0;
u8 max_djump = 1;
s16 start_game_flash = 0;
u16 deaths = 0;
bool new_bg = false;


//-- room function prototypes --
//-------------------------
#define level_index() (room.x%8+room.y*8)
#define is_title() (level_index()==31)
void restart_room();
void load_room(u8 x, u8 y);
void next_room();


//-- helper functions --
//----------------------
#define btn(x) ((kheld & (x))? 1: 0)
#define btnp(x) ((kdown & (x))? 1: 0)

#define abs(x) ( ((x) < 0)? -(x): (x) )
#define cos(x) (COS[(u16)((x)*360) % 360])
#define sin(x) (SIN[(u16)((x)*360) % 360])

#define clamp(val,a,b) ( max((a), min((b), (val))) )
#define appr(val,target,amount) ( ((val) > (target))? max((val) - (amount), (target)): min((val) + (amount), (target)))
#define sign(v) ( ((v) > 0) - ((v) < 0) )
#define maybe() (rndi(2) < 1)

u8 tile_at(u8 x, u8 y)
{
	u16* map_adr = MAP_BASE_ADR(4);
	return map_adr[x + (y*32)];
}

bool tile_flag_at(u8 x, u8 y, u8 w, u8 h, u8 flag)
{
	for (u8 i = max(0,flr(x/8)); i <= min(15,(x+w-1)/8); i++) {
		for (u8 j = max(0,flr(y/8)); j <= min(15,(y+h-1)/8); j++) {
			if (fget(tile_at(i,j),flag)) {
				return true;
			}
		}
	}
	return false;
}

inline bool ice_at(u8 x, u8 y, u8 w, u8 h)
{
	return tile_flag_at(x,y,w,h,4);
}

inline bool is_ice(Object* obj, u8 ox, u8 oy)
{
	return ice_at(obj->x+obj->hitbox.x+ox,obj->y+obj->hitbox.y+oy,obj->hitbox.w,obj->hitbox.h);
}

bool spikes_at(u8 x, u8 y, u8 w, u8 h, float xspd, float yspd)
{
	for (u8 i = max(0,flr(x/8)); i <= min(15,(x+w-1)/8); i++) {
		for (u8 j = max(0,flr(y/8)); j <= min(15,(y+h-1)/8); j++) {
			u8 tile = tile_at(i,j);

			if (tile == 17 || tile == 27 || tile == 43 || tile == 59)
			{
				if (tile == 17 && ((y+h-1)%8>=6 || y+h == j*8+8) && yspd >= 0) {
					return true;
				}
				else if (tile == 27 && y%8 <= 2 && yspd <= 0) {
					return true;
				}
				else if (tile == 43 && x%8 <= 2 && xspd <= 0) {
					return true;
				}
				else if (tile == 59 && ((x+w-1)%8 >= 6 || x+w == i*8+8) && xspd >= 0) {
					return true;
				}
			}
		}
	}

	return false;
}


void psfx(u8 num)
{
	if (sfx_timer <= 0) {
		sfx(num);
	}
}


inline Object* collide_check(Object* obj, Object* other, s8 ox, s8 oy)
{
	if (other != NULL)
	{
		if (!other->collideable ||
			other->y+other->hitbox.y >= obj->y+obj->hitbox.y+obj->hitbox.h+oy ||
			other->y+other->hitbox.y+other->hitbox.h <= obj->y+obj->hitbox.y+oy ||
			other->x+other->hitbox.x > obj->x+obj->hitbox.x+obj->hitbox.w+ox ||
			other->x+other->hitbox.x+other->hitbox.w <= obj->x+obj->hitbox.x+ox)
		{}
		else
			return other;
	}

	return NULL;
}

Object* collide(Object* obj, u8 type, s8 ox, s8 oy)
{
	switch (type)
	{
		case type_player:
			return collide_check(obj,&(player.obj),ox,oy);
			break;

		case type_fake_wall:
		{
			if (fake_wall.active)
				return collide_check(obj,&fake_wall,ox,oy);
		}
		break;

		case type_platform:
		{
			Object* other;
			for (u8 i = 0; i < MAX_PLATFORMS; i++)
			{
				other = &(platforms[i].obj);
				if (!other->active)
					break;
				else
				{
					Object* ret = collide_check(obj,other,ox,oy);
					if (ret != NULL)
						return ret;
				}
			}
		}
		break;

		case type_spring:
		{
			Object* other;
			for (u8 i = 0; i < MAX_SPRINGS; i++)
			{
				other = &(springs[i].obj);
				if (!other->active)
					break;
				else
				{
					Object* ret = collide_check(obj,other,ox,oy);
					if (ret != NULL)
						return ret;
				}
			}
		}
		break;

		case type_fall_floor:
		{
			Object* other;
			for (u8 i = 0; i < MAX_FALL_FLOORS; i++)
			{
				other = &(fall_floors[i].obj);
				if (!other->active)
					break;
				else
				{
					Object* ret = collide_check(obj,other,ox,oy);
					if (ret != NULL)
						return ret;
				}
			}
		}
		break;
	}

	return NULL;
}

inline bool check(Object* obj, u8 type, s8 ox, s8 oy)
{
	return collide(obj, type, ox, oy) != NULL;
}

inline bool solid_at(u8 x, u8 y, u8 w, u8 h)
{
	return tile_flag_at(x,y,w,h,0);
}

inline bool platform_check()
{
	Platform* p;
	for (u8 i = 0; i < MAX_PLATFORMS; i++)
	{
		p = &(platforms[i]);

		if (!p->obj.active)
			return false;

		if (player.obj.y != p->obj.y - 8 ||
			player.obj.x + player.obj.hitbox.x >= p->obj.x + 16 || player.obj.x + player.obj.hitbox.x + player.obj.hitbox.w <= p->obj.x)
			continue;

		return true;
	}

	return false;
}

bool is_solid(Object* obj, s8 ox, s8 oy)
{
	if (oy > 0 && platform_check()) {
		return true;
	}
	return solid_at(obj->x+obj->hitbox.x+ox,obj->y+obj->hitbox.y+oy,obj->hitbox.w,obj->hitbox.h)
		|| check(obj,type_fake_wall,ox,oy);
}

void draw_time(u8 x, u8 y)
{
	char str[9] = "00:00:00";

	u8 s = seconds;
	u8 m = minutes%60;
	u8 h = flr(minutes/60.f);

	//rectfill
	str[0] = (h/10) + '0';
	str[1] = (h%10) + '0';

	str[3] = (m/10) + '0';
	str[4] = (m%10) + '0';

	str[6] = (s/10) + '0';
	str[7] = (s%10) + '0';

	print(str,x+1,y+1,7);
}


//-- entry point --
//-----------------
void title_screen();


#define CARTRIDGE_RAM ((u8*)0x0E000000)


int sram_load(SaveData* output)
{
	// NOTE: if you try to read/write sram from a base pointer + offset, the
	// compiler is not guaranteed to emit the correct asm instructions for
	// interacting with the save memory. You should put the target cartridge
	// address in a local variable.
	u8* save_mem = CARTRIDGE_RAM;

	for (int i = 0; i < sizeof *output; ++i)
	{
		((u8*)output)[i] = *save_mem++;
	}

	if (output->magic == SAVE_DATA_MAGIC)
	{
		return 1;
	}

	return 0;
}


int sram_save()
{
	SaveData save;
	save.magic = SAVE_DATA_MAGIC;
	save.seconds = seconds;
	save.minutes = minutes;
	save.deaths = deaths;
	save.room = room;

	u8* save_mem = CARTRIDGE_RAM;

	// Note: cartridge ram has an 8-bit data bus, so save data _must_ be
	// written one byte at a time.
	for (int i = 0; i < sizeof save; ++i)
	{
		*save_mem++ = ((u8*)&save)[i];
	}

	SaveData checksum;
	if (!sram_load(&checksum))
	{
		return 0;
	}

	for (int i = 0; i < sizeof checksum; ++i)
	{
		if (*((u8*)&checksum) != *((u8*)&save))
		{
			return 0;
		}
	}

	return 1;
}


void sram_erase()
{
	u8* save_mem = CARTRIDGE_RAM;

	for (int i = 0; i < sizeof(SaveData); ++i)
	{
		*save_mem++ = 0;
	}
}


void __init()
{
	for (u8 i = 0; i < MAX_PARTICLES; i++)
	{
		Particle* this = &(particles[i]);
		this->x = rndi(128);
		this->y = rndi(128);
		//this->y = rndi(64);
		this->s = 0;
		if (rndi(3) < 1)
			this->s = 1;
		this->spd = 0.25+rndi(5);
		this->off = rnd(1);
		this->c = 6+flr(0.5+rnd(1));
	}

	SaveData previous_game = {0};

	if (sram_load(&previous_game))
	{
		frames = 0;
		seconds = previous_game.seconds;
		minutes = previous_game.minutes;
		music_timer = 0;
		start_game = false;

		if (room.x == 2 && room.y == 1) {
			music(30,500,7);
		} else if (room.x == 3 && room.y == 1) {
			music(20,500,7);
		} else if (room.x == 4 && room.y == 2) {
			music(30,500,7);
		} else if (room.x == 5 && room.y == 3) {
			music(30,500,7);
		} else {
			music(0,0,7);
		}

		load_room(previous_game.room.x, previous_game.room.y);
	}
	else
	{
		title_screen();
	}
}

void title_screen()
{
        sram_erase();

	for (u8 i = 0; i < 32; i++)
		got_fruit[i] = false;
	frames = 0;
	seconds = 0;
	minutes = 0;
	deaths = 0;
	max_djump = 1;
	start_game = false;
	start_game_flash = 0;
	cheated = false;

	new_bg = false;

	music(40,0,7);

	load_room(7,3);
	player_spawn.active = false;
}

void begin_game()
{
	frames = 0;
	seconds = 0;
	minutes = 0;
	music_timer = 0;
	start_game = false;
	music(0,0,7);
	load_room(0,0);
}


//-- object class --
//----------------------
void init_object(Object* obj, u8 type, u8 x, u8 y)
{
	obj->active = true;

	obj->type = type;
	obj->collideable = true;
	obj->solids = true;

	obj->spr = type;
	obj->flip.x = false;
	obj->flip.y = false;

	obj->x = x;
	obj->y = y;
	obj->hitbox.x = 0;
	obj->hitbox.y = 0;
	obj->hitbox.w = 8;
	obj->hitbox.h = 8;

	obj->spd.x = 0;
	obj->spd.y = 0;
	obj->rem.x = 0;
	obj->rem.y = 0;
}

void draw_object(Object* obj)
{
	spr(obj->spr,obj->x,obj->y,1,0,obj->flip.x,obj->flip.y);
}


//-- effects --
//-------------
void create_hair(s16 x, s16 y)
{
	for (u8 i = 0; i < MAX_HAIR; i++)
	{
		hair[i].x = x;
		hair[i].y = y;
		hair[i].size = max(1,min(3,4-i));
	}
}

void set_hair_color(u8 djump)
{
	if (djump == 1)
		pal(8, 8, PAL_PLAYER);
	else if (djump == 2)
		pal(8, 7+flr((frames/3)%2)*4, PAL_PLAYER);
	else
		pal(8,12, PAL_PLAYER);
}

void draw_hair(float x, float y, s8 facing)
{
	Hair temp = { x+4-facing*2, y+(btn(k_down)? 4: 3), 3 };
	Hair* last = &temp;
	Hair* h;

	for (u8 i = 0; i < MAX_HAIR; i++)
	{
		h = &(hair[i]);
		h->x += (last->x-h->x)/1.5f;
		h->y += (last->y+0.5-h->y)/1.5f;
		spr(131+h->size, h->x-3, h->y-3, 1, PAL_PLAYER, 0, 0);
		last = h;
	}
}


void lifeup_init(Lifeup* this, s16 x, s16 y)
{
	init_object(&(this->obj),type_lifeup,x-8,y-4);
	
	this->obj.spd.y = -0.25;
	this->duration = 30;
	this->flash = 0;
	this->obj.solids = false;
}

void lifeup_draw(Lifeup* this)
{
	this->obj.y += this->obj.spd.y;

	this->duration -= 1;
	if (this->duration <= 0) {
		this->obj.active = false;
	}
	else
	{
		if (frames%2 == 0)
			this->flash += 1;

		print("1000",this->obj.x,this->obj.y,7+(this->flash%2));
	}
}


void smoke_init(s16 x, s16 y)
{
	for (u8 i = 0; i < MAX_SMOKE; i++)
	{
		Object* this = &(smoke[i]);
		if (!this->active)
		{			
			init_object(this, type_smoke, x, y);

			this->spr = 29;
			this->spd.y = -0.1;
			this->spd.x = 0.3 + rnd(0.2);
			this->x += -1 + rndi(2);
			this->y += -1 + rndi(2);
			this->flip.x = maybe();
			this->flip.y = maybe();
			this->solids = false;

			break;
		}
	}
}

void smoke_update(Object* this)
{
	if (frames % 5 == 0)
	{
		this->spr += 1;
		if (this->spr >= 32) {
			this->active = false;
		}
	}
}


//-- player entity --
//-------------------
void move_x(Object* obj, s16 amount, u8 start)
{
	if (obj->solids) {
		s8 step = sign(amount);
		for (u8 i = start; i <= abs(amount); i++) {
			if (!is_solid(obj,step,0))
				obj->x += step;
			else
			{
				obj->spd.x = 0;
				obj->rem.x = 0;
				break;
			}
		}
	}
	else
		obj->x += amount;
}

void move_y(Object* obj, s16 amount)
{
	if (obj->solids) {
		s8 step = sign(amount);
		for (u8 i = 0; i <= abs(amount); i++) {
			if (!is_solid(obj,0,step))
				obj->y += step;
			else
			{
				obj->spd.y = 0;
				obj->rem.y = 0;
				break;
			}
		}
	}
	else
		obj->y += amount;
}

void move(Object* obj, float ox, float oy)
{
	s16 amount;
	//-- [x] get move amount
	obj->rem.x += ox;
	amount = flr(obj->rem.x + 0.5f);
	obj->rem.x -= amount;
	move_x(obj,amount,0);

	//-- [y] get move amount
	obj->rem.y += oy;
	amount = flr(obj->rem.y + 0.5f);
	obj->rem.y -= amount;
	move_y(obj,amount);
}


void player_init(Player* this, u8 x, u8 y)
{
	init_object(&(this->obj), type_player, x, y);

	this->p_jump = false;
	this->p_dash = false;
	this->grace = 0;
	this->jbuffer = 0;
	this->djump = max_djump;
	this->dash_time = 0;
	this->dash_effect_time = 0;
	this->dash_target.x = 0;
	this->dash_target.y = 0;
	this->dash_accel.x = 0;
	this->dash_accel.y = 0;
	this->obj.hitbox.x = 1;
	this->obj.hitbox.y = 3;
	this->obj.hitbox.w = 6;
	this->obj.hitbox.h = 5;
	this->spr_off = 0;
	this->was_on_ground = false;

	create_hair(this->obj.x, this->obj.y);
}

void kill_player()
{
	sfx_timer = 12;
	sfx(0);
	deaths += 1;
	shake = 10;
	player.obj.active = false;
	for (u8 dir = 0; dir <= 7; dir++)
	{
		Dead_Particle* obj = &(dead_particles[dir]);

		float angle = (dir/8.f);

		obj->active = true;
		obj->x = player.obj.x+4;
		obj->y = player.obj.y+4;
		obj->t = 10;
		obj->spd.x = sin(angle)*3;
		obj->spd.y = cos(angle)*3;
	}
	restart_room();
}

void player_update(Player* this)
{
	if (pause_player) return;

	s8 input = btn(k_right) - btn(k_left);

	//-- spikes collide
	if (spikes_at(this->obj.x+this->obj.hitbox.x,this->obj.y+this->obj.hitbox.y,this->obj.hitbox.w,this->obj.hitbox.h,this->obj.spd.x,this->obj.spd.y)) {
		kill_player(); }

	//-- bottom death
	if (this->obj.y > 128) {
		kill_player(); }

	bool on_ground = is_solid(&(this->obj),0,1);
	bool on_ice = is_ice(&(this->obj),0,1);

	//-- smoke particles
	if (on_ground && !this->was_on_ground) {
		smoke_init(this->obj.x,this->obj.y+4);
	}

	bool jump = btn(k_jump) && !this->p_jump;
	this->p_jump = btn(k_jump);
	if (jump)
		this->jbuffer = 4;
	else if (this->jbuffer > 0)
		this->jbuffer -= 1;

	bool dash = btn(k_dash) && !this->p_dash;
	this->p_dash = btn(k_dash);

	if (on_ground) {
		this->grace = 6;
		if (this->djump < max_djump) {
			psfx(54);
			this->djump = max_djump;
		}
	}
	else if (this->grace > 0) {
		this->grace -= 1;
	}

	if (this->dash_effect_time > 0)
		this->dash_effect_time -= 1;

	if (this->dash_time > 0) {
		smoke_init(this->obj.x,this->obj.y);
		this->dash_time -= 1;
		this->obj.spd.x = appr(this->obj.spd.x,this->dash_target.x,this->dash_accel.x);
		this->obj.spd.y = appr(this->obj.spd.y,this->dash_target.y,this->dash_accel.y);
	}
	else
	{
		//-- move
		u8 maxrun = 1;
		float accel = 0.6;
		float deccel = 0.15;

		if (!on_ground)
			accel = 0.4;
		else if (on_ice) {
			accel = 0.05;
			if (input == ((this->obj.flip.x)? -1: 1)) {
				accel = 0.05;
			}
		}

		if (abs(this->obj.spd.x) > maxrun)
			this->obj.spd.x = appr(this->obj.spd.x,sign(this->obj.spd.x)*maxrun,deccel);
		else
			this->obj.spd.x = appr(this->obj.spd.x,input*maxrun,accel);

		//--facing
		if (this->obj.spd.x != 0) {
			this->obj.flip.x = (this->obj.spd.x < 0);
		}

		//-- gravity
		float maxfall = 2;
		float gravity = 0.21;

		if (abs(this->obj.spd.y) <= 0.15) {
			gravity *= 0.5;
		}

		//-- wall slide
		if (input != 0 && is_solid(&(this->obj),input,0) && !is_ice(&(this->obj),input,0)) {
			maxfall = 0.4;
			if (rndi(10) < 2) {
				smoke_init(this->obj.x+input*6,this->obj.y);
			}
		}

		if (!on_ground) {
			this->obj.spd.y = appr(this->obj.spd.y,maxfall,gravity);
		}

		//-- jump
		if (this->jbuffer > 0) {
			if (this->grace > 0) {
				//-- normal jump
				psfx(1);
				this->jbuffer = 0;
				this->grace = 0;
				this->obj.spd.y = -2;
				smoke_init(this->obj.x,this->obj.y+4);
			}
			else
			{
				//-- wall jump
				s8 wall_dir = is_solid(&(this->obj),3,0) - is_solid(&(this->obj),-3,0);
				if (wall_dir != 0) {
					psfx(2);
					this->jbuffer = 0;
					this->obj.spd.y = -2;
					this->obj.spd.x = -wall_dir*(maxrun+1);
					if (!is_ice(&(this->obj),wall_dir*3,0)) {
						smoke_init(this->obj.x+wall_dir*6,this->obj.y);
					}
				}
			}
		}

		//-- dash
		u8 d_full = 5;
		float d_half = d_full*0.70710678118f;

		if (this->djump > 0 && dash) {
			smoke_init(this->obj.x,this->obj.y);
			this->djump -= 1;
			this->dash_time = 4;
			has_dashed = true;
			this->dash_effect_time = 10;
			s8 v_input = (btn(k_down) - btn(k_up));
			if (input != 0) {
				if (v_input != 0) {
					this->obj.spd.x = input*d_half;
					this->obj.spd.y = v_input*d_half;
				}
				else
				{
					this->obj.spd.x = input*d_full;
					this->obj.spd.y = 0;
				}
			}
			else
			{
				if (v_input != 0) {
					this->obj.spd.x = 0;
					this->obj.spd.y = v_input*d_full;
				}
				else
				{
					this->obj.spd.x = ((this->obj.flip.x)? -1: 1);
					this->obj.spd.y = 0;
				}
			}

			psfx(3);
			freeze = 2;
			shake = 6;
			this->dash_target.x = 2*sign(this->obj.spd.x);
			this->dash_target.y = 2*sign(this->obj.spd.y);
			this->dash_accel.x = 1.5;
			this->dash_accel.y = 1.5;

			if (this->obj.spd.y < 0) {
				this->dash_target.y *= 0.75;
			}

			if (this->obj.spd.y != 0) {
				this->dash_accel.x *= 0.70710678118f;
			}
			if (this->obj.spd.x != 0) {
				this->dash_accel.y *= 0.70710678118f;
			}
		}
		else if (dash && this->djump <= 0) {
			psfx(9);
			smoke_init(this->obj.x,this->obj.y);
		}
	}

	//-- animation
	if (frames%4 == 0)
		this->spr_off += 1;
	
	if (!on_ground) {
		if (is_solid(&(this->obj),input,0))
			this->obj.spr = 5;
		else
			this->obj.spr = 3;
	}
	else if (btn(k_down))
		this->obj.spr = 6;
	else if (btn(k_up))
		this->obj.spr = 7;
	else if (this->obj.spd.x == 0 || (!btn(k_left) && !btn(k_right)))
		this->obj.spr = 1;
	else
		this->obj.spr = 1+this->spr_off%4;

	//-- next level
	//if (this->obj.y < -4 && level_index() < 30) { next_room(); }

	//-- was on the ground
	this->was_on_ground = on_ground;
}

void player_draw(Player* this)
{
	//-- clamp in screen
	if (this->obj.x < -1 || this->obj.x > 121) {
		this->obj.x = clamp(this->obj.x,-1,121);
		this->obj.spd.x = 0;
	}

	set_hair_color(this->djump);
	spr(this->obj.spr,this->obj.x,this->obj.y,1,PAL_PLAYER,this->obj.flip.x,this->obj.flip.y);
	draw_hair(this->obj.x,this->obj.y,(this->obj.flip.x)? -1: 1);

	//-- next level
	if (this->obj.y < -4 && level_index() < 30) { next_room(); }
}


void player_spawn_init(Player_Spawn* this, u8 x, u8 y)
{
	sfx(4);
	this->active = true;
	this->x = x * 8;
	this->y = y * 8;
	this->spr = 3;
	this->target.x = this->x;
	this->target.y = this->y;
	this->y = 128;
	this->spd.y = -4;
	this->state = 0;
	this->delay = 0;
	this->solids = false;
	this->flip.x = false;
	this->flip.y = false;
	create_hair(this->x, this->y);
}

void player_spawn_update(Player_Spawn* this)
{
	//-- jumping up
	if (this->state == 0) {
		this->y += this->spd.y;
		if (this->y < this->target.y+16) {
			this->state = 1;
			this->delay = 3;
		}
	}
	//-- falling
	else if (this->state == 1) {
		this->spd.y += 0.5;
		this->y += this->spd.y;
		if (this->spd.y > 0 && this->delay > 0) {
			this->spd.y = 0;
			this->delay -= 1;
		}
		if (this->spd.y > 0 && this->y > this->target.y) {
			this->y = this->target.y;
			this->spd.x = 0;
			this->spd.y = 0;
			this->state = 2;
			this->delay = 5;
			shake = 5;
			smoke_init(this->x,this->y+4);
			sfx(5);
		}
	}
	//-- landing
	else if (this->state == 2) {
		this->delay -= 1;
		this->spr = 6;
		if (this->delay == 0) {
			this->active = false;
			player_init(&player,this->x,this->y);
		}
	}
}

void player_spawn_draw(Player_Spawn* this)
{
	set_hair_color(max_djump);
	spr(this->spr,this->x,this->y,1,PAL_PLAYER,this->flip.x,this->flip.y);
	draw_hair(this->x,this->y,1);
}


//-- object functions --
//----------------------
void orb_init(u8 x, u8 y)
{
	init_object(&(orb.obj),type_orb,x,y);
	orb.obj.spd.y = -4;
	orb.obj.solids = false;
}

void orb_draw(Orb* this)
{
	this->obj.spd.y = appr(this->obj.spd.y,0,0.5);
	if (this->obj.spd.y == 0) {
		if (collide_check(&(this->obj), &(player.obj), 0, 0)) {
			music_timer = 45;
			sfx(51);
			freeze = 10;
			shake = 10;
			this->obj.active = false;
			max_djump = 2;
			player.djump = 2;
		}
	}

	spr(102,this->obj.x,this->obj.y,1,0,0,0);

	float off = frames / 30.f;
	for (u8 i = 0; i < 8; i++)
		spr(140,this->obj.x+1+cos(off+i/8.f)*8,this->obj.y+1+sin(off+i/8.f)*8,1,0,0,0);
}


void big_chest_init(u8 x, u8 y)
{
	init_object(&(big_chest.obj),type_big_chest,x,y);
	big_chest.state = 0;
	big_chest.obj.hitbox.w = 16;
	big_chest.obj.hitbox.h = 16;
	big_chest.timer = 0;

	for (u8 i = 0; i < 50; i++)
	{
		big_chest.particles[i].active = false;
		big_chest.particles[i].x = 0;
		big_chest.particles[i].y = 0;
		big_chest.particles[i].h = 0;
		big_chest.particles[i].spd = 0;
	}
}

void big_chest_draw(Big_Chest* this)
{
	if (this->state == 0) {
		if (collide_check(&(this->obj),&(player.obj),0,0) && is_solid(&(player.obj),0,1)) {
			music(-1,500,7);
			sfx(37);
			pause_player = true;
			player.obj.spd.x = 0;
			player.obj.spd.y = 0;
			this->state = 1;
			smoke_init(this->obj.x, this->obj.y);
			smoke_init(this->obj.x+8, this->obj.y);
			this->timer = 60;
		}
		spr(96,this->obj.x,this->obj.y,1,0,0,0);
		spr(97,this->obj.x+8,this->obj.y,1,0,0,0);
	}
	else if (this->state == 1) {
		this->timer -= 1;
		shake = 5;
		flash_bg = true;
		if (this->timer <= 45) {
			for (u8 i = 0; i < 50; i++) {
				Big_Chest_Particle* p = &(this->particles[i]);
				if (!p->active)
				{
					p->active = true;
					p->x = this->obj.x + 1 + rndi(14);
					p->y = this->obj.y+8;
					p->h = 32+rndi(32);
					p->spd = 8 + rndi(8);
					break;
				}
			}
		}
		if (this->timer <= 0) {
			this->state = 2;

			flash_bg = false;
			new_bg = true;
			orb_init(this->obj.x+4,this->obj.y+4);
			pause_player = false;
		}

		for (u8 i = 0; i < 50; i++)
		{
			Big_Chest_Particle* p = &(this->particles[i]);
			if (p->active)
			{
				p->y -= p->spd;
				if (p->y < 0) p->active = false;

				spr(139,p->x,p->y,1,0,0,0);
				spr(139,p->x,p->y-8,1,0,0,0);
				spr(139,p->x,p->y-16,1,0,0,0);
				spr(139,p->x,p->y-24,1,0,0,0);
			}
		}
	}
	spr(112,this->obj.x,this->obj.y+8,1,0,0,0);
	spr(113,this->obj.x+8,this->obj.y+8,1,0,0,0);
}


void add_balloon(u8 x, u8 y)
{
	Balloon* this;
	for (u8 i = 0; i < MAX_BALLOONS; i++)
	{
		this = &(balloons[i]);
		if (this->obj.active == false)
		{
			init_object(&(this->obj), type_balloon, x, y);

			this->offset = rnd(1);
			this->start = this->obj.y;
			this->timer = 0;
			this->obj.hitbox.x = -1;
			this->obj.hitbox.y = -1;
			this->obj.hitbox.w = 10;
			this->obj.hitbox.h = 10;

			break;
		}
	}
}

void balloon_update(Balloon* this)
{
	if (this->obj.spr == 22) {
		this->offset += 0.01;
		this->obj.y = this->start + sin(this->offset)*2;
		if (player.djump < max_djump) {
			if (collide_check(&(this->obj), &(player.obj), 0, 0)) {
				psfx(6);
				smoke_init(this->obj.x, this->obj.y);
				player.djump = max_djump;
				this->obj.spr = 0;
				this->timer = 60;
			}
		}
	}
	else if (this->timer > 0) {
		this->timer -= 1;
	}
	else
	{
		psfx(7);
		smoke_init(this->obj.x, this->obj.y);
		this->obj.spr = 22;
	}
}

void balloon_draw(Balloon* this)
{
	if (this->obj.spr == 22) {
		spr(this->obj.spr, this->obj.x, this->obj.y, 1, 0, 0, 0);
		spr(13+((int)(this->offset*8)%3), this->obj.x, this->obj.y+6, 1, 0, 0, 0);
	}
}


void break_spring(Spring* obj)
{
	obj->hide_in = 15;
}

void break_fall_floor(Fall_Floor* obj)
{
	if (obj->state == 0) {
		psfx(15);
		obj->state = 1;
		obj->delay = 15; //--how long until it falls
		smoke_init(obj->obj.x,obj->obj.y);
		Spring* hit = (Spring*)collide(&(obj->obj),type_spring,0,-1);
		if (hit != NULL) {
			break_spring(hit); 
		}
	}
}


void add_spring(u8 x, u8 y)
{
	for (u8 i = 0; i < MAX_SPRINGS; i++)
	{
		Spring* this = &(springs[i]);
		if (this->obj.active == false)
		{
			init_object(&(this->obj), type_spring, x, y);
			this->hide_in = 0;
			this->hide_for = 0;

			break;
		}
	}
}

void spring_update(Spring* this)
{
	if (this->hide_for > 0) {
		this->hide_for -= 1;
		if (this->hide_for <= 0) {
			this->obj.spr = 18;
			this->delay = 0;
		}
	}
	else if (this->obj.spr == 18) {
		if (player.obj.spd.y >= 0)
		{
			if (collide_check(&(this->obj), &(player.obj), 0, 0)) {
				this->obj.spr = 19;
				player.obj.y = this->obj.y-4;
				player.obj.spd.x *= 0.2;
				player.obj.spd.y = -3;
				player.djump = max_djump;
				this->delay = 10;
				smoke_init(this->obj.x, this->obj.y);

				//-- breakable below us
				Fall_Floor* below = (Fall_Floor*)collide(&(this->obj),type_fall_floor,0,1);
				if (below != NULL) {
					break_fall_floor(below);
				}

				psfx(8);
			}
		}
	}
	else if (this->delay > 0) {
		this->delay -= 1;
		if (this->delay <= 0) {
			this->obj.spr = 18;
		}
	}
	//-- begin hiding
	if (this->hide_in > 0) {
		this->hide_in -= 1;
		if (this->hide_in <= 0) {
			this->hide_for = 60;
			this->obj.spr = 0;
		}
	}
}


void add_fall_floor(u8 x, u8 y, u16* map_adr)
{
	Fall_Floor* this;
	for (u8 i = 0; i < MAX_FALL_FLOORS; i++)
	{
		this = &(fall_floors[i]);
		if (!this->obj.active)
		{
			init_object(&(this->obj), type_fall_floor, x, y);

			this->state = 0;
			this->solid = true;
			this->delay = 0;
			this->map_adr = map_adr;

			this->obj.hitbox.x = -1;
			this->obj.hitbox.y = -1;
			this->obj.hitbox.w = 10;
			this->obj.hitbox.h = 9;

			break;
		}
	}
}

void fall_floor_update(Fall_Floor* this)
{
	//-- idling
	if (this->state == 0) {
		if (collide_check(&(this->obj),&(player.obj),0,0))
			break_fall_floor(this);
	}
	//-- shaking
	else if (this->state == 1) {
		this->delay -= 1;
		*(this->map_adr) = 23+(15-this->delay)/5;
		if (this->delay <= 0) {
			this->state = 2;
			this->delay = 60; //--how long it hides for
			this->obj.collideable = false;
			*(this->map_adr) = 0;
		}
	}
	//-- invisible, waiting to reset
	else if (this->state == 2) {
		if (this->delay > 0)
			this->delay -= 1;
		else
		{
			if (!check(&(this->obj),type_player,0,0)) {
				psfx(7);
				this->state = 0;
				this->obj.collideable = true;
				*(this->map_adr) = 23;
				smoke_init(this->obj.x,this->obj.y);
			}
		}
	}
}

void fall_floor_draw(Fall_Floor* this)
{
	if (!new_bg)
		return;

	if (this->state != 2) {
		if (this->state != 1)
			spr(23,this->obj.x,this->obj.y,1,0,0,0);
		else
			spr(23+(15-this->delay)/5,this->obj.x,this->obj.y,1,0,0,0);
	}
}


void fruit_init(u8 x, u8 y)
{
	if (got_fruit[1+level_index()] == true)
	{
		fruit.obj.active = false;
		return;
	}

	init_object(&(fruit.obj), type_fruit, x, y);
	fruit.start = fruit.obj.y;
	fruit.off = 0;
}

void fruit_update(Fruit* this)
{
	if (collide_check(&(this->obj), &(player.obj), 0, 0)) {
		player.djump = max_djump;
		sfx_timer = 20;
		sfx(13);
		got_fruit[1+level_index()] = true;
		lifeup_init(&lifeup,this->obj.x,this->obj.y);
		this->obj.active = false;
	}
	this->off += 1;
	this->obj.y = this->start + sin(this->off/40.f)*2.5;
}


void fly_fruit_init(u8 x, u8 y)
{
	if (got_fruit[1+level_index()] == true)
	{
		fly_fruit.obj.active = false;
		return;
	}

	init_object(&(fly_fruit.obj), type_fly_fruit, x, y);
	fly_fruit.start = fly_fruit.obj.y;
	fly_fruit.fly = false;
	fly_fruit.step = 0.5f;
	fly_fruit.obj.solids = false;
	fly_fruit.sfx_delay = 8;
}

void fly_fruit_update(Fly_Fruit* this)
{
	//--fly away
	if (this->fly) {
		if (this->sfx_delay > 0) {
			this->sfx_delay -= 1;
			if (this->sfx_delay <= 0) {
				sfx_timer = 20;
				sfx(14);
			}
		}
		this->obj.spd.y = appr(this->obj.spd.y, -3.5, 0.25);
		if (this->obj.y < -16) {
			this->obj.active = false;
		}
	}
	//-- wait
	else {
		if (has_dashed) {
			this->fly = true;
		}
		this->step += 0.05;
		this->obj.spd.y = sin(this->step) * 0.5;
	}
	//-- collect
	if (collide_check(&(this->obj), &(player.obj), 0, 0)) {
		player.djump = max_djump;
		sfx_timer = 20;
		sfx(13);
		got_fruit[1+level_index()] = true;
		lifeup_init(&lifeup,this->obj.x,this->obj.y);
		this->obj.active = false;
	}
}

void fly_fruit_draw(Fly_Fruit* this)
{
	u8 off = 0;
	if (!this->fly) {
		float dir = sin(this->step);
		if (dir < 0) {
			off = 1 + max(0, sign(this->obj.y-this->start));
		}
	}
	else {
		off = (int)(off+0.25) % 3;
	}
	spr(45+off, this->obj.x-6, this->obj.y-2, 1, 0, true, false);
	spr(this->obj.spr, this->obj.x, this->obj.y, 1, 0, 0, 0);
	spr(45+off, this->obj.x+6, this->obj.y-2, 1, 0, false, false);
}


void fake_wall_init(u8 x, u8 y)
{
	if (got_fruit[1+level_index()] == true)
	{
		fake_wall.active = false;
		return;
	}

	init_object(&fake_wall, type_fake_wall, x, y);

	fake_wall.hitbox.x = 0;
	fake_wall.hitbox.y = 0;
	fake_wall.hitbox.w = 16;
	fake_wall.hitbox.h = 16;
}

void fake_wall_update()
{
	if (player.dash_effect_time > 0) {

		fake_wall.hitbox.x = -1;
		fake_wall.hitbox.y = -1;
		fake_wall.hitbox.w = 18;
		fake_wall.hitbox.h = 18;

		if (collide_check(&fake_wall, &(player.obj), 0, 0)) {
			player.obj.spd.x = -sign(player.obj.spd.x)*1.5;
			player.obj.spd.y = -1.5;
			player.dash_time = 0;
			sfx_timer = 20;
			sfx(16);
			fake_wall.active = false;
			smoke_init(fake_wall.x, fake_wall.y);
			smoke_init(fake_wall.x+8, fake_wall.y);
			smoke_init(fake_wall.x, fake_wall.y+8);
			smoke_init(fake_wall.x+4, fake_wall.y+4);
			fruit_init(fake_wall.x+4, fake_wall.y+4);
		}

		fake_wall.hitbox.x = 0;
		fake_wall.hitbox.y = 0;
		fake_wall.hitbox.w = 16;
		fake_wall.hitbox.h = 16;
	}
}

void fake_wall_draw()
{
	Object* this = &fake_wall;
	spr(64, this->x, this->y, 1, 0, 0, 0);
	spr(65, this->x+8, this->y, 1, 0, 0, 0);
	spr(80, this->x, this->y+8, 1, 0, 0, 0);
	spr(81, this->x+8, this->y+8, 1, 0, 0, 0);
}


void flag_init(u8 x, u8 y)
{
	init_object(&(flag.obj), type_flag, x + 5, y);
	flag.score = 0;
	flag.show = false;

	for (u8 i = 0; i < 32; i++)
	{
		if (got_fruit[i])
			flag.score += 1;
	}
}

void flag_draw(Flag* this)
{
	//summit curtains
	if (player.obj.active)
	{
		u8 diff = player.obj.x;
		if (player.obj.x > 64)
			diff = 128 - player.obj.x;
		
		if (diff > 36)
		{
			rectfill(0,0,3,16,0);
			rectfill(13,0,3,16,0);
		}
		else if (diff > 24)
		{
			rectfill(0,0,2,16,0);
			rectfill(14,0,2,16,0);
		}
		else if (diff > 12)
		{
			rectfill(0,0,1,16,0);
			rectfill(15,0,1,16,0);
		}
	}

	this->obj.spr = 118 + (frames/5) % 3;
	spr(this->obj.spr,this->obj.x,this->obj.y,2,0,0,0);
	if (this->show) {
		rectfill(4,1,8,4,0);
		spr(26,55,6+6,0,0,0,0);

		{
			char str[8] = "x  ";
			if (this->score < 10)
			{
				str[1] = (this->score%10) + '0';
			}
			else
			{
				str[1] = (this->score/10) + '0';
				str[2] = (this->score%10) + '0';
			}
			print(str,64-4,9+6,7);
		}

		draw_time(49-4,16+6);
		
		{
			char str[16] = "deaths:9999";
			s8 xoff = 4;
			
			if (cheated)
			{
				str[0] = 'c';
				str[1] = 'h';
				str[2] = 'e';
				str[3] = 'a';
				str[4] = 't';
				str[5] = 'e';
				str[6] = 'r';
			}

			if (deaths < 9999)
			{
				u16 temp = deaths;

				u8 thousands = temp/1000;
				temp -= thousands * 1000;

				u8 hundreds = temp/100;
				temp -= hundreds * 100;

				u8 tens = temp/10;
				temp -= tens * 10;

				u8 ones = temp%10;

				if (deaths >= 1000)
				{
					str[7] = thousands + '0';
					str[8] = hundreds + '0';
					str[9] = tens + '0';
					str[10] = ones + '0';
				}
				else if (deaths >= 100)
				{
					str[7] = hundreds + '0';
					str[8] = tens + '0';
					str[9] = ones + '0';
					str[10] = '\0';
					xoff = 2;
				}
				else if (deaths >= 10)
				{
					str[7] = tens + '0';
					str[8] = ones + '0';
					str[9] = '\0';
					xoff = 0;
				}
				else
				{
					str[7] = ones + '0';
					str[8] = '\0';
					xoff = -2;
				}
			}

			print(str,48-xoff-4,24+6,7);
		}

		if (btnp(KEY_START))
			title_screen();
	}
	else if (collide_check(&(this->obj),&(player.obj),0,0)) {
		sfx(55);
		sfx_timer = 30;
		this->show = true;
	}
}


void chest_init(u8 x, u8 y)
{
	if (got_fruit[1+level_index()] == true)
	{
		chest.obj.active = false;
		return;
	}

	init_object(&(chest.obj), type_chest, x, y);
	chest.start = chest.obj.x;
	chest.timer = 20;
}

void chest_update(Chest* this)
{
	if (has_key) {
		if (this->timer > 0)
			this->timer -= 1;
		this->obj.x = this->start-1+rnd(3);
		if (this->timer <= 0) {
			sfx_timer = 20;
			sfx(16);
			fruit_init(this->obj.x, this->obj.y-4);
			this->obj.active = false;
		}
	}
}


void key_update(Object* this)
{
	u8 was = flr(this->spd.x);
	this->spd.x = 9+(sin(frames/30.f)+0.5)*1;
	u8 is = flr(this->spd.x);
	this->spr = is;
	if (is == 10 && is != was) {
		this->flip.x = !this->flip.x;
	}
	if (collide_check(this, &(player.obj), 0, 0)) {
		sfx(23);
		sfx_timer = 10;
		this->active = false;
		has_key = true;
	}
}


void message_init(u8 x, u8 y)
{
	Message* this = &message;

	init_object(&(this->obj), type_message, x, y);
	this->index = 0;
	this->lines = 1;
	this->len = 0;
}

void message_draw(Message* this)
{
	char text[] = "-- celeste mountain --#this memorial to those# perished on the climb";
	if (collide_check(&(this->obj), &(player.obj), 4, 0)) {
		if (text[this->index] != '\0') {
			if (frames%2 == 0)
			{
				this->index += 1;
				this->len += 1;
				if (text[this->index] == '#')
				{
					this->lines += 1;
					this->len = 0;
				}
			}

			if (frames%3 == 0)
				sfx(35);
		}

		for (u8 i = 0; i < this->lines; i++)
			rectfill(2,11,(i+1 < this->lines)? 12: min(12,(this->len/2)+1),i+1,7);

		text[this->index] = '\0';
		print(text,8*2,1+(11*8),0);
	}
	else
	{
		this->index = 0;
		this->lines = 1;
		this->len = 0;
	}
}


void add_platform(u8 x, u8 y, s8 dir)
{
	Platform* this;
	for (u8 i = 0; i < MAX_PLATFORMS; i++)
	{
		this = &(platforms[i]);

		if (!this->obj.active)
		{
			init_object(&(this->obj), type_platform, x, y);

			this->obj.x -= 4;
			this->obj.solids = false;
			this->obj.hitbox.w = 16;
			this->last = this->obj.x;
			this->obj.spd.x = dir*0.65f;

			break;
		}
	}
}

void platform_update(Platform* this)
{
	this->obj.x += this->obj.spd.x;
	if (this->obj.x < -16) { this->obj.x = 128;	}
	else if (this->obj.x > 128) { this->obj.x = -16; }

	if (flr(this->last) != flr(this->obj.x)) {
		if (player.obj.spd.y < 0 || player.obj.y != this->obj.y - 8 ||
			player.obj.x + player.obj.hitbox.x >= this->obj.x + 16 || player.obj.x + player.obj.hitbox.x + player.obj.hitbox.w <= this->obj.x)
		{}
		else {
			move_x(&(player.obj), flr(this->obj.x)-flr(this->last),1);
		}
	}

	this->last = this->obj.x;
}

void platform_draw(Platform* this)
{
	spr(11, this->obj.x, this->obj.y-1, 1, 0, 0, 0);
	spr(12, this->obj.x+8, this->obj.y-1, 1, 0, 0, 0);
}


void room_title_init(Room_Title* this)
{
	this->active = true;
	this->delay = 5;
}

void room_title_draw(Room_Title* this)
{
	this->delay -= 1;
	if (this->delay < -30) {
		this->active = false;
	}
	else if (this->delay < 0) {
		//rectfill
		rectfill(1,1,5,1,0);
		rectfill(3,7,10,2,0);

		if (room.x == 3 && room.y == 1) {
			print("old site",48,62,7);
		} else if (level_index() == 30) {
			print("summit",52,62,7);
		} else {
			char str[8] = " 000 m";
			u16 level = (1+level_index())*100;

			if (level >= 1000)
				str[0] = (level / 1000) + '0';

			str[1] = ((level % 1000) / 100) + '0';

			print(str,52-((level<1000)? 2 : 0),62,7);
		}

		draw_time(4+4,4+4);
	}
}


//-- room functions --
//--------------------
void restart_room()
{
	will_restart = true;
	delay_restart = 15;
}

void load_room(u8 x, u8 y)
{
	has_dashed = false;
	has_key = false;

	//--remove existing objects
	player.obj.active = false;
	fake_wall.active = false;
	key.active = false;
	chest.obj.active = false;
	fruit.obj.active = false;
	fly_fruit.obj.active = false;
	lifeup.obj.active = false;
	room_title.active = false;
	big_chest.obj.active = false;
	orb.obj.active = false;
	flag.obj.active = false;
	message.obj.active = false;

	for (u8 i = 0; i < MAX_SMOKE; i++)
		smoke[i].active = false;

	for (u8 i = 0; i < MAX_FALL_FLOORS; i++)
		fall_floors[i].obj.active = false;

	for (u8 i = 0; i < MAX_SPRINGS; i++)
		springs[i].obj.active = false;

	for (u8 i = 0; i < MAX_BALLOONS; i++)
		balloons[i].obj.active = false;

	for (u8 i = 0; i < MAX_PLATFORMS; i++)
		platforms[i].obj.active = false;

	//--current room
	room.x = x;
	room.y = y;

	//-- entities
	u32 i = level_index() * 16 * 16;
	u16* map_adr = MAP_BASE_ADR(4);

	for (u8 ty = 0; ty < 16; ty++)
	{
		for (u8 tx = 0; tx < 16; tx++)
		{
			u16 tile = MAP_DATA[i];
			*map_adr = 0x0000;

			switch (tile)
			{
				case type_message:
				{
					message_init(tx*8, ty*8);
					*map_adr = tile;
				}
				break;

				case type_big_chest:
					big_chest_init(tx*8, ty*8);
					break;

				case type_big_chest+1:
					break;

				case type_key:
				{
					if (!got_fruit[1+level_index()])
						init_object(&key, type_key, tx*8, ty*8);
				}
				break;

				case type_chest:
					chest_init(tx*8, ty*8);
					break;

				case type_fruit:
					fruit_init(tx*8, ty*8);
					break;

				case type_fly_fruit:
					fly_fruit_init(tx*8, ty*8);
					break;

				case type_player_spawn:
					player_spawn_init(&player_spawn, tx, ty);
					break;

				case type_fall_floor:
					add_fall_floor(tx*8, ty*8, map_adr);
					*map_adr = tile;
					break;

				case type_fake_wall:
					fake_wall_init(tx*8, ty*8);
					break;

				case type_spring:
					add_spring(tx*8, ty*8);
					break;

				case type_balloon:
					add_balloon(tx*8, ty*8);
					break;

				case type_flag:
					flag_init(tx*8, ty*8);
					break;

				case 11:
					add_platform(tx*8, ty*8, -1);
					break;

				case 12:
					add_platform(tx*8, ty*8, 1);
					break;

				default:
					*map_adr = tile;
			}

			i++;
			map_adr++;
		}

		//blank
		for (u8 x = 0; x < 16; x++)
		{
			*map_adr = 0x0000;
			map_adr++;
		}
	}

	if (!is_title())
	{
		room_title_init(&room_title);
	}
}

void next_room()
{
	if (room.x == 2 && room.y == 1) {
		music(30,500,7);
	} else if (room.x == 3 && room.y == 1) {
		music(20,500,7);
	} else if (room.x == 4 && room.y == 2) {
		music(30,500,7);
	} else if (room.x == 5 && room.y == 3) {
		music(30,500,7);
	}

	if (room.x == 7)
		load_room(0,room.y+1);
	else
		load_room(room.x+1,room.y);

        sram_save();
}


//-- update function --
//---------------------
void _update()
{
	frames = ((frames+1)%30);
	if (frames == 0 && level_index()<30) {
		seconds = ((seconds+1)%60);
		if (seconds == 0) {
			minutes += 1;
		}
	}

	if (music_timer > 0) {
		music_timer -= 1;
		if (music_timer <= 0) {
			music(10,0,7);
		}
	}

	if (sfx_timer > 0) {
		sfx_timer -= 1;
	}

	//-- cancel if freeze
	if (freeze > 0 ) { freeze -= 1; return;	}

	//-- screenshake
	if (shake > 0) {
		shake -= 1;
		camera(0,0);
		if (can_shake && shake > 0) {
			camera(-2-rndi(5),-2+rndi(5));
		}
	}

	//-- restart (soon)
	if (will_restart && delay_restart > 0) {
		delay_restart -= 1;
		if (delay_restart <= 0) {
			will_restart = false;
			load_room(room.x,room.y);
		}
	}

	//-- update each object
	if (player_spawn.active)
		player_spawn_update(&player_spawn);

	if (player.obj.active)
	{
		move(&(player.obj),player.obj.spd.x,player.obj.spd.y);
		player_update(&player);
	}

	if (key.active)
		key_update(&key);

	if (chest.obj.active)
		chest_update(&chest);

	if (fruit.obj.active)
		fruit_update(&fruit);

	if (fly_fruit.obj.active)
	{
		move(&(fly_fruit.obj), fly_fruit.obj.spd.x, fly_fruit.obj.spd.y);
		fly_fruit_update(&fly_fruit);
	}

	if (fake_wall.active)
		fake_wall_update();

	for (u8 i = 0; i < MAX_FALL_FLOORS; i++)
	{
		Fall_Floor* this = &(fall_floors[i]);
		if (!this->obj.active)
			break;
		else
			fall_floor_update(this);
	}

	for (u8 i = 0; i < MAX_SPRINGS; i++)
	{
		Spring* this = &(springs[i]);
		if (!this->obj.active)
			break;
		else
			spring_update(this);
	}

	for (u8 i = 0; i < MAX_BALLOONS; i++)
	{
		Balloon* this = &(balloons[i]);
		if (!this->obj.active)
			break;
		else
			balloon_update(this);
	}

	//-- start game
	if (is_title()) {
		if (!start_game && (btn(k_jump) || btn(k_dash) || btnp(KEY_START))) {
			music(-1,0,0);
			start_game_flash = 50;
			start_game = true;
			sfx(38);
			if (btn(KEY_L) && btn(KEY_R))
				cheated = true;
		}

		if (start_game) {
			start_game_flash -= 1;
			if (start_game_flash <= -30) {
				begin_game();
				if (cheated)
					max_djump = 2;
			}
		}
	}
}

//-- drawing functions --
//-----------------------
static s8 cloud_scroll[2] = { 0 };

void _draw()
{
	if (freeze > 0) return;

	//-- reset all palette values
	pal(0,0,0);

	//pal(0,5,PAL_BG);
	pal(15,0,PAL_BG);

	//-- credits
	if (is_title()) {
		pal(1,0,PAL_BG); //hide clouds

		print("press start",42,80,5);
		print("matt thorson",40,96,5);
		print("noel berry",44,102,5);
	}

	//-- start game flash
	if (start_game) {
		u8 c = 10;
		if (start_game_flash > 10) {
			if (frames%10<5) {
				c = 7;
			}
		}
		else if (start_game_flash > 5) {
			c = 2;
		}
		else if (start_game_flash > 0) {
			c = 1;
		}
		else {
			c = 0;
		}

		if (c < 10) {
			pal(6, c,PAL_BG | PAL_SPRITES | PAL_TEXT);
			pal(12,c,PAL_BG | PAL_SPRITES | PAL_TEXT);
			pal(13,c,PAL_BG | PAL_SPRITES | PAL_TEXT);
			pal(5, c,PAL_BG | PAL_SPRITES | PAL_TEXT);
			pal(1, c,PAL_BG | PAL_SPRITES | PAL_TEXT);
			pal(7, c,PAL_BG | PAL_SPRITES | PAL_TEXT);
		}

		pal(1,0,PAL_BG); //hide clouds
	}

	//-- clear screen
	u8 bg_col = 0;
	if (flash_bg) {
		bg_col = frames/5.f;
	} else if (new_bg) {
		bg_col = 2;
		pal(1,14,PAL_BG);
	}

	rectfill(0,0,16,16,-1);

	if (bg_col != 0)
		pal(0,bg_col,PAL_BG);

	//-- clouds
	cloud_scroll[0] -= 4;
	cloud_scroll[1] -= 3;
	REG_BG2HOFS = cloud_scroll[0];
	REG_BG3HOFS = cloud_scroll[1];

	//-- draw objects
	if (flag.obj.active)
		flag_draw(&flag);

	if (message.obj.active)
		message_draw(&message);

	if (lifeup.obj.active)
		lifeup_draw(&lifeup);

	if (room_title.active)
		room_title_draw(&room_title);

	if (fruit.obj.active)
		draw_object(&(fruit.obj));

	if (fly_fruit.obj.active)
		fly_fruit_draw(&fly_fruit);

	for (u8 i = 0; i < MAX_SMOKE; i++)
	{
		Object* this = &(smoke[i]);
		if (this->active)
		{
			move(this, this->spd.x, this->spd.y);
			smoke_update(this);

			if (this->active)
				draw_object(this);
		}
	}

	for (u8 i = 0; i < MAX_BALLOONS; i++)
	{
		Balloon* this = &(balloons[i]);
		if (!this->obj.active)
			break;
		else
			balloon_draw(this);
	}

	if (player_spawn.active)
		player_spawn_draw(&player_spawn);

	if (player.obj.active)
	{
		player_draw(&player);
	}
	else
	{
		//-- dead particles
		for (u8 i = 0; i < MAX_DEAD_PARTICLES; i++)
		{
			Dead_Particle* p = &(dead_particles[i]);
			if (p->active)
			{
				p->x += p->spd.x;
				p->y += p->spd.y;
				p->t -= 1;
				if (p->t <= 0)
					p->active = false;
				
				pal(7,14+p->t%2,PAL_PLAYER);
				spr(128+clamp((p->t/2),0,3),p->x-4,p->y-4,1,PAL_PLAYER,0,0);
			}
		}
	}

	if (key.active)
		draw_object(&key);

	if (chest.obj.active)
		draw_object(&(chest.obj));

	if (orb.obj.active)
	{
		move(&(orb.obj),orb.obj.spd.x,orb.obj.spd.y);
		orb_draw(&orb);
	}

	if (fake_wall.active)
		fake_wall_draw();

	for (u8 i = 0; i < MAX_SPRINGS; i++)
	{
		Object* this = &(springs[i].obj);
		if (this->active)
			draw_object(this);
	}

	for (u8 i = 0; i < MAX_FALL_FLOORS; i++)
	{
		Fall_Floor* this = &(fall_floors[i]);
		if (!this->obj.active)
			break;
		else
			fall_floor_draw(this);
	}

	//-- platforms/big chest
	for (u8 i = 0; i < MAX_PLATFORMS; i++)
	{
		Platform* this = &(platforms[i]);
		if (!this->obj.active)
			break;
		else
		{
			platform_update(this);
			platform_draw(this);
		}
	}

	if (big_chest.obj.active)
		big_chest_draw(&big_chest);

	//-- particles
	for (u8 i = 0; i < MAX_PARTICLES; i++)
	{
		Particle* p = &(particles[i]);
		p->x += p->spd;
		p->y += sin(p->off);
		p->off += min(0.05,p->spd/32);
		spr(128+p->s,p->x,p->y,1,0,0,0);
		//spr(128+p->s,p->x,p->y+64,1,0,0,0);
		if (p->x > 128+4) {
			p->x = -4;
			p->y = rndi(128);
			//p->y = rndi(64);
		}
	}
}

static void init_clouds()
{
	{
		u8 cloud_map[32*16] = {
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		};

		u16* map_adr = MAP_BASE_ADR(5);
		for (u8 y = 0; y < 16; y++)
		{
			for (u8 x = 0; x < 32; x++)
			{
				u8 val = cloud_map[x+(y*32)];
				if (val != 0)
					*map_adr = 134 + val;
				map_adr++;
			}
		}
	}

	{
		u8 cloud_map[32*16] = {
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		};

		u16* map_adr = MAP_BASE_ADR(6);
		for (u8 y = 0; y < 16; y++)
		{
			for (u8 x = 0; x < 32; x++)
			{
				u8 val = cloud_map[x+(y*32)];
				if (val != 0)
					*map_adr = 134 + val;
				map_adr++;
			}
		}
	}
}
static void init_overlay()
{
	u16* map_adr = MAP_BASE_ADR(3);

	for (u8 y = 0; y < 22; y++)
	{
		for (u8 x = 0; x < 32; x++)
		{
			if (x < 8 || x >= 24 || y < 3 || y >= 19)
				*map_adr = 137;

			map_adr++;
		}
	}
}

int main(void)
{
	// Set up the interrupt handlers
	irqInit();

	// Maxmod requires the vblank interrupt to reset sound DMA.
	// Link the VBlank interrupt to mmVBlank, and enable it. 
	irqSet(IRQ_VBLANK, mmVBlank);

	// Enable Vblank Interrupt to allow VblankIntrWait
	irqEnable(IRQ_VBLANK);

	// initialise maxmod with soundbank and 8 channels
    mmInitDefault((mm_addr)soundbank_bin, 8);
    mmSetModuleVolume(1024);
    mmSetJingleVolume(1024);

	//load graphics
	CpuFastSet(GFX_DATA, TILE_BASE_ADR(0), (GFX_SIZE/4) | COPY32);
	CpuFastSet(GFX_DATA, SPRITE_GFX, (GFX_SIZE/4) | COPY32);
	CpuFastSet(FNT_DATA, SPRITE_GFX + 0x1000, (FNT_SIZE/4) | COPY32);

	REG_BG0CNT = TILE_BASE(0) | MAP_BASE(3) | BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(0); //overlays
	REG_BG1CNT = TILE_BASE(0) | MAP_BASE(4) | BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(1); //main tile layer
	REG_BG2CNT = TILE_BASE(0) | MAP_BASE(5) | BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(2); //clouds (close)
	REG_BG3CNT = TILE_BASE(0) | MAP_BASE(6) | BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(3); //clouds, background color	

	init_clouds();
	init_overlay();

	SetMode(MODE_0 | BG_ALL_ON | OBJ_ENABLE);

	//game start
	camera(0,0);

	__init();

	bool paused = false;
	while (1)
	{
		//update
		VBlankIntrWait();
		mmFrame();

		scanKeys();
		kheld = keysHeld();
		kdown = keysDown();

		//if (btnp(KEY_SELECT))
		//	next_room();		
		
		if (btnp(KEY_START))
		{
			if (level_index() < 30)
			{
				paused = !paused;

				if (paused)
					mmPause();
				else
					mmResume();
			}
		}

		if (!paused)
		{
			if (freeze <= 0)
				update_screen();
	
			_update();
			
			//toggle screen shake
			if (btnp(KEY_SELECT))
				can_shake = !can_shake;
		}
		
		//reset key combo
		if (btn(KEY_SELECT) && btn(KEY_START) && btn(KEY_L) && btn(KEY_R))
		{
			if (!is_title())
			{
				paused = false;
				title_screen();
			}
		}

		//draw
		VBlankIntrWait();
		mmFrame();

		if (!paused)
			_draw();		
	}

	return 0;
}
