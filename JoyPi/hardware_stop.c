/************************************************************
 * Projet      : Fusée
 * Fichier     : hardware_stop.c
 * Description : Extinction propre des actionneurs JoyPi.
 *               Eteint matrice LED (MAX7219), afficheur 7-segments
 *               (HT16K33), LCD (MCP23017), LED et buzzer.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <stdio.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringPiSPI.h>

#define PIN_BUZZER_PHYS 12
#define PIN_LED_PHYS    37
#define SPI_CHAN_CE1     1
#define SPI_SPEED_HZ    500000
#define SEGMENT_ADDR    0x70
#define LCD_ADDR        0x21
#define MCP_IODIR_REG   0x00
#define MCP_GPIO_REG    0x09

/* ---------- MAX7219 ---------- */

static void max7219_write(unsigned char reg, unsigned char val) {
    unsigned char buf[2];
    buf[0] = reg;
    buf[1] = val;
    (void)wiringPiSPIDataRW(SPI_CHAN_CE1, buf, 2);
}

static void stop_matrix(void) {
    int i;
    printf("[stop] Matrice LED MAX7219...\n");
    if (wiringPiSPISetupMode(SPI_CHAN_CE1, SPI_SPEED_HZ, 0) < 0) {
        printf("[stop] matrice: SPI indisponible\n");
        return;
    }
    /* Effacer toutes les lignes */
    for (i = 1; i <= 8; i++) {
        max7219_write((unsigned char)i, 0x00);
    }
    /* Shutdown mode */
    max7219_write(0x0C, 0x00);
    printf("[stop] matrice: eteinte\n");
}

/* ---------- HT16K33 7-segments ---------- */

static void stop_segment(void) {
    int fd;
    int i;
    int ok = 1;
    printf("[stop] 7-segments HT16K33 0x70...\n");
    fd = wiringPiI2CSetupInterface("/dev/i2c-1", SEGMENT_ADDR);
    if (fd < 0) {
        fd = wiringPiI2CSetupInterface("/dev/i2c-2", SEGMENT_ADDR);
    }
    if (fd < 0) {
        printf("[stop] 7-segments: I2C indisponible\n");
        return;
    }
    /* Effacer tous les registres */
    for (i = 0; i < 8; i++) {
        if (wiringPiI2CWriteReg8(fd, i * 2,     0x00) < 0) ok = 0;
        if (wiringPiI2CWriteReg8(fd, i * 2 + 1, 0x00) < 0) ok = 0;
    }
    /* Display off */
    if (wiringPiI2CWrite(fd, 0x80) < 0) ok = 0;
    /* Oscillator off */
    if (wiringPiI2CWrite(fd, 0x20) < 0) ok = 0;
    printf("[stop] 7-segments: %s\n", ok ? "eteint" : "erreur I2C");
}

/* ---------- MCP23017 LCD ---------- */

static void stop_lcd(void) {
    int fd;
    printf("[stop] LCD MCP23017 0x21...\n");
    fd = wiringPiI2CSetupInterface("/dev/i2c-1", LCD_ADDR);
    if (fd < 0) {
        fd = wiringPiI2CSetupInterface("/dev/i2c-2", LCD_ADDR);
    }
    if (fd < 0) {
        printf("[stop] LCD: I2C indisponible\n");
        return;
    }
    /* Eteindre le backlight et mettre GPIO a zero */
    (void)wiringPiI2CWriteReg8(fd, MCP_IODIR_REG, 0x00);
    (void)wiringPiI2CWriteReg8(fd, MCP_GPIO_REG,  0x00);
    printf("[stop] LCD: eteint\n");
}

/* ---------- LED + Buzzer ---------- */

static void stop_gpio(void) {
    printf("[stop] LED pin%d + Buzzer pin%d...\n", PIN_LED_PHYS, PIN_BUZZER_PHYS);
    pinMode(PIN_LED_PHYS,    OUTPUT);
    pinMode(PIN_BUZZER_PHYS, OUTPUT);
    digitalWrite(PIN_LED_PHYS,    0);
    digitalWrite(PIN_BUZZER_PHYS, 0);
    printf("[stop] LED + Buzzer: eteints\n");
}

#endif /* USE_WIRINGPI */

int main(void) {
    printf("[hardware-stop] Extinction des actionneurs JoyPi\n");

#ifdef USE_WIRINGPI
    wiringPiSetupPhys();
    stop_gpio();
    stop_matrix();
    stop_segment();
    stop_lcd();
#else
    printf("[hardware-stop] Mode simulation : rien a eteindre\n");
#endif

    printf("[hardware-stop] Termine\n");
    return 0;
}
