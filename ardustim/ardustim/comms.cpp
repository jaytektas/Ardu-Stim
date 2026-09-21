/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Arbritrary wheel pattern generator
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

#include "globals.h"
#include "ardustim.h"
#include "enums.h"
#include "comms.h"
#include "storage.h"
#include "wheel_defs.h"
#include <avr/pgmspace.h>
#include <math.h>
#include <util/delay.h>

/* External Globla Variables */
extern wheels Wheels[];

/* Volatile variables (USED in ISR's) */
extern volatile bool normal;
extern volatile uint16_t edge_counter;
extern volatile uint16_t new_OCR1A;

bool cmdPending;
byte currentCommand;

//! Initializes the serial port and sets up the Menu
/*!
 * Sets up the serial port and menu for the serial user interface
 * Sets user input timeout to 20 seconds and overall interactivity timeout at 30
 * at which point it'll disconnect the user
 */
void serialSetup()
{
  Serial.begin(115200);
  cmdPending = false;
}

// Wait briefly for one more byte. Returns false if none arrives, so a stray command letter cannot
// wedge the parser waiting for an argument that is never coming.
static bool await_byte()
{
  for (uint16_t i = 0; i < 2000; i++) {      // ~5 ms at 16 MHz
    if (Serial.available() > 0) return true;
    delayMicroseconds(2);
  }
  return false;
}

void commandParser()
{
  char buf[80];
  byte tmp_wheel;
  void* pnt_Config = &config;
  if (cmdPending == false) { currentCommand = Serial.read(); }

  switch (currentCommand)
  {
    case 'a':
      break;

    case 'c': //Receive a full config buffer
      //uint8_t targetBytes = (sizeof(struct configTable)-1); //No byte is sent for the version
      while(Serial.available() < (sizeof(struct configTable)-1) ) {} //Wait for all bytes
      for(uint8_t x=1; x<(sizeof(struct configTable)); x++)
      {
        *((uint8_t *)pnt_Config + x) = Serial.read(); //Read each byte into the config table
      }
      saveConfig();
      break;

    case 'C': //Send the current config
      for(uint8_t x=0; x<sizeof(struct configTable); x++)
      {
        Serial.write(*((uint8_t *)pnt_Config + x)); //Each byte is simply the location in memory of the config Page + the offset
      }
      break;
      
    case 'L': // send the list of wheel names
      //First byte sent is the number of wheels
      //Serial.println(MAX_WHEELS);
      
      //Wheel names are then sent 1 per line
      for(byte x=0;x<MAX_WHEELS;x++)
      {
        strcpy_P(buf,Wheels[x].decoder_name);
        Serial.println(buf);
      }
      break;

    case 'n': //Send the number of wheels
      Serial.println(MAX_WHEELS);
      break;

    case 'N': //Send the number of the current wheel
      Serial.println(config.wheel);
      break;
    
    case 'p': //Send the size of the current wheel
      Serial.println(Wheels[config.wheel].wheel_max_edges);
      break;

    case 'P': //Send the pattern for the current wheel
      for(uint16_t x=0; x<Wheels[config.wheel].wheel_max_edges; x++)
      {
        if(x != 0) { Serial.print(","); }

        byte tempByte = pgm_read_byte(&Wheels[config.wheel].edge_states_ptr[x]);
        Serial.print(tempByte);
      }
      Serial.println("");
      //2nd row of data sent is the number of degrees the wheel runs over (360 or 720 typically)
      Serial.println(Wheels[config.wheel].wheel_degrees);
      break;

    case 'V': //Set the CRANK CURVE: amplitude (percent), compressions per revolution x2, and the
      // speed above which the wheel runs steady again. Amplitude 0 turns it off, so one command both
      // arms and disarms it and there is no separate enable to get out of step with.
      //
      // FIVE BYTES, and the last two are the rpm ceiling LITTLE-endian, matching 'F'. Like every
      // other command here the arguments are consumed positionally, so they are read together or not
      // at all — see the note on 'H' for what a single stray byte does to a protocol with no framing.
      while(Serial.available() < 4) {}
      config.curveAmplitude = Serial.read();
      config.curveHalves    = Serial.read();
      config.curveMaxRpm    = word(Serial.read(), Serial.read());
      if(config.curveAmplitude > 90) { config.curveAmplitude = 90; }
      if(config.curveHalves < 1 || config.curveHalves > 16) { config.curveHalves = 4; }
      config.crankCurve = (config.curveAmplitude > 0);
      // Persisted for the same reason the wheel and the rpm are: opening this port resets the board,
      // so anything not in EEPROM is lost the moment the next host connects.
      saveConfig();
      break;

    case 'v': //Report the crank curve, so a bench script can CHECK what it set rather than assume it
      Serial.print(config.curveAmplitude);
      Serial.print(",");
      Serial.print(config.curveHalves);
      Serial.print(",");
      Serial.println(config.curveMaxRpm);
      break;

    case 'R': //Send the current RPM
      Serial.println(currentStatus.rpm);
      break;

    case 'r': //Set the high and low RPM for sweep mode
      config.mode = LINEAR_SWEPT_RPM;
      while(Serial.available() < 6) {} //Wait for 4 bytes representing the new low and high RPMs

      config.sweep_low_rpm = word(Serial.read(), Serial.read());
      config.sweep_high_rpm = word(Serial.read(), Serial.read());
      config.sweep_interval = word(Serial.read(), Serial.read());
      saveConfig();

      //sweep_low_rpm = 100;
      //sweep_high_rpm = 4000;
      break;

    case 'F': //Set a FIXED rpm — software override of the pot. Switches mode to FIXED_RPM so loop()
      // drives config.fixed_rpm directly, letting a bench host hold a steady RPM without the pot.
      // The host sends the 16-bit RPM LITTLE-endian — verified on the bench as b'F'+pack('<H', rpm).
      config.mode = FIXED_RPM;
      while(Serial.available() < 2) {} //Wait for the 2 RPM bytes
      config.fixed_rpm = word(Serial.read(), Serial.read());
      // PERSIST IT. Opening this port raises DTR, DTR is capacitor-coupled to RESET, so the board
      // reboots whenever any host on any machine connects, meant to or not. That cannot be fixed from
      // the host side, so the board stops LOSING anything to a reboot instead: it comes back up on
      // whatever wheel and rpm it was last told. saveConfig() is EEPROM.update throughout, so
      // unchanged bytes are never rewritten and saving on every command costs no wear.
      saveConfig();
      break;

    // 'H'/'h' take a MAGIC second byte. This protocol has no framing and every other command with
    // arguments ('F' + 2 rpm bytes, 'S' + 1 wheel byte) consumes them positionally, so one dropped
    // or extra byte shifts an ARGUMENT into the command slot. Before halt existed a stray byte hit
    // default: and did nothing; a bare 'H' would silently stop the pattern generator instead — and
    // 0x48 is 'H', which is the high byte of any rpm from 18432 to 18687, a range this bench sweeps
    // through. Requiring 0x5A makes a lone stray byte harmless again.
    case 'H': //HALT the wheel — stop emitting edges entirely (a stopped engine, not a slow one).
      // 'F' cannot express this: fixed_rpm is constrained to a 100 rpm floor, so the wheel keeps
      // turning. Masking TIMER1's compare interrupt stops the pattern generator dead and leaves the
      // port at its idle level, so an ECU sees no teeth at all.
      // BOUNDED wait, not `while(Serial.available()<1){}`. A blocking wait turns a stray 0x48 with
      // nothing behind it into a permanent lockup of the command parser — worse than the silent
      // halt it was added to prevent. Give the magic byte a few ms to arrive, then give up.
      if (!await_byte()) break;
      if (Serial.read() != 0x5A) break;          // not a real halt — a stray byte
      halted = true;
      TIMSK1 &= ~(1 << OCIE1A);
      PORTB = output_invert_mask;        // idle level, honouring the invert mask
      currentStatus.rpm = 0;
      Serial.println(F("HALTED"));
      break;

    case 'h': //Resume from halt, restarting the pattern at edge 0.
      if (!await_byte()) break;
      if (Serial.read() != 0x5A) break;
      if (halted)
      {
        edge_counter = 0;
        TCNT1 = 0;
        halted = false;
        TIMSK1 |= (1 << OCIE1A);
      }
      Serial.println(F("RUNNING"));
      break;

    case 's': //Save the current config
      saveConfig();
      break;

    case 'S': //Set the current wheel
      while(Serial.available() < 1) {} 
      tmp_wheel = Serial.read();
      if(tmp_wheel < MAX_WHEELS)
      {
        config.wheel = tmp_wheel;
        display_new_wheel();
        saveConfig();
      }
      break;

    case 'X': //Just a test method for switching the to the next wheel
      select_next_wheel_cb();
      strcpy_P(buf,Wheels[config.wheel].decoder_name);
      Serial.println(buf);
      saveConfig();
      break;

    default:
      break;
  }
  cmdPending = false;
}

/* Helper function to spit out amount of ram remainig */
//! Returns the amount of freeRAM
/*!
 * Figures out the amount of free RAM remaining nad returns it to the caller
 * \return amount of free memory
 */
uint16_t freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

/* SerialUI Callbacks */
//! Inverts the polarity of the primary output signal
void toggle_invert_primary_cb()
{
  extern uint8_t output_invert_mask;
  output_invert_mask ^= 0x01; /* Flip crank invert mask bit */

}

//! Inverts the polarity of the secondary output signal
void toggle_invert_secondary_cb()
{
  extern uint8_t output_invert_mask;
  output_invert_mask ^= 0x02; /* Flip cam invert mask bit */
}

void display_new_wheel()
{
  reset_new_OCR1A(currentStatus.rpm);
  edge_counter = 0; // Reset to beginning of the wheel pattern */
}


//! Selects the next wheel in the list
/*!
 * Selects the next wheel, if at the end, wrap to the beginning of the list,
 * re-calculate the OCR1A value (RPM) and reset, return user information on the
 * selected wheel and current RPM
 */
void select_next_wheel_cb()
{
  if (config.wheel == (MAX_WHEELS-1))
    config.wheel = 0;
  else 
    config.wheel++;
  
  display_new_wheel();
}

//
//! Selects the previous wheel in the list
/*!
 * Selects the nex, if at the beginning, wrap to the end of the list,
 * re-calculate the OCR1A value (RPM) and reset, return user information on the
 * selected wheel and current RPM
 */
void select_previous_wheel_cb()
{
  if (config.wheel == 0)
    config.wheel = MAX_WHEELS-1;
  else 
    config.wheel--;
  
  display_new_wheel();
}