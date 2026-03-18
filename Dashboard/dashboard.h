/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard.h
 * Description : Structures et API du dashboard fusee (etat, logique, rendu).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdbool.h>

typedef struct {
    int speed;
    int pressure;
    int altitude;
    int fuel;
    int temperature;
    int downrange;
    int stage;
    int tilt;
    bool running;
    bool paused;
    bool launched;
    bool landing;
    bool exploded;
    bool problem_active;

    int thrust_kn;
    int pitch;
    int yaw;
    int roll;
    int apogee;
    int periapsis;
    int mission_ms;
    float accel_ms2;
    float g_force;

    bool alerts[3];
    int melody_test;
    int melody_ticks;
    char last_event[96];

    bool sim_flight;
    int sim_offset;
    int sim_dir;
    int sim_tick;
    int flame_size;

    bool launch_auth_popup;
    char launch_passbuf[16];
    int launch_passlen;
    int launch_auth_result_ticks;
    bool launch_auth_ok;
} RocketState;

void init_state(RocketState *st);
void apply_cmd(char *line, RocketState *st);
void apply_data(char *line, RocketState *st);
void update_dynamics(RocketState *st, int frame_ms);
void handle_local_input(RocketState *st, int ch);
void draw_dashboard(const RocketState *st, int rows, int cols, const char *cmd_pipe, const char *data_pipe);

#endif
