#include "browser.h"
#include "types.h"
#include "project.h"
#include "fft.h"
#include "motion.h"
#include "power.h"

void MotionReset(void)
{
  g_work.motion.batch.reservedResetByte = 0;
  g_work.motion.batch.pendingStepsQ9 = 0;
  g_work.motion.batch.stepFractionQ9 = 0;
  g_work.motion.batch.lastRejected = 0;
}

extern const u8 g_cadenceBins[10];

s32 MotionEstimate(u16 *spectrum);

#define PW_MOTION_IDLE_SECONDS 30
#define PW_STEP_DISPLAY_MAX_HOUR 9999
#define PW_STEP_DISPLAY_MAX_DAY 99999

/* Compute the Q9 weighted centroid around class_index in a ten-bin window
 * starting at bin_5. The caller supplies class 0..9. Edge classes use two bins;
 * interior classes use three. */
s16 MotionCentroid(u8 classIndex, u16 *bin5)
{
  u16 total;
  u32 weighted;

  if (classIndex == 0) {
    total = bin5[classIndex] + bin5[classIndex + 1];
    weighted = (u32)bin5[classIndex] * ((classIndex + 5) << 9) +
               (u32)bin5[classIndex + 1] * ((classIndex + 6) << 9);
  } else if (classIndex == 9) {
    total = bin5[8] + bin5[9];
    weighted = (u32)bin5[8] * 0x1a00 + (u32)bin5[9] * 0x1c00;
  } else {
    total = bin5[classIndex - 1] + bin5[classIndex + 1] + bin5[classIndex];
    weighted = (u32)bin5[classIndex - 1] * ((classIndex + 4) << 9) +
               (u32)bin5[classIndex + 1] * ((classIndex + 6) << 9) +
               (u32)bin5[classIndex] * ((classIndex + 5) << 9);
  }
  return (weighted / total);
}

/* FFT three accelerometer axes, peak-pick a cadence, then fold Q9 epoch
 * remainder into step / watt / hour / day counters. */
void MotionProcess(void)
{
  u16 i;
  s32 cadence;
  u8 mode;

  g_state.events.byte &= EVENT_CLEAR(EVENT_MOTION);
  for (i = 0; i < 32; i++) {
    g_work.motion.fftAccumulator[i] = 0;
  }

  FftAccumulate(g_work.motion.x);
  FftAccumulate(g_work.motion.y);
  FftAccumulate(g_work.motion.z);
  cadence = MotionEstimate(g_work.motion.fftAccumulator);
  BrowserMotionResult(cadence);

  mode = g_state.view;
  if (mode == VIEW_THRESHOLD_TEST) {
    if (g_ui.view.accel.walkingSamples <
        g_ui.view.accel.countParameters.stepSamples) {
      if (cadence != 0) {
        g_ui.view.accel.walkingSamples++;
        if (g_ui.view.accel.xActivity <
            g_ui.view.accel.countParameters.motionMinimum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.yActivity <
            g_ui.view.accel.countParameters.motionMinimum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.zActivity <
            g_ui.view.accel.countParameters.motionMinimum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.xActivity >
            g_ui.view.accel.countParameters.motionMaximum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.yActivity >
            g_ui.view.accel.countParameters.motionMaximum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.zActivity >
            g_ui.view.accel.countParameters.motionMaximum) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
      }
    } else if (g_ui.view.accel.stillSamples <
               g_ui.view.accel.countParameters.stopSamples) {
      if ((g_ui.view.accel.xActivity <
           g_ui.view.accel.countParameters.stopLimit) &&
          (g_ui.view.accel.yActivity <
           g_ui.view.accel.countParameters.stopLimit) &&
          (g_ui.view.accel.zActivity <
           g_ui.view.accel.countParameters.stopLimit)) {
        g_ui.view.accel.stillSamples++;
      }
    }
  }

  if (cadence == 0) {
    g_work.motion.batch.pendingStepsQ9 = 0;
    return;
  }

  g_state.events.byte |= EVENT_MOTION;

  if (g_work.motion.batch.pendingStepsQ9 != 0) {
    g_work.motion.batch.stepFractionQ9 += g_work.motion.batch.pendingStepsQ9;
    g_work.motion.batch.pendingStepsQ9 = 0;
    g_state.stepPacing.batchSteps =
        ((s32)g_work.motion.batch.stepFractionQ9 >> 9);
    g_work.motion.batch.stepFractionQ9 &= 0x1fful;
    g_state.hourSteps = (g_state.hourSteps + g_state.stepPacing.batchSteps);
    if (g_state.hourSteps > PW_STEP_DISPLAY_MAX_HOUR) {
      g_state.hourSteps = PW_STEP_DISPLAY_MAX_HOUR;
    }
    g_state.dailySteps += g_state.stepPacing.batchSteps;
    if (g_state.dailySteps > PW_STEP_DISPLAY_MAX_DAY) {
      g_state.dailySteps = PW_STEP_DISPLAY_MAX_DAY;
    }
    StoreTotalSteps(g_state.save.totalSteps + g_state.stepPacing.batchSteps);
    g_state.save.stepsTowardNextWatt += g_state.stepPacing.batchSteps;
    if (g_state.save.stepsTowardNextWatt >= STEPS_PER_WATT) {
      g_state.save.stepsTowardNextWatt =
          (g_state.save.stepsTowardNextWatt - STEPS_PER_WATT);
      g_state.save.watts++;
      if (g_state.save.watts > WATTS_MAX) {
        g_state.save.watts = WATTS_MAX;
      }
    }
  }

  g_work.motion.batch.stepFractionQ9 += cadence;
  g_state.stepPacing.batchSteps =
      ((s32)g_work.motion.batch.stepFractionQ9 >> 9);
  g_work.motion.batch.stepFractionQ9 &= 0x1fful;
  if (g_state.stepPacing.batchSteps != 0) {
    g_state.idleSeconds[1] = PW_MOTION_IDLE_SECONDS;
  }
  g_state.stepPacing.stepsEmitted = 0;
  g_state.stepPacing.stepPhase = 0x20;
}

/* Peak-pick a 10-bin cadence class from bins 5..14 of a 32-bin magnitude
 * spectrum, then either reject the batch or return the Q9 centroid. */
s32 MotionEstimate(u16 *spectrum)
{
  u16 max;
  u16 best;
  u16 *cursor;
  u16 *window;
  u16 divisor;
  u8 *entry;
  struct {
    u16 energy;
    u8 classIndex;
    u8 i;
  } walk;

  max = 0;
  window = spectrum;
  window++;
  walk.i = 0;
  cursor = window;
  do {
    if (max < *cursor) {
      max = *cursor;
    }
    walk.i++;
    cursor++;
  } while (walk.i < 0x1d);

  best = 0;
  walk.classIndex = 0xff;
  window = spectrum;
  window += 5;
  walk.i = 0;
  do {
    walk.energy = window[*(entry = g_cadenceBins + walk.i)];
    if (walk.energy >= 0x200) {
      if ((u16)(best * 3) < (u16)(walk.energy * 2)) {
        walk.classIndex = *entry;
        best = walk.energy;
      }
    }
    walk.i++;
  } while (walk.i < 10);

  if (g_work.motion.batch.lastRejected != 0) {
    divisor = 3;
    best = ((u16)(best << 2) / divisor);
    if (max > best) {
      walk.classIndex = 0xff;
    }
  } else {
    if (max > (u16)(best << 1)) {
      walk.classIndex = 0xff;
    }
  }

  if (walk.classIndex == 0xff) {
    g_work.motion.batch.reservedResetByte = 0;
    g_work.motion.batch.pendingStepsQ9 = 0;
    g_work.motion.batch.lastRejected = 1;
    return 0;
  }
  g_work.motion.batch.lastRejected = 0;
  return MotionCentroid(walk.classIndex, spectrum + 5);
}

/* cadence bin search order */
const u8 g_cadenceBins[10] = {0x03, 0x02, 0x04, 0x01, 0x05,
                              0x00, 0x06, 0x07, 0x08, 0x09};
