/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#define USE_HAL_USER_PTR
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_SDMMC1_SD_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PWM_PRESCALER ((160000000/50000)-1)
#define LED_PWM_PRESCALER ((160000000/20000)-1)
#define DAC_TRIGGER_PERIOD ((160000000/44100)-1)
#define PWM_RED_Pin GPIO_PIN_3
#define PWM_RED_GPIO_Port GPIOE
#define PWM_YELLOW_Pin GPIO_PIN_4
#define PWM_YELLOW_GPIO_Port GPIOE
#define PWM_GREEN_Pin GPIO_PIN_5
#define PWM_GREEN_GPIO_Port GPIOE
#define PWM_BLUE_Pin GPIO_PIN_6
#define PWM_BLUE_GPIO_Port GPIOE
#define USER_BUTTON_Pin GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC
#define USER_BUTTON_EXTI_IRQn EXTI13_IRQn
#define TLV_SDA_Pin GPIO_PIN_0
#define TLV_SDA_GPIO_Port GPIOF
#define TLV_SCL_Pin GPIO_PIN_1
#define TLV_SCL_GPIO_Port GPIOF
#define SDCARD_DET_Pin GPIO_PIN_7
#define SDCARD_DET_GPIO_Port GPIOF
#define BL_CTRL_Pin GPIO_PIN_9
#define BL_CTRL_GPIO_Port GPIOF
#define VBUS_SENSE_Pin GPIO_PIN_2
#define VBUS_SENSE_GPIO_Port GPIOC
#define BT_RTS_Pin GPIO_PIN_0
#define BT_RTS_GPIO_Port GPIOA
#define BT_CTS_Pin GPIO_PIN_1
#define BT_CTS_GPIO_Port GPIOA
#define BT_RXD_Pin GPIO_PIN_2
#define BT_RXD_GPIO_Port GPIOA
#define BT_TXD_Pin GPIO_PIN_3
#define BT_TXD_GPIO_Port GPIOA
#define AUDIO_LEFT_Pin GPIO_PIN_4
#define AUDIO_LEFT_GPIO_Port GPIOA
#define AUDIO_LEFTA5_Pin GPIO_PIN_5
#define AUDIO_LEFTA5_GPIO_Port GPIOA
#define LCD_D4_Pin GPIO_PIN_7
#define LCD_D4_GPIO_Port GPIOE
#define LCD_D5_Pin GPIO_PIN_8
#define LCD_D5_GPIO_Port GPIOE
#define LCD_D6_Pin GPIO_PIN_9
#define LCD_D6_GPIO_Port GPIOE
#define LCD_D7_Pin GPIO_PIN_10
#define LCD_D7_GPIO_Port GPIOE
#define LCD_D8_Pin GPIO_PIN_11
#define LCD_D8_GPIO_Port GPIOE
#define LCD_D9_Pin GPIO_PIN_12
#define LCD_D9_GPIO_Port GPIOE
#define LCD_D10_Pin GPIO_PIN_13
#define LCD_D10_GPIO_Port GPIOE
#define LCD_D11_Pin GPIO_PIN_14
#define LCD_D11_GPIO_Port GPIOE
#define LCD_D12_Pin GPIO_PIN_15
#define LCD_D12_GPIO_Port GPIOE
#define UCPD_FLT_Pin GPIO_PIN_14
#define UCPD_FLT_GPIO_Port GPIOB
#define UCPD1_CC2_Pin GPIO_PIN_15
#define UCPD1_CC2_GPIO_Port GPIOB
#define LCD_D13_Pin GPIO_PIN_8
#define LCD_D13_GPIO_Port GPIOD
#define LCD_D14_Pin GPIO_PIN_9
#define LCD_D14_GPIO_Port GPIOD
#define LCD_D15_Pin GPIO_PIN_10
#define LCD_D15_GPIO_Port GPIOD
#define LCD_DC_Pin GPIO_PIN_13
#define LCD_DC_GPIO_Port GPIOD
#define LCD_D0_Pin GPIO_PIN_14
#define LCD_D0_GPIO_Port GPIOD
#define LCD_D1_Pin GPIO_PIN_15
#define LCD_D1_GPIO_Port GPIOD
#define LED_RED_Pin GPIO_PIN_2
#define LED_RED_GPIO_Port GPIOG
#define BT_RESET_Pin GPIO_PIN_6
#define BT_RESET_GPIO_Port GPIOC
#define LED_GREEN_Pin GPIO_PIN_7
#define LED_GREEN_GPIO_Port GPIOC
#define VCP_TX_Pin GPIO_PIN_9
#define VCP_TX_GPIO_Port GPIOA
#define VCP_RX_Pin GPIO_PIN_10
#define VCP_RX_GPIO_Port GPIOA
#define USB_OTG_FS_DM_Pin GPIO_PIN_11
#define USB_OTG_FS_DM_GPIO_Port GPIOA
#define USB_OTG_FS_DP_Pin GPIO_PIN_12
#define USB_OTG_FS_DP_GPIO_Port GPIOA
#define UCPD1_CC1_Pin GPIO_PIN_15
#define UCPD1_CC1_GPIO_Port GPIOA
#define LCD_D2_Pin GPIO_PIN_0
#define LCD_D2_GPIO_Port GPIOD
#define LCD_D3_Pin GPIO_PIN_1
#define LCD_D3_GPIO_Port GPIOD
#define LCD_RD_Pin GPIO_PIN_4
#define LCD_RD_GPIO_Port GPIOD
#define LCD_WR_Pin GPIO_PIN_5
#define LCD_WR_GPIO_Port GPIOD
#define LCD_CS_Pin GPIO_PIN_7
#define LCD_CS_GPIO_Port GPIOD
#define TPC_SDA_Pin GPIO_PIN_13
#define TPC_SDA_GPIO_Port GPIOG
#define TPC_SCL_Pin GPIO_PIN_14
#define TPC_SCL_GPIO_Port GPIOG
#define CTP_INT_Pin GPIO_PIN_15
#define CTP_INT_GPIO_Port GPIOG
#define CTP_INT_EXTI_IRQn EXTI15_IRQn
#define UCPD_DBn_Pin GPIO_PIN_5
#define UCPD_DBn_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_7
#define LED_BLUE_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
