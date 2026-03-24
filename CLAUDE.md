# CLAUDE.md — Projet Fusée JoyPi/VM

## Architecture réseau

| Nœud | IP | Rôle | Binaires |
|------|-----|------|---------|
| VM | 192.168.1.25 | Serveur TCP central | `satellite_server`, `controle_fusee_data`, `controle_fusee` |
| JoyPi | 192.168.1.21 | Client GPIO | `joypi_controller`, `controle_fusee` |

Déploiement JoyPi : `sshpass -p raspberry scp To-JoyPI/bin-util/controle_fusee To-JoyPI/bin-util/joypi_controller pi@192.168.1.21:/home/pi/rocket/`

---

## Build

```bash
make all         # VM x86 + ARM cross-compilé
make vm          # VM seulement → To-VM/
make joypi       # JoyPi ARM → To-JoyPI/
make clean
```

- Cross-compilateur : `arm-linux-gnueabihf-gcc` (Linaro)
- wiringPi : `/home/leo/Desktop/Objet_Connecte/wiringPi-36fb7f1/wiringPi/`
- ncurses cross : `/home/leo/Desktop/CCR/ncurses-lab/target_NC_PI/`
- Build actuel : **0 warning, 0 erreur**

---

## Protocole TCP (port 5555)

### CONTROLLER → Satellite
```
AUTH CONTROLLER          → AUTH_OK
CMD LU                   → OK LAUNCH | FAIL ALREADY_LAUNCHED | FAIL NOT_READY
CMD LD                   → OK LAND | FAIL NOT_FLYING
CMD ALT                  → DATA ALT <v>
CMD TEMP                 → DATA TEMP <v>
CMD PRES                 → OK PRES_FAULT | OK PRES_FIX | OK PRES_OK
CMD MEL <1|2|3>          → OK MEL <n>
CMD REP1                 → OK REP1_FIX | OK REP1_NONE   (répare panne température)
CMD REP2                 → OK REP2_FIX | OK REP2_NONE   (répare panne stress)
```

### Satellite → CONTROLLER (push)
```
TELEMETRY alt=<v> speed=<v> fuel=<v> temp=<v> pressure=<v> thrust=<v> state=<STATE>
EVENT LAUNCH | LAND | LAND_AUTO | LANDED
EVENT PROBLEM | RESOLVED                  (CMD PRES / BT5 — rétrocompat)
EVENT PROBLEM1 | RESOLVED1               (panne température — BT7)
EVENT PROBLEM2 | RESOLVED2               (panne stress — BT8)
EVENT MEL <n>
```

### INJECTOR → Satellite
```
AUTH INJECTOR
SET ALTITUDE|SPEED|FUEL|TEMP|PRESSURE|THRUST|STRESS <v>
```

### Satellite → INJECTOR (push)
```
CMD_EVENT LAUNCH | LAND | RESUME | FIX_TEMP | FIX_STRESS
```

### Seuils de déclenchement auto (satellite_protocol.h)
- `SAT_TEMP_CRITICAL = 350` → EVENT PROBLEM1 si FLYING
- `SAT_STRESS_CRITICAL = 80` → EVENT PROBLEM2 si FLYING

---

## Mapping des 8 boutons JoyPi

Seuls les COL pin33 (c=3) et pin35 (c=2) sont câblés physiquement → 8 touches actives.

| Bouton | key_num | Row/Col | Action |
|--------|---------|---------|--------|
| BT1 | 1 | row0/col3 (pin33) | Lancement → popup mdp dans controle_fusee + arme IR |
| BT2 | 2 | row0/col2 (pin35) | Atterrissage d'urgence |
| BT3 | 5 | row1/col3 (pin33) | Demande altitude → 7-seg (reste affiché) |
| BT4 | 6 | row1/col2 (pin35) | Demande température → 7-seg (reste affiché) |
| BT5 | 9 | row2/col3 (pin33) | Cycle pression (3 états : fault → fix → ok) |
| BT6 | 10 | row2/col2 (pin35) | Mélodie test (mélodie 1 fixe) |
| **BT7** | **13** | **row3/col3 (pin33)** | **Répare panne 1 (température)** — CMD REP1 |
| **BT8** | **14** | **row3/col2 (pin35)** | **Répare panne 2 (stress structurel)** — CMD REP2 |

Touches mode mot de passe (IR remote uniquement — matrice ignorée en MODE_PASSWORD) :
- **KEY_CONFIRM = 13** — même pin que BT7, utilisé par IR remote pour valider le mdp
- **KEY_BACKSPACE = 14** — même pin que BT8, utilisé par IR remote pour effacer/annuler

**Affichage 7-seg** : BT3/BT4 verrouillent l'affichage (ne se reset pas) jusqu'à qu'un autre bouton soit pressé. La télémétrie (vitesse) reprend ensuite automatiquement.

Joystick physique (pins partagés avec COL, mode switché) :
- **pin37 (UP)** → envoie `LEFT\n` au dashboard (tilt gauche)
- **pin35 (RIGHT)** → envoie `RIGHT\n` au dashboard (tilt droite)
- **pin33 (DOWN)** → envoie `DOWN\n`
- **pin22 (LEFT)** → envoie `LEFT\n`

---

## Commandes `controle_fusee_data` (injecteur VM)

```
help        Liste toutes les commandes
LAUNCH      Démarrer la simulation de vol
GEN 0|1     Désactiver/activer le générateur automatique
log 0|1     Activer/désactiver l'affichage [GEN] stats (off par défaut)
SET <champ> <val>   Injecter valeur brute (ALTITUDE, SPEED, FUEL, TEMP, PRESSURE, THRUST, STRESS)
fault1      Panne température (SET TEMP 420 → EVENT PROBLEM1) — résolution : BT7
fault2      Panne stress (SET STRESS 90 → EVENT PROBLEM2) — résolution : BT8
fault       Alias de fault2
resolve     Réduire les symptômes (SET STRESS 0 + SET TEMP 20)
quit        Quitter
```

---

## Effets des pannes sur la fusée

| Panne | Déclencheur | Effet dashboard | Résolution JoyPi | Résolution auto |
|-------|------------|-----------------|-----------------|-----------------|
| PROBLEM1 | temp > 350°C | Descente : speed -20/tick, alt -18/tick, flame=1 | BT7 → CMD REP1 | SET TEMP <350 |
| PROBLEM2 | stress > 80 | Descente : speed -20/tick, alt -18/tick, flame=1 | BT8 → CMD REP2 | SET STRESS <80 |

---

## Hardware JoyPi (pins physiques wiringPiSetupPhys)

| Composant | Pin physique | GPIO | Notes |
|-----------|-------------|------|-------|
| LED verte | 37 | GPIO 26 | Servo droit 1 — décollage, résolu |
| LED rouge | 11 | GPIO 17 | LED rouge uniquement (OUTPUT) |
| Capteur IR | 38 | GPIO 20 | Télécommande NEC — confirmé Test/led.c |
| Buzzer PWM | 12 | GPIO 18 | Melodies A/B/C + bip |
| MAX7219 matrice | 26 | SPI CE1 | Animation décollage, urgence |
| HT16K33 7-seg | I2C 0x70 | — | Affichage altitude/vitesse/temp — mapping XOR ^2 |
| MCP23017 LCD | I2C 0x21 | — | Init HD44780 8 commandes |
| Clavier ROW | 13, 15, 29, 31 | — | 4 lignes |
| Clavier COL | 37, 22, 35, 33 | — | Partagés avec joystick |

**Pas de conflit PIN** : IR est sur pin 38 (GPIO20), LED rouge sur pin 11 (GPIO17) — pins distincts.

---

## IR télécommande (saisie mot de passe)

- Fichier : `JoyPi/ir_input.c` — décodeur NEC par edge-detection (delta entre fronts, comme Test/led.c)
- Pin physique **38** (GPIO20 BCM) — confirmé fonctionnel sur matériel
- Fenêtre poll : **80ms** par appel → couvre une trame NEC complète (~86ms)
- En MODE_PASSWORD : usleep sauté (ir_poll déjà bloquant 80ms)
- Codes confirmés : 0→0xFF6897, 1→0xFF30CF, 2→0xFF18E7, 3→0xFF7A85, OK→0xFF02FD, *→0xFF22DD

---

## Buzzers

| Événement | Mélodie | LED |
|-----------|---------|-----|
| LAUNCH OK | Mélodie A | Verte clignote |
| LAND OK | Mélodie B | Rouge steady |
| LANDED | Bip | Tout éteint |
| PROBLEM / PROBLEM1 | Mélodie C | Rouge steady |
| PROBLEM2 | Mélodie C + Bip | Rouge steady |
| RESOLVED / RESOLVED1 / RESOLVED2 | Bip | Verte |
| MEL 1/2/3 | Mélodie A/B/C | — |
| Erreur cmd | Bip | Rouge clignote |

---

## Procédure de démarrage et test

### 1. Sur la VM
```bash
cd ~/Desktop/To-VM/bin-util
./satellite_server &          # laisser tourner en arrière-plan
./controle_fusee_data          # injecteur (commandes interactives)
# (optionnel) ./controle_fusee  # dashboard ncurses local
```

### 2. Sur le JoyPi
```bash
cd ~/rocket
./controle_fusee &             # dashboard ncurses (terminal 1)
./joypi_controller 192.168.1.25  # contrôleur GPIO (terminal 2)
```

### 3. Deux voies de lancement

**Voie A — télécommande IR ou clavier matriciel JoyPi :**
1. BT1 (key1, pin33 row0) → `joypi_controller` passe en `MODE_PASSWORD` et écrit `LAUNCH\n` au pipe → popup étoiles dans `controle_fusee`
2. Saisir `1`, `2`, `3` sur télécommande IR (code NEC)
3. Appuyer KEY_CONFIRM (key13) → `joypi_controller` envoie `CMD LU` au satellite, puis écrit `LAUNCH_OK\n` + `SIM_FLIGHT ON\n` au pipe

**Voie B — clavier USB connecté au JoyPi :**
1. BT1 (key1) → popup étoiles apparaît dans `controle_fusee` (ncurses)
2. Taper `1`, `2`, `3` sur le clavier USB + Entrée
3. `controle_fusee` valide le mot de passe, écrit `LAUNCH_AUTH_OK\n` sur `/tmp/rocket_auth.pipe`
4. `joypi_controller` lit le pipe → envoie `CMD LU` au satellite → `LAUNCH_OK\n` au dashboard

### 4. Séquence de test complète

```
1.  Vérifier que satellite_server et controle_fusee_data tournent sur VM (192.168.1.25)
2.  Lancer dashboard + controller sur JoyPi
3.  BT1 (key1, pin33) → popup étoiles dans controle_fusee
        Voie A : saisir 123 sur télécommande IR + KEY_CONFIRM (key13)
        Voie B : saisir 123 sur clavier USB dans controle_fusee + Entrée
        → LED verte clignote, matrice animation, buzzer mélodie A
4.  Attendre 2s → vérifier télémétrie sur 7-seg (vitesse)
5.  BT3 (key5, pin33 row1) → altitude sur 7-seg
6.  BT4 (key6, pin35 row1) → température sur 7-seg
7.  Joystick UP (pin37) → fusée va à gauche dans dashboard ncurses
8.  Joystick RIGHT (pin35) → fusée dérive à droite

--- Test panne 1 (température) ---
9.  Taper "fault1" dans controle_fusee_data
        → EVENT PROBLEM1 → LED rouge, mélodie C, fusée descend dans dashboard
10. BT7 (touch 15) → CMD REP1 → LED verte, bip, fusée remonte

--- Test panne 2 (stress structurel) ---
11. Taper "fault2" dans controle_fusee_data
        → EVENT PROBLEM2 → LED rouge, mélodie C + bip extra, fusée descend
12. BT8 (touch 16) → CMD REP2 → LED verte, bip

--- Test atterrissage ---
13. BT2 (touch 4) → atterrissage d'urgence → mélodie B, LED rouge
14. Attendre touchdown → EVENT LANDED → tout éteint

--- Test melodies ---
15. BT6 (touch 12) × 3 → mélodie 1, 2, 3
```

### 5. Test hardware seul (sans serveur)
```bash
cd ~/Desktop/To-JoyPI/bin-proto
./hardware_actuators_test -r   # LED verte/rouge alternées + buzzer
./moteur_test                  # test servos
./led_test                     # test LED isolé
```

---

## Structure des fichiers

```
Projet_OBJ/
├── Network/
│   ├── satellite_server.c      (~349 lignes) — serveur TCP
│   ├── satellite_handler.c     (~350 lignes) — dispatch CONTROLLER/INJECTOR
│   ├── satellite_handler.h
│   └── satellite_protocol.h   — types partagés, seuils, SatTelemetry
├── Dashboard/
│   ├── data_input_text.c       (~388 lignes) — injecteur interactif
│   ├── data_gen_model.c        (~143 lignes) — modèle physique
│   ├── dashboard_logic.c       (~275 lignes)
│   ├── dashboard_dynamics.c    (~200 lignes) — physique + descente pannes
│   ├── dashboard_visuals.c     (~253 lignes)
│   ├── dashboard_common.c
│   └── main.c
├── JoyPi/
│   ├── joypi_controller.c      (~141 lignes) — main + GPIO setup
│   ├── joypi_ctrl_net.c        (~390 lignes) — réseau + events + télémetrie
│   ├── joypi_ctrl_keys.c       (~160 lignes) — scan GPIO + IR
│   ├── joypi_ctrl_actions.c    (~340 lignes) — BT1..8 + mot de passe
│   ├── joypi_controller.h      — ControllerState, constantes, KEY_BT*
│   ├── ir_input.c              — décodeur NEC non-bloquant
│   ├── ir_input.h
│   ├── actuators.c             (~206 lignes) — LED, buzzer
│   └── actuators_display.c     (~282 lignes) — MAX7219, HT16K33, LCD
├── To-VM/bin-util/             — binaires VM prêts à l'emploi
├── To-JoyPI/bin-util/          — binaires JoyPi prêts à l'emploi
└── Makefile
```

---

## Contraintes projet

- Tous les fichiers `.c` < 400 lignes
- Cross-compilation ARM : `arm-linux-gnueabihf-gcc`
- Flags : `-Wall -Wextra -std=gnu99` — 0 warning toléré
- wiringPi compilé statiquement pour le JoyPi
- Pas de threads — architecture select() / poll non-bloquant
