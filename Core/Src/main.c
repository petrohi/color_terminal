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
#include "usb_device.h"
#include "usb_host.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbd_cdc_if.h"

#include "keys.h"
#include "terminal.h"
#include "terminal_config_ui.h"

extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
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
  CDC_Transmit_HS(ptr, len);
  return len;
}

struct terminal *global_terminal = NULL;
struct terminal_config_ui *global_terminal_config_ui = NULL;

#define UART_TRANSMIT_BUFFER_SIZE 256
#define UART_RECEIVE_BUFFER_SIZE (1024 * 16)
#define LOCAL_BUFFER_SIZE 256

 __attribute__((section(".dma"))) static character_t uart_transmit_buffer[UART_TRANSMIT_BUFFER_SIZE];
 __attribute__((section(".dma"))) static character_t uart_receive_buffer[UART_RECEIVE_BUFFER_SIZE];

static character_t local_buffer[LOCAL_BUFFER_SIZE];

static size_t local_head = 0;
static size_t local_tail = 0;

#define MAX_COLS 80
#define MAX_ROWS 30
#define TAB_STOPS_SIZE (MAX_COLS / 8)

static struct visual_cell default_cells[MAX_ROWS * MAX_COLS];
static struct visual_cell alt_cells[MAX_ROWS * MAX_COLS];
uint8_t tab_stops[TAB_STOPS_SIZE];

/*__attribute__((
    __section__(".flash_data")))*/ struct terminal_config terminal_config = {
    .format_rows = FORMAT_24_ROWS,

    .baud_rate = BAUD_RATE_115200,
    .word_length = WORD_LENGTH_8B,
    .stop_bits = STOP_BITS_1,
    .parity = PARITY_NONE,

    .charset = CHARSET_UTF8,
    .keyboard_compatibility = KEYBOARD_COMPATIBILITY_PC,
    .receive_c1_mode = C1_MODE_8BIT,
    .transmit_c1_mode = C1_MODE_7BIT,

    .auto_wrap_mode = true,
    .screen_mode = false,

    .send_receive_mode = true,

    .new_line_mode = false,
    .cursor_key_mode = false,
    .auto_repeat_mode = true,
    .ansi_mode = true,
    .backspace_mode = false,
    .application_keypad_mode = false,
    .keyboard_layout = KEYBOARD_LAYOUT_US,

    .start_up = START_UP_MESSAGE,

    .flow_control = true,
};

static void reset() {
  HAL_NVIC_SystemReset();
}

static void yield() {
  MX_USB_HOST_Process();

  if (Appli_state == APPLICATION_READY) {

    HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(&hUsbHostFS);

    if (info && global_terminal) {
      bool menu = false;
      uint8_t key = KEY_NONE;

      for (size_t i = 0; i < 6; ++i) {
        if (info->keys[i] == KEY_MENU) {
          menu = true;
        } else if (key == KEY_NONE) {
          key = info->keys[i];
        }
      }

      terminal_keyboard_handle_key(
          global_terminal, info->lshift || info->rshift,
          info->lalt || info->ralt, info->lctrl || info->rctrl,
          info->lgui || info->rgui, menu, key);
    }
  }
}

static void uart_transmit(character_t *characters, size_t size, size_t head) {
#ifdef DEBUG_LOG_TX
  printf("TX: %d\r\n", size);
#endif
  if (!global_terminal->send_receive_mode) {
    while (size--) {
      local_buffer[local_head] = *characters;
      local_head++;
      characters++;

      if (local_head == LOCAL_BUFFER_SIZE)
        local_head = 0;
    }
  }

  while (HAL_UART_Transmit_DMA(&huart7, (void *)characters, size) != HAL_OK)
    ;
}

static void screen_draw_codepoint_callback(struct format format, size_t row,
                                           size_t col, codepoint_t codepoint,
                                           enum font font, bool italic,
                                           bool underlined, bool crossedout,
                                           color_t active, color_t inactive) {
  screen_draw_codepoint(ltdc_get_screen(format), row, col, codepoint, font,
                        italic, underlined, crossedout, active, inactive);
}

static void screen_clear_rows_callback(struct format format, size_t from_row,
                                       size_t to_row, color_t inactive) {
  screen_clear_rows(ltdc_get_screen(format), from_row, to_row, inactive, yield);
}

static void screen_clear_cols_callback(struct format format, size_t row,
                                       size_t from_col, size_t to_col,
                                       color_t inactive) {
  screen_clear_cols(ltdc_get_screen(format), row, from_col, to_col, inactive,
                    yield);
}

static void screen_scroll_callback(struct format format, enum scroll scroll,
                                   size_t from_row, size_t to_row, size_t rows,
                                   color_t inactive) {
  screen_scroll(ltdc_get_screen(format), scroll, from_row, to_row, rows,
                inactive, yield);
}

static void screen_shift_right_callback(struct format format, size_t row,
                                        size_t col, size_t cols,
                                        color_t inactive) {
  screen_shift_right(ltdc_get_screen(format), row, col, cols, inactive, yield);
}

static void screen_shift_left_callback(struct format format, size_t row,
                                       size_t col, size_t cols,
                                       color_t inactive) {
  screen_shift_left(ltdc_get_screen(format), row, col, cols, inactive, yield);
}

#define MANDELBROT_X -0.7453
#define MANDELBROT_Y 0.1127
#define MANDELBROT_R 6.5e-4

static void screen_test_callback(struct format format,
                                 enum screen_test screen_test) {
  struct screen *screen = ltdc_get_screen(format);
  switch (screen_test) {
  case SCREEN_TEST_FONT1:
    screen_test_fonts(screen, FONT_NORMAL);
    break;
  case SCREEN_TEST_FONT2:
    screen_test_fonts(screen, FONT_BOLD);
    break;
  case SCREEN_TEST_COLOR1:
    screen_test_colors(screen);
    break;
  case SCREEN_TEST_COLOR2:
    screen_test_mandelbrot(screen, MANDELBROT_X, MANDELBROT_Y, MANDELBROT_R,
                           NULL);
    break;
  }
}

static void activate_config() {
  terminal_config_ui_activate(global_terminal_config_ui);
}

static void write_config(struct terminal_config *terminal_config_copy) {
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR);
  FLASH_Erase_Sector(FLASH_SECTOR_11, VOLTAGE_RANGE_3);

  for (size_t i = 0; i < sizeof(struct terminal_config); ++i)
    HAL_FLASH_Program(TYPEPROGRAM_BYTE,
                      (uint32_t)((uint8_t *)&terminal_config + i),
                      *(((uint8_t *)terminal_config_copy) + i));

  HAL_FLASH_Lock();
}

static void keyboard_set_leds(struct lock_state state) {
  uint8_t led_state =
      (state.scroll ? 0x4 : 0) | (state.caps ? 0x2 : 0) | (state.num ? 1 : 0);
  while (USBH_HID_SetReport(&hUsbHostFS, 0x02, 0x0, &led_state, 1) == USBH_BUSY)
    ;
}

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
  MX_UART7_Init();
  MX_USB_HOST_Init();
  MX_USB_DEVICE_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(READY_LED_GPIO_Port, READY_LED_Pin, GPIO_PIN_SET);

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
      .screen_test = screen_test_callback,
      .reset = reset,
      .yield = yield,
      .activate_config = activate_config,
      .write_config = write_config};
  terminal_init(&terminal, &callbacks, default_cells, alt_cells, tab_stops,
                TAB_STOPS_SIZE, &terminal_config, uart_transmit_buffer,
                UART_TRANSMIT_BUFFER_SIZE);
  global_terminal = &terminal;

  struct terminal_config_ui terminal_config_ui;
  global_terminal_config_ui = &terminal_config_ui;
  terminal_config_ui_init(&terminal_config_ui, &terminal, &terminal_config);

  terminal_keyboard_update_leds(&terminal);

  HAL_TIM_Base_Start_IT(&htim1);
  while (HAL_UART_Receive_DMA(&huart7, uart_receive_buffer,
                              UART_RECEIVE_BUFFER_SIZE) != HAL_OK)
    ;

  uint16_t uart_receive_tail = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */
    terminal_screen_update(&terminal);
    terminal_keyboard_repeat_key(&terminal);

    yield();

    if (terminal_config_ui.activated)
      continue;

    if (local_tail != local_head) {
      size_t size = 0;
      if (local_tail < local_head)
        size = local_head - local_tail;
      else
        size = local_head + (LOCAL_BUFFER_SIZE - local_tail);

      while (size--) {
        character_t character = local_buffer[local_tail];
        terminal_uart_receive_character(&terminal, character);
        local_tail++;

        if (local_tail == LOCAL_BUFFER_SIZE)
          local_tail = 0;
      }
    }

    uint16_t uart_receive_head =
        UART_RECEIVE_BUFFER_SIZE - huart7.hdmarx->Instance->NDTR;

    if (uart_receive_tail != uart_receive_head) {
      uint16_t size = 0;
      if (uart_receive_tail < uart_receive_head)
        size = uart_receive_head - uart_receive_tail;
      else
        size =
            uart_receive_head + (UART_RECEIVE_BUFFER_SIZE - uart_receive_tail);

#ifdef DEBUG_LOG_RX
      printf("RX: %d\r\n", size);
#endif

      terminal_uart_flow_control(&terminal, size);

      while (size--) {
        yield();

        if (terminal_config_ui.activated)
          break;

        terminal_uart_flow_control(&terminal, size);

        character_t character = uart_receive_buffer[uart_receive_tail];
        terminal_uart_receive_character(&terminal, character);
        uart_receive_tail++;

        if (uart_receive_tail == UART_RECEIVE_BUFFER_SIZE)
          uart_receive_tail = 0;
      }
    } else {
      terminal_uart_flow_control(&terminal, 0);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
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
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48|RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 50;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLQ;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
