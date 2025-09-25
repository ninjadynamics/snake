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

  // Enable rendering
  ppu_on_all();

  // The main loop
  while (1) {
    // Clear sprites
    oam_clear();

    // Wait for next frame
    ppu_wait_nmi();
  }
}
