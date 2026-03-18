/************************************************************
 * Projet      : Fusée
 * Fichier     : satellite_protocol.h
 * Description : Definitions du protocole satellite TCP.
 *               VM (192.168.64.7) = serveur satellite, port 5555.
 *               JoyPi             = client CONTROLLER.
 *               controle_fusee_data = client INJECTOR (localhost).
 *
 * Protocole JoyPi -> Satellite :
 *   AUTH CONTROLLER\n       -> AUTH_OK\n
 *   CMD LU\n                -> OK LAUNCH\n | FAIL ALREADY_LAUNCHED\n | FAIL NOT_READY\n
 *   CMD LD\n                -> OK LAND\n   | FAIL NOT_FLYING\n
 *   CMD ALT\n               -> DATA ALT <v>\n
 *   CMD TEMP\n              -> DATA TEMP <v>\n
 *   CMD PRES\n              -> OK PRES_FAULT\n | OK PRES_FIX\n | OK PRES_OK\n
 *   CMD MEL <1|2|3>\n       -> OK MEL <n>\n
 *
 * Protocole controle_fusee_data -> Satellite :
 *   AUTH INJECTOR\n         -> AUTH_OK\n
 *   SET ALTITUDE <v>\n      -> OK\n
 *   SET SPEED <v>\n         -> OK\n
 *   SET FUEL <v>\n          -> OK\n
 *   SET TEMP <v>\n          -> OK\n
 *   SET PRESSURE <v>\n      -> OK\n
 *   SET THRUST <v>\n        -> OK\n
 *   SET STRESS <v>\n        -> OK\n
 *
 * Push Satellite -> JoyPi (CONTROLLER) :
 *   TELEMETRY alt=<v> speed=<v> fuel=<v> temp=<v> pressure=<v> thrust=<v> state=<STATE>\n
 *   EVENT LAUNCH\n | EVENT LAND\n | EVENT LAND_AUTO\n | EVENT LANDED\n
 *   EVENT PROBLEM\n | EVENT RESOLVED\n | EVENT MEL <n>\n
 *
 * Push Satellite -> controle_fusee_data (INJECTOR) :
 *   CMD_EVENT LAUNCH\n | CMD_EVENT LAND\n | CMD_EVENT PAUSE\n | CMD_EVENT RESUME\n
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef SATELLITE_PROTOCOL_H
#define SATELLITE_PROTOCOL_H

#include <stdbool.h>

#define SAT_PORT              5555
#define SAT_TEMP_CRITICAL      350   /* degres C -> EVENT PROBLEM auto */
#define SAT_STRESS_CRITICAL     80   /* unites  -> EVENT PROBLEM auto */

/* Note : le mot de passe de lancement ("123") est verifie localement
 * sur le JoyPi avant d'envoyer CMD LU. Le satellite fait confiance
 * au client CONTROLLER authentifie (reseau local LAN). */

typedef enum {
    SAT_CLIENT_UNKNOWN     = 0,
    SAT_CLIENT_CONTROLLER  = 1,   /* JoyPi */
    SAT_CLIENT_INJECTOR    = 2    /* controle_fusee_data */
} SatClientType;

typedef enum {
    SAT_STATE_READY      = 0,
    SAT_STATE_FLYING     = 1,
    SAT_STATE_LANDING    = 2,
    SAT_STATE_EMERGENCY  = 3,
    SAT_STATE_EXPLODED   = 4
} SatRocketState;

typedef struct {
    int             altitude;
    int             speed;
    int             fuel;
    int             temp;
    int             pressure;
    int             thrust;
    int             stress;
    SatRocketState  state;
    bool            pressure_fault;
    bool            pressure_corrector;
} SatTelemetry;

/* Retourne le nom lisible d'un etat fusee. */
const char *sat_state_name(SatRocketState s);

#endif /* SATELLITE_PROTOCOL_H */
