/************************************************************
 * Projet      : Fusée
 * Fichier     : control_ncurses.c
 * Description : Interface de controle mission en ncurses (commandes operateur).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "dashboard_common.h"

#include <curses.h>

#define CMD_PIPE "/tmp/rocket_cmd.pipe"

int main(int argc, char **argv) {
    bool socket_mode = false;
    const char *server_ip = "127.0.0.1";
    int server_port = 5555;
    int pipe_fd = -1;

    if (argc > 1) {
        socket_mode = true;
        server_ip = argv[1];
        if (argc > 2) {
            server_port = atoi(argv[2]);
            if (server_port <= 0) server_port = 5555;
        }
        pipe_fd = connect_socket_fd(server_ip, server_port);
    } else {
        pipe_fd = setup_fifo_writer(CMD_PIPE);
    }

    bool quit = false;
    char last_cmd[48] = "NONE";
    char last_reply[128] = "NONE";

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(150);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_RED, -1);
    }

    while (!quit) {
        clear();

        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(1, 2, "ROCKET CONTROL CENTER (NCURSES)");
        attroff(A_BOLD | COLOR_PAIR(1));

        if (socket_mode) {
            mvprintw(3, 2, "Mode socket : %s:%d", server_ip, server_port);
            mvprintw(4, 2, "Lien socket_bridge_server: %s", pipe_fd >= 0 ? "OK" : "EN ATTENTE");
        } else {
            mvprintw(3, 2, "Pipe commande : %s", CMD_PIPE);
            mvprintw(4, 2, "Lien dashboard: %s", pipe_fd >= 0 ? "OK" : "EN ATTENTE (lance ./bin-util/controle_fusee)");
        }

        mvhline(6, 2, '=', 82);
        mvprintw(7, 2, "PILOTAGE AZERTY");
        mvprintw(8, 2, "q = gauche, d = droite, s = droit");
        mvprintw(9, 2, "Fleche HAUT/BAS = vitesse +/-, + / - = pression +/-");

        mvhline(11, 2, '-', 82);
        mvprintw(12, 2, "ACTIONS MISSION");
        mvprintw(13, 2, "t = decollage (mdp popup=123) | a = atterrissage | 1/2/3 = test melodie");
        mvprintw(14, 2, "r = regler probleme | e = exploser");
        mvprintw(15, 2, "f = sim flight ON | g = sim flight OFF");

        mvhline(17, 2, '-', 82);
        mvprintw(18, 2, "TEST ALERTES");
        mvprintw(19, 2, "z = Alerte1 LowFuel | x = Alerte2 HighG | c = Alerte3 Guidance");
        mvprintw(20, 2, "v = clear alertes | p = pause | o = resume");

        mvhline(22, 2, '-', 82);
        mvprintw(23, 2, "0 = quitter dashboard (QUIT) | ESC = quitter seulement ce controleur");

        attron(COLOR_PAIR(2));
        mvprintw(24, 2, "Derniere commande envoyee: %s", last_cmd);
        attroff(COLOR_PAIR(2));

        refresh();

        if (pipe_fd < 0) {
            if (socket_mode) {
                pipe_fd = connect_socket_fd(server_ip, server_port);
            } else {
                pipe_fd = setup_fifo_writer(CMD_PIPE);
            }
        }

        int ch = getch();
        const char *cmd = NULL;

        switch (ch) {
            case KEY_UP: cmd = "SPEED_UP"; break;
            case KEY_DOWN: cmd = "SPEED_DOWN"; break;
            case '+': cmd = "PRESSURE_UP"; break;
            case '-': cmd = "PRESSURE_DOWN"; break;
            case 'q': case 'Q': cmd = "TILT_LEFT"; break;
            case 'd': case 'D': cmd = "TILT_RIGHT"; break;
            case 's': case 'S': cmd = "STRAIGHT"; break;
            case 't': case 'T': cmd = "LAUNCH"; break;
            case 'a': case 'A': cmd = "LAND"; break;
            case '1': cmd = "TEST_MELODY_1"; break;
            case '2': cmd = "TEST_MELODY_2"; break;
            case '3': cmd = "TEST_MELODY_3"; break;
            case 'r': case 'R': cmd = "FIX_PROBLEM"; break;
            case 'e': case 'E': cmd = "EXPLODE"; break;
            case 'z': case 'Z': cmd = "ALERT1"; break;
            case 'x': case 'X': cmd = "ALERT2"; break;
            case 'c': case 'C': cmd = "ALERT3"; break;
            case 'v': case 'V': cmd = "CLEAR_ALERTS"; break;
            case 'p': case 'P': cmd = "PAUSE"; break;
            case 'o': case 'O': cmd = "RESUME"; break;
            case 'f': case 'F': cmd = "SIM_FLIGHT ON"; break;
            case 'g': case 'G': cmd = "SIM_FLIGHT OFF"; break;
            case '0': cmd = "QUIT"; quit = true; break;
            case 27: quit = true; break;
            default: break;
        }

        if (cmd && pipe_fd >= 0) {
            send_line_with_reply(pipe_fd, cmd, socket_mode, last_reply, sizeof(last_reply));
            snprintf(last_cmd, sizeof(last_cmd), "%s", cmd);
        }
    }

    endwin();
    if (pipe_fd >= 0) {
        close(pipe_fd);
    }
    return 0;
}
