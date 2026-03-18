# Overview

## Objectif
La librairie fournit une abstraction simple au-dessus des sockets TCP/UDP :
- la couche Session gere la creation, le bind, l'ecoute et la connexion.
- la couche Data Representation gere l'envoi/reception et la (de)serialisation.

Elle vise des echanges simples (messages C terminés par '\0') avec possibilite de
serialiser des structures applicatives via une fonction utilisateur.

## Architecture

### Couche Session (src/session.c)
- Cree une socket (STREAM ou DGRAM).
- Attache une adresse locale (bind).
- Met en ecoute (listen) pour les serveurs TCP.
- Accepte un client (accept) ou connecte un client vers un serveur (connect).

Les adresses locales/distantes sont conservees dans `socket_t`.

### Couche Data Representation (src/data.c)
- Convertit une requete/reponse applicative en buffer (serialisation).
- Envoie le buffer via `send` (STREAM) ou `sendto` (DGRAM).
- Recoit un buffer via `recv` (STREAM) ou `recvfrom` (DGRAM).
- Reconstruit l'objet applicatif (de-serialisation).

En DGRAM, l'adresse source est capturee lors d'une reception et stockee dans
`socket_t.addrDst`, ce qui permet de repondre au client sans ressaisir l'adresse.

## Flux typiques

### STREAM (TCP)
- Serveur : creerSocketEcoute -> accepterClt -> recevoir/envoyer.
- Client  : connecterClt2Srv -> envoyer/recevoir.

### DGRAM (UDP)
- Serveur : creerSocketAdr -> recevoir (remplit addrDst) -> envoyer(reponse).
- Client  : creerSocketAdr -> envoyer(IP, port) -> recevoir.
