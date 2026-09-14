#ifndef PW_RUNTIME_STATE_H
#define PW_RUNTIME_STATE_H

#include "flags.h"

#include "data.h"
#include "state.h"

#include <stddef.h>
#include "ir_state.h"

/* Shared runtime state. Both RTC countdown uses share one two-byte field; the
 * interrupt latch reserves three bytes before the aligned scratch cursor. */

typedef struct {
  u8 secondBcd;
  u8 minuteBcd;
  u8 hourBcd24h;
  u8 pendingUpdates;
} RtcTimeShadow;

typedef struct {
  u32 stepFractionQ9;
  u32 pendingStepsQ9;
  u8 reservedResetByte;
  u8 lastRejected;
} MotionBatch;

typedef struct {
  u8 batchSteps;
  u8 stepsEmitted;
  u8 stepPhase;
} MotionStepPacing;

typedef struct {
  SaveData save;
  volatile u8 buttons;
  volatile u8 previousButtons;
  volatile u8 pressedButtons;
  volatile u8 centerHoldTicks;
  volatile u32 dailySteps;
  volatile u16 hourSteps;
  volatile u16 socialElapsedSeconds;
  volatile RtcTimeShadow time;
  u8 rolloverHourBcd;
  u8 baseContrast;
  volatile u8 menuSelection;
  u8 viewUpdates;
  volatile u8 uiFrame;
  volatile u8 irResult;
  volatile u8 sampleIndex;
  volatile u8 idleSeconds[2];
  volatile u8 view;
  MotionStepPacing stepPacing;
  SystemEvents events;
  volatile SystemFlags flags;
  volatile u16 irTimerStart;
  u8 irReceiveWindow;
  volatile u8 buttonWake[3];
  u16 scratchUsed;
  u32 randomState;
} RuntimeState;

typedef struct {
  u8 outputMode;
  volatile u16 periodsRemaining;
  volatile u16 separatorPeriodsRemaining;
  /* The worker samples tempo separately in each duration arm. */
  volatile u8 durationDivisor;
  ViewState view;
} UiState;

/* One workspace is reused by motion, sound, and infrared communication. Motion
 * owns the first 256 bytes, its following 10-byte epoch state, and scratch
 * storage. IRC instead uses two status records, session state, a complete
 * 136-byte receive frame, and a 128-byte decompression buffer. The receive
 * frame overlaps both motion epoch state and scratch storage; separate C
 * objects would make ordinary packet accesses cross their bounds. Keep the
 * typed views in one top-level union, including the beeper score. */
typedef union {
  DeviceStatus status;
  u8 bytes[sizeof(DeviceStatus)];
} StatusBuffer;

typedef union {
  u8 bytes[0x600];
  u16 words[0x300];
  struct {
    u8 scratch[0x400];
    u8 retainedReserve[0x200];
  } layout;
} ScratchStorage;

typedef union {
  struct {
    u16 fftAccumulator[32];
    /* Power and diagnostic consumers re-read samples in each delta arm. */
    volatile s8 x[64];
    volatile s8 y[64];
    volatile s8 z[64];
    MotionBatch batch;
    ScratchStorage scratch;
  } motion;
  struct {
    u8 reservedBeforeScore[64];
    Note score[96];
  } beeper;
  struct {
    StatusBuffer statusA;
    StatusBuffer statusB;
    IrcWork work;
    u8 packet[136];
    u8 eepromScratch[128];
  } irc;
} Workspace;

typedef char
    EpochStatusBufferMustBe104Bytes[sizeof(StatusBuffer) == 104 ? 1 : -1];
typedef char BeeperScoreMustStartAfter64Bytes
    [offsetof(Workspace, beeper.score) == 64 ? 1 : -1];
typedef char EpochArenaMustBe0x70ABytes[sizeof(Workspace) == 0x70A ? 1 : -1];

typedef union {
  u16 word;
  struct {
    u8 index;
    u8 reserved;
  } bytes;
} DisplayBank;

/* Startup clears the complete workspace. Scratch allocation uses its first
 * 0x400 bytes; the following 0x200 bytes remain reserved. */
typedef char RuntimeStateMustBe0x44Bytes[sizeof(RuntimeState) == 0x44 ? 1 : -1];
typedef char
    ScratchStorageMustBe0x600Bytes[sizeof(ScratchStorage) == 0x600 ? 1 : -1];
typedef char EpochArenaPacketMustFollowSession
    [offsetof(Workspace, irc.packet) == 232 ? 1 : -1];
typedef char EpochArenaDecodeMustFollowPacket
    [offsetof(Workspace, irc.eepromScratch) == 368 ? 1 : -1];
typedef char EpochArenaScratchMustFollowMotion
    [offsetof(Workspace, motion.scratch) == 266 ? 1 : -1];

extern RuntimeState g_state;
extern const Note *g_note;
extern UiState g_ui;
extern void (*g_task)(void);
extern void (*g_previousTask)(void);
extern DisplayBank g_displayBank;
extern Workspace g_work;

#endif
