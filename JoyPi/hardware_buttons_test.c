/************************************************************
 * Projet      : Fusée
 * Fichier     : hardware_buttons_test.c
 * Description : Outil de diagnostic des boutons JoyPi (matrice + directions).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>

#define ROW_COUNT 4
#define COL_COUNT 4

/* Physical header pin numbers (BOARD mode) */
static const int ROW_PINS[ROW_COUNT] = {13, 15, 29, 31};
/* Ordre revu : pin33/35 actives en col3/4 -> keys 1,2,5,6,9,10,13,14 */
static const int COL_PINS[COL_COUNT] = {37, 22, 35, 33};
#define PIN_DIR_DOWN 33
#define PIN_DIR_RIGHT 35
#define PIN_DIR_UP 37
#define PIN_DIR_LEFT 22
#endif

/* Libelle fonctionnel associe a chaque key_num (mapping controller) */
#ifdef USE_WIRINGPI
static const char *key_label(int key_num) {
    switch (key_num) {
        case 1:  return "BT1 LANCEMENT";
        case 2:  return "BT2 ATTERRISSAGE";
        case 5:  return "BT3 ALTITUDE";
        case 6:  return "BT4 TEMPERATURE";
        case 9:  return "BT5 PRESSION";
        case 10: return "BT6 MELODIE";
        case 13: return "CONFIRMER (mdp)";
        case 14: return "EFFACER/ANNULER (mdp)";
        default: return "(non assigne)";
    }
}
#endif

static void print_banner(void) {
    printf("[hardware-buttons-test] Detection boutons JoyPi\n");
#ifdef USE_WIRINGPI
    printf("Mode GPIO reel. Colonnes actives: pin33 pin35. Ctrl+C pour quitter.\n");
    printf("Mapping: KEY1=BT1  KEY2=BT2  KEY5=BT3  KEY6=BT4  KEY9=BT5  KEY10=BT6\n");
    printf("         KEY13=CONFIRMER  KEY14=EFFACER/ANNULER\n");
#else
    printf("Mode simulation clavier. Touches: 1..6, q pour quitter.\n");
#endif
}

#ifdef USE_WIRINGPI
static void setup_gpio(void) {
    int r = 0;
    int c = 0;
    wiringPiSetupPhys();

    for (r = 0; r < ROW_COUNT; ++r) {
        pinMode(ROW_PINS[r], INPUT);
        pullUpDnControl(ROW_PINS[r], PUD_UP);
    }
    for (c = 0; c < COL_COUNT; ++c) {
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], HIGH);
    }

}

static int read_matrix(int state[ROW_COUNT][COL_COUNT]) {
    int r = 0;
    int c = 0;
    int pressed_count = 0;

    for (c = 0; c < COL_COUNT; ++c) {
        digitalWrite(COL_PINS[c], LOW);
        delayMicroseconds(200);

        for (r = 0; r < ROW_COUNT; ++r) {
            state[r][c] = (digitalRead(ROW_PINS[r]) == LOW);
            if (state[r][c]) {
                pressed_count++;
            }
        }

        digitalWrite(COL_PINS[c], HIGH);
    }

    return pressed_count;
}

static void read_direction(int *up, int *down, int *right, int *left) {
    int c = 0;
    for (c = 0; c < COL_COUNT; ++c) {
        pinMode(COL_PINS[c], INPUT);
        pullUpDnControl(COL_PINS[c], PUD_UP);
    }
    delayMicroseconds(120);
    *down = (digitalRead(PIN_DIR_DOWN) == LOW);
    *right = (digitalRead(PIN_DIR_RIGHT) == LOW);
    *up = (digitalRead(PIN_DIR_UP) == LOW);
    *left = (digitalRead(PIN_DIR_LEFT) == LOW);
    for (c = 0; c < COL_COUNT; ++c) {
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], HIGH);
    }
}

#endif

int main(void) {
    print_banner();

#ifdef USE_WIRINGPI
    setup_gpio();
    int prev[ROW_COUNT][COL_COUNT];
    int prev_dir_up = 0, prev_dir_down = 0, prev_dir_right = 0, prev_dir_left = 0;
    memset(prev, 0, sizeof(prev));
    printf("Rows phys: 13,15,29,31 | Cols phys: 37,22,35,33\n");
    printf("Keys actifs: 1,2 (row1) | 5,6 (row2) | 9,10 (row3) | 13,14 (row4)\n");
    printf("Directions: DOWN pin33, RIGHT pin35, UP pin37, LEFT pin22\n");

    while (1) {
        int cur[ROW_COUNT][COL_COUNT];
        int r = 0;
        int c = 0;
        (void)read_matrix(cur);

        for (r = 0; r < ROW_COUNT; ++r) {
            for (c = 0; c < COL_COUNT; ++c) {
                if (cur[r][c] && !prev[r][c]) {
                    int key_num = (r * COL_COUNT) + (COL_COUNT - c);
                    printf("KEY%d [%s] (row%d pin%d, col%d pin%d)\n",
                           key_num, key_label(key_num),
                           r + 1, ROW_PINS[r], c + 1, COL_PINS[c]);
                }
                prev[r][c] = cur[r][c];
            }
        }

        {
            int dir_up = 0, dir_down = 0, dir_right = 0, dir_left = 0;
            read_direction(&dir_up, &dir_down, &dir_right, &dir_left);

            if (dir_down && !prev_dir_down) printf("KEY17 DOWN (pin33)\n");
            if (dir_right && !prev_dir_right) printf("KEY18 RIGHT (pin35)\n");
            if (dir_up && !prev_dir_up) printf("KEY19 UP (pin37)\n");
            if (dir_left && !prev_dir_left) printf("KEY20 LEFT (pin22)\n");

            prev_dir_up = dir_up;
            prev_dir_down = dir_down;
            prev_dir_right = dir_right;
            prev_dir_left = dir_left;
        }

        usleep(80000);
    }
#else
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        struct timeval tv = {0, 200000};

        int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (rc > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
            char c = 0;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == '1') printf("BT1 clique\n");
                else if (c == '2') printf("BT2 clique\n");
                else if (c == '3') printf("BT3 clique\n");
                else if (c == '4') printf("BT4 clique\n");
                else if (c == '5') printf("BT5 clique\n");
                else if (c == '6') printf("BT6 clique\n");
                else if (c == 'q' || c == 'Q') break;
            }
        }
    }
#endif

    return 0;
}
