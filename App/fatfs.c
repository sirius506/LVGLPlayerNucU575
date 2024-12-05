/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "DoomPlayer.h"
#include "fatfs.h"
#include "doomtype.h"
/* USER CODE END Header */

/* USER CODE BEGIN Variables */
uint8_t retSD;    /* Return value for SD */
char SDPath[4];   /* SD logical drive path */

FATFS SDFatFS;    /* File system object for SD logical drive */
FIL SDFile;       /* File object for SD */
FILINFO SDInfo;
FIL MusicFile;
FIL RGBFile;

SEMAPHORE_DEF(sem_sdfile)

osSemaphoreId_t sdfile_semId;

/* USER CODE END Variables */
int list_dir(const char *path)
{
  FRESULT res;
  DIR dir;
  FILINFO fno;
  int nfile, ndir;

  res = f_opendir(&dir, path);
  if (res == FR_OK) {
    nfile = ndir = 0;
    for (;;) {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == 0) break;
      if (fno.fattrib & AM_DIR) {
        debug_printf("  <DIR> %s\n", fno.fname);
        ndir++;
      } else {
        debug_printf("%10u %s\n", fno.fsize, fno.fname);
        nfile++;
      }
    }
    //closedir(&dir);
    debug_printf("%d dirs, %d files.\n", ndir, nfile);
  } else {
    debug_printf("Failed to open %s (%u)\n", path, res);
  }
  return res;
}

void MX_FATFS_Init(void)
{
  MX_SDMMC1_SD_Init();

  /*## FatFS: Link the SD driver ###########################*/
  retSD = FATFS_LinkDriver(&SD_Driver, SDPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  retSD = f_mount(&SDFatFS, (TCHAR const*)SDPath, 0);

  sdfile_semId = osSemaphoreNew(1, 1, &attributes_sem_sdfile);

  //list_dir("/");
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  extern RTC_HandleTypeDef hrtc;
  RTC_TimeTypeDef cTime;
  RTC_DateTypeDef cDate;
  DWORD val;

  HAL_RTC_GetTime(&hrtc, &cTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &cDate, RTC_FORMAT_BIN);

  val = (cTime.Seconds /2) | (cTime.Minutes << 5) | (cTime.Hours << 11) |
		(cDate.Date << 16) | (cDate.Month << 21) | ((cDate.Year + 20) << 25);
  return val;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

FIL *OpenFATFile(char *name)
{
  FRESULT res;

  osSemaphoreAcquire(sdfile_semId, osWaitForever);

  res = f_open(&SDFile, name, FA_READ);
  if (res != FR_OK)
  {
    osSemaphoreRelease(sdfile_semId);
    return NULL;
  }
  return &SDFile;
}

FIL *CreateFATFile(char *name)
{
  FRESULT res;

  osSemaphoreAcquire(sdfile_semId, osWaitForever);

  res = f_open(&SDFile, name, FA_CREATE_ALWAYS|FA_WRITE);
  if (res != FR_OK)
  {
    debug_printf("Create Failed: %d\n", res);
    osSemaphoreRelease(sdfile_semId);
    return NULL;
  }
  return &SDFile;
}

void CloseFATFile(FIL *pfile)
{
  f_close(pfile);
  osSemaphoreRelease(sdfile_semId);
}

FIL *OpenMusicFile(char *name)
{
  FRESULT res;

  res = f_open(&MusicFile, name, FA_READ);
  if (res != FR_OK)
  {
    return NULL;
  }
  return &MusicFile;
}

void CloseMusicFile(FIL *pfile)
{
  f_close(pfile);
}

FIL *CreateRGBFile(char *name)
{
  FRESULT res;

  res = f_open(&RGBFile, name, FA_CREATE_ALWAYS|FA_WRITE);
  if (res != FR_OK)
  {
    return NULL;
  }
  return &RGBFile;
}

void CloseRGBFile(FIL *pfile)
{
  f_close(pfile);
}

boolean M_SDFileExists(char *filename)
{
  FRESULT res;

  res = f_stat(filename, &SDInfo);
  if (res == FR_OK)
  {
    return true;
  }
  return false;
}

FRESULT M_SDMakeDirectory(char *dirname)
{
  FRESULT res;

  res = f_mkdir(dirname);
  return res;
}

/* USER CODE END Application */
