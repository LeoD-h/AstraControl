/************************************************************
 * Projet      : Fusée
 * Fichier     : ir_input.c
 * Description : Capteur IR JoyPi, décodeur NEC non-bloquant.
 *               API : ir_arm / ir_disarm / ir_poll
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 19/03/2026
 * Version     : 1.0
 ************************************************************/
#include "ir_input.h"
#include "joypi_controller.h"   /* KEY_CONFIRM, KEY_BACKSPACE (valeurs canoniques) */

#include <stdio.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

#define PIN_IR_PHYS     38   /* GPIO 20, physique pin 38 — même broche que Test/led.c */

/* Codes NEC télécommande JoyPi standard (confirmés avec Test/led.c) */
#define IR_KEY_0      0x00FF6897UL
#define IR_KEY_1      0x00FF30CFUL
#define IR_KEY_2      0x00FF18E7UL
#define IR_KEY_3      0x00FF7A85UL
#define IR_KEY_4      0x00FF10EFUL
#define IR_KEY_5      0x00FF38C7UL
#define IR_KEY_6      0x00FF5AA5UL
#define IR_KEY_7      0x00FF42BDUL
#define IR_KEY_8      0x00FF4AB5UL
#define IR_KEY_9      0x00FF52ADUL
#define IR_KEY_OK     0x00FF02FDUL
#define IR_KEY_STAR   0x00FF22DDUL
#define IR_KEY_MINUS  0x00FFE01FUL
#define IR_KEY_REPEAT 0x00FFFFFFUL

/* Fenêtre de poll IR en MODE_PASSWORD (µs) — couvre largement une trame NEC ~86ms */
#define IR_POLL_WINDOW_US  120000U

#ifdef USE_WIRINGPI

static unsigned long g_ir_code = 0;
static int g_ir_bit_count = 0;
static unsigned int g_ir_last_time = 0;
static int g_ir_code_ready = 0;
static unsigned long g_ir_final_code = 0;

static void ir_on_falling_edge(void)
{
    unsigned int current_time = micros();
    unsigned int delta = current_time - g_ir_last_time;
    g_ir_last_time = current_time;

    if (delta > 10000U) {
        g_ir_bit_count = 0;
        g_ir_code = 0;
        return;
    }

    if (g_ir_bit_count < 32) {
        if (delta > 2000U) {
            g_ir_code = (g_ir_code << 1) | 1UL;
            g_ir_bit_count++;
        } else if (delta > 1000U) {
            g_ir_code = (g_ir_code << 1);
            g_ir_bit_count++;
        }
    }

    if (g_ir_bit_count == 32) {
        g_ir_final_code = g_ir_code & 0x00FFFFFFUL;
        g_ir_code_ready = 1;
        g_ir_bit_count = 0;
    }
}

static unsigned long ir_decode_poll(unsigned int poll_us)
{
    static int prev_state = HIGH;
    unsigned int deadline = micros() + poll_us;

    while ((int)(micros() - (int)deadline) < 0) {
        int cur_state = digitalRead(PIN_IR_PHYS);
        if (cur_state == LOW && prev_state == HIGH) {
            ir_on_falling_edge();
        }
        prev_state = cur_state;

        if (g_ir_code_ready) {
            g_ir_code_ready = 0;
            return g_ir_final_code;
        }
        delayMicroseconds(50);
    }
    return 0;
}

static int ir_code_to_key(unsigned long code)
{
    unsigned long masked = code & 0x00FFFFFFUL;
    switch (masked) {
        case (IR_KEY_1 & 0x00FFFFFFUL): return 1;
        case (IR_KEY_2 & 0x00FFFFFFUL): return 2;
        case (IR_KEY_3 & 0x00FFFFFFUL): return 3;
        case (IR_KEY_4 & 0x00FFFFFFUL): return 4;
        case (IR_KEY_5 & 0x00FFFFFFUL): return 5;
        case (IR_KEY_6 & 0x00FFFFFFUL): return 6;
        case (IR_KEY_7 & 0x00FFFFFFUL): return 7;
        case (IR_KEY_8 & 0x00FFFFFFUL): return 8;
        case (IR_KEY_9 & 0x00FFFFFFUL): return 9;
        case (IR_KEY_0 & 0x00FFFFFFUL): return 10;
        case (IR_KEY_OK & 0x00FFFFFFUL):   return KEY_CONFIRM;
        case (IR_KEY_STAR & 0x00FFFFFFUL): return KEY_BACKSPACE;
        case (IR_KEY_MINUS & 0x00FFFFFFUL): return KEY_BACKSPACE;
        default:                           return 0;
    }
}

#endif /* USE_WIRINGPI */

/* ------------------------------------------------------------------ */
/* API publique                                                        */
/* ------------------------------------------------------------------ */

void ir_arm(void)
{
#ifdef USE_WIRINGPI
    /* Pin 38 dédié IR (pas de conflit LED) */
    pinMode(PIN_IR_PHYS, INPUT);
    pullUpDnControl(PIN_IR_PHYS, PUD_UP);
    g_ir_code = 0;
    g_ir_bit_count = 0;
    g_ir_last_time = 0;
    g_ir_code_ready = 0;
    g_ir_final_code = 0;
#endif
}

void ir_disarm(void)
{
#ifdef USE_WIRINGPI
    /* Rien à faire : pin 38 reste INPUT, pas de ressource partagée */
    (void)0;
#endif
}

int ir_poll(void)
{
#ifdef USE_WIRINGPI
    /* Écoute pendant IR_POLL_WINDOW_US µs (80ms) avec détection de fronts.
     * Retourne le numéro de touche, 0 si aucun code reçu dans la fenêtre. */
    unsigned long code = ir_decode_poll(IR_POLL_WINDOW_US);
    if (code == 0 || code == IR_KEY_REPEAT)
        return 0;
    {
        int key = ir_code_to_key(code);
        printf("[IR] raw=0x%06lX key=%d\n", code & 0x00FFFFFFUL, key);
        fflush(stdout);
        return key;
    }
#else
    return 0;
#endif
}
