#ifndef BOARD_IF_H
#define BOARD_IF_H
#include "lvgl.h"

typedef enum {
  DOOM_SCREEN_INACTIVE,
  DOOM_SCREEN_ACTIVE,
  DOOM_SCREEN_SUSPEND,
  DOOM_SCREEN_SUSPENDED,
} DOOM_SCREEN_STATUS;

int Board_FlashInfo(HAL_DEVICE *haldev);
int Board_PSRAMInfo(HAL_DEVICE *haldev);
int Board_Flash_Init(HAL_DEVICE *haldev, int mapmode);
int Board_PSRAM_Init(HAL_DEVICE *haldev);
int Board_EraseSectorSize();
void Board_DoomModeLCD(int expand);
void Board_Erase_Block(HAL_DEVICE *haldev, uint32_t baddr);
void Board_Flash_ReInit(int mapmode);
int Board_Flash_Write(HAL_DEVICE *haldev, uint8_t *bp, uint32_t baddr, int len);
void Board_Endoom(uint8_t *bp);
lv_image_dsc_t *GetGameImageDesc();
extern volatile DOOM_SCREEN_STATUS DoomScreenStatus;
void Board_Set_Brightness(HAL_DEVICE *haldev, int brval);
int Board_Get_Brightness();

void Board_Audio_ClockConfig(HAL_DEVICE *haldev, int sample_rate);
void Board_Audio_Start(HAL_DEVICE *haldev, uint8_t *bp, int len);
void Board_Audio_DeInit(HAL_DEVICE *haldev);
void Board_Audio_Init(HAL_DEVICE *haldev,  int sampleRate);
void Board_Audio_Pause(HAL_DEVICE *haldev);
void Board_Audio_Resume(HAL_DEVICE *haldev);
void Board_Audio_Stop(HAL_DEVICE *haldev);

#endif
