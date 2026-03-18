/************************************************************
 * Projet      : Fusée
 * Fichier     : session.c
 * Description : Primitives de creation/configuration de sessions sockets.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include "session.h"
#include <arpa/inet.h>
#include <string.h>

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/**
 *	\fn			void adr2struct (struct sockaddr_in *addr, char *adrIP, short port)
 *	\brief		Convertit une adresse IP/port en structure sockaddr_in
 *	\param		addr : structure d'adressage BSD a initialiser
 *	\param		adrIP : adresse IP en notation point-decimal
 *	\param		port : port associe a l'adresse
 *	\result		parametre *addr modifie
 */
void adr2struct(struct sockaddr_in *addr, char *adrIP, short port) {
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons((uint16_t)port);
	int sts = inet_pton(AF_INET, adrIP, &addr->sin_addr);
	if (sts <= 0) {
		perror("inet_pton");
		exit(-1);
	}
}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/**
 *	\fn			socket_t creerSocket (int mode)
 *	\brief		Cree une socket en mode STREAM ou DGRAM
 *	\param		mode : type de socket (SOCK_STREAM ou SOCK_DGRAM)
 *	\result		socket initialisee
 */
socket_t creerSocket(int mode) {
	socket_t sock;
	memset(&sock, 0, sizeof(sock));
	sock.mode = mode;
	sock.fd = socket(AF_INET, mode, 0); //Protocol 0 choisit automatiquement TCP/UDP
	CHECK(sock.fd, "socket");
	return sock;
}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/**
 *	\fn			socket_t creerSocketAdr (int mode, char *adrIP, short port)
 *	\brief		Cree une socket et l'attache a une adresse
 *	\param		mode : type de socket (SOCK_STREAM ou SOCK_DGRAM)
 *	\param		adrIP : adresse IP locale
 *	\param		port : port local
 *	\result		socket initialisee et liee
 */
socket_t creerSocketAdr(int mode, char *adrIP, short port) {
	socket_t sock = creerSocket(mode);
	adr2struct(&sock.addrLoc, adrIP, port);

	int opt = 1;
	CHECK(setsockopt(sock.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)), "setsockopt");
	CHECK(bind(sock.fd, (struct sockaddr *)&sock.addrLoc, sizeof(sock.addrLoc)), "bind");
	return sock;
}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/**
 *	\fn			socket_t creerSocketEcoute (char *adrIP, short port)
 *	\brief		Cree une socket TCP d'ecoute
 *	\param		adrIP : adresse IP locale
 *	\param		port : port TCP local
 *	\result		socket d'ecoute prete a accepter des connexions
 */
socket_t creerSocketEcoute(char *adrIP, short port) {
	socket_t sock = creerSocketAdr(SOCK_STREAM, adrIP, port);
	CHECK(listen(sock.fd, 5), "listen");
	return sock;
}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/**
 *	\fn			socket_t accepterClt (const socket_t sockEcoute)
 *	\brief		Accepte une connexion entrante
 *	\param		sockEcoute : socket d'ecoute
 *	\result		socket connectee au client
 */
socket_t accepterClt(const socket_t sockEcoute) {
	socket_t sock;
	memset(&sock, 0, sizeof(sock));
	sock.mode = SOCK_STREAM;

	socklen_t addrLen = sizeof(sock.addrDst);
	sock.fd = accept(sockEcoute.fd, (struct sockaddr *)&sock.addrDst, &addrLen);
	CHECK(sock.fd, "accept");
	sock.addrLoc = sockEcoute.addrLoc;
	return sock;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/**
 *	\fn			socket_t connecterClt2Srv (char *adrIP, short port)
 *	\brief		Cree une socket client et se connecte au serveur
 *	\param		adrIP : adresse IP du serveur
 *	\param		port : port TCP du serveur
 *	\result		socket connectee au serveur
 */
socket_t connecterClt2Srv(char *adrIP, short port) {
	socket_t sock = creerSocket(SOCK_STREAM);
	adr2struct(&sock.addrDst, adrIP, port);
	CHECK(connect(sock.fd, (struct sockaddr *)&sock.addrDst, sizeof(sock.addrDst)), "connect");

	socklen_t len = sizeof(sock.addrLoc);
	CHECK(getsockname(sock.fd, (struct sockaddr *)&sock.addrLoc, &len), "getsockname");
	return sock;
}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
