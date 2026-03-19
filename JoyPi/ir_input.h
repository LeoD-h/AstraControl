/************************************************************
 * Projet      : Fusée
 * Fichier     : ir_input.h
 * Description : API capteur IR JoyPi pour saisie mot de passe.
 *               ir_arm()  : configure pin 11 en INPUT (entre MODE_PASSWORD)
 *               ir_disarm(): reconfigure pin 11 en OUTPUT (sortie PASSWORD)
 *               ir_poll() : non-bloquant, retourne key_num ou 0
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 19/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef IR_INPUT_H
#define IR_INPUT_H

/* Configure le pin IR en INPUT (appeler en entrant en MODE_PASSWORD). */
void ir_arm(void);

/* Reconfigure le pin IR en OUTPUT pour LED rouge (sortie MODE_PASSWORD). */
void ir_disarm(void);

/* Lit un code IR de façon non-bloquante.
 * Retourne un key_num (1..10, KEY_CONFIRM=13, KEY_BACKSPACE=14) ou 0. */
int ir_poll(void);

#endif /* IR_INPUT_H */
