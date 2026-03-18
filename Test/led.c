#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>


#define IR_PIN 20 


volatile unsigned long ir_code = 0;
volatile int bit_count = 0;
volatile unsigned int last_time = 0;
volatile int code_ready = 0;
volatile unsigned long final_code = 0;


void ir_interrupt(void) {
    unsigned int current_time = micros(); // Temps actuel en microsecondes
    unsigned int delta = current_time - last_time; // Temps écoulé depuis la dernière impulsion
    last_time = current_time;

    // Détection du signal de départ
    if (delta > 10000) {
        bit_count = 0;
        ir_code = 0;
        return;
    }

    // Décodage des bits (0 ou 1) en fonction du temps écoulé
    if (bit_count < 32) {
        if (delta > 2000) {
            // Un temps long (~2.25 ms) correspond à un '1' logique
            ir_code = (ir_code << 1) | 1;
            bit_count++;
        } else if (delta > 1000) {
            // Un temps court (~1.12 ms) correspond à un '0' logique
            ir_code = (ir_code << 1) | 0;
            bit_count++;
        }
    }

    // Le protocole NEC envoie 32 bits de données. Quand on a les 32, le code est prêt.
    if (bit_count == 32) {
        final_code = ir_code;
        code_ready = 1;
        bit_count = 0; // Réinitialisation pour la prochaine touche
    }
}

// Fonction pour traduire le code hexadécimal en texte
void decode_button(unsigned long code) {
    // On masque pour ne garder que les 24 derniers bits (comme ta librairie Python le fait)
    unsigned long masked_code = code & 0xFFFFFF;

    switch(masked_code) {
        case 0xffa25d: printf("KEY_CH-\n"); break;
        case 0xff629d: printf("KEY_CH\n"); break;
        case 0xffe21d: printf("KEY_CH+\n"); break;
        case 0xff22dd: printf("KEY_PREV\n"); break;
        case 0xff02fd: printf("KEY_NEXT\n"); break;
        case 0xffc23d: printf("KEY_PLAY/PAUSE\n"); break;
        case 0xffe01f: printf("KEY_VOL-\n"); break;
        case 0xffa857: printf("KEY_VOL+\n"); break;
        case 0xff906f: printf("KEY_EQ\n"); break;
        case 0xff6897: printf("KEY_0\n"); break;
        case 0xff9867: printf("KEY_100+\n"); break;
        case 0xffb04f: printf("KEY_200+\n"); break;
        case 0xff30cf: printf("KEY_1\n"); break;
        case 0xff18e7: printf("KEY_2\n"); break;
        case 0xff7a85: printf("KEY_3\n"); break;
        case 0xff10ef: printf("KEY_4\n"); break;
        case 0xff38c7: printf("KEY_5\n"); break;
        case 0xff5aa5: printf("KEY_6\n"); break;
        case 0xff42bd: printf("KEY_7\n"); break;
        case 0xff4ab5: printf("KEY_8\n"); break;
        case 0xff52ad: printf("KEY_9\n"); break;
        default: 
            // Si la touche n'est pas reconnue, on affiche son code hexadécimal
            printf("UNKNOWN (Code recu: 0x%06lx)\n", masked_code); 
            break;
    }
}

int main(void) {
    // Initialisation
    if (wiringPiSetupGpio() == -1) {
        printf("Erreur d'initialisation de wiringPi\n");
        return 1;
    }

    pinMode(IR_PIN, INPUT);
    pullUpDnControl(IR_PIN, PUD_UP);

    printf("Starting IR remote (C version - polling)\n");
    printf("Use ctrl-c to exit program\n");

    /* wiringPiISR utilise sysfs (/sys/class/gpio/) absent sur kernel >= 6.
     * On remplace par un polling : detection manuelle des fronts descendants. */
    {
        int prev_state = digitalRead(IR_PIN); /* idle = HIGH (pull-up) */
        int cur_state;
        while (1) {
            cur_state = digitalRead(IR_PIN);
            if (cur_state == LOW && prev_state == HIGH) {
                /* Front descendant detecte : appel du handler */
                ir_interrupt();
            }
            prev_state = cur_state;

            if (code_ready) {
                decode_button(final_code);
                code_ready = 0;
            }
            /* Pause 50 µs : suffisant pour le protocole NEC (pulse min ~562 µs) */
            delayMicroseconds(50);
        }
    }

    return 0;
}