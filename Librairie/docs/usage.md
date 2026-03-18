# Utilisation

## Prerequis
- Compiler la bibliotheque via `make`.
- Inclure les headers depuis `include/`.
- Lier contre `build/libinet.a`.

## Exemple STREAM (TCP)

### Serveur
```c
#include <arpa/inet.h>
#include "session.h"
#include "data.h"

int main(void) {
    socket_t srv = creerSocketEcoute("127.0.0.1", 50010);
    socket_t clt = accepterClt(srv);

    buffer_t msg;
    recevoir(&clt, msg, NULL);
    envoyer(&clt, "pong", NULL);

    close(clt.fd);
    close(srv.fd);
    return 0;
}
```

### Client
```c
#include "session.h"
#include "data.h"

int main(void) {
    socket_t clt = connecterClt2Srv("127.0.0.1", 50010);
    envoyer(&clt, "ping", NULL);

    buffer_t rep;
    recevoir(&clt, rep, NULL);

    close(clt.fd);
    return 0;
}
```

## Exemple DGRAM (UDP)

### Serveur
```c
#include "session.h"
#include "data.h"

int main(void) {
    socket_t srv = creerSocketAdr(SOCK_DGRAM, "127.0.0.1", 50011);

    buffer_t msg;
    recevoir(&srv, msg, NULL); // remplit addrDst

    // repond a l'adresse/port du dernier client
    char adr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &srv.addrDst.sin_addr, adr, sizeof(adr));
    short port = ntohs(srv.addrDst.sin_port);
    envoyer(&srv, "pong", NULL, adr, port);

    close(srv.fd);
    return 0;
}
```

### Client
```c
#include "session.h"
#include "data.h"

int main(void) {
    socket_t clt = creerSocketAdr(SOCK_DGRAM, "127.0.0.1", 0);
    envoyer(&clt, "ping", NULL, "127.0.0.1", 50011);

    buffer_t rep;
    recevoir(&clt, rep, NULL);

    close(clt.fd);
    return 0;
}
```

## Compilation d'une appli utilisateur

```sh
gcc -Wall -Wextra -Iinclude -c mon_app.c -o build/mon_app.o
gcc -Wall -Wextra -o build/mon_app build/mon_app.o build/libinet.a
```
