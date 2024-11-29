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

## Bluetooth Player
* Bluethooth music player. Software acts as A2DP sink and works as bluetooth audio output device.
* Only SBC codec is supported.
* Title and artist name displayed on the screen.

## Oscilloscope Music
* Plays [Oscilloscpe Music](https://oscilloscopemusic.com/) .WAV sound files on the SD card and shows its visual effects on the LCD.
* This is very simple implementation. It simply plots upper 8bit PCM values as (X,Y) coordinates, but still you can enjoy visual effects.
* You need to purchase necessary WAV files and store those files on the SD card.

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
## Prepare SD card

To run LVGLPlayer firmware, you need a SD card which contains all necessary files. To prepare this SD card, follow below steps.

1. unzip sdcard.zip and copy all contents into your SD card.
2. Obtain Doom WAD file(s) and copy to under FlashData dir.
3. Download SC55 music packs and copy flac files under doom1-music, doom2-music and tnt-music directory.
4. Prepare ttf font file which is used to display music title and artist name in Bluetooth Player. Then rename its file name as 'AlbumFont.ttf' and copy it to under FlashData directory.
