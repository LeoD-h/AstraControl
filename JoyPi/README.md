# Projet Fusee - Guide Rapide

## Architecture

```
VM (192.168.64.7)          <-- TCP 5555 -->    JoyPi (192.168.1.21)
  satellite_server                               joypi_controller
  controle_fusee_data                            controle_fusee
```

- La VM est le **satellite** : elle heberge le serveur TCP et l'injecteur de telemetrie.
- Le JoyPi est le **controleur** : il lit les boutons physiques et affiche le dashboard ncurses.
- Un seul serveur TCP (`satellite_server`) sur la VM, port **5555**.
- Le JoyPi se connecte en tant que client.

## Dossiers principaux

- `Dashboard/` : dashboard ncurses + injecteur telemetrie texte
- `Network/`   : serveur satellite TCP + protocole
- `JoyPi/`     : controleur JoyPi (boutons, actionneurs)
- `docs/`      : documentation de deploiement et d'architecture

## Programmes

| Programme            | Machine | Sources                                                           |
|----------------------|---------|-------------------------------------------------------------------|
| `satellite_server`   | VM      | `Network/satellite_server.c` + `Network/satellite_handler.c`     |
| `controle_fusee_data`| VM      | `Dashboard/data_input_text.c` + `Dashboard/dashboard_common.c`   |
| `joypi_controller`   | JoyPi   | `JoyPi/joypi_controller.c` + `JoyPi/actuators.c`                 |
| `controle_fusee`     | JoyPi   | `Dashboard/main.c` + `dashboard_logic.c` + `dashboard_visuals.c` + `pipes.c` |

## Compilation

### Bundle VM (x86 + ARM RPi)
```bash
make vm
```
Genere `To-VM/bin-util/` avec `satellite_server`, `satellite_server_rpi`, `controle_fusee_data`, `controle_fusee_data_rpi`.

### Bundle JoyPi (ARM cross-compile)
```bash
make joypi
```
Genere `To-JoyPI/` avec `controle_fusee`, `joypi_controller`, les tests hardware, et `lib/` (ncurses).

### Tout compiler
```bash
make
```

### Nettoyer
```bash
make clean
```

## Deploiement

### Envoyer sur la VM
```bash
make svm
```
Cree `To-VM.zip` et l'envoie par SCP (IP et utilisateur demandes interactivement).

### Envoyer sur le JoyPi
```bash
make sjoypi
```
Cree `To-JoyPI.zip` et l'envoie par SCP (IP et utilisateur demandes interactivement).

## Lancement

### Sur la VM (192.168.64.7)
```bash
cd To-VM
./bin-util/satellite_server 5555
# Second terminal :
./bin-util/controle_fusee_data
```

### Sur le JoyPi (192.168.1.21)
```bash
cd To-JoyPI
. ./joypi_env.sh
./bin-util/joypi_controller 192.168.64.7 5555 &
./bin-util/controle_fusee 192.168.64.7 5555
```

## Tests hardware JoyPi

```bash
. ./joypi_env.sh
./bin-proto/hardware_buttons_test
./bin-proto/hardware_actuators_test
```

## Binaires - organisation

- `bin-util/` : executables metier (satellite, dashboard, controller, data)
- `bin-proto/` : executables de test hardware (boutons, actionneurs)

## Documentation detaillee

Voir `docs/DEPLOIEMENT_JOYPI.md` pour le schema reseau complet, le protocole TCP,
les commandes/reponses et les procedures de test sans hardware physique.

## Auteurs

Leo, Ines, Juliann
