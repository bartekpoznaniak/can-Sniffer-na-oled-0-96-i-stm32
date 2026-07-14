
/* ============================================================
 *  Orientacja wyświetlacza — odkomentuj żeby obrócić o 180°
 * ============================================================ */
#define SSD1306_MIRROR_VERT    // ← DODAJ
#define SSD1306_MIRROR_HORIZ   // ← DODAJ

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

/* Rodzina MCU */
#define STM32F1

/* Interfejs */
#define SSD1306_USE_I2C
#define SSD1306_I2C_PORT hi2c1
#define SSD1306_I2C_ADDR (0x3C << 1)

/* Fonty, których chcesz używać */
#define SSD1306_INCLUDE_FONT_7x10
/* opcjonalnie:*/
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_11x18
/**/

/* Rozmiar ekranu (dla typowego 0.96" SSD1306) */
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#endif /* __SSD1306_CONF_H__ */
