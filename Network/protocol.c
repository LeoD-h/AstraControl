/************************************************************
 * Projet      : Fusée
 * Fichier     : protocol.c
 * Description : Decodeur du protocole court vers commandes/dashboard et data.
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#include "protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *code;
    ProtoTarget target;
    const char *mapped;
    int needs_value;
} DictEntry;

static const DictEntry k_dict[] = {
    {"JK", PROTO_TARGET_NONE, "JOYPI_KEY", 1},
    {"TL", PROTO_TARGET_CMD, "TILT_LEFT", 0},
    {"TR", PROTO_TARGET_CMD, "TILT_RIGHT", 0},
    {"TS", PROTO_TARGET_CMD, "STRAIGHT", 0},
    {"LU", PROTO_TARGET_CMD, "LAUNCH", 0},
    {"LD", PROTO_TARGET_CMD, "LAND", 0},
    {"M1", PROTO_TARGET_CMD, "TEST_MELODY_1", 0},
    {"M2", PROTO_TARGET_CMD, "TEST_MELODY_2", 0},
    {"M3", PROTO_TARGET_CMD, "TEST_MELODY_3", 0},
    {"FX", PROTO_TARGET_CMD, "FIX_PROBLEM", 0},
    {"EX", PROTO_TARGET_CMD, "EXPLODE", 0},
    {"PZ", PROTO_TARGET_CMD, "PAUSE", 0},
    {"RS", PROTO_TARGET_CMD, "RESUME", 0},
    {"QT", PROTO_TARGET_CMD, "QUIT", 0},
    {"A1", PROTO_TARGET_CMD, "ALERT1", 0},
    {"A2", PROTO_TARGET_CMD, "ALERT2", 0},
    {"A3", PROTO_TARGET_CMD, "ALERT3", 0},
    {"AC", PROTO_TARGET_CMD, "CLEAR_ALERTS", 0},
    {"SV", PROTO_TARGET_CMD, "SIM_FLIGHT", 1},

    {"SF", PROTO_TARGET_DATA, "SET FUEL", 1},
    {"SS", PROTO_TARGET_DATA, "SET SPEED", 1},
    {"SP", PROTO_TARGET_DATA, "SET PRESSURE", 1},
    {"SA", PROTO_TARGET_DATA, "SET ALTITUDE", 1},
    {"ST", PROTO_TARGET_DATA, "SET TEMP", 1},
    {"SH", PROTO_TARGET_DATA, "SET THRUST", 1},
    {"PR", PROTO_TARGET_DATA, "PROBLEM", 1},
};

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[--n] = '\0';
    }

    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s, s + i, strlen(s + i) + 1);
    }
}

static void upper(char *s) {
    for (; *s; ++s) {
        *s = (char)toupper((unsigned char)*s);
    }
}

int protocol_decode_line(const char *input, ProtoMessage *out) {
    if (!input || !out) {
        return -1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "%s", input);
    trim(buf);
    if (buf[0] == '\0') {
        return -1;
    }

    char code[16] = {0};
    char value[64] = {0};
    int nb = sscanf(buf, "%15s %63[^\n]", code, value);
    upper(code);

    if (nb == 1 && strlen(code) > 2) {
        char merged_code[3] = {code[0], code[1], '\0'};
        if (!strcmp(merged_code, "SF") || !strcmp(merged_code, "SS") ||
            !strcmp(merged_code, "SP") || !strcmp(merged_code, "SA") ||
            !strcmp(merged_code, "ST") || !strcmp(merged_code, "SH") ||
            !strcmp(merged_code, "PR") || !strcmp(merged_code, "SV")) {
            snprintf(value, sizeof(value), "%s", code + 2);
            snprintf(code, sizeof(code), "%s", merged_code);
            nb = 2;
        }
    }

    for (size_t i = 0; i < sizeof(k_dict) / sizeof(k_dict[0]); ++i) {
        if (strcmp(code, k_dict[i].code) != 0) {
            continue;
        }

        out->target = k_dict[i].target;
        if (!k_dict[i].needs_value) {
            snprintf(out->line, sizeof(out->line), "%s", k_dict[i].mapped);
            return 0;
        }

        if (nb < 2) {
            return -2;
        }

        trim(value);
        if (strcmp(code, "JK") == 0) {
            snprintf(out->line, sizeof(out->line), "JOYPI_KEY %s", value);
            return 0;
        }
        if (strcmp(code, "PR") == 0) {
            upper(value);
            if (!strcmp(value, "0") || !strcmp(value, "OFF")) {
                snprintf(out->line, sizeof(out->line), "PROBLEM OFF");
                return 0;
            }
            snprintf(out->line, sizeof(out->line), "PROBLEM ON");
            return 0;
        }
        if (strcmp(code, "SV") == 0) {
            upper(value);
            if (!strcmp(value, "0") || !strcmp(value, "OFF")) {
                snprintf(out->line, sizeof(out->line), "SIM_FLIGHT OFF");
                return 0;
            }
            snprintf(out->line, sizeof(out->line), "SIM_FLIGHT ON");
            return 0;
        }

        snprintf(out->line, sizeof(out->line), "%s %s", k_dict[i].mapped, value);
        return 0;
    }

    upper(buf);
    if (!strncmp(buf, "SET ", 4) || !strncmp(buf, "PROBLEM", 7) || !strncmp(buf, "ALERT", 5)) {
        out->target = PROTO_TARGET_DATA;
        snprintf(out->line, sizeof(out->line), "%s", buf);
        return 0;
    }

    out->target = PROTO_TARGET_CMD;
    snprintf(out->line, sizeof(out->line), "%s", buf);
    return 0;
}
