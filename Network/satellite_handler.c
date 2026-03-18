/************************************************************
 * Projet      : Fusée
 * Fichier     : satellite_handler.c
 * Description : Handlers CONTROLLER et INJECTOR du satellite_server.
 *               Broadcast, télémétrie, transitions d'état.
 *               Séparé de satellite_server.c pour respecter la limite 400 lignes.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "satellite_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_PIPE  "/tmp/rocket_cmd.pipe"
#define DATA_PIPE "/tmp/rocket_data.pipe"

/* ------------------------------------------------------------------ */
/* Broadcast                                                           */
/* ------------------------------------------------------------------ */

void broadcast_controllers(SatClientH clients[], const char *msg) {
    int i;
    for (i = 0; i < MAX_CLIENTS_H; i++) {
        if (clients[i].fd >= 0
                && clients[i].type  == SAT_CLIENT_CONTROLLER
                && clients[i].authed) {
            send_to_client(clients[i].fd, msg);
        }
    }
}

void broadcast_injectors(SatClientH clients[], const char *msg) {
    int i;
    for (i = 0; i < MAX_CLIENTS_H; i++) {
        if (clients[i].fd >= 0
                && clients[i].type  == SAT_CLIENT_INJECTOR
                && clients[i].authed) {
            send_to_client(clients[i].fd, msg);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Push télémétrie périodique                                          */
/* ------------------------------------------------------------------ */

void push_telemetry(SatClientH clients[]) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "TELEMETRY alt=%d speed=%d fuel=%d temp=%d pressure=%d thrust=%d state=%s",
             g_telem.altitude,
             g_telem.speed,
             g_telem.fuel,
             g_telem.temp,
             g_telem.pressure,
             g_telem.thrust,
             sat_state_name(g_telem.state));
    broadcast_controllers(clients, buf);
}

/* ------------------------------------------------------------------ */
/* Transitions d'état automatiques                                    */
/* ------------------------------------------------------------------ */

void check_state_transitions(SatClientH clients[]) {
    /* Carburant épuisé en vol -> atterrissage automatique */
    if (g_telem.state == SAT_STATE_FLYING && g_telem.fuel <= 0) {
        g_telem.state = SAT_STATE_LANDING;
        log_line("INFO", "carburant epuise -> LANDING automatique");
        broadcast_controllers(clients, "EVENT LAND_AUTO");
        broadcast_injectors(clients, "CMD_EVENT LAND");
        write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "LAND");
        return;
    }

    /* Altitude zéro en atterrissage ou urgence -> posée */
    if ((g_telem.state == SAT_STATE_LANDING
         || g_telem.state == SAT_STATE_EMERGENCY)
            && g_telem.altitude <= 0
            && g_altitude_received) {
        g_telem.state     = SAT_STATE_READY;
        g_telem.altitude  = 0;
        g_telem.speed     = 0;
        g_telem.thrust    = 0;
        g_altitude_received = false;
        log_line("INFO", "altitude=0 -> READY (posee)");
        broadcast_controllers(clients, "EVENT LANDED");
        broadcast_injectors(clients, "CMD_EVENT RESUME");
        write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "RESUME");
    }
}

/* ------------------------------------------------------------------ */
/* Handler commandes CONTROLLER                                        */
/* ------------------------------------------------------------------ */

static void handle_controller_cmd(SatClientH clients[], int idx, const char *line) {
    SatClientH *c = &clients[idx];

    if (strcmp(line, "AUTH CONTROLLER") == 0) {
        c->type   = SAT_CLIENT_CONTROLLER;
        c->authed = true;
        send_to_client(c->fd, "AUTH_OK");
        log_line("INFO", "CONTROLLER authentifie peer=%s", c->peer);
        push_telemetry(clients);
        return;
    }

    if (!c->authed) {
        send_to_client(c->fd, "AUTH_FAIL");
        return;
    }

    if (strcmp(line, "CMD LU") == 0) {
        if (g_telem.state != SAT_STATE_READY) {
            send_to_client(c->fd,
                g_telem.state == SAT_STATE_FLYING ? "FAIL ALREADY_LAUNCHED"
                                                   : "FAIL NOT_READY");
            return;
        }
        g_telem.state              = SAT_STATE_FLYING;
        g_telem.pressure_fault     = false;
        g_telem.pressure_corrector = false;
        send_to_client(c->fd, "OK LAUNCH");
        broadcast_controllers(clients, "EVENT LAUNCH");
        broadcast_injectors(clients, "CMD_EVENT LAUNCH");
        write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "LAUNCH");
        write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "SIM_FLIGHT ON");
        log_line("INFO", "LAUNCH par peer=%s", c->peer);
        return;
    }

    if (strcmp(line, "CMD LD") == 0) {
        if (g_telem.state != SAT_STATE_FLYING
                && g_telem.state != SAT_STATE_LANDING) {
            send_to_client(c->fd, "FAIL NOT_FLYING");
            return;
        }
        g_telem.state = SAT_STATE_EMERGENCY;
        send_to_client(c->fd, "OK LAND");
        broadcast_controllers(clients, "EVENT LAND");
        broadcast_injectors(clients, "CMD_EVENT LAND");
        write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "LAND");
        log_line("INFO", "EMERGENCY LAND par peer=%s", c->peer);
        return;
    }

    if (strcmp(line, "CMD ALT") == 0) {
        char reply[64];
        snprintf(reply, sizeof(reply), "DATA ALT %d", g_telem.altitude);
        send_to_client(c->fd, reply);
        return;
    }

    if (strcmp(line, "CMD TEMP") == 0) {
        char reply[64];
        snprintf(reply, sizeof(reply), "DATA TEMP %d", g_telem.temp);
        send_to_client(c->fd, reply);
        return;
    }

    if (strcmp(line, "CMD PRES") == 0) {
        if (!g_telem.pressure_fault) {
            g_telem.pressure_fault     = true;
            g_telem.pressure_corrector = false;
            send_to_client(c->fd, "OK PRES_FAULT");
            broadcast_controllers(clients, "EVENT PROBLEM");
            write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", "PROBLEM ON");
            log_line("INFO", "PRESSION: panne peer=%s", c->peer);
        } else if (!g_telem.pressure_corrector) {
            g_telem.pressure_corrector = true;
            send_to_client(c->fd, "OK PRES_FIX");
            broadcast_controllers(clients, "EVENT RESOLVED");
            write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "FIX_PROBLEM");
            write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", "PROBLEM OFF");
            log_line("INFO", "PRESSION: correcteur peer=%s", c->peer);
        } else {
            g_telem.pressure_fault     = false;
            g_telem.pressure_corrector = false;
            send_to_client(c->fd, "OK PRES_OK");
            write_to_pipe(&g_cmd_pipe_fd, "/tmp/rocket_cmd.pipe", "CLEAR_ALERTS");
            log_line("INFO", "PRESSION: nominale peer=%s", c->peer);
        }
        return;
    }

    if (strncmp(line, "CMD MEL ", 8) == 0) {
        int n = atoi(line + 8);
        if (n < 1 || n > 3) {
            send_to_client(c->fd, "FAIL BAD_MELODY");
            return;
        }
        char reply[32];
        snprintf(reply, sizeof(reply), "OK MEL %d", n);
        send_to_client(c->fd, reply);
        char ev[32];
        snprintf(ev, sizeof(ev), "EVENT MEL %d", n);
        broadcast_controllers(clients, ev);
        log_line("INFO", "TEST MELODIE %d peer=%s", n, c->peer);
        return;
    }

    send_to_client(c->fd, "FAIL UNKNOWN_CMD");
    log_line("WARN", "commande inconnue CONTROLLER peer=%s raw='%s'", c->peer, line);
}

/* ------------------------------------------------------------------ */
/* Handler commandes INJECTOR                                          */
/* ------------------------------------------------------------------ */

static void handle_injector_cmd(SatClientH clients[], int idx, const char *line) {
    SatClientH *c = &clients[idx];

    if (strcmp(line, "AUTH INJECTOR") == 0) {
        c->type   = SAT_CLIENT_INJECTOR;
        c->authed = true;
        send_to_client(c->fd, "AUTH_OK");
        log_line("INFO", "INJECTOR authentifie peer=%s", c->peer);
        return;
    }

    if (!c->authed) {
        send_to_client(c->fd, "AUTH_FAIL");
        return;
    }

    if (strncmp(line, "SET ", 4) == 0) {
        char field[32] = {0};
        int  val       = 0;
        if (sscanf(line + 4, "%31s %d", field, &val) == 2) {
            if (strcmp(field, "ALTITUDE") == 0) {
                g_telem.altitude = val;
                g_altitude_received = true;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET ALTITUDE %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
            } else if (strcmp(field, "SPEED") == 0) {
                g_telem.speed = val;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET SPEED %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
            } else if (strcmp(field, "FUEL") == 0) {
                g_telem.fuel = val;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET FUEL %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
            } else if (strcmp(field, "TEMP") == 0) {
                g_telem.temp = val;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET TEMP %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
                if (val > SAT_TEMP_CRITICAL
                        && g_telem.state == SAT_STATE_FLYING
                        && !g_telem.pressure_fault) {
                    broadcast_controllers(clients, "EVENT PROBLEM");
                    log_line("WARN", "temperature critique %dC -> EVENT PROBLEM", val);
                }
            } else if (strcmp(field, "PRESSURE") == 0) {
                g_telem.pressure = val;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET PRESSURE %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
            } else if (strcmp(field, "THRUST") == 0) {
                g_telem.thrust = val;
                char buf[64];
                snprintf(buf, sizeof(buf), "SET THRUST %d", val);
                write_to_pipe(&g_data_pipe_fd, "/tmp/rocket_data.pipe", buf);
            } else if (strcmp(field, "STRESS") == 0) {
                g_telem.stress = val;
                if (val > SAT_STRESS_CRITICAL
                        && g_telem.state == SAT_STATE_FLYING
                        && !g_telem.pressure_fault) {
                    broadcast_controllers(clients, "EVENT PROBLEM");
                    log_line("WARN", "stress critique %d -> EVENT PROBLEM", val);
                }
            }
            send_to_client(c->fd, "OK");
        } else {
            send_to_client(c->fd, "FAIL BAD_FORMAT");
        }
        return;
    }

    send_to_client(c->fd, "FAIL UNKNOWN_CMD");
    log_line("WARN", "commande inconnue INJECTOR peer=%s raw='%s'", c->peer, line);
}

/* ------------------------------------------------------------------ */
/* Dispatch selon type client                                          */
/* ------------------------------------------------------------------ */

void dispatch_line(SatClientH clients[], int idx, const char *line) {
    SatClientH *c = &clients[idx];

    if (c->type == SAT_CLIENT_UNKNOWN) {
        if (strcmp(line, "AUTH CONTROLLER") == 0) {
            c->type = SAT_CLIENT_CONTROLLER;
            handle_controller_cmd(clients, idx, line);
        } else if (strcmp(line, "AUTH INJECTOR") == 0) {
            c->type = SAT_CLIENT_INJECTOR;
            handle_injector_cmd(clients, idx, line);
        } else {
            send_to_client(c->fd, "AUTH_FAIL");
            log_line("WARN", "client non identifie peer=%s raw='%s'", c->peer, line);
        }
        return;
    }

    if (c->type == SAT_CLIENT_CONTROLLER) {
        handle_controller_cmd(clients, idx, line);
    } else {
        handle_injector_cmd(clients, idx, line);
    }
}
