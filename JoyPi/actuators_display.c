/************************************************************
 * Projet      : Fusée
 * Fichier     : actuators_display.c
 * Description : Afficheurs JoyPi : matrice LED 8x8 MAX7219 (SPI CE1),
 *               7-segments HT16K33 (I2C 0x70), LCD MCP23017 (I2C 0x21).
 *               LED et buzzer dans actuators.c.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#include "actuators.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringPiSPI.h>
#endif

/* ------------------------------------------------------------------ */
/* Constantes hardware                                                 */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
#define SPI_CHAN_CE1    1
#define SPI_SPEED_HZ    500000
#define SEG_I2C_ADDR    0x70
#define LCD_I2C_ADDR    0x21
#define MCP_IODIR_REG   0x00
#define MCP_GPIO_REG    0x09
#endif

/* ------------------------------------------------------------------ */
/* État global                                                         */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
static int g_spi_ready    = 0;
static int g_seg_fd       = -1;
static int g_lcd_fd       = -1;
static int g_lcd_backlight = 0x80;
static int g_lcd_rs        = 0x00;
#endif

/* ================================================================== */
/* Matrice LED 8x8 — MAX7219 via SPI CE1                              */
/* ================================================================== */

#ifdef USE_WIRINGPI
static void max7219_write(unsigned char reg, unsigned char val) {
    unsigned char buf[2] = { reg, val };
    (void)wiringPiSPIDataRW(SPI_CHAN_CE1, buf, 2);
}

static void matrix_init_spi(void) {
    if (g_spi_ready) return;
    if (wiringPiSPISetupMode(SPI_CHAN_CE1, SPI_SPEED_HZ, 0) < 0) return;
    max7219_write(0x0F, 0x00);
    max7219_write(0x0C, 0x01);
    max7219_write(0x0B, 0x07);
    max7219_write(0x09, 0x00);
    max7219_write(0x0A, 0x08);
    max7219_write(0x01, 0x00); max7219_write(0x02, 0x00);
    max7219_write(0x03, 0x00); max7219_write(0x04, 0x00);
    max7219_write(0x05, 0x00); max7219_write(0x06, 0x00);
    max7219_write(0x07, 0x00); max7219_write(0x08, 0x00);
    g_spi_ready = 1;
}

static void matrix_draw(const unsigned char rows[8]) {
    int i;
    matrix_init_spi();
    if (!g_spi_ready) return;
    for (i = 0; i < 8; i++) {
        max7219_write((unsigned char)(i + 1), rows[i]);
    }
}
#endif

void actuator_matrix_launch(void) {
#ifdef USE_WIRINGPI
    static const unsigned char frames[8][8] = {
        {0x00, 0x00, 0x08, 0x1C, 0x08, 0x00, 0x00, 0x00},
        {0x00, 0x08, 0x1C, 0x3E, 0x1C, 0x08, 0x00, 0x00},
        {0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x1C, 0x08, 0x00},
        {0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08, 0x00},
        {0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x7F, 0x1C, 0x00},
        {0x1C, 0x3E, 0x7F, 0xFF, 0x7F, 0x3E, 0x1C, 0x00},
        {0x08, 0x1C, 0x3E, 0x7F, 0xFF, 0x7F, 0x3E, 0x08},
        {0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08},
    };
    int i;
    for (i = 0; i < 8; i++) {
        matrix_draw(frames[i]);
        usleep(90000);
    }
#else
    static const char *frames[] = {
        "[MATRIX]   ^   ",
        "[MATRIX]  /|\\  ",
        "[MATRIX] /_|_\\ ",
        "[MATRIX]  |||  "
    };
    int i;
    for (i = 0; i < 8; i++) {
        printf("\n%s", frames[i % 4]);
        fflush(stdout);
        usleep(90000);
    }
    printf("\n");
    fflush(stdout);
#endif
}

void actuator_matrix_emergency(void) {
#ifdef USE_WIRINGPI
    static const unsigned char x_pat[8] = {
        0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81
    };
    static const unsigned char blank[8] = {0,0,0,0,0,0,0,0};
    int i;
    for (i = 0; i < 6; i++) {
        matrix_draw((i % 2 == 0) ? x_pat : blank);
        usleep(120000);
    }
#else
    int i;
    for (i = 0; i < 6; i++) {
        printf("\r%s", (i % 2 == 0) ? "[URGENCE!!!]" : "            ");
        fflush(stdout);
        usleep(120000);
    }
    printf("\n");
    fflush(stdout);
#endif
}

void actuator_matrix_clear(void) {
#ifdef USE_WIRINGPI
    static const unsigned char blank[8] = {0,0,0,0,0,0,0,0};
    matrix_draw(blank);
#else
    printf("[MATRIX] clear\n");
    fflush(stdout);
#endif
}

/* ================================================================== */
/* 7-segments HT16K33 — I2C 0x70                                     */
/* ================================================================== */

#ifdef USE_WIRINGPI
static void seg_init(void) {
    if (g_seg_fd >= 0) return;
    g_seg_fd = wiringPiI2CSetupInterface("/dev/i2c-1", SEG_I2C_ADDR);
    if (g_seg_fd < 0)
        g_seg_fd = wiringPiI2CSetupInterface("/dev/i2c-2", SEG_I2C_ADDR);
    if (g_seg_fd < 0) return;
    (void)wiringPiI2CWrite(g_seg_fd, 0x21);
    (void)wiringPiI2CWrite(g_seg_fd, 0x81);
    (void)wiringPiI2CWrite(g_seg_fd, 0xEF);
}
#endif

void actuator_segment_show(int value) {
    int n = value;
    if (n < 0)    n = 0;
    if (n > 9999) n = 9999;

    int d0 = (n / 1000) % 10;
    int d1 = (n /  100) % 10;
    int d2 = (n /   10) % 10;
    int d3 =  n         % 10;

#ifdef USE_WIRINGPI
    static const unsigned char digits[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    seg_init();
    if (g_seg_fd < 0) {
        printf("[7SEG] (i2c indisponible) value=%d\n", value);
        fflush(stdout);
        return;
    }
    unsigned char buf[8] = {0};
    buf[0] = digits[d2];
    buf[1] = digits[d3];
    buf[2] = digits[d0];
    buf[3] = digits[d1];
    int i;
    for (i = 0; i < 8; i++) {
        (void)wiringPiI2CWriteReg8(g_seg_fd, i * 2,     buf[i]);
        (void)wiringPiI2CWriteReg8(g_seg_fd, i * 2 + 1, 0x00);
    }
#else
    printf("[7SEG] %d%d%d%d  (valeur=%d)\n", d0, d1, d2, d3, value);
    fflush(stdout);
#endif
}

/* ================================================================== */
/* LCD MCP23017 — I2C 0x21                                           */
/* ================================================================== */

#ifdef USE_WIRINGPI
static void lcd_command(int data);

static void lcd_init(void) {
    if (g_lcd_fd >= 0) return;
    g_lcd_fd = wiringPiI2CSetupInterface("/dev/i2c-1", LCD_I2C_ADDR);
    if (g_lcd_fd < 0)
        g_lcd_fd = wiringPiI2CSetupInterface("/dev/i2c-2", LCD_I2C_ADDR);
    if (g_lcd_fd < 0) return;
    g_lcd_backlight = 0x80;
    g_lcd_rs        = 0x00;
    (void)wiringPiI2CWriteReg8(g_lcd_fd, MCP_IODIR_REG, 0x00);
    lcd_command(0x33);
    lcd_command(0x32);
    lcd_command(0x0C);
    lcd_command(0x28);
    lcd_command(0x06);
    lcd_command(0x01);
    delay(2);
    lcd_command(0x80);
}

static void lcd_write_bus(int data) {
    int enabled = data | 0x04;
    (void)wiringPiI2CWriteReg8(g_lcd_fd, MCP_GPIO_REG, enabled);
    delayMicroseconds(1);
    (void)wiringPiI2CWriteReg8(g_lcd_fd, MCP_GPIO_REG, data);
    delayMicroseconds(50);
}

static void lcd_set(int data) {
    int d = ((data & 0xF0) >> 1) | g_lcd_backlight | g_lcd_rs;
    lcd_write_bus(d);
}

static void lcd_command(int data) {
    g_lcd_rs = 0x00;
    lcd_set(data);
    lcd_set(data << 4);
}

static void lcd_send_data(int data) {
    g_lcd_rs = 0x02;
    lcd_set(data);
    lcd_set(data << 4);
}

static void lcd_print(const char *s) {
    while (*s) lcd_send_data((unsigned char)*s++);
}
#endif

void actuator_lcd_show(const char *line1, const char *line2) {
#ifdef USE_WIRINGPI
    lcd_init();
    if (g_lcd_fd < 0) {
        printf("[LCD] (i2c indisponible) %s | %s\n",
               line1 ? line1 : "", line2 ? line2 : "");
        fflush(stdout);
        return;
    }
    lcd_command(0x01);
    delay(2);
    lcd_command(0x80);
    if (line1) lcd_print(line1);
    lcd_command(0xC0);
    if (line2) lcd_print(line2);
#else
    printf("[LCD] %-16s | %-16s\n",
           line1 ? line1 : "", line2 ? line2 : "");
    fflush(stdout);
#endif
}
