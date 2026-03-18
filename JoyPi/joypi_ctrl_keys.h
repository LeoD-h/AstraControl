/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_ctrl_keys.h
 * Description : Gestion des touches, directions et mot de passe.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#ifndef JOYPI_CTRL_KEYS_H
#define JOYPI_CTRL_KEYS_H

#include "joypi_controller.h"

/* Scanne les boutons GPIO (ou stdin en simulation) et dispatche les actions. */
void scan_buttons_and_handle(ControllerState *st);

#endif /* JOYPI_CTRL_KEYS_H */
