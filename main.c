/* - - - - - - - - - - - - - - - - - - */
/* Necessary for 8bitworkshop          */
/* - - - - - - - - - - - - - - - - - - */
//
#define LIBARGS nes.lib
#define CFGFILE mmc3.cfg
//
//#resource "mmc3.cfg"
//#resource "neslib.s"
//#resource "famitone2.s"
//#resource "chr_default.s"
//
//#resource "music.s"
//#resource "sounds.s"
//#resource "music_themoon.s"
//#resource "music_journey.s"
//#resource "music_shatterhand.s"
//#resource "music_farewell.s"
//#resource "music_dedricil.s"
//#resource "music_rain.s"
//#resource "sfx.s"
//
//#link "mouse.c"
//#link "mmc3.c"
//#link "crt0.s"
//
/* - - - - - - - - - - - - - - - - - - */

#include "core.h"
#include "mmc3.h"
#include "neslib.h"
#include "mouse.h"
#include "vram.h"

#include <stdint.h>

#define INITIAL_X 16
#define INITIAL_Y 14

// Sprite layer
#define APPLE 0x84
#define LEAF  0x85
#define GRASS 0x95
#define HOLE  0x86

// Background layer
#define BODY  0x29

#define GRASS_DENSITY 32

enum Direction {
  None, Up, Right, Down, Left
};

typedef struct Vec2 {
  int8_t x;
  int8_t y;
} Vec2;

typedef struct Snake {
  Vec2 head;
  Vec2 tail;
  uint8_t size;
  uint8_t direction;
  uint8_t frame;
} Snake;

typedef struct Gamepad {
  byte poll;
  byte trigger;
} Gamepad;

const byte new_vram_buffer[] = {
  // Update Buffer Structure:
  // - - - - - - - - - - - - - - - - -
  // [  0] MSB
  // [  1] LSB
  // [  2] Number of tiles
  // [3:x] Tile data (read from map)
  // (...)
  // [ -1] End of Buffer (NT_UPD_EOF)
  // - - - - - - - - - - - - - - - - -
  MSB(NTADR_A(INITIAL_X, INITIAL_Y)), LSB(NTADR_A(INITIAL_X, INITIAL_Y)), BODY + Right,
  MSB(NTADR_A(INITIAL_X, INITIAL_Y)), LSB(NTADR_A(INITIAL_X, INITIAL_Y)), BODY + Right,
  MSB(NTADR_A(INITIAL_X, INITIAL_Y)), LSB(NTADR_A(INITIAL_X, INITIAL_Y)), BODY + Right,
  NT_UPD_EOF
};

static const int8_t dir_x[5] = { 0,  0,  1,  0, -1};
static const int8_t dir_y[5] = { 0, -1,  0,  1,  0};

#pragma bss-name (push, "HIMEM")
byte world[30][32];
#pragma bss-name (pop)

#pragma bss-name (push, "ZEROPAGE")
Vec2 apple;
Gamepad pad;
Snake snake;
uint8_t spd;
uint8_t frame;
uint8_t distance;
uint8_t dir;
uint8_t debug, i;
uint8_t tx, ty, f;
uint8_t apple_count;
uint8_t step_px;
uint16_t step_fp;
bool draw_hole;
#pragma bss-name (pop)

Vec2 grass[GRASS_DENSITY];

const unsigned char pal[16]={
  0x19, 0x0f, 0x21, 0x30,
  0x19, 0x0f, 0x26, 0x30,
  0x19, 0x0f, 0x2A, 0x30,
  0x19, 0x18, 0x29, 0x30
};

const unsigned char spr_head[][] = {
  {
    - 4, - 4, 0x80, 0,
    + 4, - 4, 0x81, 0,
    - 4, + 4, 0x90, 0,
    + 4, + 4, 0x91, 0,
    0x80
  },
  {
    - 4, - 4, 0x82, 0,
    + 4, - 4, 0x83, 0,
    - 4, + 4, 0x92, 0,
    + 4, + 4, 0x93, 0,
    0x80
  },
};

void create_new_apple(void) {
  do { // Make sure it's an empty cell
    tx = 2 + rand8() % 28;
    ty = 2 + rand8() % 26;
  } while (world[ty][tx]);
  world[ty][tx] = APPLE;
  apple.x = (tx * 8) - 0;
  apple.y = (ty * 8) - 1;  
}

void main(void) {

  // Set the palettes
  pal_bg(pal);
  pal_spr(pal);

  // Clear sprites
  oam_clear();

  // Set default tilesets
  chr_bg   = CHR_DEFAULT;
  chr_spr  = CHR_DEFAULT;

  // Load tilesets
  mmc3_setup();

  // Init bank swapping
  active_data_bank_index = 0;
  active_code_bank_index = 0;
  active_data_bank[0] = 0;
  active_code_bank[0] = 0;

  // Initialize sound
  FAMITONE_MUSIC_INIT(NULL);
  FAMITONE_SFX_INIT(NULL);
  
  // NMI Setup
  nmi_set_callback(mmc3_famitone_update_nmi);
  
  start:
  
  draw_hole = true;
  
  i = GRASS_DENSITY; while (i--) {
    grass[i].x = (2 + rand8() % 28) * 8; rand8();
    grass[i].y = (2 + rand8() % 26) * 8; rand8();
  }
  
  // VRAM buffer
  memcpy(vram_buffer, new_vram_buffer, sizeof(new_vram_buffer));
  set_vram_update(vram_buffer);

  // Enable rendering
  ppu_on_all();
  
  // Init game
  spd = 8;
  frame = 0;
  distance = 1;
  step_fp = (8u << 8) / spd;
  
  snake.size = 5;
  snake.frame = 0;
  snake.head.x = INITIAL_X;
  snake.head.y = INITIAL_Y;
  snake.tail.x = INITIAL_X;
  snake.tail.y = INITIAL_Y;
  snake.direction = Right;
  
  memfill(&world, 0x00, 32 * 30);
  world[INITIAL_Y][INITIAL_X] = snake.direction;
  
  create_new_apple();  

  // Main loop
  while (true) {
       
    // Read gamepad
    pad.trigger = pad_trigger(0);
    pad.poll = pad_poll(0);    
    
    if (++snake.frame == spd) {
    
      snake.head.x += dir_x[snake.direction];
      if (snake.head.x > 31) snake.head.x = 0;
      else if (snake.head.x < 0) snake.head.x = 31;

      snake.head.y += dir_y[snake.direction];
      if (snake.head.y > 29) snake.head.y = 0;
      else if (snake.head.y < 0) snake.head.y = 29;
      
      // When catching an apple
      if (world[snake.head.y][snake.head.x] == APPLE) {
        create_new_apple();
        ++snake.size; // Grow snake
        // Increase speed every 4 apples
        if (++apple_count % 4 == 0 && --spd == 0) spd = 1;
        // Step calculation for smooth visuals
        step_fp = (8u << 8) / spd;
      }
      // When bumping against itself --> GAME OVER
      else if (world[snake.head.y][snake.head.x]) {
        pal_col(0, 0x06); // Red screen of death
        while(pad_trigger(0) != PAD_START) ppu_wait_nmi();
        // - - - - - - - - - - - - - - - - - - - - - - - -
        ppu_off();
        vram_adr(NTADR_A(0, 0));
        vram_fill(0x00, 1024);
        pal_col(0, 0x19);
        goto start;
      }
      
      world[snake.head.y][snake.head.x] = snake.direction;
      
      // Draw head
      vram_buffer[0] = MSB(NTADR_A(snake.head.x, snake.head.y));
      vram_buffer[1] = LSB(NTADR_A(snake.head.x, snake.head.y));
      vram_buffer[2] = BODY + snake.direction;
           
      if (++distance > snake.size) {       
        // Clear tail
        vram_buffer[3] = MSB(NTADR_A(snake.tail.x, snake.tail.y));
        vram_buffer[4] = LSB(NTADR_A(snake.tail.x, snake.tail.y));
        vram_buffer[5] = NULL;
        
        dir = world[snake.tail.y][snake.tail.x];
        world[snake.tail.y][snake.tail.x] = None;
        
        snake.tail.x += dir_x[dir];
        if (snake.tail.x > 31) snake.tail.x = 0;
        else if (snake.tail.x < 0) snake.tail.x = 31;

        snake.tail.y += dir_y[dir];
        if (snake.tail.y > 29) snake.tail.y = 0;
        else if (snake.tail.y < 0) snake.tail.y = 29;   
                
        distance = snake.size;
        draw_hole = false;
      }
      
      // Clear tail
      dir = world[snake.tail.y][snake.tail.x];
      vram_buffer[6] = MSB(NTADR_A(snake.tail.x, snake.tail.y));
      vram_buffer[7] = LSB(NTADR_A(snake.tail.x, snake.tail.y));
      vram_buffer[8] = 0x10 + BODY + dir;
      
      // Reset frame counter
      snake.frame = 0;  
    }
    
    // Change direction using the DPAD
    dir = snake.direction;
    switch (pad.poll) {
      case PAD_UP:
        if (snake.direction != Down) {
          snake.direction = Up;
        }
        break;

      case PAD_DOWN:
        if (snake.direction != Up) {
          snake.direction = Down;
        }
        break;
        
      case PAD_LEFT:
        if (snake.direction != Right) {
          snake.direction = Left;
        }
        break;
        
      case PAD_RIGHT:
        if (snake.direction != Left) {
          snake.direction = Right;
        }
        break;
    }
    
    // Turn left or right using A and B
    switch (pad.trigger) {
      case PAD_A:
        if (++snake.direction > 4)
          snake.direction = 1;
        break;
        
      case PAD_B:
        if (--snake.direction < 1)
          snake.direction = 4;        
        break;
    }
    
    // Mask corners
    if (snake.direction != dir) {
      vram_buffer[2] = 0x1E;
    }
    
    // Update world
    world[snake.head.y][snake.head.x] = snake.direction;
        
    // Head animation frame
    f = (frame / 8) % 2;
    
    // Movement interpolation
    step_px = (snake.frame * step_fp) >> 8;
    
    // Update background animation
    chr_bg = (CHR_DEFAULT + 4) + step_px;    
    
    // Calculate head position
    tx = (snake.head.x * 8) - 0;
    ty = (snake.head.y * 8) - 2;       
    tx += dir_x[snake.direction] * step_px;
    ty += dir_y[snake.direction] * step_px;
    
    // Clear sprites
    oam_clear();

    // Draw head
    oam_meta_spr(tx, ty, spr_head[f]);
        
    // Draw apple
    oam_spr(apple.x + 1, apple.y - 5, LEAF, 2);
    oam_spr(apple.x, apple.y, APPLE, 1);
    
    // Draw hole
    if (draw_hole) oam_spr(
      INITIAL_X * 8 - 0, 
      INITIAL_Y * 8 - 1, 
      HOLE, 0 | OAM_BEHIND
    );
    
    // Draw grass
    i = GRASS_DENSITY; while (i--) {
      oam_spr(grass[i].x, grass[i].y, GRASS, 3 | OAM_BEHIND);
    }
        
    // Wait for next frame
    ppu_wait_nmi();
    ++frame;
    
    
  }
}
