/* smoke_ctrl.c
 * Sterownik dymu — logika state machine
 */

#include "smoke_ctrl.h"
#include <string.h>

extern CAN_HandleTypeDef hcan;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

/* ---- Stan wewnętrzny ---- */
static SmokeState_t s_state         = SMOKE_STATE_IDLE;
static uint16_t     s_throttle_raw  = CRSF_MIN;
static uint32_t     s_last_can_tick = 0;
//static uint32_t     s_preheat_start = 0;
static uint32_t     s_coast_start   = 0;
static uint8_t      s_can_ever_recv = 0;

/* Duty aktualne — eksponowane przez gettery */
static uint32_t     s_pump_duty     = 0;
static uint32_t     s_heater_duty   = 0;

/* ---- Helpers PWM ---- */
static void set_heater(uint32_t duty)
{
    s_heater_duty = duty;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty);
}

static void set_pump(uint32_t duty)
{
    s_pump_duty = duty;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty);
}

/* ---- Przelicznik throttle → duty pompki ---- */
static uint32_t throttle_to_pump_duty(uint16_t raw)
{
    if (raw <= SMOKE_THR_THRESHOLD)
        return PWM_MIN;

    uint32_t span_in = CRSF_MAX - SMOKE_THR_THRESHOLD;
    uint32_t val     = raw - SMOKE_THR_THRESHOLD;
    uint32_t duty    = (val * PWM_MAX) / span_in;

    if (duty > PWM_MAX) duty = PWM_MAX;
    return duty;
}

/* ---- Callback CAN RX ---- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_h)
{
    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    if (HAL_CAN_GetRxMessage(hcan_h, CAN_RX_FIFO0, &hdr, data) != HAL_OK)
        return;

    s_last_can_tick = HAL_GetTick();
    s_can_ever_recv = 1;

    if (hdr.StdId == CAN_ID_THROTTLE && hdr.DLC >= 2)
    {
        s_throttle_raw = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
    }
}

void Smoke_Init(void)
{
    CAN_FilterTypeDef filter = {0};

    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (CAN_ID_THROTTLE << 5) & 0xFFFF;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = (0x7FF << 5) & 0xFFFF;
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;

    HAL_CAN_ConfigFilter(&hcan, &filter);
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    set_heater(PWM_MIN);
    set_pump(PWM_MIN);

    s_state = SMOKE_STATE_IDLE;
}





/* HTR skalowany do PMP — im więcej pompki, tym więcej grzania.
 * PMP=0   → HTR=600 (HOLD, samo podtrzymanie temperatury)
 * PMP=999 → HTR=850 (MAX, pełny przepływ wymaga więcej ciepła)
 * Kalibruj HEATER_SMOKING_MAX na żywym urządzeniu.
 */
//#define HEATER_SMOKING_MAX 850UL

//static uint32_t heater_from_pump(uint32_t pump_duty)
//{
//    uint32_t span = HEATER_SMOKING_MAX - HEATER_HOLD_DUTY;
//    uint32_t htr  = HEATER_HOLD_DUTY + (span * pump_duty) / PWM_MAX;
//    if (htr > HEATER_SMOKING_MAX) htr = HEATER_SMOKING_MAX;
//    return htr;
//}


/* Nowa funkcja skalująca — HTR i PMP razem, liniowo */
static uint32_t throttle_to_heater_duty(uint16_t raw)
{
    if (raw <= SMOKE_THR_THRESHOLD) return PWM_MIN;
    uint32_t span = CRSF_MAX - SMOKE_THR_THRESHOLD;
    uint32_t val  = raw - SMOKE_THR_THRESHOLD;
    uint32_t duty = (val * PWM_MAX) / span;
    if (duty > PWM_MAX) duty = PWM_MAX;
    return duty;
}

void Smoke_Process(void)
{
    uint32_t now = HAL_GetTick();

    switch (s_state)
    {
    case SMOKE_STATE_IDLE:
        set_heater(PWM_MIN);
        set_pump(PWM_MIN);
        if (s_can_ever_recv) {
            s_state = SMOKE_STATE_READY;
        }
        break;

    case SMOKE_STATE_PREHEAT:
        /* NIEUŻYWANY — przejdź od razu do READY */
        s_state = SMOKE_STATE_READY;
        break;

    case SMOKE_STATE_READY:
        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
            s_state = SMOKE_STATE_SAFE_OFF;
            break;
        }
        set_heater(PWM_MIN);
        set_pump(PWM_MIN);
        if (s_throttle_raw > SMOKE_THR_THRESHOLD) {
            s_state = SMOKE_STATE_SMOKING;
        }
        break;

    case SMOKE_STATE_SMOKING:
        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
            s_state = SMOKE_STATE_SAFE_OFF;
            break;
        }
        if (s_throttle_raw <= SMOKE_THR_THRESHOLD) {
            s_coast_start = now;
            set_pump(PWM_MIN);
            set_heater(PWM_MIN);
            s_state = SMOKE_STATE_COAST;
        } else {
            uint32_t pump_d = throttle_to_pump_duty(s_throttle_raw);
            uint32_t htr_d  = throttle_to_heater_duty(s_throttle_raw);
            set_pump(pump_d);
            set_heater(htr_d);
        }
        break;

    case SMOKE_STATE_COAST:
        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
            s_state = SMOKE_STATE_SAFE_OFF;
            break;
        }
        set_pump(PWM_MIN);
        set_heater(PWM_MIN);
        if (s_throttle_raw > SMOKE_THR_THRESHOLD) {
            s_state = SMOKE_STATE_SMOKING;
            break;
        }
        if ((now - s_coast_start) >= PUMP_COAST_TIME_MS) {
            s_state = SMOKE_STATE_READY;
        }
        break;

    case SMOKE_STATE_SAFE_OFF:
        set_pump(PWM_MIN);
        set_heater(PWM_MIN);
        if (s_can_ever_recv && (now - s_last_can_tick) < CAN_WATCHDOG_MS) {
            s_state = SMOKE_STATE_READY;
        }
        break;

    default:
        s_state = SMOKE_STATE_IDLE;
        break;
    }
}


//void Smoke_Process(void)
//{
//    uint32_t now = HAL_GetTick();
//
//    switch (s_state)
//    {
//    case SMOKE_STATE_IDLE:
//        set_heater(PWM_MIN);
//        set_pump(PWM_MIN);
//
//        if (s_can_ever_recv) {
//            s_preheat_start = now;
//            set_heater(HEATER_PREHEAT_DUTY);
//            s_state = SMOKE_STATE_PREHEAT;
//        }
//        break;
//
//    case SMOKE_STATE_PREHEAT:
//        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
//            s_state = SMOKE_STATE_SAFE_OFF;
//            break;
//        }
//
//        if ((now - s_preheat_start) >= PREHEAT_TIME_MS) {
//            set_heater(HEATER_HOLD_DUTY);
//            s_state = SMOKE_STATE_READY;
//        }
//        break;
//
//    case SMOKE_STATE_READY:
//        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
//            s_state = SMOKE_STATE_SAFE_OFF;
//            break;
//        }
//
//        set_heater(HEATER_HOLD_DUTY);
//        set_pump(PWM_MIN);
//
//        if (s_throttle_raw > SMOKE_THR_THRESHOLD) {
//            s_state = SMOKE_STATE_SMOKING;
//        }
//        break;
//
//
//
//    case SMOKE_STATE_SMOKING:
//        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
//            s_state = SMOKE_STATE_SAFE_OFF;
//            break;
//        }
//
//        if (s_throttle_raw <= SMOKE_THR_THRESHOLD) {
//            s_coast_start = now;
//            set_pump(PWM_MIN);
//            set_heater(HEATER_HOLD_DUTY);
//            s_state = SMOKE_STATE_COAST;
//        } else {
//            uint32_t pump_d = throttle_to_pump_duty(s_throttle_raw);
//            set_pump(pump_d);
//            set_heater(heater_from_pump(pump_d));
//        }
//        break;
//
//
//
//    case SMOKE_STATE_COAST:
//        if ((now - s_last_can_tick) > CAN_WATCHDOG_MS) {
//            s_state = SMOKE_STATE_SAFE_OFF;
//            break;
//        }
//
//        set_pump(PWM_MIN);
//        set_heater(HEATER_HOLD_DUTY);
//
//        if (s_throttle_raw > SMOKE_THR_THRESHOLD) {
//            s_state = SMOKE_STATE_SMOKING;
//            break;
//        }
//
//        if ((now - s_coast_start) >= PUMP_COAST_TIME_MS) {
//            s_state = SMOKE_STATE_READY;
//        }
//        break;
//
//    case SMOKE_STATE_SAFE_OFF:
//        set_pump(PWM_MIN);
//        set_heater(PWM_MIN);
//
//        if (s_can_ever_recv && (now - s_last_can_tick) < CAN_WATCHDOG_MS) {
//            s_preheat_start = now;
//            set_heater(HEATER_PREHEAT_DUTY);
//            s_state = SMOKE_STATE_PREHEAT;
//        }
//        break;
//
//    default:
//        s_state = SMOKE_STATE_IDLE;
//        break;
//    }
//}

/* ---- Gettery ---- */
SmokeState_t Smoke_GetState(void)
{
    return s_state;
}

uint16_t Smoke_GetThrottle(void)
{
    return s_throttle_raw;
}

uint32_t Smoke_GetPumpDuty(void)
{
    return s_pump_duty;
}

uint32_t Smoke_GetHeaterDuty(void)
{
    return s_heater_duty;
}

uint32_t Smoke_GetCanAge(void)
{
    if (!s_can_ever_recv)
        return 9999UL;

    return HAL_GetTick() - s_last_can_tick;
}
