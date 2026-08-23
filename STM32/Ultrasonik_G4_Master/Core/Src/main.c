/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "system_state.h"
#include "esp32_uart.h"
#include "pt100_adc.h"
#include "heater_relay.h"
#include "ultrasonic_pwm.h"
#include "process_timer.h"
#include "x9c103s.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // HIL_DEEP_DEBUG: snprintf for the COM11 debug stream
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
static uint32_t last_status_tick_ms = 0;
static uint32_t last_hil_debug_tick_ms = 0;  // HIL_DEEP_DEBUG

/* Tank ID override storage: last 2KB page of 512KB flash (Bank 2, page 127),
 * clear of the application image. Layout: [0]=magic (uint32_t), [4]=id (uint32_t). */
#define TANK_ID_FLASH_ADDR  0x0807F800UL
#define TANK_ID_FLASH_PAGE  127U
#define TANK_ID_FLASH_BANK  FLASH_BANK_2
#define TANK_ID_MAGIC       0xA5A5A5A5UL

/* Bench test bypass: forces MY_TANK_ID to this value and skips Flash/DIP
 * reads entirely. Set to 0 to restore normal production boot logic. */
#define BENCH_DEV_MODE_ID 0  // 0 = Production boot logic (Flash Page 127 override -> DIP -> UNCOMMISSIONED)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

OPAMP_HandleTypeDef hopamp3;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart3;
UART_HandleTypeDef hlpuart1;  // HIL_TEST_MOD: ST-Link VCP (COM11) debug/telemetry echo
IWDG_HandleTypeDef hiwdg;

/* USER CODE BEGIN PV */
/* This node's multi-drop bus address (1-10). Set at boot in main() from a
 * Flash override (TankId_Load) or, failing that, the DIP switch GPIOs. */
uint8_t MY_TANK_ID = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_LPUART1_UART_Init(void);  // HIL_TEST_MOD: ST-Link VCP (COM11) init
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define STAGING_TIMEOUT_MS  10000U
static uint32_t s_staging_start_tick = 0U;
static uint8_t  s_staging_active     = 0U;
static uint8_t  s_saved_tank_id      = 1U;
static ProvState_t s_saved_prov_state = PROV_STATE_UNCOMMISSIONED;

/* Returns the Flash-stored override ID (1..10), or 0 if no valid override is present. */
uint8_t TankId_Load(uint8_t *out_state)
{
  uint32_t magic          = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR);
  uint32_t stored_payload = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR + 4U);

  if (magic == TANK_ID_MAGIC)
  {
    uint8_t id    = (uint8_t)(stored_payload & 0xFFU);
    uint8_t state = (uint8_t)((stored_payload >> 8) & 0xFFU);

    if (id >= 1U && id <= 10U)
    {
      if (out_state != NULL)
      {
        *out_state = state;
      }
      return id;
    }
  }

  if (out_state != NULL)
  {
    *out_state = (uint8_t)PROV_STATE_UNCOMMISSIONED;
  }
  return 0U;
}

/* Writes Flash Bank 2 Page 127 (0x0807F800) with 0xA5A5A5A5 magic key and performs instant readback verification. */
uint8_t TankId_SaveAndVerifyOverride(uint8_t new_id, uint8_t state)
{
  if (new_id < 1U || new_id > 10U)
  {
    return 0U;
  }

  if (g_system_state.mode == SYS_MODE_RUNNING)
  {
    return 0U; /* Interlock: Never modify Flash while high-voltage PWM/heating is active */
  }

  FLASH_EraseInitTypeDef erase_init;
  uint32_t page_error = 0U;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks     = TANK_ID_FLASH_BANK;
  erase_init.Page      = TANK_ID_FLASH_PAGE;
  erase_init.NbPages   = 1U;

  __disable_irq();
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
  {
    HAL_FLASH_Lock();
    __enable_irq();
    __DSB();
    return 0U;
  }

  uint32_t payload = ((uint32_t)state << 8) | (uint32_t)new_id;
  uint64_t data = ((uint64_t)payload << 32) | (uint64_t)TANK_ID_MAGIC;

  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, TANK_ID_FLASH_ADDR, data) != HAL_OK)
  {
    HAL_FLASH_Lock();
    __enable_irq();
    __DSB();
    return 0U;
  }

  HAL_FLASH_Lock();
  __enable_irq();
  __DSB();

  /* Instant Readback Verification */
  uint32_t verify_magic   = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR);
  uint32_t verify_payload = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR + 4U);

  uint8_t readback_id    = (uint8_t)(verify_payload & 0xFFU);
  uint8_t readback_state = (uint8_t)((verify_payload >> 8) & 0xFFU);

  if (verify_magic == TANK_ID_MAGIC && readback_id == new_id && readback_state == state)
  {
    MY_TANK_ID = new_id;
    g_system_state.prov_state = (ProvState_t)state;
    return 1U;
  }

  return 0U;
}

/* Erases Flash Bank 2 Page 127, reverting Flash state to UNCOMMISSIONED */
uint8_t TankId_EraseOverride(void)
{
  if (g_system_state.mode == SYS_MODE_RUNNING)
  {
    return 0U;
  }

  FLASH_EraseInitTypeDef erase_init;
  uint32_t page_error = 0U;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks     = TANK_ID_FLASH_BANK;
  erase_init.Page      = TANK_ID_FLASH_PAGE;
  erase_init.NbPages   = 1U;

  __disable_irq();
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
  {
    HAL_FLASH_Lock();
    __enable_irq();
    __DSB();
    return 0U;
  }

  HAL_FLASH_Lock();
  __enable_irq();
  __DSB();

  MY_TANK_ID = 0U;
  g_system_state.prov_state = PROV_STATE_UNCOMMISSIONED;
  return 1U;
}

/* Volatile RAM Staging logic: updates RAM state to ID=0 STAGING without erasing/writing Flash,
 * initiating non-blocking 10,000 ms auto-timeout rollback timer. */
void TankId_StartStaging(void)
{
  if (s_staging_active)
  {
    /* Already staging: refresh tick timer only, do NOT overwrite s_saved_tank_id */
    s_staging_start_tick = HAL_GetTick();
    return;
  }

  s_saved_tank_id = MY_TANK_ID;
  s_saved_prov_state = g_system_state.prov_state;

  MY_TANK_ID = 0U;
  g_system_state.prov_state = PROV_STATE_STAGING;

  s_staging_start_tick = HAL_GetTick();
  s_staging_active = 1U;
}

void TankId_ProcessStagingTimeout(void)
{
  if (s_staging_active)
  {
    if ((HAL_GetTick() - s_staging_start_tick) >= STAGING_TIMEOUT_MS)
    {
      MY_TANK_ID = s_saved_tank_id;
      g_system_state.prov_state = s_saved_prov_state;
      s_staging_active = 0U;

      const char *log_msg = "LOG:STAGING_TIMEOUT_ROLLBACK\n";
      HAL_UART_Transmit(&huart3, (const uint8_t *)log_msg, (uint16_t)strlen(log_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)log_msg, (uint16_t)strlen(log_msg), 10);
    }
  }
}

void TankId_CancelStaging(void)
{
  if (s_staging_active)
  {
    MY_TANK_ID = s_saved_tank_id;
    g_system_state.prov_state = s_saved_prov_state;
    s_staging_active = 0U;
  }
}

void TankId_ConfirmStaging(uint8_t final_id)
{
  if (s_staging_active)
  {
    s_staging_active = 0U;
  }
  (void)TankId_SaveAndVerifyOverride(final_id, (uint8_t)PROV_STATE_ACTIVE);
}

/* ====================================================================
 * BENCH TEST DIAGNOSTIC API
 * ==================================================================== */

uint8_t HeaterTest_Readback(void)
{
  uint8_t expected = HAL_GPIO_ReadPin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin) == GPIO_PIN_SET ? 1 : 0;
  uint8_t actual = HAL_GPIO_ReadPin(HEATER_TEST_FB_GPIO_Port, HEATER_TEST_FB_Pin) == GPIO_PIN_SET ? 1 : 0;
  return (expected == actual) ? 1 : 0;
}

uint8_t TriacTest_Readback(void)
{
  uint8_t expected = HAL_GPIO_ReadPin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin) == GPIO_PIN_SET ? 1 : 0;
  uint8_t actual = HAL_GPIO_ReadPin(TRIAC_TEST_FB_GPIO_Port, TRIAC_TEST_FB_Pin) == GPIO_PIN_SET ? 1 : 0;
  return (expected == actual) ? 1 : 0;
}

static void BenchTest_Process(void)
{
  static uint32_t last_diagnostic_tick = 0;
  if ((HAL_GetTick() - last_diagnostic_tick) >= 1000u)
  {
    last_diagnostic_tick = HAL_GetTick();
    if (!HeaterTest_Readback())
    {
      char msg[] = "DIAGNOSTIC: HEATER_FEEDBACK_MISMATCH\r\n";
      HAL_UART_Transmit(&hlpuart1, (uint8_t *)msg, sizeof(msg) - 1, 10);
    }
    if (!TriacTest_Readback())
    {
      char msg[] = "DIAGNOSTIC: TRIAC_FEEDBACK_MISMATCH\r\n";
      HAL_UART_Transmit(&hlpuart1, (uint8_t *)msg, sizeof(msg) - 1, 10);
    }
  }
}

static uint8_t s_lpuart_rx_buf[64];
static uint8_t s_lpuart_rx_idx = 0;
static uint8_t s_x9c_bench_active = 0;
static uint32_t s_last_bench_stream_tick = 0;

static void LPUART1_Process(void)
{
  if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_ORE) != RESET)
  {
    __HAL_UART_CLEAR_FLAG(&hlpuart1, UART_CLEAR_OREF);
  }
  if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE) != RESET)
  {
    __HAL_UART_CLEAR_FLAG(&hlpuart1, UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
  }

  while (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE) != RESET)
  {
    uint8_t ch = (uint8_t)(hlpuart1.Instance->RDR & 0xFF);
    if (ch == '\n' || ch == '\r')
    {
      if (s_lpuart_rx_idx > 0)
      {
        s_lpuart_rx_buf[s_lpuart_rx_idx] = '\0';
        char *cmd = (char *)s_lpuart_rx_buf;
        while (*cmd == ' ') cmd++;

        if (strcmp(cmd, "X9C_TEST:FREQ28") == 0 || strcmp(cmd, "SET_FREQ:28") == 0)
        {
          X9C103S_SetSweepEnabled(0);
          (void)X9C103S_SetFrequency(28);
          s_x9c_bench_active = 1;
          const char *msg = "ACK:X9C_TEST:FREQ28\r\n";
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
        }
        else if (strcmp(cmd, "X9C_TEST:FREQ40") == 0 || strcmp(cmd, "SET_FREQ:40") == 0)
        {
          X9C103S_SetSweepEnabled(0);
          (void)X9C103S_SetFrequency(40);
          s_x9c_bench_active = 1;
          const char *msg = "ACK:X9C_TEST:FREQ40\r\n";
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
        }
        else if (strncmp(cmd, "X9C_TEST:STEP", 13) == 0)
        {
          uint8_t stp = (uint8_t)atoi(&cmd[13]);
          X9C103S_SetSweepEnabled(0);
          (void)X9C103S_SetStep(stp);
          s_x9c_bench_active = 1;
          char ack[40];
          int acklen = snprintf(ack, sizeof(ack), "ACK:X9C_TEST:STEP%u\r\n", stp);
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack, (uint16_t)acklen, 10);
        }
        else if (strcmp(cmd, "X9C_TEST:SWEEP28") == 0)
        {
          (void)X9C103S_SetFrequency(28);
          X9C103S_SetSweepEnabled(1);
          s_x9c_bench_active = 1;
          const char *msg = "ACK:X9C_TEST:SWEEP28\r\n";
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
        }
        else if (strcmp(cmd, "X9C_TEST:SWEEP40") == 0)
        {
          (void)X9C103S_SetFrequency(40);
          X9C103S_SetSweepEnabled(1);
          s_x9c_bench_active = 1;
          const char *msg = "ACK:X9C_TEST:SWEEP40\r\n";
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
        }
        else if (strcmp(cmd, "X9C_TEST:STOP") == 0 || strcmp(cmd, "SWEEP:OFF") == 0)
        {
          X9C103S_SetSweepEnabled(0);
          s_x9c_bench_active = 0;
          const char *msg = "ACK:X9C_TEST:STOP\r\n";
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
        }
        else if (strncmp(cmd, "SET_STEP_INC:", 13) == 0)
        {
          uint8_t inc = (uint8_t)atoi(&cmd[13]);
          (void)X9C103S_SetStepIncrement(inc);
          char ack[40];
          int acklen = snprintf(ack, sizeof(ack), "ACK:STEP_INC:%u\r\n", inc);
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack, (uint16_t)acklen, 10);
        }
        else if (strncmp(cmd, "SET_SWP_SPAN:", 13) == 0)
        {
          uint8_t span = (uint8_t)atoi(&cmd[13]);
          (void)X9C103S_SetSweepSpan(span);
          char ack[40];
          int acklen = snprintf(ack, sizeof(ack), "ACK:SWP_SPAN:%u\r\n", span);
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack, (uint16_t)acklen, 10);
        }
        else if (strncmp(cmd, "SET_SWP_PER:", 12) == 0)
        {
          uint16_t per = (uint16_t)atoi(&cmd[12]);
          (void)X9C103S_SetSweepPeriod(per);
          char ack[40];
          int acklen = snprintf(ack, sizeof(ack), "ACK:SWP_PER:%u\r\n", per);
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack, (uint16_t)acklen, 10);
        }
        else if (strcmp(cmd, "SWEEP:ON") == 0)
        {
          X9C103S_SetSweepEnabled(1);
          s_x9c_bench_active = 1;
          char ack[64];
          int acklen = snprintf(ack, sizeof(ack), "ACK:SWEEP:ON,PERIOD_MS=%u,SPAN=+-%uKHZ\r\n",
                                (unsigned int)X9C103S_GetSweepPeriod(),
                                (unsigned int)X9C103S_GetSweepSpan());
          HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack, (uint16_t)acklen, 10);
        }
        s_lpuart_rx_idx = 0;
      }
    }
    else if (s_lpuart_rx_idx < sizeof(s_lpuart_rx_buf) - 1)
    {
      if (ch >= 32 && ch <= 126)
      {
        s_lpuart_rx_buf[s_lpuart_rx_idx++] = ch;
      }
    }
  }

  /* 20 Hz (50 ms) Telemetry stream when bench active */
  if (s_x9c_bench_active && (HAL_GetTick() - s_last_bench_stream_tick) >= 50u)
  {
    s_last_bench_stream_tick = HAL_GetTick();
    uint16_t raw = PA0_ADC1_GetLastRaw();
    uint32_t mv = ((uint32_t)raw * 3300U + 2047U) / 4095U;
    char logbuf[160];
    int len = snprintf(logbuf, sizeof(logbuf),
                       "X9C_TEST,t_ms=%lu,target=%u,actual=%u,adc_raw=%u,adc_v=%lu.%03lu,cs=%s,ud=%s,inc=%s\r\n",
                       (unsigned long)HAL_GetTick(),
                       (unsigned int)X9C103S_GetTargetStep(),
                       (unsigned int)X9C103S_GetCurrentStep(),
                       (unsigned int)raw,
                       (unsigned long)(mv / 1000U),
                       (unsigned long)(mv % 1000U),
                       (HAL_GPIO_ReadPin(X9C_CS_GPIO_Port, X9C_CS_Pin) == GPIO_PIN_SET) ? "HIGH" : "LOW",
                       (HAL_GPIO_ReadPin(X9C_UD_GPIO_Port, X9C_UD_Pin) == GPIO_PIN_SET) ? "HIGH" : "LOW",
                       (HAL_GPIO_ReadPin(X9C_INC_GPIO_Port, X9C_INC_Pin) == GPIO_PIN_SET) ? "HIGH" : "LOW");
    if (len > 0)
    {
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)logbuf, (uint16_t)len, 10);
    }
  }
}

/* HIL_DEEP_DEBUG: white-box internals, printed only from the main superloop (never an ISR)
 * onto the ST-Link VCP (COM11) so the HIL test host can verify internal MCU math/logic
 * (raw ADC counts, triac firing delay, relay bit) that the STAT telegram never exposes. */
static void HIL_DeepDebug_Print(void)
{
  uint16_t pa0_raw = PA0_ADC1_GetLastRaw();
  uint32_t pa0_mv = ((uint32_t)pa0_raw * 3300U + 2047U) / 4095U;
  char buf[160];
  int len = snprintf(buf, sizeof(buf), "DEBUG_STM: PT100_ADC=%lu, PA0_ADC=%u, PA0_V=%lu.%03lu, DELAY=%lu, RELAY=%u, HEATER_OUT=%u, HEATER_FB=%u, TRIAC_OUT=%u, TRIAC_FB=%u\r\n",
                      (unsigned long)PT100_ADC_GetLastRaw(),
                      (unsigned int)pa0_raw,
                      (unsigned long)(pa0_mv / 1000U),
                      (unsigned long)(pa0_mv % 1000U),
                      (unsigned long)UltrasonicPWM_GetCurrentDelayUs(),
                      (unsigned int)g_system_state.relay_state,
                      (unsigned int)(HAL_GPIO_ReadPin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin) == GPIO_PIN_SET ? 1 : 0),
                      (unsigned int)(HAL_GPIO_ReadPin(HEATER_TEST_FB_GPIO_Port, HEATER_TEST_FB_Pin) == GPIO_PIN_SET ? 1 : 0),
                      (unsigned int)(HAL_GPIO_ReadPin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin) == GPIO_PIN_SET ? 1 : 0),
                      (unsigned int)(HAL_GPIO_ReadPin(TRIAC_TEST_FB_GPIO_Port, TRIAC_TEST_FB_Pin) == GPIO_PIN_SET ? 1 : 0));
  if (len > 0)
  {
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)buf, (uint16_t)len, 10);
  }
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
  MX_ADC1_Init();
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  MX_ADC2_Init();
  MX_OPAMP3_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  MX_LPUART1_UART_Init();  // HIL_TEST_MOD: ST-Link VCP (COM11) for HIL test observability
  MX_IWDG_Init();          // 1000 ms Hardware IWDG initialization

  /* USER CODE BEGIN 2 */

  SystemState_Init();

#if (BENCH_DEV_MODE_ID > 0)
  /* Bench dev mode: fixed ID override for single-node desktop testing */
  MY_TANK_ID = BENCH_DEV_MODE_ID;
  g_system_state.prov_state = PROV_STATE_ACTIVE;
#else
  /* Production Boot Rule: Flash Page 127 override determines commissioning identity */
  {
    uint8_t init_state = (uint8_t)PROV_STATE_UNCOMMISSIONED;
    uint8_t override_id = TankId_Load(&init_state);
    if (override_id >= 1U && override_id <= 10U && init_state == (uint8_t)PROV_STATE_ACTIVE)
    {
      MY_TANK_ID = override_id;
      g_system_state.prov_state = PROV_STATE_ACTIVE;
    }
    else
    {
      MY_TANK_ID = 0U;
      g_system_state.prov_state = PROV_STATE_UNCOMMISSIONED;
    }
  }
#endif

  /* Check if reset was caused by Hardware IWDG timeout */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
    __HAL_RCC_CLEAR_RESET_FLAGS();
    SystemState_SafeStop(STOP_REASON_WATCHDOG_RESET);
  }

  ESP32_UART_Init();
  PT100_ADC_Init();
  HeaterRelay_Init();
  UltrasonicPWM_Init();
  ProcessTimer_Init();
  X9C103S_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    ESP32_UART_Process();
    X9C103S_SweepProcess();
    PT100_ADC_Process();
    PA0_ADC1_Process();
    LPUART1_Process();
    HeaterRelay_Process();
    UltrasonicPWM_Process();
    ProcessTimer_Process();

    /* Check auto-timeout for volatile RAM staging rollback */
    TankId_ProcessStagingTimeout();

    /* Non-blocking 500 ms telemetry heartbeat to the ESP32 (HAL_GetTick(),
     * never blocks the superloop) */
    if ((HAL_GetTick() - last_status_tick_ms) >= 500u)
    {
      last_status_tick_ms = HAL_GetTick();
      ESP32_UART_SendStatus();
    }

    /* HIL_DEEP_DEBUG: independent non-blocking 500 ms white-box stream on COM11 */
    if ((HAL_GetTick() - last_hil_debug_tick_ms) >= 500u)
    {
      last_hil_debug_tick_ms = HAL_GetTick();
      HIL_DeepDebug_Print();
    }

    BenchTest_Process();

    /* Refresh Hardware Independent Watchdog (1000 ms timeout) */
    HAL_IWDG_Refresh(&hiwdg);
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function (PA0 / ADC1_IN1 X9C Wiper Voltage)
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_1; /* PA0 = ADC1_IN1 */
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5; /* 640.5 cycles (15 us) settling time for 1k-3.5k ohm source impedance */
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP3_ADC2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief OPAMP3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp3.Init.Mode = OPAMP_PGA_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp3.Init.InternalOutput = ENABLE;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp3.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function (HIL_TEST_MOD: ST-Link VCP, exposed to host PC as COM11)
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* HIL_TEST_MOD: routes libc printf() straight to the ST-Link VCP (COM11) so any
 * future/debug printf() calls are visible to the HIL test host alongside the
 * STAT,... telegrams echoed from ESP32_UART_SendStatus(). Weak symbol expected
 * by syscalls.c's _write(). */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  HAL_UART_Transmit(&hlpuart1, &c, 1, 10);
  return ch;
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level (Hold PB15, PC6, and RS485 DE LOW at reset/init) */
  HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, X9C_CS_Pin | X9C_INC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, X9C_UD_Pin, GPIO_PIN_RESET);

  /*Configure RS485_DE_Pin (PB1) for RS485 direction control */
  GPIO_InitStruct.Pin = RS485_DE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(RS485_DE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LPUART1_TX_Pin LPUART1_RX_Pin */
  GPIO_InitStruct.Pin = LPUART1_TX_Pin|LPUART1_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure BENCH TEST physical loopback feedback pins (PA4, PA6) */
  GPIO_InitStruct.Pin = HEATER_TEST_FB_Pin | TRIAC_TEST_FB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure HEATER_RELAY_Pin (PB15) with active pull-down */
  GPIO_InitStruct.Pin = HEATER_RELAY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure X9C_CS_Pin X9C_UD_Pin X9C_INC_Pin */
  GPIO_InitStruct.Pin = X9C_CS_Pin | X9C_UD_Pin | X9C_INC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure TRIAC_GATE_Pin (PC6) with active pull-down */
  GPIO_InitStruct.Pin = TRIAC_GATE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/**
  * @brief IWDG Initialization Function (1000 ms timeout using 32 kHz LSI clock / 32 prescaler)
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{
  __HAL_RCC_LSI_ENABLE();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET)
  {
  }

  IWDG->KR = 0xCCCC; /* Enable IWDG clock domain */

  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 1000;
  hiwdg.Init.Window = IWDG_WINDOW_DISABLE;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
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
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
