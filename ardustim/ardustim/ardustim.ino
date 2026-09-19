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
#include <EEPROM.h>

struct configTable config;
struct status currentStatus;

/* Sensistive stuff used in ISR's */
volatile uint16_t adc0; /* POT RPM */
volatile uint16_t adc1; /* Pot Wheel select */
/* Setting rpm to any value over 0 will enabled sweeping by default */
/* Stuff for handling prescaler changes (small tooth wheels are low RPM) */
volatile uint8_t analog_port = 0;
volatile bool adc0_read_complete = false;
volatile bool adc1_read_complete = false;
volatile bool reset_prescaler = false;
volatile uint8_t output_invert_mask = 0x00; /* Don't invert anything */
volatile uint8_t prescaler_bits = 0;
volatile uint8_t last_prescaler_bits = 0;
volatile uint16_t new_OCR1A = 5000; /* sane default */
volatile uint16_t edge_counter = 0;
/* Wheel halted — TIMER1's compare interrupt is masked off, so no edges are emitted at all. */
volatile bool halted = false;
volatile uint32_t cycleStartTime = micros();
volatile uint32_t cycleDuration = 0;
uint32_t sweep_time_counter = 0;
uint8_t sweep_direction = ASCENDING;

/* Less sensitive globals */
uint8_t bitshift = 0;




wheels Wheels[MAX_WHEELS] = {
   /* Pointer to friendly name string, pointer to edge array, number of entries in that array,
    * and whether those entries cover 360 or 720 crank degrees.
    *
    * THERE IS NO RPM SCALER HERE ANY MORE. It was a third number, hand-computed from the two that
    * follow, and fourteen rows disagreed with their own inputs — so it is now derived where it is
    * used (reset_new_OCR1A) and cannot drift. A row states what the wheel IS; the clock rate follows. */
  { dizzy_four_cylinder_friendly_name, dizzy_four_cylinder, 4, 360 },
  { dizzy_six_cylinder_friendly_name, dizzy_six_cylinder, 6, 360 },
  { dizzy_eight_cylinder_friendly_name, dizzy_eight_cylinder, 8, 360 },
  { sixty_minus_two_friendly_name, sixty_minus_two, 120, 360 },
  { sixty_minus_two_with_cam_friendly_name, sixty_minus_two_with_cam, 240, 720 },
  { sixty_minus_two_with_halfmoon_cam_friendly_name, sixty_minus_two_with_halfmoon_cam, 240, 720 },
  { thirty_six_minus_one_friendly_name, thirty_six_minus_one, 72, 360 },
  /* This row once carried a hand-typed 0.5 where the convention wanted 0.4, so it spun 25% FASTER
   * than the RPM it reported — found by counting edges on a bench, because the stim's own 'R' will
   * always agree with whatever you set. That fix corrected one row; thirteen others were wrong the
   * same way and stayed wrong. The scaler is derived now, so neither can happen again. */
  { twenty_four_minus_one_friendly_name, twenty_four_minus_one, 48, 360 },
  { four_minus_one_with_cam_friendly_name, four_minus_one_with_cam, 16, 720 },
  { eight_minus_one_friendly_name, eight_minus_one, 16, 360 },
  { six_minus_one_with_cam_friendly_name, six_minus_one_with_cam, 36, 720 },
  { twelve_minus_one_with_cam_friendly_name, twelve_minus_one_with_cam, 144, 720 },
  { fourty_minus_one_friendly_name, fourty_minus_one, 80, 360 },
  { dizzy_four_trigger_return_friendly_name, dizzy_four_trigger_return, 9, 720 },
  { oddfire_vr_friendly_name, oddfire_vr, 24, 360 },
  { optispark_lt1_friendly_name, optispark_lt1, 720, 720 },
  { twelve_minus_three_friendly_name, twelve_minus_three, 48, 360 },
  { thirty_six_minus_two_two_two_friendly_name, thirty_six_minus_two_two_two, 72, 360 },
  { thirty_six_minus_two_two_two_h6_friendly_name, thirty_six_minus_two_two_two_h6, 72, 360 },
  { thirty_six_minus_two_two_two_with_cam_friendly_name, thirty_six_minus_two_two_two_with_cam, 144, 720 },
  { fourty_two_hundred_wheel_friendly_name, fourty_two_hundred_wheel, 72, 360 },
  { thirty_six_minus_one_with_cam_fe3_friendly_name, thirty_six_minus_one_with_cam_fe3, 144, 720 },
  { six_g_seventy_two_with_cam_friendly_name, six_g_seventy_two_with_cam, 144, 720 },
  { buell_oddfire_cam_friendly_name, buell_oddfire_cam, 80, 720 },
  { gm_ls1_crank_and_cam_friendly_name, gm_ls1_crank_and_cam, 720, 720 },
  { gm_ls_58X_crank_and_4x_cam_friendly_name, GM_LS_58X_crank_and_4x_cam, 240, 720 },
  { lotus_thirty_six_minus_one_one_one_one_friendly_name, lotus_thirty_six_minus_one_one_one_one, 72, 360 },
  { honda_rc51_with_cam_friendly_name, honda_rc51_with_cam, 48, 720 },
  { thirty_six_minus_one_with_second_trigger_friendly_name, thirty_six_minus_one_with_second_trigger, 144, 720 },
  { chrysler_ngc_thirty_six_plus_two_minus_two_with_ngc4_cam_friendly_name, chrysler_ngc_thirty_six_plus_two_minus_two_with_ngc4_cam, 720, 720 },
  { chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc6_cam_friendly_name, chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc6_cam, 720, 720 },
  { chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc8_cam_friendly_name, chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc8_cam, 720, 720 },
  { weber_iaw_with_cam_friendly_name, weber_iaw_with_cam, 144, 720 },
  { fiat_one_point_eight_sixteen_valve_with_cam_friendly_name, fiat_one_point_eight_sixteen_valve_with_cam, 720, 720 },
  { three_sixty_nissan_cas_friendly_name, three_sixty_nissan_cas, 720, 720 },
  { twenty_four_minus_two_with_second_trigger_friendly_name, twenty_four_minus_two_with_second_trigger, 72, 720 },
  { yamaha_eight_tooth_with_cam_friendly_name, yamaha_eight_tooth_with_cam, 64, 720 },
  { gm_four_tooth_with_cam_friendly_name, gm_four_tooth_with_cam, 8, 720 },
  { gm_six_tooth_with_cam_friendly_name, gm_six_tooth_with_cam, 12, 720 },
  { gm_eight_tooth_with_cam_friendly_name, gm_eight_tooth_with_cam, 16, 720 },
  { volvo_d12acd_with_cam_friendly_name, volvo_d12acd_with_cam, 480, 720 },
  { mazda_thirty_six_minus_two_two_two_with_six_tooth_cam_friendly_name, mazda_thirty_six_minus_two_two_two_with_six_tooth_cam, 360, 720 },
  { mitsubishi_4g63_4_2_friendly_name, mitsubishi_4g63_4_2, 144, 720 },
  { audi_135_with_cam_friendly_name, audi_135_with_cam, 1080, 720 },
  { honda_d17_no_cam_friendly_name, honda_d17_no_cam, 144, 720 },
  { mazda_323_au_friendly_name, mazda_323_au, 30, 720 },
  { daihatsu_3cyl_friendly_name, daihatsu_3cyl, 144, 360 },
  { miata_9905_friendly_name, miata_9905, 144, 720 },
  { twelve_with_cam_friendly_name, twelve_with_cam, 144, 720 },
  { twenty_four_with_cam_friendly_name, twenty_four_with_cam, 144, 720 },
  { subaru_six_seven_name_friendly_name, subaru_six_seven, 720, 720 },
  { gm_seven_x_friendly_name, gm_seven_x, 180, 720 },
  { four_twenty_a_friendly_name, four_twenty_a, 144, 720 },
  { ford_st170_friendly_name, ford_st170, 720, 720 },
  { mitsubishi_3A92_friendly_name, mitsubishi_3A92, 144, 720 },
  { Toyota_4AGE_CAS_friendly_name, toyota_4AGE_CAS, 144, 720 },
  { Toyota_4AGZE_friendly_name, toyota_4AGZE, 144, 720 },
  { Suzuki_DRZ400_friendly_name, suzuki_DRZ400, 72, 360 },
  { Jeep_2000_4cyl_friendly_name, jeep_2000_4cyl, 360, 720 },
  { Jeep_2000_6cyl_friendly_name, jeep_2000_6cyl, 360, 720 },
  { BMW_N20_friendly_name, bmw_n20, 240, 720 },
  { VIPER9602_friendly_name, viper9602wheel, 240, 720 },
  { thirty_six_minus_two_with_second_trigger_friendly_name, thirty_six_minus_two_with_second_trigger, 144, 720 },
  { GM_40_Tooth_Trans_OSS_friendly_name, GM40toothOSS, 80, 360 },
};

/* Initialization */
void setup() {
  loadConfig();
  serialSetup();

  cli(); // stop interrupts

  /* Configuring TIMER1 (pattern generator) */
  // Set timer1 to generate pulses
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // Set compare register to sane default
  OCR1A = 1000;  /* 8000 RPM (60-2) */

  // Turn on CTC mode
  TCCR1B |= (1 << WGM12); // Normal mode (not PWM)
  // Set prescaler to 1
  TCCR1B |= (1 << CS10); /* Prescaler of 1 */
  // Enable output compare interrupt for timer channel 1 (16 bit)
  TIMSK1 |= (1 << OCIE1A);

  // Set timer2 to run sweeper routine
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  // Set compare register to sane default
  OCR2A = 249;  /* With prescale of x64 gives 1ms tick */

  // Turn on CTC mode
  TCCR2A |= (1 << WGM21); // Normal mode (not PWM)
  // Set prescaler to x64
  TCCR2B |= (1 << CS22); /* Prescaler of 64 */
  // Enable output compare interrupt for timer channel 2
  //TIMSK2 |= (1 << OCIE2A); //Disabled as no longer using TIMER2 for sweep


  /* Configure ADC as per http://www.glennsweeney.com/tutorials/interrupt-driven-analog-conversion-with-an-atmega328p */
  // clear ADLAR in ADMUX (0x7C) to right-adjust the result
  // ADCL will contain lower 8 bits, ADCH upper 2 (in last two bits)
  ADMUX &= B11011111;
  
  // Set REFS1..0 in ADMUX (0x7C) to change reference voltage to the
  // proper source (01)
  ADMUX |= B01000000;
  
  // Clear MUX3..0 in ADMUX (0x7C) in preparation for setting the analog
  // input
  ADMUX &= B11110000;
  
  // Set MUX3..0 in ADMUX (0x7C) to read from AD8 (Internal temp)
  // Do not set above 15! You will overrun other parts of ADMUX. A full
  // list of possible inputs is available in Table 24-4 of the ATMega328
  // datasheet
  // ADMUX |= 8;
  // ADMUX |= B00001000; // Binary equivalent
  
  // Set ADEN in ADCSRA (0x7A) to enable the ADC.
  // Note, this instruction takes 12 ADC clocks to execute
  ADCSRA |= B10000000;
  
  // Set ADATE in ADCSRA (0x7A) to enable auto-triggering.
  ADCSRA |= B00100000;
  
  // Clear ADTS2..0 in ADCSRB (0x7B) to set trigger mode to free running.
  // This means that as soon as an ADC has finished, the next will be
  // immediately started.
  ADCSRB &= B11111000;
  
  // Set the Prescaler to 128 (16000KHz/128 = 125KHz)
  // Above 200KHz 10-bit results are not reliable.
  ADCSRA |= B00000111;
  
  // Set ADIE in ADCSRA (0x7A) to enable the ADC interrupt.
  // Without this, the internal interrupt will not trigger.
  ADCSRA |= B00001000;

//  pinMode(7, OUTPUT); /* Debug pin for Saleae to track sweep ISR execution speed */
  pinMode(8, OUTPUT); /* Primary (crank usually) output */
  pinMode(9, OUTPUT); /* Secondary (cam1 usually) output */
  pinMode(10, OUTPUT); /* Tertiary (cam2 usually) output */
  pinMode(11, OUTPUT); /* Knock signal for seank, ony on LS1 pattern, NOT IMPL YET */
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  pinMode(53, OUTPUT); /* crank */
  pinMode(52, OUTPUT); /* cam 1 */
  pinMode(51, OUTPUT); /* untested - should be cam2*/
#endif

  sei(); // Enable interrupts
  // Set ADSC in ADCSRA (0x7A) to start the ADC conversion
  ADCSRA |= B01000000;
  /* Make sure we are using the DEFAULT RPM on startup */
  reset_new_OCR1A(currentStatus.rpm); 

} // End setup


//! ADC ISR for alternating between ADC pins 0 and 1
/*!
 * Reads ADC ports 0 and 1 alternately. Port 0 is RPM, Port 1 is for
 * future fun (possible wheel selection)
 */
ISR(ADC_vect){
  if (analog_port == 0)
  {
    adc0 = ADCL | (ADCH << 8);
	  adc0_read_complete = true;
    /* Flip to channel 1 */
    //ADMUX |= B00000001;
    //analog_port = 1;
    /* Trigger another conversion */
    //ADCSRA |= B01000000;
    return;
  } 
//  if (analog_port == 1)
//  {
//    adc1 = ADCL | (ADCH << 8);
//	adc1_read_complete = true;
//    /* Flip to channel 0 */
//    /* Tell it to read ADC0, clear MUX0..3 */
//    ADMUX &= B11110000;
//    analog_port = 0;
//    /* Trigger another conversion */
//    ADCSRA |= B01000000;
//    return;
//  }
}

/* Pumps the pattern out of flash to the port 
 * The rate at which this runs is dependent on what OCR1A is set to
 */
ISR(TIMER1_COMPA_vect) 
{
  /* This is VERY simple, just walk the array and wrap when we hit the limit */
  PORTB = output_invert_mask ^ pgm_read_byte(&Wheels[config.wheel].edge_states_ptr[edge_counter]);   /* Write it to the port */
  
  edge_counter++;
  if (edge_counter == Wheels[config.wheel].wheel_max_edges) 
  {
    edge_counter = 0;
    cycleDuration = micros() - cycleStartTime;
    cycleStartTime = micros();
  }
  /* The tables are in flash so we need pgm_read_byte() */

  /* Reset Prescaler only if flag is set */
  if (reset_prescaler)
  {
    TCCR1B &= ~((1 << CS10) | (1 << CS11) | (1 << CS12)); /* Clear CS10, CS11 and CS12 */
    TCCR1B |= prescaler_bits;
    reset_prescaler = false;
  }
  /* Reset next compare value for RPM changes */
  OCR1A = new_OCR1A;  /* Apply new "RPM" from Timer2 ISR, i.e. speed up/down the virtual "wheel" */
}

void loop() 
{
  uint16_t tmp_rpm = currentStatus.base_rpm;
  /* Just handle the Serial UI, everything else is in 
   * interrupt handlers or callbacks from SerialUI.
   */
  if(Serial.available() > 0) { commandParser(); }

  /* Halted: report a stopped wheel and drive nothing. This must sit AFTER commandParser(), or the
   * resume command could never be read. */
  if (halted) { currentStatus.rpm = 0; return; }

  if(config.mode == POT_RPM)
  {
    if (adc0_read_complete == true)
    {
      adc0_read_complete = false;
      tmp_rpm = adc0 << TMP_RPM_SHIFT;
      if (tmp_rpm > TMP_RPM_CAP) { tmp_rpm = TMP_RPM_CAP; }
    }
  }
  else if (config.mode == LINEAR_SWEPT_RPM)
  {
    
    if(micros() > (sweep_time_counter + config.sweep_interval))
    {
      sweep_time_counter = micros();
      if(sweep_direction == ASCENDING)
      {
        tmp_rpm = currentStatus.base_rpm + 1;
        if(tmp_rpm >= config.sweep_high_rpm) { sweep_direction = DESCENDING; }
      }
      else
      {
        tmp_rpm = currentStatus.base_rpm - 1;
        if(tmp_rpm <= config.sweep_low_rpm) { sweep_direction = ASCENDING; }
      }
    }
    
  }
  else if (config.mode == FIXED_RPM)
  {
    tmp_rpm = config.fixed_rpm;
  }
  currentStatus.base_rpm = tmp_rpm;

  currentStatus.compressionModifier = calculateCompressionModifier();
  if(currentStatus.compressionModifier >= currentStatus.base_rpm ) { currentStatus.compressionModifier = 0; }

  setRPM( (currentStatus.base_rpm - currentStatus.compressionModifier) );
}

uint16_t calculateCompressionModifier()
{
  if( (currentStatus.base_rpm > config.compressionRPM) || (config.useCompression != true) ) { return 0; }
  //if( currentStatus.base_rpm > 400 ) { return 0;}

  uint16_t crankAngle = calculateCurrentCrankAngle();
  uint16_t modAngle = crankAngle;

  uint16_t compressionModifier = 0;
  switch(config.compressionType)
  {
    case COMPRESSION_TYPE_2CYL_4STROKE:
      modAngle = crankAngle / 2;
      compressionModifier = pgm_read_byte(&sin_100_180[modAngle]);
      break;
    case COMPRESSION_TYPE_4CYL_4STROKE:
      modAngle = (crankAngle % 180) ;
      compressionModifier = pgm_read_byte(&sin_100_180[modAngle]);
      break;
    case COMPRESSION_TYPE_6CYL_4STROKE:
      modAngle = crankAngle % 120;
      compressionModifier = pgm_read_byte(&sin_100_120[modAngle]);
      break;
    case COMPRESSION_TYPE_8CYL_4STROKE:
      modAngle = crankAngle % 90;
      compressionModifier = pgm_read_byte(&sin_100_90[modAngle]);
      break;
    default:
      modAngle = (crankAngle % 180) ;
      compressionModifier = pgm_read_byte(&sin_100_180[modAngle]);
      break;
  }

  //RPM scaler - Varies the amplitude of the compression modifier based on how far below the compression RPM point we are. Eg:
  //If the compression RPM value is 400
  //At 300rpm the amplitude will be 75%
  //At 200rpm the amplitude will be 50%
  //At 100rpm the amplitude will be 25% etc
  //Base RPM must be below 650 to prevent overflow
  if(config.compressionDynamic && (currentStatus.base_rpm < 655U) ) { compressionModifier = (compressionModifier * currentStatus.base_rpm) / config.compressionRPM; }
  
  return compressionModifier;
}

uint16_t calculateCurrentCrankAngle()
{
  if(cycleDuration == 0) { return 0; }

  uint32_t cycleTime = micros() - cycleStartTime;
  if( Wheels[config.wheel].wheel_degrees == 720 ) { cycleTime = cycleTime * 2; } 
  
  uint16_t tmpCrankAngle = ((cycleTime * 360U) / cycleDuration);
  tmpCrankAngle += config.compressionOffset;
  while(tmpCrankAngle > 360) { tmpCrankAngle -= 360; }

  return tmpCrankAngle;
}

/*!
 * Validates the new user requested RPM and sets it if valid. 
 */ 
void setRPM(uint16_t newRPM)
{
  if (newRPM < 10)  { return; }

  if(currentStatus.rpm != newRPM) { reset_new_OCR1A( newRPM ); }
  currentStatus.rpm = newRPM;
}


void reset_new_OCR1A(uint32_t new_rpm)
{
  uint32_t tmp;
  uint8_t bitshift;
  uint8_t tmp_prescaler_bits;
  // THE SCALER IS DERIVED, NOT STORED. It is (entries per 360 deg)/120, and both of its inputs sit
  // in the same row — so keeping it as a third, hand-typed number meant a row could disagree with
  // itself, and fourteen of the sixty-four did. Six of them by exactly 2x, all cycle wheels whose
  // scaler was computed from entries per CYCLE instead of per REVOLUTION; the rest by 0.33x, 0.55x,
  // 1.5x, 4x and 8x. Nothing caught it because the stim reports the rpm you ASKED for ('R' prints
  // currentStatus.rpm, the setpoint) and never counts its own output, so the instrument agreed with
  // whatever you typed while emitting something else. Anything ever calibrated against one of those
  // wheels was calibrated against a lie — see the note on the 24-1 wheel, which is this same bug
  // found the hard way and fixed one row at a time.
  const float rpm_scaler = (float)Wheels[config.wheel].wheel_max_edges * 3.0f
                         / (float)Wheels[config.wheel].wheel_degrees;
  tmp = (uint32_t)(8000000.0/(rpm_scaler * (float)(new_rpm < 10 ? 10:new_rpm)));
  //tmp = (uint32_t)(8000000/(Wheels[config.wheel].rpm_scaler * (new_rpm < 10 ? 10:new_rpm)));
  //uint64_t x = 800000000000ULL;

  get_prescaler_bits(&tmp,&tmp_prescaler_bits,&bitshift);

  new_OCR1A = (uint16_t)(tmp >> bitshift); 
  prescaler_bits = tmp_prescaler_bits;
  reset_prescaler = true; 
}


uint8_t get_bitshift_from_prescaler(uint8_t *prescaler_bits)
{
  switch (*prescaler_bits)
  {
    case PRESCALE_1024:
    return 10;
    case PRESCALE_256:
    return 8;
    case PRESCALE_64:
    return 6;
    case PRESCALE_8:
    return 3;
    case PRESCALE_1:
    return 0;
  }
  return 0;
}

//! Gets prescaler enum and bitshift based on OC value
void get_prescaler_bits(uint32_t *potential_oc_value, uint8_t *prescaler, uint8_t *bitshift)
{
  if (*potential_oc_value >= 16777216)
  {
    *prescaler = PRESCALE_1024;
    *bitshift = 10;
  }
  else if (*potential_oc_value >= 4194304)
  {
    *prescaler = PRESCALE_256;
    *bitshift = 8;
  }
  else if (*potential_oc_value >= 524288)
  {
    *prescaler = PRESCALE_64;
    *bitshift = 6;
  }
  else if (*potential_oc_value >= 65536)
  {
    *prescaler = PRESCALE_8;
    *bitshift = 3;
  }
  else
  {
    *prescaler = PRESCALE_1;
    *bitshift = 0;
  }
}
