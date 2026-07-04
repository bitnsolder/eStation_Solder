/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "debug.h"
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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define THERMOCOUPLE_Pin GPIO_PIN_4
#define THERMOCOUPLE_GPIO_Port GPIOC
#define TFT_DC_Pin GPIO_PIN_2
#define TFT_DC_GPIO_Port GPIOB
#define TFT_CS_Pin GPIO_PIN_10
#define TFT_CS_GPIO_Port GPIOB
#define TFT_BL_Pin GPIO_PIN_11
#define TFT_BL_GPIO_Port GPIOB
#define TFT_RST_X_Pin GPIO_PIN_12
#define TFT_RST_X_GPIO_Port GPIOB
#define TFT_SCL_Pin GPIO_PIN_13
#define TFT_SCL_GPIO_Port GPIOB
#define VBUS_Pin GPIO_PIN_14
#define VBUS_GPIO_Port GPIOB
#define TFT_SDA_Pin GPIO_PIN_15
#define TFT_SDA_GPIO_Port GPIOB
#define LED_Pin GPIO_PIN_6
#define LED_GPIO_Port GPIOC
#define HEATER_Pin GPIO_PIN_8
#define HEATER_GPIO_Port GPIOA
#define I2C_SCL_Pin GPIO_PIN_15
#define I2C_SCL_GPIO_Port GPIOA
#define TFT_RST_Pin GPIO_PIN_10
#define TFT_RST_GPIO_Port GPIOC
#define STAND_INP_Pin GPIO_PIN_11
#define STAND_INP_GPIO_Port GPIOC
#define ROTARY_SW_Pin GPIO_PIN_3
#define ROTARY_SW_GPIO_Port GPIOB
#define ROTARY_CLK_Pin GPIO_PIN_4
#define ROTARY_CLK_GPIO_Port GPIOB
#define ROTARY_DT_Pin GPIO_PIN_5
#define ROTARY_DT_GPIO_Port GPIOB
#define TX_Pin GPIO_PIN_6
#define TX_GPIO_Port GPIOB
#define RX_Pin GPIO_PIN_7
#define RX_GPIO_Port GPIOB
#define I2C_SDA_Pin GPIO_PIN_9
#define I2C_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
