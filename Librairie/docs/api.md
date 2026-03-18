# API Reference

## Types
- `socket_t` : structure qui encapsule le fd, le mode et les adresses locale/distante.
- `buffer_t` : `char[MAX_BUFFER]` pour emission/reception.
- `generic` : `void *` pour passer des structures generiques.
- `pFct` : pointeur sur fonction (generic, generic).

## Macros
- `CHECK(sts, msg)` : termine le programme si `sts == -1`.
- `PAUSE(msg)` : affiche un message et attend entree clavier.

## Session (include/session.h)

- `void adr2struct(struct sockaddr_in *addr, char *adrIP, short port)`
  - Convertit IP/port en `sockaddr_in`.

- `socket_t creerSocket(int mode)`
  - Cree une socket STREAM ou DGRAM.

- `socket_t creerSocketAdr(int mode, char *adrIP, short port)`
  - Cree une socket et la lie a l'adresse locale fournie.

- `socket_t creerSocketEcoute(char *adrIP, short port)`
  - Cree une socket TCP d'ecoute (bind + listen).

- `socket_t accepterClt(const socket_t sockEcoute)`
  - Accepte une connexion entrante (TCP).

- `socket_t connecterClt2Srv(char *adrIP, short port)`
  - Cree une socket client TCP et se connecte.

## Data (include/data.h)

- `void envoyer(socket_t *sockEch, generic quoi, pFct serial, ...)`
  - Serialise `quoi` dans un buffer puis envoie.
  - `serial == NULL` => `quoi` est une chaine C.
  - DGRAM : il faut fournir en plus `adrIP` et `port`.

- `void recevoir(socket_t *sockEch, generic quoi, pFct deSerial)`
  - Recoit un buffer puis de-serialise.
  - `deSerial == NULL` => `quoi` est une chaine C.

- `void dialogueClient(socket_t *sockEch, const char *msg, char *reponse, int repSize, ...)`
  - Envoi puis reception en une seule fonction.
  - DGRAM : ajouter `adrIP` et `port` du serveur.

- `void dialogueServeur(socket_t *sockEch, char *msgRecu, int msgSize, const char *rep)`
  - Reception puis envoi de reponse.
