/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard_visuals.c
 * Description : Rendu graphique ncurses du dashboard et des widgets mission.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include "dashboard.h"

#include <curses.h>
#include <stdbool.h>
#include <string.h>

static void draw_bar(int y, int x, const char *label, int value, int max_value, int width, int pair) {
    int fill = 0;
    if (max_value > 0) {
        fill = (value * width) / max_value;
    }
    if (fill < 0) {
        fill = 0;
    }
    if (fill > width) {
        fill = width;
    }

    mvprintw(y, x, "%s [", label);
    attron(COLOR_PAIR(pair));
    for (int i = 0; i < width; ++i) {
        addch(i < fill ? '#' : ' ');
    }
    attroff(COLOR_PAIR(pair));
    addch(']');
}

static void draw_flame(int y, int x, int size) {
    if (size <= 0) {
        return;
    }

    attron(COLOR_PAIR(3) | A_BOLD);
    if (size >= 1) mvprintw(y + 0, x, "   /\\   ");
    if (size >= 2) mvprintw(y + 1, x, "  /  \\  ");
    if (size >= 3) mvprintw(y + 2, x, " /_/\\_\\ ");
    if (size >= 4) mvprintw(y + 3, x, "   |||   ");
    attroff(COLOR_PAIR(3) | A_BOLD);
}

static void draw_rocket(int y, int x, int tilt, int sim_offset, int flame_size, bool exploded) {
    static const char *rocket_straight[] = {
        "             /\\",
        "            /  \\",
        "           / /\\ \\",
        "          / /  \\ \\",
        "         /_/____\\_\\",
        "         |  NASA  |",
        "         |   ||   |",
        "         |  [##]  |",
        "         |   ||   |",
        "         |  [##]  |",
        "         |   ||   |",
        "        /|_________|\\",
        "       /_|  /   \\  |_\\",
        "         /__/     \\__\\",
        "           /_/ \\_\\",
        "            /_/\\_\\",
        "             /_\\",
        "            _/ \\_",
        "           /_____\\",
        "            |||",
        "           |||||",
        "          |||||||",
        "         ||| |||",
        "          |||||",
        "           |||",
        "            |"
    };

    int shift = (tilt < 0) ? -2 : ((tilt > 0) ? 2 : 0);
    int draw_x = x + shift + sim_offset;
    if (draw_x < 0) {
        draw_x = 0;
    }

    attron(COLOR_PAIR(exploded ? 5 : 2));
    for (int i = 0; i < 26; ++i) {
        mvprintw(y + i, draw_x, "%s", rocket_straight[i]);
    }
    attroff(COLOR_PAIR(exploded ? 5 : 2));

    if (!exploded) {
        draw_flame(y + 26, draw_x + 10, flame_size);
    } else {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(y + 10, draw_x + 25, "*** BOOM ***");
        attroff(COLOR_PAIR(5) | A_BOLD);
    }
}

static void draw_logo(int y, int x) {
    attron(COLOR_PAIR(4));
    mvprintw(y + 0, x, "   _      _____    _");
    mvprintw(y + 1, x, "  | |    |_   _|  | |");
    mvprintw(y + 2, x, "  | |___   | |    | |___");
    mvprintw(y + 3, x, "  |_____|  |_|    |_____|");
    mvprintw(y + 4, x, "   LEO  |  INES  | JULIANN");
    mvprintw(y + 6, x, "  Mission Crew Symbol");
    attroff(COLOR_PAIR(4));
}

static void draw_center_alert_popup(const RocketState *st, int rows, int cols) {
    if (!(st->alerts[0] || st->alerts[1] || st->alerts[2] || st->problem_active)) {
        return;
    }

    int blink_on = ((st->mission_ms / 250) % 2) == 0;
    if (!blink_on) {
        return;
    }

    int w = 48;
    int h = 7;
    int y = (rows - h) / 2;
    int x = (cols - w) / 2;

    attron(A_BOLD | COLOR_PAIR(5));
    for (int i = 0; i < h; ++i) {
        mvhline(y + i, x, ' ', w);
    }
    mvprintw(y + 1, x + 2, "!!! ALERTE CRITIQUE MISSION !!!");
    mvprintw(y + 3, x + 2, "A1:%s  A2:%s  A3:%s",
             st->alerts[0] ? "ON" : "OFF",
             st->alerts[1] ? "ON" : "OFF",
             st->alerts[2] ? "ON" : "OFF");
    mvprintw(y + 5, x + 2, "Verifier guidage / carburant / G-load");
    attroff(A_BOLD | COLOR_PAIR(5));
}

static void draw_launch_password_popup(const RocketState *st, int rows, int cols) {
    if (!st->launch_auth_popup && st->launch_auth_result_ticks <= 0) {
        return;
    }

    int w = 50;
    int h = 9;
    int y = (rows - h) / 2;
    int x = (cols - w) / 2;

    attron(A_BOLD | COLOR_PAIR(1));
    for (int i = 0; i < h; ++i) {
        mvhline(y + i, x, ' ', w);
    }
    mvprintw(y + 1, x + 2, "AUTHENTIFICATION DECOLLAGE");
    mvprintw(y + 3, x + 2, "Mot de passe requis pour launch");

    if (st->launch_auth_popup) {
        char masked[20];
        int n = st->launch_passlen;
        if (n < 0) n = 0;
        if (n > 15) n = 15;
        for (int i = 0; i < n; ++i) masked[i] = '*';
        masked[n] = '\0';
        mvprintw(y + 5, x + 2, "Password: %s", masked);
        mvprintw(y + 7, x + 2, "Entrer mdp puis ENTER (attendu: 123)");
    } else {
        if (st->launch_auth_ok) {
            attron(COLOR_PAIR(4));
            mvprintw(y + 5, x + 2, "ACCES AUTORISE - DECOLLAGE EN COURS");
            attroff(COLOR_PAIR(4));
        } else {
            attron(COLOR_PAIR(5));
            mvprintw(y + 5, x + 2, "ACCES REFUSE - MOT DE PASSE INVALIDE");
            attroff(COLOR_PAIR(5));
        }
    }
    attroff(A_BOLD | COLOR_PAIR(1));
}

void draw_dashboard(const RocketState *st, int rows, int cols, const char *cmd_pipe, const char *data_pipe) {
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(0, 2, "MISSION DASHBOARD | MODE: %s | CMD=%s | DATA=%s",
             st->exploded ? "LOSS" : (st->landing ? "LANDING" : (st->launched ? "ASCENT" : "READY")),
             cmd_pipe, data_pipe);
    attroff(A_BOLD | COLOR_PAIR(1));

    draw_rocket(2, 2, st->tilt, st->sim_offset, st->flame_size, st->exploded);
    draw_logo(rows - 9, 2);

    int px = cols - 58;
    int y = 2;

    attron(COLOR_PAIR(1));
    mvhline(y, px, '=', 56);
    mvprintw(y + 1, px + 1, "FLIGHT / PROPULSION TELEMETRY");
    mvhline(y + 2, px, '-', 56);
    attroff(COLOR_PAIR(1));

    mvprintw(y + 4, px + 1, "T+               : %02d:%02d", (st->mission_ms / 1000) / 60, (st->mission_ms / 1000) % 60);
    mvprintw(y + 5, px + 1, "Velocity         : %6d km/h", st->speed);
    mvprintw(y + 6, px + 1, "Altitude         : %6d m", st->altitude);
    mvprintw(y + 7, px + 1, "Downrange        : %6d m", st->downrange);
    mvprintw(y + 8, px + 1, "Cabin Pressure   : %6d hPa", st->pressure);
    mvprintw(y + 9, px + 1, "Thrust           : %6d kN", st->thrust_kn);
    mvprintw(y + 10, px + 1, "Acceleration     : %6.2f m/s2", st->accel_ms2);
    mvprintw(y + 11, px + 1, "G-Load           : %6.2f g", st->g_force);
    mvprintw(y + 12, px + 1, "Stage            : S-%d", st->stage);
    mvprintw(y + 13, px + 1, "Guidance         : %s", st->paused ? "HOLD" : "NOMINAL");
    mvprintw(y + 14, px + 1, "Attitude P/Y/R   : %3d / %3d / %3d deg", st->pitch, st->yaw, st->roll);
    mvprintw(y + 15, px + 1, "Apo/Peri Est.    : %6d / %6d m", st->apogee, st->periapsis);
    mvprintw(y + 16, px + 1, "Inclinaison cmd  : %s", st->tilt < 0 ? "LEFT" : (st->tilt > 0 ? "RIGHT" : "STRAIGHT"));
    mvprintw(y + 17, px + 1, "Sim flight       : %s", st->sim_flight ? "ON" : "OFF");

    draw_bar(y + 18, px + 1, "Fuel", st->fuel, 100, 28, st->fuel < 20 ? 5 : 4);
    mvprintw(y + 18, px + 40, "%3d %%", st->fuel);
    draw_bar(y + 19, px + 1, "Thermal", st->temperature, 120, 28, st->temperature > 95 ? 5 : 3);
    mvprintw(y + 19, px + 40, "%3d C", st->temperature);

    attron(COLOR_PAIR(1));
    mvhline(y + 21, px, '-', 56);
    mvprintw(y + 22, px + 1, "ALERT MATRIX");
    mvhline(y + 23, px, '-', 56);
    attroff(COLOR_PAIR(1));

    mvprintw(y + 24, px + 1, "A1 LOW FUEL      : %s", st->alerts[0] ? "ACTIVE" : "OFF");
    mvprintw(y + 25, px + 1, "A2 HIGH G LOAD   : %s", st->alerts[1] ? "ACTIVE" : "OFF");
    mvprintw(y + 26, px + 1, "A3 GUIDANCE FAIL : %s", st->alerts[2] ? "ACTIVE" : "OFF");

    if (st->melody_test > 0) {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(y + 28, px + 1, "AUDIO TEST       : MELODY-%d RUNNING", st->melody_test);
        attroff(COLOR_PAIR(3) | A_BOLD);
    } else {
        mvprintw(y + 28, px + 1, "AUDIO TEST       : IDLE");
    }

    if (st->alerts[0] || st->alerts[1] || st->alerts[2] || st->exploded) {
        attron(A_BOLD | COLOR_PAIR(5));
        mvprintw(rows - 2, px + 1, "MASTER WARNING: CHECK ALERT MATRIX IMMEDIATELY");
        attroff(A_BOLD | COLOR_PAIR(5));
    } else {
        attron(COLOR_PAIR(4));
        mvprintw(rows - 2, px + 1, "SYSTEM STATUS: ALL NOMINAL");
        attroff(COLOR_PAIR(4));
    }

    mvprintw(rows - 4, px + 1, "Last event: %s", st->last_event);
    mvprintw(rows - 3, px + 1, "Control UI: ./controle_fusee_control | Data UI: ./controle_fusee_data");
    mvprintw(rows - 1, px + 1, "Socket bridge: ./socket_bridge_server 5555");

    draw_center_alert_popup(st, rows, cols);
    draw_launch_password_popup(st, rows, cols);
}
