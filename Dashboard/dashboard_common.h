/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard_common.h
 * Description : Utilitaires communs Dashboard (socket, temps, texte, FIFO).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#ifndef DASHBOARD_COMMON_H
#define DASHBOARD_COMMON_H

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

int setup_fifo_writer(const char *path);
int connect_socket_fd(const char *ip, int port);
void set_nonblocking(int fd);
int connect_viewer_socket(const char *ip, int port);
void trim_line(char *s);
int starts_with_prefix(const char *s, const char *prefix);
unsigned long long now_ms(void);
void send_line_with_reply(int fd, const char *line, bool socket_mode, char *last_reply, size_t reply_sz);

#endif
