/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_net.h
 * Description : API réseau du contrôleur JoyPi :
 *               pipes locaux, connexion satellite, push télémétrie.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#ifndef JOYPI_CTRL_NET_H
#define JOYPI_CTRL_NET_H

#include "joypi_controller.h"
#include <stddef.h>

/* ---- Pipes locaux ---- */
void cmd_pipe_write(ControllerState *st, const char *msg);
void data_pipe_write(ControllerState *st, const char *msg);

/* ---- Connexion satellite ---- */
void try_reconnect(ControllerState *st);

/* ---- Commande + réponse (bloquant 2s max) ---- */
bool send_cmd_recv(ControllerState *st,
                   const char *cmd,
                   char *resp,
                   size_t resp_sz);

/* ---- Réception non-bloquante messages push ---- */
void poll_satellite_push(ControllerState *st);

/* ---- Auth pipe : décollage depuis clavier controle_fusee ---- */
void poll_auth_pipe(ControllerState *st);

/* ---- Initialisation état ---- */
void state_init(ControllerState *st, const char *ip, int port);

#endif /* JOYPI_CTRL_NET_H */
