/************************************************************
 * Projet      : Fusée
 * Fichier     : test_all.c
 * Description : Tests d'integration protocole + bridge socket + pipes.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../Network/protocol.h"

#define TEST_PORT 5566
#define CMD_PIPE "/tmp/rocket_cmd.pipe"
#define DATA_PIPE "/tmp/rocket_data.pipe"

static int fail_count = 0;

static void assert_true(bool cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "[FAIL] %s\n", msg);
        fail_count++;
    } else {
        fprintf(stdout, "[OK] %s\n", msg);
    }
}

static int read_line_timeout(int fd, char *out, size_t out_sz, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        return -1;
    }

    ssize_t nr = read(fd, out, out_sz - 1);
    if (nr <= 0) {
        return -1;
    }
    out[nr] = '\0';
    return 0;
}

static int connect_client(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void protocol_expect(const char *raw, ProtoTarget target, const char *line) {
    ProtoMessage m;
    int rc = protocol_decode_line(raw, &m);
    char msg[160];

    snprintf(msg, sizeof(msg), "decode %s", raw);
    assert_true(rc == 0, msg);

    snprintf(msg, sizeof(msg), "target %s", raw);
    assert_true(m.target == target, msg);

    snprintf(msg, sizeof(msg), "mapping %s -> %s", raw, line);
    assert_true(strcmp(m.line, line) == 0, msg);
}

static void test_protocol_dict(void) {
    protocol_expect("SF2", PROTO_TARGET_DATA, "SET FUEL 2");
    protocol_expect("SS 1200", PROTO_TARGET_DATA, "SET SPEED 1200");
    protocol_expect("SP980", PROTO_TARGET_DATA, "SET PRESSURE 980");
    protocol_expect("SA 300", PROTO_TARGET_DATA, "SET ALTITUDE 300");
    protocol_expect("ST45", PROTO_TARGET_DATA, "SET TEMP 45");
    protocol_expect("SH 1600", PROTO_TARGET_DATA, "SET THRUST 1600");

    protocol_expect("TL", PROTO_TARGET_CMD, "TILT_LEFT");
    protocol_expect("TR", PROTO_TARGET_CMD, "TILT_RIGHT");
    protocol_expect("TS", PROTO_TARGET_CMD, "STRAIGHT");
    protocol_expect("LU", PROTO_TARGET_CMD, "LAUNCH");
    protocol_expect("LD", PROTO_TARGET_CMD, "LAND");
    protocol_expect("M1", PROTO_TARGET_CMD, "TEST_MELODY_1");
    protocol_expect("M2", PROTO_TARGET_CMD, "TEST_MELODY_2");
    protocol_expect("M3", PROTO_TARGET_CMD, "TEST_MELODY_3");
    protocol_expect("FX", PROTO_TARGET_CMD, "FIX_PROBLEM");
    protocol_expect("EX", PROTO_TARGET_CMD, "EXPLODE");
    protocol_expect("PZ", PROTO_TARGET_CMD, "PAUSE");
    protocol_expect("RS", PROTO_TARGET_CMD, "RESUME");
    protocol_expect("A1", PROTO_TARGET_CMD, "ALERT1");
    protocol_expect("A2", PROTO_TARGET_CMD, "ALERT2");
    protocol_expect("A3", PROTO_TARGET_CMD, "ALERT3");
    protocol_expect("AC", PROTO_TARGET_CMD, "CLEAR_ALERTS");
    protocol_expect("QT", PROTO_TARGET_CMD, "QUIT");

    protocol_expect("PR 0", PROTO_TARGET_DATA, "PROBLEM OFF");
    protocol_expect("PR 1", PROTO_TARGET_DATA, "PROBLEM ON");
    protocol_expect("SV0", PROTO_TARGET_CMD, "SIM_FLIGHT OFF");
    protocol_expect("SV 1", PROTO_TARGET_CMD, "SIM_FLIGHT ON");
}

static void send_and_expect(int cfd, int expected_pipe_fd, const char *raw, const char *mapped, const char *label) {
    char buf[256];
    char line[128];
    snprintf(line, sizeof(line), "%s\n", raw);
    ssize_t nw = write(cfd, line, strlen(line));
    (void)nw;

    int got_ack = read_line_timeout(cfd, buf, sizeof(buf), 1000) == 0;
    char msg[160];
    snprintf(msg, sizeof(msg), "ack %s", label);
    assert_true(got_ack, msg);

    int got_pipe = read_line_timeout(expected_pipe_fd, buf, sizeof(buf), 1000) == 0;
    snprintf(msg, sizeof(msg), "pipe receives %s", label);
    assert_true(got_pipe, msg);

    if (got_pipe) {
        snprintf(msg, sizeof(msg), "mapped value %s", label);
        assert_true(strstr(buf, mapped) != NULL, msg);
    }
}

static void test_socket_bridge(void) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("./bin-proto/socket_bridge_server", "./bin-proto/socket_bridge_server", "5566", (char *)NULL);
        _exit(127);
    }

    sleep(1);

    int cmd_rd = open(CMD_PIPE, O_RDONLY | O_NONBLOCK);
    int data_rd = open(DATA_PIPE, O_RDONLY | O_NONBLOCK);
    assert_true(cmd_rd >= 0, "open cmd pipe reader");
    assert_true(data_rd >= 0, "open data pipe reader");

    int cfd = connect_client(TEST_PORT);
    assert_true(cfd >= 0, "connect socket client to bridge");
    int vfd = connect_client(TEST_PORT);
    assert_true(vfd >= 0, "connect socket viewer to bridge");

    char buf[256];
    if (cfd >= 0) {
        (void)read_line_timeout(cfd, buf, sizeof(buf), 1000); /* OK CONNECTED */
        if (vfd >= 0) {
            (void)read_line_timeout(vfd, buf, sizeof(buf), 1000); /* OK CONNECTED */
            ssize_t vw = write(vfd, "VIEWER_ON\n", 10);
            (void)vw;
            int got_vack = read_line_timeout(vfd, buf, sizeof(buf), 1000) == 0;
            assert_true(got_vack, "viewer mode ack");
        }

        send_and_expect(cfd, data_rd, "SF 42", "SET FUEL 42", "SF");
        if (vfd >= 0) {
            int got_mirror = read_line_timeout(vfd, buf, sizeof(buf), 1000) == 0;
            assert_true(got_mirror, "viewer mirror receives SF");
            if (got_mirror) {
                assert_true(strstr(buf, "mapped='SET FUEL 42'") != NULL, "viewer SF mirror payload");
            }
        }
        send_and_expect(cfd, data_rd, "PR 1", "PROBLEM ON", "PR");
        send_and_expect(cfd, cmd_rd, "TL", "TILT_LEFT", "TL");
        send_and_expect(cfd, cmd_rd, "SV 1", "SIM_FLIGHT ON", "SV");
        send_and_expect(cfd, cmd_rd, "LU", "LAUNCH", "LU");
        send_and_expect(cfd, cmd_rd, "LD", "LAND", "LD");

        ssize_t wq = write(cfd, "QT\n", 3);
        (void)wq;
        close(cfd);
        if (vfd >= 0) close(vfd);
    }

    if (cmd_rd >= 0) close(cmd_rd);
    if (data_rd >= 0) close(data_rd);

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

int main(void) {
    test_protocol_dict();
    test_socket_bridge();

    if (fail_count > 0) {
        fprintf(stderr, "\nTests finished with %d failure(s).\n", fail_count);
        return 1;
    }

    printf("\nAll tests passed.\n");
    return 0;
}
