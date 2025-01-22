# LVGL Player for Nucleo-U575ZI-Q

This is LVGL Player project for STMicro Nucleo-U575ZI-Q board with extension board.

## Hardware
The [extension board](https://github.com/sirius506/LVGLPlayerNucU575/blob/main/Schematic.pdf) contains following parts.

* TI TLV320DAC3203 for Audio output.
* Micro SD card slot.
* BT860-SA Bluetooth Module.
* ER-TFM035-6. 480x320 Color LCD with capacitive touch panel.
* W25Q256. 256Mbit QSPI Flash
* APS6404L-3SQR. 64Mbit QSPI PSRAM
* Four color LEDs (Red, Yellow, Green and Blue)

Although hardware allows to use DAC and/or SAI for audio output
interface, current software supports SAI only.
Please solder between pin 1 and 2 of JP3 and JP4.

When coin cell battery is installed, it backups RTC and allows to record
date/time stamp on the screen capture file.
When battery is connected, make sure to remove
SB50 jumper on the Nucleo board.

## Features

LVGL Player supports three applications and you can select one of the
application on the initial startup screen.

![select_screen](https://github.com/user-attachments/assets/f4b2d042-1b5c-420f-8219-2e2b0eccfac1)

1. DoomPlayer -- Play DOOM music, sounds and games.
2. Bluetooth Player -- Act as bluetooth A2DP sink client.
3. Osilloscope Music -- Play and display [Oscilloscope music](https://oscilloscopemusic.com/) files.

* [LVGL](https://github.com/lvgl/lvgl) based GUI operation.
* Sreenshot by User button (blue button) on the Nucleo board. Screen images are saved on the SD card.
* Supports Bluetooth connected game controllers -- DUALSHOCK4, DualSense and 8BitDo Zero 2.

## Doom Player
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the game playing.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.
* Game music files must be downloaded from [this site](http://sc55.duke4.net/games.php#doom). Please note that you need to download 'FLA Pack' files.

![DoomPlayerStart](https://github.com/user-attachments/assets/fbc68a92-5db6-420b-83eb-81d23e44db99)

![DoomPlayerMenu](https://github.com/user-attachments/assets/7ce2ef10-5a91-48d1-a33a-d13c271e0287)

![DoomPlayerMusic](https://github.com/user-attachments/assets/64c4d27d-d4fb-4f43-9cf0-52608cd5dbc8)

![DoomPlayerSound](https://github.com/user-attachments/assets/77bc90bd-78f0-4b23-834f-7d82ef2413ee)

If gamepad is not connected when DOOM has started, control buttons are displayed on the LCD screen. But it is not easy to play the game, it is strongly recommented to use gamepad.

![doom_small](https://github.com/user-attachments/assets/89af701a-7601-403e-8a4b-07e58540d4ff)

While game play is on-going, touch center of LCD screen to bring cheat mode buttons.

![doom_cheat](https://github.com/user-attachments/assets/338aee16-52bb-43d7-bdb6-4229785b9fa0)

![DoomPlayerGame1](https://github.com/user-attachments/assets/35791cc3-d0b3-4a01-bd18-f9dfeedd7269)

![DoomPlayerGame2](https://github.com/user-attachments/assets/4dcf60e3-f8c0-452a-b048-6c33286816d8)

## Bluetooth Player
* Bluethooth music player. Software acts as A2DP sink and works as bluetooth audio output device.
* Only SBC codec is supported.
* Title and artist name displayed on the screen.

![IMG240122134504](https://github.com/user-attachments/assets/b1bf8205-1930-4cf9-be88-4e7d4c5a33e7)

![IMG240122093558](https://github.com/user-attachments/assets/ea9c3550-ccec-4fbd-b9d5-01e10dbf74c1)

![IMG240122140244](https://github.com/user-attachments/assets/034bd091-fd5c-472d-9acb-d32da9b01438)

## Oscilloscope Music
* Plays [Oscilloscpe Music](https://oscilloscopemusic.com/) .WAV sound files on the SD card and shows its visual effects on the LCD.
* This is very simple implementation. It simply plots upper 8bit PCM values as (X,Y) coordinates, but still you can enjoy visual effects.
* You need to purchase necessary WAV files and store those files on the SD card.

![OscCore2](https://github.com/user-attachments/assets/bf867100-0e9a-4036-8a14-bad15a9174f5)


![OscCore3](https://github.com/user-attachments/assets/20ee7a28-dc10-43ab-a09e-eafb821838f7)
![OscCore5](https://github.com/user-attachments/assets/b9fbd95b-1320-4528-976e-b0973c33d476)

![OscFunction2](https://github.com/user-attachments/assets/eb4fbffb-53fc-4739-96db-92271637e641)
![OscFunction3](https://github.com/user-attachments/assets/ed0e3b06-f57a-4f11-9224-c3ab04a117b3)

![OscFunction4](https://github.com/user-attachments/assets/6a94c06f-ea43-49fb-a7c9-64167109fc55)
![OscFunction6](https://github.com/user-attachments/assets/6e48bbb2-19b7-4b59-9eae-e977542a0b0b)

Please refer [Wiki pages](https://github.com/sirius506/DoomPlayerNucU575/wiki) for more descriptions and screen shot images.

## Build

To build LVGLPlayer, clone this repo and follow normal cmake build steps shown below.

```
% git clone --recursive https://github.com/sirius506/LVGLPlayerNucU575.git
% cd LVGLPlayerNucU575
% mkdir build
% cd build
% cmake ..
% make -j
```
## Get Started

### Prepare SD card

To run LVGLPlayer firmware, you need a SD card which contains all necessary files. To prepare this SD card, follow below steps.

1. unzip [sdcard.zip](sdcard.zip) and copy all contents into your SD card.
2. Obtain Doom WAD file(s) and copy to under FlashData dir.
3. Download SC55 music packs and copy flac files under doom1-music, doom2-music and tnt-music directory.
4. To play 'Oscilloscope Music', you need to buy and download WAV files under OscMusic and/or N-SPHERES.

See also: [this page](https://github.com/sirius506/LVGLPlayerNucU575/wiki/SD-Card)

### Install

1. Build or download LVGLPlayerNucU575.elf from latest [relase](https://github.com/sirius506/LVGLPlayerNucU575/releases).
2. Write elf binary into Nucleo-U575.
3. Insert SD card and supply power. Be sure to have at least one WAD file under FlashData directory.
4. On the application select screen. Press 'Doom Player' button.
5. Initially, nothing is written on the QSPI Flash. You need to copy WAD and other config files from SD card to QSPI Flash. Please refer [this wiki page](https://github.com/sirius506/LVGLPlayerNucU575/wiki/Initial-Flash-Copy).
