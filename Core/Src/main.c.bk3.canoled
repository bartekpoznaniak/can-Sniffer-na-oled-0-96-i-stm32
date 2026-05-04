/* ============================================================
 *  CAN NODE TEMPLATE — węzeł oświetlenia
 *  Aby dodać nowy węzeł: zmień TYLKO MY_DEVICE_ID poniżej
 *  Aby dodać nową komendę: dodaj case w can_dispatch() i wypełnij handler
 * ============================================================ */

#include "main.h"
#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"



#define HAL_I2C_MODULE_ENABLED

/* ============================================================
 *  Adres Displaya
 * ============================================================ */

#define SSD1306_I2C_ADDR (0x3C << 1)

/* ============================================================
 *  KONFIGURACJA WĘZŁA — jedyna linia którą zmieniasz per urządzenie
 * ============================================================ */
#define MY_DEVICE_ID    0u      // 0–39 — unikalny adres węzła

/* ============================================================
 *  Schemat adresacji CAN (automatyczny na podstawie MY_DEVICE_ID)
 *
 *  Węzeł N nasłuchuje na dwóch ID:
 *    CAN_ID_OSW1 = 0x110 + N*2       np. węzeł 0→0x110, węzeł 1→0x112
 *    CAN_ID_OSW2 = 0x110 + N*2 + 1   np. węzeł 0→0x111, węzeł 1→0x113
 *
 *  Filtr akceptuje oba ID jednocześnie (różnią się bitem 0)
 * ============================================================ */
#define CAN_ID_OSW1     (0x110u + MY_DEVICE_ID * 2u)
#define CAN_ID_OSW2     (0x110u + MY_DEVICE_ID * 2u + 1u)

/* Piny wyjściowe */
#define OSW1_PORT       GPIOA
#define OSW1_PIN        GPIO_PIN_1
#define OSW2_PORT       GPIOA
#define OSW2_PIN        GPIO_PIN_4

/* ============================================================
 *  Handles HAL
 * ============================================================ */
CAN_HandleTypeDef  hcan;
UART_HandleTypeDef huart2;
I2C_HandleTypeDef hi2c1;


/* USER CODE BEGIN PV */
char    uart_buf[64];
volatile uint8_t  led_blinks = 0;
volatile uint16_t led_delay  = 200;
/* USER CODE END PV */

/* Prototypy */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);
void uart_print(const char *msg);
static void MX_I2C1_Init(void);

//FUNKCJA DO WYŚWIETLANIA NA OLED
void oled_show(const char *line_top, const char *line_bot)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(line_top, Font_7x10, White);

    ssd1306_SetCursor(0, 14);
    ssd1306_WriteString(line_bot, Font_7x10, White);

    ssd1306_UpdateScreen();
}

/* ============================================================
 *  HANDLERY WYKONAWCZE — wypełnij co węzeł ma robić
 *  Tutaj jedyna logika specyficzna dla danego zastosowania
 * ============================================================ */

void on_osw1_on(void)
{
    HAL_GPIO_WritePin(OSW1_PORT, OSW1_PIN, GPIO_PIN_SET);
    /* === TU DODAJ WŁASNĄ LOGIKĘ === */
    // np. HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // ściemniacz
    // np. set_relay(RELAY_1, ON);
    led_blinks = 1;
    led_delay  = 300;
    uart_print(">>> OSW1 ON  <<<\r\n");
    oled_show("OSW1", "ON");
}

void on_osw1_off(void)
{
    HAL_GPIO_WritePin(OSW1_PORT, OSW1_PIN, GPIO_PIN_RESET);
    /* === TU DODAJ WŁASNĄ LOGIKĘ === */
    led_blinks = 2;
    led_delay  = 300;
    uart_print(">>> OSW1 OFF <<<\r\n");
    oled_show("OSW1", "OFF");
}

void on_osw2_on(void)
{
    HAL_GPIO_WritePin(OSW2_PORT, OSW2_PIN, GPIO_PIN_SET);
    /* === TU DODAJ WŁASNĄ LOGIKĘ === */
    led_blinks = 1;
    led_delay  = 80;
    uart_print(">>> OSW2 ON  <<<\r\n");
    oled_show("OSW2", "ON");
}

void on_osw2_off(void)
{
    HAL_GPIO_WritePin(OSW2_PORT, OSW2_PIN, GPIO_PIN_RESET);
    /* === TU DODAJ WŁASNĄ LOGIKĘ === */
    led_blinks = 2;
    led_delay  = 80;
    uart_print(">>> OSW2 OFF <<<\r\n");
    oled_show("OSW2", "OFF");
}

/* ============================================================
 *  DISPATCHER CAN — routing ramek do handlerów
 *  Dodanie nowej komendy = nowy case + nowa funkcja handler
 * ============================================================ */
static void can_dispatch(uint32_t can_id, uint8_t *data)
{
    snprintf(uart_buf, sizeof(uart_buf),
             "CAN RX: ID=0x%03lX [%02X]\r\n", can_id, data[0]);
    uart_print(uart_buf);

    if (can_id == CAN_ID_OSW1)
    {
        (data[0] == 0x01) ? on_osw1_on() : on_osw1_off();
    }
    else if (can_id == CAN_ID_OSW2)
    {
        (data[0] == 0x01) ? on_osw2_on() : on_osw2_off();
    }
    /* --- Tutaj dodajesz kolejne komendy: ---
    else if (can_id == CAN_ID_DIMMER)   { on_dimmer(data); }
    else if (can_id == CAN_ID_COLOR)    { on_color(data);  }
    */
}

/* ============================================================
 *  CALLBACK CAN RX — tylko odbiera, przekazuje do dispatchera
 * ============================================================ */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_h)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    if (HAL_CAN_GetRxMessage(hcan_h, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
        return;
    can_dispatch(RxHeader.StdId, RxData);
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void)
{



    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_CAN_Init();


    MX_I2C1_Init();


    HAL_Delay(100);

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);

    char line1[] = "CAN NODE INIT";
    ssd1306_WriteString(line1, Font_7x10, White);

    ssd1306_SetCursor(0, 16);
    char line2[] = "OLED HAL OK";
    ssd1306_WriteString(line2, Font_7x10, White);

    ssd1306_UpdateScreen();


    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(line1, Font_7x10, White);
    ssd1306_UpdateScreen();


//WYŚWIETLANIE CAN NA OLED

    char line_1[22];
    char line_2[22];

    snprintf(line_1, sizeof(line_1), "CAN NODE #%u", MY_DEVICE_ID);
    snprintf(line_2, sizeof(line_2), "0x%03X / 0x%03X", CAN_ID_OSW1, CAN_ID_OSW2);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(line_1, Font_7x10, White);

    ssd1306_SetCursor(0, 14);
    ssd1306_WriteString(line_2, Font_7x10, White);

    ssd1306_UpdateScreen();
//    char line_3[22];  // 128px / 7px szerokość znaku = ~18 znaków max
//    snprintf(line_3, sizeof(line_3), "NODE#%u 0x%03X/0x%03X",
//             MY_DEVICE_ID, CAN_ID_OSW1, CAN_ID_OSW2);
//
//    ssd1306_SetCursor(0, 0);
//    ssd1306_WriteString(line_3, Font_7x10, White);
//
//    ssd1306_SetCursor(0, 16);
//

    snprintf(uart_buf, sizeof(uart_buf),
             "=== CAN NODE #%u | OSW1=0x%03X OSW2=0x%03X ===\r\n",
             MY_DEVICE_ID, CAN_ID_OSW1, CAN_ID_OSW2);
    uart_print(uart_buf);

    /* Filtr CAN: akceptuje CAN_ID_OSW1 i CAN_ID_OSW2
     * Oba ID różnią się tylko bitem 0 → maska 0x7FE pokrywa oba jednocześnie */
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (CAN_ID_OSW1 << 5);
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = (0x7FEu << 5);   // bit0 wolny → oba ID przechodzą
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &filter);

    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    while (1)
    {
        if (led_blinks > 0)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            HAL_Delay(led_delay);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_Delay(led_delay);
            led_blinks--;
        }
    }
}

/* ============================================================
 *  Init functions — bez zmian
 * ============================================================ */
void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState        = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState        = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL      = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_CAN_Init(void)
{
    hcan.Instance                  = CAN1;
    hcan.Init.Prescaler            = 4;
    hcan.Init.Mode                 = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1             = CAN_BS1_15TQ;
    hcan.Init.TimeSeg2             = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode    = DISABLE;
    hcan.Init.AutoBusOff           = DISABLE;
    hcan.Init.AutoWakeUp           = DISABLE;
    hcan.Init.AutoRetransmission   = ENABLE;
    hcan.Init.ReceiveFifoLocked    = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}





static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Ustaw stan początkowy */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);                  // LED na Blue Pill (aktywny niski)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1 | GPIO_PIN_4, GPIO_PIN_RESET);    // OSW1=PA1, OSW2=PA4

  /* PC13 jako wyjście */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PA1 i PA4 jako wyjścia */
  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}


static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLED;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}


void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
