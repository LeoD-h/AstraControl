/************************************************************
 * Projet      : Fusée
 * Fichier     : actuators.h
 * Description : Interface actuateurs JoyPi (LED, buzzer, matrice, 7-seg, LCD).
 *
 * Contrainte matérielle :
 *   - Les 2 servomoteurs de DROITE contrôlent des LEDs :
 *       LED verte = servo droit 1 = physical pin 37 (GPIO 26)
 *       LED rouge = servo droit 2 = physical pin 11 (GPIO 17)
 *   - Pin 22 et 33 NON DISPONIBLES
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#ifndef ACTUATORS_H
#define ACTUATORS_H

/* Initialise tous les actuateurs (GPIO, SPI, I2C).
 * Doit être appelé une fois au démarrage, APRÈS wiringPiSetupPhys(). */
void actuator_init(void);

/* ---- LED simple (compatibilité) ----
 *   0 = éteint (les deux off)
 *   1 = alerte : LED rouge clignote 3x rapidement
 *   2 = ok     : LED verte allumée fixe */
void actuator_led_set(int state);

/* LED verte (servo droit 1, pin 37) : clignote count fois.
 * Utilisée au décollage pour signaler le lancement. */
void actuator_led_green_blink(int count);

/* LED rouge allumée fixe (servo droit 2, pin 11), LED verte éteinte.
 * Utilisée à l'atterrissage. */
void actuator_led_red_on(void);

/* Éteindre les deux LEDs. */
void actuator_led_all_off(void);

/* ---- Matrice LED 8x8 MAX7219 via SPI CE1 ---- */
void actuator_matrix_launch(void);
void actuator_matrix_emergency(void);
void actuator_matrix_clear(void);

/* ---- Buzzer pin 12 (GPIO 18, PWM) ---- */

/* Mélodie A : Décollage — montée festive */
void actuator_buzzer_melody_a(void);

/* Mélodie B : Urgence/Atterrissage — alarme rapide + descente */
void actuator_buzzer_melody_b(void);

/* Mélodie C : Panne — avertissement grave */
void actuator_buzzer_melody_c(void);

/* Bip court : confirmation / correcteur */
void actuator_buzzer_bip(void);

/* ---- Afficheurs ---- */

/* Afficheur 7 segments HT16K33 (I2C 0x70) : affiche valeur 0-9999. */
void actuator_segment_show(int value);

/* LCD 16x2 MCP23017 (I2C 0x21) : affiche deux lignes (max 16 chars). */
void actuator_lcd_show(const char *line1, const char *line2);

#endif /* ACTUATORS_H */
