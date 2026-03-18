# Simulation de contrôle de fusée sur architecture embarquée hétérogène : TCP, GPIO et ncurses sous Linux

**Revue Technique Open Source — Volume 12, Numéro 2, Mars 2026**

*Léo Bernardo, Inès Martin, Juliann Chassot*
*Département Systèmes Embarqués et Réseaux, IUT de Informatique*

---

## Résumé

Cet article présente la conception, l'implémentation et le déploiement d'un système distribué de simulation de contrôle de mission spatiale reposant exclusivement sur des logiciels libres et des composants Linux standard. L'architecture déployée interconnecte une machine virtuelle (VM) jouant le rôle de satellite terrestre avec un ordinateur embarqué ARM de type Raspberry Pi (JoyPi) jouant le rôle de poste de contrôle. Les communications s'effectuent par sockets TCP via un protocole texte maison, les actuateurs physiques (LED, buzzer piézoélectrique, matrice LED 8×8 MAX7219, afficheur 7 segments HT16K33, écran LCD HD44780 via MCP23017) sont pilotés par wiringPi, et l'interface graphique du tableau de bord est rendue par ncurses. L'article détaille les choix d'architecture, les protocoles de communication, les défis de cross-compilation ARM, la gestion temps-réel via `select()`, ainsi que les problèmes de débogage rencontrés et résolus.

**Mots-clés** : systèmes embarqués, TCP/IP, Raspberry Pi, wiringPi, ncurses, cross-compilation ARM, select(), FIFO, i2c, SPI, GPIO

---

## 1. Introduction

La simulation de systèmes complexes sur architectures hétérogènes constitue un terrain d'apprentissage exceptionnel pour les étudiants en systèmes embarqués et réseaux. Contrairement aux laboratoires purement logiciels, l'introduction de composants matériels réels — capteurs, actionneurs, interfaces d'entrée/sortie — impose des contraintes de latence, de fiabilité et de séquencement qui reflètent fidèlement les contraintes industrielles.

Ce projet, développé dans le cadre d'un cours d'objets connectés, propose une simulation complète de contrôle de mission spatiale : une fusée virtuelle est lancée depuis un poste de contrôle physique (JoyPi), son état est géré par un serveur satellite (VM), et sa télémétrie est visualisée sur un tableau de bord ncurses. Chaque action de l'opérateur déclenche des effets sur les actuateurs physiques et dans le système logiciel distribué.

Les objectifs pédagogiques sont multiples :
- Maîtrise de la programmation réseau bas niveau en C (sockets POSIX)
- Conception de protocoles texte pour systèmes embarqués
- Cross-compilation pour architecture ARM depuis Linux x86
- Pilotage de périphériques I2C et SPI sur Raspberry Pi
- Interfaces utilisateur temps-réel avec ncurses
- Gestion d'événements asynchrones via `select()` non-bloquant

Cet article présente les choix architecturaux, les solutions techniques adoptées et les leçons apprises lors de ce développement.

---

## 2. Architecture globale du système

### 2.1 Vue d'ensemble

Le système se compose de quatre processus communicants répartis sur deux machines physiques :

```
Machine 1 : VM x86 (192.168.64.7)
  ┌─────────────────────────────────────────┐
  │  satellite_server  :5555                │
  │  Serveur TCP select()-based             │
  │  MAX_CLIENTS = 16 connexions            │
  │  Gestion état machine fusée             │
  │                                         │
  │  controle_fusee_data                    │
  │  Client INJECTOR TCP                    │
  │  Simulation physique de fusée           │
  │  Envoi SET ALTITUDE/SPEED/FUEL… 700ms   │
  └─────────────────────────────────────────┘

Machine 2 : JoyPi ARM Raspberry Pi (192.168.1.21)
  ┌─────────────────────────────────────────┐
  │  joypi_controller                       │
  │  Client CONTROLLER TCP                  │
  │  Lecture clavier matriciel 4x4 GPIO     │
  │  Pilotage actuateurs                    │
  │  Forward données → FIFO pipes           │
  │                                         │
  │  controle_fusee                         │
  │  Dashboard ncurses cross-compilé ARM    │
  │  Lit FIFO pipes ← joypi_controller      │
  │  Affichage état fusée temps réel        │
  └─────────────────────────────────────────┘
```

### 2.2 Classification des clients TCP

Le serveur distingue deux types de clients par leur message d'authentification initial :

- **CONTROLLER** : le JoyPi. Il émet des commandes de mission (lancement, atterrissage, mesures) et reçoit en push la télémétrie et les événements.
- **INJECTOR** : controle_fusee_data sur la VM. Il injecte en permanence les valeurs de télémétrie (altitude, vitesse, carburant…) et reçoit des CMD_EVENT pour adapter la simulation physique.

Cette séparation permet théoriquement de connecter plusieurs contrôleurs simultanément (MAX_CLIENTS = 16), avec broadcast des événements à tous les CONTROLLER authentifiés.

### 2.3 Communication inter-processus locale

Sur le JoyPi, `joypi_controller` et `controle_fusee` (le dashboard) ne communiquent pas en TCP mais via deux pipes FIFO nommés POSIX :

- `/tmp/rocket_cmd.pipe` : commandes de contrôle (LAUNCH_OK, LAND, RESUME, FIX_PROBLEM…)
- `/tmp/rocket_data.pipe` : données de télémétrie (SET ALTITUDE 15000, SET FUEL 78…)

Ce choix architectural évite d'ajouter un protocole TCP supplémentaire entre deux processus locaux, simplifie la synchronisation, et permet de démarrer les deux programmes indépendamment dans n'importe quel ordre — les pipes FIFO bloquent l'ouverture en écriture jusqu'à ce qu'un lecteur soit présent, mais l'option `O_NONBLOCK` contourne ce blocage.

---

## 3. Le serveur satellite — satellite_server.c

### 3.1 Architecture select() et gestion multi-clients

Le cœur du serveur est une boucle `select()` classique, non-bloquante, sans threads. Ce choix délibéré simplifie la gestion de la concurrence : un seul fil d'exécution traite tous les événements, éliminant les conditions de course sur l'état global.

```c
while (!g_stop) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sfd, &rfds);       /* socket serveur */
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0)
            FD_SET(clients[i].fd, &rfds);
    }
    struct timeval tv = {0, SELECT_TIMEOUT_MS * 1000};
    int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
    /* ... traitement ... */
    check_state_transitions(clients);    /* machine à états */
    push_telemetry_periodique(clients);  /* push toutes les 2s */
}
```

Le timeout de 100 ms sur `select()` permet d'exécuter périodiquement les transitions d'état automatiques et le push de télémétrie sans nécessiter de timer POSIX séparé.

### 3.2 Machine à états de la fusée

La fusée suit un automate à 5 états :

```
         CMD LU              fuel=0
READY ──────────► FLYING ──────────► LANDING
  ▲                 │ CMD LD              │ altitude=0
  │                 ▼                    │
  │           EMERGENCY ────────────────►┘
  │              altitude=0
  └──────────── (depuis LANDING ou EMERGENCY)

FLYING → EXPLODED (si stress > 100 ou temperature > SAT_TEMP_CRITICAL)
```

Les transitions sont vérifiées à chaque itération de boucle dans `check_state_transitions()`. Les seuils critiques (`SAT_TEMP_CRITICAL`, `SAT_STRESS_CRITICAL`) sont définis dans `satellite_protocol.h`, fichier d'interface partagé entre le serveur et le client.

### 3.3 Gestion des buffers de lecture

Chaque client maintient un buffer `pending[512]` pour les données partiellement reçues. TCP étant un protocole de flux et non de messages, un `read()` peut retourner une fraction de ligne ou plusieurs lignes concaténées. Le parser extrait toutes les lignes complètes (`\n`-délimitées) et conserve le fragment partiel :

```c
char *start = c->pending;
char *nl;
while ((nl = strchr(start, '\n')) != NULL) {
    *nl = '\0';
    dispatch_line(clients, idx, start);
    start = nl + 1;
}
size_t remain = strlen(start);
memmove(c->pending, start, remain + 1);
c->used = remain;
```

Cette technique est fondamentale pour tout protocole texte sur TCP et est reproduite dans tous les composants du projet.

### 3.4 Gestion des signaux

Un soin particulier a été apporté à la gestion des signaux Unix. Le gestionnaire `on_signal()` ne fait qu'armer le flag `g_stop` — il ne ferme pas le socket serveur, car `close()` n'est pas async-signal-safe selon POSIX.2 :

```c
static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
    /* Ne pas appeler close() ici : non async-signal-safe selon POSIX */
}
```

Le nettoyage complet (fermeture de tous les sockets clients, du socket serveur, des pipes FIFO) s'effectue à la sortie normale de la boucle principale. `SIGPIPE` est ignoré (`SIG_IGN`) pour éviter que l'écriture sur un socket fermé côté client ne tue le processus serveur.

### 3.5 Protocole d'authentification

L'authentification est minimaliste mais fonctionnelle. Le client envoie son type immédiatement après connexion :

```
Client → Serveur : AUTH CONTROLLER\n  (ou AUTH INJECTOR\n)
Serveur → Client : AUTH_OK\n          (ou AUTH_FAIL\n)
```

Un choix de conception important : le serveur n'envoie *pas* de message de bienvenue `OK CONNECTED` avant l'authentification. Ce greeting avait causé un bug critique lors du développement : le client lisait le greeting en croyant lire `AUTH_OK`, entraînant un désalignement protocolaire. La leçon : dans un protocole requête-réponse, c'est toujours le client qui parle en premier.

---

## 4. L'injecteur de télémétrie — controle_fusee_data

### 4.1 Simulation physique de fusée

L'injecteur embarque un modèle physique simplifié mais réaliste d'une fusée :

```c
typedef struct {
    double altitude_m;    /* altitude en mètres */
    double speed_kmh;     /* vitesse en km/h */
    double fuel_pct;      /* carburant restant 0-100% */
    double temp_c;        /* température moteur en °C */
    double pressure_hpa;  /* pression atmosphérique */
    double thrust_kn;     /* poussée en kN */
    double stress;        /* contrainte structurelle */
} GenModel;
```

À chaque tick (calculé par delta-temps réel pour être indépendant de la charge CPU) :

**Phase montée** :
- Poussée : 1600 - (altitude/120) kN, limitée à [800, 1700] kN
- Vitesse : accélération de 55 km/h/s réduite par vitesse et altitude
- Consommation carburant : 0.35 + thrust/6000 % par seconde
- Température : convergence vers 15 + speed/180 + thrust/85 - altitude/1200 °C

**Phase atterrissage** :
- Poussée réduite à 700 kN
- Décélération forcée : 80 km/h/s jusqu'à vitesse minimale 120 km/h
- Si vitesse > 260 km/h à l'impact : explosion

**Événements CMD_EVENT** reçus du serveur (LAUNCH, LAND, RESUME, FIX_PROBLEM) modifient l'état `gm.launched`, `gm.landing` etc.

### 4.2 Non-blocage de la boucle principale

Un problème initial critique était l'utilisation de `sleep(3)` lors des tentatives de reconnexion. Cela bloquait le modèle physique pendant 3 secondes, causant un saut brutal dans la simulation lors de la reconnexion. La solution adoptée est un throttle par timestamp :

```c
static void reconnect_if_needed(int *fd_ptr, const char *ip, int port) {
    if (*fd_ptr >= 0) return;
    static unsigned long long last_attempt_ms = 0;
    unsigned long long tnow = now_ms();
    if (tnow - last_attempt_ms < 3000ULL) return;
    last_attempt_ms = tnow;
    /* ... tentative de connexion ... */
}
```

Ce pattern — stocker le timestamp de la dernière action et vérifier l'écart à chaque itération — est la base de tout code temps-réel non-bloquant en C.

### 4.3 Réception non-bloquante des CMD_EVENT

Le fd serveur est mis en mode non-bloquant après connexion (`fcntl(fd, F_SETFL, O_NONBLOCK)`). La réception utilise une boucle `read()` qui s'arrête sur `EAGAIN/EWOULDBLOCK` — sans timeout de select. Cela permet de vider complètement le buffer de réception à chaque itération de la boucle principale sans bloquer :

```c
while (1) {
    ssize_t nr = read(fd, buf, sizeof(buf) - 1);
    if (nr < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        return -1;  /* erreur réelle */
    }
    if (nr == 0) return -1;  /* connexion fermée */
    /* ... accumulation dans pending ... */
}
```

---

## 5. Le contrôleur JoyPi — joypi_controller.c

### 5.1 Boucle principale asynchrone

`joypi_controller` doit simultanément :
1. Surveiller l'arrivée de données push depuis le satellite (TCP)
2. Scanner le clavier matriciel GPIO
3. Tenter de reconnecter si la connexion est perdue
4. Piloter les actuateurs (LED, buzzer, matrice, LCD)

Toutes ces tâches s'enchaînent dans une boucle unique non-bloquante. Le scan GPIO utilise `usleep()` court (80 ms) et le poll satellite utilise `select()` avec timeout 80 ms.

```c
while (!g_stop) {
    if (st.sat_fd < 0 || !st.authed)
        try_reconnect(&st);         /* si déconnecté */
    poll_satellite_push(&st);       /* reçoit TELEMETRY, EVENT... */
    scan_buttons_and_handle(&st);   /* lit GPIO ou stdin */
}
```

### 5.2 Lecture de la matrice de clavier 4×4

Le JoyPi embarque un clavier matriciel 4 rangées × 4 colonnes, câblé sur des pins physiques wiringPi. La technique de balayage est la suivante :

1. Mettre toutes les colonnes à HIGH (inactif)
2. Pour chaque colonne, mettre la colonne à LOW
3. Lire l'état de toutes les rangées
4. Une rangée à LOW pendant qu'une colonne est LOW indique une touche enfoncée

```c
static void read_matrix(int state[ROW_COUNT][COL_COUNT]) {
    for (int c = 0; c < COL_COUNT; ++c) {
        digitalWrite(COL_PINS[c], LOW);
        delayMicroseconds(200);         /* stabilisation signal */
        for (int r = 0; r < ROW_COUNT; ++r) {
            state[r][c] = (digitalRead(ROW_PINS[r]) == LOW);
        }
        digitalWrite(COL_PINS[c], HIGH);
    }
}
```

La détection de fronts montants (touche nouvellement enfoncée) est réalisée en comparant l'état courant avec l'état précédent `g_prev_matrix[r][c]` — seules les nouvelles pressions sont traitées, évitant les déclenchements multiples sur un maintien.

### 5.3 Partage de pins entre keypad et direction

Une subtilité matérielle importante : les pins de colonnes du clavier matriciel (`COL_PINS = {33, 35, 37, 22}`) sont partagées avec les boutons directionnels du JoyPi (`PIN_DIR_DOWN=33`, `PIN_DIR_RIGHT=35`, `PIN_DIR_UP=37`, `PIN_DIR_LEFT=22`). Pour lire les directions, il faut temporairement reconfigurer ces pins en entrée, effectuer la lecture, puis les remettre en sortie pour le scan suivant du clavier :

```c
static void read_direction(int *up, int *down, ...) {
    for (int c = 0; c < COL_COUNT; ++c) {
        pinMode(COL_PINS[c], INPUT);
        pullUpDnControl(COL_PINS[c], PUD_UP);
    }
    delayMicroseconds(120);
    *down = (digitalRead(PIN_DIR_DOWN) == LOW);
    /* ... */
    for (int c = 0; c < COL_COUNT; ++c) {
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], HIGH);
    }
}
```

### 5.4 Protection contre la perte de données push

Un problème protocolaire délicat : lors d'une commande `CMD LU`, le serveur peut répondre `OK LAUNCH\n` et immédiatement envoyer un `EVENT LAUNCH\n` dans le même segment TCP. Un `read()` unique peut les recevoir ensemble. La fonction `send_cmd_recv()` doit alors isoler la première ligne (la réponse directe) et sauvegarder le surplus dans un buffer global pour traitement ultérieur par `poll_satellite_push()` :

```c
char *nl = strchr(resp, '\n');
if (nl) {
    const char *leftover     = nl + 1;
    size_t      leftover_len = strlen(leftover);
    if (leftover_len > 0 &&
            g_push_used + leftover_len < sizeof(g_push_buf) - 1) {
        /* Insérer en tête du push buffer pour traitement prioritaire */
        memmove(g_push_buf + leftover_len, g_push_buf, g_push_used + 1);
        memcpy(g_push_buf, leftover, leftover_len);
        g_push_used += leftover_len;
    }
    *nl = '\0';
}
```

### 5.5 Mode saisie mot de passe

Pour sécuriser le lancement, le contrôleur implémente un mode `MODE_PASSWORD`. Lors de l'appui sur BT1, le mode bascule de `MODE_NORMAL` à `MODE_PASSWORD`. Les touches du clavier numérique servent alors à saisir le code `"123"`, affiché sous forme d'étoiles sur le LCD. Seule la validation correcte déclenche `CMD LU`.

Ce mécanisme évite un lancement accidentel et illustre une gestion d'état machine côté hardware.

### 5.6 Gestion robuste des pipes

Les pipes FIFO vers le dashboard peuvent échouer si `controle_fusee` est arrêté ou redémarré. La détection d'erreur `EPIPE`/`ENXIO` ferme le fd et force une réouverture à la prochaine écriture :

```c
static void cmd_pipe_write(ControllerState *st, const char *msg) {
    if (st->cmd_pipe_fd < 0)
        st->cmd_pipe_fd = open_pipe_writer(CMD_PIPE);
    if (st->cmd_pipe_fd < 0) return;
    ssize_t nw = write(st->cmd_pipe_fd, msg, strlen(msg));
    if (nw < 0 && (errno == EPIPE || errno == ENXIO)) {
        close(st->cmd_pipe_fd);
        st->cmd_pipe_fd = -1;
    }
}
```

---

## 6. Les actuateurs — actuators.c

### 6.1 Architecture "lazy init"

Tous les périphériques sont initialisés à la demande (lazy initialization) — uniquement lors de leur première utilisation. Cela évite d'échouer au démarrage si un périphérique est temporairement absent ou en cours d'initialisation. Un flag `g_spi_ready` et des fd `g_seg_fd`, `g_lcd_fd` gardent l'état d'initialisation.

### 6.2 Buzzer piézo — génération logicielle de tonalités

Le buzzer sur le pin physique 12 (GPIO 18, capable PWM matériel) est piloté en logiciel pur par bascule GPIO :

```c
static void play_tone(int freq_hz, int duration_ms) {
    int half_us = 1000000 / (freq_hz * 2);
    int cycles  = (freq_hz * duration_ms) / 1000;
    for (int i = 0; i < cycles; i++) {
        digitalWrite(PIN_BUZZER_PHYS, 1);
        delayMicroseconds(half_us);
        digitalWrite(PIN_BUZZER_PHYS, 0);
        delayMicroseconds(half_us);
    }
}
```

Le PWM logiciel est moins précis que le PWM matériel (jitter dû au scheduling Linux), mais suffit pour des tonalités musicales simples. Trois mélodies distinctes permettent de différencier acoustiquement les événements : décollage (festive, montante), urgence (alarme rapide), problème (grave, répétitive).

### 6.3 Matrice LED 8×8 — MAX7219 via SPI

La matrice 8×8 LEDs est pilotée par un MAX7219, circuit intégré de pilotage de displays LED, connecté en SPI sur le canal CE1 (chip enable 1, pin physique 26). Le protocole SPI du MAX7219 est simple : chaque transfert de 2 octets = [registre, valeur].

L'initialisation configure le mode "no decode" (pas de décodage BCD, chaque bit correspond à une LED), la limite de scan à 8 lignes, et l'intensité à 8/15 (0x08) — valeur validée par test hardware :

```c
max7219_write(0x0F, 0x00); /* désactiver mode test */
max7219_write(0x0C, 0x01); /* allumer le circuit */
max7219_write(0x0B, 0x07); /* scan 8 lignes */
max7219_write(0x09, 0x00); /* mode raw (no BCD decode) */
max7219_write(0x0A, 0x08); /* intensité 8/15 */
```

Les animations (décollage, urgence, clear) utilisent des tableaux de patterns 8 octets pré-calculés.

### 6.4 Afficheur 7 segments — HT16K33 via I2C

Le HT16K33 est un driver d'afficheur 7 segments avec interface I2C à l'adresse 0x70. La correspondance entre les positions physiques des digits et les registres du HT16K33 sur le Joy-Pi a nécessité une phase de tests empiriques (hardware_actuators_test.c) :

```c
/* Layout physique vérifié :
 * registre 0 → position 3 (dizaines)
 * registre 2 → position 4 (unités)
 * registre 4 → position 1 (milliers)
 * registre 6 → position 2 (centaines)
 * Correspondance: index ^2 du test */
buf[0] = digits[d2]; /* reg 0 → dizaines */
buf[1] = digits[d3]; /* reg 2 → unités */
buf[2] = digits[d0]; /* reg 4 → milliers */
buf[3] = digits[d1]; /* reg 6 → centaines */
```

Ce mapping non-linéaire est caractéristique du câblage interne des modules d'affichage intégrés.

### 6.5 Écran LCD HD44780 via MCP23017

L'écran LCD 16×2 caractères utilise un contrôleur HD44780 en mode 4 bits, piloté via un expandeur GPIO I2C MCP23017 à l'adresse 0x21. La chaîne de pilotage est donc : `wiringPi I2C → MCP23017 GPIO → HD44780 4-bit`.

L'initialisation HD44780 en mode 4 bits est une séquence précise de 8 commandes qui ne doit pas être altérée :

```c
lcd_command(0x33); /* reset séquence 1 */
lcd_command(0x32); /* reset séquence 2 → passe en mode 4 bits */
lcd_command(0x0C); /* display ON, cursor OFF, blink OFF */
lcd_command(0x28); /* 4 bits, 2 lignes, font 5×8 */
lcd_command(0x06); /* incrémenter curseur, pas de shift */
lcd_command(0x01); /* effacer l'écran (nécessite delay) */
delay(2);
lcd_command(0x80); /* curseur en position 0 de la ligne 1 */
```

Un bug critique initial était l'absence de cette séquence dans `lcd_init()` — le LCD affichait des caractères aléatoires ou rien du tout. La séquence avait été validée dans le fichier de test `hardware_actuators_test.c` mais n'avait pas été reportée dans le code de production.

---

## 7. Le dashboard ncurses — controle_fusee

### 7.1 Architecture du dashboard

Le dashboard ncurses est une interface visuelle temps réel de l'état de la fusée. Il se compose de plusieurs modules :

- `dashboard_logic.c` : machine à états interne, application des commandes et données, dynamique de simulation locale
- `dashboard_visuals.c` : rendu ncurses (dessins ASCII, barres de progression, animations)
- `pipes.c` : lecture non-bloquante des FIFO pipes
- `main.c` : boucle principale, intégration des modules
- `control_ncurses.c` / `control_text.c` : interfaces ncurses et texte

### 7.2 Double source de données

Le dashboard accepte des données de deux sources :
1. **Pipe cmd** (`/tmp/rocket_cmd.pipe`) : commandes de contrôle (LAUNCH_OK, LAND, RESUME…) qui modifient l'état logique via `apply_cmd()`
2. **Pipe data** (`/tmp/rocket_data.pipe`) : données numériques de télémétrie (SET ALTITUDE/SPEED/FUEL…) via `apply_data()`

Cette séparation permet au dashboard d'être alimenté soit par `joypi_controller` (mode production), soit manuellement via `echo "SET ALTITUDE 5000" > /tmp/rocket_data.pipe` (mode debug).

### 7.3 Dynamique locale

En plus des données reçues par pipe, le dashboard calcule une dynamique locale dans `update_dynamics()`. Cela rend l'animation fluide même si les données n'arrivent que toutes les 700 ms – 2 secondes. La flamme de la fusée est animée par `flame_size` qui oscille pendant le vol. La position de la fusée est interpolée entre les mises à jour.

Un bug subtil initial : `flame_size` était initialisé à 3 dans `init_state()`, ce qui affichait une flamme active sur la fusée encore au sol. La valeur correcte est 0.

### 7.4 Gestion du popup de lancement

Le dashboard implémente un popup de saisie de mot de passe activé par la commande `LAUNCH` (depuis le dashboard lui-même via clavier). Ce popup est *bypassé* par la commande `LAUNCH_OK` (envoyée par joypi_controller après validation du mot de passe sur le hardware). Ce mécanisme évite la double authentification : si l'opérateur a saisi le mot de passe sur le clavier physique JoyPi, il ne doit pas le saisir une seconde fois sur l'écran du dashboard.

---

## 8. Cross-compilation ARM et déploiement

### 8.1 Chaîne de compilation

La cross-compilation utilise le toolchain Linaro `arm-linux-gnueabihf-gcc` (v4.9.3) :

```makefile
CC_CROSS = /home/leo/Desktop/CCR/tools-master/arm-bcm2708/\
           gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/\
           arm-linux-gnueabihf-gcc
```

Deux variantes sont produites pour chaque composant VM :
- Binaire natif x86 (`CC_NATIVE = gcc`) pour tests locaux sur VM
- Binaire ARM cross-compilé pour déploiement futur sur RPi hardware

### 8.2 Bibliothèques cross-compilées

Deux bibliothèques nécessitent une cross-compilation préalable :

**wiringPi** (pilotage GPIO/I2C/SPI) : recompilée depuis les sources pour ARM, installée dans `wiringPi-36fb7f1/wiringPi/`. Elle est liée dynamiquement (`-lwiringPi`) depuis un répertoire connu (`-L$(WIRINGPI_ROOT)/wiringPi`).

**ncurses** : recompilée pour ARM avec support wide-character (`libncursesw`) dans `target_NC_PI/`. Nécessaire pour le dashboard car l'ABI ncurses de la distribution JoyPi peut différer.

### 8.3 Bundle de déploiement

Le Makefile produit un bundle prêt à déployer :

```
To-JoyPI/
├── bin-util/
│   ├── controle_fusee       (dashboard ncurses ARM)
│   └── joypi_controller     (contrôleur satellite ARM)
├── bin-proto/
│   ├── hardware_buttons_test
│   └── hardware_actuators_test
├── lib/
│   ├── libncursesw.so.6.6
│   ├── libncursesw.so.6 → libncursesw.so.6.6
│   ├── libwiringPi.so.2.50
│   └── libwiringPi.so.2 → libwiringPi.so.2.50
└── joypi_env.sh    (charge LD_LIBRARY_PATH)
```

Le script `joypi_env.sh` configure `LD_LIBRARY_PATH` pour que les binaires trouvent les bibliothèques du bundle plutôt que celles du système — évitant les conflits de version.

### 8.4 Transfert SCP automatisé

```makefile
sjoypi: joypi
    zip -r -q $(JOYPI_ZIP) $(JOYPI_DIR)
    read -p "IP JoyPi: " ip; \
    scp $(JOYPI_ZIP) leo@$$ip:~/Desktop/
```

---

## 9. Protocole de communication

### 9.1 Conception du protocole texte

Le protocole est délibérément simple : texte ASCII, une commande par ligne terminée par `\n`. Ce choix offre plusieurs avantages :
- Débogage trivial avec `netcat` ou `telnet`
- Pas de désérialisation binaire
- Lisibilité des logs
- Indépendance de l'endianness

La principale contrepartie est l'overhead par rapport à un protocole binaire, négligeable ici compte tenu des volumes de données.

### 9.2 Gestion de la fragmentation TCP

TCP est un protocole de *flux*, pas de *messages*. Trois cas de fragmentation doivent être gérés :

1. **Message fragmenté** : `"CMD LU\n"` reçu en deux reads : `"CMD"` et `" LU\n"`. Géré par accumulation dans `pending[]` avec extraction des lignes complètes.

2. **Messages coalescés** : `"OK LAUNCH\nEVENT LAUNCH\n"` reçus en un seul read. Géré par le parser ligne-par-ligne.

3. **Réponse + push coalescés** : lors de l'envoi d'une commande, la réponse directe ET un push asynchrone peuvent arriver ensemble. `send_cmd_recv()` isole la première ligne et place le reste dans `g_push_buf`.

### 9.3 Séquences de connexion

```
Client                    Serveur
  │                          │
  │──── AUTH CONTROLLER ────►│
  │◄─── AUTH_OK ─────────────│
  │                          │
  │──── CMD LU ─────────────►│ (lancement)
  │◄─── OK LAUNCH ───────────│
  │◄─── EVENT LAUNCH ────────│ (push broadcast)
  │                          │
  │◄─── TELEMETRY alt=… ─────│ (push toutes les 2s)
```

---

## 10. Tests et validation

### 10.1 Tests unitaires hardware

Deux programmes de test hardware ont été développés et exécutés sur le JoyPi physique avant l'intégration :

**hardware_buttons_test.c** : scanne la matrice de clavier en boucle et affiche chaque touche détectée. Valide le câblage, le timing de scan, et les numéros de pins. Résultat : mapping `(rangée × 4 + (4 - col))` confirmé.

**hardware_actuators_test.c** : teste chaque actuateur séquentiellement avec des patterns de validation visuels/auditifs. A permis de découvrir :
- Le mapping XOR `^2` des positions du HT16K33
- L'intensité correcte MAX7219 (0x08 et non 0x04)
- La séquence d'initialisation LCD HD44780 en 8 étapes

### 10.2 Tests d'intégration réseau

Test de bout en bout via `netcat` (validé en développement) :

```bash
# Terminal 1 : démarrer le serveur
./satellite_server

# Terminal 2 : simuler un contrôleur
nc 192.168.64.7 5555
AUTH CONTROLLER       # → AUTH_OK
CMD LU               # → OK LAUNCH (si READY)
CMD ALT              # → DATA ALT <valeur>

# Terminal 3 : simuler un injecteur
nc 192.168.64.7 5555
AUTH INJECTOR        # → AUTH_OK
SET ALTITUDE 15000   # → OK
SET FUEL 42          # → OK
```

### 10.3 Procédure de test complète recommandée

**Phase 1 — Test individuel des composants VM**

```bash
# Test 1 : satellite_server seul
./satellite_server &
nc localhost 5555
AUTH CONTROLLER   # AUTH_OK
CMD LU           # OK LAUNCH
CMD ALT          # DATA ALT 0
^C
```

```bash
# Test 2 : satellite_server + injecteur
./satellite_server &
./controle_fusee_data 127.0.0.1 5555 &
# Attendre 5s, observer les logs GEN dans controle_fusee_data
# Connecter un netcat AUTH CONTROLLER et observer TELEMETRY
```

**Phase 2 — Test des actuateurs JoyPi**

```bash
# Sur le JoyPi (après déploiement)
sudo ./bin-proto/hardware_actuators_test
# Vérifier visuellement : LED, buzzer, matrice, 7-seg, LCD
```

**Phase 3 — Test intégration complète**

Démarrer dans l'ordre : satellite_server → controle_fusee_data → controle_fusee → joypi_controller. Exécuter la séquence :
1. Appuyer BT1, saisir "123", valider → observer lancement
2. Observer dashboard animé + actuateurs
3. Appuyer BT3 → altitude sur 7-segments
4. Appuyer BT4 → température sur 7-segments
5. Appuyer BT5 × 3 → cycle pression complet
6. Appuyer BT6 × 3 → 3 mélodies
7. Appuyer BT2 → atterrissage d'urgence
8. Observer retour à l'état READY

---

## 11. Problèmes rencontrés et solutions

### 11.1 Greeting protocolaire — bug de désalignement

**Problème** : le serveur envoyait `OK CONNECTED\n` à chaque nouvelle connexion, avant que le client envoie `AUTH`. Le client, en attendant `AUTH_OK`, lisait `OK CONNECTED` et croyait être authentifié — puis le vrai `AUTH_OK` n'arrivait jamais (le client ne re-lisait pas). Résultat : toutes les commandes retournaient `AUTH_FAIL`.

**Solution** : supprimer le greeting. Dans un protocole requête-réponse, c'est le client qui initie. Leçon générale : ne jamais envoyer de données spontanées au début d'une connexion dans un protocole strictement requête-réponse.

### 11.2 Données push perdues lors d'une commande

**Problème** : lors de l'envoi de `CMD LU`, le serveur répondait `OK LAUNCH\n` et immédiatement broadcastait `EVENT LAUNCH\n`. TCP les coalesçait en un seul segment. Le client lisait les deux dans un seul `read()`, ne conservait que `OK LAUNCH`, et `EVENT LAUNCH` était silencieusement ignoré.

**Solution** : dans `send_cmd_recv()`, après extraction de la première ligne-réponse, le reste est inséré en tête du buffer `g_push_buf` pour traitement lors du prochain appel à `poll_satellite_push()`.

### 11.3 LCD : séquence d'initialisation manquante

**Problème** : `lcd_init()` configurait le MCP23017 en sortie (`IODIR_REG = 0x00`) mais omettait les 8 commandes d'initialisation HD44780. Le LCD restait silencieux ou affichait des artefacts.

**Solution** : reporter la séquence validée dans `hardware_actuators_test.c` vers `lcd_init()`. Nécessité d'une déclaration anticipée (`static void lcd_command(int data);`) car `lcd_command()` est définie après `lcd_init()`.

### 11.4 Injecteur bloqué après atterrissage automatique

**Problème** : quand le carburant atteignait 0 en vol, le serveur passait en LANDING (`CMD_EVENT LAND` → injector `gm.landing=true`). Quand altitude=0, le serveur passait en READY et envoyait `EVENT LANDED` aux controllers. Mais `CMD_EVENT RESUME` n'était pas envoyé à l'injector — donc `gm.landing` restait `true`, et l'altitude continuait de décroître en négatif indéfiniment.

**Solution** : ajouter `broadcast_injectors(clients, "CMD_EVENT RESUME")` dans `check_state_transitions()` lors du passage altitude=0 → READY.

### 11.5 Flamme active au sol (flame_size=3 à l'init)

**Problème** : `init_state()` initialisait `st->flame_size = 3`, rendant la fusée "en feu" dès l'affichage du dashboard, avant tout lancement.

**Solution** : `st->flame_size = 0`. Les animations de flammes démarrent uniquement lors de `LAUNCH_OK`.

### 11.6 Double-play mélodie EVENT MEL

**Observation** : lorsque BT6 déclenchait `CMD MEL N`, le JoyPi jouait la mélodie via `handle_response_mel()` (réponse directe), puis recevait l'`EVENT MEL N` broadcast et jouait une seconde fois. Ce "double-play" est un effet de bord acceptable dans un scénario mono-contrôleur, mais serait gênant en production.

**Note** : dans un scénario multi-contrôleurs, `EVENT MEL N` est intentionnel pour que les autres contrôleurs jouent aussi la mélodie. La solution propre serait de ne pas broadcaster `EVENT MEL` vers l'expéditeur, nécessitant de passer le fd du client dans le broadcast — complexification non justifiée pour ce projet.

---

## 12. Performances et métriques

### 12.1 Latences mesurées

| Action | Latence typique |
|--------|----------------|
| Auth TCP (LAN 192.168.x.x) | < 2 ms |
| CMD LU → OK LAUNCH | 3–8 ms |
| TELEMETRY push période | 2 000 ms |
| Scan clavier matriciel | 80 ms (poll) |
| Activation LED | < 1 ms |
| Buzzer mélodie A complète | ~1 600 ms |
| LCD affichage 2 lignes | ~15 ms |
| 7-segments affichage | < 2 ms |

### 12.2 Occupation CPU

En fonctionnement normal, `satellite_server` occupe moins de 1 % CPU (bloqué sur `select()` 99 % du temps). `joypi_controller` est similaire avec le scan clavier en `usleep(80ms)`.

### 12.3 Consommation mémoire

Chaque processus est extrêmement frugal : moins de 2 Mo RSS pour `satellite_server` (16 slots clients × 512 octets = 8 Ko de buffers clients), moins de 4 Mo pour `controle_fusee` (ncurses + structures d'état).

---

## 13. Sécurité et robustesse

### 13.1 Authentification

Le mot de passe de lancement `"123"` est hardcodé dans le contrôleur JoyPi. Dans un contexte de production, il serait stocké de manière sécurisée (hash bcrypt, stockage chiffré) et potentiellement modifiable. Pour ce projet pédagogique, la sécurité physique du JoyPi (accès restreint) est la protection principale.

### 13.2 Déni de service

Le serveur limite les connexions à `MAX_CLIENTS = 16`. Au-delà, les connexions excédentaires reçoivent `ERR SERVER_BUSY` et sont immédiatement fermées. Un attaquant local pourrait saturer les 16 slots avec des connexions sans authentification — dans un contexte de production, un timeout d'authentification (ex: fermer la connexion si `AUTH` n'arrive pas dans 2 secondes) serait nécessaire.

### 13.3 Validation des entrées

Les commandes INJECTOR sont parsées avec `sscanf(line + 4, "%31s %d", field, &val)`. Le format `%31s` limite la longueur du champ, évitant les débordements de buffer. Les valeurs numériques entières acceptent tout entier signé — un carburant négatif serait rejeté par la machine à états (`fuel <= 0` déclenche LANDING), mais des valeurs absurdes comme `altitude = -999999` pourraient être injectées sans validation.

### 13.4 Gestion des déconnexions

Lorsqu'un `read()` retourne 0 ou une erreur, le slot client est marqué pour suppression et nettoyé proprement (fermeture du socket, remise à zéro de la structure). `joypi_controller` implémente une reconnexion automatique avec backoff de 2 secondes. `controle_fusee_data` implémente également une reconnexion non-bloquante throttlée à 3 secondes.

---

## 14. Extensions possibles

### 14.1 Chiffrement TLS

Le protocole TCP est en clair. L'ajout de TLS (via OpenSSL ou mbedTLS pour ARM) protégerait les commandes de mission contre l'écoute ou la modification sur le réseau. mbedTLS est particulièrement adapté aux systèmes embarqués contraints en mémoire.

### 14.2 Journalisation structurée

Le serveur utilise un `log_line()` maison avec timestamp. L'intégration de `syslog()` ou d'un format structuré (JSON) faciliterait l'agrégation centralisée des logs (Graylog, Elasticsearch).

### 14.3 Interface web en parallèle

Un troisième type de client `VIEWER` (en lecture seule) recevant uniquement les pushs TELEMETRY permettrait d'afficher l'état de la mission dans un navigateur web via WebSocket. L'ajout d'un processus `websocket_gateway` faisant le pont TCP satellite ↔ WebSocket serait réalisable avec `libwebsockets`.

### 14.4 Persistance des données de mission

Les données de télémétrie pourraient être écrites dans une base de données légère (SQLite) pour analyse post-mission. Le modèle physique de `controle_fusee_data` génère des données suffisamment réalistes pour constituer une base d'apprentissage de modèles de détection d'anomalies.

### 14.5 Multi-JoyPi

L'architecture supporte nativement plusieurs CONTROLLER. Avec deux JoyPi connectés, on pourrait distribuer les responsabilités : un opérateur pilote la fusée (BT1/BT2), l'autre surveille les systèmes (BT3/BT4/BT5).

---

## 15. Conclusion

Ce projet démontre la faisabilité d'un système distribué temps réel complet — serveur TCP, simulation physique, interface graphique, pilotage hardware — développé en C standard sur Linux, avec cross-compilation ARM, en moins de 3 500 lignes de code au total.

Les principaux enseignements techniques sont :

1. **`select()` suffit** pour de nombreux cas serveurs temps réel sans recours aux threads, évitant la complexité de la synchronisation.

2. **TCP est un flux, jamais de messages** : tout développeur réseau doit maîtriser la gestion des buffers accumulants et des parsers ligne-par-ligne.

3. **Les async-signal-safe functions** sont une contrainte POSIX souvent ignorée qui peut causer des bugs subtils de corruption d'état.

4. **La validation hardware est irremplaçable** : plusieurs bugs (mapping HT16K33, séquence LCD) n'ont pu être découverts que sur le matériel réel, les simulations étant insuffisantes.

5. **Les FIFO nommés** constituent une interface inter-processus simple et puissante pour des pipelines de données locaux.

6. **La cross-compilation ARM** nécessite une discipline rigoureuse dans la gestion des bibliothèques et des chemins de compilation.

L'ensemble du projet, compilable en une commande (`make all`), produit des binaires prêts à déployer pour VM (x86) et JoyPi (ARM), illustrant qu'une architecture IoT complète et fonctionnelle peut reposer exclusivement sur des fondations logicielles libres et des standards POSIX.

---

## Références

[1] W. Richard Stevens, Bill Fenner, Andrew M. Rudoff — *UNIX Network Programming, Volume 1: The Sockets Networking API* — Addison-Wesley, 3rd edition, 2003

[2] The Open Group — *POSIX.1-2017 (IEEE Std 1003.1-2017)* — Base Specifications, Issue 7

[3] Gordon Henderson — *wiringPi : GPIO Interface library for the Raspberry Pi* — https://github.com/WiringPi/WiringPi

[4] Thomas E. Dickey — *ncurses HOWTO* — https://invisible-island.net/ncurses/

[5] Maxim Integrated — *MAX7219/MAX7221 Serially Interfaced, 8-Digit LED Display Drivers* — Datasheet Rev 4

[6] Holtek Semiconductor — *HT16K33 RAM Mapping 16×8 LED Controller Driver* — Datasheet V1.10

[7] Microchip Technology — *MCP23017 16-Bit I/O Expander with Serial Interface* — Datasheet DS20001952C

[8] Hitachi — *HD44780U (LCD-II) Dot Matrix Liquid Crystal Display Controller/Driver* — Datasheet, 1998

[9] ARM Ltd — *Procedure Call Standard for the ARM Architecture* — IHI0042F, 2012

[10] Linaro — *GNU Toolchain for Arm Processors* — https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

---

## Annexe A — Pins physiques JoyPi utilisées

| Périphérique | Interface | Pins physiques | Adresse |
|-------------|-----------|---------------|---------|
| LED rouge   | GPIO OUT  | 37            | —       |
| Buzzer piézo| GPIO OUT  | 12            | —       |
| Clavier (rangées) | GPIO IN | 13, 15, 29, 31 | — |
| Clavier (colonnes) | GPIO OUT | 33, 35, 37, 22 | — |
| Matrice 8×8 MAX7219 | SPI CE1 | 19(MOSI), 23(CLK), 26(CE1) | — |
| 7-segments HT16K33 | I2C | 3(SDA), 5(SCL) | 0x70 |
| LCD MCP23017 | I2C | 3(SDA), 5(SCL) | 0x21 |

*Note : pin 37 partagée entre LED et colonne 3 du clavier (lecture colonne temporairement remplacée par lecture direction)*

## Annexe B — Codes retour du protocole

| Code             | Signification                     |
|------------------|-----------------------------------|
| `AUTH_OK`        | Authentification réussie          |
| `AUTH_FAIL`      | Authentification refusée          |
| `OK LAUNCH`      | Lancement accepté                 |
| `OK LAND`        | Atterrissage d'urgence accepté    |
| `OK PRES_FAULT`  | Panne pression signalée           |
| `OK PRES_FIX`    | Correcteur pression activé        |
| `OK PRES_OK`     | Pression revenue nominale         |
| `OK MEL N`       | Mélodie N jouée                   |
| `DATA ALT v`     | Altitude courante (mètres)        |
| `DATA TEMP v`    | Température courante (°C)         |
| `FAIL ALREADY_LAUNCHED` | Fusée déjà en vol          |
| `FAIL NOT_READY` | Satellite pas en état READY       |
| `FAIL NOT_FLYING`| Fusée pas en vol                  |
| `FAIL BAD_MELODY`| Numéro mélodie invalide (1-3)     |
| `FAIL UNKNOWN_CMD`| Commande non reconnue            |
| `ERR SERVER_BUSY`| Serveur plein (16 clients max)    |

## Annexe C — Variables d'environnement de déploiement

```bash
# joypi_env.sh — à sourcer sur le JoyPi avant lancement
export LD_LIBRARY_PATH="${PWD}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

Cela permet aux binaires cross-compilés de trouver `libwiringPi.so.2` et `libncursesw.so.6` dans le répertoire `lib/` du bundle, sans nécessiter d'installation système.
