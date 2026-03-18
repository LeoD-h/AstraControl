/************************************************************
 * Projet      : Fusée
 * Fichier     : session.h
 * Description : API de session reseau (creation sockets, connexion, ecoute).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef SESSION_H
#define SESSION_H
/*
*****************************************************************************************
 *	\noop		I N C L U D E S   S P E C I F I Q U E S
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
/*
*****************************************************************************************
 *	\noop		D E F I N I T I O N   DES   M A C R O S
 */
/**
 *	\def		CHECK(sts, msg)
 *	\brief		Macro-fonction qui vÃ©rifie que sts est Ã©gal -1 (cas d'erreur : sts==-1) 
 *				En cas d'erreur, il y a affichage du message adÃ©quat et fin d'exÃ©cution  
 */
#define CHECK(sts, msg) if ((sts)==-1) {perror(msg); exit(-1);}
/**
 *	\def		PAUSE(msg)
 *	\brief		Macro-fonction qui affiche msg et attend une entrÃ©e clavier  
 */
#define PAUSE(msg)	printf("%s [Appuyez sur entrÃ©e pour continuer]", msg); getchar();
/*
*****************************************************************************************
 *	\noop		S T R C T U R E S   DE   D O N N E E S
 */
/**
 *	\struct		socket
 *	\brief		DÃ©finition de la structure de donnÃ©es socket
 *	\note		Ce type est composÃ© du fd de la socket, du mode (connectÃ©/non)
 *				et des adresses applicatives (locale/distante)
 */
struct socket {
	int fd;							/**< numÃ©ro de la socket crÃ©Ã©e			*/
	int mode;						/**< mode connectÃ©/non : STREAM/DGRAM	*/
	struct sockaddr_in addrLoc;		/**< adresse locale de la socket 		*/
	struct sockaddr_in addrDst;		/**< adresse distante de la socket 		*/
};
/**
 *	\typedef	socket_t
 *	\brief		DÃ©finition du type de donnÃ©es socket_t
 */
typedef struct socket socket_t; 
/*
*****************************************************************************************
 *	\noop		P R O T O T Y P E S   DES   F O N C T I O N S
 */
/**
 *	\fn			void adr2struct (struct sockaddr_in *addr, char *adrIP, short port)
 *	\brief		Transformer une adresse au format humain en structure SocketBSD
 *	\param		addr : structure d'adressage BSD d'une socket INET
 *	\param		adrIP : adresse IP de la socket crÃ©Ã©e
 *	\param		port : port de la socket crÃ©Ã©e
 *	\note		Le domaine dÃ©pend du mode choisi (TCP/UDP)
 *	\result		paramÃ¨tre *adr modifiÃ©
 */
void adr2struct (struct sockaddr_in *addr, char *adrIP, short port);
/**
 *	\fn			socket_t creerSocket (int mode)
 *	\brief		CrÃ©ation d'une socket de type DGRAM/STREAM
 *	\param		mode : mode connectÃ© (STREAM) ou non (DGRAM)
 *	\result		socket crÃ©Ã©e selon le mode choisi
 */
socket_t creerSocket (int mode);
/**
 *	\fn			socket_t creerSocketAdr (int mode, char *adrIP, short port)
 *	\brief		CrÃ©ation d'une socket de type DGRAM/STREAM
 *	\param		mode : adresse IP de la socket crÃ©Ã©e
 *	\param		adrIP : adresse IP de la socket crÃ©Ã©e
 *	\param		port : port de la socket crÃ©Ã©e
 *	\result		socket crÃ©Ã©e dans le domaine choisi avec l'adressage fourni
 */
socket_t creerSocketAdr (int mode, char *adrIP, short port);
/**
 *	\fn			creerSocketEcoute (char *adrIP, short port)
 *	\brief		CrÃ©ation d'une socket d'Ã©coute avec l'adressage fourni en paramÃ¨tre
 *	\param		adrIP : adresse IP du serveur Ã  mettre en Ã©coute
 *	\param		port : port TCP du serveur Ã  mettre en Ã©coute
 *	\result		socket crÃ©Ã©e avec l'adressage fourni en paramÃ¨tre et dans un Ã©tat d'Ã©coute
 *	\note		Le domaine est nÃ©cessairement STREAM
 */
socket_t creerSocketEcoute (char *adrIP, short port);
/**
 *	\fn			socket_t accepterClt (const socket_t sockEcoute)
 *	\brief		Acceptation d'une demande de connexion d'un client
 *	\param		sockEcoute : socket d'Ã©coute pour rÃ©ception de la demande
 *	\result		socket (dialogue) connectÃ©e par le serveur avec un client
 */
socket_t accepterClt (const socket_t sockEcoute);
/**
 *	\fn			socket_t connecterClt2Srv (char *adrIP, short port)
 *	\brief		CrÃ©taion d'une socket d'appel et connexion au seveur dont
 *				l'adressage est fourni en paramÃ¨tre
 *	\param		adrIP : adresse IP du serveur Ã  connecter
 *	\param		port : port TCP du serveur Ã  connecter
 *	\result		socket connectÃ©e au serveur fourni en paramÃ¨tre
 */
socket_t connecterClt2Srv (char *adrIP, short port);

#endif /* SESSION_H */
