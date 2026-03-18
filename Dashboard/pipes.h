/************************************************************
 * Projet      : Fusée
 * Fichier     : pipes.h
 * Description : API de lecture des pipes commandes/data pour le dashboard.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef PIPES_H
#define PIPES_H

#include <stddef.h>
#include "dashboard.h"

typedef void (*line_handler_fn)(char *line, RocketState *st);

int setup_pipe_reader(const char *path, int *keepalive_fd);
void poll_pipe_lines(int fd, line_handler_fn apply_fn, RocketState *st, char *pending, size_t *used);

#endif
