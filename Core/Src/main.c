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
#include "terminal_config_ui.h"

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

struct terminal *global_terminal;
struct terminal_config_ui *global_terminal_config_ui;

#define UART_TRANSMIT_BUFFER_SIZE 64
#define UART_RECEIVE_BUFFER_SIZE 1024 * 4

#define XOFF_LIMIT UART_RECEIVE_BUFFER_SIZE / 8
#define KEYBOARD_CHARS_PER_POLL 80

static void uart_transmit(character_t *characters, size_t size) {
  if (global_terminal_config_ui->activated) {
    terminal_config_ui_receive_characters(global_terminal_config_ui, characters,
                                       size);
  } else {
#ifdef DEBUG_LOG_RX_TX
    printf("TX: %d\r\n", size);
#endif
    while (HAL_UART_Transmit_DMA(&huart3, (void *)characters, size) != HAL_OK)
      ;
  }
}

static void screen_draw_codepoint_callback(size_t row, size_t col,
                                           codepoint_t codepoint,
                                           enum font font, bool italic,
                                           bool underlined, bool crossedout,
                                           color_t active, color_t inactive) {
  screen_draw_codepoint(ltdc_get_screen(), row, col, codepoint, font, italic,
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

static void screen_shift_right_callback(size_t row, size_t col, size_t cols,
                                        color_t inactive) {
  screen_shift_right(ltdc_get_screen(), row, col, cols, inactive);
}

static void screen_shift_left_callback(size_t row, size_t col, size_t cols,
                                       color_t inactive) {
  screen_shift_left(ltdc_get_screen(), row, col, cols, inactive);
}

static void system_test_callback(enum system_test system_test) {
  switch (system_test) {
  case SYSTEM_TEST_FONT1:
    screen_test_fonts(ltdc_get_screen(), FONT_NORMAL);
    break;
  case SYSTEM_TEST_FONT2:
    screen_test_fonts(ltdc_get_screen(), FONT_BOLD);
    break;
  case SYSTEM_TEST_COLOR1:
    screen_test_colors(ltdc_get_screen());
    break;
  case SYSTEM_TEST_COLOR2:
    screen_test_mandelbrot(ltdc_get_screen(), MANDELBROT_X, MANDELBROT_Y,
                           MANDELBROT_R, NULL);
    break;
  }
}

static void keyboard_set_leds(struct lock_state state) {
  uint8_t led_state =
      (state.scroll ? 0x4 : 0) | (state.caps ? 0x2 : 0) | (state.num ? 1 : 0);
  while (USBH_HID_SetReport(&hUsbHostHS, 0x02, 0x0, &led_state, 1) == USBH_BUSY)
    ;
}

static void keyboard_handle(struct terminal *terminal) {
  if (Appli_state == APPLICATION_READY) {

    HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(&hUsbHostHS);

    if (info) {
      if (info->keys[0] == KEY_F12 && (info->lshift || info->rshift)) {
        terminal_config_ui_enter(global_terminal_config_ui);
      } else {
        terminal_keyboard_handle_shift(terminal, info->lshift || info->rshift);
        terminal_keyboard_handle_alt(terminal, info->lalt || info->ralt);
        terminal_keyboard_handle_ctrl(terminal, info->lctrl || info->rctrl);
        terminal_keyboard_handle_key(terminal, info->keys[0]);
      }
    }
  }
}

static character_t uart_transmit_buffer[UART_TRANSMIT_BUFFER_SIZE];
static character_t uart_receive_buffer[UART_RECEIVE_BUFFER_SIZE];

__attribute__((
    __section__(".flash_data"))) struct terminal_config terminal_config = {
    .baud_rate = BAUD_RATE_115200,
    .word_length = WORD_LENGTH_8B,
    .word_length = WORD_LENGTH_8B,
    .stop_bits = STOP_BITS_1,
    .parity = PARITY_NONE,

    .charset = CHARSET_UTF8,
    .c1_mode = C1_MODE_7BIT,

    .auto_wrap_mode = true,
    .screen_mode = false,

    .send_receive_mode = true,

    .new_line_mode = false,
    .cursor_key_mode = false,
    .auto_repeat_mode = true,
    .ansi_mode = true,
    .backspace_mode = false,

    .start_up = START_UP_MESSAGE,
};

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU
   * Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the
   * Systick. */
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
  MX_USB_HOST_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  printf("Hello World\n");

  struct terminal terminal;
  struct terminal_callbacks callbacks = {
      .keyboard_set_leds = keyboard_set_leds,
      .uart_transmit = uart_transmit,
      .screen_draw_codepoint = screen_draw_codepoint_callback,
      .screen_clear_rows = screen_clear_rows_callback,
      .screen_clear_cols = screen_clear_cols_callback,
      .screen_scroll = screen_scroll_callback,
      .screen_shift_left = screen_shift_left_callback,
      .screen_shift_right = screen_shift_right_callback,
      .system_reset = HAL_NVIC_SystemReset,
      .system_test = system_test_callback};
  terminal_init(&terminal, &callbacks, &terminal_config, uart_transmit_buffer,
                UART_TRANSMIT_BUFFER_SIZE);
  global_terminal = &terminal;

  struct terminal_config_ui terminal_config_ui;
  global_terminal_config_ui = &terminal_config_ui;
  terminal_config_ui_init(&terminal_config_ui, &terminal, &terminal_config);

  HAL_TIM_Base_Start_IT(&htim1);
  while (HAL_UART_Receive_DMA(&huart3, uart_receive_buffer,
                              UART_RECEIVE_BUFFER_SIZE) != HAL_OK)
    ;

  uint16_t uart_receive_current_offset = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    keyboard_handle(&terminal);
    terminal_screen_update(&terminal);
    terminal_keyboard_repeat_key(&terminal);

    if (terminal_config_ui.activated)
      continue;

    uint16_t uart_receive_next_offset =
        UART_RECEIVE_BUFFER_SIZE - huart3.hdmarx->Instance->NDTR;
    if (uart_receive_current_offset == uart_receive_next_offset) {
      terminal_uart_xon_off(&terminal, XON);
      continue;
    }

    uint16_t size = 0;
    if (uart_receive_current_offset < uart_receive_next_offset)
      size = uart_receive_next_offset - uart_receive_current_offset;
    else
      size = uart_receive_next_offset +
             (UART_RECEIVE_BUFFER_SIZE - uart_receive_current_offset);

#ifdef DEBUG_LOG_RX_TX
    printf("RX: %d\r\n", size);
#endif

    if (size > XOFF_LIMIT)
      terminal_uart_xon_off(&terminal, XOFF);

    while (size--) {
      if (size % 80 == 0) {
        MX_USB_HOST_Process();
        keyboard_handle(&terminal);
      }

      character_t character = uart_receive_buffer[uart_receive_current_offset];
      terminal_uart_receive_character(&terminal, character);
      uart_receive_current_offset++;

      if (uart_receive_current_offset == UART_RECEIVE_BUFFER_SIZE)
        uart_receive_current_offset = 0;
    }
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
