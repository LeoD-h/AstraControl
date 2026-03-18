/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_hardware_button.c
 * Description : Gestion des entrées hardware du JoyPi.
 *               Module autonome qui encapsule la lecture de la
 *               matrice de boutons 4x4 et du joystick directionnel.
 *               Peut être utilisé seul (test) ou inclus dans
 *               joypi_controller.c via l'API.
 *
 * Matrice 4x4 (wiringPiSetupPhys) :
 *   Lignes ROW : pin13, pin15, pin29, pin31
 *   Colonnes COL : pin37, pin22, pin35, pin33
 *
 * Touche numéro = (row * 4) + (4 - col_index)
 *
 * Joystick directionnel (partagé avec colonnes) :
 *   DOWN=pin33  RIGHT=pin35  UP=pin37  LEFT=pin22
 *
 * Compilation :
 *   arm-linux-gnueabihf-gcc -DUSE_WIRINGPI -std=gnu99 -Wall \
 *       -I../wiringPi joypi_hardware_button.c \
 *       -L../wiringPi -lwiringPi -ldl -o hw_button_test
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 18/03/2026
 ************************************************************/

#include "joypi_hardware_button.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

/* ------------------------------------------------------------------ */
/* Configuration hardware                                              */
/* ------------------------------------------------------------------ */

#define HW_ROW_COUNT  4
#define HW_COL_COUNT  4

#ifdef USE_WIRINGPI
static const int row_pins[HW_ROW_COUNT] = {13, 15, 29, 31};
static const int col_pins[HW_COL_COUNT] = {37, 22, 35, 33};

#define PIN_DIR_DOWN   33
#define PIN_DIR_RIGHT  35
#define PIN_DIR_UP     37
#define PIN_DIR_LEFT   22
#endif

/* ------------------------------------------------------------------ */
/* État interne (anti-rebond)                                         */
/* ------------------------------------------------------------------ */

static int g_prev_matrix[HW_ROW_COUNT][HW_COL_COUNT];
static int g_prev_dir_up    = 0;
static int g_prev_dir_down  = 0;
static int g_prev_dir_right = 0;
static int g_prev_dir_left  = 0;
static int g_initialized    = 0;

/* ------------------------------------------------------------------ */
/* Initialisation GPIO                                                 */
/* ------------------------------------------------------------------ */

void hw_button_init(void) {
#ifdef USE_WIRINGPI
    int r, c;
    if (g_initialized) return;

    wiringPiSetupPhys();

    for (r = 0; r < HW_ROW_COUNT; r++) {
        pinMode(row_pins[r], INPUT);
        pullUpDnControl(row_pins[r], PUD_UP);
    }
    for (c = 0; c < HW_COL_COUNT; c++) {
        pinMode(col_pins[c], OUTPUT);
        digitalWrite(col_pins[c], HIGH);
    }
    memset(g_prev_matrix, 0, sizeof(g_prev_matrix));
    g_initialized = 1;
#else
    memset(g_prev_matrix, 0, sizeof(g_prev_matrix));
    g_initialized = 1;
    printf("[hw_button] init (mode simulation)\n");
#endif
}

/* ------------------------------------------------------------------ */
/* Lecture interne de la matrice                                       */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
static void read_matrix_raw(int state[HW_ROW_COUNT][HW_COL_COUNT]) {
    int r, c;
    for (c = 0; c < HW_COL_COUNT; c++) {
        digitalWrite(col_pins[c], LOW);
        delayMicroseconds(200);
        for (r = 0; r < HW_ROW_COUNT; r++) {
            state[r][c] = (digitalRead(row_pins[r]) == LOW) ? 1 : 0;
        }
        digitalWrite(col_pins[c], HIGH);
    }
}

static void read_direction_raw(int *up, int *down, int *right, int *left) {
    int c;
    /* Basculer les colonnes en INPUT pour lire les directions */
    for (c = 0; c < HW_COL_COUNT; c++) {
        pinMode(col_pins[c], INPUT);
        pullUpDnControl(col_pins[c], PUD_UP);
    }
    delayMicroseconds(120);
    *down  = (digitalRead(PIN_DIR_DOWN)  == LOW) ? 1 : 0;
    *right = (digitalRead(PIN_DIR_RIGHT) == LOW) ? 1 : 0;
    *up    = (digitalRead(PIN_DIR_UP)    == LOW) ? 1 : 0;
    *left  = (digitalRead(PIN_DIR_LEFT)  == LOW) ? 1 : 0;
    /* Rétablir les colonnes en OUTPUT */
    for (c = 0; c < HW_COL_COUNT; c++) {
        pinMode(col_pins[c], OUTPUT);
        digitalWrite(col_pins[c], HIGH);
    }
}
#endif /* USE_WIRINGPI */

/* ------------------------------------------------------------------ */
/* Scan et dispatch via callback                                       */
/* ------------------------------------------------------------------ */

/*
 * hw_button_scan() : appelé dans la boucle principale.
 * Lit la matrice et le joystick, détecte les fronts montants (press),
 * appelle le callback pour chaque nouvelle touche détectée.
 *
 * callback(key_num) où key_num :
 *   3,4,7,8,11,12,15,16 : boutons mission (colonnes pin22/pin37)
 *   1,2,5,6,9,10,13,14  : boutons secondaires (colonnes pin33/pin35)
 *   HW_DIR_UP/DOWN/LEFT/RIGHT : directions joystick
 */
void hw_button_scan(hw_button_callback_fn callback) {
    if (!g_initialized) {
        hw_button_init();
    }

#ifdef USE_WIRINGPI
    int cur[HW_ROW_COUNT][HW_COL_COUNT];
    int r, c;

    read_matrix_raw(cur);
    for (r = 0; r < HW_ROW_COUNT; r++) {
        for (c = 0; c < HW_COL_COUNT; c++) {
            if (cur[r][c] && !g_prev_matrix[r][c]) {
                int key_num = (r * HW_COL_COUNT) + (HW_COL_COUNT - c);
                if (callback) callback(key_num);
            }
            g_prev_matrix[r][c] = cur[r][c];
        }
    }

    {
        int dir_up = 0, dir_down = 0, dir_right = 0, dir_left = 0;
        read_direction_raw(&dir_up, &dir_down, &dir_right, &dir_left);

        if (dir_up    && !g_prev_dir_up    && callback) callback(HW_DIR_UP);
        if (dir_down  && !g_prev_dir_down  && callback) callback(HW_DIR_DOWN);
        if (dir_right && !g_prev_dir_right && callback) callback(HW_DIR_RIGHT);
        if (dir_left  && !g_prev_dir_left  && callback) callback(HW_DIR_LEFT);

        g_prev_dir_up    = dir_up;
        g_prev_dir_down  = dir_down;
        g_prev_dir_right = dir_right;
        g_prev_dir_left  = dir_left;
    }

    usleep(80000);   /* anti-rebond 80ms */

#else
    /* Mode simulation : lecture stdin non-bloquante */
    struct timeval tv = {0, 80000};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0) {
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) > 0 && callback) {
            int key = 0;
            switch (ch) {
                /* Touches mission (colonnes actives) */
                case '3': key =  3; break;   /* BT1 */
                case '4': key =  4; break;   /* BT2 */
                case '7': key =  7; break;   /* BT3 */
                case '8': key =  8; break;   /* BT4 */
                case 'b': key = 11; break;   /* BT5 (11) */
                case 'c': key = 12; break;   /* BT6 (12) */
                case 'e': key = 15; break;   /* BT7 (15) */
                case 'f': key = 16; break;   /* BT8 (16) */
                /* Touches saisie mot de passe */
                case '1': key =  1; break;
                case '2': key =  2; break;
                case '5': key =  5; break;
                case '6': key =  6; break;
                case '9': key =  9; break;
                case '0': key = 10; break;   /* 10 => '0' */
                case 'o': key = 13; break;   /* CONFIRMER */
                case 'x': key = 14; break;   /* EFFACER */
                /* Directions */
                case 'u': key = HW_DIR_UP;    break;
                case 'd': key = HW_DIR_DOWN;  break;
                case 'l': key = HW_DIR_LEFT;  break;
                case 'r': key = HW_DIR_RIGHT; break;
                default:  break;
            }
            if (key > 0) callback(key);
        }
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Programme de test autonome (main optionnel)                        */
/* ------------------------------------------------------------------ */

#ifdef HW_BUTTON_STANDALONE

static void test_callback(int key_num) {
    const char *label = "";
    switch (key_num) {
        case  3: label = "BT1 LANCEMENT";         break;
        case  4: label = "BT2 ATTERRISSAGE";      break;
        case  7: label = "BT3 ALTITUDE";           break;
        case  8: label = "BT4 TEMPERATURE";        break;
        case 11: label = "BT5 PRESSION";           break;
        case 12: label = "BT6 MELODIE";            break;
        case 15: label = "BT7 VITESSE";            break;
        case 16: label = "BT8 CARBURANT";          break;
        case 13: label = "CONFIRMER (mdp)";        break;
        case 14: label = "EFFACER/ANNULER (mdp)";  break;
        case HW_DIR_UP:    label = "DIR UP";       break;
        case HW_DIR_DOWN:  label = "DIR DOWN";     break;
        case HW_DIR_LEFT:  label = "DIR LEFT";     break;
        case HW_DIR_RIGHT: label = "DIR RIGHT";    break;
        default: label = "(non assigné)";          break;
    }
    printf("[hw_button] KEY%d : %s\n", key_num, label);
    fflush(stdout);
}

int main(void) {
    printf("=== Test hardware boutons JoyPi ===\n");
    printf("Appuyez sur les touches. Ctrl+C pour quitter.\n");
#ifndef USE_WIRINGPI
    printf("Simulation : 3/4/7/8=BT1-4  b/c=BT5-6  e/f=BT7-8\n");
    printf("             o=CONF  x=BACK  u/d/l/r=directions\n");
#endif
    printf("\n");

    hw_button_init();

    while (1) {
        hw_button_scan(test_callback);
    }
    return 0;
}

#endif /* HW_BUTTON_STANDALONE */
