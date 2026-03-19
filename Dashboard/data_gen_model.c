/************************************************************
 * Projet      : Fusée
 * Fichier     : data_gen_model.c
 * Description : Modèle physique de la fusée pour l'injecteur.
 *               Simule la physique de vol : poussée, altitude,
 *               carburant, température, pression, stress.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#define _GNU_SOURCE
#include "data_gen_model.h"

static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void gen_init(GenModel *gm) {
    memset(gm, 0, sizeof(*gm));
    gm->gen_enabled   = 1;
    gm->fuel_pct      = 100.0;
    gm->temp_c        = 20.0;
    gm->pressure_hpa  = 1013.0;
    gm->last_tick_ms  = now_ms();
    gm->last_emit_ms  = gm->last_tick_ms;
}

void gen_reset_to_ground(GenModel *gm) {
    gm->launched     = false;
    gm->landing      = false;
    gm->exploded     = false;
    gm->paused       = false;
    gm->altitude_m   = 0.0;
    gm->speed_kmh    = 0.0;
    gm->fuel_pct     = 100.0;
    gm->temp_c       = 20.0;
    gm->pressure_hpa = 1013.0;
    gm->thrust_kn    = 0.0;
    gm->stress       = 0.0;
}

void gen_on_event(GenModel *gm, const char *event) {
    if (!event || !event[0]) return;

    if (!strcmp(event, "LAUNCH")) {
        gm->launched = true;
        gm->landing  = false;
        gm->exploded = false;
        gm->paused   = false;
        if (gm->fuel_pct < 5.0) gm->fuel_pct = 100.0;
    } else if (!strcmp(event, "LAND")) {
        if (gm->launched) gm->landing = true;
    } else if (!strcmp(event, "PAUSE")) {
        gm->paused = true;
    } else if (!strcmp(event, "RESUME")) {
        gm->paused = false;
    } else if (!strcmp(event, "TILT_LEFT") || !strcmp(event, "TILT_RIGHT")) {
        if (gm->launched && gm->speed_kmh > 700.0) gm->stress += 1.2;
    } else if (!strcmp(event, "FIX_PROBLEM")) {
        gen_reset_to_ground(gm);
    } else if (!strcmp(event, "FIX_TEMP")) {
        gm->temp_c = 20.0;
    } else if (!strcmp(event, "FIX_STRESS")) {
        gm->stress = 0.0;
    }
}

void gen_step(GenModel *gm, double dt_s) {
    if (!gm->gen_enabled || gm->paused) return;

    if (gm->exploded) {
        gm->thrust_kn   = 0.0;
        gm->speed_kmh  *= 0.92;
        gm->altitude_m -= 220.0 * dt_s;
        gm->temp_c     += 80.0 * dt_s;
        gm->stress     += 5.0 * dt_s;
        if (gm->altitude_m < 0.0) gm->altitude_m = 0.0;
        if (gm->temp_c > 900.0)   gm->temp_c = 900.0;

    } else if (gm->launched && !gm->landing) {
        gm->thrust_kn = 1600.0 - (gm->altitude_m / 120.0);
        gm->thrust_kn = clampd(gm->thrust_kn, 800.0, 1700.0);

        gm->speed_kmh += (55.0 - (gm->speed_kmh / 600.0) - (gm->altitude_m / 8000.0)) * dt_s;
        gm->speed_kmh  = clampd(gm->speed_kmh, 0.0, 30000.0);
        gm->altitude_m += (gm->speed_kmh / 3.6) * dt_s;

        gm->fuel_pct -= (0.35 + gm->thrust_kn / 6000.0) * dt_s;
        if (gm->fuel_pct <= 0.0) {
            gm->fuel_pct = 0.0;
            gm->landing  = true;
        }

    } else if (gm->launched && gm->landing) {
        gm->thrust_kn  = 700.0;
        gm->speed_kmh -= 80.0 * dt_s;
        if (gm->speed_kmh < 120.0) gm->speed_kmh = 120.0;

        gm->altitude_m -= (gm->speed_kmh / 3.6) * dt_s;
        gm->fuel_pct   -= 0.15 * dt_s;
        gm->fuel_pct    = clampd(gm->fuel_pct, 0.0, 100.0);

        if (gm->altitude_m <= 0.0) {
            gm->altitude_m = 0.0;
            if (gm->speed_kmh > 260.0) {
                gm->exploded = true;
            } else {
                gm->launched  = false;
                gm->landing   = false;
                gm->speed_kmh = 0.0;
                gm->thrust_kn = 0.0;
            }
        }
    } else {
        gm->thrust_kn = 0.0;
        if (gm->speed_kmh > 0.0) gm->speed_kmh *= 0.95;
        if (gm->speed_kmh < 1.0) gm->speed_kmh  = 0.0;
        if (gm->altitude_m > 0.0) {
            gm->altitude_m -= 30.0 * dt_s;
            if (gm->altitude_m < 0.0) gm->altitude_m = 0.0;
        }
        if (gm->stress > 0.0) gm->stress -= 0.4 * dt_s;
        if (gm->stress < 0.0) gm->stress  = 0.0;
    }

    if (!gm->exploded) {
        double target_temp = 15.0 + (gm->speed_kmh / 180.0) + (gm->thrust_kn / 85.0) - (gm->altitude_m / 1200.0);
        target_temp = clampd(target_temp, -50.0, 450.0);
        gm->temp_c += (target_temp - gm->temp_c) * 0.25;
    }

    gm->pressure_hpa = 1013.0 - (gm->altitude_m * 0.06);
    gm->pressure_hpa = clampd(gm->pressure_hpa, 30.0, 1013.0);

    if (gm->temp_c > 380.0) gm->stress += (gm->temp_c - 380.0) * 0.02 * dt_s;
    if (gm->fuel_pct < 4.0 && gm->thrust_kn > 1000.0) gm->stress += 8.0 * dt_s;
    if (gm->stress > 100.0) gm->exploded = true;
}
