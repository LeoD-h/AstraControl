# Serialisation

La couche Data prend une fonction de (de)serialisation optionnelle.

## Principe
- `envoyer(sock, quoi, serial, ...)`
  - si `serial != NULL`, elle doit remplir un `buffer_t` a partir de `quoi`.
  - sinon, `quoi` est traite comme une chaine C.

- `recevoir(sock, quoi, deSerial)`
  - si `deSerial != NULL`, elle doit convertir le buffer en structure cible.
  - sinon, `quoi` doit etre un buffer char.

## Exemple simple

```c
typedef struct {
    int id;
    char nom[32];
} personne_t;

void serialPersonne(generic src, generic dst) {
    personne_t *p = (personne_t *)src;
    buffer_t *b = (buffer_t *)dst;
    snprintf(*b, MAX_BUFFER, "%d:%s", p->id, p->nom);
}

void deSerialPersonne(generic src, generic dst) {
    buffer_t *b = (buffer_t *)src;
    personne_t *p = (personne_t *)dst;
    sscanf(*b, "%d:%31s", &p->id, p->nom);
}
```

## Usage

```c
personne_t p = {1, "alice"};
socket_t sock = connecterClt2Srv("127.0.0.1", 50010);

envoyer(&sock, &p, serialPersonne);

personne_t rep;
recevoir(&sock, &rep, deSerialPersonne);
```
