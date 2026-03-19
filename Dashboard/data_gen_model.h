/************************************************************
 * Projet      : Fusée
 * Fichier     : data_gen_model.h
 * Description : Modèle physique de la fusée pour l'injecteur
 *               (GenModel struct + API).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef DATA_GEN_MODEL_H
#define DATA_GEN_MODEL_H

#include "dashboard_common.h"

typedef struct {
    int  gen_enabled;
    bool launched;
    bool landing;
    bool exploded;
    bool paused;
    double altitude_m;
    double speed_kmh;
    double fuel_pct;
    double temp_c;
    double pressure_hpa;
    double thrust_kn;
    double stress;
    unsigned long long last_tick_ms;
    unsigned long long last_emit_ms;
} GenModel;

void gen_init(GenModel *gm);
void gen_reset_to_ground(GenModel *gm);
void gen_on_event(GenModel *gm, const char *event);
void gen_step(GenModel *gm, double dt_s);

#endif /* DATA_GEN_MODEL_H */
