# Projet OBJ - Architecture Socket + NCurses

## Structure
- `Dashboard/` : dashboard ncurses, logique mission, controleur local, injecteur local.
- `Network/` : protocole dictionnaire, serveur bridge TCP, client TCP interactif + viewer.
- `JoyPi/` : interaction JoyPi (boutons physiques -> commandes socket).
- `Test/` : tests C (protocole + integration socket).

## Flux temps reel
1. `controle_fusee` (dashboard) lit en continu les pipes:
   - `/tmp/rocket_cmd.pipe`
   - `/tmp/rocket_data.pipe`
2. `socket_bridge_server` ecoute en TCP (port `5555` par defaut).
3. Chaque message recu est decode via `satellite_handler.c` puis redirige:
   - vers pipe commande (tilt, launch, land, alertes, etc.)
   - vers pipe data (SET FUEL, SET SPEED, PROBLEM ON/OFF, etc.)
4. Le dashboard met a jour l'affichage en temps reel.

## Codes d'echange (dictionnaire)
### Commandes mission
- `JK <n>` => evenement JoyPi (viewer uniquement, pas de pipe dashboard)
- `TL` => `TILT_LEFT`
- `TR` => `TILT_RIGHT`
- `TS` => `STRAIGHT`
- `LU` => `LAUNCH`
- `LD` => `LAND`
- `M1|M2|M3` => test melodie 1/2/3
- `FX` => `FIX_PROBLEM`
- `EX` => `EXPLODE`
- `PZ` => `PAUSE`
- `RS` => `RESUME`
- `A1|A2|A3` => test alertes 1/2/3
- `AC` => `CLEAR_ALERTS`
- `SV 1|ON` => `SIM_FLIGHT ON`
- `SV 0|OFF` => `SIM_FLIGHT OFF`
- `QT` => `QUIT`

### Donnees telemetrie
- `SF <v>` ou `SF<v>` => `SET FUEL <v>`
- `SS <v>` ou `SS<v>` => `SET SPEED <v>`
- `SP <v>` ou `SP<v>` => `SET PRESSURE <v>`
- `SA <v>` ou `SA<v>` => `SET ALTITUDE <v>`
- `ST <v>` ou `ST<v>` => `SET TEMP <v>`
- `SH <v>` ou `SH<v>` => `SET THRUST <v>`
- `PR 1|ON` => `PROBLEM ON`
- `PR 0|OFF` => `PROBLEM OFF`

Exemple: `SF2` = fuel a 2.

## Lancement
1. `make`
2. Terminal A: `./bin-util/controle_fusee`
3. Terminal B: `./bin-proto/socket_bridge_server 5555`
4. Terminal C: `./bin-proto/socket_data_client` puis saisir IP/port
5. Terminal D (option): `./bin-proto/socket_data_viewer` pour voir les commandes mirrored
6. Option demo launch-only: `./bin-proto/socket_launch_demo`
7. Option pilotage VM: `./bin-util/visual_motor 127.0.0.1 5555`
8. Monitoring appuis JoyPi + injection data: `./bin-util/controle_fusee_data 127.0.0.1 5555`

## Scenario VM/JoyPi
- JoyPi (controleur): `socket_bridge_server` + `controle_fusee`
- VM (fusee): `visual_motor` + `controle_fusee_data`
- `controle_fusee_data` supporte `GEN 1` (defaut) / `GEN 0` pour generation auto des donnees.
- Les boutons JoyPi envoient uniquement des commandes de controle (pas d'injection `SET ...`).

Pour activer auto-completion TAB + historique (si readline dispo):
- `make READLINE_CFLAGS=-DENABLE_READLINE READLINE_LIB=-lreadline`

Optionnel en local:
- `./bin-util/controle_fusee_control` (controle ncurses local)
- `./bin-util/controle_fusee_data` (injection locale via pipe)

## Alerte probleme
Quand `problem_active` est vrai (ou alerte 3 active), le dashboard affiche:
- `PROBLEME CRITIQUE GUIDAGE` en rouge clignotant.

## Launch securise
- La commande `launch` / `LU` ouvre une popup de mot de passe sur le dashboard.
- Mot de passe attendu: `123`.

## Cross compilation
- Build complet: `make cross`
- Build avec wiringPi: `make cross WIRINGPI_CFLAGS=-DUSE_WIRINGPI WIRINGPI_LIBS=-lwiringPi`
- Build JoyPi minimal: `make pack-joypi`
- Build VM minimal (compile RPi, sortie `To-RPI`): `make pack-rpi`
