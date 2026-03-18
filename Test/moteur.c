/************************************************************
 * Projet      : Fusée
 * Fichier     : Test/moteur.c
 * Description : Test jeu de lumières sur les 2 LEDs connectées aux
 *               servomoteurs de droite du Joy-Pi.
 *
 * Contrainte matérielle :
 *   - Les 2 servomoteurs de DROITE servent à contrôler 2 LEDs
 *   - LED verte (décollage/OK)  : physical pin 37 = GPIO 26 = servo droit 1
 *   - LED rouge (atterrissage)  : physical pin 11 = GPIO 17 = servo droit 2
 *   - Pin 22 et pin 33 NON DISPONIBLES sur ce JoyPi
 *   - Utiliser wiringPiSetupPhys() pour les numéros physiques
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 2.0
 ************************************************************/
#include <stdio.h>
#include <unistd.h>

#ifdef USE_WIRINGPI
#include <wiringPi.h>
#endif

/* Pins physiques (wiringPiSetupPhys) */
#define LED_GREEN_PIN  37  /* GPIO 26 — servo droit 1 — LED verte  */
#define LED_RED_PIN    11  /* GPIO 17 — servo droit 2 — LED rouge   */

int main(void) {
#ifdef USE_WIRINGPI
    if (wiringPiSetupPhys() == -1) {
        printf("Erreur d'initialisation de wiringPi (mode physique)\n");
        return 1;
    }

    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN,   OUTPUT);
    digitalWrite(LED_GREEN_PIN, 0);
    digitalWrite(LED_RED_PIN,   0);

    printf("Jeu de lumières démarré : LED verte=pin%d  LED rouge=pin%d\n",
           LED_GREEN_PIN, LED_RED_PIN);
    printf("Appuyez sur Ctrl+C pour quitter.\n");

    while (1) {
        printf("Motif 1 : Gyrophare (alternance verte/rouge)...\n");
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_GREEN_PIN, 1);
            digitalWrite(LED_RED_PIN,   0);
            delay(200);
            digitalWrite(LED_GREEN_PIN, 0);
            digitalWrite(LED_RED_PIN,   1);
            delay(200);
        }
        digitalWrite(LED_GREEN_PIN, 0);
        digitalWrite(LED_RED_PIN,   0);

        printf("Motif 2 : Flash rapide (les deux ensemble)...\n");
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_GREEN_PIN, 1);
            digitalWrite(LED_RED_PIN,   1);
            delay(80);
            digitalWrite(LED_GREEN_PIN, 0);
            digitalWrite(LED_RED_PIN,   0);
            delay(80);
        }

        printf("Pause...\n");
        delay(1000);
    }
#else
    printf("[TEST-MOTEUR] Simulation — LED verte pin%d  LED rouge pin%d\n",
           LED_GREEN_PIN, LED_RED_PIN);
    printf("[TEST-MOTEUR] Motif 1 : gyrophare\n");
    for (int i = 0; i < 5; i++) {
        printf("  VERT ON  / ROUGE OFF\n");
        usleep(200000);
        printf("  VERT OFF / ROUGE ON\n");
        usleep(200000);
    }
    printf("[TEST-MOTEUR] Motif 2 : flash rapide\n");
    for (int i = 0; i < 5; i++) {
        printf("  TOUS ON\n");
        usleep(80000);
        printf("  TOUS OFF\n");
        usleep(80000);
    }
    printf("[TEST-MOTEUR] Fin simulation.\n");
    return 0;
#endif
}
