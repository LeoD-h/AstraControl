/************************************************************
 * Projet      : Fusée
 * Fichier     : useLibInet.c
 * Description : Exemple client/serveur d'utilisation de la librairie reseau.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include "data.h"
/*
*****************************************************************************************
 *	\noop		D E F I N I T I O N   DES   C O N S T A N T E S
 */
/**
 *	\def		IP_ANY
 *	\brief		Adresse IP par défaut du serveur
 */
#define IP_ANY		"0.0.0.0"
/**
 *	\def		IP_LOCAL
 *	\brief		Adresse IP par défaut du client
 */
#define IP_LOCAL	"127.0.0.1"
/**
 *	\def		PORT_SRV
 *	\brief		Numéro de port par défaut du serveur
 */
#define PORT_SRV	50000
/*
*****************************************************************************************
 *	\noop		D E C L A R A T I O N   DES   V A R I A B L E S    G L O B A L E S
 */
/**
 *	\var		progName
 *	\brief		Nom de l'exécutable : libnet nécessite cette variable qui pointe sur argv[0]
 */
char *progName;
/*
*****************************************************************************************
 *	\noop		I M P L E M E N T A T I O N   DES   F O N C T I O N S
 */
/**
 *	\fn			void client (char *adrIP, int port)
 *	\brief		lance un client STREAM connecté à l'adresse applicative adrIP:port 
 *	\param 		adrIP : adresse IP du serveur à connecter
 *	\param 		port : port du serveur à connecter
 */
void client (char *adrIP, int port) {
	socket_t sockAppel;	// socket d'appel
	buffer_t buff;

	printf("Client: connexion vers %s:%d...\n", adrIP, port);
	// Créer une connexion avec le serveur
	sockAppel = connecterClt2Srv (adrIP, port);
	printf("Client: connecte, envoi du message.\n");
	// Dialoguer avec le serveur
	envoyer(&sockAppel,"Hi", NULL);
	recevoir(&sockAppel,buff, NULL);
	printf("Client: reponse recue: %s\n", buff);
	PAUSE("Fin du client");
	// Fermer la socket d'appel
	CHECK(shutdown(sockAppel.fd, SHUT_WR),"-- PB shutdown() --");
}
/**
 *	\fn				void serveur (char *adrIP, int port)
 *	\brief			lance un serveur STREAM en écoute sur l'adresse applicative adrIP:port
 *	\param 			adrIP : adresse IP du serveur à metrre en écoute
 *	\param 			port : port d'écoute
 */
void serveur (char *adrIP, int port) {
	socket_t sockEcoute;	// socket d'écoute de demande de connexion d'un client
	socket_t sockDial;		// socket de dialogue avec un client
	buffer_t buff;
	
	printf("Serveur: ecoute sur %s:%d...\n", adrIP, port);
	// sockEcoute est une variable externe
	sockEcoute = creerSocketEcoute(adrIP, port);
	while(1)	// daemon !
	{
		// Accepter une connexion
		printf("Serveur: en attente d'un client...\n");
		sockDial = accepterClt(sockEcoute);
		printf("Serveur: client connecte.\n");
		// Dialoguer avec le client connecté
		recevoir(&sockDial,buff, NULL);
		printf("Serveur: message recu: %s\n", buff);
		envoyer(&sockDial,"Salut", NULL);
		printf("Serveur: reponse envoyee, fermeture.\n");
		// Fermer la socket d'écoute
		CHECK(close(sockDial.fd),"-- PB close() --");
	}
	// Fermer la socket d'écoute
	CHECK(close(sockEcoute.fd),"-- PB close() --");
}
#ifdef SERVEUR
int main (int argc, char *argv[]) {
	const char *adrIP = IP_ANY;
	int port = PORT_SRV;

	progName = basename(argv[0]);
	if (argc >= 2) {
		adrIP = argv[1];
	}
	if (argc >= 3) {
		port = atoi(argv[2]);
	}
	serveur((char *)adrIP, port);
	return 0;
}
#endif

#ifdef CLIENT
int main (int argc, char *argv[]) {
	const char *adrIP = IP_LOCAL;
	int port = PORT_SRV;

	progName = basename(argv[0]);
	if (argc >= 2) {
		adrIP = argv[1];
	}
	if (argc >= 3) {
		port = atoi(argv[2]);
	}
	client((char *)adrIP, port);
	return 0;
}
#endif
