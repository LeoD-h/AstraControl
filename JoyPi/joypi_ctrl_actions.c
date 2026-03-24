/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_actions.c
 * Description : Mot de passe, réponses aux commandes satellite
 *               et actions des 8 boutons de mission.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include "joypi_ctrl_actions.h"
#include "joypi_ctrl_net.h"
#include "actuators.h"
#include "ir_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Mot de passe                                                        */
/* ------------------------------------------------------------------ */

static void password_confirm(ControllerState *st);

static void password_reset(ControllerState *st) {
    memset(st->password_buf, 0, sizeof(st->password_buf));
    st->password_len = 0;
    cmd_pipe_write(st, "PASSRESET\n");
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
    cmd_pipe_write(st, "PASSCHAR\n");
    password_show(st);
    if (st->password_len == 3)
        password_confirm(st);
}

static void password_backspace(ControllerState *st) {
    if (st->password_len > 0) {
        st->password_buf[--st->password_len] = '\0';
        cmd_pipe_write(st, "PASSBACK\n");
        password_show(st);
    } else {
        printf("[ctrl] Saisie mot de passe annulée\n");
        ir_disarm();
        st->mode = MODE_NORMAL;
        password_reset(st);
    }
}

/* ------------------------------------------------------------------ */
/* Réponses aux commandes                                              */
/* ------------------------------------------------------------------ */

static void handle_response_launch(ControllerState *st, const char *resp) {
    (void)st;
    if (strcmp(resp, "OK LAUNCH") == 0) {
        /* Le satellite va broadcaster EVENT LAUNCH → handle_event s'occupe
         * des actuateurs (LED, matrice, mélodie) et de LAUNCH_OK pipe.
         * On ne fait rien ici pour éviter la double exécution bloquante. */
        printf("[ctrl] CMD LU accepté — EVENT LAUNCH attendu du satellite\n");
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
        actuator_led_green_blink(4);
        actuator_matrix_emergency();
        actuator_buzzer_bip();
    } else if (strcmp(resp, "FAIL NOT_IN_ORBIT") == 0) {
        printf("[ctrl] ECHEC atterrissage : orbite non atteinte\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
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
        actuator_segment_show(val / 1000);
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
        actuator_buzzer_bip();
    } else if (strcmp(resp, "OK PRES_FIX") == 0) {
        printf("[ctrl] PRESSION : correcteur activé\n");
        data_pipe_write(st, "PROBLEM OFF\n");
        cmd_pipe_write(st, "CLEAR_ALERTS\n");
        st->fault_active = false;
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
        ir_disarm();
        st->mode = MODE_NORMAL;
        st->explosion_notified = false;
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
    st->explosion_notified = false;
    password_reset(st);
    ir_arm();
    cmd_pipe_write(st, "LAUNCH\n");  /* déclenche popup mot de passe dans controle_fusee */
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
    if (send_cmd_recv(st, "CMD ALT\n", resp, sizeof(resp))) {
        handle_response_alt(resp);
        st->seg_override = false;
    }
}

static void action_bt4(ControllerState *st) {
    printf("[ctrl] BT4 : demande température\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD TEMP\n", resp, sizeof(resp))) {
        handle_response_temp(resp);
        st->seg_override = true;  /* affichage reste jusqu'au prochain bouton */
    }
}

static void action_bt5(ControllerState *st) {
    printf("[ctrl] BT5 : cycle pression\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD PRES\n", resp, sizeof(resp)))
        handle_response_pres(st, resp);
}

static void action_bt6(ControllerState *st) {
    /* Cycle mélodies 1→2→3→1 à chaque pression */
    st->melody_idx = (st->melody_idx % 3) + 1;
    printf("[ctrl] BT6 : mélodie test %d\n", st->melody_idx);
    char cmd_buf[32];
    snprintf(cmd_buf, sizeof(cmd_buf), "CMD MEL %d\n", st->melody_idx);
    char resp[256];
    if (send_cmd_recv(st, cmd_buf, resp, sizeof(resp)))
        handle_response_mel(resp);
}

static void handle_response_rep1(ControllerState *st, const char *resp) {
    if (strcmp(resp, "OK REP1_FIX") == 0) {
        printf("[ctrl] PANNE 1 RESOLUE (temperature normalisee)\n");
        st->fault1_active = false;
        if (!st->fault2_active && !st->fault_active) cmd_pipe_write(st, "CLEAR_ALERTS\n");
        actuator_led_set(2);
        actuator_buzzer_bip();
    } else if (strcmp(resp, "OK REP1_NONE") == 0) {
        printf("[ctrl] BT7 : aucune panne temperature active\n");
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD REP1) : %s\n", resp);
    }
}

static void handle_response_rep2(ControllerState *st, const char *resp) {
    if (strcmp(resp, "OK REP2_FIX") == 0) {
        printf("[ctrl] PANNE 2 RESOLUE (stress structurel reduit)\n");
        st->fault2_active = false;
        if (!st->fault1_active && !st->fault_active) cmd_pipe_write(st, "CLEAR_ALERTS\n");
        actuator_led_set(2);
        actuator_buzzer_bip();
    } else if (strcmp(resp, "OK REP2_NONE") == 0) {
        printf("[ctrl] BT8 : aucune panne stress active\n");
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] Réponse inattendue (CMD REP2) : %s\n", resp);
    }
}

static void action_bt7(ControllerState *st) {
    printf("[ctrl] BT7 : résolution panne temperature (CMD REP1)\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD REP1\n", resp, sizeof(resp)))
        handle_response_rep1(st, resp);
    else
        actuator_buzzer_bip();
}

static void action_bt8(ControllerState *st) {
    printf("[ctrl] BT8 : résolution panne stress (CMD REP2)\n");
    char resp[256];
    if (send_cmd_recv(st, "CMD REP2\n", resp, sizeof(resp)))
        handle_response_rep2(st, resp);
    else
        actuator_buzzer_bip();
}

/* ------------------------------------------------------------------ */
/* Dispatch touche en mode NORMAL                                      */
/* ------------------------------------------------------------------ */

static void handle_key_normal(ControllerState *st, int key_num) {
    /* Tout bouton sauf BT3/BT4 libère le verrou affichage 7-seg */
    if (key_num != KEY_BT3 && key_num != KEY_BT4)
        st->seg_override = false;

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

void handle_key(ControllerState *st, int key_num) {
    if (st->mode == MODE_PASSWORD)
        handle_key_password(st, key_num);
    else
        handle_key_normal(st, key_num);
}
