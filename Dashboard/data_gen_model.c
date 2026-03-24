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
#include "satellite_protocol.h"

#define ORBIT_ALTITUDE_M 100000.0
#define ORBIT_SPEED_KMH 27200.0
#define ASCENT_TARGET_S 40.0
#define LANDING_TARGET_S 30.0

static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int gen_in_orbit(const GenModel *gm) {
    return gm->altitude_m >= ORBIT_ALTITUDE_M;
}

static double fuel_burn_per_sec(const GenModel *gm) {
    if (gm->fuel_pct > 70.0)
        return 1.9;
    if (gm->fuel_pct > 20.0)
        return 1.5;
    if (gm->fuel_pct > 0.0)
        return 0.7;
    return 0.0;
}

static double nominal_speed_target(const GenModel *gm) {
    double progress = clampd(gm->ascent_elapsed_s / ASCENT_TARGET_S, 0.0, 1.0);
    double curve = progress * progress * (3.0 - (2.0 * progress));
    return 4000.0 + (ORBIT_SPEED_KMH - 4000.0) * curve;
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
    gm->fault1_active = false;
    gm->fault2_active = false;
    gm->fault1_elapsed_s = 0.0;
    gm->fault2_elapsed_s = 0.0;
    gm->ascent_elapsed_s = 0.0;
    gm->landing_elapsed_s = 0.0;
    gm->landing_start_fuel_pct = 0.0;
}

void gen_on_event(GenModel *gm, const char *event) {
    if (!event || !event[0]) return;

    if (!strcmp(event, "LAUNCH")) {
        gen_reset_to_ground(gm);
        gm->launched = true;
    } else if (!strcmp(event, "LAND")) {
        if (gm->launched && gm->altitude_m >= ORBIT_ALTITUDE_M) {
            gm->landing = true;
            gm->landing_elapsed_s = 0.0;
            gm->landing_start_fuel_pct = gm->fuel_pct;
        }
        if (gm->speed_kmh > 6000.0) gm->speed_kmh = 6000.0;
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
        gm->fault1_active = false;
        gm->fault1_elapsed_s = 0.0;
    } else if (!strcmp(event, "FIX_STRESS")) {
        gm->stress = 0.0;
        gm->fault2_active = false;
        gm->fault2_elapsed_s = 0.0;
    }
}

void gen_step(GenModel *gm, double dt_s) {
    if (gm->paused) return;

    if (gm->exploded) {
        gm->thrust_kn   = 0.0;
        gm->speed_kmh  *= 0.92;
        gm->altitude_m -= 220.0 * dt_s;
        gm->temp_c     += 80.0 * dt_s;
        gm->stress     += 5.0 * dt_s;
        if (gm->altitude_m < 0.0) gm->altitude_m = 0.0;
        if (gm->temp_c > 900.0)   gm->temp_c = 900.0;

    } else if (gm->launched && !gm->landing) {
        if (!gen_in_orbit(gm)) {
            double fuel_ratio;
            double progress;
            double speed_target = nominal_speed_target(gm);
            double speed_delta = speed_target - gm->speed_kmh;
            double recovery_ratio = clampd(speed_delta / 6000.0, 0.0, 1.0);
            double altitude_rate;
            double speed_adjust;

            gm->ascent_elapsed_s += dt_s;
            progress = clampd(gm->ascent_elapsed_s / ASCENT_TARGET_S, 0.0, 1.0);
            fuel_ratio = clampd(gm->fuel_pct / 20.0, 0.0, 1.0);

            if (gm->fuel_pct > 0.0) {
                altitude_rate = (900.0 + (1150.0 * progress) + (150.0 * recovery_ratio))
                              * (0.25 + (0.75 * fuel_ratio));
                gm->thrust_kn = (1450.0 - (320.0 * progress) + (260.0 * recovery_ratio))
                              * (0.25 + (0.75 * fuel_ratio));
                gm->thrust_kn = clampd(gm->thrust_kn, 0.0, 1700.0);
                if (speed_delta >= 0.0)
                    speed_adjust = clampd(speed_delta * 0.16, 80.0, 760.0 * (0.25 + (0.75 * fuel_ratio)));
                else
                    speed_adjust = clampd(speed_delta * 0.08, -520.0, -40.0);
                gm->speed_kmh += speed_adjust * dt_s;
                gm->altitude_m += clampd(altitude_rate, 280.0, 2300.0) * dt_s;
            } else {
                gm->thrust_kn = 0.0;
                gm->speed_kmh -= 360.0 * dt_s;
                gm->altitude_m += clampd((gm->speed_kmh / 3.6) * 0.10, -250.0, 520.0) * dt_s;
                if (gm->speed_kmh < 400.0)
                    gm->altitude_m -= 140.0 * dt_s;
            }

            gm->speed_kmh  = clampd(gm->speed_kmh, 0.0, ORBIT_SPEED_KMH);
            if (gm->altitude_m < 0.0)
                gm->altitude_m = 0.0;

            if (gm->fuel_pct > 0.0) {
                double burn = fuel_burn_per_sec(gm) + (0.8 * recovery_ratio);
                gm->fuel_pct -= burn * dt_s;
                if (gm->fuel_pct < 0.0)
                    gm->fuel_pct = 0.0;
            } else {
                gm->fuel_pct = 0.0;
            }

            if (gm->altitude_m >= ORBIT_ALTITUDE_M && gm->fuel_pct > 0.0) {
                gm->altitude_m = ORBIT_ALTITUDE_M;
                gm->speed_kmh  = ORBIT_SPEED_KMH;
            }
        } else {
            gm->altitude_m += (ORBIT_ALTITUDE_M - gm->altitude_m) * 0.04;
            gm->speed_kmh  += (ORBIT_SPEED_KMH - gm->speed_kmh) * 0.06;
            gm->thrust_kn   = 0.0;
        }

        if (gm->fuel_pct <= 0.0) {
            gm->fuel_pct = 0.0;
        }

    } else if (gm->launched && gm->landing) {
        double progress;
        double correction_ratio;
        int controlled_descent;

        gm->landing_elapsed_s += dt_s;
        progress = clampd(gm->landing_elapsed_s / LANDING_TARGET_S, 0.0, 1.0);
        correction_ratio = clampd((gm->speed_kmh - 160.0) / 24000.0, 0.0, 1.0);
        controlled_descent = (gm->landing_start_fuel_pct >= 20.0 &&
                              gm->fuel_pct > 0.0);

        gm->thrust_kn  = controlled_descent ? (820.0 + (520.0 * correction_ratio)) : 120.0;
        if (controlled_descent) {
            double descent_rate = 1800.0 + (3200.0 * (1.0 - progress));
            double landing_burn = (0.24 - (0.08 * progress)) + (1.9 * correction_ratio);
            gm->speed_kmh += (160.0 - gm->speed_kmh) * (0.17 + (0.33 * correction_ratio)) * dt_s;
            gm->speed_kmh = clampd(gm->speed_kmh, 140.0, 6000.0);
            gm->altitude_m -= descent_rate * dt_s;
            gm->fuel_pct -= landing_burn * dt_s;
            gm->fuel_pct = clampd(gm->fuel_pct, 0.0, 100.0);
        } else {
            double descent_rate = 2200.0 + (1600.0 * (1.0 - progress));
            gm->thrust_kn = 160.0;
            gm->speed_kmh += (900.0 - gm->speed_kmh) * 0.12 * dt_s;
            gm->speed_kmh  = clampd(gm->speed_kmh, 260.0, 9000.0);
            gm->altitude_m -= descent_rate * dt_s;
        }
        gm->fuel_pct = clampd(gm->fuel_pct, 0.0, 100.0);

        if (gm->altitude_m <= 0.0) {
            gm->altitude_m = 0.0;
            if (gm->speed_kmh > 220.0 || gm->fuel_pct <= 0.0 || !controlled_descent) {
                gm->exploded = true;
            } else {
                gm->launched  = false;
                gm->landing   = false;
                gm->speed_kmh = 0.0;
                gm->thrust_kn = 0.0;
                gm->landing_elapsed_s = 0.0;
                gm->landing_start_fuel_pct = 0.0;
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
        double target_temp = 18.0 + (gm->speed_kmh / 650.0) + (gm->thrust_kn / 140.0) - (gm->altitude_m / 1800.0);
        target_temp = clampd(target_temp, -55.0, 320.0);
        gm->temp_c += (target_temp - gm->temp_c) * 0.25;
    }

    gm->pressure_hpa = 1013.0 - (gm->altitude_m * 0.06);
    gm->pressure_hpa = clampd(gm->pressure_hpa, 30.0, 1013.0);

    gm->fault1_active = (gm->temp_c > SAT_TEMP_CRITICAL);
    gm->fault2_active = (gm->stress > SAT_STRESS_CRITICAL);
    if (gm->fault1_active) gm->fault1_elapsed_s += dt_s; else gm->fault1_elapsed_s = 0.0;
    if (gm->fault2_active) gm->fault2_elapsed_s += dt_s; else gm->fault2_elapsed_s = 0.0;
    if (gm->temp_c > 380.0) gm->stress += (gm->temp_c - 380.0) * 0.02 * dt_s;
    if (gm->landing && gm->fuel_pct <= 0.0) gm->stress += 6.0 * dt_s;
    if (!gm->landing && gm->stress > 0.0) gm->stress -= 1.5 * dt_s;
    gm->stress = clampd(gm->stress, 0.0, 100.0);
    if (gm->stress > 100.0 || gm->fault1_elapsed_s >= 10.0 || gm->fault2_elapsed_s >= 10.0)
        gm->exploded = true;
}
