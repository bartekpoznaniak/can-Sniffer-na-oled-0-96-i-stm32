/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : main.c
* @brief          : Sterownik dymu — Blue Pill STM32F103C8T6
* CAN1 500kbps (PB8/PB9), SSD1306 I2C1 (PB6/PB7)
* UART3 debug (PB10/PB11 115200), TIM2/TIM1 PWM
******************************************************************************
*/
/* USER CODE END Header */

#include "main.h"
#include "smoke_ctrl.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * HAL handles — globalne (używane też w smoke_ctrl.c)
 * ============================================================ */
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart3;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1; /* PB15 — TIM1_CH3N (pompka) */
TIM_HandleTypeDef htim2; /* PA5  — TIM2_CH1  (grzałka) */

/* ============================================================
 * Prototypy funkcji init
 * ============================================================ */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_CAN_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Init(void);

/* ============================================================
 * Helpers — UART i OLED
 * ============================================================ */

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), 200);
}

static const char *state_names[] = {
    "IDLE   ",
    "PREHEAT",
    "READY  ",
    "SMOKING",
    "COAST  ",
    "SAFEOFF"
};

static void oled_update(void)
{
    SmokeState_t st  = Smoke_GetState();
    uint16_t     thr = Smoke_GetThrottle();
    uint32_t     age = Smoke_GetCanAge();
    uint32_t     pmp = Smoke_GetPumpDuty();
    uint32_t     htr = Smoke_GetHeaterDuty();

    char line[22];

    ssd1306_Fill(Black);

    snprintf(line, sizeof(line), "ST:%s", state_names[st]);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(line, Font_7x10, White);

    if (age > 9999) age = 9999;
    snprintf(line, sizeof(line), "HTR:%4u CAN:%4lums", thr, age);
    ssd1306_SetCursor(0, 12);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "PMP:%3lu HTR:%3lu", pmp, htr);
    ssd1306_SetCursor(0, 24);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "UP:%5lus", HAL_GetTick() / 1000UL);
    ssd1306_SetCursor(0, 36);
    ssd1306_WriteString(line, Font_7x10, White);

    ssd1306_UpdateScreen();
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_TIM2_Init();
    MX_TIM1_Init();
    MX_CAN_Init();
    MX_I2C1_Init();

    HAL_Delay(50);

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("SMOKER BOOT", Font_7x10, White);
    ssd1306_UpdateScreen();

    uart_print("=== SMOKER BOOT ===\r\n");

    Smoke_Init();
    uart_print("Smoke_Init OK\r\n");

    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(100);
    }

    uart_print("Entering main loop\r\n");

    uint32_t last_oled_tick = 0;
    uint32_t last_uart_tick = 0;

    while (1)
    {
        Smoke_Process();

        uint32_t now = HAL_GetTick();

        if ((now - last_oled_tick) >= 250UL)
        {
            last_oled_tick = now;
            oled_update();
        }

        if ((now - last_uart_tick) >= 1000UL)
        {
            last_uart_tick = now;

            SmokeState_t st  = Smoke_GetState();
            uint16_t     thr = Smoke_GetThrottle();
            uint32_t     pmp = Smoke_GetPumpDuty();
            uint32_t     htr = Smoke_GetHeaterDuty();
            uint32_t     age = Smoke_GetCanAge();

            char buf[96];
            snprintf(buf, sizeof(buf),
                     "ST:%u THR:%4u PMP:%3lu HTR:%3lu CAN:%4lums UP:%lus\r\n",
                     (unsigned)st, thr, pmp, htr, age, now / 1000UL);
            uart_print(buf);
        }
    }
}

/* ============================================================
 * SystemClock_Config — HSE 8MHz, PLL x9 → 72 MHz
 * ============================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}








static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PC13 — LED */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

//    /* PA5 — TIM2_CH1 (grzałka, partial remap 1) — MUSI być AF_PP */
//    GPIO_InitStruct.Pin   = GPIO_PIN_5;
//    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull  = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA0 — TIM2_CH1 (grzałka) — domyślny pin, bez remapa */
    GPIO_InitStruct.Pin   = GPIO_PIN_0;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB15 — TIM1_CH3N (pompka) — MUSI być AF_PP */
    GPIO_InitStruct.Pin   = GPIO_PIN_15;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


}






static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
//    __HAL_RCC_AFIO_CLK_ENABLE();
//    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_TIM2_REMAP)
//               | AFIO_MAPR_TIM2_REMAP_PARTIALREMAP1;

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    //oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCPolarity = TIM_OCPOLARITY_LOW;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

static void MX_TIM1_Init(void)
{
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 71;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 999;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) Error_Handler();

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
