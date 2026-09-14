#ifndef PW_STATE_H
#define PW_STATE_H

#include "data.h"
#include "flags.h"
#include "feeling.h"

#define VIEW_HOME 0
#define VIEW_MAIN_MENU 1
#define VIEW_DOWSING 2
#define VIEW_RADAR 3
#define VIEW_BATTLE 4
#define VIEW_RADAR_FAILURE 6
#define VIEW_DISCARD 7
#define VIEW_TRAINER 8
#define VIEW_SETTINGS 9
#define VIEW_POKEMON_LIST 10
#define VIEW_ITEM_LIST 11
#define VIEW_SOCIAL 12
#define VIEW_PEER 13
#define VIEW_IR_RESULT 14
#define VIEW_WALK_START 15
#define VIEW_WALK_END 16
#define VIEW_EVENT_REWARD 17
#define VIEW_DIAGNOSTICS 22
#define VIEW_THRESHOLD_TEST 23
#define VIEW_THRESHOLD_FAILURE 24

/* The active view selects the interpretation of this shared 18-byte UI bank.
 * Word fields require two-byte alignment. */

/* Dowsing view of the shared UI state. */
typedef struct {
  u8 rewardIndex;
  u8 substate;
  u8 animationFrame;
  u8 cursor;
  u8 attempts;
  u8 hiddenSlot;
  u8 revealedSlot;
  u8 watts;
  u8 emptyItemSlot;
  u8 unused9;
  u16 itemNumber;
  u8 unusedTail[6];
} DowsingView;

typedef struct {
  u8 sourceIndex; /* One-based for Pokémon; zero-based for items. */
  u8 selectedSlot;
  u8 inventoryKind;
  u8 unusedTail[15];
} DiscardView;

/* Device-diagnostics state, including stable RTC samples and the probe result.
 */
typedef struct {
  u8 stage;
  u8 introReady;
  u8 holdSeconds;
  u8 rtcFirstSample;
  u8 rtcSecondSample;
  u8 probeResult;
  u16 mirrorWord;
  u8 unusedTail[10];
} DiagnosticsView;

typedef struct {
  u16 resetWord;
  u8 walkingSamples;
  u8 stillSamples;
  u16 xActivity;
  u16 yActivity;
  u16 zActivity;
  MotionThresholds countParameters;
} ThresholdView;

/* Inventory presence masks are 16-bit words. */
typedef struct {
  u8 cursor;
  u8 unused1;
  u16 pokemonPresent;
  u16 itemPresent;
  u8 unusedTail[12];
} InventoryView;

typedef struct {
  IrRoleFlags role;
  u8 selfTestFailed;
  u8 unusedTail[16];
} IrView;

typedef struct {
  u8 pendingEventId;
  u8 eventOfferCountdown;
  u8 animatedRasterOriginLow;
  HomeMotionFlags animationControl;
  u8 unusedTail[14];
} HomeView;

typedef struct {
  u8 encounter;
  u8 substate;
  u8 cursor;
  u8 stage;
  u8 reverseFrame;
  u8 target;
  u8 delay;
  u8 remaining;
  u8 revealFrame;
  u8 unusedTail[9];
} RadarView;

typedef union {
  u8 byte;
  struct {
    u8 hpVisible : 1;
      u8 action : 2;
      u8 outcome : 2;
      u8 stage : 3;
} bits;
} BattleFlags;

typedef struct {
  u8 encounter; /* route slot 1..3, 4+ = bonus course */
  u8 substate;
  u8 playerHp;
  u8 opponentHp;
  u8 frame;
  u8 limit;
  u8 playerX;
  u8 opponentX;
  u16 wattLoss;
  BattleFlags flags;
  u8 captureCount;
} BattleView;

typedef struct {
  PeerFlags session;
  u8 phase;
  u8 substate;
  u8 giftTier;
  u8 rewardIndex;
  u8 watts;
  u8 unusedTail[12];
} PeerView;

typedef struct {
  u8 subpage;
  u8 page;
  u8 unusedTail[16];
} TrainerView;

typedef struct {
  u8 page;
  u8 cursor;
  u8 unusedTail[16];
} SettingsView;

typedef struct {
  u8 eventType;
  u8 rewardValue;
  u8 messageVariant;
  u8 unused3;
  const SocialFrame *frame;
} SocialView;

typedef struct {
  u8 stage;
  u8 frame;
  u8 rewardKind;
  u8 unusedTail[15];
} PresentationView;

typedef struct {
  u8 error;
  u8 unusedTail[17];
} MenuView;

typedef union {
  u8 raw[18];
  PresentationView presentation;
  MenuView menu;
  DiscardView discard;
  DowsingView dowsing;
  InventoryView inventory;
  u16 words[9];
  HomeView home;
  DiagnosticsView diag;
  ThresholdView accel;
  IrView ir;
  RadarView radar;
  BattleView battle;
  PeerView peer;
  TrainerView trainer;
  volatile SettingsView settings;
  SocialView social;
} ViewState;

#define PW_DEFAULT_UI_BOUNDARY_LATCH_MASK 1
#define PW_DEFAULT_UI_RENDER_PHASE_MASK 2
#define PW_DEFAULT_UI_ORIGIN_DIRECTION_MASK 4

/* An eight-byte bulk-transfer record with little-endian word fields stored as
 * byte pairs; no alignment padding is permitted. */
typedef struct {
  u8 mode;
  u8 bytesRemainingLo;
  u8 bytesRemainingHi;
  u8 sourceEepromAddressLo;
  u8 sourceEepromAddressHi;
  u8 destinationEepromAddressLo;
  u8 destinationEepromAddressHi;
  u8 chunksCompleted;
} IrBulkTransferState;

#define PW_DISPLAY_BAND_6_ORIGIN_Y 0x30

/* Packed span and index. The target word is big-endian: span is the high byte.
 */
typedef union {
  u16 word;
  struct {
    u8 span;
    u8 index;
  } b;
} SpanWalk;

#endif
