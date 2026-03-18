/************************************************************
 * Projet      : Fusée
 * Fichier     : joypi_hardware_button.h
 * Description : API gestion entrées hardware JoyPi (matrice + directions).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 18/03/2026
 ************************************************************/
#ifndef JOYPI_HARDWARE_BUTTON_H
#define JOYPI_HARDWARE_BUTTON_H

/* Codes virtuels pour les directions joystick (hors plage 1-16) */
#define HW_DIR_UP     17
#define HW_DIR_DOWN   18
#define HW_DIR_RIGHT  19
#define HW_DIR_LEFT   20

/* Callback appelé sur chaque front montant détecté.
 * key_num : numéro de la touche (1-16) ou HW_DIR_* */
typedef void (*hw_button_callback_fn)(int key_num);

/* Initialise le GPIO (à appeler une fois, avant hw_button_scan). */
void hw_button_init(void);

/* Scanne la matrice + joystick, appelle callback pour chaque
 * nouveau press. Inclut un délai anti-rebond (80ms). */
void hw_button_scan(hw_button_callback_fn callback);

#endif /* JOYPI_HARDWARE_BUTTON_H */
