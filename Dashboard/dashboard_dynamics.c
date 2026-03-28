/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard_dynamics.c
 * Description : Dynamique physique de la fusée dans le dashboard
 *               (simulation locale, indépendante de la télémétrie réseau).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "dashboard.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>

static int clampi(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static bool in_orbit(const RocketState *st) {
    return st->altitude >= 100000;
}

static bool telemetry_fresh(const RocketState *st) {
    return (st->mission_ms - st->last_telemetry_ms) <= 1200;
}

static int nominal_speed_target(const RocketState *st) {
    int alt_ratio = st->altitude > 100000 ? 100000 : st->altitude;
    if (st->fuel > 70)
        return 9000 + ((9000 * alt_ratio) / 100000);
    return 18000 + ((9200 * alt_ratio) / 100000);
}

static int controlled_landing_target_fuel(const RocketState *st) {
    int progress_pct = 0;
    int target_end = clampi(st->landing_start_fuel - 18, 10, 14);

    if (st->altitude <= 0) {
        return target_end;
    }
    progress_pct = 100 - ((st->altitude * 100) / 100000);
    progress_pct = clampi(progress_pct, 0, 100);
    return st->landing_start_fuel +
           ((target_end - st->landing_start_fuel) * progress_pct) / 100;
}

void update_dynamics(RocketState *st, int frame_ms) {
    static int prev_speed = 0;
    static float filtered_accel = 0.0f;
    bool live_telemetry;

    if (st->paused || st->exploded) {
        return;
    }

    st->mission_ms += frame_ms;
    live_telemetry = telemetry_fresh(st);

    if (!st->launched && !st->landing) {
        prev_speed = st->speed;
        filtered_accel = 0.0f;
        st->accel_ms2 = 0.0f;
        st->g_force = 1.0f;
    }

    if (st->launched && !st->landing && !live_telemetry) {
        if (st->problem_active) {
            /* Panne active : perte de poussée, descente forcée */
            st->speed    = st->speed    > 20 ? st->speed    - 20 : 0;
            st->altitude = st->altitude > 18 ? st->altitude - 18 : 0;
            st->flame_size = 1;
        } else if (st->fuel > 0 && !in_orbit(st)) {
            int speed_target = nominal_speed_target(st);
            int speed_deficit = speed_target - st->speed;
            int fuel_burn = st->fuel > 70 ? 2 : (st->fuel > 20 ? 1 : 0);
            if (st->fuel > 70) {
                if (st->speed < 18000)
                    st->speed += 120 + (speed_deficit > 0 ? speed_deficit / 80 : 0);
                st->altitude += st->speed / 22;
            } else {
                if (st->speed < 27200)
                    st->speed += 55 + (speed_deficit > 0 ? speed_deficit / 110 : 0);
                st->altitude += st->speed / 28;
            }

            st->fuel -= fuel_burn;
            if (st->fuel < 0) {
                st->fuel = 0;
            }
            if (st->altitude >= 100000) {
                st->altitude = 100000;
                st->speed = 27200;
                if (st->fuel < 20) st->fuel = 20;
                if (st->fuel > 27) st->fuel = 27;
            }
            st->downrange += st->speed / 130;
            st->flame_size = 2 + ((st->mission_ms / 120) % 3);
            if (in_orbit(st)) {
                st->altitude = 100000;
                st->speed = 27200;
                if (st->fuel < 20) st->fuel = 20;
                if (st->fuel > 27) st->fuel = 27;
                snprintf(st->last_event, sizeof(st->last_event),
                         "Orbit reached: nominal insertion, fuel reserve 20%%");
            }
        } else if (in_orbit(st)) {
            st->altitude += (100000 - st->altitude) / 8;
            st->speed += (27200 - st->speed) / 6;
            if (st->fuel < 20) st->fuel = 20;
            if (st->fuel > 27) st->fuel = 27;
            st->downrange += st->speed / 160;
            st->flame_size = 0;
            st->sim_tick = 0;
        } else {
            st->fuel = 0;
            st->exploded = true;
            st->launched = false;
            st->landing = false;
            st->problem_active = true;
            st->alerts[0] = st->alerts[1] = st->alerts[2] = true;
            snprintf(st->last_event, sizeof(st->last_event),
                     "Fuel exhausted during flight: vehicle lost");
            st->thrust_kn = 0;
            st->flame_size = 0;
        }
    }

    /* Position latérale pilotée par le joystick (tilt).
     * LEFT → tilt=-1 → sim_offset dérive vers -8 (gauche)
     * RIGHT → tilt=+1 → sim_offset dérive vers +8 (droite)
     * Neutre → retour progressif au centre. */
    if (st->launched && !st->landing) {
        if (st->tilt < 0 && st->sim_offset > -8)
            st->sim_offset--;
        else if (st->tilt > 0 && st->sim_offset < 8)
            st->sim_offset++;
        else if (st->tilt == 0) {
            if (st->sim_offset > 0) st->sim_offset--;
            else if (st->sim_offset < 0) st->sim_offset++;
        }
    } else if (!st->launched) {
        if (st->sim_offset > 0)
            st->sim_offset--;
        else if (st->sim_offset < 0)
            st->sim_offset++;
    }

    if (st->landing && !live_telemetry) {
        int controlled_descent = (st->landing_start_fuel >= 20 &&
                                  st->fuel > 0 &&
                                  st->speed <= 27200 &&
                                  !st->fault1_display && !st->fault2_display);

        if (controlled_descent) {
            int target_fuel = controlled_landing_target_fuel(st);
            st->speed = st->speed > 140 ? st->speed - 420 : 140;
            {
                int descent_step = st->altitude / 14;
                if (descent_step < 160) descent_step = 160;
                if (descent_step > 2600) descent_step = 2600;
                st->altitude = st->altitude > descent_step ? st->altitude - descent_step : 0;
            }
            if (st->fuel > target_fuel) {
                st->fuel = target_fuel;
            }
        } else {
            st->problem_active = true;
            st->alerts[2] = true;
            st->speed = st->speed > 260 ? st->speed - 60 : 260;
            st->altitude = st->altitude > (st->speed / 7) ? st->altitude - (st->speed / 7) : 0;
            snprintf(st->last_event, sizeof(st->last_event),
                     "Landing unstable: insufficient fuel reserve or excessive speed");
        }

        if (st->fuel <= 0) {
            st->fuel = 0;
            st->exploded = true;
            st->launched = false;
            st->landing = false;
            st->problem_active = true;
            st->alerts[0] = st->alerts[1] = st->alerts[2] = true;
            st->flame_size = 0;
            snprintf(st->last_event, sizeof(st->last_event),
                     "Fuel exhausted during landing: vehicle lost");
        }

        if (st->exploded) {
            st->flame_size = 0;
        } else if (st->altitude > 600) {
            st->flame_size = 4;
        } else if (st->altitude > 300) {
            st->flame_size = 3;
        } else if (st->altitude > 100) {
            st->flame_size = 2;
        } else {
            st->flame_size = 1;
        }

        if (!st->exploded && st->altitude <= 0) {
            st->altitude       = 0;
            if (st->fuel <= 0) {
                st->exploded = true;
                st->flame_size = 0;
                st->alerts[0] = st->alerts[1] = st->alerts[2] = true;
                snprintf(st->last_event, sizeof(st->last_event),
                         "Landing failure: no fuel reserve for cushioning");
            } else {
                st->landing        = false;
                st->launched       = false;
                st->landing_start_fuel = 0;
                st->flame_size     = 0;
                st->problem_active = false;
                st->alerts[2]      = false;
                st->speed          = 0;
                snprintf(st->last_event, sizeof(st->last_event), "Touchdown confirmed");
            }
        }
    }

    if (!live_telemetry) {
        st->pressure = 1013 - (st->altitude / 30);
        if (st->pressure < 120) {
            st->pressure = 120;
        }
    }

    if (!live_telemetry) {
        st->thrust_kn = (st->launched && !in_orbit(st) && !st->landing && st->fuel > 0) ? (1000 + st->speed / 12)
                       : (st->landing && st->fuel > 0) ? 820
                       : 0;
        st->temperature = 18 + st->thrust_kn / 70 - st->altitude / 1800;
    }
    if (frame_ms > 0) {
        float raw_accel = ((float)(st->speed - prev_speed) / 3.6f) / (frame_ms / 1000.0f);
        filtered_accel = (filtered_accel * 0.82f) + (raw_accel * 0.18f);
        if (live_telemetry && fabsf(raw_accel) < 0.05f)
            filtered_accel *= 0.97f;
        st->accel_ms2 = filtered_accel;
    } else {
        st->accel_ms2 = filtered_accel;
    }
    st->g_force = 1.0f + st->accel_ms2 / 9.81f + (st->thrust_kn / 2600.0f);
    if (st->g_force < 0.0f) {
        st->g_force = 0.0f;
    }

    st->pitch = st->tilt * 14;
    st->yaw = ((st->mission_ms / 1000) % 11) - 5;
    st->roll = (st->mission_ms / 15) % 360;

    if (st->fuel > 66) {
        st->stage = 1;
    } else if (st->fuel > 33) {
        st->stage = 2;
    } else {
        st->stage = 3;
    }

    st->apogee = st->altitude + st->speed * 10;
    st->periapsis = st->altitude > 900 ? st->altitude - 900 : 0;

    st->alerts[0] = false;
    st->alerts[1] = false;
    st->alerts[2] = st->problem_active || st->fault1_display || st->fault2_display;
    if (!st->fault1_display && !st->fault2_display && !st->problem_active)
        st->alerts[2] = false;
    if (st->fault1_display && st->fault1_since_ms > 0 &&
        st->mission_ms - st->fault1_since_ms >= 10000) {
        st->exploded = true;
        snprintf(st->last_event, sizeof(st->last_event),
                 "Fault1 unresolved for 10s: vehicle lost");
    }
    if (st->fault2_display && st->fault2_since_ms > 0 &&
        st->mission_ms - st->fault2_since_ms >= 10000) {
        st->exploded = true;
        snprintf(st->last_event, sizeof(st->last_event),
                 "Fault2 unresolved for 10s: vehicle lost");
    }
    if (st->temperature < -60) {
        st->temperature = -60;
    }
    if (st->temperature > 420) {
        st->temperature = 420;
    }

    if (st->melody_ticks > 0) {
        st->melody_ticks--;
        if (st->melody_ticks == 0) {
            st->melody_test = 0;
            snprintf(st->last_event, sizeof(st->last_event), "Melody test complete");
        }
    }

    if (st->launch_auth_result_ticks > 0) {
        st->launch_auth_result_ticks--;
        if (st->launch_auth_result_ticks == 0) {
            st->launch_auth_ok = false;
        }
    }

    prev_speed = st->speed;
}
