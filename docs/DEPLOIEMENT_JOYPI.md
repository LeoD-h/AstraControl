# Deploiement VM Satellite + JoyPi Controleur

## Architecture reseau

```
  +---------------------------+          TCP 5555          +---------------------------+
  |   VM  (192.168.64.7)      |<-------------------------->|  JoyPi (192.168.1.21)     |
  |                           |                            |                           |
  |  satellite_server         |   <-- commandes JoyPi      |  joypi_controller         |
  |    (ecoute :5555)         |   --> telemetrie / ACK      |    (client TCP)           |
  |                           |                            |                           |
  |  controle_fusee_data      |                            |  controle_fusee           |
  |    (injecteur telemetrie) |                            |    (dashboard ncurses)    |
  +---------------------------+                            +---------------------------+
          [x86 / VM Linux]                                       [ARM / Raspberry Pi]
```

- Le seul serveur TCP est `satellite_server` sur la VM, port **5555**.
- Le JoyPi est **client** : `joypi_controller` se connecte a la VM.
- `controle_fusee` (dashboard ncurses) tourne **uniquement sur le JoyPi**.
- `controle_fusee_data` (injecteur de telemetrie) tourne **uniquement sur la VM**.

---

## Programmes

### VM - satellite_server

Serveur TCP central. Recoit les commandes du JoyPi, gere l'etat de la fusee, repond avec la telemetrie.

- Sources : `Network/satellite_server.c` + `Network/protocol.c`
- Binaire VM (x86) : `To-VM/bin-util/satellite_server`
- Binaire RPi (ARM) : `To-VM/bin-util/satellite_server_rpi`
- Lancement :
  ```bash
  ./bin-util/satellite_server 5555
  ```

### VM - controle_fusee_data

Injecteur de telemetrie en mode texte. Envoie des donnees simulees de vol au satellite_server via pipe ou stdin.

- Sources : `Dashboard/data_input_text.c` + `Dashboard/dashboard_common.c`
- Binaire VM (x86) : `To-VM/bin-util/controle_fusee_data`
- Binaire RPi (ARM) : `To-VM/bin-util/controle_fusee_data_rpi`
- Lancement (dans un second terminal sur la VM) :
  ```bash
  ./bin-util/controle_fusee_data
  ```

### JoyPi - joypi_controller

Client TCP. Lit les boutons physiques du JoyPi (wiringPi), envoie les commandes au satellite_server sur la VM.

- Sources : `JoyPi/joypi_controller.c` + `JoyPi/actuators.c`
- Binaire : `To-JoyPI/bin-util/joypi_controller`
- Lancement :
  ```bash
  ./bin-util/joypi_controller 192.168.64.7 5555
  ```

### JoyPi - controle_fusee

Dashboard ncurses interactif. Affiche la telemetrie recue depuis le satellite_server.

- Sources : `Dashboard/main.c` + `Dashboard/dashboard_logic.c` + `Dashboard/dashboard_visuals.c` + `Dashboard/pipes.c`
- Binaire : `To-JoyPI/bin-util/controle_fusee`
- Lancement (necessite ncurses cross-compile) :
  ```bash
  . ./joypi_env.sh
  ./bin-util/controle_fusee 192.168.64.7 5555
  ```

---

## Compilation

### Prerequis

- Cross-compilateur ARM installe dans :
  `/home/leo/Desktop/CCR/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin`
- ncurses cross-compile pour RPi dans :
  `/home/leo/Desktop/CCR/ncurses-lab/target_NC_PI`

### Compiler le bundle VM

```bash
make vm
```

Genere `To-VM/` avec :
```
To-VM/
  bin-util/
    satellite_server        (x86 - a lancer sur la VM)
    satellite_server_rpi    (ARM - pour futur RPi physique)
    controle_fusee_data     (x86 - a lancer sur la VM)
    controle_fusee_data_rpi (ARM - pour futur RPi physique)
```

### Compiler le bundle JoyPi

```bash
make joypi
```

Genere `To-JoyPI/` avec :
```
To-JoyPI/
  bin-util/
    controle_fusee      (ARM - dashboard ncurses)
    joypi_controller    (ARM - client satellite + wiringPi)
  bin-proto/
    hardware_buttons_test    (ARM - test boutons physiques)
    hardware_actuators_test  (ARM - test actionneurs)
  lib/
    libncursesw.so.6.6  (ncurses cross-compile)
    libncursesw.so.6    -> libncursesw.so.6.6
    libncurses.so.6     -> libncursesw.so.6.6
  joypi_env.sh          (configure LD_LIBRARY_PATH)
```

### Compiler les deux en meme temps

```bash
make
# ou
make all
```

### Nettoyer

```bash
make clean
```

---

## Deploiement

### Envoyer le bundle VM sur la VM (ou RPi)

```bash
make svm
```

La commande demande interactivement l'IP de destination et le nom d'utilisateur SSH, puis envoie `To-VM.zip` via `scp`.

### Envoyer le bundle JoyPi sur le JoyPi

```bash
make sjoypi
```

La commande demande interactivement l'IP du JoyPi et le nom d'utilisateur SSH, puis envoie `To-JoyPI.zip` via `scp`.

### Demarrer manuellement sur la VM

```bash
ssh leo@192.168.64.7
cd ~/Desktop
unzip To-VM.zip
cd To-VM
./bin-util/satellite_server 5555
# Dans un second terminal :
./bin-util/controle_fusee_data
```

### Demarrer manuellement sur le JoyPi

```bash
ssh leo@192.168.1.21
cd ~/Desktop
unzip To-JoyPI.zip
cd To-JoyPI
. ./joypi_env.sh
./bin-util/joypi_controller 192.168.64.7 5555 &
./bin-util/controle_fusee 192.168.64.7 5555
```

---

## Tests hardware (sans fusee)

### Tester les boutons physiques du JoyPi

```bash
. ./joypi_env.sh
./bin-proto/hardware_buttons_test
```

Affiche les evenements de chaque bouton detecte par wiringPi.

### Tester les actionneurs du JoyPi

```bash
. ./joypi_env.sh
./bin-proto/hardware_actuators_test
```

Active et desactive sequentiellement chaque actionneur pour verification.

### Simuler sans JoyPi physique (clavier)

Il est possible de tester le dashboard seul en simulant les commandes au clavier.
Lancer uniquement `controle_fusee` en mode standalone (sans `joypi_controller`) :
```bash
. ./joypi_env.sh
./bin-util/controle_fusee 192.168.64.7 5555
```
Les touches du clavier (fleches, Entree, lettres) remplacent les boutons physiques
pour naviguer dans le dashboard et envoyer des commandes de test.

---

## Protocole TCP (port 5555)

### Commandes envoyees par le JoyPi -> VM

| Commande         | Description                              |
|------------------|------------------------------------------|
| `LAUNCH`         | Lancer la sequence de mise a feu         |
| `ABORT`          | Avorter la mission                       |
| `STATUS`         | Demander l'etat courant de la fusee      |
| `JOYPI_KEY <k>`  | Touche physique JoyPi (code de touche k) |
| `SET <param> <v>`| Modifier un parametre de telemetrie      |
| `PROBLEM ON`     | Injecter une panne                       |
| `PROBLEM OFF`    | Retirer la panne                         |
| `GEN 1`          | Activer la generation automatique        |
| `GEN 0`          | Desactiver la generation automatique     |

### Reponses envoyees par la VM -> JoyPi

| Reponse          | Description                              |
|------------------|------------------------------------------|
| `ACK <cmd>`      | Commande acceptee                        |
| `NACK <cmd>`     | Commande refusee / invalide              |
| `TELEM <data>`   | Paquet de telemetrie (JSON ou CSV)       |
| `ALERT <msg>`    | Alerte ou panne detectee                 |
| `STATE <etat>`   | Etat courant de la fusee                 |

### Adresses et port

| Machine | IP              | Role         |
|---------|-----------------|--------------|
| VM      | 192.168.64.7    | Serveur TCP  |
| JoyPi   | 192.168.1.21    | Client TCP   |
| Port    | 5555            | TCP          |

---

## Auteurs

Leo, Ines, Juliann
