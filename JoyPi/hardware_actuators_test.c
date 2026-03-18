/************************************************************
 * Projet      : Fusée
 * Fichier     : hardware_actuators_test.c
 * Description : Outil de diagnostic des actionneurs JoyPi.
 *               Tests : LED, servos, buzzer, matrice SPI, 7-seg, LCD.
 *
 * Usage :
 *   ./hardware_actuators_test          → tests visuels (buzzer désactivé)
 *   ./hardware_actuators_test -r       → test matériel réel (buzzer actif)
 *
 * Contrainte matérielle :
 *   LED verte = servo droit 1 = physical pin 37 (GPIO 26)
 *   LED rouge = servo droit 2 = physical pin 11 (GPIO 17)
 *   Buzzer    = physical pin 12 (GPIO 18)
 *   Pin 22 et 33 NON DISPONIBLES
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringPiSPI.h>

#define PIN_LED_GREEN_PHYS  37
#define PIN_LED_RED_PHYS    11
#define PIN_BUZZER_PHYS     12
#define SPI_CHAN_CE1         1
#define SPI_SPEED_HZ    500000
#define SEGMENT_ADDR      0x70
#define LCD_ADDR          0x21
#define MCP_IODIR_REG     0x00
#define MCP_GPIO_REG      0x09

static int g_lcd_backlight = 0x80;
static int g_lcd_rs        = 0x00;

/* -r : vrai test matériel (buzzer actif) */
static int g_real_test = 0;

static int i2c_open_probe(int addr, const char **bus_used) {
    int fd = wiringPiI2CSetupInterface("/dev/i2c-1", addr);
    if (fd >= 0) { *bus_used = "/dev/i2c-1"; return fd; }
    fd = wiringPiI2CSetupInterface("/dev/i2c-2", addr);
    if (fd >= 0) { *bus_used = "/dev/i2c-2"; return fd; }
    *bus_used = "none";
    return -1;
}

/* Génération de ton buzzer (utilisé seulement avec -r) */
static void play_tone_local(int freq_hz, int duration_ms) {
    if (freq_hz <= 0) { usleep((unsigned)duration_ms * 1000U); return; }
    int half_us = 1000000 / (freq_hz * 2);
    int cycles  = (freq_hz * duration_ms) / 1000;
    for (int i = 0; i < cycles; i++) {
        digitalWrite(PIN_BUZZER_PHYS, 1);
        delayMicroseconds((unsigned)half_us);
        digitalWrite(PIN_BUZZER_PHYS, 0);
        delayMicroseconds((unsigned)half_us);
    }
    digitalWrite(PIN_BUZZER_PHYS, 0);
}
#endif

/* ------------------------------------------------------------------ */

static void test_buzzer(void) {
#ifdef USE_WIRINGPI
    printf("[actuator] Buzzer test (pin %d)\n", PIN_BUZZER_PHYS);
    pinMode(PIN_BUZZER_PHYS, OUTPUT);
    digitalWrite(PIN_BUZZER_PHYS, 0);
    if (g_real_test) {
        printf("  Mode reel : buzzer actif\n");
        play_tone_local(440, 200);
        play_tone_local(0,   100);
        play_tone_local(880, 200);
        play_tone_local(0,   100);
        play_tone_local(1047, 300);
    } else {
        printf("  Mode safe : buzzer désactivé (utiliser -r pour test réel)\n");
    }
    printf("buzzer check\n");
#else
    printf("[actuator] Test buzzer (simulation)\n");
    printf("buzzer check\n");
#endif
}

static void test_leds(void) {
#ifdef USE_WIRINGPI
    printf("[actuator] LED verte (pin %d) + LED rouge (pin %d)\n",
           PIN_LED_GREEN_PHYS, PIN_LED_RED_PHYS);
    pinMode(PIN_LED_GREEN_PHYS, OUTPUT);
    pinMode(PIN_LED_RED_PHYS,   OUTPUT);
    /* LED verte ON */
    printf("  LED verte ON\n");
    digitalWrite(PIN_LED_GREEN_PHYS, 1);
    usleep(500000);
    digitalWrite(PIN_LED_GREEN_PHYS, 0);
    usleep(200000);
    /* LED rouge ON */
    printf("  LED rouge ON\n");
    digitalWrite(PIN_LED_RED_PHYS, 1);
    usleep(500000);
    digitalWrite(PIN_LED_RED_PHYS, 0);
    usleep(200000);
    printf("led check\n");
#else
    printf("[actuator] led check (simulation)\n");
#endif
}

static void test_servos(void) {
#ifdef USE_WIRINGPI
    printf("[actuator] Test servomoteurs/LEDs : servo1=pin%d (vert) servo2=pin%d (rouge)\n",
           PIN_LED_GREEN_PHYS, PIN_LED_RED_PHYS);
    /* Alternance 4 fois */
    for (int i = 0; i < 4; i++) {
        printf("  iter %d/4 : vert ON / rouge OFF\n", i + 1);
        digitalWrite(PIN_LED_GREEN_PHYS, 1);
        digitalWrite(PIN_LED_RED_PHYS,   0);
        usleep(250000);
        printf("  iter %d/4 : vert OFF / rouge ON\n", i + 1);
        digitalWrite(PIN_LED_GREEN_PHYS, 0);
        digitalWrite(PIN_LED_RED_PHYS,   1);
        usleep(250000);
    }
    digitalWrite(PIN_LED_GREEN_PHYS, 0);
    digitalWrite(PIN_LED_RED_PHYS,   0);
    printf("servo check: ok\n");
#else
    printf("[actuator] servo check (simulation)\n");
    printf("  Alternance 4x : vert/rouge\n");
    for (int i = 0; i < 4; i++) {
        printf("  [VERT ON  ROUGE OFF]\n");
        usleep(100000);
        printf("  [VERT OFF ROUGE ON ]\n");
        usleep(100000);
    }
    printf("servo check: ok\n");
#endif
}

#ifdef USE_WIRINGPI
static void max7219_write(unsigned char reg, unsigned char val) {
    unsigned char buf[2] = { reg, val };
    (void)wiringPiSPIDataRW(SPI_CHAN_CE1, buf, 2);
}
#endif

static void test_matrix_spi(void) {
#ifdef USE_WIRINGPI
    static const unsigned char cross[8] = {
        0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81
    };
    printf("[actuator] Matrix SPI CE1 (pin 26)\n");
    if (wiringPiSPISetupMode(SPI_CHAN_CE1, SPI_SPEED_HZ, 0) < 0) {
        printf("matrix check: fail (spi init)\n");
        return;
    }
    max7219_write(0x0F, 0x00);
    max7219_write(0x0C, 0x01);
    max7219_write(0x0B, 0x07);
    max7219_write(0x09, 0x00);
    max7219_write(0x0A, 0x08);
    for (int i = 1; i <= 8; ++i) max7219_write((unsigned char)i, 0xFF);
    usleep(1200000);
    for (int i = 0; i < 8; ++i) max7219_write((unsigned char)(i+1), cross[i]);
    usleep(2200000);
    printf("matrix check: ok\n");
#else
    printf("[actuator] matrix check (simulation)\n");
#endif
}

static void test_segment_0x70(void) {
#ifdef USE_WIRINGPI
    static const unsigned short digits[4] = {0x06, 0x5B, 0x4F, 0x66};
    unsigned short displaybuffer[8] = {0};
    int ok = 1;
    const char *bus = NULL;
    printf("[actuator] 7-segment addr 0x70\n");
    int fd = i2c_open_probe(SEGMENT_ADDR, &bus);
    if (fd < 0) { printf("segment check: fail (i2c setup)\n"); return; }
    printf("[actuator] segment bus=%s\n", bus);
    if (wiringPiI2CWrite(fd, 0x21) < 0) ok = 0;
    if (wiringPiI2CWrite(fd, 0x81) < 0) ok = 0;
    if (wiringPiI2CWrite(fd, 0xEF) < 0) ok = 0;
    for (int i = 0; i < 4; ++i) displaybuffer[i ^ 2] = digits[i];
    displaybuffer[2] = 0x02;
    for (int i = 0; i < 8; ++i) {
        if (wiringPiI2CWriteReg8(fd, i*2,   displaybuffer[i] & 0xFF) < 0) ok = 0;
        if (wiringPiI2CWriteReg8(fd, i*2+1, (displaybuffer[i]>>8) & 0xFF) < 0) ok = 0;
    }
    usleep(2500000);
    for (int i = 0; i < 8; ++i) {
        if (wiringPiI2CWriteReg8(fd, i*2,   0x00) < 0) ok = 0;
        if (wiringPiI2CWriteReg8(fd, i*2+1, 0x00) < 0) ok = 0;
    }
    printf("segment check: %s\n", ok ? "ok" : "fail");
#else
    printf("[actuator] segment check (simulation)\n");
#endif
}

#ifdef USE_WIRINGPI
static int lcd_write_reg(int fd, int reg, int val) {
    return wiringPiI2CWriteReg8(fd, reg, val);
}
static void lcd_write_bus(int fd, int data) {
    (void)lcd_write_reg(fd, MCP_GPIO_REG, data | 0x04);
    delayMicroseconds(1);
    (void)lcd_write_reg(fd, MCP_GPIO_REG, data);
    delayMicroseconds(50);
}
static void lcd_set(int fd, int data) {
    data = ((data & 0xF0) >> 1) | g_lcd_backlight | g_lcd_rs;
    lcd_write_bus(fd, data);
}
static void lcd_command_test(int fd, int data) {
    g_lcd_rs = 0x00;
    lcd_set(fd, data);
    lcd_set(fd, data << 4);
}
static void lcd_send_data_test(int fd, int data) {
    g_lcd_rs = 0x02;
    lcd_set(fd, data);
    lcd_set(fd, data << 4);
}
static void lcd_print_test(int fd, const char *s) {
    while (*s) lcd_send_data_test(fd, (unsigned char)*s++);
}
#endif

static void test_lcd_0x21(void) {
#ifdef USE_WIRINGPI
    int ok = 1;
    const char *bus = NULL;
    printf("[actuator] LCD addr 0x21\n");
    int fd = i2c_open_probe(LCD_ADDR, &bus);
    if (fd < 0) { printf("lcd check: fail (i2c setup)\n"); return; }
    printf("[actuator] lcd bus=%s\n", bus);
    if (lcd_write_reg(fd, MCP_IODIR_REG, 0x00) < 0) ok = 0;
    g_lcd_backlight = 0x80;
    g_lcd_rs        = 0x00;
    lcd_command_test(fd, 0x33);
    lcd_command_test(fd, 0x32);
    lcd_command_test(fd, 0x0C);
    lcd_command_test(fd, 0x28);
    lcd_command_test(fd, 0x06);
    lcd_command_test(fd, 0x01);
    delay(2);
    lcd_command_test(fd, 0x80);
    lcd_print_test(fd, "Hello Joy-Pi");
    lcd_command_test(fd, 0xC0);
    lcd_print_test(fd, "LCD 0x21 OK");
    delay(3);
    printf("lcd check: %s\n", ok ? "ok" : "fail");
#else
    printf("[actuator] lcd check (simulation)\n");
#endif
}

static void ensure_all_off(void) {
#ifdef USE_WIRINGPI
    pinMode(PIN_BUZZER_PHYS,    OUTPUT); digitalWrite(PIN_BUZZER_PHYS,    0);
    pinMode(PIN_LED_GREEN_PHYS, OUTPUT); digitalWrite(PIN_LED_GREEN_PHYS, 0);
    pinMode(PIN_LED_RED_PHYS,   OUTPUT); digitalWrite(PIN_LED_RED_PHYS,   0);
#endif
}

int main(int argc, char **argv) {
#ifdef USE_WIRINGPI
    /* Lecture option -r */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            g_real_test = 1;
        }
    }
    wiringPiSetupPhys();
    printf("[hardware-actuators-test] Mode : %s\n",
           g_real_test ? "REEL (-r actif)" : "SAFE (buzzer desactive, ajouter -r pour test reel)");
#else
    (void)argc; (void)argv;
    printf("[hardware-actuators-test] Mode simulation\n");
#endif

    printf("[hardware-actuators-test] Debut tests actionneurs JoyPi\n");
    test_buzzer();
    test_leds();
    test_servos();
    test_matrix_spi();
    test_segment_0x70();
    test_lcd_0x21();
    ensure_all_off();
    printf("[hardware-actuators-test] Fin tests\n");
    return 0;
}
