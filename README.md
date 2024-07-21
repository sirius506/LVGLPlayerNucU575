# Doom Player for Nucleo-U575ZI-Q

This is Doom Player project for STMicro Nucleo-U575ZI-Q board with extension board.

## Hardware
The extension board contains following parts.

* TI TLV320DAC3203 for Audio output.
* Micro SD card slot.
* BT860-ST Bluetooth Module.
* ER-TFM035-6. 480x320 Color LCD with touch panel.
* W25Q256. 256Mbit QSPI Flash
* APS6404L-3SQR. 64Mbit QSPI PSRAM
* Four color LEDs (Red, Yellow, Green, Blue)

Although hardware allows to use either to use DAC or
SAI for audio output, current software only supports SAI
interface connected to TLV320DAC3203. Please solder between pin 1 and 2 of
JP3 and JP4.

With battery allows to backup RTC and it allows to record date/time stamp
on the screen capture file. When battery is connected, make sure to remove
SB50 jumper on the Nucleo board.

## Features

* [LVGL](https://github.com/lvgl/lvgl) based GUI operation.
* Supports Bluetooth connected game controllers -- DUALSHOCK4, DualSense and 8BitDo Zero 2.
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the playing.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.
* Sreenshot by User button on the Nucleo board. Screen images are saved on the SD card.
* Bluethooth music player. If SD card is not inserted, software act as A2DP sink and act as bluetooth audio output device.

Please refer [Wiki pages](https://github.com/sirius506/DoomPlayerNucU575/wiki) for more descriptions and screen shot images.
