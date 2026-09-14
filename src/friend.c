#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "diary.h"
#include "display.h"
#include "beep.h"
#include "eeprom.h"
#include "friend.h"
#include "home.h"
#include "common.h"
#include "scratch.h"

extern const u8 g_peerPlayNoteShiftTable6[6];

/* Peer-play view of the shared UI state. */

void ResolveGift(void);
void ShiftPeerRecords(void);
void AppendPeerDiary(void);

#define PW_PEER_RECORD_EEPROM 0xf6c0
#define PW_PEER_RECORD_BYTES 0x38
#define PW_PEER_ITEM_EEPROM 0xCEC8
#define PW_PEER_ITEM_BYTES 0x28
#define PW_PEER_HISTORY_BASE 0xD000
#define PW_PEER_HISTORY_PROFILE_OFF 0x10
#define PW_PEER_HISTORY_KEY_LEN 0x28
#define PW_PEER_HISTORY_RECORD_SIZE 0x224
#define PW_PEER_HISTORY_RECORD_COUNT 10
#define PW_PEER_SHIFT_SRC 0xef44
#define PW_PEER_SHIFT_DST 0xf168
#define PW_PEER_SHIFT_BYTES 0x224
#define PW_DIARY_ENTRY_BYTES 0x88
#define PW_NOTE_RASTER_EEPROM 0x2470
#define PW_NOTE_RASTER_BYTES 0x10
#define PW_DISPLAY_MAIN_RASTER_EXTENT 0x1018
#define PW_DIARY_ACTION_PEER_SLOT_1 17
#define PEER_DIARY_NAME_COPY_BYTES 18

typedef union {
  PeerInfo record;
  struct {
    u8 prefix[offsetof(PeerInfo, trainerName)];
    u8 bytes[PEER_DIARY_NAME_COPY_BYTES];
  } nameSpan;
} PeerNameView;

typedef union {
  DiaryEntry record;
  struct {
    u8 prefix[offsetof(DiaryEntry, trainerName)];
    u8 bytes[PEER_DIARY_NAME_COPY_BYTES];
  } nameSpan;
} DiaryNameView;

typedef char PeerNameSpanFits[offsetof(PeerInfo, trainerName) +
                                          PEER_DIARY_NAME_COPY_BYTES <=
                                      sizeof(PeerInfo)
                                  ? 1
                                  : -1];
typedef char DiaryNameSpanFits[offsetof(DiaryEntry, trainerName) +
                                           PEER_DIARY_NAME_COPY_BYTES <=
                                       sizeof(DiaryEntry)
                                   ? 1
                                   : -1];

void MusicNote(u8 x, u8 y, u8 rightShift);

/* Apply the peer's sprite orientation, resolve the gift, and update the peer
 * history and diary. */
void PeerFinalize(void)
{
  PeerInfo *peer;

  ScratchReset();
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead((EEPROM_PEER_IMAGE + 384 + 320), peer, sizeof(PeerInfo));
  g_ui.view.peer.session.bits.fixedFacing = peer->fixedFacing;
  g_ui.view.peer.phase = 0;
  g_ui.view.peer.substate = 0;
  ResolveGift();
  ShiftPeerRecords();
  AppendPeerDiary();
}

void PeerUpdate(void)
{
}

/* Combine both walkers' day and hour steps and cap at 20,000. Award 1-99 Watts
 * when the gift inventory is full; otherwise store a course item in its first
 * empty slot. */
void ResolveGift(void)
{
  PeerInfo *peer;
  Item *items;
  u32 combined;
  u8 slot;

  ScratchReset();
  ScratchAlloc(sizeof(Course));
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead((EEPROM_PEER_IMAGE + 384 + 320), peer, sizeof(PeerInfo));
  items = ScratchAlloc(sizeof(Item) * 10);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
             items, sizeof(Item) * 10);
  combined = g_state.dailySteps + peer->dailySteps +
             ((g_state.hourSteps + peer->hourSteps) * 10);
  if (combined > 20000ul) {
    combined = 20000ul;
  }
  slot = 0;
  do {
    if (items[slot].id == 0) {
      break;
    }
    slot++;
  } while (slot < 10);
  g_ui.view.peer.watts = 0;
  if (slot >= 10) {
    if ((g_ui.view.peer.watts = (combined / 200ul)) == 0) {
      g_ui.view.peer.watts = 1;
    }
    if (g_ui.view.peer.watts > 99) {
      g_ui.view.peer.watts = 99;
    }
    WattsAdd(g_ui.view.peer.watts);
  }
  if (combined >= 20000ul) {
    g_ui.view.peer.giftTier = 0x2c;
    if (g_ui.view.peer.watts != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.rewardIndex = 0;
    } else {
      g_ui.view.peer.rewardIndex = 1;
    }
    items[slot].id = CourseItemNumber(g_ui.view.peer.rewardIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (combined >= 10000ul) {
    g_ui.view.peer.giftTier = 0x2d;
    if (g_ui.view.peer.watts != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.rewardIndex = 2;
    } else {
      g_ui.view.peer.rewardIndex = 3;
    }
    items[slot].id = CourseItemNumber(g_ui.view.peer.rewardIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (combined >= 5000ul) {
    g_ui.view.peer.giftTier = 0x2e;
    if (g_ui.view.peer.watts != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.rewardIndex = 4;
    } else {
      g_ui.view.peer.rewardIndex = 5;
    }
    items[slot].id = CourseItemNumber(g_ui.view.peer.rewardIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (combined >= 2500ul) {
    g_ui.view.peer.giftTier = 0x2f;
    if (g_ui.view.peer.watts != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.rewardIndex = 6;
    } else {
      g_ui.view.peer.rewardIndex = 7;
    }
    items[slot].id = CourseItemNumber(g_ui.view.peer.rewardIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else {
    g_ui.view.peer.giftTier = 0x30;
    if (g_ui.view.peer.watts != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.rewardIndex = 8;
    } else {
      g_ui.view.peer.rewardIndex = 9;
    }
    items[slot].id = CourseItemNumber(g_ui.view.peer.rewardIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  }
}

/* Load image->note, shift each byte right, and blit the 8x8 glyph. */
void MusicNote(u8 x, u8 y, u8 rightShift)
{
  u8 *raster;
  u8 *source;
  u8 i;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->note));
  source = ((UiResources *)EEPROM_UI)->note;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->note));
  i = 0;
  do {
    raster[i] >>= rightShift;
    i++;
  } while (i < sizeof(((UiResources *)0)->note));
  DisplayBlit(x, y, 8, 8, raster);
}

/* Draw both Pokemon facing each other, then animate the gift and reward.
 * Advance the 8-tick substate and return home after phase 4. */
void PeerRender(void)
{
  u8 fixedFacing;
  u8 x;
  u8 count;
  u8 half;
  u8 i;

  if (g_ui.view.peer.phase < 3) {
    RenderHeldPokemon(0x38, 0x08);
    fixedFacing = g_ui.view.peer.session.bits.fixedFacing;
    if (g_ui.view.peer.phase == 0) {
      x = (8 - (7 - g_ui.view.peer.substate) * 3);
      if (fixedFacing != 0) {
        RenderPeerPokemon(x, 8, 0);
      } else {
        RenderPeerPokemon(x, 8, 1);
      }
    } else if (fixedFacing != 0) {
      RenderPeerPokemon(8, 8, 0);
    } else {
      RenderPeerPokemon(8, 8, 1);
    }
  }

  switch (g_ui.view.peer.phase) {
  case 1:
    RenderPeerName(0x02, 0x20, 1);
    RenderMessage(0x30, 0x2b, 0x0e, 0);
    break;
  case 2:
    count = (g_ui.view.peer.substate + 1);
    switch (g_ui.view.peer.giftTier) {
    case 0x2c:
      half = count;
      if (half > 5) {
        half = 5;
      }
      for (i = 0; i < half; i++) {
        MusicNote((i * 8 + 0x1c), 0, g_peerPlayNoteShiftTable6[i]);
      }
      break;
    case 0x2d:
      half = count;
      if (half > 4) {
        half = 4;
      }
      for (i = 0; i < half; i++) {
        MusicNote((i * 8 + 0x1c), 0, g_peerPlayNoteShiftTable6[i]);
      }
      break;
    case 0x2e:
      half = ((g_ui.view.peer.substate >> 1) + 1);
      if (half > 3) {
        half = 3;
      }
      for (i = 0; i < half; i++) {
        MusicNote((i * 8 + 0x24), 0, g_peerPlayNoteShiftTable6[i + 1]);
      }
      break;
    case 0x2f:
      half = ((g_ui.view.peer.substate >> 1) + 1);
      if (half > 2) {
        half = 2;
      }
      for (i = 0; i < half; i++) {
        MusicNote((i * 8 + 0x24), 0, g_peerPlayNoteShiftTable6[i + 1]);
      }
      break;
    case 0x30:
      MusicNote(0x2c, 0, g_peerPlayNoteShiftTable6[2]);
      break;
    }
    RenderMessage(0x30, g_ui.view.peer.giftTier, 0x0f, 0);
    break;
  case 3:
    RenderItemCheckPresent(0x20, 0x04);
    RenderMessage(0x30, 0x31, 0x0f, 0);
    break;
  case 4:
    RenderItemCheckPresent(0x20, 0x04);
    if (g_ui.view.peer.watts != 0) {
      RenderDecoratedNumber(0x02, 0x20, g_ui.view.peer.watts, 0x0d);
    } else {
      RenderCourseItem(0x00, 0x20, g_ui.view.peer.rewardIndex, 0x0d);
    }
    RenderMessage(0x30, 0x0f, 0x0e, 0);
    break;
  }

  if (++g_ui.view.peer.substate >= 8) {
    g_ui.view.peer.phase++;
    g_ui.view.peer.substate = 0;
    switch (g_ui.view.peer.phase) {
    case 2:
      BeepLoadScore(9);
      break;
    case 4:
      BeepLoadScore(6);
      break;
    }
  }
  if (g_ui.view.peer.phase >= 5) {
    HomeInit();
    SetView(VIEW_HOME);
  }
  RenderBattery(0, 0);
}

/* Scan the ten peer PeerRecords slots after the self record and return 1 if any
 * stored unique id matches the caller's 40-byte id. */
u8 SeenPeer(u8 *uniqueId)
{
  PeerRecords *record;
  u8 *view;
  u8 slot;
  u8 i;
  u8 match;

  record = (PeerRecords *)EEPROM_PEER_RECORDS + 1;
  view = g_work.irc.eepromScratch;
  slot = 0;
  while (slot < 10) {
    match = 1;
    EepromRead((u16)&record->deviceId, view, sizeof(record->deviceId));
    i = 0;
    do {
      if (view[i] != uniqueId[i]) {
        match = 0;
      }
      i++;
    } while (i < sizeof(record->deviceId));
    if (match == 1) {
      return 1;
    }
    slot++;
    record++;
  }
  return 0;
}

void ShiftPeerRecords(void)
{
  u8 *buffer;
  u16 src;
  u16 dst;
  u8 n;

  ScratchReset();
  buffer = ScratchAlloc(PW_PEER_SHIFT_BYTES);
  src = PW_PEER_SHIFT_SRC;
  dst = PW_PEER_SHIFT_DST;
  n = 10;
  do {
    EepromRead(src, buffer, PW_PEER_SHIFT_BYTES);
    EepromWrite(dst, buffer, PW_PEER_SHIFT_BYTES);
    src = (src - PW_PEER_SHIFT_BYTES);
    dst = (dst - PW_PEER_SHIFT_BYTES);
  } while (--n != 0);
}

/* Fill a DiaryEntry from the received PeerInfo, then append it with the gifted
 * course item when watts were not awarded instead. */
void AppendPeerDiary(void)
{
  Course *course;
  PeerInfo *peer;
  DiaryEntry *diary;
  u8 i;

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead((EEPROM_PEER_IMAGE + 384 + 320), peer, sizeof(PeerInfo));
  diary = ScratchAlloc(sizeof(DiaryEntry));
  diary->compatibilityLe = peer->compatibilityLe;
  diary->gameVersionLe = peer->gameVersionLe;
  diary->encounterId = peer->id;
  diary->peerForm = peer->form;
  diary->peerSex = peer->sex;
  diary->peerShiny = peer->shiny;
  diary->peerHourSteps = peer->hourSteps;
  diary->peerDaySteps = peer->dailySteps;
  i = 0;
  do {
    diary->peerNickname[i] = peer->nickname[i];
    i++;
  } while (i < sizeof(peer->nickname));
  i = 0;
  /* The copy includes the two appearance bytes following the peer name.
   * DiaryAppend replaces these extra bytes with the local Pokemon's name. */
  do {
    ((DiaryNameView *)diary)->nameSpan.bytes[i] =
        ((const PeerNameView *)peer)->nameSpan.bytes[i];
    i++;
  } while (i < PEER_DIARY_NAME_COPY_BYTES);
  if (g_ui.view.peer.watts == 0) {
    if (g_ui.view.peer.rewardIndex < 10) {
      DiaryAppend(course, diary, (g_ui.view.peer.rewardIndex + 1),
                  g_state.save.bonusCourse, 0,
                  course->itemId[g_ui.view.peer.rewardIndex]);
    }
  }
}

/* note shift per gift tier */
const u8 g_peerPlayNoteShiftTable6[6] = {0x00, 0x01, 0x02, 0x01, 0x00, 0x00};
