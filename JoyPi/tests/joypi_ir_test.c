/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ir_test.c
 * Description : Test du capteur infrarouge JoyPi.
 *               Lit les codes IR reçus et les affiche en console.
 *               Permet de calibrer le mapping des touches de la
 *               télécommande pour la saisie du mot de passe.
 *
 *               Hardware : GPIO IR sur pin physique 11 (GPIO 17)
 *               Protocol : NEC (38 kHz), standard télécommandes JoyPi
 *
 * Compilation (JoyPi/RPi) :
 *   arm-linux-gnueabihf-gcc -DUSE_WIRINGPI -std=gnu99 -Wall \
 *       -I../wiringPi joypi_ir_test.c -L../wiringPi -lwiringPi -ldl -o ir_test
 *
 * Utilisation :
 *   sudo ./ir_test           (lit les signaux IR en continu)
 *   Appuyez sur les touches de la télécommande pour voir leurs codes.
 *   Ctrl+C pour quitter.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 18/03/2026
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

/* Pin physique du récepteur IR (TSOP4838 ou équivalent) */
#define PIN_IR_PHYS   11   /* GPIO 17, physique pin 11 */

/* Timings NEC (en microsecondes) */
#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_BIT_MARK     560
#define NEC_ONE_SPACE   1690
#define NEC_ZERO_SPACE   560
#define NEC_TOLERANCE    200

/* Codes NEC connus pour la télécommande JoyPi standard (à calibrer) */
#define IR_KEY_0   0x00FF6897
#define IR_KEY_1   0x00FF30CF
#define IR_KEY_2   0x00FF18E7
#define IR_KEY_3   0x00FF7A85
#define IR_KEY_4   0x00FF10EF
#define IR_KEY_5   0x00FF38C7
#define IR_KEY_6   0x00FF5AA5
#define IR_KEY_7   0x00FF42BD
#define IR_KEY_8   0x00FF4AB5
#define IR_KEY_9   0x00FF52AD
#define IR_KEY_OK  0x00FF02FD
#define IR_KEY_STAR 0x00FF22DD
#define IR_KEY_HASH 0x00FFB04F
#define IR_KEY_UP  0x00FF629D
#define IR_KEY_DOWN 0x00FFA857
#define IR_KEY_LEFT 0x00FF22DD
#define IR_KEY_RIGHT 0x00FFC23D
#define IR_KEY_REPEAT 0xFFFFFFFF

static volatile int g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

/* Retourne le nom lisible d'un code IR */
static const char *ir_key_name(unsigned long code) {
    switch (code) {
        case IR_KEY_0:     return "0";
        case IR_KEY_1:     return "1";
        case IR_KEY_2:     return "2";
        case IR_KEY_3:     return "3";
        case IR_KEY_4:     return "4";
        case IR_KEY_5:     return "5";
        case IR_KEY_6:     return "6";
        case IR_KEY_7:     return "7";
        case IR_KEY_8:     return "8";
        case IR_KEY_9:     return "9";
        case IR_KEY_OK:    return "OK (confirmer)";
        case IR_KEY_STAR:  return "* (effacer)";
        case IR_KEY_HASH:  return "# (annuler)";
        case IR_KEY_UP:    return "HAUT";
        case IR_KEY_DOWN:  return "BAS";
        case IR_KEY_LEFT:  return "GAUCHE";
        case IR_KEY_RIGHT: return "DROITE";
        case IR_KEY_REPEAT: return "(répétition)";
        default:           return "(inconnu)";
    }
}

/* Mapping IR -> action mot de passe */
static void handle_ir_password(unsigned long code) {
    switch (code) {
        case IR_KEY_0: printf("  => saisit '0'\n"); break;
        case IR_KEY_1: printf("  => saisit '1'\n"); break;
        case IR_KEY_2: printf("  => saisit '2'\n"); break;
        case IR_KEY_3: printf("  => saisit '3'\n"); break;
        case IR_KEY_4: printf("  => saisit '4'\n"); break;
        case IR_KEY_5: printf("  => saisit '5'\n"); break;
        case IR_KEY_6: printf("  => saisit '6'\n"); break;
        case IR_KEY_7: printf("  => saisit '7'\n"); break;
        case IR_KEY_8: printf("  => saisit '8'\n"); break;
        case IR_KEY_9: printf("  => saisit '9'\n"); break;
        case IR_KEY_OK:    printf("  => CONFIRMER le mot de passe\n"); break;
        case IR_KEY_STAR:  printf("  => EFFACER dernier caractère\n"); break;
        case IR_KEY_HASH:  printf("  => ANNULER la saisie\n"); break;
        case IR_KEY_REPEAT: break; /* ignorer les répétitions */
        default:
            printf("  => code inconnu, non mappé\n");
    }
}

#ifdef USE_WIRINGPI
/* Mesure la durée d'un niveau (HIGH ou LOW) sur le pin IR.
 * Retourne la durée en µs, ou -1 si timeout (>15ms). */
static long pulse_duration(int expected_level) {
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

/* Décode une trame NEC. Retourne le code 32 bits, ou 0 en cas d'erreur. */
static unsigned long decode_nec(void) {
    long dur;

    /* Attendre le header mark (~9ms HIGH actif-bas sur TSOP → LOW sur pin) */
    dur = pulse_duration(LOW);
    if (dur < 0 || (dur < NEC_HDR_MARK - NEC_TOLERANCE * 4)) return 0;
    if (dur > NEC_HDR_MARK + NEC_TOLERANCE * 4) return 0;

    /* Header space (~4.5ms) */
    dur = pulse_duration(HIGH);
    if (dur < 0) return 0;

    /* Répétition NEC (~2.25ms space) */
    if (dur < NEC_HDR_SPACE / 2) return IR_KEY_REPEAT;

    /* Décode 32 bits */
    unsigned long code = 0;
    int i;
    for (i = 0; i < 32; i++) {
        /* Bit mark */
        dur = pulse_duration(LOW);
        if (dur < 0) return 0;

        /* Bit space : long = 1, court = 0 */
        dur = pulse_duration(HIGH);
        if (dur < 0) return 0;

        code >>= 1;
        if (dur > (NEC_ONE_SPACE - NEC_TOLERANCE)) {
            code |= 0x80000000UL;
        }
    }
    return code;
}
#endif /* USE_WIRINGPI */

int main(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("=== Test capteur infrarouge JoyPi ===\n");
    printf("Capteur IR : pin physique %d\n", PIN_IR_PHYS);
    printf("Appuyez sur les touches de la télécommande.\n");
    printf("Ctrl+C pour quitter.\n\n");

#ifdef USE_WIRINGPI
    wiringPiSetupPhys();
    pinMode(PIN_IR_PHYS, INPUT);

    printf("Mode GPIO réel. En attente de signal IR...\n\n");

    while (!g_stop) {
        /* Attendre un front descendant (début de trame) */
        if (digitalRead(PIN_IR_PHYS) == LOW) {
            unsigned long code = decode_nec();
            if (code != 0) {
                printf("Code IR reçu : 0x%08lX  =>  %s\n",
                       code, ir_key_name(code));
                handle_ir_password(code);
                printf("\n");
                fflush(stdout);
            }
        }
        delayMicroseconds(50);
    }
#else
    printf("Mode simulation (pas de wiringPi). Entrez un code IR à la main :\n");
    printf("  0-9 : chiffres  |  o=OK  |  s=effacer  |  q=quitter\n\n");

    while (!g_stop) {
        char c;
        int rc = (int)read(STDIN_FILENO, &c, 1);
        if (rc <= 0) break;

        unsigned long code = 0;
        switch (c) {
            case '0': code = IR_KEY_0; break;
            case '1': code = IR_KEY_1; break;
            case '2': code = IR_KEY_2; break;
            case '3': code = IR_KEY_3; break;
            case '4': code = IR_KEY_4; break;
            case '5': code = IR_KEY_5; break;
            case '6': code = IR_KEY_6; break;
            case '7': code = IR_KEY_7; break;
            case '8': code = IR_KEY_8; break;
            case '9': code = IR_KEY_9; break;
            case 'o': case 'O': code = IR_KEY_OK;   break;
            case 's': case 'S': code = IR_KEY_STAR; break;
            case 'q': case 'Q': g_stop = 1; continue;
            default: continue;
        }
        printf("Code IR simulé : 0x%08lX  =>  %s\n", code, ir_key_name(code));
        handle_ir_password(code);
        printf("\n");
        fflush(stdout);
    }
#endif

    printf("\n[ir_test] Arrêté.\n");
    return 0;
}
