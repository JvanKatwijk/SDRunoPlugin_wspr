#pragma once

#define SAMPLING_RATE       192000

#define SIGNAL_LENGTH       116
#define SIGNAL_SAMPLE_RATE  375
#define SIGNAL_SAMPLES      SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE
#define NBITS               81
#define NSYM                162
#define NSPERSYM            256
#define DF                  375.0 / 256.0
#define DT                  1.0 / 375.0
#define DF05                DF * 0.5
#define DF15                DF * 1.5
#define TWOPIDT             2.0 * M_PI * DT

#define MHz(x)  (1000000 * x)

