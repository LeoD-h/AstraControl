/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_actions.h
 * Description : API des actions boutons et saisie mot de passe.
 *               Défini dans joypi_ctrl_actions.c.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef JOYPI_CTRL_ACTIONS_H
#define JOYPI_CTRL_ACTIONS_H

#include "joypi_controller.h"

/* Dispatch une touche (mode PASSWORD ou NORMAL selon st->mode). */
void handle_key(ControllerState *st, int key_num);

#endif /* JOYPI_CTRL_ACTIONS_H */
