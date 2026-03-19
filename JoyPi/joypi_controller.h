/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_controller.h
 * Description : Types partagés et constantes du contrôleur JoyPi.
 *               Inclus par joypi_ctrl_net.h et joypi_ctrl_keys.h.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#ifndef JOYPI_CONTROLLER_H
#define JOYPI_CONTROLLER_H

#include <stdbool.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */

#define DEFAULT_SAT_IP   "192.168.64.7"
#define DEFAULT_SAT_PORT 5555

#define CMD_PIPE  "/tmp/rocket_cmd.pipe"
#define DATA_PIPE "/tmp/rocket_data.pipe"
#define AUTH_PIPE "/tmp/rocket_auth.pipe"

#define RECONNECT_DELAY_S 2
#define CMD_TIMEOUT_S     2
#define POLL_TIMEOUT_MS   80

/* Numéros logiques des touches physiques
 * COL_PINS = {37, 22, 35, 33} (partagés avec joystick — gérés par read_direction)
 * key_num = r*4 + (4-c)
 *   c=0/pin37 → keys 4,8,12,16  |  c=1/pin22 → keys 3,7,11,15
 *   c=2/pin35 → keys 2,6,10,14  |  c=3/pin33 → keys 1,5,9,13   */
#define KEY_BT1        3   /* row1, col1 : LANCEMENT (saisie mdp)     */
#define KEY_BT2        4   /* row1, col0 : ATTERRISSAGE URGENCE        */
#define KEY_BT3        7   /* row2, col1 : DEMANDE ALTITUDE             */
#define KEY_BT4        8   /* row2, col0 : DEMANDE TEMPERATURE          */
#define KEY_BT5       11   /* row3, col1 : CYCLE PRESSION               */
#define KEY_BT6       12   /* row3, col0 : MELODIE (cycle 1..3)         */
#define KEY_BT7       15   /* row4, col1 : RÉSOLUTION PANNE TEMP (REP1) */
#define KEY_BT8       16   /* row4, col0 : RÉSOLUTION PANNE STRESS (REP2)*/

/* Mode saisie mot de passe */
#define MODE_NORMAL       0
#define MODE_PASSWORD     1
#define PASSWORD_MAX_LEN  8
#define PASSWORD_CORRECT  "123"

/* Touches spéciales en mode mot de passe */
#define KEY_CONFIRM    13   /* row4, col3 (pin33) — confirmer */
#define KEY_BACKSPACE  14   /* row4, col2 (pin35) — effacer   */

/* ------------------------------------------------------------------ */
/* Structure état contrôleur                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    int    sat_fd;
    int    cmd_pipe_fd;
    int    data_pipe_fd;
    int    auth_pipe_fd;
    char   sat_ip[64];
    int    sat_port;
    bool   authed;
    int    melody_idx;
    time_t last_reconnect_attempt;
    /* Mode saisie mot de passe */
    int    mode;
    char   password_buf[PASSWORD_MAX_LEN + 1];
    int    password_len;
    /* Suivi pannes actives */
    bool   fault_active;    /* panne générique (CMD PRES / EVENT PROBLEM) */
    bool   fault1_active;   /* panne 1 : temperature (EVENT PROBLEM1)     */
    bool   fault2_active;   /* panne 2 : stress structurel (EVENT PROBLEM2) */
    /* Dernière vitesse reçue en télémétrie */
    int    last_speed;
} ControllerState;

/* Global stop (défini dans joypi_controller.c, utilisé dans ctrl_keys.c) */
extern volatile int g_stop;

#endif /* JOYPI_CONTROLLER_H */
