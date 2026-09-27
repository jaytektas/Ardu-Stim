<div align="center">

<img src="https://github.com/speeduino/wiki.js/raw/master/img/Speeduino%20logo_med.png" alt="Speeduino" width="600" />

[![Release](https://img.shields.io/github/release/speeduino/Ardu-Stim.svg)](https://github.com/speeduino/Ardu-Stim/releases/latest)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](https://github.com/speeduino/Ardu-Stim/blob/master/LICENSE)
[![https://img.shields.io/discord/879495735912071269 ](https://img.shields.io/discord/879495735912071269?label=Discord&logo=Discord)](https://discord.gg/YWCEexaNDe)

##### This is the Speeduino fork of the ardustim engine simulator.
</div>

## Changes in this fork (jaytek branch)

This is the jaytek fork of Speeduino's Ardu-Stim, modified by Jason Roughley in 2026 for the jayecu
bench. It is based on upstream `51f624a` (2025-02-23) and, like the original, is licensed under the
GNU GPL version 3 or later (see `LICENSE`). The original copyright notices are unchanged.

Changes from upstream:

- **Correct rpm on every wheel.** The rpm scaler is computed from each wheel's own edge count and
  degrees instead of being a hand-typed number per wheel; fourteen of the sixty-four hand-typed values
  were wrong, so those wheels ran at the wrong speed while reporting the rpm asked for.
- **`F` + rpm (u16, little-endian)** — run at a fixed rpm set over serial, overriding the pot.
- **`H` / `h` + `0x5A`** — halt and resume the wheel. Halt stops the pattern generator entirely and
  leaves the outputs at their idle level, so an ECU sees no teeth at all. The `0x5A` second byte stops a
  stray byte being taken as a halt.
- **Settings survive a reset.** Opening the serial port resets the board (DTR is wired to RESET), so the
  wheel, rpm, sweep and crank-curve settings are saved to EEPROM whenever a command changes them.
- **Crank curve: `V` (set) and `v` (report).** Models the speed swing of a cranking engine: amplitude
  in percent, compressions per revolution (in halves), and an rpm above which the wheel runs steady.
  The swing is centred on the requested rpm, and the mean is corrected so the wheel really completes
  the rpm asked for.

## Ardu-Stim

Ardustim is an engine simulator built on the Arduino platform. It produces simulated crank and cam signals that can be used for testing aftermarket ECUs as well as being a useful tool for the development of firmware for these. It supports a large number of simulated rtigger patterns as well as multiple options for output speed (Eg Using an external pot, a fixed value or a sweep range)

<div align="center"><img src="https://github.com/speeduino/Ardu-Stim/raw/master/docs/demo.gif" alt="Ardu-Stim Demo" width="80%" /></div>

This version is a fork of the original by David Andruczyk [https://gitlab.com/libreems-suite/ardu-stim](https://gitlab.com/libreems-suite/ardu-stim) and is intended to provide a more modern, cross platform GUI as well as continued expansion of the trigger pattern library. It was primarily developed for use by the Speeduino community, but can be utilised for testing virtually any aftermarket ECU system

It is designed to run on an Arduino Nano, but will also work with Arduino Uno and Mega boards. 

## Wiring

- **Arduino Nano or Uno**
  - pin `8` will provide the `crank` or primary wheel signal
  - pin `9` will provide the `cam` or secondary wheel signal
  - Pin `10` will provide a `2nd cam` or tertiary wheel signal. This is for simulating some dual cam patterns
- **Arduino Mega**
  - pin `53` will provide the `crank` or primary wheel signal
  - pin `52` will provide the `cam` or secondary wheel signal
  - Pin `51` will provide a `2nd cam` or tertiary wheel signal. This is for simulating some dual cam patterns

Example for `Arduino Nano` connected to `Speeduino v0.4` ECU:

<div align="center"><img src="https://github.com/speeduino/Ardu-Stim/raw/master/docs/nano-v0.4-wiring.png" alt="Ardu-Stim Wiring" width="80%" /></div>

### RPM Potentiometer

An optional potentiometer can be added to control the RPM value (With the relevant RPM mode selected). This should be connect to pin A0 if in use. 

## Installing and Using

Ardu-Stim is distributed as a ready-to-run binary for Windows, Mac (Intel and Arm) and linux (AppImage) so no installation is required. Simply down the latest release (https://github.com/speeduino/Ardu-Stim/releases/latest) and 

### First time Connection
The first time you connect Ardu-Stim to an Arduino Nano board, you need to upload the included firmware to it. Plug the Nano into your PC and then select the port from the list. Press the upload firmware button and wait for this to complete

<div align="center"><img src="https://github.com/speeduino/Ardu-Stim/raw/master/docs/upload-firmware.png" alt="Ardu-Stim Wiring" width="80%" /></div>

**Note:** This only needs to be performed with a new Arduino Nano or if upgrading from an earlier version. The automated uploading of firmware is ONLY available for Arduino Nano boards. If using another board you will need to manually compile and upload the firmware (Not the GUI)

## Firmware Build

Optionally, the firmware source code can be built in either PlatformIO or the Arduino IDE and does not have any dependencies on 3rd party libraries that were used in the original version of Ardustim (Eg SerialUI)

Simply open the `ardustim` sub-folder in PlatformIO or the Arduino IDE and it should compile without issue.

Intended hardware platform is the Arduino Nano or Uno.

## Installing GUI from Source

### Pre-Requisites

- NPM - https://www.npmjs.com/get-npm
- Python
- Git

### GUI Installation steps

```bash
$ git clone https://github.com/speeduino/Ardu-Stim.git
$ cd Ardu-Stim/UI
$
$ npm install electron-rebuild -g
$ npm install
$ npm start
```
