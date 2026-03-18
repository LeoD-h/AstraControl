/************************************************************
 * Projet      : Fusée
 * Fichier     : actuators.c
 * Description : LED, buzzer et init actuateurs JoyPi.
 *               Afficheurs (matrice, 7-seg, LCD) dans actuators_display.c.
 *
 * Contrainte matérielle :
 *   LED verte = servo droit 1 = physical pin 37 (GPIO 26)
 *   LED rouge = servo droit 2 = physical pin 11 (GPIO 17)
 *   Buzzer    = physical pin 12 (GPIO 18, capable PWM)
 *   Pin 22 et 33 NON DISPONIBLES
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#include "actuators.h"

#include <stdio.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

/* ------------------------------------------------------------------ */
/* Pins hardware (physiques, wiringPiSetupPhys)                       */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
#define PIN_LED_GREEN_PHYS  37   /* GPIO 26 — LED verte, servo droit 1 */
#define PIN_LED_RED_PHYS    11   /* GPIO 17 — LED rouge, servo droit 2  */
#define PIN_BUZZER_PHYS     12   /* GPIO 18 — buzzer PWM                */
#endif

/* ------------------------------------------------------------------ */
/* Buzzer : génération de ton par bascule GPIO                        */
/* ------------------------------------------------------------------ */

static void play_tone(int freq_hz, int duration_ms) {
#ifdef USE_WIRINGPI
    if (freq_hz <= 0) {
        usleep((unsigned int)duration_ms * 1000U);
        return;
    }
    int half_us = 1000000 / (freq_hz * 2);
    int cycles  = (freq_hz * duration_ms) / 1000;
    int i;
    for (i = 0; i < cycles; i++) {
        digitalWrite(PIN_BUZZER_PHYS, 1);
        delayMicroseconds((unsigned int)half_us);
        digitalWrite(PIN_BUZZER_PHYS, 0);
        delayMicroseconds((unsigned int)half_us);
    }
    digitalWrite(PIN_BUZZER_PHYS, 0);
#else
    (void)freq_hz;
    usleep((unsigned int)duration_ms * 1000U);
#endif
}

/* ================================================================== */
/* API publique                                                        */
/* ================================================================== */

void actuator_init(void) {
#ifdef USE_WIRINGPI
    pinMode(PIN_LED_GREEN_PHYS, OUTPUT);
    digitalWrite(PIN_LED_GREEN_PHYS, 0);
    pinMode(PIN_LED_RED_PHYS, OUTPUT);
    digitalWrite(PIN_LED_RED_PHYS, 0);
    pinMode(PIN_BUZZER_PHYS, OUTPUT);
    digitalWrite(PIN_BUZZER_PHYS, 0);
    /* SPI et I2C initialisés de façon lazy dans actuators_display.c */
#else
    printf("[actuators] init (simulation mode)\n");
    fflush(stdout);
#endif
}

/* ------------------------------------------------------------------ */
/* LED : interface compat + gestion 2 LEDs                            */
/* ------------------------------------------------------------------ */

void actuator_led_all_off(void) {
#ifdef USE_WIRINGPI
    digitalWrite(PIN_LED_GREEN_PHYS, 0);
    digitalWrite(PIN_LED_RED_PHYS,   0);
#else
    printf("[LED] VERT=OFF  ROUGE=OFF\n");
    fflush(stdout);
#endif
}

void actuator_led_green_blink(int count) {
#ifdef USE_WIRINGPI
    int i;
    for (i = 0; i < count; i++) {
        digitalWrite(PIN_LED_GREEN_PHYS, 1);
        usleep(150000);
        digitalWrite(PIN_LED_GREEN_PHYS, 0);
        usleep(100000);
    }
#else
    printf("[LED] VERT clignote %dx\n", count);
    fflush(stdout);
#endif
}

void actuator_led_red_on(void) {
#ifdef USE_WIRINGPI
    digitalWrite(PIN_LED_GREEN_PHYS, 0);
    digitalWrite(PIN_LED_RED_PHYS,   1);
#else
    printf("[LED] ROUGE=ON  VERT=OFF\n");
    fflush(stdout);
#endif
}

void actuator_led_set(int state) {
#ifdef USE_WIRINGPI
    if (state == 0) {
        actuator_led_all_off();
    } else if (state == 1) {
        /* Alerte : LED rouge clignote 3x */
        int i;
        digitalWrite(PIN_LED_GREEN_PHYS, 0);
        for (i = 0; i < 3; i++) {
            digitalWrite(PIN_LED_RED_PHYS, 1);
            usleep(80000);
            digitalWrite(PIN_LED_RED_PHYS, 0);
            usleep(80000);
        }
    } else {
        /* OK : LED verte allumée steady */
        digitalWrite(PIN_LED_RED_PHYS,   0);
        digitalWrite(PIN_LED_GREEN_PHYS, 1);
    }
#else
    const char *label = (state == 0) ? "VERT=OFF ROUGE=OFF"
                      : (state == 1) ? "ROUGE/ALERTE (3 blinks)"
                      :                "VERT/OK (steady)";
    printf("[LED] %s\n", label);
    fflush(stdout);
#endif
}

/* ------------------------------------------------------------------ */
/* Buzzer : mélodies événements                                       */
/* ------------------------------------------------------------------ */

void actuator_buzzer_melody_a(void) {
    /* Mélodie A : Décollage — montée festive */
#ifndef USE_WIRINGPI
    printf("[BUZZER] Melodie A (decollage)\n");
    fflush(stdout);
#endif
    play_tone(523,  200); /* C5 */
    play_tone(659,  200); /* E5 */
    play_tone(784,  200); /* G5 */
    play_tone(1047, 400); /* C6 */
    play_tone(988,  100); /* B5 */
    play_tone(1047, 600); /* C6 */
    play_tone(0,     50);
}

void actuator_buzzer_melody_b(void) {
    /* Mélodie B : Atterrissage/Urgence — alarme rapide + descente */
    int i;
#ifndef USE_WIRINGPI
    printf("[BUZZER] Melodie B (atterrissage/urgence)\n");
    fflush(stdout);
#endif
    for (i = 0; i < 5; i++) {
        play_tone(880, 150);
        play_tone(0,    80);
    }
    play_tone(660, 200);
    play_tone(440, 300);
    play_tone(220, 500);
    play_tone(0,    50);
}

void actuator_buzzer_melody_c(void) {
    /* Mélodie C : Panne — avertissement grave */
#ifndef USE_WIRINGPI
    printf("[BUZZER] Melodie C (panne)\n");
    fflush(stdout);
#endif
    play_tone(440, 300);
    play_tone(0,   100);
    play_tone(440, 300);
    play_tone(0,   100);
    play_tone(330, 500);
    play_tone(0,    50);
}

void actuator_buzzer_bip(void) {
    /* Bip court : confirmation / correcteur */
#ifndef USE_WIRINGPI
    printf("[BUZZER] Bip\n");
    fflush(stdout);
#endif
    play_tone(1000, 80);
    play_tone(0,    30);
}
