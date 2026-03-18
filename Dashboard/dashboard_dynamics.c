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

static bool in_orbit(const RocketState *st) {
    return st->altitude >= 100000 && st->speed >= 27000;
}

void update_dynamics(RocketState *st, int frame_ms) {
    static int prev_speed = 0;

    if (st->paused || st->exploded) {
        return;
    }

    st->mission_ms += frame_ms;

    if (!st->launched && !st->landing) {
        prev_speed = st->speed;
        st->accel_ms2 = 0.0f;
        st->g_force = 1.0f;
    }

    if (st->launched && !st->landing) {
        if (st->fuel > 0) {
            st->fuel -= 1;
            st->speed += 6;
            st->altitude += st->speed / 70;
            st->downrange += st->speed / 130;
            st->flame_size = 2 + ((st->mission_ms / 120) % 3);
        } else {
            st->fuel = 0;
            if (!in_orbit(st)) {
                if (!st->landing) {
                    st->landing = true;
                    st->problem_active = true;
                    st->alerts[2] = true;
                    snprintf(st->last_event, sizeof(st->last_event),
                             "Fuel exhausted outside orbit: ballistic return");
                }
                st->speed = st->speed > 40 ? st->speed - 30 : 0;
            } else {
                st->downrange += st->speed / 160;
            }
            st->thrust_kn = 0;
            st->flame_size = 0;
        }
    }

    if (st->sim_flight && st->launched && !st->landing) {
        st->sim_tick++;
        if (st->sim_tick >= 2) {
            st->sim_tick = 0;
            st->sim_offset += st->sim_dir;
            if (st->sim_offset >= 8) {
                st->sim_offset = 8;
                st->sim_dir = -1;
            }
            if (st->sim_offset <= -8) {
                st->sim_offset = -8;
                st->sim_dir = 1;
            }
        }
    } else {
        if (st->sim_offset > 0) {
            st->sim_offset--;
        } else if (st->sim_offset < 0) {
            st->sim_offset++;
        }
    }

    if (st->landing) {
        st->speed = st->speed > 20 ? st->speed - 18 : 0;
        st->altitude = st->altitude > 12 ? st->altitude - 12 : 0;

        if (st->altitude > 600) {
            st->flame_size = 4;
        } else if (st->altitude > 300) {
            st->flame_size = 3;
        } else if (st->altitude > 100) {
            st->flame_size = 2;
        } else {
            st->flame_size = 1;
        }

        if (st->altitude <= 0) {
            st->altitude  = 0;
            st->landing   = false;
            st->launched  = false;
            st->flame_size = 0;
            snprintf(st->last_event, sizeof(st->last_event), "Touchdown confirmed");
        }
    }

    st->pressure = 1013 - (st->altitude / 30);
    if (st->pressure < 120) {
        st->pressure = 120;
    }

    st->thrust_kn = st->fuel > 0 ? (900 + st->speed / 2) : 0;
    st->temperature = 20 + st->thrust_kn / 150;
    if (frame_ms > 0)
        st->accel_ms2 = ((float)(st->speed - prev_speed) / 3.6f) / (frame_ms / 1000.0f);
    else
        st->accel_ms2 = 0.0f;
    st->g_force = 1.0f + st->accel_ms2 / 9.81f;
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

    if (st->fuel < 20) {
        st->alerts[0] = true;
    }
    if (st->launched && st->mission_ms > 3000 && st->g_force > 4.0f) {
        st->alerts[1] = true;
    }
    if (st->problem_active) {
        st->alerts[2] = true;
    }
    if (st->launched && !st->landing && !in_orbit(st) &&
        st->mission_ms > 10000 && st->altitude > 5000 && st->speed < 900) {
        if (!st->problem_active) {
            snprintf(st->last_event, sizeof(st->last_event),
                     "Insufficient velocity: trajectory unstable");
        }
        st->problem_active = true;
        st->alerts[2] = true;
    }
    if (st->launched && st->mission_ms > 5000 && st->temperature > 100) {
        if (!st->problem_active) {
            snprintf(st->last_event, sizeof(st->last_event),
                     "Thermal warning: propulsion temperature too high");
        }
        st->problem_active = true;
        st->alerts[2] = true;
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
        if (st->launch_auth_result_ticks == 0 && st->launch_auth_ok && !st->launched) {
            st->launched = true;
            st->landing = false;
            st->exploded = false;
            st->paused = false;
            st->fuel  = 100;
            st->speed = 180;
            prev_speed = st->speed;
            st->launch_auth_ok = false;
            snprintf(st->last_event, sizeof(st->last_event),
                     "Ignition sequence complete: liftoff");
        }
    }

    prev_speed = st->speed;
}
