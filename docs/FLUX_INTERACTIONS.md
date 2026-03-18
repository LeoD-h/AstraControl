# Flux d'interactions — Chaque bouton JoyPi en détail

> Version 1.0 — Mars 2026
> Auteurs : Léo, Inès, Juliann

---

## Vue d'ensemble du système

```
   ┌──────────────┐     GPIO      ┌─────────────────────┐
   │  Clavier 4x4 │──────────────►│                     │
   │  (matrice)   │               │  joypi_controller   │
   └──────────────┘               │  (ARM, wiringPi)    │
                                  │                     │
   ┌──────────────┐               │  ┌───────────────┐  │
   │  LED pin37   │◄──────────────┤  │ Actuateurs :  │  │
   │  Buzzer pin12│               │  │  LED          │  │
   │  MAX7219 SPI │               │  │  Buzzer       │  │
   │  HT16K33 I2C │               │  │  Matrice 8x8  │  │
   │  LCD MCP I2C │               │  │  7-segments   │  │
   └──────────────┘               │  │  LCD 16x2     │  │
                                  │  └───────────────┘  │
                                  │          │           │
                                  │    TCP :5555         │
                                  │          │           │
                 FIFO pipes        │          ▼           │
  ┌────────────────────┐          │  satellite_server    │
  │  controle_fusee    │◄─────────┤  (VM 192.168.64.7)  │
  │  (dashboard ncurses│ cmd.pipe │          │           │
  │   sur JoyPi)       │◄─────────┤  controle_fusee_data│
  └────────────────────┘ data.pipe│  (INJECTOR physique) │
                                  └─────────────────────┘
```

---

## Mapping des touches du clavier matriciel 4x4

Le clavier est câblé sur :
- Rangées (ROW) : pins physiques 13, 15, 29, 31
- Colonnes (COL) : pins physiques 33, 35, 37, 22

La numérotation logique (key_num) est calculée : `(rangée × 4) + (4 - colonne)`

| key_num | Action en mode NORMAL          |
|---------|-------------------------------|
| **1**   | BT1 — Entrer mode mot de passe → lancement |
| **2**   | BT2 — Atterrissage d'urgence    |
| **3**   | BT3 — Demande altitude          |
| **4**   | BT4 — Demande température       |
| **5**   | BT5 — Cycle pression (3 états)  |
| **6**   | BT6 — Test mélodie (cycle 1→2→3) |
| 7–16    | Ignorées en mode normal         |

En mode `MODE_PASSWORD` (saisie du mot de passe) :
| key_num | Action                     |
|---------|---------------------------|
| 1–9     | Saisir chiffre 1–9        |
| 10      | Saisir chiffre 0           |
| 11      | Valider (CONFIRM)          |
| 12      | Effacer dernier (BACKSPACE) |
| 13      | Annuler (CANCEL)            |

---

## BT1 — Lancement (mot de passe requis)

### Diagramme de séquence

```
JoyPi (bouton physique)          joypi_controller       satellite_server      controle_fusee
        │                              │                       │                    │
   BT1 appuyé                         │                       │                    │
        │──────────GPIO────────────────►│                       │                    │
        │                          MODE_PASSWORD               │                    │
        │                          LCD: "MOT DE PASSE:"        │                    │
        │                              │                       │                    │
   Saisie chiffres via keypad          │                       │                    │
   (étoiles affichées sur LCD)        │                       │                    │
        │                              │                       │                    │
   Touche CONFIRM (key 11)             │                       │                    │
        │──────────GPIO────────────────►│                       │                    │
        │                         compare "123"                │                    │
        │                              │                       │                    │
        │              [Si correct]    │                       │                    │
        │                          "CMD LU\n"─────TCP─────────►│                    │
        │                              │                 vérifie état=READY         │
        │                              │                  state → FLYING            │
        │                              │◄────────"OK LAUNCH"───│                    │
        │                              │◄────────"EVENT LAUNCH"│                    │
        │                              │                 ──────"CMD_EVENT LAUNCH"──►│(injector)
        │                              │                       │                    │
        │                    [Actuateurs JoyPi]               │                    │
        │                    LED allumée (vert)               │                    │
        │                    Matrice: animation fusée         │                    │
        │                    Buzzer: mélodie A (décollage)    │                    │
        │                              │                       │                    │
        │                    [Pipes dashboard]                 │                    │
        │                    cmd.pipe← "LAUNCH_OK\n"          │─────────────────────►│
        │                    cmd.pipe← "SIM_FLIGHT ON\n"      │─────────────────────►│
        │                              │                       │                    │
        │              [Si incorrect]  │                       │                    │
        │                    LED rouge (clignotement)         │                    │
        │                    Buzzer: bip court                │                    │
        │                    LCD: "MOT DE PASSE INCORRECT"    │                    │
        │                    reste en MODE_PASSWORD           │                    │
```

### Résumé étape par étape

1. **Appui BT1** → `joypi_controller` passe en `MODE_PASSWORD`
2. **LCD** affiche `"MOT DE PASSE:"` sur la ligne 1
3. **Saisie chiffres** (touches 1–9, 10=zéro) → étoiles `***` sur LCD ligne 2
4. **Validation** (touche 11) → compare avec `"123"` (mot de passe hardcodé)
5. **Si correct** :
   - Envoi `CMD LU\n` au satellite via TCP
   - Satellite répond `OK LAUNCH\n` + broadcast `EVENT LAUNCH\n`
   - Satellite passe en état `FLYING`
   - Satellite notifie l'injecteur `CMD_EVENT LAUNCH`
   - JoyPi : LED verte, matrice animation, buzzer mélodie A
   - Pipe cmd → `LAUNCH_OK\n` + `SIM_FLIGHT ON\n` → dashboard s'anime
6. **Si incorrect** : bip d'erreur, LED rouge, LCD "INCORRECT", re-saisie possible

---

## BT2 — Atterrissage d'urgence

### Diagramme de séquence

```
JoyPi (bouton physique)          joypi_controller       satellite_server      injector
        │                              │                       │                 │
   BT2 appuyé                         │                       │                 │
        │──────────GPIO────────────────►│                       │                 │
        │                          "CMD LD\n"─────TCP─────────►│                 │
        │                              │                 vérifie état=FLYING     │
        │                              │                  state → EMERGENCY      │
        │                              │◄────────"OK LAND"─────│                 │
        │                              │◄────────"EVENT LAND"──│                 │
        │                              │               ────────"CMD_EVENT LAND"─►│
        │                              │               (injector: gm.landing=true)│
        │                              │                       │                 │
        │                    [Actuateurs JoyPi]               │                 │
        │                    LED rouge (clignotements)        │                 │
        │                    Matrice: pattern X clignotant    │                 │
        │                    Buzzer: mélodie B (alarme)       │                 │
        │                              │                       │                 │
        │                    [Pipe dashboard]                  │                 │
        │                    cmd.pipe← "LAND\n"               │──────────────────►│
        │                              │                       │                 │
        │                    [Auto - quand altitude=0]         │                 │
        │                              │               state → READY             │
        │                              │◄────────"EVENT LANDED"─│                │
        │                              │               ────────"CMD_EVENT RESUME"►│
        │                    LED OFF                          │                 │
        │                    Matrice: clear                   │                 │
        │                    LCD: "POSEE / AU SOL"            │                 │
        │                    cmd.pipe← "RESUME\n"             │──────────────────►│
```

### Résumé étape par étape

1. **Appui BT2** → `CMD LD\n` envoyé au satellite
2. **Satellite** : état → `EMERGENCY`, broadcast `EVENT LAND` aux controllers, `CMD_EVENT LAND` à l'injector
3. **Injector** : `gm.landing = true` → commence à décendre (speed -80/s)
4. **JoyPi** : LED rouge clignote, matrice X, buzzer mélodie urgence B
5. **Dashboard** reçoit `LAND\n` sur pipe → affiche phase d'atterrissage
6. **Quand altitude=0** (détecté par satellite) : état → `READY`, broadcast `EVENT LANDED` + `CMD_EVENT RESUME`
7. **JoyPi** reçoit `EVENT LANDED` en push → LED off, matrice clear, LCD "POSEE AU SOL"
8. **Dashboard** reçoit `RESUME\n` → animation s'arrête

---

## BT3 — Demande altitude

### Diagramme de séquence

```
JoyPi (bouton physique)          joypi_controller       satellite_server
        │                              │                       │
   BT3 appuyé                         │                       │
        │──────────GPIO────────────────►│                       │
        │                          "CMD ALT\n"─────TCP────────►│
        │                              │               lit g_telem.altitude      │
        │                              │◄────────"DATA ALT 15230"──│
        │                              │                       │
        │                    [Actuateurs JoyPi]               │
        │                    7-segments: affiche 15230        │
        │                    console: "[ctrl] ALTITUDE: 15230 m"
```

### Résumé étape par étape

1. **Appui BT3** → `CMD ALT\n` envoyé au satellite
2. **Satellite** répond `DATA ALT <valeur>` (altitude courante en mètres)
3. **Afficheur 7-segments HT16K33** affiche la valeur (4 chiffres max : 0–9999 m)
4. **Console** log l'altitude

> Note : l'altitude réelle injectée par controle_fusee_data peut dépasser 9999 m. L'afficheur est limité à 4 chiffres (modulo 10000).

---

## BT4 — Demande température

### Diagramme de séquence

```
JoyPi (bouton physique)          joypi_controller       satellite_server
        │                              │                       │
   BT4 appuyé                         │                       │
        │──────────GPIO────────────────►│                       │
        │                          "CMD TEMP\n"────TCP────────►│
        │                              │◄────────"DATA TEMP 347"─│
        │                              │                       │
        │                    7-segments: affiche 347           │
        │                    Si val > 350 :                    │
        │                      Buzzer: bip d'alerte           │
        │                      LCD: "TEMP CRITIQUE! / T=347C"  │
```

### Résumé étape par étape

1. **Appui BT4** → `CMD TEMP\n` envoyé au satellite
2. **Satellite** répond `DATA TEMP <valeur>` (température en °C)
3. **Afficheur 7-segments** affiche la température
4. **Si température > 350°C** : bip buzzer + LCD alerte thermique critique

---

## BT5 — Cycle pression (3 états)

Le bouton BT5 cycle à travers 3 états successifs :

```
État 0 (nominal) → État 1 (panne) → État 2 (correcteur) → État 0 (nominal)
```

### État 1 — Panne pression

```
JoyPi                 joypi_controller       satellite_server        Tous controllers
   │                        │                       │                      │
BT5 (état 0→1)              │                       │                      │
   │──GPIO──────────────────►│                       │                      │
   │                    "CMD PRES\n"─────TCP─────────►│                      │
   │                         │               pressure_fault=true             │
   │                         │◄──"OK PRES_FAULT"─────│                      │
   │                         │                 ──────"EVENT PROBLEM"────────►│
   │                         │               ──data_pipe──"PROBLEM ON"──────►│(dashboard)
   │                 LED rouge clignotement          │                      │
   │                 LCD: "PRESSION: PANNE"          │                      │
   │                 LCD: "BT5 = CORRIGER"           │                      │
   │                 Buzzer: mélodie C (problème)    │                      │
   │                 data.pipe← "PROBLEM ON\n"       │──────────────────────►│
```

### État 2 — Correcteur activé

```
BT5 (état 1→2)              │                       │
   │──GPIO──────────────────►│                       │
   │                    "CMD PRES\n"─────TCP─────────►│
   │                         │               pressure_corrector=true          │
   │                         │◄──"OK PRES_FIX"───────│                      │
   │                         │                 ──────"EVENT RESOLVED"────────►│
   │                 LCD: "CORRECTEUR / ACTIVE"       │                      │
   │                 Buzzer: bip court                │                      │
   │                 cmd.pipe← "FIX_PROBLEM\n"        │──────────────────────►│
   │                 data.pipe← "PROBLEM OFF\n"       │──────────────────────►│
```

### État 3 — Retour nominal

```
BT5 (état 2→3)              │                       │
   │──GPIO──────────────────►│                       │
   │                    "CMD PRES\n"─────TCP─────────►│
   │                         │               fault=false, corrector=false     │
   │                         │◄──"OK PRES_OK"────────│
   │                 LED verte (OK)                  │
   │                 LCD: "PRESSION / NOMINALE"       │
   │                 cmd.pipe← "CLEAR_ALERTS\n"       │──────────────────────►│
```

---

## BT6 — Test mélodie (cycle 3 mélodies)

```
JoyPi                 joypi_controller       satellite_server        Tous controllers
   │                        │                       │                      │
BT6 appuyé (cycle idx++)    │                       │                      │
   │──GPIO──────────────────►│                       │                      │
   │                    "CMD MEL 1\n"───TCP──────────►│                      │
   │                         │                 valide n ∈ {1,2,3}           │
   │                         │◄──"OK MEL 1"──────────│                      │
   │                         │                 ──────"EVENT MEL 1"──────────►│(tous controllers)
   │                 Buzzer: mélodie A (C5-E5-G5-C6) │                      │
   │                         │                       │                      │
   │                    [Appuis suivants : MEL 2, MEL 3, MEL 1…]
```

### Les 3 mélodies

| Mélodie | Nom       | Notes                          | Usage              |
|---------|-----------|--------------------------------|--------------------|
| **A**   | Décollage | C5 E5 G5 C6 B5 C6 (festive)   | Lancement réussi   |
| **B**   | Urgence   | 5×880Hz + 660-440-220 (alarme) | Atterrissage urgence |
| **C**   | Problème  | 2×440Hz + 330Hz (grave)        | Anomalie détectée  |

---

## Événements PUSH automatiques du satellite

Le satellite envoie automatiquement des données sans que le JoyPi appuie sur un bouton :

### TELEMETRY (toutes les 2 secondes)

```
satellite_server ──────TCP PUSH──────► joypi_controller
"TELEMETRY alt=15230 speed=2800 fuel=78 temp=347 pressure=215 thrust=1450 state=FLYING"
                                              │
                                     handle_telemetry()
                                              │
                                    data.pipe→ "SET ALTITUDE 15230\n"
                                    data.pipe→ "SET SPEED 2800\n"
                                    data.pipe→ "SET FUEL 78\n"
                                    data.pipe→ "SET TEMP 347\n"
                                    data.pipe→ "SET PRESSURE 215\n"
                                    data.pipe→ "SET THRUST 1450\n"
                                              │
                                    controle_fusee (dashboard) met à jour l'affichage
```

### EVENT PROBLEM (quand température critique ou stress élevé)

```
satellite_server ──"EVENT PROBLEM"──► joypi_controller
                                              │
                                    LED rouge (clignotement)
                                    LCD: "PROBLEME / DETECTE!"
                                    Buzzer: mélodie C
                                    data.pipe← "PROBLEM ON\n"
```

### EVENT LANDED (quand altitude = 0 après landing)

```
satellite_server ──"EVENT LANDED"──► joypi_controller
                                              │
                                    LED OFF
                                    Matrice: clear
                                    LCD: "POSEE / AU SOL"
                                    cmd.pipe← "RESUME\n"
```

### EVENT LAND_AUTO (carburant épuisé en vol)

```
satellite_server ──"EVENT LAND_AUTO"──► joypi_controller
                                               │
                                     cmd.pipe← "LAND\n"
                                     (dashboard passe en mode atterrissage)
```

---

## Flux complet d'une mission type

```
Étape 1 : Prêt
  Système connecté, état READY
  Dashboard affiche fusée au sol
  LCD: "SYSTEME PRET"

Étape 2 : Lancement
  BT1 → saisie mot de passe "123" → CMD LU
  Satellite: READY → FLYING
  JoyPi: LED verte, matrice animation, buzzer mélodie A
  Dashboard: fusée qui monte, flammes animées
  Injector: physique en mode ascension (vitesse++, carburant--)

Étape 3 : En vol (automatique)
  Injector envoie SET ALTITUDE/SPEED/FUEL… toutes les 700ms
  Satellite push TELEMETRY toutes les 2s vers JoyPi
  JoyPi forward vers dashboard via data.pipe
  Dashboard animé en temps réel

Étape 4a : Atterrissage manuel
  BT2 → CMD LD
  Satellite: FLYING → EMERGENCY
  JoyPi: LED rouge, matrice X, buzzer mélodie B
  Injector: descente forcée
  Satellite détecte altitude=0 → READY, EVENT LANDED
  JoyPi: LED off, LCD "POSEE"

Étape 4b : Atterrissage automatique (fuel=0)
  Injector: fuel=0, landing=true
  Satellite détecte fuel=0 → LANDING auto, EVENT LAND_AUTO
  JoyPi: reçoit EVENT LAND_AUTO → dashboard en mode landing
  Même fin que 4a

Étape 5 : Problème en vol
  Injector: température > 380°C ou stress > 100
  Satellite: EVENT PROBLEM → JoyPi + dashboard
  JoyPi: LED rouge, buzzer mélodie C, LCD "PROBLEME DETECTE!"
  BT5 (état 1) → signaler panne pression
  BT5 (état 2) → activer correcteur → FIX_PROBLEM
  BT5 (état 3) → retour nominal → CLEAR_ALERTS

Étape 6 : Reset pour relance
  FIX_PROBLEM → dashboard reset état initial
  État retourne READY, nouvelle mission possible
```

---

## Résumé des pipes FIFO

| Pipe                   | Direction              | Messages envoyés par              | Reçus par        |
|------------------------|------------------------|-----------------------------------|------------------|
| `/tmp/rocket_cmd.pipe` | joypi_controller → dashboard | LAUNCH_OK, LAND, RESUME, FIX_PROBLEM, CLEAR_ALERTS, SIM_FLIGHT ON/OFF | controle_fusee (apply_cmd) |
| `/tmp/rocket_data.pipe`| joypi_controller → dashboard | SET ALTITUDE/SPEED/FUEL/TEMP/PRESSURE/THRUST v, PROBLEM ON/OFF | controle_fusee (apply_data) |

---

## Résumé du protocole TCP satellite

| Commande (→ satellite)  | Réponse satellite      | Broadcast controllers           |
|-------------------------|------------------------|---------------------------------|
| `AUTH CONTROLLER\n`     | `AUTH_OK\n`            | —                               |
| `CMD LU\n`              | `OK LAUNCH\n`          | `EVENT LAUNCH\n`                |
| `CMD LD\n`              | `OK LAND\n`            | `EVENT LAND\n`                  |
| `CMD ALT\n`             | `DATA ALT <v>\n`       | —                               |
| `CMD TEMP\n`            | `DATA TEMP <v>\n`      | —                               |
| `CMD PRES\n` (panne)    | `OK PRES_FAULT\n`      | `EVENT PROBLEM\n`               |
| `CMD PRES\n` (correcteur)| `OK PRES_FIX\n`       | `EVENT RESOLVED\n`              |
| `CMD PRES\n` (nominal)  | `OK PRES_OK\n`         | —                               |
| `CMD MEL N\n`           | `OK MEL N\n`           | `EVENT MEL N\n`                 |

| Push satellite (automatique)  | Déclencheur                  |
|-------------------------------|------------------------------|
| `TELEMETRY alt=… speed=…\n`   | Toutes les 2s (timer)        |
| `EVENT LAND_AUTO\n`           | fuel ≤ 0 en vol              |
| `EVENT LANDED\n`              | altitude = 0 en landing      |
| `EVENT PROBLEM\n`             | temp > SAT_TEMP_CRITICAL ou stress > SAT_STRESS_CRITICAL |
