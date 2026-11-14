/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : BMP280 Temperature and Pressure using STM32
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

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
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */


struct bme280_dev dev;
struct bme280_data comp_data;
struct bme280_uncomp_data uncomp_data;

int8_t rslt = BME280_OK;

uint8_t sensor_addr = 0x76;

double temperature;
double pressure;

uint8_t calibration[24];

char msg[150];

#define WIFI_SSID "SNUC"
#define WIFI_PASSWORD "snu12345"
#define NODE_RED_IP "10.31.30.248"

char json[150];
char http_request[350];
char esp_cmd[100];



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

HAL_StatusTypeDef BMP280_Read(uint8_t reg, uint8_t *data, uint16_t len);
HAL_StatusTypeDef BMP280_Write(uint8_t reg, uint8_t data);
int8_t BMP280_Read_Calibration(void);
int8_t BMP280_Init(void);
int8_t BMP280_Read_Data(void);

uint16_t read_u16_le(uint8_t *data);
int16_t read_s16_le(uint8_t *data);
void ESP_SendCommand(char *command, uint32_t delay);
void ESP_Init(void);
void ESP_SendReadings(double temp, double press);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Read BMP280 registers using I2C
 */

HAL_StatusTypeDef BMP280_Read(uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(sensor_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, data, len, 100);
}


/*
 * Write BMP280 registers using I2C
 */

HAL_StatusTypeDef BMP280_Write(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(sensor_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}


/*
 * Convert two bytes into unsigned 16-bit value
 */

uint16_t read_u16_le(uint8_t *data)
{
    return (uint16_t)((data[1] << 8) | data[0]);
}


/*
 * Convert two bytes into signed 16-bit value
 */

int16_t read_s16_le(uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}


/*
 * Read BMP280 calibration coefficients
 *
 * BMP280 has 24 bytes of temperature and pressure calibration data.
 * Unlike BME280, it has no humidity calibration registers.
 */

int8_t BMP280_Read_Calibration(void)
{
    if (BMP280_Read(0x88, calibration, 24) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    dev.calib_data.dig_t1 = read_u16_le(&calibration[0]);
    dev.calib_data.dig_t2 = read_s16_le(&calibration[2]);
    dev.calib_data.dig_t3 = read_s16_le(&calibration[4]);

    dev.calib_data.dig_p1 = read_u16_le(&calibration[6]);
    dev.calib_data.dig_p2 = read_s16_le(&calibration[8]);
    dev.calib_data.dig_p3 = read_s16_le(&calibration[10]);
    dev.calib_data.dig_p4 = read_s16_le(&calibration[12]);
    dev.calib_data.dig_p5 = read_s16_le(&calibration[14]);
    dev.calib_data.dig_p6 = read_s16_le(&calibration[16]);
    dev.calib_data.dig_p7 = read_s16_le(&calibration[18]);
    dev.calib_data.dig_p8 = read_s16_le(&calibration[20]);
    dev.calib_data.dig_p9 = read_s16_le(&calibration[22]);

    return BME280_OK;
}


/*
 * Initialize BMP280
 */

int8_t BMP280_Init(void)
{
    uint8_t chip_id = 0;

    /*
     * Read chip ID
     */

    if (BMP280_Read(0xD0, &chip_id, 1) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    if (chip_id != 0x58)
    {
        return BME280_E_DEV_NOT_FOUND;
    }

    sprintf(msg, "BMP280 DETECTED! CHIP ID: 0x%02X\r\n", chip_id);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    /*
     * Soft reset
     */

    if (BMP280_Write(0xE0, 0xB6) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    HAL_Delay(10);

    /*
     * Read temperature and pressure calibration
     */

    rslt = BMP280_Read_Calibration();

    if (rslt != BME280_OK)
    {
        return rslt;
    }

    /*
     * Configure filter
     *
     * 0xF5 = CONFIG register
     */

    if (BMP280_Write(0xF5, 0x00) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    /*
     * Configure temperature and pressure oversampling
     *
     * osrs_t = 001 -> 1X
     * osrs_p = 001 -> 1X
     * mode   = 011 -> Normal mode
     *
     * Binary: 00100111
     * Hex:    0x27
     */

    if (BMP280_Write(0xF4, 0x27) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    HAL_Delay(100);

    sprintf(msg, "BMP280 INITIALIZATION SUCCESSFUL!\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    return BME280_OK;
}


/*
 * Read raw temperature and pressure
 * Compensate using Bosch BME280 driver algorithms
 */

int8_t BMP280_Read_Data(void)
{
    uint8_t raw_data[6];

    if (BMP280_Read(0xF7, raw_data, 6) != HAL_OK)
    {
        return BME280_E_COMM_FAIL;
    }

    /*
     * Extract raw pressure
     */

    uncomp_data.pressure = ((uint32_t)raw_data[0] << 12) | ((uint32_t)raw_data[1] << 4) | ((uint32_t)raw_data[2] >> 4);

    /*
     * Extract raw temperature
     */

    uncomp_data.temperature = ((uint32_t)raw_data[3] << 12) | ((uint32_t)raw_data[4] << 4) | ((uint32_t)raw_data[5] >> 4);

    /*
     * Humidity is not supported by BMP280.
     */

    uncomp_data.humidity = 0;

    /*
     * Use Bosch compensation algorithm.
     *
     * Only pressure and temperature are selected.
     */

    rslt = bme280_compensate_data(BME280_PRESS | BME280_TEMP, &uncomp_data, &comp_data, &dev.calib_data);

    if (rslt != BME280_OK)
    {
        return rslt;
    }

    temperature = comp_data.temperature;
    pressure = comp_data.pressure;

    return BME280_OK;
}

/*
 * Send AT commands to ESP8266 through USART1
 */

void ESP_SendCommand(char *command, uint32_t delay)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)command, strlen(command), HAL_MAX_DELAY);
    HAL_Delay(delay);
}


/*
 * Initialize ESP8266 and connect to Wi-Fi
 */

void ESP_Init(void)
{
    ESP_SendCommand("AT\r\n", 1000);

    ESP_SendCommand("AT+CWMODE=1\r\n", 1000);

    snprintf(esp_cmd, sizeof(esp_cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);

    ESP_SendCommand(esp_cmd, 8000);
}


/*
 * Construct JSON and send HTTP POST request to Node-RED
 */

void ESP_SendReadings(double temp, double press)
{
    int content_length;
    int request_length;

    /*
     * Construct dynamic JSON payload
     */

    snprintf(json, sizeof(json), "{\"status\":\"success\",\"temperature\":%.2f,\"pressure\":%.2f}", temp, press);

    /*
     * Calculate actual JSON length
     */

    content_length = strlen(json);

    /*
     * Establish TCP connection with Node-RED
     */

    snprintf(esp_cmd, sizeof(esp_cmd), "AT+CIPSTART=\"TCP\",\"%s\",1880\r\n", NODE_RED_IP);

    ESP_SendCommand(esp_cmd, 3000);

    /*
     * Construct HTTP POST request
     */

    snprintf(http_request, sizeof(http_request), "POST /bmp280 HTTP/1.1\r\nHost: %s:1880\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", NODE_RED_IP, content_length, json);

    /*
     * Calculate complete HTTP request length
     */

    request_length = strlen(http_request);

    /*
     * Tell ESP8266 how many bytes to transmit
     */

    snprintf(esp_cmd, sizeof(esp_cmd), "AT+CIPSEND=%d\r\n", request_length);

    ESP_SendCommand(esp_cmd, 1000);

    /*
     * Transmit complete HTTP request
     */

    HAL_UART_Transmit(&huart1, (uint8_t*)http_request, request_length, HAL_MAX_DELAY);

    HAL_Delay(200);

    /*
     * Close TCP connection
     */

    ESP_SendCommand("AT+CIPCLOSE\r\n", 500);
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

    uint8_t addresses[] = {0x76, 0x77};
    uint8_t chip_id = 0;
    uint8_t found = 0;

    /*
     * Scan both possible I2C addresses.
     */

    for (int i = 0; i < 2; i++)
    {
        sensor_addr = addresses[i];

        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(sensor_addr << 1), 3, 100) == HAL_OK)
        {
            if (BMP280_Read(0xD0, &chip_id, 1) == HAL_OK && chip_id == 0x58)
            {
                found = 1;
                break;
            }
        }
    }

    if (!found)
    {
        sprintf(msg, "BMP280 NOT FOUND!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

        while (1);
    }

    sprintf(msg, "I2C ADDRESS: 0x%02X\r\n", sensor_addr);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    /*
     * Initialize BMP280.
     */

    rslt = BMP280_Init();
    ESP_Init();

    if (rslt != BME280_OK)
    {
        sprintf(msg, "BMP280 INITIALIZATION FAILED! ERROR: %d\r\n", rslt);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

        while (1);
    }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    	    HAL_Delay(100);

    	    rslt = BMP280_Read_Data();

    	    if (rslt == BME280_OK)
    	    {
    	        double pressure_hpa = pressure / 100.0;

    	        sprintf(msg, "Temperature: %.2f C | Pressure: %.2f hPa\r\n", temperature, pressure_hpa);

    	        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    	        ESP_SendReadings(temperature, pressure_hpa);
    	    }
    	    else
    	    {
    	        sprintf(msg, "BMP280 DATA READ FAILED! ERROR: %d\r\n", rslt);

    	        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
