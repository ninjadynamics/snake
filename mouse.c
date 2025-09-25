#include "mouse.h"

#include <peekpoke.h>

#define LATCH_PORT 0x4016
#define DATA_PORT1 0x4016
#define DATA_PORT2 0x4017

// If the mouse is connected on the second
// controller port, use DATA_PORT2 instead
#define MOUSE_PORT DATA_PORT2
// NOTE: If you wish to autodetect the mouse port or
// make configurable, you can use a variable instead

#pragma bss-name (push, "ZEROPAGE")
Mouse mouse;
#pragma bss-name (pop)

#pragma bss-name (push, "ZEROPAGE")
static uint8_t new_x, new_y;
static int8_t x_velocity;
static int8_t y_velocity;
static uint8_t report[4];
static bool p_left;
static bool p_right;
#pragma bss-name (pop)

void __fastcall__ mouse_clear(void) {
  p_left = false;
  p_right = false;
  mouse.left.press = false;
  mouse.left.click = false;
  mouse.right.press = false;
  mouse.right.click = false;
}

void __fastcall__ mouse_init(uint8_t x, uint8_t y) {
  mouse_clear();
  mouse.x = x;
  mouse.y = y;
}

void __fastcall__ mouse_update(void) {
  // Latch the data by turning the latch on and off
  POKE(LATCH_PORT, 1);
  POKE(LATCH_PORT, 0);

  // Wait 2 CPU cycles
  __asm__("nop");

  // Read 32 bits from the mouse into the report array using unrolled loops
  // NOTE: Each read should take at least 14 CPU cycles and there should be
  //       at least 28 CPU cycles between the 2nd and 3rd bytes.

  // Get the first byte
  #define LOOP_CODE(_i) \
    __asm__("LDA %w", MOUSE_PORT); \
    __asm__("LSR A"); \
    __asm__("ROL %v+0", report); \
    __asm__("NOP"); \
    __asm__("NOP");
  LOOP(8);
  #undef LOOP_CODE

  // Get the second byte
  #define LOOP_CODE(_i) \
    __asm__("LDA %w", MOUSE_PORT); \
    __asm__("LSR A"); \
    __asm__("ROL %v+1", report); \
    __asm__("NOP"); \
    __asm__("NOP");
  LOOP(8);
  #undef LOOP_CODE

  // Wait a few more CPU cycles for
  // Hyperkin mouse compatibility
  __asm__("nop"); __asm__("nop");

  // Set mouse connection state
  if ((report[1] & 0x0F) != 0x01) {
    mouse.connected = false;
    return; // - - - - - - - - - - - - - - - - - - -
  }

  // Get the third byte
  #define LOOP_CODE(_i) \
    __asm__("LDA %w", MOUSE_PORT); \
    __asm__("LSR A"); \
    __asm__("ROL %v+2", report); \
    __asm__("NOP"); \
    __asm__("NOP");
  LOOP(8);
  #undef LOOP_CODE

  // Get the fourth byte
  #define LOOP_CODE(_i) \
    __asm__("LDA %w", MOUSE_PORT); \
    __asm__("LSR A"); \
    __asm__("ROL %v+3", report); \
    __asm__("NOP"); \
    __asm__("NOP");
  LOOP(8);
  #undef LOOP_CODE

  // Extract button states
  mouse.left.press = (report[1] >> 6) & 0x01;
  mouse.right.press = (report[1] >> 7) & 0x01;

  // Set left-click state
  mouse.left.click = (mouse.left.press && !p_left);
  p_left = mouse.left.press;

  // Set right-click state
  mouse.right.click = (mouse.right.press && !p_right);
  p_right = mouse.right.press;

  // Convert vertical displacement to two's complement
  y_velocity = report[2] & 0x7F;
  if (report[2] & 0x80) {
    y_velocity = -y_velocity;
  }

  // Convert horizontal displacement to two's complement
  x_velocity = report[3] & 0x7F;
  if (report[3] & 0x80) {
    x_velocity = -x_velocity;
  }

  // At this point, x_velocity and y_velocity contain the displacement values
  // mouse.left and mouse.right contain the button states (1 = pressed, 0 = not pressed)

  // Update the cursor X position
  if (x_velocity) {
    new_x = mouse.x + x_velocity;
    if (x_velocity > 0) {
      mouse.x = new_x < MAX_X && new_x > mouse.x ? new_x : MAX_X;
    }
    else {
      mouse.x = new_x > MIN_X && new_x < mouse.x ? new_x : MIN_X;
    }
  }

  // Update the cursor Y position
  if (y_velocity) {
    new_y = mouse.y + y_velocity;
    if (y_velocity > 0) {
      mouse.y = new_y < MAX_Y && new_y > mouse.y ? new_y : MAX_Y;
    }
    else {
      mouse.y = new_y > MIN_Y && new_y < mouse.y ? new_y : MIN_Y;
    }
  }

  // Set mouse connection state
  mouse.connected = true;
}