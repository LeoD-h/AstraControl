/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard_logic.c
 * Description : Logique métier du dashboard (état fusée, commandes, events).
 *               La dynamique physique est dans dashboard_dynamics.c.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "dashboard.h"

#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>

static void uppercase_inplace(char *s) {
    for (; *s; ++s) {
        *s = (char)toupper((unsigned char)*s);
    }
}

static void set_event(RocketState *st, const char *msg) {
    snprintf(st->last_event, sizeof(st->last_event), "%s", msg);
}

static void reset_for_relaunch(RocketState *st) {
    st->launched = false;
    st->landing = false;
    st->exploded = false;
    st->paused = false;
    st->altitude = 0;
    st->downrange = 0;
    st->speed = 0;
    st->fuel = 100;
    st->tilt = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    st->sim_flight = false;
    st->sim_offset = 0;
    st->sim_dir = 1;
    st->sim_tick = 0;
    st->flame_size = 0;
    st->launch_auth_popup = false;
    st->launch_auth_ok = false;
    st->launch_auth_result_ticks = 0;
    st->launch_passlen = 0;
    st->launch_passbuf[0] = '\0';
    st->mission_ms = 0;
}

void init_state(RocketState *st) {
    memset(st, 0, sizeof(*st));
    st->pressure = 1013;
    st->fuel = 100;
    st->temperature = 20;
    st->stage = 1;
    st->running = true;
    st->g_force = 1.0f;
    st->sim_dir = 1;
    st->flame_size = 0;
    snprintf(st->last_event, sizeof(st->last_event), "Awaiting launch command");
}

void apply_cmd(char *line, RocketState *st) {
    char cmd[96];
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    snprintf(cmd, sizeof(cmd), "%s", line);
    char *nl = strchr(cmd, '\n');
    if (nl) {
        *nl = '\0';
    }
    uppercase_inplace(cmd);

    if (!strcmp(cmd, "SPEED_UP") || !strcmp(cmd, "UP")) {
        st->speed += 40;
    } else if (!strcmp(cmd, "SPEED_DOWN") || !strcmp(cmd, "DOWN")) {
        st->speed = st->speed > 40 ? st->speed - 40 : 0;
    } else if (!strcmp(cmd, "PRESSURE_UP")) {
        st->pressure += 2;
    } else if (!strcmp(cmd, "PRESSURE_DOWN")) {
        st->pressure -= 2;
    } else if (!strcmp(cmd, "TILT_LEFT") || !strcmp(cmd, "LEFT")) {
        st->tilt = -1;
        set_event(st, "Guidance: tilt left");
    } else if (!strcmp(cmd, "TILT_RIGHT") || !strcmp(cmd, "RIGHT")) {
        st->tilt = 1;
        set_event(st, "Guidance: tilt right");
    } else if (!strcmp(cmd, "STRAIGHT")) {
        st->tilt = 0;
        set_event(st, "Guidance: straight");
    } else if (!strcmp(cmd, "LAUNCH")) {
        if (!st->launched) {
            st->launch_auth_popup = true;
            st->launch_auth_result_ticks = 0;
            st->launch_passlen = 0;
            st->launch_passbuf[0] = '\0';
            st->launch_auth_ok = false;
            set_event(st, "Launch requested: enter password on dashboard");
        }
    } else if (!strcmp(cmd, "LAUNCH_OK")) {
        /* Lancement confirmé par le JoyPi (auth déjà faite côté physique). */
        if (!st->launched) {
            st->launched = true;
            st->launch_auth_popup = false;
            st->fuel  = 100;
            st->speed = 0;
            set_event(st, "Launch confirmed (JoyPi auth OK)");
        }
    } else if (!strcmp(cmd, "LAND")) {
        st->landing = true;
        set_event(st, "Landing sequence armed");
    } else if (!strcmp(cmd, "TEST_MELODY_1")) {
        st->melody_test = 1;
        st->melody_ticks = 18;
        set_event(st, "Melody test 1 running");
    } else if (!strcmp(cmd, "TEST_MELODY_2")) {
        st->melody_test = 2;
        st->melody_ticks = 18;
        set_event(st, "Melody test 2 running");
    } else if (!strcmp(cmd, "TEST_MELODY_3")) {
        st->melody_test = 3;
        st->melody_ticks = 18;
        set_event(st, "Melody test 3 running");
    } else if (!strcmp(cmd, "FIX_PROBLEM")) {
        st->problem_active = false;
        st->alerts[0] = false;
        st->alerts[1] = false;
        st->alerts[2] = false;
        reset_for_relaunch(st);
        set_event(st, "Anomaly resolved: mission reset ready");
    } else if (!strcmp(cmd, "PROBLEM")) {
        st->problem_active = true;
        st->alerts[2] = true;
        set_event(st, "Anomaly injected");
    } else if (!strcmp(cmd, "ALERT1")) {
        st->alerts[0] = true;
        set_event(st, "Alert 1 test triggered");
    } else if (!strcmp(cmd, "ALERT2")) {
        st->alerts[1] = true;
        set_event(st, "Alert 2 test triggered");
    } else if (!strcmp(cmd, "ALERT3")) {
        st->alerts[2] = true;
        set_event(st, "Alert 3 test triggered");
    } else if (!strcmp(cmd, "CLEAR_ALERTS")) {
        st->alerts[0] = st->alerts[1] = st->alerts[2] = false;
        set_event(st, "Alerts cleared");
    } else if (!strcmp(cmd, "SIM_FLIGHT ON")) {
        st->sim_flight = true;
        set_event(st, "Flight simulation enabled");
    } else if (!strcmp(cmd, "SIM_FLIGHT OFF")) {
        st->sim_flight = false;
        st->sim_offset = 0;
        set_event(st, "Flight simulation disabled");
    } else if (!strcmp(cmd, "PAUSE")) {
        st->paused = true;
    } else if (!strcmp(cmd, "RESUME")) {
        st->paused = false;
    } else if (!strcmp(cmd, "EXPLODE")) {
        st->exploded = true;
        st->alerts[0] = st->alerts[1] = st->alerts[2] = true;
        st->speed = 0;
        set_event(st, "Critical failure: vehicle lost");
    } else if (!strcmp(cmd, "QUIT")) {
        st->running = false;
    }
}

void apply_data(char *line, RocketState *st) {
    char cmd[96];
    char key[32];
    int value = 0;

    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    snprintf(cmd, sizeof(cmd), "%s", line);
    char *nl = strchr(cmd, '\n');
    if (nl) {
        *nl = '\0';
    }
    uppercase_inplace(cmd);

    if (sscanf(cmd, "SET %31s %d", key, &value) == 2) {
        if (!strcmp(key, "SPEED")) {
            st->speed = value;
        } else if (!strcmp(key, "PRESSURE")) {
            st->pressure = value;
        } else if (!strcmp(key, "ALTITUDE")) {
            st->altitude = value;
        } else if (!strcmp(key, "FUEL")) {
            st->fuel = value;
        } else if (!strcmp(key, "TEMP") || !strcmp(key, "TEMPERATURE")) {
            st->temperature = value;
        } else if (!strcmp(key, "THRUST")) {
            st->thrust_kn = value;
        }
        set_event(st, "External data injected");
        return;
    }

    if (!strcmp(cmd, "PROBLEM ON")) {
        st->problem_active = true;
        st->alerts[2] = true;
        set_event(st, "External anomaly ON");
    } else if (!strcmp(cmd, "PROBLEM OFF")) {
        st->problem_active = false;
        set_event(st, "External anomaly OFF");
    } else if (!strcmp(cmd, "ALERT1 ON")) {
        st->alerts[0] = true;
    } else if (!strcmp(cmd, "ALERT1 OFF")) {
        st->alerts[0] = false;
    } else if (!strcmp(cmd, "ALERT2 ON")) {
        st->alerts[1] = true;
    } else if (!strcmp(cmd, "ALERT2 OFF")) {
        st->alerts[1] = false;
    } else if (!strcmp(cmd, "ALERT3 ON")) {
        st->alerts[2] = true;
    } else if (!strcmp(cmd, "ALERT3 OFF")) {
        st->alerts[2] = false;
    } else if (!strcmp(cmd, "CLEAR_ALERTS")) {
        st->alerts[0] = st->alerts[1] = st->alerts[2] = false;
    }
}

void handle_local_input(RocketState *st, int ch) {
    if (!st->launch_auth_popup) {
        return;
    }

    if (ch == ERR) {
        return;
    }

    if (ch == '\n' || ch == '\r' || ch == 10 || ch == KEY_ENTER) {
        if (!strcmp(st->launch_passbuf, "123")) {
            st->launch_auth_popup = false;
            st->launch_auth_ok = true;
            st->launch_auth_result_ticks = 20;
            set_event(st, "Launch authorized (password OK), waiting ignition");
        } else {
            st->launch_auth_ok = false;
            st->launch_auth_result_ticks = 20;
            st->launch_passlen = 0;
            st->launch_passbuf[0] = '\0';
            set_event(st, "Launch denied (bad password)");
        }
        return;
    }

    if (ch == 127 || ch == 8 || ch == KEY_BACKSPACE) {
        if (st->launch_passlen > 0) {
            st->launch_passlen--;
            st->launch_passbuf[st->launch_passlen] = '\0';
        }
        return;
    }

    if (isdigit((unsigned char)ch) && st->launch_passlen < (int)sizeof(st->launch_passbuf) - 1) {
        st->launch_passbuf[st->launch_passlen++] = (char)ch;
        st->launch_passbuf[st->launch_passlen] = '\0';
    }
}
