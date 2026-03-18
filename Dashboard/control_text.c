/************************************************************
 * Projet      : Fusée
 * Fichier     : control_text.c
 * Description : Controle mission en mode texte (version cross RPi sans ncurses).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "dashboard_common.h"

static void print_help(void) {
    printf("Commandes supportees:\n");
    printf(" left/right/straight | launch | land\n");
    printf(" melody1 melody2 melody3 | alert1 alert2 alert3 | clear\n");
    printf(" pause resume | sim on|off | fix | explode | speed_up speed_down | pressure_up pressure_down\n");
    printf(" quit\n\n");
}

static const char *map_cmd(const char *in) {
    if (!strcmp(in, "left")) return "TILT_LEFT";
    if (!strcmp(in, "right")) return "TILT_RIGHT";
    if (!strcmp(in, "straight")) return "STRAIGHT";
    if (!strcmp(in, "launch")) return "LAUNCH";
    if (!strcmp(in, "land")) return "LAND";
    if (!strcmp(in, "melody1")) return "TEST_MELODY_1";
    if (!strcmp(in, "melody2")) return "TEST_MELODY_2";
    if (!strcmp(in, "melody3")) return "TEST_MELODY_3";
    if (!strcmp(in, "alert1")) return "ALERT1";
    if (!strcmp(in, "alert2")) return "ALERT2";
    if (!strcmp(in, "alert3")) return "ALERT3";
    if (!strcmp(in, "clear")) return "CLEAR_ALERTS";
    if (!strcmp(in, "pause")) return "PAUSE";
    if (!strcmp(in, "resume")) return "RESUME";
    if (!strcmp(in, "sim on")) return "SIM_FLIGHT ON";
    if (!strcmp(in, "sim off")) return "SIM_FLIGHT OFF";
    if (!strcmp(in, "fix")) return "FIX_PROBLEM";
    if (!strcmp(in, "explode")) return "EXPLODE";
    if (!strcmp(in, "speed_up")) return "SPEED_UP";
    if (!strcmp(in, "speed_down")) return "SPEED_DOWN";
    if (!strcmp(in, "pressure_up")) return "PRESSURE_UP";
    if (!strcmp(in, "pressure_down")) return "PRESSURE_DOWN";
    if (!strcmp(in, "quit")) return "QUIT";
    return in;
}

int main(int argc, char **argv) {
    bool socket_mode = false;
    const char *server_ip = "127.0.0.1";
    int server_port = 5555;
    int fd = -1;
    char last_reply[128] = "NONE";

    if (argc > 1) {
        socket_mode = true;
        server_ip = argv[1];
        if (argc > 2) {
            server_port = atoi(argv[2]);
            if (server_port <= 0) server_port = 5555;
        }
        fd = connect_socket_fd(server_ip, server_port);
    }

    if (fd < 0) {
        fprintf(stderr, "controle_fusee_control(text): lance en mode socket: ./controle_fusee_control <ip> <port>\n");
        return 1;
    }

    printf("[controle_fusee_control text] connecte a %s:%d\n", server_ip, server_port);
    print_help();

    while (1) {
        char line[128];
        printf("cmd> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        trim_line(line);
        if (!line[0]) continue;

        const char *mapped = map_cmd(line);
        send_line_with_reply(fd, mapped, socket_mode, last_reply, sizeof(last_reply));
        printf("[server] %s\n", last_reply);

        if (!strcmp(mapped, "QUIT")) {
            break;
        }
    }

    close(fd);
    return 0;
}
