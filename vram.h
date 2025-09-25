#ifndef _VRAM_H
#define _VRAM_H

#include "core.h"

#define _VRB_TYPE_SINGLE 2
#define _VRB_TYPE_ARRAY  3
#define _VRB_TYPE_END    1

// Put address into VRAM buffer
#define PUT_ADDRESS_VRB(address, vrb_index) ( \
  *((word*)(&(vram_buffer[(vrb_index)]))) = SWAP_BYTES((address)) \
)

// VRAM Address
#define NTADR(A, X, Y) ( \
  A | (((Y) << 5) | (X)) \
)

#define VRAM_BUFFER_SIZE 128
#define VRAM_BUFFER_ADDR 0x300
typedef byte _vram_buffer_t[VRAM_BUFFER_SIZE];
#define vram_buffer (*(_vram_buffer_t*)VRAM_BUFFER_ADDR)

#endif // vram.h