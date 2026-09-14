#ifndef PW_FEELING_H
#define PW_FEELING_H

#include "types.h"

#define SOCIAL_SILENT 16
#define SOCIAL_NO_BUBBLE 7
#define SOCIAL_TERMINAL 0x01
#define SOCIAL_SHOW_POKEMON 0x02
#define SOCIAL_SHOW_TREASURE 0x04
#define SOCIAL_POKEMON_NAME 0xFC
#define SOCIAL_ITEM_NAME 0xFD
#define SOCIAL_WATTS 0xFE
#define SOCIAL_NO_MESSAGE 0xFF
#define SOCIAL_FLAGS(bubble, panel, icons)                                     \
  (((bubble) << 5) | ((panel) << 3) | (icons))

typedef union {
  u8 byte;
  struct {
    u8 terminal : 1;
      u8 showPokemon : 1;
      u8 showTreasure : 1;
      u8 panelMode : 2;
      u8 bubble : 3;
} bits;
} SocialFlags;

typedef struct {
  SocialFlags flags;
  u8 beeper;
  u8 message;
  u8 panel;
} SocialFrame;

void SocialApply(u8 eventType);

#endif
