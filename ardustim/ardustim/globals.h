/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Arbritrary wheel pattern generator wheel definitions
 *
 * copyright 2014 David J. Andruczyk
 * 
 * Ardu-Stim software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ArduStim software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with any ArduStim software.  If not, see http://www.gnu.org/licenses/
 *
 */
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "Arduino.h"
#include "wheel_defs.h"

#define VERSION 2
 
#define TMP_RPM_SHIFT 4 /* x16, 0-16384 RPM via pot */
#define TMP_RPM_CAP 9000 /* MAX RPM via pot control. Adjusted to 9,000rpm max from 16,384rpm to match the GUI */
#define EEPROM_LAST_MODE  100


struct configTable 
{
  uint8_t version;                                                   
  uint8_t wheel = FOUR_TWENTY_A;
  uint8_t mode;
  uint16_t fixed_rpm = 2500;
  uint16_t sweep_low_rpm = 250;
  uint16_t sweep_high_rpm = 4000;
  uint16_t sweep_interval = 1000;

  //11
  // THE CRANK CURVE. A cranking engine does not turn at a speed — it stalls against each compression
  // and runs away between them, and a capture taken on a starter has that written all over it. The
  // stim used to model this as "compression": a fixed amplitude baked into three lookup tables, a
  // frequency chosen from an enum of 2/4/6/8 cylinders, and a modifier that could only ever SUBTRACT
  // — so switching it on lowered the mean speed rather than swinging about it, and the wheel no
  // longer ran at the rpm it was asked for. Amplitude and frequency are both numbers now, and the
  // curve is symmetric about the mean, so turning it on changes the shape and not the speed.
  bool     crankCurve   = false;   // off = a perfectly steady wheel, which no real engine is
  uint8_t  curveHalves  = 4;       // compressions per revolution X2: 3 a triple, 4 a four, 6 a six, 8 an eight
  uint8_t  curveAmplitude = 0;     // percent of the mean speed, each way. 0 is the same as off
  uint16_t curveMaxRpm  = 600;     // above this the curve is off: a running engine does not do this
  uint16_t curveOffset  = 0;       // degrees — where the first compression falls in the turn
} __attribute__ ((packed));
extern struct configTable config;


struct status 
{
  uint16_t base_rpm;    // the mean the curve swings about
  int16_t  curveDelta;  // what the curve is adding or taking off, right now
  uint16_t rpm; //Final RPM
};
extern struct status currentStatus;

// HALT: the wheel stopped, as distinct from turning slowly. config.fixed_rpm is constrained to a
// floor of 100 rpm (storage.ino), so 'F' with 0 cannot stop the pattern — it just runs the wheel at
// the floor. Diagnosing an ECU's behaviour with NO trigger at all (sync loss, stale capture, cold
// start before the first tooth) needs the edges to actually cease, so this gates the generator ISR.
extern volatile bool halted;
extern volatile uint8_t  output_invert_mask;
extern volatile uint16_t edge_counter;

/* Tie things wheel related into one nicer structure ... */
typedef struct _wheels wheels;
struct _wheels {
  const char *decoder_name PROGMEM;
  const unsigned char *edge_states_ptr PROGMEM;
  const uint16_t wheel_max_edges;
  const uint16_t wheel_degrees;
};

//A sin wave of amplitude 100 with a complete cycle in 180 degrees (1 entry per degree). 
// ONE SIGNED SINE, over a full turn of the curve, in 256 steps of 1/127.
//
// It replaces three unsigned tables — one per cylinder count — that between them could only pull the
// speed DOWN, by an amount the caller could not choose. A compression slows the engine and the
// expansion after it speeds the engine up; a model with only the first half of that is not a slower
// engine, it is a different engine.
// PUTTING BACK THE REVOLUTIONS THE SWING COSTS.
//
// Swinging the instantaneous speed evenly either side of the mean does NOT leave the mean speed
// alone: the engine spends longer at the slow end than at the fast one, so it completes fewer
// revolutions per minute. The shortfall is exactly sqrt(1 - amplitude^2) — ask for 300 rpm at 45%
// and the wheel turns at 268, which is what the fitter measured off the bench and what sent this
// back to be corrected. A stim that reports the rpm you typed while emitting another is the same
// fault as the hand-typed rpm_scaler, and it is worth no more trust here than it was there.
//
// So the mean is pre-divided by that factor. In 1/256ths, by amplitude in steps of 5%.
const uint16_t crank_curve_mean_fix[19] PROGMEM =
{
  256, 256, 257, 259, 261, 264, 268, 273, 279, 287, 296, 307, 320, 337, 358, 387, 427, 486, 587
};

const int8_t crank_curve_sine[256] PROGMEM =
{
     0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
    49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
    90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
   117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
   127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
   117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
    90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
    49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
     0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
   -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
   -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
  -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
  -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
  -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
   -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
   -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
};

#endif
