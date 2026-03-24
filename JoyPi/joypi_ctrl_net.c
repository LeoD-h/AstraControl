/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_net.c
 * Description : Réseau du contrôleur JoyPi : pipes locaux, connexion
 *               satellite_server, push télémétrie, events.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "joypi_ctrl_net.h"
#include "actuators.h"
#include "ir_input.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Buffer persistant push satellite */
static char   g_push_buf[2048];
static size_t g_push_used = 0;

/* ------------------------------------------------------------------ */
/* Pipes locaux                                                        */
/* ------------------------------------------------------------------ */

static int open_pipe_writer(const char *path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0 && errno == ENXIO) return -1;
    if (fd < 0) perror(path);
    return fd;
}

void cmd_pipe_write(ControllerState *st, const char *msg) {
    if (st->cmd_pipe_fd < 0)
        st->cmd_pipe_fd = open_pipe_writer(CMD_PIPE);
    if (st->cmd_pipe_fd < 0) return;
    ssize_t nw = write(st->cmd_pipe_fd, msg, strlen(msg));
    if (nw < 0 && (errno == EPIPE || errno == ENXIO)) {
        close(st->cmd_pipe_fd);
        st->cmd_pipe_fd = -1;
    }
}

void data_pipe_write(ControllerState *st, const char *msg) {
    if (st->data_pipe_fd < 0)
        st->data_pipe_fd = open_pipe_writer(DATA_PIPE);
    if (st->data_pipe_fd < 0) return;
    ssize_t nw = write(st->data_pipe_fd, msg, strlen(msg));
    if (nw < 0 && (errno == EPIPE || errno == ENXIO)) {
        close(st->data_pipe_fd);
        st->data_pipe_fd = -1;
    }
}

/* ------------------------------------------------------------------ */
/* Connexion / reconnexion satellite                                   */
/* ------------------------------------------------------------------ */

static int connect_to_satellite(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    /* Timeout 2s sur connect() pour ne pas bloquer la boucle de scan */
    struct timeval tv = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "[ctrl] IP invalide : %s\n", ip);
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static bool auth_satellite(ControllerState *st) {
    if (st->sat_fd < 0) return false;

    const char *auth_msg = "AUTH CONTROLLER\n";
    if (write(st->sat_fd, auth_msg, strlen(auth_msg)) <= 0) return false;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(st->sat_fd, &rfds);
    struct timeval tv = {CMD_TIMEOUT_S, 0};
    if (select(st->sat_fd + 1, &rfds, NULL, NULL, &tv) <= 0) return false;

    char buf[64];
    ssize_t nr = read(st->sat_fd, buf, sizeof(buf) - 1);
    if (nr <= 0) return false;
    buf[nr] = '\0';

    if (strncmp(buf, "AUTH_OK", 7) == 0) {
        printf("[ctrl] Authentifié au satellite (%s:%d)\n", st->sat_ip, st->sat_port);
        return true;
    }
    fprintf(stderr, "[ctrl] AUTH refusé : %s\n", buf);
    return false;
}

void try_reconnect(ControllerState *st) {
    time_t now = time(NULL);
    if (now - st->last_reconnect_attempt < RECONNECT_DELAY_S) return;
    st->last_reconnect_attempt = now;

    if (st->sat_fd >= 0) {
        close(st->sat_fd);
        st->sat_fd = -1;
        st->authed = false;
    }

    printf("[ctrl] Tentative connexion satellite %s:%d...\n",
           st->sat_ip, st->sat_port);

    st->sat_fd = connect_to_satellite(st->sat_ip, st->sat_port);
    if (st->sat_fd < 0) {
        printf("[ctrl] Connexion échouée, retry dans %ds\n", RECONNECT_DELAY_S);
        return;
    }

    st->authed = auth_satellite(st);
    if (!st->authed) {
        close(st->sat_fd);
        st->sat_fd = -1;
        st->mode   = MODE_NORMAL;
    } else {
        if (st->cmd_pipe_fd  < 0) st->cmd_pipe_fd  = open_pipe_writer(CMD_PIPE);
        if (st->data_pipe_fd < 0) st->data_pipe_fd = open_pipe_writer(DATA_PIPE);
    }
}

/* ------------------------------------------------------------------ */
/* Envoi commande + réception réponse (bloquant 2s max)               */
/* ------------------------------------------------------------------ */

bool send_cmd_recv(ControllerState *st,
                   const char *cmd,
                   char *resp,
                   size_t resp_sz) {
    if (st->sat_fd < 0 || !st->authed) {
        fprintf(stderr, "[ctrl] Non connecté au satellite\n");
        return false;
    }

    if (write(st->sat_fd, cmd, strlen(cmd)) <= 0) {
        perror("[ctrl] write cmd");
        close(st->sat_fd);
        st->sat_fd = -1;
        st->authed = false;
        return false;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(st->sat_fd, &rfds);
    struct timeval tv = {CMD_TIMEOUT_S, 0};
    int rc = select(st->sat_fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        fprintf(stderr, "[ctrl] Timeout réponse (cmd=%s)\n", cmd);
        close(st->sat_fd);
        st->sat_fd = -1;
        st->authed = false;
        return false;
    }

    ssize_t nr = read(st->sat_fd, resp, (ssize_t)resp_sz - 1);
    if (nr <= 0) {
        close(st->sat_fd);
        st->sat_fd = -1;
        st->authed = false;
        return false;
    }
    resp[nr] = '\0';

    /* Sauvegarder les octets surplus dans g_push_buf */
    char *nl = strchr(resp, '\n');
    if (nl) {
        const char *leftover     = nl + 1;
        size_t      leftover_len = strlen(leftover);
        if (leftover_len > 0
                && g_push_used + leftover_len < sizeof(g_push_buf) - 1) {
            memmove(g_push_buf + leftover_len, g_push_buf, g_push_used + 1);
            memcpy(g_push_buf, leftover, leftover_len);
            g_push_used += leftover_len;
        }
        *nl = '\0';
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Traitement messages push satellite                                  */
/* ------------------------------------------------------------------ */

static void handle_telemetry(ControllerState *st, const char *line) {
    char alt[32]="", speed[32]="", fuel[32]="", temp[32]="";
    char pressure[32]="", thrust[32]="", state[32]="";
    int prev_altitude = st->last_altitude;
    int prev_speed = st->last_speed;

    const char *p = line;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;

        char key[32]="", val[32]="";
        int ki=0, vi=0;
        while (*p && *p != '=' && *p != ' ' && ki < 31) key[ki++] = *p++;
        key[ki] = '\0';
        if (*p == '=') {
            ++p;
            while (*p && *p != ' ' && vi < 31) val[vi++] = *p++;
            val[vi] = '\0';
        }

        if      (!strcmp(key,"alt"))      snprintf(alt,      sizeof(alt),      "%s", val);
        else if (!strcmp(key,"speed"))    snprintf(speed,    sizeof(speed),    "%s", val);
        else if (!strcmp(key,"fuel"))     snprintf(fuel,     sizeof(fuel),     "%s", val);
        else if (!strcmp(key,"temp"))     snprintf(temp,     sizeof(temp),     "%s", val);
        else if (!strcmp(key,"pressure")) snprintf(pressure, sizeof(pressure), "%s", val);
        else if (!strcmp(key,"thrust"))   snprintf(thrust,   sizeof(thrust),   "%s", val);
        else if (!strcmp(key,"state"))    snprintf(state,    sizeof(state),    "%s", val);
    }

    if (alt[0]) {
        st->last_altitude = atoi(alt);
    }
    if (speed[0]) {
        st->last_speed = atoi(speed);
    }
    /* Altitude temps réel sur le 7-segments (par défaut, en milliers de mètres). */
    if (!st->seg_override && alt[0]) {
        actuator_segment_show(st->last_altitude / 1000);
    }
    if (alt[0]) {
        char line1[17];
        char line2[17];
        snprintf(line1, sizeof(line1), "ALT %06dm", st->last_altitude);
        snprintf(line2, sizeof(line2), "MODE %-.10s", state[0] ? state : "READY");
        actuator_lcd_show(line1, line2);
    }

    /* Transfert capteurs vers dashboard.
     * Fuel : forwardé uniquement si augmentation significative (ex: SET FUEL 100
     * depuis controle_fusee_data) pour corriger un reset injecteur. */
    char msg[64];
    if (alt[0])      { snprintf(msg,sizeof(msg),"SET ALTITUDE %d\n", st->last_altitude); data_pipe_write(st,msg); }
    if (speed[0])    { snprintf(msg,sizeof(msg),"SET SPEED %d\n",    st->last_speed);    data_pipe_write(st,msg); }
    if (temp[0])     { snprintf(msg,sizeof(msg),"SET TEMP %s\n",      temp);     data_pipe_write(st,msg); }
    if (pressure[0]) { snprintf(msg,sizeof(msg),"SET PRESSURE %s\n",  pressure); data_pipe_write(st,msg); }
    if (thrust[0])   { snprintf(msg,sizeof(msg),"SET THRUST %s\n",    thrust);   data_pipe_write(st,msg); }
    if (fuel[0]) {
        int f = atoi(fuel);
        snprintf(msg, sizeof(msg), "SET FUEL %d\n", f);
        data_pipe_write(st, msg);
    }

    if (state[0] && strcmp(state, "READY") == 0 &&
        st->last_altitude == 0 && st->last_speed == 0 &&
        (prev_altitude > 0 || prev_speed > 0)) {
        cmd_pipe_write(st, "RESET_SIM\n");
    }
}

static void handle_event(ControllerState *st, const char *event) {
    if (strcmp(event, "LAUNCH") == 0) {
        printf("[ctrl] EVENT LAUNCH\n");
        st->explosion_notified = false;
        cmd_pipe_write(st, "LAUNCH_OK\n");
        /* Actuateurs exécutés ici (une seule fois) : */
        actuator_led_green_blink(4);
        actuator_led_set(2);
        actuator_matrix_launch();
        actuator_buzzer_bip();

    } else if (strcmp(event,"LAND")==0 || strcmp(event,"LAND_AUTO")==0) {
        printf("[ctrl] EVENT %s\n", event);
        cmd_pipe_write(st, "LAND\n");
        actuator_led_green_blink(4);

    } else if (strcmp(event, "LANDED") == 0) {
        printf("[ctrl] EVENT LANDED\n");
        st->explosion_notified = false;
        cmd_pipe_write(st, "SIM_FLIGHT OFF\n");
        actuator_led_all_off();
        actuator_matrix_clear();

    } else if (strcmp(event, "PROBLEM") == 0) {
        printf("[ctrl] EVENT PROBLEM\n");
        st->fault_active = true;
        data_pipe_write(st, "PROBLEM ON\n");
        actuator_led_set(1);
        actuator_buzzer_bip();

    } else if (strcmp(event, "PROBLEM1") == 0) {
        printf("[ctrl] EVENT PROBLEM1 (temperature critique)\n");
        st->fault1_active = true;
        cmd_pipe_write(st, "FAULT1\n");
        actuator_led_set(1);
        actuator_buzzer_melody_c();

    } else if (strcmp(event, "PROBLEM2") == 0) {
        printf("[ctrl] EVENT PROBLEM2 (stress structurel)\n");
        st->fault2_active = true;
        cmd_pipe_write(st, "FAULT2\n");
        actuator_led_set(1);
        actuator_buzzer_melody_c();

    } else if (strcmp(event, "RESOLVED") == 0) {
        printf("[ctrl] EVENT RESOLVED\n");
        st->fault_active = false;
        data_pipe_write(st, "PROBLEM OFF\n");
        actuator_led_set(2);
        actuator_buzzer_bip();

    } else if (strcmp(event, "RESOLVED1") == 0) {
        printf("[ctrl] EVENT RESOLVED1 (temperature normalisee)\n");
        st->fault1_active = false;
        cmd_pipe_write(st, "FAULT1_OFF\n");
        actuator_led_set(2);
        actuator_buzzer_bip();

    } else if (strcmp(event, "RESOLVED2") == 0) {
        printf("[ctrl] EVENT RESOLVED2 (stress reduit)\n");
        st->fault2_active = false;
        cmd_pipe_write(st, "FAULT2_OFF\n");
        actuator_led_set(2);
        actuator_buzzer_bip();

    } else if (strncmp(event, "MEL ", 4) == 0) {
        /* Mélodie jouée directement depuis handle_response_mel (CMD MEL N) — évite le double-play */
        printf("[ctrl] EVENT MEL %s (mélodie gérée côté réponse CMD)\n", event + 4);
    } else if (strcmp(event, "EXPLODED") == 0) {
        if (st->explosion_notified)
            return;
        st->explosion_notified = true;
        printf("[ctrl] EVENT EXPLODED\n");
        cmd_pipe_write(st, "EXPLODE\n");
        actuator_led_set(1);
        actuator_buzzer_bip();
    } else {
        printf("[ctrl] EVENT inconnu : %s\n", event);
    }
}

static void handle_push_line(ControllerState *st, const char *line) {
    if (strncmp(line, "TELEMETRY ", 10) == 0) {
        handle_telemetry(st, line + 10);
    } else if (strncmp(line, "EVENT ", 6) == 0) {
        handle_event(st, line + 6);
    }
}

void poll_satellite_push(ControllerState *st) {
    if (st->sat_fd < 0 || !st->authed) return;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(st->sat_fd, &rfds);
    struct timeval tv = {0, POLL_TIMEOUT_MS * 1000};
    if (select(st->sat_fd + 1, &rfds, NULL, NULL, &tv) <= 0) return;

    char buf[512];
    ssize_t nr = read(st->sat_fd, buf, sizeof(buf) - 1);
    if (nr <= 0) {
        printf("[ctrl] Connexion satellite perdue\n");
        close(st->sat_fd);
        st->sat_fd = -1;
        st->authed = false;
        return;
    }
    buf[nr] = '\0';

    if (g_push_used + (size_t)nr >= sizeof(g_push_buf) - 1) g_push_used = 0;
    memcpy(g_push_buf + g_push_used, buf, (size_t)nr);
    g_push_used += (size_t)nr;
    g_push_buf[g_push_used] = '\0';

    char *start = g_push_buf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        if (*start != '\0') handle_push_line(st, start);
        start = nl + 1;
    }
    size_t remain = strlen(start);
    memmove(g_push_buf, start, remain + 1);
    g_push_used = remain;
}

/* ------------------------------------------------------------------ */
/* Auth pipe : décollage depuis clavier controle_fusee                */
/* ------------------------------------------------------------------ */

void poll_auth_pipe(ControllerState *st) {
    /* Ouvrir le pipe en lecture non-bloquante si pas encore fait */
    if (st->auth_pipe_fd < 0) {
        mkfifo(AUTH_PIPE, 0666);
        st->auth_pipe_fd = open(AUTH_PIPE, O_RDONLY | O_NONBLOCK);
        if (st->auth_pipe_fd < 0) return;
    }
    char buf[64];
    ssize_t nr = read(st->auth_pipe_fd, buf, sizeof(buf) - 1);
    if (nr <= 0) return;
    buf[nr] = '\0';
    if (strncmp(buf, "LAUNCH_AUTH_OK", 14) == 0 && st->authed) {
        printf("[ctrl] Auth clavier reçue → envoi CMD LU au satellite\n");
        /* Sortir du mode PASSWORD dans tous les cas (clavier USB a pris la main) */
        st->mode = MODE_NORMAL;
#ifdef USE_WIRINGPI
        ir_disarm();
#endif
        char resp[256];
        if (send_cmd_recv(st, "CMD LU\n", resp, sizeof(resp))
                && strncmp(resp, "OK LAUNCH", 9) == 0) {
            /* Le satellite va broadcaster EVENT LAUNCH → handle_event
             * exécutera les actuateurs et enverra LAUNCH_OK au pipe. */
            printf("[ctrl] CMD LU accepté (clavier) — EVENT LAUNCH attendu\n");
        } else {
            printf("[ctrl] Auth clavier : satellite refus (%s)\n", resp);
            actuator_led_set(1);
            actuator_buzzer_bip();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Initialisation état                                                 */
/* ------------------------------------------------------------------ */

void state_init(ControllerState *st, const char *ip, int port) {
    memset(st, 0, sizeof(*st));
    st->sat_fd              = -1;
    st->cmd_pipe_fd         = -1;
    st->data_pipe_fd        = -1;
    st->auth_pipe_fd        = -1;
    st->authed              = false;
    st->mode                = MODE_NORMAL;
    st->melody_idx          = 0;
    st->password_len        = 0;
    st->password_buf[0]     = '\0';
    st->last_reconnect_attempt = 0;
    st->fault_active        = false;
    st->fault1_active       = false;
    st->fault2_active       = false;
    st->explosion_notified  = false;
    strncpy(st->sat_ip, ip, sizeof(st->sat_ip) - 1);
    st->sat_ip[sizeof(st->sat_ip) - 1] = '\0';
    st->sat_port = port;
}
