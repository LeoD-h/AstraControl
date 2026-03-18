# Procédure de Lancement — Projet Fusée TCP

> Version 1.0 — Mars 2026
> Auteurs : Léo, Inès, Juliann

---

## Architecture réseau

```
VM (192.168.64.7)                JoyPi (192.168.1.21)
┌────────────────────┐           ┌────────────
──────────────┐
│  satellite_server  │◄──TCP────►│   joypi_controller       │
│  :5555             │           │   (boutons GPIO)          │
│                    │           │                           │
│  controle_fusee_   │           │   controle_fusee          │
│  data (INJECTOR)   │           │   (dashboard ncurses)     │
│  ──TCP→satellite   │           │   ◄──FIFO /tmp/rocket_*  │
└────────────────────┘           └──────────────────────────┘
```

---

## Étape 0 — Prérequis (une seule fois)

### Sur la VM

```bash
# Vérifier que les binaires VM sont présents
ls ~/Desktop/To-VM/bin-util/
# satellite_server
# controle_fusee_data
chmod +x ~/Desktop/To-VM/bin-util/*
```

### Sur le JoyPi

```bash
# Dézipper le bundle JoyPi reçu par SCP
cd ~/Desktop/
unzip To-JoyPI.zip
cd To-JoyPI/

# Charger les bibliothèques (wiringPi + ncurses cross-compilés)
. ./joypi_env.sh

# Vérifier les binaires
ls bin-util/
# controle_fusee       (dashboard ncurses)
# joypi_controller     (contrôleur satellite)
chmod +x bin-util/*
```

---

## Étape 1 — Démarrage du serveur satellite (VM)

**Terminal VM #1** — Serveur TCP

```bash
cd ~/Desktop/To-VM/
./bin-util/satellite_server
```

Sortie attendue :
```
[2026-03-18 10:00:00] [INFO] satellite_server demarre sur 0.0.0.0:5555
[2026-03-18 10:00:00] [INFO] VM satellite 192.168.64.7 | JoyPi controleur 192.168.1.21
[2026-03-18 10:00:00] [INFO] Attente CONTROLLER (JoyPi) et INJECTOR (controle_fusee_data)...
```

> Le serveur reste en écoute. **Ne pas fermer ce terminal.**
> Arrêt propre : `Ctrl+C`

---

## Étape 2 — Démarrage de l'injecteur de télémétrie (VM)

**Terminal VM #2** — Injecteur physique

```bash
cd ~/Desktop/To-VM/
./bin-util/controle_fusee_data 192.168.64.7 5555
```

Sortie attendue :
```
[INJECTOR] connecte a 192.168.64.7:5555 – AUTH_OK
[INJECTOR] demarrage boucle principale
  Commandes stdin : LAUNCH | GEN 0/1 | SET XXX v | quit
```

> L'injecteur simule la physique de la fusée (altitude, vitesse, carburant…).
> Il reçoit les événements `CMD_EVENT LAUNCH/LAND/RESUME` du serveur et s'adapte.
> **Ne pas fermer ce terminal.**

---

## Étape 3 — Démarrage du dashboard ncurses (JoyPi)

**Terminal JoyPi #1** — Interface visuelle

```bash
cd ~/Desktop/To-JoyPI/
. ./joypi_env.sh        # charger les libs si pas déjà fait
./bin-util/controle_fusee
```

L'interface ncurses s'ouvre avec :
- Représentation graphique ASCII de la fusée
- Altimètre, vitesse, carburant, température, pression
- Log des événements en bas
- Le dashboard attend les données sur `/tmp/rocket_data.pipe` et `/tmp/rocket_cmd.pipe`

> **Ne pas fermer ce terminal.**
> Arrêt : `q` dans l'interface ncurses

---

## Étape 4 — Démarrage du contrôleur JoyPi (JoyPi)

**Terminal JoyPi #2** — Contrôleur satellite + actuateurs

```bash
cd ~/Desktop/To-JoyPI/
. ./joypi_env.sh        # si pas déjà chargé dans ce shell
sudo ./bin-util/joypi_controller 192.168.64.7 5555
```

> `sudo` est nécessaire pour wiringPi (accès GPIO).

Sortie attendue :
```
[ctrl] JoyPi Controller v1.0
[ctrl] Satellite : 192.168.64.7:5555
[ctrl] Mode GPIO réel (wiringPi). Rows: 13,15,29,31 | Cols: 33,35,37,22
[ctrl] Authentifié au satellite (192.168.64.7:5555)
```

> Le JoyPi est maintenant connecté. Les boutons physiques sont actifs.
> Arrêt : `Ctrl+C`

---

## Ordre de démarrage — Récapitulatif

| Ordre | Machine | Terminal | Commande                                        |
|-------|---------|----------|-------------------------------------------------|
| 1     | VM      | #1       | `./bin-util/satellite_server`                   |
| 2     | VM      | #2       | `./bin-util/controle_fusee_data 192.168.64.7 5555` |
| 3     | JoyPi   | #1       | `./bin-util/controle_fusee`                     |
| 4     | JoyPi   | #2       | `sudo ./bin-util/joypi_controller 192.168.64.7 5555` |

> **Important** : toujours démarrer le `satellite_server` en premier.
> Le dashboard (étape 3) peut démarrer avant ou après le contrôleur — les pipes FIFO attendent.

---

## Ordre d'arrêt

| Ordre | Action                                 |
|-------|----------------------------------------|
| 1     | `Ctrl+C` dans joypi_controller (JoyPi #2) |
| 2     | `q` dans controle_fusee (JoyPi #1)     |
| 3     | `quit` dans controle_fusee_data (VM #2) |
| 4     | `Ctrl+C` dans satellite_server (VM #1)  |

---

## Déploiement des binaires sur le JoyPi (depuis la VM de dev)

Compiler et transférer depuis la machine de développement :

```bash
# Sur la machine de développement (Linux x86)
cd ~/Desktop/Objet_Connecte/Projet_OBJ/
make joypi          # compile ARM cross-compilé
make sjoypi         # compresse et envoie par SCP vers JoyPi
# Entrer l'IP JoyPi (192.168.1.21) et l'utilisateur (leo)
```

Ou manuellement :

```bash
# Compiler
make joypi

# Transférer
scp -r To-JoyPI/ leo@192.168.1.21:~/Desktop/
```

---

## Vérification des pipes FIFO (diagnostic)

```bash
# Sur le JoyPi, vérifier que les pipes existent
ls -la /tmp/rocket_cmd.pipe /tmp/rocket_data.pipe

# Lire ce qui passe dans le pipe cmd (debug)
cat /tmp/rocket_cmd.pipe

# Lire ce qui passe dans le pipe data (debug)
cat /tmp/rocket_data.pipe
```

---

## Tests de démarrage minimal (sans JoyPi physique)

Il est possible de tester le système complet sur la VM seule en mode simulation :

**Terminal VM #1** — Serveur
```bash
./bin-util/satellite_server
```

**Terminal VM #2** — Injecteur
```bash
./bin-util/controle_fusee_data 127.0.0.1 5555
```

**Terminal VM #3** — Contrôleur en mode simulation clavier (sans GPIO)
```bash
# joypi_controller compilé sans -DUSE_WIRINGPI (mode simulation)
# Touches : 1=LANCEMENT 2=ATTERRISSAGE 3=ALTITUDE 4=TEMP 5=PRESSION 6=MELODIE q=quitter
./bin-util/joypi_controller_native 127.0.0.1 5555
```

> En mode simulation, le dashboard controle_fusee ne peut pas tourner sur VM (ncurses cross ARM),
> mais l'ensemble satellite_server ↔ controle_fusee_data ↔ joypi_controller est pleinement testable.

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| `Connection refused :5555` | satellite_server non démarré | Démarrer le serveur en premier |
| `AUTH_FAIL` | Mauvais type d'auth (CONTROLLER/INJECTOR) | Vérifier les binaires |
| Dashboard vide | Pipes FIFO non créés | Vérifier que joypi_controller est démarré |
| LCD rien | i2c non détecté | `i2cdetect -y 1` — vérifier adresse 0x21 |
| 7-seg rien | i2c non détecté | `i2cdetect -y 1` — vérifier adresse 0x70 |
| Boutons non réactifs | wiringPi non initialisé | Vérifier `sudo` sur joypi_controller |
| `Permission denied GPIO` | Pas de sudo | Relancer avec `sudo` |
| Double connexion refusée | `ERR SERVER_BUSY` si >16 clients | Vérifier qu'aucun processus zombie tourne |
