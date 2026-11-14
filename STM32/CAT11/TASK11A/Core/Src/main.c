/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F410 and ESP8266 HTTP server using AT commands
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ESP_RX_SIZE       1024
#define ESP_BAUD_TIMEOUT  2000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
static char esp_rx[ESP_RX_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void debug_print(const char *text)
{
    HAL_UART_Transmit(&huart6, (uint8_t *)text,
                      (uint16_t)strlen(text), HAL_MAX_DELAY);
}

/* Read UART1 until no new byte has arrived for quiet_ms, or max_ms expires. */
static int esp_read_quiet(char *buffer, int capacity,
                          uint32_t quiet_ms, uint32_t max_ms)
{
    uint8_t ch;
    int count = 0;
    uint32_t start = HAL_GetTick();
    uint32_t last_byte = start;

    if (capacity < 2) return 0;
    memset(buffer, 0, (size_t)capacity);

    while (count < capacity - 1 &&
           (HAL_GetTick() - start) < max_ms)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 20) == HAL_OK)
        {
            buffer[count++] = (char)ch;
            last_byte = HAL_GetTick();
        }
        else if (count > 0 &&
                 (HAL_GetTick() - last_byte) >= quiet_ms)
        {
            break;
        }
    }

    buffer[count] = '\0';
    return count;
}

static void esp_send_text(const char *text)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)text,
                      (uint16_t)strlen(text), ESP_BAUD_TIMEOUT);
}

/* Send one AT command, collect its response, and show it on the debug UART. */
static void esp_command(const char *command, uint32_t wait_ms)
{
    esp_send_text(command);
    if (esp_read_quiet(esp_rx, sizeof(esp_rx), 150, wait_ms) > 0)
    {
        debug_print(esp_rx);
    }
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART6_UART_Init();

    /* USER CODE BEGIN 2 */
    debug_print("\r\nTask 11A: ESP8266 TCP server setup\r\n");
    esp_command("AT\r\n", 1500);
    esp_command("AT+CIPMUX=1\r\n", 1500);
    esp_command("AT+CIPSERVER=1,80\r\n", 1500);
    esp_command("AT+CIFSR\r\n", 1500);
    debug_print("\r\nTCP server configuration sequence complete.\r\n");
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
HAL_Delay(1000);
        /* USER CODE END 3 */
    }
}

/* System Clock Configuration -----------------------------------------------*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 100;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitTypeDef RCC_ClkInitStruct2 = {0};
    RCC_ClkInitStruct2.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct2.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct2.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct2.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct2.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct2, FLASH_LATENCY_3) != HAL_OK)
        Error_Handler();
}

/* GPIO Initialization -------------------------------------------------------*/
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
}

/* USART1 Initialization: PA9 TX, PA10 RX -----------------------------------*/
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

/* USART2 Initialization: PA2 TX, PA3 RX ------------------------------------*/
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* USART6 Initialization: PC6 TX, PC7 RX ------------------------------------*/
static void MX_USART6_UART_Init(void)
{
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
