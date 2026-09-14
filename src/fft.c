#include "types.h"
#include "project.h"
#include "fft.h"
#include "scratch.h"

#define PW_FFT_N 64
#define PW_FFT_LAST 63
#define PW_FFT_HALF 32
#define PW_FFT_WORK_BYTES 0x80
#define PW_Q11_SHIFT 11

#define PW_FFT_PRODUCT(a, b) ((s32)(a) * (b))

/* 64-point radix-2 DIT FFT of one signed 8-bit accelerometer axis. Real inputs
 * are sign-extended samples and imaginary inputs start at zero. After six
 * stages, add the L1 magnitudes of bins 0..31 to the shared accumulator. */
void FftAccumulate(const volatile s8 *samples)
{
  s16 *real;
  s16 *imaginary;
  u8 i;
  u8 j;
  u8 stage;
  u8 span;
  u8 phaseStep;
  u8 twiddle;
  u8 partner;
  s16 cosineQ11;
  s16 sineQ11;
  s16 rotatedReal;
  s16 rotatedImaginary;
  u16 bin;

  ScratchReset();
  real = ScratchAlloc(PW_FFT_WORK_BYTES);
  imaginary = ScratchAlloc(PW_FFT_WORK_BYTES);

  {
    u8 n;
    for (n = 0; n < PW_FFT_N; n++) {
      real[n] = samples[n];
    }
  }

  {
    u8 n;
    s16 zero;
    zero = 0;
    n = 0;
    do {
      imaginary[n] = zero;
      n++;
      imaginary[n] = zero;
      n++;
    } while (n < PW_FFT_N);
  }

  j = 0;
  for (i = 1; i < PW_FFT_LAST; i++) {
    for (partner = PW_FFT_HALF; partner > (j ^= partner); partner >>= 1) {
    }
    if (i < j) {
      real[j] ^= real[i];
      real[i] ^= real[j];
      real[j] ^= real[i];
    }
  }

  phaseStep = PW_FFT_N;
  for (stage = 1; (span = (stage * 2)) <= PW_FFT_N; stage = span) {
    phaseStep >>= 1;
    for (j = twiddle = 0; j < stage; j++, twiddle += phaseStep) {
      cosineQ11 = g_sineQ11[(u16)twiddle + 16];
      sineQ11 = g_sineQ11[twiddle];
      for (i = j; i < PW_FFT_N; i += span) {
        partner = (i + stage);
        if (twiddle == 0) {
          rotatedReal = real[partner];
          rotatedImaginary = imaginary[partner];
        } else if (twiddle == 16) {
          rotatedReal = -imaginary[partner];
          rotatedImaginary = real[partner];
        } else if (imaginary[partner] == 0) {
          rotatedReal =
              ((PW_FFT_PRODUCT(real[partner], cosineQ11)) >> PW_Q11_SHIFT);
          rotatedImaginary =
              ((PW_FFT_PRODUCT(real[partner], sineQ11)) >> PW_Q11_SHIFT);
        } else {
          /* Scale each complete complex component once, after the wide sum. */
          rotatedReal = ((PW_FFT_PRODUCT(real[partner], cosineQ11) -
                          PW_FFT_PRODUCT(imaginary[partner], sineQ11)) >>
                         PW_Q11_SHIFT);
          rotatedImaginary = ((PW_FFT_PRODUCT(imaginary[partner], cosineQ11) +
                               PW_FFT_PRODUCT(real[partner], sineQ11)) >>
                              PW_Q11_SHIFT);
        }
        real[partner] = (real[i] - rotatedReal);
        imaginary[partner] = (imaginary[i] - rotatedImaginary);
        real[i] = (real[i] + rotatedReal);
        imaginary[i] = (imaginary[i] + rotatedImaginary);
      }
    }
  }

  /* Accumulate the lower-half L1 magnitudes across acceleration axes. */
  for (bin = 0; bin < PW_FFT_HALF; bin++) {
    g_work.motion.fftAccumulator[bin] +=
        ((real[bin] >= 0 ? real[bin] : -real[bin]) +
         (imaginary[bin] >= 0 ? imaginary[bin] : -imaginary[bin]));
  }
}

/* Q11 sine, 64 entries plus 16 wrap for cosine */
const s16 g_sineQ11[80] = {
    0,     201,   400,   595,   784,   965,   1138,  1299,  1448,  1583,
    1703,  1806,  1892,  1960,  2009,  2038,  2048,  2038,  2009,  1960,
    1892,  1806,  1703,  1583,  1448,  1299,  1138,  965,   784,   595,
    400,   201,   0,     -201,  -400,  -595,  -784,  -965,  -1138, -1299,
    -1448, -1583, -1703, -1806, -1892, -1960, -2009, -2038, -2048, -2038,
    -2009, -1960, -1892, -1806, -1703, -1583, -1448, -1299, -1138, -965,
    -784,  -595,  -400,  -201,  0,     201,   400,   595,   784,   965,
    1138,  1299,  1448,  1583,  1703,  1806,  1892,  1960,  2009,  2038};
