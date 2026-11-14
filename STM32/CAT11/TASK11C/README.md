# EXPERIMENT DETAILS
**EXP:** CONTINUOUS ASSESSMENT 11
**DATE:** 12/10/2026
**NAME OF EXP:** HTTP Server - STM32-Wemos Wi-Fi Modem Using AT Commands
**HARDWARE SETUP / PLATFORM:** STM32CubeMX + Keil MDK + STM32F410RBT6 custom development board + Wemos D1 mini (ESP8266)

---

## TASK11C Node-RED as a Second HTTP Client
### 1. Circuit Diagram
- STM32 development board: custom board described by the supplied CAT10C.ioc configuration.
- MCU: STM32F410RBT6, LQFP64 package.
- PA9 (USART1_TX) -> Wemos D1 mini RXD0 / GPIO3.
- PA10 (USART1_RX) <- Wemos D1 mini TXD0 / GPIO1.
- PC6 (USART6_TX) -> USB-UART adapter RX for debug output (assumed, optional).
- PC7 (USART6_RX) is configured by the IOC but is not used by this firmware; no wire is required.
- STM32 GND and Wemos GND must be connected together.
- Wemos 5V pin is powered from a regulated 5V supply (assumed); UART signals are 3.3V logic.
- The STM32 board is powered separately according to its board requirements (assumed).
![Circuit Diagram](circuit.png)
### 2. Core Logic / Pseudocode / Code Base
**Working Principle:** The STM32 firmware runs the same ESP8266-backed HTTP server as Task 11B. Node-RED acts as a second HTTP client: an Inject node triggers an HTTP Request node pointed at the ESP8266 IP address, and a Debug node displays the returned HTML response.

**Pseudocode:**
1. Initialize HAL, clock, GPIO and UART peripherals.
2. Configure the ESP8266 for multiple TCP connections, start the TCP server on port 80, and query its IP address.
3. Keep receiving UART data and detect +IPD requests from browser or Node-RED clients.
4. Extract the connection ID and construct the static HTTP response.
5. Send the response using the two-stage CIPSEND prompt/data sequence.
6. Close the client connection after sending the response.
7. In Node-RED, trigger an HTTP Request to http://<ESP8266_IP>/ using Inject and display the reply in Debug.

**Code Base:** `HAL_Init`, `SystemClock_Config`, `MX_GPIO_Init`, `MX_USART1_UART_Init`, `MX_USART2_UART_Init`, and `MX_USART6_UART_Init` initialize the STM32F410. USART1 carries ESP8266 AT commands and modem data at 115200 baud, 8 data bits, no parity and 1 stop bit. USART6 provides debug text through PC6 at 115200 baud; PC7 is configured but unused. The application uses `HAL_UART_Transmit`, `HAL_UART_Receive`, `HAL_GetTick`, `memset`, `snprintf`, `strlen`, `strstr`, and `atoi`.
### 3. Inference
This task demonstrates that the STM32/ESP8266 HTTP server can serve more than one HTTP client. Node-RED independently requests the endpoint and displays the response returned by the STM32.

**Assumptions and items to confirm:**
- The supplied `CAT10C.ioc` is used as the base configuration. It specifies STM32F410RBT6, USART1 on PA9/PA10, USART2 on PA2/PA3, and USART6 on PC6/PC7.
- USART1 is connected to the Wemos UART and USART6 is used for a serial debug terminal, following the supplied CAT10 HTTP-server code pattern (assumed).
- The ESP8266 is already connected to a Wi-Fi access point before `AT+CIPSERVER=1,80` is issued (assumed). This document does not provide Wi-Fi credentials.
- The Wemos 5V supply arrangement and optional USB-UART debug adapter are assumed; verify the actual board wiring and common ground before powering.
- [FILL: Wi-Fi SSID and password, if the ESP8266 has not already joined the network.]
- [FILL: ESP8266 IP address shown by `AT+CIFSR`, for browser and Node-RED URL configuration.]
- The IOC identifies a custom STM32F410 board, not a Blue Pill STM32F103 board. The supplied Blue Pill picture is therefore not used as the MCU board drawing.
- Firmware has not been compiled in this environment because the STM32CubeF4 HAL project headers and ARM toolchain are not available here.
