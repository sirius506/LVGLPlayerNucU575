# LVGL Player for Nucleo-U575ZI-Q

This is LVGL Player project for STMicro Nucleo-U575ZI-Q board with extension board.

## Hardware
The extension board contains following parts.

* TI TLV320DAC3203 for Audio output.
* Micro SD card slot.
* BT860-ST Bluetooth Module.
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
* Sreenshot by User button on the Nucleo board. Screen images are saved on the SD card.

## Doom Player
* Supports Bluetooth connected game controllers -- DUALSHOCK4, DualSense and 8BitDo Zero 2.
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the playing.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.

## Bluetooth Player
* Bluethooth music player. Software acts as A2DP sink and works as bluetooth audio output device.

## Oscilloscope Music
* Plays Oscilloscpe Music .WAV sound files and shows its visual effects on the LCD.

Please refer [Wiki pages](https://github.com/sirius506/DoomPlayerNucU575/wiki) for more descriptions and screen shot images.
