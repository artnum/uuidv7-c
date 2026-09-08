# Single header library for UUIDv7

## UUIDv7 with IPC accross processes.

This implementation generate RFC 9562 UUIDv7. Sequence is sync accross
multiple process with semaphore and shared memory, each process register
itself and the last one to exit properly will unlink named semaphore and
shared memory (of course if one crashes, the refcount won't be decreased
and it will be lying around).

Passage of time use clock_gettime: CLOCK_MONOTONIC and a relative view of
the time, meaning that if one process becomes somehow much in advance than
others, the others will take that process timestamp as their wall clock. We
make sure that the time is going forward and that the biggest timestamp of
the sequence become the time of the process.

Based on https://github.com/artnum/snowflake-c/, adapted for UUIDv7.

## Documentation

Doxgen documentation within the source code. Running `doxygen uuidv7.h` give
you usable documentation.

### 3 functions API

In one source file, define `UUIDV7_IMPLEMENTATION` before including
`uuidv7.h`.

 - Open a context to generate uuidv7 id with
```c
bool     uuidv7_open  (uuidv7_ctx_t *ctx, uint16_t node_id);
```

 - Close a context with
```c
void     uuidv7_close (uuidv7_ctx_t *ctx);
```

 - Get an id with
```c
uuidv7_t uuidv7_get   (uuidv7_ctx_t *ctx);
```

For a working example, see the file `example.c`.

## License

Public domain.
