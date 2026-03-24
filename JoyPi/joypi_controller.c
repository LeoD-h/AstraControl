/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_controller.c
 * Description : Programme principal JoyPi — boucle principale, GPIO setup, main.
 *               Réseau    → joypi_ctrl_net.c
 *               Boutons   → joypi_ctrl_keys.c
 *               Afficheurs → actuators.c + actuators_display.c
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "joypi_controller.h"
#include "joypi_ctrl_net.h"
#include "joypi_ctrl_keys.h"
#include "actuators.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

/* ------------------------------------------------------------------ */
/* Global stop (exporté via joypi_controller.h)                       */
/* ------------------------------------------------------------------ */

volatile int g_stop = 0;

/* ------------------------------------------------------------------ */
/* GPIO pins (exportées vers joypi_ctrl_keys.c)                       */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
const int ROW_PINS[4] = {13, 15, 29, 31};
/* Colonnes : toutes disponibles, partagées avec joystick (gérées par read_direction). */
const int COL_PINS[4] = {37, 22, 35, 33};
#endif

/* ------------------------------------------------------------------ */
/* Signal                                                              */
/* ------------------------------------------------------------------ */

static void handle_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

/* ------------------------------------------------------------------ */
/* Setup GPIO                                                          */
/* ------------------------------------------------------------------ */

#ifdef USE_WIRINGPI
static void setup_gpio(void) {
    int r, c;
    wiringPiSetupPhys();
    for (r = 0; r < 4; ++r) {
        pinMode(ROW_PINS[r], INPUT);
        pullUpDnControl(ROW_PINS[r], PUD_UP);
    }
    for (c = 0; c < 4; ++c) {
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], HIGH);
    }
    actuator_init();
}
#endif

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *ip   = DEFAULT_SAT_IP;
    int         port = DEFAULT_SAT_PORT;

    if (argc > 1) ip   = argv[1];
    if (argc > 2) {
        int p = atoi(argv[2]);
        if (p > 0) port = p;
    }

    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    printf("[ctrl] JoyPi Controller v1.1\n");
    printf("[ctrl] Satellite : %s:%d\n", ip, port);

    ControllerState st;
    state_init(&st, ip, port);

#ifdef USE_WIRINGPI
    setup_gpio();
    printf("[ctrl] Mode GPIO réel (wiringPi, physique)\n");
    printf("[ctrl] Rows: 13,15,29,31  |  Cols actifs boutons: pin33(col3) pin35(col2)\n");
    printf("[ctrl] BT1(1)=LAUNCH BT2(2)=LAND BT3(5)=ALT BT4(6)=TEMP\n");
    printf("[ctrl] BT5(9)=PRES BT6(10)=MEL BT7(13)=REP1 BT8(14)=REP2\n");
    printf("[ctrl] DIR UP/DOWN/LEFT/RIGHT → commandes dashboard ncurses\n");
#else
    actuator_init();
    printf("[ctrl] Mode simulation clavier\n");
    printf("[ctrl] 1=LAUNCH 2=LAND 3=ALT 4=TEMP 5=PRES 6=MEL 7=REP1 8=REP2\n");
    printf("[ctrl] u/d/r/l=directions  q=quitter\n");
#endif

    /* Ouverture initiale des pipes */
    st.cmd_pipe_fd  = -1;
    st.data_pipe_fd = -1;
    st.auth_pipe_fd = -1;

    /* Première tentative de connexion */
    try_reconnect(&st);

    while (!g_stop) {
        /* 1. Reconnexion si nécessaire */
        if (st.sat_fd < 0 || !st.authed) {
            try_reconnect(&st);
        }

        /* 2. Réception push satellite (non-bloquant, timeout 80ms inclus) */
        poll_satellite_push(&st);

        /* 3. Scanner boutons GPIO (ou stdin en simulation) */
        scan_buttons_and_handle(&st);

        /* 4. Auth pipe : décollage initié depuis le clavier de controle_fusee */
        poll_auth_pipe(&st);
    }

    printf("\n[ctrl] Arrêt demandé\n");

    if (st.sat_fd      >= 0) close(st.sat_fd);
    if (st.auth_pipe_fd>= 0) close(st.auth_pipe_fd);
    if (st.cmd_pipe_fd >= 0) close(st.cmd_pipe_fd);
    if (st.data_pipe_fd>= 0) close(st.data_pipe_fd);

    printf("[ctrl] Arrêté proprement\n");
    return 0;
}
