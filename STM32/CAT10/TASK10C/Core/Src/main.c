/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 + ESP8266 HTTP Server using AT Commands
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* Function prototypes -------------------------------------------------------*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
void Error_Handler(void);

/* Configuration -------------------------------------------------------------*/

#define RX_BUF_SIZE       1024
#define HTTP_BUF_SIZE     512
#define ESP_TIMEOUT       1000

/* Buffers -------------------------------------------------------------------*/

static char rx_buffer[RX_BUF_SIZE];
static char http_response[HTTP_BUF_SIZE];
static char at_command[64];

/* UART debug output ---------------------------------------------------------*/

static void debug_print(const char *text)
{
    HAL_UART_Transmit(&huart6,
                      (uint8_t *)text,
                      strlen(text),
                      HAL_MAX_DELAY);
}

/* Send an AT command --------------------------------------------------------*/

static void esp_send_command(const char *command)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)command,
                      strlen(command),
                      2000);
}

/* Receive UART data until timeout -------------------------------------------*/

static int esp_receive(char *buffer,
                       int max_length,
                       uint32_t timeout)
{
    uint8_t ch;
    int count = 0;
    uint32_t last_byte_time = HAL_GetTick();

    if (max_length <= 1)
        return 0;

    memset(buffer, 0, max_length);

    while (count < max_length - 1)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 20) == HAL_OK)
        {
            buffer[count++] = (char)ch;
            last_byte_time = HAL_GetTick();

            HAL_UART_Transmit(&huart6, &ch, 1, 100);

            if (ch == '>')
                break;
        }

        if ((HAL_GetTick() - last_byte_time) >= timeout)
            break;
    }

    buffer[count] = '\0';

    return count;
}

/* Send a complete HTTP response --------------------------------------------*/

static void send_http_response(int link_id)
{
    char body[] =
        "<html><body>"
        "<h1>STM32 IoT Node</h1>"
        "<p>Hello from STM32</p>"
        "</body></html>";

    int body_length = strlen(body);

    int response_length = snprintf(
        http_response,
        sizeof(http_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        body_length,
        body
    );

    if (response_length <= 0 ||
        response_length >= sizeof(http_response))
    {
        debug_print("\r\nHTTP response error\r\n");
        return;
    }

    /* STEP 1: Tell ESP8266 the response length. */

    snprintf(at_command,
             sizeof(at_command),
             "AT+CIPSEND=%d,%d\r\n",
             link_id,
             response_length);

    esp_send_command(at_command);

    /* STEP 2: Wait for the ESP8266 prompt. */

    char prompt_buffer[128];

    int count = esp_receive(prompt_buffer,
                            sizeof(prompt_buffer),
                            5000);

    if (count == 0 || strchr(prompt_buffer, '>') == NULL)
    {
        debug_print("\r\nCIPSEND prompt timeout\r\n");
        return;
    }

    /* STEP 3: Transmit the HTTP response. */

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)http_response,
                      response_length,
                      5000);

    /* STEP 4: Read SEND OK or ERROR. */

    char result_buffer[128];

    esp_receive(result_buffer,
                sizeof(result_buffer),
                2000);

    /* STEP 5: Close the client connection. */

    snprintf(at_command,
             sizeof(at_command),
             "AT+CIPCLOSE=%d\r\n",
             link_id);

    esp_send_command(at_command);

    char close_buffer[64];

    esp_receive(close_buffer,
                sizeof(close_buffer),
                1000);
}

/* Configure the ESP8266 -----------------------------------------------------*/

static void ESP8266_Init(void)
{
    debug_print("\r\nInitializing ESP8266...\r\n");

    /* Test AT communication. */

    esp_send_command("AT\r\n");
    esp_receive(rx_buffer, sizeof(rx_buffer), ESP_TIMEOUT);

    /* Station mode. */

    esp_send_command("AT+CWMODE=1\r\n");
    esp_receive(rx_buffer, sizeof(rx_buffer), ESP_TIMEOUT);

    /* Connect to Wi-Fi. */

    debug_print("\r\nConnecting to Wi-Fi...\r\n");

    esp_send_command("AT+CWJAP=\"SNUC\",\"snu12345\"\r\n");

    esp_receive(rx_buffer, sizeof(rx_buffer), 20000);

    /* Display IP address. */

    debug_print("\r\nESP8266 IP address:\r\n");

    esp_send_command("AT+CIFSR\r\n");
    esp_receive(rx_buffer, sizeof(rx_buffer), 2000);

    /* Enable multiple TCP connections. */

    esp_send_command("AT+CIPMUX=1\r\n");
    esp_receive(rx_buffer, sizeof(rx_buffer), ESP_TIMEOUT);

    /* Start TCP server on port 80. */

    esp_send_command("AT+CIPSERVER=1,80\r\n");
    esp_receive(rx_buffer, sizeof(rx_buffer), ESP_TIMEOUT);

    debug_print("\r\nHTTP server configured on port 80\r\n");
}

/* Main ----------------------------------------------------------------------*/

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART6_UART_Init();

    ESP8266_Init();

    debug_print("\r\nWaiting for HTTP requests...\r\n");

    while (1)
    {
        memset(rx_buffer, 0, sizeof(rx_buffer));

        int received = esp_receive(rx_buffer,
                                   sizeof(rx_buffer),
                                   ESP_TIMEOUT);

        if (received > 0)
        {
            /*
             * Example:
             * +IPD,0,342:GET / HTTP/1.1
             *
             * The number immediately following +IPD,
             * is the TCP connection ID in CIPMUX mode.
             */

            char *ipd = strstr(rx_buffer, "+IPD,");

            if (ipd != NULL)
            {
                int link_id = atoi(ipd + 5);

                if (link_id >= 0 && link_id <= 4)
                {
                    debug_print("\r\nHTTP request detected\r\n");

                    send_http_response(link_id);
                }
            }
        }
    }
}

/* System Clock Configuration -----------------------------------------------*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1);

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

    RCC_ClkInitTypeDef RCC_ClkInitStruct2 = {0};

    RCC_ClkInitStruct2.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct2.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct2.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct2.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct2.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct2,
                            FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART1 Initialization -----------------------------------------------------*/

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

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART2 Initialization -----------------------------------------------------*/

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

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART6 Initialization -----------------------------------------------------*/

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

    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Error_Handler();
    }
}

/* GPIO Initialization -------------------------------------------------------*/

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* Error Handler -------------------------------------------------------------*/

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
