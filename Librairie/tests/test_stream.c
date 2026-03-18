/************************************************************
 * Projet      : Fusée
 * Fichier     : test_stream.c
 * Description : Tests unitaires des echanges stream (TCP-like).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "data.h"

char *progName = "test_stream";

static void fail(const char *msg) {
	fprintf(stderr, "Echec: %s\n", msg);
	exit(1);
}

int main(void) {
	const short port = 50010;
	pid_t pid = fork();
	CHECK(pid, "fork");

	if (pid == 0) {
		sleep(1);
		socket_t clt = connecterClt2Srv("127.0.0.1", port);
		envoyer(&clt, "ping", NULL);

		buffer_t buff;
		recevoir(&clt, buff, NULL);
		if (strcmp(buff, "pong") != 0) {
			fail("reponse client incorrecte");
		}
		close(clt.fd);
		return 0;
	}

	socket_t srv = creerSocketEcoute("127.0.0.1", port);
	socket_t dlg = accepterClt(srv);
	buffer_t buff;
	recevoir(&dlg, buff, NULL);
	if (strcmp(buff, "ping") != 0) {
		fail("message serveur incorrect");
	}
	envoyer(&dlg, "pong", NULL);

	close(dlg.fd);
	close(srv.fd);
	waitpid(pid, NULL, 0);
	printf("OK STREAM\n");
	return 0;
}
