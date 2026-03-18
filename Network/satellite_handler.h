/************************************************************
 * Projet      : Fusée
 * Fichier     : satellite_handler.h
 * Description : Déclarations des handlers CONTROLLER/INJECTOR,
 *               broadcast, télémétrie et transitions d'état.
 *               Séparé de satellite_server.c pour respecter la limite 400 lignes.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#ifndef SATELLITE_HANDLER_H
#define SATELLITE_HANDLER_H

#include "satellite_protocol.h"
#include <stdbool.h>
#include <stddef.h>

/* Taille maximale du tableau de clients (définie dans satellite_server.c) */
#define MAX_CLIENTS_H 16

/* Identique à SatClient dans satellite_server.c — déclarée ici pour
 * que satellite_handler.c puisse accéder au tableau de clients.       */
typedef struct {
    int            fd;
    SatClientType  type;
    char           pending[512];
    size_t         used;
    char           peer[64];
    bool           authed;
} SatClientH;

/* Globaux partagés (définis dans satellite_server.c) */
extern SatTelemetry g_telem;
extern bool         g_altitude_received;
extern int          g_cmd_pipe_fd;
extern int          g_data_pipe_fd;

/* Envoi d'un message à un client (défini dans satellite_server.c) */
void send_to_client(int fd, const char *msg);

/* Écriture dans un pipe local (défini dans satellite_server.c) */
void write_to_pipe(int *fd_ptr, const char *path, const char *msg);

/* Logging (défini dans satellite_server.c) */
void log_line(const char *level, const char *fmt, ...);

/* ------------------------------------------------------------------ */
/* Fonctions définies dans satellite_handler.c                        */
/* ------------------------------------------------------------------ */

void broadcast_controllers(SatClientH clients[], const char *msg);
void broadcast_injectors(SatClientH clients[], const char *msg);
void push_telemetry(SatClientH clients[]);
void check_state_transitions(SatClientH clients[]);
void dispatch_line(SatClientH clients[], int idx, const char *line);

#endif /* SATELLITE_HANDLER_H */
