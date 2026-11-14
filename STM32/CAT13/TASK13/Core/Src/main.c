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
#include "bme280.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define I2C_TIMEOUT_MS   100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
struct bme280_dev dev;
struct bme280_settings settings;
struct bme280_data comp_data;
int8_t rslt = BME280_OK;
uint8_t chip_id = 0;
uint32_t meas_delay_us = 0;

/* I2C address of the sensor: 0x76 (SDO low) or 0x77 (SDO high) */
static uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

/* Final calculated values */
float temperature, pressure, humidity;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
void user_delay_us(uint32_t period, void *intf_ptr);
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
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */

  /* Hook the Bosch driver up to the STM32 HAL I2C functions */
  dev.intf_ptr = &dev_addr;
  dev.intf     = BME280_I2C_INTF;
  dev.read     = user_i2c_read;
  dev.write    = user_i2c_write;
  dev.delay_us = user_delay_us;

  /* Read chip ID (register 0xD0) and check it */
  rslt = bme280_get_regs(BME280_REG_CHIP_ID, &chip_id, 1, &dev);
  if (rslt != BME280_OK || chip_id != BME280_CHIP_ID)
  {
    printf("BME280 not found (rslt=%d, chip_id=0x%02X)\r\n", rslt, chip_id);
    Error_Handler();
  }

  /* Soft reset and load trim (calibration) data */
  rslt = bme280_init(&dev);
  if (rslt != BME280_OK)
  {
    printf("bme280_init failed (rslt=%d)\r\n", rslt);
    Error_Handler();
  }

  /* Configure sensor: oversampling, filter, standby time */
  settings.osr_h        = BME280_OVERSAMPLING_1X;
  settings.osr_p        = BME280_OVERSAMPLING_1X;
  settings.osr_t        = BME280_OVERSAMPLING_1X;
  settings.filter       = BME280_FILTER_COEFF_OFF;
  settings.standby_time = BME280_STANDBY_TIME_1000_MS;

  rslt = bme280_set_sensor_settings(BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP |
                                    BME280_SEL_OSR_HUM   | BME280_SEL_FILTER   |
                                    BME280_SEL_STANDBY, &settings, &dev);
  if (rslt != BME280_OK)
  {
    printf("bme280_set_sensor_settings failed (rslt=%d)\r\n", rslt);
    Error_Handler();
  }

  /* Sensor mode: normal = continuous measurement cycle */
  rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
  if (rslt != BME280_OK)
  {
    printf("bme280_set_sensor_mode failed (rslt=%d)\r\n", rslt);
    Error_Handler();
  }

  /* Wait for the first conversion to complete */
  bme280_cal_meas_delay(&meas_delay_us, &settings);
  dev.delay_us(meas_delay_us, dev.intf_ptr);

  printf("BME280 ready (chip ID 0x%02X)\r\n", chip_id);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Read raw data and apply Bosch compensation */
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);

    if (rslt == BME280_OK)
    {
      /* Default build (BME280_DOUBLE_ENABLE): degC, Pa, %RH */
      temperature = (float)comp_data.temperature;        /* degC */
      pressure    = (float)comp_data.pressure / 100.0f;  /* Pa -> hPa */
      humidity    = (float)comp_data.humidity;           /* %RH */

      printf("T = %.2f C   P = %.2f hPa   H = %.2f %%\r\n",temperature, pressure, humidity);
    }
    else
    {
      printf("Read error (rslt=%d)\r\n", rslt);
    }

    HAL_Delay(1000); /* Poll once per second */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* Bosch driver -> STM32 HAL: read registers */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  /* HAL expects the 7-bit address shifted left by 1 */
  uint8_t addr = *(uint8_t *)intf_ptr;

  if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), reg_addr,
                       I2C_MEMADD_SIZE_8BIT, reg_data, (uint16_t)len,
                       I2C_TIMEOUT_MS) == HAL_OK)
  {
    return BME280_OK;
  }
  return BME280_E_COMM_FAIL;
}

/* Bosch driver -> STM32 HAL: write registers */
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  uint8_t addr = *(uint8_t *)intf_ptr;

  if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr << 1), reg_addr,
                        I2C_MEMADD_SIZE_8BIT, (uint8_t *)reg_data, (uint16_t)len,
                        I2C_TIMEOUT_MS) == HAL_OK)
  {
    return BME280_OK;
  }
  return BME280_E_COMM_FAIL;
}

/* Bosch driver -> STM32 HAL: delay (driver passes microseconds) */
void user_delay_us(uint32_t period, void *intf_ptr)
{
  (void)intf_ptr;
  /* HAL_Delay is in ms; round up so we never wait too little */
  HAL_Delay((period / 1000U) + 1U);
}

/* Retarget printf to USART1 */
int _write(int file, char *ptr, int len)
{
  (void)file;
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, 100);
  return len;
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
