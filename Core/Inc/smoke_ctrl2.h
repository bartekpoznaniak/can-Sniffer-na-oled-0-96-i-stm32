/* smoke_ctrl.h
 * Sterownik dymu — STM32F103C8T6
 * CAN1 @ 500kbps (PB8/PB9 + SN65HVD230)
 * Grzałka: PA5  → TIM2_CH1  (PWM)
 * Pompka:  PB15 → TIM1_CH3N (PWM, complementary)
 */

#ifndef SMOKE_CTRL_H
#define SMOKE_CTRL_H

#include "main.h"
#include <stdint.h>

/* ============================================================
 *  CAN
 * ============================================================ */
#define CAN_ID_THROTTLE         0x130UL

/* ============================================================
 *  CRSF raw range
 * ============================================================ */
#define CRSF_MIN                172
#define CRSF_MID                992
#define CRSF_MAX                1810

/* ============================================================
 *  Progi dymu
 *  SMOKE_THR_THRESHOLD — poniżej: pompka off
 *  (~10% = 172 + (1810-172)*0.10 = 336)
 * ============================================================ */
#define SMOKE_THR_THRESHOLD     340

/* ============================================================
 *  PWM — wartości wypełnienia (0–1000 = 0–100%)
 *  Timer skonfigurowany na ARR=999, więc CCR=0..999
 * ============================================================ */
#define PWM_MAX                 999
#define PWM_MIN                 0

/* Grzałka podczas preheat (np. 80% mocy) */
#define HEATER_PREHEAT_DUTY     800

/* Grzałka podczas pracy (podtrzymanie temp, np. 60%) */
#define HEATER_HOLD_DUTY        600

/* Czas wstępnego nagrzewania [ms] — do kalibracji doświadczalnie */
#define PREHEAT_TIME_MS         8000UL

/* Czas wybiegu pompki po spadku throttle [ms] */
#define PUMP_COAST_TIME_MS      1500UL

/* ============================================================
 *  Watchdog CAN — czas bez jakiejkolwiek ramki
 *  Ustawiamy duży bo przy nieruchomym throttle CAN milczy
 *  (Pico wysyła tylko przy zmianie)
 * ============================================================ */
#define CAN_WATCHDOG_MS         3000UL

/* ============================================================
 *  State machine
 * ============================================================ */
typedef enum {
    SMOKE_STATE_IDLE        = 0,  /* brak CAN lub system nieaktywny */
    SMOKE_STATE_PREHEAT     = 1,  /* grzałka się nagrzewa, pompka off */
    SMOKE_STATE_READY       = 2,  /* gotowy, czeka na throttle > próg */
    SMOKE_STATE_SMOKING     = 3,  /* dym aktywny, pompka prop. do THR */
    SMOKE_STATE_COAST       = 4,  /* throttle spadł, pompka wybiega */
    SMOKE_STATE_SAFE_OFF    = 5,  /* utrata CAN > watchdog, bezpieczne wył. */
} SmokeState_t;

/* ============================================================
 *  Publiczne API
 * ============================================================ */
void Smoke_Init(void);
void Smoke_Process(void);         /* wywołuj w main loop */
SmokeState_t Smoke_GetState(void);
uint16_t Smoke_GetThrottle(void); /* ostatnia odebrana wartość raw CRSF */

#endif /* SMOKE_CTRL_H */
