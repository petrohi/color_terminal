/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "ltdc.h"
#include "tim.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>

#include "terminal.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MANDELBROT_X -0.7453
#define MANDELBROT_Y 0.1127
#define MANDELBROT_R 6.5e-4

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/*
char rx_buffer[256];
char buffer[256];
*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len) {
  for (int i = 0; i < len; i++) {
    ITM_SendChar(*ptr++);
  }
  return len;
}

/*
static bool cancel_mandelbrot() {
  MX_USB_HOST_Process();
  HID_KEYBD_Info_TypeDef *info = 0; //MX_USBH_HID_KeyboardDecode();
  return (info && info->keys[0] == KEY_ESCAPE);
}

static void test_mandelbrot() {

  static float window_x = MANDELBROT_X;
  static float window_y = MANDELBROT_Y;
  static float window_r = MANDELBROT_R;

  static bool render = true;

  HID_KEYBD_Info_TypeDef *info = 0; //MX_USBH_HID_KeyboardDecode();

  if (info) {
    switch (info->keys[0]) {
    case KEY_KEYPAD_8_UP_ARROW:
      window_y -= (window_r / 4.0);
      render = true;
      break;

    case KEY_KEYPAD_2_DOWN_ARROW:
      window_y += (window_r / 4.0);
      render = true;
      break;

    case KEY_KEYPAD_4_LEFT_ARROW:
      window_x -= (window_r / 4.0);
      render = true;
      break;

    case KEY_KEYPAD_6_RIGHT_ARROW:
      window_x += (window_r / 4.0);
      render = true;
      break;

    case KEY_KEYPAD_PLUS:
      window_r *= 1.25;
      render = true;
      break;

    case KEY_KEYPAD_MINUS:
      window_r *= 0.75;
      render = true;
      break;
    }
  }

  if (render) {
    screen_test_mandelbrot(ltdc_get_screen(), window_x, window_y, window_r,
                   cancel_mandelbrot);

    render = false;

    size_t size =
        snprintf(buffer, sizeof(buffer), "Rendered x=%e y=%e r=%e\r\n",
                 window_x, window_y, window_r);
    uart_transmit((uint8_t *)buffer, size);
  }
}
*/

static void screen_draw_character_callback(size_t row, size_t col,
                                           character_t character,
                                           enum font font, bool italic,
                                           bool underlined, bool crossedout,
                                           color_t active, color_t inactive) {
  screen_draw_character(ltdc_get_screen(), row, col, character, font, italic,
                        underlined, crossedout, active, inactive);
}

static void screen_clear_rows_callback(size_t from_row, size_t to_row,
                                       color_t inactive) {
  screen_clear_rows(ltdc_get_screen(), from_row, to_row, inactive);
}

static void screen_clear_cols_callback(size_t row, size_t from_col,
                                       size_t to_col, color_t inactive) {
  screen_clear_cols(ltdc_get_screen(), row, from_col, to_col, inactive);
}

static void screen_scroll_callback(enum scroll scroll, size_t from_row,
                                   size_t to_row, size_t rows,
                                   color_t inactive) {
  screen_scroll(ltdc_get_screen(), scroll, from_row, to_row, rows, inactive);
}

static void screen_shift_characters_right_callback(size_t row, size_t col,
                                                   size_t cols,
                                                   color_t inactive) {
  screen_shift_characters_right(ltdc_get_screen(), row, col, cols, inactive);
}

static void screen_shift_characters_left_callback(size_t row, size_t col,
                                                  size_t cols,
                                                  color_t inactive) {
  screen_shift_characters_left(ltdc_get_screen(), row, col, cols, inactive);
}

struct terminal *global_terminal;

#define TRANSMIT_BUFFER_SIZE 64
#define RECEIVE_BUFFER_SIZE 1024 * 2

static character_t transmit_buffer[TRANSMIT_BUFFER_SIZE];
static character_t receive_buffer[RECEIVE_BUFFER_SIZE];

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LTDC_Init();
  MX_USART1_UART_Init();
  MX_USB_HOST_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  printf("Hello World\n");

  struct terminal terminal;
  struct terminal_callbacks callbacks = {
      .keyboard_set_leds = keyboard_set_leds,
      .uart_transmit = uart_transmit,
      .uart_receive = uart_receive,
      .screen_draw_character = screen_draw_character_callback,
      .screen_clear_rows = screen_clear_rows_callback,
      .screen_clear_cols = screen_clear_cols_callback,
      .screen_scroll = screen_scroll_callback,
      .screen_shift_characters_left = screen_shift_characters_left_callback,
      .screen_shift_characters_right = screen_shift_characters_right_callback,
      .system_reset = HAL_NVIC_SystemReset};
  terminal_init(&terminal, &callbacks, transmit_buffer, TRANSMIT_BUFFER_SIZE,
                receive_buffer, RECEIVE_BUFFER_SIZE);
  global_terminal = &terminal;
  start_timer();

  // screen_test_colors(ltdc_get_screen());
  // screen_scroll(ltdc_get_screen(), UP, 2, ROWS - 2, 2, 0);
  // screen_scroll(ltdc_get_screen(), DOWN, 2, ROWS - 2, 2, 0);
  // screen_test_fonts(ltdc_get_screen());
  // screen_clear(ltdc_get_screen(), 15);
  // screen_clear(ltdc_get_screen(), 0);

  //Receive((uint8_t *)rx_buffer, sizeof(rx_buffer));
  //int count = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    //int new_count = (int)ReceiveByteCount();

    //if (new_count != count) {
    //  count = new_count;
    //  printf("Receive byte count %d\n", count);
    //}

    //test_mandelbrot();

    keyboard_handle(&terminal);
    terminal_uart_receive(&terminal, uart_receive_count());
    terminal_screen_update(&terminal);
    terminal_keyboard_repeat_key(&terminal);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 50;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
