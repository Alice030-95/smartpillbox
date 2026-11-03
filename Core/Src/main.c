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
#include "usart.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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
typedef enum
{
    WAITING = 0,
    REMINDING,
    CONFIRMED,
    OVERDUE,
    MISSED
} PillState;

typedef enum
{
    NOT_TAKEN = 0,
    TAKEN_ON_TIME,
    TAKEN_LATE,
    MISSED_DOSE
} PillResult;

volatile uint32_t system_ms = 0;

volatile uint8_t button_event = 0;
volatile uint32_t last_button_ms = 0;

PillState state = WAITING;

PillResult breakfast_result = NOT_TAKEN;
PillResult dinner_result = NOT_TAKEN;

uint8_t current_meal = 0;
uint8_t day_completed = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define BREAKFAST_TIME_MS   15000U
#define BREAKFAST_LATE_MS   25000U
#define BREAKFAST_END_MS    30000U

#define DINNER_TIME_MS      40000U
#define DINNER_LATE_MS      50000U
#define DINNER_END_MS       55000U

#define DAY_END_MS          60000U

#define LED_GREEN_PIN       GPIO_PIN_8
#define LED_YELLOW_PIN      GPIO_PIN_9
#define LED_RED_PIN         GPIO_PIN_10

#define LED_PORT            GPIOC

static void UART_Print(const char *text)
{
    HAL_UART_Transmit(
        &hlpuart1,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        100
    );
}

static void LED_Set(
    GPIO_PinState green,
    GPIO_PinState yellow,
    GPIO_PinState red
)
{
    HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN, green);
    HAL_GPIO_WritePin(LED_PORT, LED_YELLOW_PIN, yellow);
    HAL_GPIO_WritePin(LED_PORT, LED_RED_PIN, red);
}

static const char *Result_Text(PillResult result)
{
    switch (result)
    {
        case TAKEN_ON_TIME:
            return "ON TIME";

        case TAKEN_LATE:
            return "LATE";

        case MISSED_DOSE:
            return "MISSED";

        default:
            return "NOT TAKEN";
    }
}

static void Start_Reminder(uint8_t meal)
{
    current_meal = meal;
    state = REMINDING;

    LED_Set(GPIO_PIN_RESET,
            GPIO_PIN_SET,
            GPIO_PIN_RESET);

    if (meal == 1)
    {
        UART_Print("\r\n[BREAKFAST] REMINDER\r\n");
    }
    else
    {
        UART_Print("\r\n[DINNER] REMINDER\r\n");
    }
}

static void Set_Overdue(void)
{
    state = OVERDUE;

    LED_Set(GPIO_PIN_RESET,
            GPIO_PIN_RESET,
            GPIO_PIN_SET);

    UART_Print("[WARNING] OVERDUE\r\n");
}

static void Confirm_Medicine(void)
{
    PillResult result;

    if (state != REMINDING && state != OVERDUE)
    {
        return;
    }

    if (state == REMINDING)
    {
        result = TAKEN_ON_TIME;
    }
    else
    {
        result = TAKEN_LATE;
    }

    if (current_meal == 1)
    {
        breakfast_result = result;
    }
    else if (current_meal == 2)
    {
        dinner_result = result;
    }

    state = CONFIRMED;

    LED_Set(GPIO_PIN_SET,
            GPIO_PIN_RESET,
            GPIO_PIN_RESET);

    UART_Print("[CONFIRMED] MEDICINE TAKEN\r\n");
}

static void Mark_Missed(void)
{
    if (state == REMINDING || state == OVERDUE)
    {
        state = MISSED;

        if (current_meal == 1)
        {
            breakfast_result = MISSED_DOSE;
            UART_Print("[BREAKFAST] MISSED\r\n");
        }
        else if (current_meal == 2)
        {
            dinner_result = MISSED_DOSE;
            UART_Print("[DINNER] MISSED\r\n");
        }
    }

    LED_Set(GPIO_PIN_RESET,
            GPIO_PIN_RESET,
            GPIO_PIN_RESET);
}

static void Print_Daily_Report(void)
{
    char buffer[100];

    UART_Print("\r\n===== DAILY REPORT =====\r\n");

    snprintf(buffer, sizeof(buffer),
             "BREAKFAST: %s\r\n",
             Result_Text(breakfast_result));
    UART_Print(buffer);

    snprintf(buffer, sizeof(buffer),
             "DINNER: %s\r\n",
             Result_Text(dinner_result));
    UART_Print(buffer);

    UART_Print("===== DAY COMPLETED =====\r\n");
}

static void Pillbox_Update(uint32_t now)
{
    static uint8_t breakfast_started = 0;
    static uint8_t breakfast_finished = 0;
    static uint8_t dinner_started = 0;
    static uint8_t dinner_finished = 0;

    if (!breakfast_started && now >= BREAKFAST_TIME_MS)
    {
        breakfast_started = 1;
        Start_Reminder(1);
    }

    if (breakfast_started &&
        !breakfast_finished &&
        now >= BREAKFAST_LATE_MS &&
        state == REMINDING &&
        current_meal == 1)
    {
        Set_Overdue();
    }

    if (breakfast_started &&
        !breakfast_finished &&
        now >= BREAKFAST_END_MS)
    {
        breakfast_finished = 1;

        if (current_meal == 1)
        {
            Mark_Missed();
        }

        state = WAITING;
    }

    if (!dinner_started && now >= DINNER_TIME_MS)
    {
        dinner_started = 1;
        Start_Reminder(2);
    }

    if (dinner_started &&
        !dinner_finished &&
        now >= DINNER_LATE_MS &&
        state == REMINDING &&
        current_meal == 2)
    {
        Set_Overdue();
    }

    if (dinner_started &&
        !dinner_finished &&
        now >= DINNER_END_MS)
    {
        dinner_finished = 1;

        if (current_meal == 2)
        {
            Mark_Missed();
        }

        state = WAITING;
    }

    if (!day_completed && now >= DAY_END_MS)
    {
        day_completed = 1;

        LED_Set(GPIO_PIN_RESET,
                GPIO_PIN_RESET,
                GPIO_PIN_RESET);

        Print_Daily_Report();
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
  MX_LPUART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  LED_Set(GPIO_PIN_RESET,
          GPIO_PIN_RESET,
          GPIO_PIN_RESET);

  HAL_TIM_Base_Start_IT(&htim2);

  UART_Print("\r\n========================\r\n");
  UART_Print("SMART PILLBOX SYSTEM\r\n");
  UART_Print("SYSTEM STARTED\r\n");
  UART_Print("WAITING FOR BREAKFAST...\r\n");
  UART_Print("========================\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t now = system_ms;

	  if (!day_completed)
	  {
	      Pillbox_Update(now);
	  }

	  if (button_event)
	  {
	      button_event = 0;

	      now = system_ms;

	      if (!day_completed &&
	          ((current_meal == 1 && now < BREAKFAST_END_MS) ||
	           (current_meal == 2 && now < DINNER_END_MS)))
	      {
	          Confirm_Medicine();
	      }
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

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        system_ms++;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        uint32_t now = system_ms;

        if ((uint32_t)(now - last_button_ms) >= 200U)
        {
            last_button_ms = now;
            button_event = 1;
        }
    }
}
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
