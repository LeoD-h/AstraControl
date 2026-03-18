/************************************************************
 * Projet      : Fusée
 * Fichier     : protocol.h
 * Description : Definitions du protocole court (types et fonction de decode).
 *
 * Auteurs     : Léo, Inès, Juliann
 * Date        : 12/03/2026
 * Version     : 1.0
 ************************************************************/
#ifndef SOCKET_PROTOCOL_H
#define SOCKET_PROTOCOL_H

#include <stddef.h>

typedef enum {
    PROTO_TARGET_NONE = 0,
    PROTO_TARGET_CMD,
    PROTO_TARGET_DATA
} ProtoTarget;

typedef struct {
    ProtoTarget target;
    char line[128];
} ProtoMessage;

int protocol_decode_line(const char *input, ProtoMessage *out);

#endif
