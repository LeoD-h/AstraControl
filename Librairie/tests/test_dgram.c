/************************************************************
 * Projet      : Fusée
 * Fichier     : test_dgram.c
 * Description : Tests unitaires des echanges datagramme (UDP-like).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "data.h"

char *progName = "test_dgram";

static void fail(const char *msg) {
	fprintf(stderr, "Echec: %s\n", msg);
	exit(1);
}

int main(void) {
	const short port = 50011;
	pid_t pid = fork();
	CHECK(pid, "fork");

	if (pid == 0) {
		sleep(1);
		socket_t clt = creerSocketAdr(SOCK_DGRAM, "127.0.0.1", 0);
		envoyer(&clt, "ping", NULL, "127.0.0.1", port);

		buffer_t buff;
		recevoir(&clt, buff, NULL);
		if (strcmp(buff, "pong") != 0) {
			fail("reponse client incorrecte");
		}
		close(clt.fd);
		return 0;
	}

	socket_t srv = creerSocketAdr(SOCK_DGRAM, "127.0.0.1", port);
	buffer_t buff;
	recevoir(&srv, buff, NULL);
	if (strcmp(buff, "ping") != 0) {
		fail("message serveur incorrect");
	}

	char adr[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &srv.addrDst.sin_addr, adr, sizeof(adr)) == NULL) {
		fail("conversion adresse client");
	}
	short portDst = ntohs(srv.addrDst.sin_port);
	envoyer(&srv, "pong", NULL, adr, portDst);

	close(srv.fd);
	waitpid(pid, NULL, 0);
	printf("OK DGRAM\n");
	return 0;
}
