/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : MeowShow — STM32 OLED Cat Animation Player
 *
 * Plays a pre-rendered cat GIF animation on a CH1116 128x64 OLED display
 * via I2C. Features a splash screen with a smiley face, then loops the
 * meow animation indefinitely.
 *
 * Hardware: STM32F103C8T6 on STM32_KIT board
 * Display:  CH1116 OLED, 128x64, I2C addr 0x7A
 * Build:    ARM GCC + CMake
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "oled.h"
#include "gif_animation.h"
#include "meow_anim.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/** I2C1 handle — used by OLED driver for I2C communication. */
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void ShowSplashScreen(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */

  /* Wait for OLED to power up (it boots slower than STM32) */
  HAL_Delay(50);

  /* Initialize CH1116 OLED display */
  OLED_Init();

  /* Show splash screen (~1 second) */
  ShowSplashScreen();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Play the meow cat animation in an infinite loop */
    GifAnim_Play(&gif_meow_anim, 1);

    /* If animation returns (shouldn't with infinite loop), restart */
  }
  /* USER CODE END 3 */
}

/* ---------------------------------------------------------------------------*/
/*                         I2C1 Initialization                                */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  Initialize the I2C1 peripheral.
  *         PB6 = SCL, PB7 = SDA, 400 kHz Fast Mode.
  * @retval None
  */
void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 400000;           /* 400 kHz Fast Mode */
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;  /* Fast Mode duty cycle */
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
}

/* ---------------------------------------------------------------------------*/
/*                        System Clock Configuration                          */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  System Clock Configuration.
  *
  * Primary plan:  HSE 8 MHz → PLL ×9 = 72 MHz (SYSCLK)
  * Fallback plan: HSI 8 MHz / 2 → PLL ×16 = 64 MHz (SYSCLK)
  *
  * AHB  = SYSCLK / 1 (72 or 64 MHz)
  * APB1 = HCLK  / 2 (36 or 32 MHz, max 36)
  * APB2 = HCLK  / 1 (72 or 64 MHz)
  *
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  HAL_StatusTypeDef  ret;

  /* ---- Attempt HSE + PLL configuration ---- */

  /* Enable HSE with 8 MHz external crystal.
   * NOTE: STM32F103C8T6 does NOT have an HSE predivider — do not set
   * HSEPredivValue, or HAL_RCC_OscConfig may return an error. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;   /* 8 MHz × 9 = 72 MHz */

  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);

  if (ret == HAL_OK) {
    /* HSE started successfully — configure system + bus clocks at 72 MHz */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* AHB  = 72 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1 = 36 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2 = 72 MHz */

    /* Flash latency: 2 wait states for 72 MHz @ 3.3V */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
      Error_Handler();
    }
  } else {
    /* ---- HSE failed: fall back to HSI + PLL ---- */

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI_DIV2;  /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL16;           /* 4×16 = 64 MHz */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* AHB  = 64 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1 = 32 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2 = 64 MHz */

    /* Flash latency: 2 wait states for 64 MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
      Error_Handler();
    }
  }

  /* Explicitly reconfigure SysTick for the new HCLK frequency.
   * HAL_RCC_ClockConfig may or may not do this internally depending on
   * HAL version — doing it explicitly guarantees HAL_Delay() works. */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* ---------------------------------------------------------------------------*/
/*                           Splash Screen                                    */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  Display a splash screen with a smiley face and loading text.
  *
  * Shows for approximately 1 second. The smiley face has:
  *   - Circular face outline (radius 18, center ~64,27)
  *   - Two filled-circle eyes
  *   - A parabolic smile mouth
  *   - "Loading Meow..." text below in 12x6 font
  *
  * @retval None
  */
static void ShowSplashScreen(void)
{
  uint8_t sx;

  OLED_NewFrame();

  /* ---- Smiley face ---- */

  /* Face outline — circle, radius 18, center (64, 27) */
  OLED_DrawCircle(64, 27, 18, OLED_WHITE);

  /* Left eye — filled circle at (56, 21), radius 3 */
  OLED_DrawFilledCircle(56, 21, 3, OLED_WHITE);

  /* Right eye — filled circle at (72, 21), radius 3 */
  OLED_DrawFilledCircle(72, 21, 3, OLED_WHITE);

  /* Smile — parabolic arc from x=50 to x=78 */
  for (sx = 50; sx <= 78; sx++) {
    int16_t dx = (int16_t)sx - 64;
    /* y = 33 + (dx^2) / 30 - 5  →  smile dips down then up */
    int16_t dy = 33 + (dx * dx) / 30 - 5;
    if (dy >= 0 && dy < (int16_t)OLED_HEIGHT) {
      OLED_SetPixel(sx, (uint8_t)dy, OLED_WHITE);
    }
  }

  /* ---- Loading text ---- */
  OLED_PrintString(16, 50, "Loading Meow...", &font12x6, OLED_WHITE);

  /* Push to display */
  OLED_ShowFrame();

  /* Hold splash for ~1 second */
  HAL_Delay(1000);
}

/* ---------------------------------------------------------------------------*/
/*                             Error Handler                                  */
/* ---------------------------------------------------------------------------*/

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* Disable interrupts and loop forever */
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
