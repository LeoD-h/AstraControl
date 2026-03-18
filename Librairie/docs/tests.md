# Tests

Les tests d'integration se trouvent dans `tests/`.

## test_stream
- Verifie le flux TCP basique (ping/pong).
- Le serveur et le client sont dans le meme binaire via `fork()`.

## test_dgram
- Verifie le flux UDP basique (ping/pong).
- Le serveur et le client sont dans le meme binaire via `fork()`.

## Execution

```sh
make tests
```

Ou individuellement :

```sh
make build/test_stream
make build/test_dgram
./build/test_stream
./build/test_dgram
```
