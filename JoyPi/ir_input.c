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

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

#define PIN_IR_PHYS     11   /* GPIO 17, physique pin 11 — partagé avec LED rouge */

/* Codes NEC télécommande JoyPi standard (à calibrer sur matériel réel) */
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
#define IR_KEY_REPEAT 0xFFFFFFFFUL

/* Timings NEC (µs) */
#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_ONE_SPACE   1690
#define NEC_TOLERANCE    200

#ifdef USE_WIRINGPI

/* Mesure la durée d'un niveau sur le pin IR. Retourne µs ou -1 si timeout. */
static long pulse_duration(int expected_level)
{
    long count = 0;
    while (digitalRead(PIN_IR_PHYS) != expected_level) {
        if (++count > 15000) return -1;
        delayMicroseconds(1);
    }
    count = 0;
    while (digitalRead(PIN_IR_PHYS) == expected_level) {
        if (++count > 15000) return -1;
        delayMicroseconds(1);
    }
    return count;
}

/* Décode une trame NEC. Retourne le code 32 bits ou 0 en cas d'erreur. */
static unsigned long decode_nec(void)
{
    long dur;

    dur = pulse_duration(LOW);
    if (dur < 0 || dur < NEC_HDR_MARK - NEC_TOLERANCE * 4) return 0;
    if (dur > NEC_HDR_MARK + NEC_TOLERANCE * 4)             return 0;

    dur = pulse_duration(HIGH);
    if (dur < 0) return 0;
    if (dur < NEC_HDR_SPACE / 2) return IR_KEY_REPEAT;

    unsigned long code = 0;
    int i;
    for (i = 0; i < 32; i++) {
        dur = pulse_duration(LOW);
        if (dur < 0) return 0;
        dur = pulse_duration(HIGH);
        if (dur < 0) return 0;
        code >>= 1;
        if (dur > (NEC_ONE_SPACE - NEC_TOLERANCE))
            code |= 0x80000000UL;
    }
    return code;
}

static int ir_code_to_key(unsigned long code)
{
    switch (code) {
        case IR_KEY_1: return 1;
        case IR_KEY_2: return 2;
        case IR_KEY_3: return 3;
        case IR_KEY_4: return 4;
        case IR_KEY_5: return 5;
        case IR_KEY_6: return 6;
        case IR_KEY_7: return 7;
        case IR_KEY_8: return 8;
        case IR_KEY_9: return 9;
        case IR_KEY_0: return 10;
        case IR_KEY_OK:   return KEY_CONFIRM;
        case IR_KEY_STAR: return KEY_BACKSPACE;
        default:          return 0;
    }
}

#endif /* USE_WIRINGPI */

/* ------------------------------------------------------------------ */
/* API publique                                                        */
/* ------------------------------------------------------------------ */

void ir_arm(void)
{
#ifdef USE_WIRINGPI
    pinMode(PIN_IR_PHYS, INPUT);
#endif
}

void ir_disarm(void)
{
#ifdef USE_WIRINGPI
    pinMode(PIN_IR_PHYS, OUTPUT);
    digitalWrite(PIN_IR_PHYS, LOW);
#endif
}

int ir_poll(void)
{
#ifdef USE_WIRINGPI
    if (digitalRead(PIN_IR_PHYS) == HIGH)
        return 0;
    unsigned long code = decode_nec();
    if (code == 0 || code == IR_KEY_REPEAT)
        return 0;
    return ir_code_to_key(code);
#else
    return 0;
#endif
}
