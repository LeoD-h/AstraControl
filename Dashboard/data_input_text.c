/************************************************************
 * Projet      : Fusée
 * Fichier     : data_input_text.c
 * Description : Injecteur télémétrie – connexion INJECTOR vers satellite_server.
 *               Génère automatiquement une physique de fusée réaliste et
 *               répond aux CMD_EVENT reçus. Interface texte pure (printf).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#define _GNU_SOURCE
#include "dashboard_common.h"

#include <sys/select.h>

/* ------------------------------------------------------------------ */
/*  Modèle physique                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    int  gen_enabled;
    bool launched;
    bool landing;
    bool exploded;
    bool paused;
    double altitude_m;
    double speed_kmh;
    double fuel_pct;
    double temp_c;
    double pressure_hpa;
    double thrust_kn;
    double stress;
    unsigned long long last_tick_ms;
    unsigned long long last_emit_ms;
} GenModel;

static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void gen_init(GenModel *gm) {
    memset(gm, 0, sizeof(*gm));
    gm->gen_enabled   = 1;
    gm->fuel_pct      = 100.0;
    gm->temp_c        = 20.0;
    gm->pressure_hpa  = 1013.0;
    gm->last_tick_ms  = now_ms();
    gm->last_emit_ms  = gm->last_tick_ms;
}

static void gen_reset_to_ground(GenModel *gm) {
    gm->launched     = false;
    gm->landing      = false;
    gm->exploded     = false;
    gm->paused       = false;
    gm->altitude_m   = 0.0;
    gm->speed_kmh    = 0.0;
    gm->fuel_pct     = 100.0;
    gm->temp_c       = 20.0;
    gm->pressure_hpa = 1013.0;
    gm->thrust_kn    = 0.0;
    gm->stress       = 0.0;
}

static void gen_on_event(GenModel *gm, const char *event) {
    if (!event || !event[0]) return;

    if (!strcmp(event, "LAUNCH")) {
        gm->launched = true;
        gm->landing  = false;
        gm->exploded = false;
        gm->paused   = false;
        if (gm->fuel_pct < 5.0) gm->fuel_pct = 100.0;
    } else if (!strcmp(event, "LAND")) {
        if (gm->launched) gm->landing = true;
    } else if (!strcmp(event, "PAUSE")) {
        gm->paused = true;
    } else if (!strcmp(event, "RESUME")) {
        gm->paused = false;
    } else if (!strcmp(event, "TILT_LEFT") || !strcmp(event, "TILT_RIGHT")) {
        if (gm->launched && gm->speed_kmh > 700.0) gm->stress += 1.2;
    } else if (!strcmp(event, "FIX_PROBLEM")) {
        gen_reset_to_ground(gm);
    }
}

static void gen_step(GenModel *gm, double dt_s) {
    if (!gm->gen_enabled || gm->paused) return;

    if (gm->exploded) {
        gm->thrust_kn   = 0.0;
        gm->speed_kmh  *= 0.92;
        gm->altitude_m -= 220.0 * dt_s;
        gm->temp_c     += 80.0 * dt_s;
        gm->stress     += 5.0 * dt_s;
        if (gm->altitude_m < 0.0) gm->altitude_m = 0.0;
        if (gm->temp_c > 900.0)   gm->temp_c = 900.0;

    } else if (gm->launched && !gm->landing) {
        gm->thrust_kn = 1600.0 - (gm->altitude_m / 120.0);
        gm->thrust_kn = clampd(gm->thrust_kn, 800.0, 1700.0);

        gm->speed_kmh += (55.0 - (gm->speed_kmh / 600.0) - (gm->altitude_m / 8000.0)) * dt_s;
        gm->speed_kmh  = clampd(gm->speed_kmh, 0.0, 30000.0);
        gm->altitude_m += (gm->speed_kmh / 3.6) * dt_s;

        gm->fuel_pct -= (0.35 + gm->thrust_kn / 6000.0) * dt_s;
        if (gm->fuel_pct <= 0.0) {
            gm->fuel_pct = 0.0;
            gm->landing  = true;
        }

    } else if (gm->launched && gm->landing) {
        gm->thrust_kn  = 700.0;
        gm->speed_kmh -= 80.0 * dt_s;
        if (gm->speed_kmh < 120.0) gm->speed_kmh = 120.0;

        gm->altitude_m -= (gm->speed_kmh / 3.6) * dt_s;
        gm->fuel_pct   -= 0.15 * dt_s;
        gm->fuel_pct    = clampd(gm->fuel_pct, 0.0, 100.0);

        if (gm->altitude_m <= 0.0) {
            gm->altitude_m = 0.0;
            if (gm->speed_kmh > 260.0) {
                gm->exploded = true;
            } else {
                gm->launched  = false;
                gm->landing   = false;
                gm->speed_kmh = 0.0;
                gm->thrust_kn = 0.0;
            }
        }
    } else {
        gm->thrust_kn = 0.0;
        if (gm->speed_kmh > 0.0) gm->speed_kmh *= 0.95;
        if (gm->speed_kmh < 1.0) gm->speed_kmh  = 0.0;
        if (gm->altitude_m > 0.0) {
            gm->altitude_m -= 30.0 * dt_s;
            if (gm->altitude_m < 0.0) gm->altitude_m = 0.0;
        }
        if (gm->stress > 0.0) gm->stress -= 0.4 * dt_s;
        if (gm->stress < 0.0) gm->stress  = 0.0;
    }

    if (!gm->exploded) {
        double target_temp = 15.0 + (gm->speed_kmh / 180.0) + (gm->thrust_kn / 85.0) - (gm->altitude_m / 1200.0);
        target_temp = clampd(target_temp, -50.0, 450.0);
        gm->temp_c += (target_temp - gm->temp_c) * 0.25;
    }

    gm->pressure_hpa = 1013.0 - (gm->altitude_m * 0.06);
    gm->pressure_hpa = clampd(gm->pressure_hpa, 30.0, 1013.0);

    if (gm->temp_c > 380.0) gm->stress += (gm->temp_c - 380.0) * 0.02 * dt_s;
    if (gm->fuel_pct < 4.0 && gm->thrust_kn > 1000.0) gm->stress += 8.0 * dt_s;
    if (gm->stress > 100.0) gm->exploded = true;
}

/* ------------------------------------------------------------------ */
/*  Connexion satellite_server                                          */
/* ------------------------------------------------------------------ */

static int connect_to_satellite(const char *ip, int port) {
    struct sockaddr_in addr;
    char buf[64];
    ssize_t nr;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    /* Authentification INJECTOR */
    const char *auth = "AUTH INJECTOR\n";
    ssize_t nw = write(fd, auth, strlen(auth));
    (void)nw;

    nr = read(fd, buf, sizeof(buf) - 1);
    if (nr <= 0) {
        close(fd);
        return -1;
    }
    buf[nr] = '\0';
    if (strncmp(buf, "AUTH_OK", 7) != 0) {
        fprintf(stderr, "[INJECTOR] auth refusee: %s\n", buf);
        close(fd);
        return -1;
    }

    printf("[INJECTOR] connecte a %s:%d – AUTH_OK\n", ip, port);
    fflush(stdout);

    /* Rendre le fd non-bloquant pour la réception des événements */
    set_nonblocking(fd);
    return fd;
}

static void reconnect_if_needed(int *fd_ptr, const char *ip, int port) {
    if (*fd_ptr >= 0) return;

    /* Attente non-bloquante : throttle a 3s entre tentatives */
    static unsigned long long last_attempt_ms = 0;
    unsigned long long tnow = now_ms();
    if (tnow - last_attempt_ms < 3000ULL) return;
    last_attempt_ms = tnow;

    printf("[INJECTOR] tentative de reconnexion vers %s:%d...\n", ip, port);
    fflush(stdout);
    *fd_ptr = connect_to_satellite(ip, port);
    if (*fd_ptr < 0) {
        printf("[INJECTOR] reconnexion echouee, retry dans 3s\n");
        fflush(stdout);
    }
}

/* ------------------------------------------------------------------ */
/*  Envoi télémétrie                                                    */
/* ------------------------------------------------------------------ */

/*
 * Envoie un SET et lit le OK correspondant.
 * Le fd est non-bloquant pour la réception, on utilise select avec
 * un timeout court pour lire la réponse.
 * Retourne 0 si OK, -1 si erreur.
 */
static int send_set(int fd, const char *line) {
    char msg[128];
    int len = snprintf(msg, sizeof(msg), "%s\n", line);
    ssize_t nw = write(fd, msg, (size_t)len);
    (void)nw;

    /* Attente réponse (non-bloquant : select 200ms) */
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec  = 0;
    tv.tv_usec = 200000;
    int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) return -1;

    char reply[64];
    ssize_t nr = read(fd, reply, sizeof(reply) - 1);
    if (nr <= 0) return -1;
    reply[nr] = '\0';
    /* On accepte toute réponse contenant "OK" */
    return strstr(reply, "OK") ? 0 : -1;
}

static int send_telemetry(int fd, GenModel *gm) {
    char line[96];
    int err = 0;

    snprintf(line, sizeof(line), "SET ALTITUDE %d",  (int)gm->altitude_m);   err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET SPEED %d",     (int)gm->speed_kmh);    err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET FUEL %d",      (int)gm->fuel_pct);     err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET TEMP %d",      (int)gm->temp_c);       err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET PRESSURE %d",  (int)gm->pressure_hpa); err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET THRUST %d",    (int)gm->thrust_kn);    err |= (send_set(fd, line) < 0);
    snprintf(line, sizeof(line), "SET STRESS %d",    (int)gm->stress);       err |= (send_set(fd, line) < 0);

    if (err) {
        close(fd);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Réception événements serveur (non-bloquant)                         */
/* ------------------------------------------------------------------ */

/*
 * Lit les données disponibles sur fd, accumule dans pending[used],
 * parse les lignes complètes et appelle gen_on_event pour chaque
 * CMD_EVENT reçu.
 * Retourne -1 si la connexion est coupée, 0 sinon.
 */
static int poll_server_events(int fd, char *pending, size_t *used, GenModel *gm) {
    char buf[512];

    while (1) {
        ssize_t nr = read(fd, buf, sizeof(buf) - 1);
        if (nr < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; /* rien à lire */
            return -1;                                           /* erreur réelle */
        }
        if (nr == 0) return -1;                                  /* connexion fermée */

        /* Vérifier débordement buffer */
        if (*used + (size_t)nr >= 1024 - 1) {
            *used = 0;
            pending[0] = '\0';
        }
        memcpy(pending + *used, buf, (size_t)nr);
        *used          += (size_t)nr;
        pending[*used]  = '\0';
    }

    /* Parser les lignes complètes */
    char *start = pending;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        /* Traiter la ligne */
        if (strncmp(start, "CMD_EVENT ", 10) == 0) {
            const char *ev = start + 10;
            /* Supprimer éventuel \r */
            char ev_clean[64];
            snprintf(ev_clean, sizeof(ev_clean), "%s", ev);
            char *cr = strchr(ev_clean, '\r');
            if (cr) *cr = '\0';
            printf("[EVENT] CMD_EVENT %s\n", ev_clean);
            fflush(stdout);
            gen_on_event(gm, ev_clean);
        }
        start = nl + 1;
    }

    /* Conserver le fragment partiel */
    size_t remain = strlen(start);
    memmove(pending, start, remain + 1);
    *used = remain;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Point d'entrée                                                      */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *ip  = "127.0.0.1";
    int         port = 5555;

    if (argc > 1) ip = argv[1];
    if (argc > 2) {
        int p = atoi(argv[2]);
        if (p > 0) port = p;
    }

    int fd = connect_to_satellite(ip, port);
    if (fd < 0) {
        fprintf(stderr, "[INJECTOR] impossible de se connecter a %s:%d\n", ip, port);
        return 1;
    }

    GenModel gm;
    gen_init(&gm);

    char   pending[1024] = {0};
    size_t used = 0;

    unsigned long long last_stats_ms = now_ms();

    printf("[INJECTOR] demarrage boucle principale\n");
    printf("  Tapez 'help' pour voir les commandes disponibles.\n\n");
    fflush(stdout);

    while (1) {
        unsigned long long tnow = now_ms();

        /* --- Pas de temps physique --- */
        {
            double dt_s = (double)(tnow - gm.last_tick_ms) / 1000.0;
            if (dt_s < 0.0)  dt_s = 0.0;
            if (dt_s > 1.5)  dt_s = 1.5;
            gm.last_tick_ms = tnow;
            gen_step(&gm, dt_s);
        }

        /* --- Envoi télémétrie toutes les 700ms --- */
        if (gm.gen_enabled && (tnow - gm.last_emit_ms) >= 700ULL) {
            if (fd >= 0) {
                if (send_telemetry(fd, &gm) < 0) {
                    printf("[INJECTOR] erreur envoi – connexion perdue\n");
                    fflush(stdout);
                    fd = -1;
                }
            }
            gm.last_emit_ms = tnow;
        }

        /* --- Reconnexion si nécessaire --- */
        if (fd < 0) {
            reconnect_if_needed(&fd, ip, port);
            /* Réinitialiser le buffer après reconnexion */
            if (fd >= 0) {
                used = 0;
                pending[0] = '\0';
            }
        }

        /* --- Affichage stats toutes les 5s --- */
        if ((tnow - last_stats_ms) >= 5000ULL) {
            const char *state;
            if (gm.exploded)       state = "EXPLODED";
            else if (gm.paused)    state = "PAUSED";
            else if (gm.landing)   state = "LANDING";
            else if (gm.launched)  state = "FLIGHT";
            else                   state = "GROUND";

            printf("[GEN] ALT=%dm SPD=%dkm/h FUEL=%d%% TEMP=%dC PRESS=%dhPa THR=%dkN STRESS=%d state=%s\n",
                   (int)gm.altitude_m,
                   (int)gm.speed_kmh,
                   (int)gm.fuel_pct,
                   (int)gm.temp_c,
                   (int)gm.pressure_hpa,
                   (int)gm.thrust_kn,
                   (int)gm.stress,
                   state);
            fflush(stdout);
            last_stats_ms = tnow;
        }

        /* --- select : attendre stdin ou fd serveur (timeout 100ms) --- */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int maxfd = STDIN_FILENO;
        if (fd >= 0) {
            FD_SET(fd, &rfds);
            if (fd > maxfd) maxfd = fd;
        }
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 100000; /* 100ms */
        int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rc < 0 && errno != EINTR) break;
        if (rc <= 0) continue;

        /* Traitement stdin */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[160];
            if (!fgets(line, sizeof(line), stdin)) break;
            trim_line(line);
            if (line[0] == '\0') continue;

            if (!strcmp(line, "quit") || !strcmp(line, "QUIT")) break;

            if (!strncmp(line, "GEN ", 4)) {
                gm.gen_enabled = atoi(line + 4) ? 1 : 0;
                printf("[GEN] mode=%d\n", gm.gen_enabled);
                fflush(stdout);
                continue;
            }

            if (!strcmp(line, "LAUNCH")) {
                gen_on_event(&gm, "LAUNCH");
                printf("[STDIN] LAUNCH simulé\n");
                fflush(stdout);
                continue;
            }

            /* Commande directe SET XXX v envoyée au serveur */
            if (!strncmp(line, "SET ", 4) && fd >= 0) {
                if (send_set(fd, line) < 0) {
                    printf("[INJECTOR] erreur envoi SET – connexion perdue\n");
                    fflush(stdout);
                    close(fd);
                    fd = -1;
                } else {
                    printf("[STDIN] envoyé: %s\n", line);
                    fflush(stdout);
                }
                continue;
            }

            printf("[STDIN] commande inconnue: %s\n", line);
            fflush(stdout);
        }

        /* Traitement données serveur disponibles (via select) */
        if (fd >= 0 && FD_ISSET(fd, &rfds)) {
            if (poll_server_events(fd, pending, &used, &gm) < 0) {
                printf("[INJECTOR] connexion coupee par le serveur\n");
                fflush(stdout);
                close(fd);
                fd = -1;
            }
        }
    }

    if (fd >= 0) close(fd);
    printf("[INJECTOR] arret.\n");
    return 0;
}
