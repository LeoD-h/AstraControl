# Task List — Projet Fusée JoyPi/VM

## Statut global : VALIDÉ — BUILD 0 WARNING 0 ERREUR

---

## LIVRABLES SESSION COURANTE (18/03/2026)

### Architecture finale

```
VM (192.168.64.7:5555)
  ├── satellite_server       ← serveur TCP central (x86 + ARM RPi)
  │     ├── satellite_server.c   (~349 lignes)
  │     └── satellite_handler.c  (~318 lignes) — broadcast, push_telemetry, dispatch
  ├── controle_fusee_data    ← injecteur télémétrie (INJECTOR)
  │     ├── data_input_text.c    (~388 lignes) — connexion, boucle, stdin
  │     ├── data_gen_model.c     (~137 lignes) — modèle physique GenModel
  │     └── dashboard_common.c
  └── controle_fusee (x86)   ← dashboard ncurses natif (test local VM)

JoyPi (10.91.241.75)
  ├── joypi_controller       ← client CONTROLLER GPIO
  │     ├── joypi_controller.c   (~141 lignes) — main + GPIO setup
  │     ├── joypi_ctrl_net.c     (~368 lignes) — réseau + handle_event/telemetry
  │     ├── joypi_ctrl_keys.c    (~150 lignes) — scan GPIO / stdin
  │     ├── joypi_ctrl_actions.c (~301 lignes) — mdp + actions BT1..8 + dispatch
  │     ├── actuators.c          (~206 lignes) — LED vert/rouge, buzzer
  │     └── actuators_display.c  (~282 lignes) — MAX7219, HT16K33, LCD
  └── controle_fusee         ← dashboard ncurses ARM
        ├── dashboard_logic.c    (~275 lignes)
        ├── dashboard_dynamics.c (~189 lignes)
        ├── dashboard_visuals.c  (~253 lignes)
        ├── pipes.c
        └── main.c
        Pipes FIFO : /tmp/rocket_cmd.pipe + /tmp/rocket_data.pipe
```

**Contrainte < 400 lignes : RESPECTÉE sur tous les fichiers.**

---

## FONCTIONNALITÉS IMPLÉMENTÉES

### 1. Directions joystick → dashboard ncurses
- UP/DOWN/LEFT/RIGHT (GPIO JoyPi) → `cmd_pipe_write(st, "UP\n")` etc.
- Simulation clavier : u/d/r/l
- `apply_cmd()` dashboard reçoit UP/DOWN/LEFT/RIGHT → bouge la fusée

### 2. Test/moteur.c corrigé et compilé
- `wiringPiSetupGpio()` → `wiringPiSetupPhys()`
- LED1_PIN = 37 (LED verte, GPIO 26, servo droit 1)
- LED2_PIN = 11 (LED rouge, GPIO 17, servo droit 2) — pin 22 NON DISPONIBLE
- Compilé : `bin/bin-proto/moteur_test_joypi`

### 3. LED verte (pin 37) — décollage
- `actuator_led_green_blink(N)` : clignote N fois
- Déclenché sur : OK LAUNCH, EVENT LAUNCH, EVENT RESOLVED

### 4. LED rouge (pin 11) — atterrissage / panne
- `actuator_led_red_on()` : allume steady
- Déclenché sur : OK LAND, EVENT LAND/LAND_AUTO, EVENT PROBLEM

### 5. 2 LEDs sur servomoteurs de droite
- Servo droit 1 (pin 37) = LED VERTE (mission active, décollage)
- Servo droit 2 (pin 11) = LED ROUGE (atterrissage, panne)
- `actuator_led_all_off()` : éteint les deux

### 6. Répertoire proto/ créé
- `proto/README.md` créé

### 7. Dashboard ncurses compilé pour VM (x86)
- Binaire : `To-VM/bin-util/controle_fusee`
- Compilation : `gcc ... -lncurses`

### 8. Tâche 8 remplacée — BT8 résout la panne
- Ancien : tâche "carburant" inerte
- Nouveau : BT8 = résolution panne si `fault_active == true` (CMD PRES)
- Sinon : bip info (pas de panne active)

### 9. hardware_actuators_test mis à jour
- `test_servos()` : alterne LED verte/rouge 4 fois
- Flag `-r` (real test) : active le buzzer en mode test réel
- `test_leds()` : teste séparément LED verte puis LED rouge

### 10. Buzzers cohérents
| Événement       | Buzzer           | LED             |
|-----------------|------------------|-----------------|
| LAUNCH OK       | mélodie A        | verte clignote  |
| LAND OK         | mélodie B        | rouge steady    |
| LANDED          | bip              | tout éteint     |
| PROBLEM         | mélodie C        | rouge steady    |
| RESOLVED        | bip              | verte steady    |
| MEL 1/2/3       | mélodie A/B/C    | —               |
| Erreur cmd      | bip              | rouge clignote  |

### 11. commandes controle_fusee_data
- `help`    : liste toutes les commandes
- `fault`   : injecte SET STRESS 90 → EVENT PROBLEM (si FLYING)
- `resolve` : réduit symptômes (SET STRESS 0 + SET TEMP 20)
- `LAUNCH`, `GEN 0|1`, `SET <champ> <v>`, `quit`

---

## FICHIERS CRÉÉS / MODIFIÉS

### Créés
- `proto/README.md`
- `JoyPi/joypi_ctrl_actions.h` + `joypi_ctrl_actions.c` (301 lignes)
- `JoyPi/joypi_ctrl_net.h` + `joypi_ctrl_net.c` (368 lignes)
- `JoyPi/joypi_ctrl_keys.h`
- `JoyPi/joypi_controller.h`
- `JoyPi/actuators_display.c` (split de actuators.c)
- `JoyPi/joypi_ir_test.c` (IR NEC, non intégré)
- `JoyPi/joypi_hardware_button.c` + `.h`
- `Dashboard/dashboard_dynamics.c` (split de dashboard_logic.c)
- `Dashboard/data_gen_model.h` + `data_gen_model.c` (137 lignes)
- `Network/satellite_handler.h` + `satellite_handler.c` (318 lignes)
- `touches.txt` — mapping complet 8 tâches + mdp + IR
- `task.md` — ce fichier

### Modifiés
- `JoyPi/joypi_controller.c` → allégé (141 lignes, main + GPIO)
- `JoyPi/joypi_ctrl_keys.c` → scan seul (150 lignes)
- `JoyPi/actuators.c` → 2 LEDs couleur + buzzers (206 lignes)
- `JoyPi/actuators.h` → nouvelles API LED vert/rouge
- `JoyPi/hardware_actuators_test.c` → servos + flag -r (303 lignes)
- `Dashboard/dashboard_logic.c` → split (275 lignes)
- `Dashboard/data_input_text.c` → split (388 lignes)
- `Network/satellite_server.c` → split (349 lignes)
- `Test/moteur.c` → wiringPiSetupPhys + LED2 pin 11
- `Makefile` → toutes les nouvelles cibles

---

## STATUT INFRAROUGE

**Non intégré dans cette session.**
- `JoyPi/joypi_ir_test.c` : test standalone IR NEC (simulation + hardware)
- `JoyPi/joypi_hardware_button.c` : API hw_button_init/scan
- Non connectés au flux contrôleur principal
- Restent à calibrer (codes IR réels) et intégrer (ir_input.c)

---

## RESTE À FAIRE

### Priorité HAUTE
- [ ] **Intégrer IR** : connecter joypi_ir_test.c au flux mot de passe
  - `ir_poll()` non-bloquant → `handle_key_password()` en mode PASSWORD
  - Créer `JoyPi/ir_input.c`
- [ ] **Calibrer codes IR** : lancer `joypi_ir_test` sur JoyPi, noter vrais codes

### Priorité MOYENNE
- [ ] **Déployer et tester** : `make sjoypi` puis séquence de test complète
- [ ] **Vérifier LCD** : connecté physiquement ? réactiver ou confirmer désactivé
- [ ] **Tâche 7 (BT7)** : envisager CMD SPEED dédié (option A) ou garder télémétrie

### Priorité BASSE
- [ ] **Documenter codes IR réels** dans touches.txt après calibration

---

## PROCÉDURE DE TEST

### Sur VM (192.168.64.7)
```bash
cd ~/Desktop/To-VM/bin-util
./satellite_server &
./controle_fusee_data
# ou dashboard local :
./controle_fusee
```

### Sur JoyPi (10.91.241.75)
```bash
cd ~/Desktop/To-JoyPI
./run_controle_fusee.sh &          # dashboard ncurses
./run_controller.sh 192.168.64.7   # contrôleur
```

### Séquence de test complète
1. Démarrer `satellite_server` sur VM
2. Démarrer `controle_fusee_data` sur VM
3. Démarrer `controle_fusee` (dashboard) sur JoyPi
4. Démarrer `joypi_controller` sur JoyPi
5. Appuyer BT1 (touch 3) → entrer "123" → touche 13 (CONF)
6. Vérifier : LED verte clignote puis steady, matrice animation, buzzer mélodie A
7. Attendre 2s → vérifier 7-seg vitesse en temps réel (télémétrie)
8. Appuyer BT3 (touch 7) → altitude sur 7-seg
9. Joystick UP → fusée monte dans dashboard ncurses
10. Joystick RIGHT → fusée dérive à droite
11. `fault` dans controle_fusee_data → EVENT PROBLEM → LED rouge + mélodie C
12. BT8 (touch 16) → CMD PRES résolution → LED verte + bip
13. BT2 (touch 4) → atterrissage urgence → LED rouge + mélodie B

### Test moteur/servos
```bash
cd ~/Desktop/To-JoyPI/bin-proto
./moteur_test           # test LED verte + rouge alternées
./hardware_actuators_test -r  # test réel avec buzzer
```

---

## BUILD

```
make all   → 0 warning, 0 erreur (validé 18/03/2026)
make vm    → To-VM/  (5 binaires : satellite x86+ARM, data x86+ARM, dashboard x86)
make joypi → To-JoyPI/ (dashboard ARM, controller ARM, tests)
```
