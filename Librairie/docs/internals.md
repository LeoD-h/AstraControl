# Internals

Ce document explique comment le code fonctionne en interne.

## session.c

### adr2struct
- Initialise une structure `sockaddr_in` a zero.
- Remplit la famille (`AF_INET`), le port (htons) et l'adresse (inet_pton).
- Arrete le programme si l'IP est invalide.

### creerSocket
- Cree une socket IPv4 avec `socket(AF_INET, mode, 0)`.
- Stocke le fd dans `socket_t` et garde le mode (STREAM/DGRAM).

### creerSocketAdr
- Cree une socket avec `creerSocket`.
- Convertit l'IP/port en `sockaddr_in` via `adr2struct`.
- Active `SO_REUSEADDR`.
- Fait un `bind` sur l'adresse locale.

### creerSocketEcoute
- Cree une socket STREAM liee a une adresse locale.
- Lance `listen` (backlog 5).

### accepterClt
- Accepte une connexion entrante sur une socket d'ecoute.
- Remplit `addrDst` avec l'adresse du client.

### connecterClt2Srv
- Cree une socket client STREAM.
- Remplit `addrDst` avec l'adresse serveur.
- Fait un `connect`.
- Recupere l'adresse locale via `getsockname`.

## data.c

### envoyerMessDGRAM / recevoirMessDGRAM
- `envoyerMessDGRAM` construit `addrDst` puis envoie avec `sendto`.
- `recevoirMessDGRAM` recoit avec `recvfrom` et stocke l'adresse source
  dans `socket_t.addrDst`.

### envoyerMessSTREAM / recevoirMessSTREAM
- Utilisent `send` / `recv` sur une socket connectee.
- Le buffer est termine par '\0' pour un usage en chaine C.

### envoyer
- Construit un `buffer_t`.
- Si `serial != NULL`, la fonction utilisateur remplit le buffer.
- Sinon, `quoi` est copie directement.
- En STREAM : envoi via `send`.
- En DGRAM : envoi via `sendto` avec IP/port additionnels.

### recevoir
- Recoit dans un buffer via `recv` ou `recvfrom`.
- Si `deSerial != NULL`, la fonction utilisateur reconstruit l'objet.
- Sinon, copie la chaine dans `quoi`.

### dialogueClient
- Envoi puis reception.
- En DGRAM : prend l'IP et le port du serveur en arguments variables.

### dialogueServeur
- Reception puis envoi de reponse.
- En DGRAM : repond automatiquement a l'adresse du dernier client
  stockee dans `sockEch->addrDst`.
