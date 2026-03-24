/************************************************************
 * Projet      : Fusée
 * Fichier     : main.c
 * Description : Point d'entree du dashboard fusee ncurses (affichage temps reel).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dashboard.h"
#include "pipes.h"

#define FRAME_MS 120
#define CMD_PIPE  "/tmp/rocket_cmd.pipe"
#define DATA_PIPE "/tmp/rocket_data.pipe"
#define AUTH_PIPE "/tmp/rocket_auth.pipe"

static void on_exit_signal(int sig) {
    (void)sig;
    endwin();   /* restaurer le terminal avant de quitter */
    _exit(0);
}

int main(void) {
    signal(SIGINT,  on_exit_signal);
    signal(SIGTERM, on_exit_signal);
    RocketState st;
    init_state(&st);

    int keep_cmd = -1;
    int keep_data = -1;
    int cmd_fd  = setup_pipe_reader(CMD_PIPE,  &keep_cmd);
    int data_fd = setup_pipe_reader(DATA_PIPE, &keep_data);

    /* Auth pipe : écriture vers joypi_controller (clavier → CMD LU) */
    mkfifo(AUTH_PIPE, 0666);
    st.auth_pipe_fd = open(AUTH_PIPE, O_WRONLY | O_NONBLOCK);
    /* ENXIO si joypi_controller pas encore connecté — réessayé à la validation */

    char pending_cmd[512] = {0};
    size_t used_cmd = 0;
    char pending_data[512] = {0};
    size_t used_data = 0;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);
        init_pair(2, COLOR_WHITE, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_GREEN, -1);
        init_pair(5, COLOR_RED, -1);
    }

    while (st.running) {
        int rows, cols;
        int ch = getch();
        getmaxyx(stdscr, rows, cols);
        clear();

        if (rows < 36 || cols < 130) {
            mvprintw(1, 2, "Terminal trop petit. Min: 130x36 (actuel: %dx%d)", cols, rows);
            mvprintw(2, 2, "Dashboard: ./controle_fusee | Controle: ./controle_fusee_control | Data: ./controle_fusee_data");
            refresh();
            napms(FRAME_MS);
            continue;
        }

        if (cmd_fd >= 0) {
            poll_pipe_lines(cmd_fd, apply_cmd, &st, pending_cmd, &used_cmd);
        }
        if (data_fd >= 0) {
            poll_pipe_lines(data_fd, apply_data, &st, pending_data, &used_data);
        }

        handle_local_input(&st, ch);
        update_dynamics(&st, FRAME_MS);
        draw_dashboard(&st, rows, cols, CMD_PIPE, DATA_PIPE);

        refresh();
        napms(FRAME_MS);
    }

    endwin();
    if (cmd_fd  >= 0) close(cmd_fd);
    if (data_fd >= 0) close(data_fd);
    if (keep_cmd  >= 0) close(keep_cmd);
    if (keep_data >= 0) close(keep_data);
    if (st.auth_pipe_fd >= 0) close(st.auth_pipe_fd);
    return 0;
}
