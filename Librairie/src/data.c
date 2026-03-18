/************************************************************
 * Projet      : Fusée
 * Fichier     : data.c
 * Description : Primitives d'echange de donnees (envoi/recevoir/dialogue).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "data.h"

/*
*****************************************************************************************
 *	\noop		D E F I N I T I O N   DES   C O N S T A N T E S
 */
/**
 *	\def		RECV_FLAGS
 *	\brief		Flags à utiliser en réception
 */
#define RECV_FLAGS 	0
/**
 *	\def		SEND_FLAGS
 *	\brief		Flags à utiliser en émission
 */
#define SEND_FLAGS 	0
/*
*****************************************************************************************
 *	\noop		D E C L A R A T I O N   DES   V A R I A B L E S    G L O B A L E S
 */
/**
 *	\var		progName
 *	\brief		Nom de l'exécutable
 *	\note		Variable externe : à déclarer par l'utilisateur
 */
extern char *progName;

/*
*****************************************************************************************
 *	\noop		I M P L E M E N T A T I O N   DES   F O N C T I O N S
 *					M O D E    D G R A M
 */

static void envoyerMessDGRAM(socket_t *sockEch, char *msg, char *adrDest, short portDest) {
	adr2struct(&sockEch->addrDst, adrDest, portDest);
	ssize_t n = sendto(sockEch->fd, msg, strlen(msg) + 1, SEND_FLAGS,
		(struct sockaddr *)&sockEch->addrDst, sizeof(sockEch->addrDst));
	CHECK(n, "sendto");
}

static void recevoirMessDGRAM(socket_t *sockEch, char *msg, int msgSize) {
	socklen_t len = sizeof(sockEch->addrDst);
	ssize_t n = recvfrom(sockEch->fd, msg, (size_t)msgSize - 1, RECV_FLAGS,
		(struct sockaddr *)&sockEch->addrDst, &len);
	CHECK(n, "recvfrom");
	if (n == 0) {
		msg[0] = '\0';
		return;
	}
	msg[n] = '\0';
}

/*
*****************************************************************************************
 *	\noop		I M P L E M E N T A T I O N   DES   F O N C T I O N S
 *					M O D E    S T R E A M
 */

static void envoyerMessSTREAM(const socket_t *sockEch, char *msg) {
	ssize_t n = send(sockEch->fd, msg, strlen(msg) + 1, SEND_FLAGS);
	CHECK(n, "send");
}

static void recevoirMessSTREAM(const socket_t *sockEch, char *msg, int msgSize) {
	ssize_t n = recv(sockEch->fd, msg, (size_t)msgSize - 1, RECV_FLAGS);
	CHECK(n, "recv");
	if (n == 0) {
		msg[0] = '\0';
		return;
	}
	msg[n] = '\0';
}

/*
*****************************************************************************************
 *	\noop		I M P L E M E N T A T I O N   DES   F O N C T I O N S
 *					M O D E    D G R A M / S T R E A M
 */

void envoyer(socket_t *sockEch, generic quoi, pFct serial, ...) {
	buffer_t buff;	// buffer d'envoi

	// Serialiser dans buff la requête/réponse à envoyer
	if (serial != NULL) {
		serial(quoi, buff);
	} else {
		strcpy(buff, (char *)quoi);
	}

	// Envoi : appel de la fonction adéquate selon le mode
	if (sockEch->mode == SOCK_STREAM) {
		envoyerMessSTREAM(sockEch, buff);
	} else {
		va_list pArg;
		va_start(pArg, serial);
		envoyerMessDGRAM(sockEch, buff, va_arg(pArg, char *), (short)va_arg(pArg, int));
		va_end(pArg);
	}
}

void recevoir(socket_t *sockEch, generic quoi, pFct deSerial) {
	buffer_t buff;	// buffer de réception

	// Réception : appel de la fonction adéquate selon le mode
	if (sockEch->mode == SOCK_STREAM) {
		recevoirMessSTREAM(sockEch, buff, MAX_BUFFER);
	} else {
		recevoirMessDGRAM(sockEch, buff, MAX_BUFFER);
	}

	// Dé-serialiser la requête/réponse
	if (deSerial != NULL) {
		deSerial(buff, quoi);
	} else if (quoi != NULL) {
		strcpy((char *)quoi, buff);
	}
}

void dialogueClient(socket_t *sockEch, const char *msg, char *reponse, int repSize, ...) {
	buffer_t buff;

	if (sockEch->mode == SOCK_STREAM) {
		envoyer(sockEch, (generic)msg, NULL);
	} else {
		va_list pArg;
		va_start(pArg, repSize);
		envoyer(sockEch, (generic)msg, NULL, va_arg(pArg, char *), (short)va_arg(pArg, int));
		va_end(pArg);
	}

	recevoir(sockEch, buff, NULL);
	if (reponse != NULL && repSize > 0) {
		strncpy(reponse, buff, (size_t)repSize - 1);
		reponse[repSize - 1] = '\0';
	}
}

void dialogueServeur(socket_t *sockEch, char *msgRecu, int msgSize, const char *rep) {
	buffer_t buff;

	recevoir(sockEch, buff, NULL);
	if (msgRecu != NULL && msgSize > 0) {
		strncpy(msgRecu, buff, (size_t)msgSize - 1);
		msgRecu[msgSize - 1] = '\0';
	}

	if (sockEch->mode == SOCK_STREAM) {
		envoyer(sockEch, (generic)rep, NULL);
	} else {
		char adr[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &sockEch->addrDst.sin_addr, adr, sizeof(adr)) == NULL) {
			CHECK(-1, "inet_ntop");
		}
		short port = ntohs(sockEch->addrDst.sin_port);
		envoyer(sockEch, (generic)rep, NULL, adr, port);
	}
}
