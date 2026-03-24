/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_keys.c
 * Description : Scan GPIO (matrice 4x4 + directions joystick) et
 *               simulation clavier stdin. Délègue à joypi_ctrl_actions.c.
 *
 * Directions (joystick) :
 *   KEY17 / pin33 → envoie "LEFT\n"  au dashboard
 *   RIGHT → envoie "RIGHT\n" au dashboard
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "joypi_ctrl_keys.h"
#include "joypi_ctrl_actions.h"
#include "joypi_ctrl_net.h"
#include "ir_input.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>

#define ROW_COUNT 4
#define COL_COUNT 4

extern const int ROW_PINS[ROW_COUNT];
extern const int COL_PINS[COL_COUNT];

#define PIN_DIR_DOWN  33
#define PIN_DIR_RIGHT 35
#define PIN_DIR_UP    37
#define PIN_DIR_LEFT  22

/* ------------------------------------------------------------------ */
/* GPIO - lecture matrice et directions                                */
/* ------------------------------------------------------------------ */

static int g_prev_matrix[4][4];
static int g_prev_dir_up    = 0;
static int g_prev_dir_down  = 0;
static int g_prev_dir_right = 0;
static int g_prev_dir_left  = 0;
static int g_steer_state    = 0; /* -1 = gauche, +1 = droite, 0 = neutre */
static int g_up_stable      = 0;
static int g_right_stable   = 0;
static int g_release_stable = 0;

static void read_matrix(int state[4][4]) {
    int r, c;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c)
            state[r][c] = 0;
    }
    for (c = 2; c < 4; ++c) {
        digitalWrite(COL_PINS[c], LOW);
        delayMicroseconds(200);
        for (r = 0; r < 4; ++r)
            state[r][c] = (digitalRead(ROW_PINS[r]) == LOW);
        digitalWrite(COL_PINS[c], HIGH);
    }
}

static void read_direction(int *up, int *down, int *right, int *left) {
    pinMode(PIN_DIR_DOWN, INPUT);
    pullUpDnControl(PIN_DIR_DOWN, PUD_UP);
    pinMode(PIN_DIR_RIGHT, INPUT);
    pullUpDnControl(PIN_DIR_RIGHT, PUD_UP);
    delayMicroseconds(200);
    *down  = (digitalRead(PIN_DIR_DOWN)  == LOW);
    *right = (digitalRead(PIN_DIR_RIGHT) == LOW);
    *up    = 0;
    *left  = 0;
    pinMode(PIN_DIR_DOWN, OUTPUT);
    digitalWrite(PIN_DIR_DOWN, HIGH);
    pinMode(PIN_DIR_RIGHT, OUTPUT);
    digitalWrite(PIN_DIR_RIGHT, HIGH);
}
#endif /* USE_WIRINGPI */

/* ------------------------------------------------------------------ */
/* Scan principal (GPIO ou stdin)                                      */
/* ------------------------------------------------------------------ */

void scan_buttons_and_handle(ControllerState *st) {
#ifdef USE_WIRINGPI
    int cur[4][4];
    int r, c;

    /* En mode mot de passe, on isole complètement la lecture IR pour
     * éviter les conflits de timing avec le scan GPIO partagé. */
    if (st->mode == MODE_PASSWORD) {
        static time_t pwd_start = 0;
        if (pwd_start == 0) pwd_start = time(NULL);
        else if (time(NULL) - pwd_start > 30) {
            printf("[ctrl] Timeout mot de passe -> MODE_NORMAL\n");
            st->mode = MODE_NORMAL;
            ir_disarm();
            pwd_start = 0;
            return;
        }
        {
            static int prev_mode = MODE_NORMAL;
            if (prev_mode != MODE_PASSWORD) ir_arm();
            prev_mode = MODE_PASSWORD;
        }
        {
            int ir_key = ir_poll();
            if (ir_key != 0) handle_key(st, ir_key);
        }
        return;
    }

    /* ---- 1. Directions lues EN PREMIER (COL pins sont OUTPUT HIGH) -----
     * Les pins partagées pin37(UP) et pin22(LEFT) viennent d'OUTPUT HIGH,
     * la transition HIGH→INPUT est propre, pas de résidu LOW du scan matrice. */
    {
        int dir_up=0, dir_down=0, dir_right=0, dir_left=0;
        read_direction(&dir_up, &dir_down, &dir_right, &dir_left);

        if (dir_down && !dir_right) {
            if (g_up_stable < 1) g_up_stable++;
            g_right_stable = 0;
            g_release_stable = 0;
            if (g_up_stable >= 1 && g_steer_state != -1) {
                cmd_pipe_write(st, "LEFT\n");
                g_steer_state = -1;
            }
        } else if (dir_right && !dir_down) {
            if (g_right_stable < 1) g_right_stable++;
            g_up_stable = 0;
            g_release_stable = 0;
            if (g_right_stable >= 1 && g_steer_state != 1) {
                cmd_pipe_write(st, "RIGHT\n");
                g_steer_state = 1;
            }
        } else {
            g_up_stable = 0;
            g_right_stable = 0;
            if (!dir_down && !dir_right) {
                if (g_release_stable < 6) g_release_stable++;
                if (g_release_stable >= 6 && g_steer_state != 0) {
                    cmd_pipe_write(st, "STRAIGHT\n");
                    g_steer_state = 0;
                }
            } else {
                g_release_stable = 0;
            }
        }

        g_prev_dir_up    = dir_up;
        g_prev_dir_down  = dir_down;
        g_prev_dir_right = dir_right;
        g_prev_dir_left  = dir_left;
    }

    /* ---- 2. Timeout MODE_PASSWORD (30s sans confirmation) -------------- */
    {
        static time_t pwd_start = 0;
        if (st->mode == MODE_PASSWORD) {
            if (pwd_start == 0) pwd_start = time(NULL);
            else if (time(NULL) - pwd_start > 30) {
                printf("[ctrl] Timeout mot de passe -> MODE_NORMAL\n");
                st->mode = MODE_NORMAL;
                ir_disarm();
                pwd_start = 0;
            }
        } else {
            pwd_start = 0;
        }
    }

    /* ---- 3. Scan matrice (après la lecture des directions) ------------- */
    read_matrix(cur);

    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            if (cur[r][c] && !g_prev_matrix[r][c]) {
                /* En mode PASSWORD, la matrice est ignorée :
                 * le mot de passe est saisi via IR ou clavier USB (controle_fusee). */
                if (st->mode != MODE_PASSWORD) {
                    int key_num = (r * 4) + (4 - c);
                    handle_key(st, key_num);
                }
            }
            g_prev_matrix[r][c] = cur[r][c];
        }
    }

    /* ---- 4. IR télécommande — arm/disarm sur transition de mode -------- */
    {
        static int prev_mode = MODE_NORMAL;
        if (prev_mode == MODE_PASSWORD) {
            ir_disarm();
            prev_mode = MODE_NORMAL;
        }
    }
    usleep(POLL_TIMEOUT_MS * 1000U);

#else
    /* Simulation clavier */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    struct timeval tv = {0, POLL_TIMEOUT_MS * 1000};

    int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (rc > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            int key_num = 0;
            switch (c) {
                case '1': key_num = KEY_BT1; break;
                case '2': key_num = KEY_BT2; break;
                case '3': key_num = KEY_BT3; break;
                case '4': key_num = KEY_BT4; break;
                case '5': key_num = KEY_BT5; break;
                case '6': key_num = KEY_BT6; break;
                case '7': key_num = KEY_BT7; break;
                case '8': key_num = KEY_BT8; break;
                /* Directions simulées */
                case 'u': cmd_pipe_write(st, "UP\n");    return;
                case 'd': cmd_pipe_write(st, "DOWN\n");  return;
                case 'r': cmd_pipe_write(st, "RIGHT\n"); return;
                case 'l': cmd_pipe_write(st, "LEFT\n");  return;
                case 'q': case 'Q': g_stop = 1; return;
                default: break;
            }
            if (key_num > 0) handle_key(st, key_num);
        }
    }
#endif
}
