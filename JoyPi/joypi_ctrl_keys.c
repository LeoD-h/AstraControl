/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_keys.c
 * Description : Scan GPIO (matrice 4x4 + directions joystick) et
 *               simulation clavier stdin. Délègue à joypi_ctrl_actions.c.
 *
 * Directions (joystick) :
 *   UP    → envoie "UP\n"    au dashboard
 *   RIGHT → envoie "RIGHT\n" au dashboard
 *   DOWN  → envoie "DOWN\n"  au dashboard
 *   LEFT  → envoie "LEFT\n"  au dashboard
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

static void read_matrix(int state[4][4]) {
    int r, c;
    for (c = 0; c < 4; ++c) {
        digitalWrite(COL_PINS[c], LOW);
        delayMicroseconds(200);
        for (r = 0; r < 4; ++r)
            state[r][c] = (digitalRead(ROW_PINS[r]) == LOW);
        digitalWrite(COL_PINS[c], HIGH);
    }
}

static void read_direction(int *up, int *down, int *right, int *left) {
    int c;
    for (c = 0; c < 4; ++c) {
        pinMode(COL_PINS[c], INPUT);
        pullUpDnControl(COL_PINS[c], PUD_UP);
    }
    delayMicroseconds(120);
    *down  = (digitalRead(PIN_DIR_DOWN)  == LOW);
    *right = (digitalRead(PIN_DIR_RIGHT) == LOW);
    *up    = (digitalRead(PIN_DIR_UP)    == LOW);
    *left  = (digitalRead(PIN_DIR_LEFT)  == LOW);
    for (c = 0; c < 4; ++c) {
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], HIGH);
    }
}
#endif /* USE_WIRINGPI */

/* ------------------------------------------------------------------ */
/* Scan principal (GPIO ou stdin)                                      */
/* ------------------------------------------------------------------ */

void scan_buttons_and_handle(ControllerState *st) {
#ifdef USE_WIRINGPI
    int cur[4][4];
    int r, c;
    read_matrix(cur);

    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            if (cur[r][c] && !g_prev_matrix[r][c]) {
                int key_num = (r * 4) + (4 - c);
                handle_key(st, key_num);
            }
            g_prev_matrix[r][c] = cur[r][c];
        }
    }

    /* Directions → commandes dashboard ncurses */
    {
        int dir_up=0, dir_down=0, dir_right=0, dir_left=0;
        read_direction(&dir_up, &dir_down, &dir_right, &dir_left);

        if (dir_up    && !g_prev_dir_up)    cmd_pipe_write(st, "UP\n");
        if (dir_down  && !g_prev_dir_down)  cmd_pipe_write(st, "DOWN\n");
        if (dir_right && !g_prev_dir_right) cmd_pipe_write(st, "RIGHT\n");
        if (dir_left  && !g_prev_dir_left)  cmd_pipe_write(st, "LEFT\n");

        g_prev_dir_up    = dir_up;
        g_prev_dir_down  = dir_down;
        g_prev_dir_right = dir_right;
        g_prev_dir_left  = dir_left;
    }

    /* IR télécommande — arm/disarm sur transition de mode uniquement */
    {
        static int prev_mode = MODE_NORMAL;
        if (st->mode == MODE_PASSWORD) {
            if (prev_mode != MODE_PASSWORD) ir_arm();
            int ir_key = ir_poll();
            if (ir_key != 0) handle_key(st, ir_key);
        } else if (prev_mode == MODE_PASSWORD) {
            ir_disarm();
        }
        prev_mode = st->mode;
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
