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


## UUIDv7 Structure

Follow the RFC 9562 UUIDv7:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           unix_ts_ms                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          unix_ts_ms           |  ver  |       rand_a          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|var|                        rand_b                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            rand_b                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Figure 11: UUIDv7 Field and Bit Layout

unix_ts_ms:
    48-bit big-endian unsigned number of the Unix Epoch timestamp in
    milliseconds [...] 
ver:
    The 4-bit version field [...]
rand_a:
    12 bits of pseudorandom data [...]
var:
    The 2-bit variant field [...]
rand_b:
    The final 62 bits of pseudorandom data [...]
```

- In this implementation, the `rand_a` 12 bits matches the 12 bits Snowflake ID
counter so this counter is used as allowed by section 6.2 of the RFC.

- In `rand_b` the 16 first bits are used to store a `node_id`, similar as in
Snowflake ID but bigger, as allowed in section 6.4 of the RFC on the subject of
distributed UUID Generation.

- The remaining bits of `rand_b` is a random counter that increase monotically.
At startup, the counter is initialized with a random value and each time an
UUID is generated, 1 is added to the random value.

## UUID predictability

They are predictable. They are not to be used as whatever about security. They
are designed to be used as primary key in a database where you want to replace
SQL auto increment by a value that can be generated without asking the database.

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

 - Convert to string (must have an allocated 36 bytes, at least, dest. No \0 
                      added at the end).
```c
void     uuidv7_str   (char *dest, uuidv7_t id);
```

For a working example, see the file `example.c`.

## License

Public domain.
