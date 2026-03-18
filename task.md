# Task List — Projet Fusée JoyPi/VM

## Statut global : PRÊT A TESTER

---

## CORRECTIONS APPLIQUÉES (session 18/03/2026)

### Bug #1 CRITIQUE — Compilation impossible [CORRIGÉ]
- **Problème** : `ControllerState` manquait les champs `mode`, `password_buf`, `password_len`
- **Problème** : Constantes `MODE_NORMAL`, `MODE_PASSWORD`, `PASSWORD_MAX_LEN`, `PASSWORD_CORRECT`, `KEY_CONFIRM`, `KEY_BACKSPACE` non définies
- **Correction** : Ajout des champs dans la struct + defines en tête de `joypi_controller.c`

### Bug #2 CRITIQUE — `auth_pipe_fd` initialisé à 0 (stdin) [CORRIGÉ]
- **Fichier** : `joypi_controller.c:state_init()`
- **Correction** : `st->auth_pipe_fd = -1;` ajouté dans `state_init()`

### Bug #3 FONCTIONNEL — Seulement 6 tâches, touches 15/16 non assignées [CORRIGÉ]
- **Correction** : Ajout `KEY_BT7=15` (VITESSE) et `KEY_BT8=16` (CARBURANT)
- **Correction** : `action_bt7()` et `action_bt8()` implémentées
- **Correction** : Mode simulation clavier étendu aux touches '7' et '8'

### Bug #4 FONCTIONNEL — 7-seg n'affichait pas la vitesse en temps réel [CORRIGÉ]
- **Fichier** : `joypi_controller.c:handle_telemetry()`
- **Correction** : `actuator_segment_show(atoi(speed))` appelé à chaque télémétrie (2s)

### Bug #5 DESIGN — LCD appelé partout malgré consigne "pas de LCD" [CORRIGÉ]
- **Fichier** : `joypi_controller.c` (6 appels `actuator_lcd_show` supprimés)
- **Correction** : Tous les appels `actuator_lcd_show()` remplacés par `printf()` ou supprimés
- **Note** : `actuator_lcd_show()` existe toujours dans `actuators.c`, disponible si besoin futur

---

## FICHIERS CRÉÉS

- [x] `touches.txt` — mapping complet de toutes les touches (8 tâches + mdp + IR)
- [x] `JoyPi/joypi_ir_test.c` — test du capteur infrarouge (décodage NEC, simulation)
- [x] `JoyPi/joypi_hardware_button.c` — module encapsulé de lecture hardware buttons
- [x] `JoyPi/joypi_hardware_button.h` — en-tête API hw_button_init / hw_button_scan
- [x] `task.md` — ce fichier

---

## RESTE À FAIRE

### Priorité HAUTE

- [ ] **Intégrer IR dans joypi_controller** : connecter `joypi_ir_test.c` au flux mot de passe
  - Ajouter une goroutine/thread ou une fonction `ir_poll()` non-bloquante
  - En mode `MODE_PASSWORD`, les codes IR chiffres alimentent `password_add_char()`
  - Code IR OK → `password_confirm()`, code IR `*` → `password_backspace()`
  - Fichier à modifier : `joypi_controller.c` + créer `JoyPi/ir_input.c`

- [ ] **Calibrer les codes IR** : les codes dans `joypi_ir_test.c` sont des valeurs par défaut
  - Lancer `joypi_ir_test` sur le JoyPi et noter les codes réels pour chaque touche
  - Mettre à jour les defines `IR_KEY_*` avec les vraies valeurs

- [ ] **Tâche 7 (touche 15)** : implémenter un vrai `CMD SPEED` dans le protocole satellite
  - Actuellement action_bt7 affiche un message console sans CMD dédié
  - Option A : ajouter `CMD SPEED` dans `satellite_server.c` → `DATA SPEED <v>`
  - Option B : conserver l'affichage temps réel via télémétrie (déjà fonctionnel)

- [ ] **Tâche 8 (touche 16)** : implémenter `CMD FUEL` dans le protocole satellite
  - Idem : ajouter `CMD FUEL` ou utiliser la valeur télémétrie

### Priorité MOYENNE

- [ ] **Recompiler** : `make all` après corrections (`joypi_controller.c` modifié)
  - Vérifier 0 warning, 0 erreur
  - Redéployer sur JoyPi : `make sjoypi`

- [ ] **Tester en simulation** : lancer `joypi_controller` sans `USE_WIRINGPI`
  - Vérifier que les touches 1-8 sont reconnues en mode simulation
  - Tester le flux complet : connexion → auth → CMD LU → EVENT LAUNCH → télémétrie

- [ ] **Ajouter les cibles IR au Makefile** : `joypi_ir_test` et `joypi_hardware_button`
  - Ajouter des cibles `ir_test_joypi` et `hw_button_joypi` dans le Makefile

- [ ] **Vérifier le LCD sur JoyPi** : si le LCD est physiquement connecté et fonctionnel,
  décider définitivement si on le réactive ou pas

### Priorité BASSE

- [ ] **Dashboard `FIX_PROBLEM`** : appel de `reset_for_relaunch()` trop agressif
  - Corriger pression ≠ réinitialiser la mission entière
  - Suggestion : retirer l'appel à `reset_for_relaunch()` dans `apply_cmd()`

- [ ] **Dashboard affiche la vitesse** : le 7-seg physique affiche la vitesse en temps réel,
  mais le dashboard ncurses ne la met pas explicitement en valeur centrale
  - Cosmétique : pas critique pour le test

- [ ] **Documenter les codes IR réels** dans `touches.txt` après calibration

---

## PROCÉDURE DE TEST

### Sur VM (192.168.64.7)
```bash
cd ~/Desktop/To-VM/bin-util
./satellite_server &
./controle_fusee_data
```

### Sur JoyPi (10.91.241.75)
```bash
cd ~/Desktop/To-JoyPI
./run_controle_fusee.sh &          # dashboard ncurses
./run_controller.sh 192.168.64.7   # contrôleur
```

### Test IR (JoyPi)
```bash
cd ~/Desktop/To-JoyPI/bin-proto
./joypi_ir_test                    # à compiler et déployer (TODO)
```

### Séquence de test complète
1. Démarrer `satellite_server` sur VM
2. Démarrer `controle_fusee_data` sur VM
3. Démarrer `controle_fusee` (dashboard) sur JoyPi
4. Démarrer `joypi_controller` sur JoyPi
5. Appuyer touche 3 (BT1) → entrer mot de passe "123" → touche 13 (CONF)
6. Vérifier : LED verte, matrice animation, buzzer mélodie A, 7-seg = vitesse
7. Attendre 2s → vérifier que le 7-seg se met à jour (télémétrie)
8. Appuyer touche 7 (BT3) → vérifier altitude sur 7-seg
9. Appuyer touche 11 (BT5) → vérifier cycle pression (PANNE → FIX → NOMINAL)
10. Appuyer touche 4 (BT2) → vérifier atterrissage d'urgence

---

## ARCHITECTURE RAPPEL

```
VM (192.168.64.7:5555)
  ├── satellite_server       ← serveur TCP central
  └── controle_fusee_data    ← injecteur télémétrie (INJECTOR)

JoyPi (10.91.241.75)
  ├── joypi_controller       ← client CONTROLLER GPIO
  │     ├── Matrice 4x4 (touches 3,4,7,8,11,12,15,16)
  │     ├── 7-seg HT16K33 I2C 0x70 (vitesse en temps réel)
  │     ├── LED pin37 (état mission)
  │     ├── Buzzer pin12 PWM (mélodies)
  │     └── Matrice MAX7219 SPI CE1 (animations)
  └── controle_fusee         ← dashboard ncurses
        ├── Pipe CMD  /tmp/rocket_cmd.pipe
        └── Pipe DATA /tmp/rocket_data.pipe
```
