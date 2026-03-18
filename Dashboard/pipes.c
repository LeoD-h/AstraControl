/************************************************************
 * Projet      : Fusée
 * Fichier     : pipes.c
 * Description : Lecture non bloquante des pipes commandes/data pour le dashboard.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include "pipes.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int setup_pipe_reader(const char *path, int *keepalive_fd) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        return -1;
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    *keepalive_fd = open(path, O_WRONLY | O_NONBLOCK);
    return fd;
}

void poll_pipe_lines(int fd, line_handler_fn apply_fn, RocketState *st, char *pending, size_t *used) {
    char buf[128];

    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';

        if (*used + (size_t)n >= 512) {
            /* Buffer plein : vider pour éviter overflow (off-by-one corrigé : >=512) */
            *used = 0;
            memset(pending, 0, 512);
        }
        memcpy(pending + *used, buf, (size_t)n);
        *used += (size_t)n;
        pending[*used] = '\0';
    }

    if (*used == 0) {
        return;
    }

    char *start = pending;
    char *nl = NULL;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        apply_fn(start, st);
        start = nl + 1;
    }

    size_t remain = strlen(start);
    memmove(pending, start, remain + 1);
    *used = remain;
}
