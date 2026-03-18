/************************************************************
 * Projet      : Fusée
 * Fichier     : dashboard_common.c
 * Description : Implementation des utilitaires communs Dashboard.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.1
 ************************************************************/
#include "dashboard_common.h"

int setup_fifo_writer(const char *path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        return -1;
    }
    return open(path, O_WRONLY | O_NONBLOCK);
}

int connect_socket_fd(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

int connect_viewer_socket(const char *ip, int port) {
    int fd = connect_socket_fd(ip, port);
    if (fd < 0) {
        return -1;
    }

    set_nonblocking(fd);
    if (write(fd, "VIEWER_ON\n", 10) < 0) { /* best-effort, erreur ignoree */ }
    return fd;
}

void trim_line(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[--n] = '\0';
    }
}

int starts_with_prefix(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

unsigned long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long long)tv.tv_sec * 1000ULL) + (unsigned long long)(tv.tv_usec / 1000ULL);
}

void send_line_with_reply(int fd, const char *line, bool socket_mode, char *last_reply, size_t reply_sz) {
    char out[128];
    snprintf(out, sizeof(out), "%s\n", line);
    if (write(fd, out, strlen(out)) < 0) { /* best-effort, erreur ignoree */ }

    if (socket_mode) {
        char reply[128];
        ssize_t nr = read(fd, reply, sizeof(reply) - 1);
        if (nr > 0) {
            reply[nr] = '\0';
            snprintf(last_reply, reply_sz, "%s", reply);
            return;
        }
        snprintf(last_reply, reply_sz, "(no server reply)");
    }
}
