/**
  ******************************************************************************
  * File Name          : LTDC.c
  * Description        : This file provides code for the configuration
  *                      of the LTDC instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ltdc.h"

/* USER CODE BEGIN 0 */

#include "rgb.h"

#include "FontProblems/bold.h"
#include "FontProblems/normal.h"

extern struct terminal_config terminal_config;

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

static const struct bitmap_font normal_bitmap_font = {
    .height = FONT_HEIGHT,
    .width = FONT_WIDTH,
    .data = normal_font_data,
    .codepoints_length = sizeof(normal_font_codepoints) / sizeof(int),
    .codepoints = normal_font_codepoints,
    .codepoints_map = normal_font_codepoints_map,
};

static const struct bitmap_font bold_bitmap_font = {
    .height = FONT_HEIGHT,
    .width = FONT_WIDTH,
    .data = bold_font_data,
    .codepoints_length = sizeof(bold_font_codepoints) / sizeof(int),
    .codepoints = bold_font_codepoints,
    .codepoints_map = bold_font_codepoints_map,
};

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

 __attribute__((section(".dma"))) static uint8_t screen_buffer[CHAR_WIDTH * CHAR_HEIGHT * 80 * 30];

#define SCREEN_BUFFER (uint32_t)&screen_buffer

static struct screen screen_24_rows = {
    .format = {.rows = 24, .cols = 80},
    .char_width = CHAR_WIDTH,
    .char_height = CHAR_HEIGHT,
    .buffer = (uint8_t *)SCREEN_BUFFER,
    .normal_bitmap_font = &normal_bitmap_font,
    .bold_bitmap_font = &bold_bitmap_font,
};

static struct screen screen_30_rows = {
    .format = {.rows = 30, .cols = 80},
    .char_width = CHAR_WIDTH,
    .char_height = CHAR_HEIGHT,
    .buffer = (uint8_t *)SCREEN_BUFFER,
    .normal_bitmap_font = &normal_bitmap_font,
    .bold_bitmap_font = &bold_bitmap_font,
};

struct screen *ltdc_get_screen(struct format format) {
  if (format.cols == 80) {
    if (format.rows == 24)
      return &screen_24_rows;
    else if (format.rows == 30)
      return &screen_30_rows;
  }

  return NULL;
}

static uint8_t reverse_byte(uint8_t byte) { 
  uint8_t reversed_byte = 0; 
  for (size_t i = 0; i < 8; i++)
    if((byte & (1 << i))) 
      reversed_byte |= 1 << (7 - i);   
  return reversed_byte;
} 

/* USER CODE END 0 */

LTDC_HandleTypeDef hltdc;

/* LTDC init function */
void MX_LTDC_Init(void)
{
  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 95;
  hltdc.Init.VerticalSync = 1;
  hltdc.Init.AccumulatedHBP = 143;
  hltdc.Init.AccumulatedVBP = 34;
  hltdc.Init.AccumulatedActiveW = 783;
  hltdc.Init.AccumulatedActiveH = 514;
  hltdc.Init.TotalWidth = 799;
  hltdc.Init.TotalHeigh = 524;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 640;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_L8;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = SCREEN_BUFFER;
  pLayerCfg.ImageWidth = 640;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  switch (terminal_config.format_rows) {
  case FORMAT_24_ROWS:
    pLayerCfg.WindowY0 = 48;
    pLayerCfg.WindowY1 = 432;
    pLayerCfg.ImageHeight = 384;
    break;
  case FORMAT_30_ROWS:
    pLayerCfg.WindowY0 = 0;
    pLayerCfg.WindowY1 = 480;
    pLayerCfg.ImageHeight = 480;
    break;
  }
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }

  // Fix CLUT for 3.0 board that has RGB2-7 pins reversed
  uint32_t rgb_table_fixed[RGB_TABLE_SIZE];

  for (size_t i = 0; i < RGB_TABLE_SIZE; ++i) {
    uint32_t entry = rgb_table[i];
    rgb_table_fixed[i] = reverse_byte((entry & 0xff) >> 2) |
      (reverse_byte(((entry >> 8) & 0xff) >> 2) << 8) |
      (reverse_byte(((entry >> 16) & 0xff) >> 2) << 16);
  }

  if (HAL_LTDC_ConfigCLUT(&hltdc, (uint32_t *)rgb_table_fixed, RGB_TABLE_SIZE, 0) !=
      HAL_OK) {
    Error_Handler();
  }

  if (HAL_LTDC_EnableCLUT(&hltdc, 0) != HAL_OK) {
    Error_Handler();
  }
}

void HAL_LTDC_MspInit(LTDC_HandleTypeDef* ltdcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspInit 0 */

  /* USER CODE END LTDC_MspInit 0 */
    /* LTDC clock enable */
    __HAL_RCC_LTDC_CLK_ENABLE();
  
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**LTDC GPIO Configuration    
    PC0     ------> LTDC_R5
    PA1     ------> LTDC_R2
    PA3     ------> LTDC_B5
    PA4     ------> LTDC_VSYNC
    PA5     ------> LTDC_R4
    PA6     ------> LTDC_G2
    PB0     ------> LTDC_R3
    PB1     ------> LTDC_R6
    PE11     ------> LTDC_G3
    PE12     ------> LTDC_B4
    PE13     ------> LTDC_DE
    PE14     ------> LTDC_CLK
    PE15     ------> LTDC_R7
    PB10     ------> LTDC_G4
    PB11     ------> LTDC_G5
    PD10     ------> LTDC_B3
    PC6     ------> LTDC_HSYNC
    PC7     ------> LTDC_G6
    PD3     ------> LTDC_G7
    PD6     ------> LTDC_B2
    PB8     ------> LTDC_B6
    PB9     ------> LTDC_B7 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5 
                          |GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF9_LTDC;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14 
                          |GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN LTDC_MspInit 1 */

  /* USER CODE END LTDC_MspInit 1 */
  }
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef* ltdcHandle)
{

  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspDeInit 0 */

  /* USER CODE END LTDC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LTDC_CLK_DISABLE();
  
    /**LTDC GPIO Configuration    
    PC0     ------> LTDC_R5
    PA1     ------> LTDC_R2
    PA3     ------> LTDC_B5
    PA4     ------> LTDC_VSYNC
    PA5     ------> LTDC_R4
    PA6     ------> LTDC_G2
    PB0     ------> LTDC_R3
    PB1     ------> LTDC_R6
    PE11     ------> LTDC_G3
    PE12     ------> LTDC_B4
    PE13     ------> LTDC_DE
    PE14     ------> LTDC_CLK
    PE15     ------> LTDC_R7
    PB10     ------> LTDC_G4
    PB11     ------> LTDC_G5
    PD10     ------> LTDC_B3
    PC6     ------> LTDC_HSYNC
    PC7     ------> LTDC_G6
    PD3     ------> LTDC_G7
    PD6     ------> LTDC_B2
    PB8     ------> LTDC_B6
    PB9     ------> LTDC_B7 
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0|GPIO_PIN_6|GPIO_PIN_7);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5 
                          |GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10|GPIO_PIN_11 
                          |GPIO_PIN_8|GPIO_PIN_9);

    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14 
                          |GPIO_PIN_15);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_6);

  /* USER CODE BEGIN LTDC_MspDeInit 1 */

  /* USER CODE END LTDC_MspDeInit 1 */
  }
} 

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
