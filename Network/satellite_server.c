/************************************************************
 * Projet      : Fusée
 * Fichier     : satellite_server.c
 * Description : Serveur TCP satellite (VM 192.168.64.7, port 5555).
 *               Gère deux types de clients :
 *                 - CONTROLLER (JoyPi) : commandes mission + push télémétrie
 *                 - INJECTOR  (controle_fusee_data) : injection télémétrie
 *               Handlers métier dans satellite_handler.c.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "satellite_handler.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */

#define CMD_PIPE              "/tmp/rocket_cmd.pipe"
#define DATA_PIPE             "/tmp/rocket_data.pipe"
#define MAX_CLIENTS           MAX_CLIENTS_H
#define TELEMETRY_INTERVAL_MS 2000ULL
#define SELECT_TIMEOUT_MS     100

/* ------------------------------------------------------------------ */
/* Globaux (exportés via satellite_handler.h)                         */
/* ------------------------------------------------------------------ */

SatTelemetry g_telem;
bool         g_altitude_received = false;
int          g_cmd_pipe_fd       = -1;
int          g_data_pipe_fd      = -1;

static volatile sig_atomic_t g_stop      = 0;
static int                   g_server_fd = -1;
static unsigned long long    g_last_push_ms = 0;

/* ------------------------------------------------------------------ */
/* Utilitaires temps                                                   */
/* ------------------------------------------------------------------ */

static unsigned long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long long)tv.tv_sec * 1000ULL)
         + (unsigned long long)(tv.tv_usec / 1000ULL);
}

/* ------------------------------------------------------------------ */
/* Logging (déclaré extern dans satellite_handler.h)                  */
/* ------------------------------------------------------------------ */

void log_line(const char *level, const char *fmt, ...) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(stdout, "[%s] [%s] ", ts, level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fputc('\n', stdout);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Nom d'état                                                          */
/* ------------------------------------------------------------------ */

const char *sat_state_name(SatRocketState s) {
    switch (s) {
        case SAT_STATE_READY:     return "READY";
        case SAT_STATE_FLYING:    return "FLYING";
        case SAT_STATE_LANDING:   return "LANDING";
        case SAT_STATE_EMERGENCY: return "EMERGENCY";
        case SAT_STATE_EXPLODED:  return "EXPLODED";
        default:                  return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/* Signal                                                              */
/* ------------------------------------------------------------------ */

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

/* ------------------------------------------------------------------ */
/* Socket serveur                                                      */
/* ------------------------------------------------------------------ */

static int create_server_socket(int port) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return -1;

    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(sfd); return -1; }
    if (listen(sfd, 8) < 0) { close(sfd); return -1; }
    return sfd;
}

/* ------------------------------------------------------------------ */
/* Pipes locaux (déclaré extern dans satellite_handler.h)             */
/* ------------------------------------------------------------------ */

static int ensure_fifo_writer(const char *path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) return -1;
    return open(path, O_WRONLY | O_NONBLOCK);
}

void write_to_pipe(int *fd_ptr, const char *path, const char *msg) {
    if (*fd_ptr < 0) {
        *fd_ptr = ensure_fifo_writer(path);
    }
    if (*fd_ptr < 0) return;
    char buf[192];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    ssize_t nw = write(*fd_ptr, buf, strlen(buf));
    if (nw < 0 && (errno == EPIPE || errno == ENXIO)) {
        close(*fd_ptr);
        *fd_ptr = -1;
    }
    (void)nw;
}

/* ------------------------------------------------------------------ */
/* Envoi à un client (déclaré extern dans satellite_handler.h)        */
/* ------------------------------------------------------------------ */

void send_to_client(int fd, const char *msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    ssize_t nw = write(fd, buf, strlen(buf));
    (void)nw;
}

/* ------------------------------------------------------------------ */
/* Lecture et traitement d'un client                                  */
/* ------------------------------------------------------------------ */

static void process_client_input(SatClientH clients[], int idx) {
    SatClientH *c = &clients[idx];

    char buf[256];
    ssize_t nr = read(c->fd, buf, sizeof(buf));
    if (nr <= 0) {
        c->used = (size_t)-1;
        return;
    }

    if (c->used + (size_t)nr >= sizeof(c->pending) - 1) {
        c->used = 0;
        memset(c->pending, 0, sizeof(c->pending));
    }
    memcpy(c->pending + c->used, buf, (size_t)nr);
    c->used += (size_t)nr;
    c->pending[c->used] = '\0';

    char *start = c->pending;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        size_t len = strlen(start);
        if (len > 0 && start[len - 1] == '\r') start[len - 1] = '\0';
        if (start[0] != '\0') dispatch_line(clients, idx, start);
        start = nl + 1;
    }

    size_t remain = strlen(start);
    memmove(c->pending, start, remain + 1);
    c->used = remain;
}

static void remove_client(SatClientH clients[], int idx) {
    if (clients[idx].fd >= 0) close(clients[idx].fd);
    log_line("INFO", "client deconnecte slot=%d peer=%s type=%s",
             idx, clients[idx].peer,
             clients[idx].type == SAT_CLIENT_CONTROLLER ? "CONTROLLER" :
             clients[idx].type == SAT_CLIENT_INJECTOR   ? "INJECTOR"   : "UNKNOWN");
    clients[idx].fd       = -1;
    clients[idx].type     = SAT_CLIENT_UNKNOWN;
    clients[idx].used     = 0;
    clients[idx].authed   = false;
    clients[idx].pending[0] = '\0';
    clients[idx].peer[0]    = '\0';
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    int port = SAT_PORT;
    if (argc > 1) {
        int p = atoi(argv[1]);
        if (p > 0) port = p;
    }

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    memset(&g_telem, 0, sizeof(g_telem));
    g_telem.state    = SAT_STATE_READY;
    g_telem.fuel     = 100;
    g_telem.temp     = 20;
    g_telem.pressure = 1013;

    g_cmd_pipe_fd  = ensure_fifo_writer(CMD_PIPE);
    g_data_pipe_fd = ensure_fifo_writer(DATA_PIPE);

    int sfd = create_server_socket(port);
    if (sfd < 0) { perror("satellite_server: bind"); return 1; }
    g_server_fd = sfd;

    SatClientH clients[MAX_CLIENTS];
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd       = -1;
        clients[i].type     = SAT_CLIENT_UNKNOWN;
        clients[i].used     = 0;
        clients[i].authed   = false;
        clients[i].pending[0] = '\0';
        clients[i].peer[0]    = '\0';
    }

    log_line("INFO", "satellite_server demarre sur 0.0.0.0:%d", port);
    log_line("INFO", "Attente CONTROLLER (JoyPi) et INJECTOR (controle_fusee_data)...");

    g_last_push_ms = now_ms();

    while (!g_stop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sfd, &rfds);
        int maxfd = sfd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd) maxfd = clients[i].fd;
            }
        }

        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = SELECT_TIMEOUT_MS * 1000;

        int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rc < 0) {
            if (errno == EINTR && !g_stop) continue;
            break;
        }

        /* Accepter nouveaux clients */
        if (rc > 0 && FD_ISSET(sfd, &rfds)) {
            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            int cfd = accept(sfd, (struct sockaddr *)&caddr, &clen);
            if (cfd >= 0) {
                char ip[INET_ADDRSTRLEN];
                const char *peer_ip = inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
                int peer_port = ntohs(caddr.sin_port);
                if (!peer_ip) peer_ip = "unknown";

                bool inserted = false;
                int j;
                for (j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd < 0) {
                        clients[j].fd     = cfd;
                        clients[j].type   = SAT_CLIENT_UNKNOWN;
                        clients[j].used   = 0;
                        clients[j].authed = false;
                        memset(clients[j].pending, 0, sizeof(clients[j].pending));
                        snprintf(clients[j].peer, sizeof(clients[j].peer),
                                 "%s:%d", peer_ip, peer_port);
                        log_line("INFO", "nouveau client slot=%d peer=%s", j, clients[j].peer);
                        inserted = true;
                        break;
                    }
                }
                if (!inserted) {
                    send_to_client(cfd, "ERR SERVER_BUSY");
                    close(cfd);
                }
            }
        }

        /* Traiter les clients existants */
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && rc > 0 && FD_ISSET(clients[i].fd, &rfds)) {
                process_client_input(clients, i);
                if (clients[i].used == (size_t)-1) remove_client(clients, i);
            }
        }

        check_state_transitions(clients);

        unsigned long long tnow = now_ms();
        if (tnow - g_last_push_ms >= TELEMETRY_INTERVAL_MS) {
            push_telemetry(clients);
            g_last_push_ms = tnow;
        }

        if (g_cmd_pipe_fd < 0)  g_cmd_pipe_fd  = open(CMD_PIPE,  O_WRONLY | O_NONBLOCK);
        if (g_data_pipe_fd < 0) g_data_pipe_fd = open(DATA_PIPE, O_WRONLY | O_NONBLOCK);
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) close(clients[i].fd);
    }
    if (sfd >= 0)             close(sfd);
    if (g_cmd_pipe_fd >= 0)   close(g_cmd_pipe_fd);
    if (g_data_pipe_fd >= 0)  close(g_data_pipe_fd);

    log_line("INFO", "satellite_server arrete");
    return 0;
}
