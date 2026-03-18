/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_keys.c
 * Description : Actions des 8 boutons de mission, saisie mot de passe,
 *               gestion des directions (ncurses dashboard), scan GPIO.
 *
 * Boutons mission :
 *   BT1 (key3)  : Lancement (saisie mot de passe)
 *   BT2 (key4)  : Atterrissage urgence
 *   BT3 (key7)  : Demande altitude
 *   BT4 (key8)  : Demande température
 *   BT5 (key11) : Cycle pression (panne → fix → nominal)
 *   BT6 (key12) : Mélodie (cycle 1..3)
 *   BT7 (key15) : Demande vitesse (7-seg)
 *   BT8 (key16) : Résolution panne (si active) / info carburant
 *
 * Directions (joystick) :
 *   UP    → envoie "UP\n"    au dashboard (fusée monte + vitesse)
 *   RIGHT → envoie "RIGHT\n" au dashboard (fusée part à droite)
 *   DOWN  → envoie "DOWN\n"  au dashboard
 *   LEFT  → envoie "LEFT\n"  au dashboard
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "joypi_ctrl_keys.h"
#include "joypi_ctrl_net.h"
#include "actuators.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#endif

/* ------------------------------------------------------------------ */
/* Mot de passe                                                        */
/* ------------------------------------------------------------------ */

static void password_reset(ControllerState *st) {
    memset(st->password_buf, 0, sizeof(st->password_buf));
    st->password_len = 0;
}

static void password_show(ControllerState *st) {
    char stars[PASSWORD_MAX_LEN + 1];
    int i;
    for (i = 0; i < st->password_len && i < PASSWORD_MAX_LEN; ++i)
        stars[i] = '*';
    stars[i] = '\0';
    printf("[ctrl] MDP: [%s] (%d car.)\n", stars, st->password_len);
    fflush(stdout);
}

static void password_add_char(ControllerState *st, char c) {
    if (st->password_len >= PASSWORD_MAX_LEN) return;
    st->password_buf[st->password_len++] = c;
    st->password_buf[st->password_len]   = '\0';
    password_show(st);
}

static void password_backspace(ControllerState *st) {
    if (st->password_len > 0) {
        st->password_buf[--st->password_len] = '\0';
        password_show(st);
    } else {
        printf("[ctrl] Saisie mot de passe annulée\n");
        st->mode = MODE_NORMAL;
        password_reset(st);
    }
}

/* ------------------------------------------------------------------ */
/* Réponses aux commandes                                              */
/* ------------------------------------------------------------------ */

static void handle_response_launch(ControllerState *st, const char *resp) {
    if (strcmp(resp, "OK LAUNCH") == 0) {
        printf("[ctrl] LANCEMENT OK\n");
        cmd_pipe_write(st, "LAUNCH_OK\n");
        cmd_pipe_write(st, "SIM_FLIGHT ON\n");
        /* LED verte clignote (excitation décollage), puis steady */
        actuator_led_green_blink(5);
        actuator_led_set(2);
        actuator_matrix_launch();
        actuator_buzzer_melody_a();
    } else if (strcmp(resp, "FAIL ALREADY_LAUNCHED") == 0) {
        printf("[ctrl] ECHEC lancement : déjà en vol\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
    } else if (strcmp(resp, "FAIL NOT_READY") == 0) {
        printf("[ctrl] ECHEC lancement : satellite non prêt\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD LU) : %s\n", resp);
    }
}

static void handle_response_land(ControllerState *st, const char *resp) {
    if (strcmp(resp, "OK LAND") == 0) {
        printf("[ctrl] ATTERRISSAGE OK\n");
        cmd_pipe_write(st, "LAND\n");
        /* LED rouge allumée pour l'atterrissage */
        actuator_led_red_on();
        actuator_matrix_emergency();
        actuator_buzzer_melody_b();
    } else if (strcmp(resp, "FAIL NOT_FLYING") == 0) {
        printf("[ctrl] ECHEC atterrissage : pas en vol\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD LD) : %s\n", resp);
    }
}

static void handle_response_alt(const char *resp) {
    if (strncmp(resp, "DATA ALT ", 9) == 0) {
        int val = atoi(resp + 9);
        printf("[ctrl] ALTITUDE: %d m\n", val);
        actuator_segment_show(val);
    } else {
        printf("[ctrl] Réponse inattendue (CMD ALT) : %s\n", resp);
    }
}

static void handle_response_temp(const char *resp) {
    if (strncmp(resp, "DATA TEMP ", 10) == 0) {
        int val = atoi(resp + 10);
        printf("[ctrl] TEMPERATURE: %d C\n", val);
        actuator_segment_show(val);
        if (val > 350) actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD TEMP) : %s\n", resp);
    }
}

static void handle_response_pres(ControllerState *st, const char *resp) {
    if (strcmp(resp, "OK PRES_FAULT") == 0) {
        printf("[ctrl] PRESSION : panne détectée\n");
        st->fault_active = true;
        data_pipe_write(st, "PROBLEM ON\n");
        actuator_led_set(1);
        actuator_buzzer_melody_c();
    } else if (strcmp(resp, "OK PRES_FIX") == 0) {
        printf("[ctrl] PRESSION : correcteur activé\n");
        cmd_pipe_write(st, "FIX_PROBLEM\n");
        data_pipe_write(st, "PROBLEM OFF\n");
        actuator_buzzer_bip();
    } else if (strcmp(resp, "OK PRES_OK") == 0) {
        printf("[ctrl] PRESSION : nominale\n");
        st->fault_active = false;
        cmd_pipe_write(st, "CLEAR_ALERTS\n");
        actuator_led_set(2);
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD PRES) : %s\n", resp);
    }
}

static void handle_response_mel(const char *resp) {
    if      (strcmp(resp, "OK MEL 1") == 0) { printf("[ctrl] Mélodie 1\n"); actuator_buzzer_melody_a(); }
    else if (strcmp(resp, "OK MEL 2") == 0) { printf("[ctrl] Mélodie 2\n"); actuator_buzzer_melody_b(); }
    else if (strcmp(resp, "OK MEL 3") == 0) { printf("[ctrl] Mélodie 3\n"); actuator_buzzer_melody_c(); }
    else { printf("[ctrl] Réponse inattendue (CMD MEL) : %s\n", resp); }
}

/* ------------------------------------------------------------------ */
/* Confirmation mot de passe                                           */
/* ------------------------------------------------------------------ */

static void password_confirm(ControllerState *st) {
    if (strcmp(st->password_buf, PASSWORD_CORRECT) == 0) {
        printf("[ctrl] Mot de passe correct, lancement...\n");
        st->mode = MODE_NORMAL;
        password_reset(st);
        char resp[256];
        if (send_cmd_recv(st, "CMD LU\n", resp, sizeof(resp))) {
            handle_response_launch(st, resp);
        }
    } else {
        printf("[ctrl] Mot de passe incorrect\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
        password_reset(st);
        usleep(1000000U);
        password_show(st);
    }
}

/* ------------------------------------------------------------------ */
/* Dispatch touche en mode PASSWORD                                    */
/* ------------------------------------------------------------------ */

static void handle_key_password(ControllerState *st, int key_num) {
    if (key_num >= 1 && key_num <= 9) {
        password_add_char(st, (char)('0' + key_num));
    } else if (key_num == 10) {
        password_add_char(st, '0');
    } else if (key_num == KEY_CONFIRM) {
        password_confirm(st);
    } else if (key_num == KEY_BACKSPACE) {
        password_backspace(st);
    }
}

/* ------------------------------------------------------------------ */
/* Actions des 8 boutons en mode NORMAL                               */
/* ------------------------------------------------------------------ */

static void action_bt1(ControllerState *st) {
#ifdef USE_WIRINGPI
    printf("[ctrl] BT1 : entrée mode saisie mot de passe\n");
    st->mode = MODE_PASSWORD;
    password_reset(st);
    printf("[ctrl] MDP: [        ] CONF=touche13  BACK=touche14\n");
    fflush(stdout);
#else
    printf("[ctrl] BT1 : saisie mot de passe (simulation)\n");
    printf("Entrez le mot de passe : ");
    fflush(stdout);
    char input[32];
    if (fgets(input, sizeof(input), stdin) == NULL) return;
    char *nl = strchr(input, '\n');
    if (nl) *nl = '\0';
    if (strcmp(input, PASSWORD_CORRECT) == 0) {
        printf("[ctrl] Mot de passe correct, lancement...\n");
        char resp[256];
        if (send_cmd_recv(st, "CMD LU\n", resp, sizeof(resp)))
            handle_response_launch(st, resp);
    } else {
        printf("[ctrl] Mot de passe incorrect\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
    }
#endif
}

static void action_bt2(ControllerState *st) {
    printf("[ctrl] BT2 : atterrissage d'urgence\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD LD\n", resp, sizeof(resp)))
        handle_response_land(st, resp);
}

static void action_bt3(ControllerState *st) {
    printf("[ctrl] BT3 : demande altitude\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD ALT\n", resp, sizeof(resp)))
        handle_response_alt(resp);
}

static void action_bt4(ControllerState *st) {
    printf("[ctrl] BT4 : demande température\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD TEMP\n", resp, sizeof(resp)))
        handle_response_temp(resp);
}

static void action_bt5(ControllerState *st) {
    printf("[ctrl] BT5 : cycle pression\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD PRES\n", resp, sizeof(resp)))
        handle_response_pres(st, resp);
}

static void action_bt6(ControllerState *st) {
    st->melody_idx = (st->melody_idx % 3) + 1;
    printf("[ctrl] BT6 : mélodie %d\n", st->melody_idx);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "CMD MEL %d\n", st->melody_idx);
    char resp[256];
    if (send_cmd_recv(st, cmd, resp, sizeof(resp)))
        handle_response_mel(resp);
}

static void action_bt7(ControllerState *st) {
    printf("[ctrl] BT7 : demande vitesse\n");
    /* La vitesse est mise à jour en temps réel sur le 7-seg via télémétrie.
     * On redemande aussi l'altitude pour synchroniser l'affichage. */
    char resp[256];
    if (send_cmd_recv(st, "CMD ALT\n", resp, sizeof(resp)))
        handle_response_alt(resp);
}

static void action_bt8(ControllerState *st) {
    if (st->fault_active) {
        /* Résolution de la panne active via le cycle CMD PRES */
        printf("[ctrl] BT8 : résolution panne (CMD PRES)\n");
        char resp[256];
        if (send_cmd_recv(st, "CMD PRES\n", resp, sizeof(resp)))
            handle_response_pres(st, resp);
    } else {
        /* Pas de panne : afficher carburant (7-seg) via une CMD ALT proxy */
        printf("[ctrl] BT8 : aucune panne active — info carburant via télémétrie\n");
        actuator_buzzer_bip();
    }
}

/* ------------------------------------------------------------------ */
/* Dispatch touche en mode NORMAL                                      */
/* ------------------------------------------------------------------ */

static void handle_key_normal(ControllerState *st, int key_num) {
    switch (key_num) {
        case KEY_BT1: action_bt1(st); break;
        case KEY_BT2: action_bt2(st); break;
        case KEY_BT3: action_bt3(st); break;
        case KEY_BT4: action_bt4(st); break;
        case KEY_BT5: action_bt5(st); break;
        case KEY_BT6: action_bt6(st); break;
        case KEY_BT7: action_bt7(st); break;
        case KEY_BT8: action_bt8(st); break;
        default: break;
    }
}

static void handle_key(ControllerState *st, int key_num) {
    if (st->mode == MODE_PASSWORD)
        handle_key_password(st, key_num);
    else
        handle_key_normal(st, key_num);
}

/* ------------------------------------------------------------------ */
/* GPIO - scan matrice et directions                                   */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
static int g_prev_matrix[4][4];
static int g_prev_dir_up    = 0;
static int g_prev_dir_down  = 0;
static int g_prev_dir_right = 0;
static int g_prev_dir_left  = 0;

/* Déclarations des pins : définies dans joypi_controller.c */
extern const int ROW_PINS[];
extern const int COL_PINS[];

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
#endif

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
