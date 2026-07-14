/* smoke_ctrl.h
 * Sterownik dymu — STM32F103C8T6
 * CAN1 @ 500kbps (PB8/PB9 + SN65HVD230)
 * Grzałka : PA5  → TIM2_CH1  (PWM)
 * Pompka  : PB15 → TIM1_CH3N (PWM, complementary)
 */

#ifndef SMOKE_CTRL_H
#define SMOKE_CTRL_H

#include "main.h"
#include <stdint.h>

/* ---- CAN IDs ---- */
#define CAN_ID_THROTTLE     0x130UL

/* ---- CRSF zakres raw ---- */
#define CRSF_MIN            172
#define CRSF_MID            992
#define CRSF_MAX            1810

/* ---- Próg startu dymu (~10% zakresu) ---- */
#define SMOKE_THR_THRESHOLD 340

/* ---- PWM (CCR 0–999) ---- */
#define PWM_MAX             999
#define PWM_MIN             0

///* ---- Duty grzałki ---- */
//#define HEATER_PREHEAT_DUTY 800
//#define HEATER_HOLD_DUTY    600
#define HEATER_SMOKING_MAX  999UL
/* ---- Czasy [ms] ---- */
#define PREHEAT_TIME_MS     1UL
#define PUMP_COAST_TIME_MS  1500UL

/* ---- Watchdog CAN ---- */
#define CAN_WATCHDOG_MS     3000UL

/* ---- Stany ---- */
typedef enum {
    SMOKE_STATE_IDLE     = 0,
    SMOKE_STATE_PREHEAT  = 1,
    SMOKE_STATE_READY    = 2,
    SMOKE_STATE_SMOKING  = 3,
    SMOKE_STATE_COAST    = 4,
    SMOKE_STATE_SAFE_OFF = 5,
} SmokeState_t;

/* ---- Publiczne API ---- */
void         Smoke_Init(void);
void         Smoke_Process(void);
SmokeState_t Smoke_GetState(void);
uint16_t     Smoke_GetThrottle(void);
uint32_t     Smoke_GetCanAge(void);
uint32_t     Smoke_GetPumpDuty(void);
uint32_t     Smoke_GetHeaterDuty(void);

#endif /* SMOKE_CTRL_H */
