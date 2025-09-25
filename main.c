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
#define APPLE 0xFF

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
  MSB(NTADR_A(INITIAL_X, INITIAL_Y)), LSB(NTADR_A(INITIAL_X, INITIAL_Y)), 0x7F,
  MSB(NTADR_A(INITIAL_X, INITIAL_Y)), LSB(NTADR_A(INITIAL_X, INITIAL_Y)), 0x7F,
  NT_UPD_EOF
};

enum Direction {
  None, Up, Down, Left, Right
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
} Snake;

typedef struct Gamepad {
  byte poll;
  byte trigger;
} Gamepad;

static const int8_t dir_x[5] = { 0,  0,  0, -1,  1};
static const int8_t dir_y[5] = { 0, -1,  1,  0,  0};

byte world[30][32];

#pragma bss-name (push, "ZEROPAGE")
Vec2 apple;
Gamepad pad;
Snake snake;
uint8_t spd;
uint8_t frame;
uint8_t distance;
uint8_t dir;
uint8_t debug;
uint8_t tx, ty;
uint8_t apple_count;
#pragma bss-name (pop)

void create_new_apple(void) {
  do { // Make sure it's an empty cell
    tx = 1 + rand8() % 30;
    ty = 1 + rand8() % 28;  
  } while (world[ty][tx]);
  world[ty][tx] = APPLE;
  apple.x = (tx * 8) - 0;
  apple.y = (ty * 8) - 1;  
}

void main(void) {

  const unsigned char pal[16]={
    0x0f,0x00,0x10,0x30,
    0x0f,0x0c,0x21,0x32,
    0x0f,0x05,0x16,0x27,
    0x0f,0x1b,0x2a,0x39
  };

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
  
  start:
  
  // VRAM buffer
  memcpy(vram_buffer, new_vram_buffer, sizeof(new_vram_buffer));
  set_vram_update(vram_buffer);

  // Enable rendering
  ppu_on_all();
  
  // Init game
  spd = 8;
  frame = 0;
  distance = 1;
  
  snake.size = 5;
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
    
    // Clear sprites
    oam_clear();
    
    oam_spr(apple.x, apple.y, 0xB0, 3);
    
    // Read gamepad
    pad.trigger = pad_trigger(0);
    pad.poll = pad_poll(0);    
    
    if (++frame == spd) {
    
      snake.head.x += dir_x[snake.direction];
      if (snake.head.x > 31) snake.head.x = 0;
      else if (snake.head.x < 0) snake.head.x = 31;

      snake.head.y += dir_y[snake.direction];
      if (snake.head.y > 29) snake.head.y = 0;
      else if (snake.head.y < 0) snake.head.y = 29;
      
      if (world[snake.head.y][snake.head.x] == APPLE) {
        create_new_apple();
        ++snake.size;
        if (++apple_count % 4 == 0 && --spd == 0) spd = 1;
      }
      else if (world[snake.head.y][snake.head.x]) {
        pal_col(0, 0x06);
        ppu_wait_nmi();
        while(pad_trigger(0) != PAD_START);
        ppu_off();
        vram_adr(NTADR_A(0, 0));
        vram_fill(0x00, 1024);
        pal_col(0, 0x0f);
        goto start;
      }
      
      world[snake.head.y][snake.head.x] = snake.direction;
      
      // Draw head
      vram_buffer[0] = MSB(NTADR_A(snake.head.x, snake.head.y));
      vram_buffer[1] = LSB(NTADR_A(snake.head.x, snake.head.y));
      vram_buffer[2] = 0x7f;      
      
      if (++distance > snake.size) {       
        // Clear tail
        vram_buffer[3] = MSB(NTADR_A(snake.tail.x, snake.tail.y));
        vram_buffer[4] = LSB(NTADR_A(snake.tail.x, snake.tail.y));
        vram_buffer[5] = 0x00;
        
        dir = world[snake.tail.y][snake.tail.x];
        world[snake.tail.y][snake.tail.x] = None;
        
        snake.tail.x += dir_x[dir];
        if (snake.tail.x > 31) snake.tail.x = 0;
        else if (snake.tail.x < 0) snake.tail.x = 31;

        snake.tail.y += dir_y[dir];
        if (snake.tail.y > 29) snake.tail.y = 0;
        else if (snake.tail.y < 0) snake.tail.y = 29;
                
        distance = snake.size;
      }
      
      // Reset frame counter
      frame = 0;  
    }
    
    switch (pad.poll) {
      case PAD_UP:
        if (snake.direction != Down) {
          snake.direction = Up;
          world[snake.head.y][snake.head.x] = Up;
        }
        break;

      case PAD_DOWN:
        if (snake.direction != Up) {
          snake.direction = Down;
          world[snake.head.y][snake.head.x] = Down;
        }
        break;
        
      case PAD_LEFT:
        if (snake.direction != Right) {
          snake.direction = Left;
          world[snake.head.y][snake.head.x] = Left;
        }
        break;
        
      case PAD_RIGHT:
        if (snake.direction != Left) {
          snake.direction = Right;
          world[snake.head.y][snake.head.x] = Right;
        }
        break;
    }

    // Wait for next frame
    ppu_wait_nmi();
  }
}
