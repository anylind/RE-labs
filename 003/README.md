# RE-003

- Target: unknown stripped ELF-64 executable
- Status: validation pipeline reconstructed; valid license recovery is still in progress

## Level 1: Identify

- x86-64, little endian, two's-complement data
- PIE, dynamically linked, stripped
- Entry point: `0x1340`
- Relevant strings: `license: `, `invalid license`, `license accepted`
- Relevant imports: `fgets`, `stdin`, `puts`, `strlen`, `stdout`, `strcspn`, `fwrite`

The `.rodata` region from `0x2040` through `0x213f` is a 256-byte lookup table. It is live algorithm data, not padding or trash: the first validation loop loads its base into `r10` at `0x1174` and reads `table[index]` through `[r10+r11]` at `0x119c` for every candidate license.

## Level 2: Input handling

The code at `0x10c0` is equivalent to the following pseudocode. The stack canary and cleanup instructions are omitted.

```c
fwrite((const void *)0x2004, 1, 9, stdout); // "license: "

char *buf = input_stack_buffer;
if (fgets(buf, 0x80, stdin) == NULL)
    return 1;

buf[strcspn(buf, "\n")] = '\0';

if (strlen(buf) != 0xc) {
    puts("invalid license");
    return 1;
}
```

The `test rax, rax` at `0x110f` checks the return value of `fgets`, so the failure condition is `fgets(...) == NULL`. The delimiter at `0x200e` is one newline byte followed by NUL, hence `strcspn(buf, "\n")`.

For the validation code:

- `input[0..11]` is the 12-byte string at `[rsp+0x10]`.
- `work[0..11]` is a derived 12-byte buffer at `[rsp+0x4]`.
- All state arithmetic below is 32-bit arithmetic modulo `2^32`.

## Level 3: Validation pipeline

### Loop 1: table-based transform (`0x1167-0x11d6`)

The first loop transforms the input into `work` and checks its final state at `0x11dc`.

```c
uint32_t state = 0x31415926;

for (uint32_t i = 0; i < 12; i++) {
    uint32_t shift = (i & 3) * 8;
    uint8_t table_index = (uint8_t)(input[i] + 0x11 * i);
    uint8_t state_byte = (uint8_t)(state >> shift);

    work[i] = (uint8_t)((state_byte ^ table[table_index]) + 0x1d * i);

    state ^= (uint32_t)work[i] << shift;
    state ^= state >> 16;
    state *= 0x7feb352d;
    state ^= state >> 15;
    state *= 0x846ca68b;
    state ^= state >> 16;
}

if (state != 0x2a9a3b3f)
    invalid;
```

Here `table` is the 256-byte array at `0x2040-0x213f`; the table index is reduced to one byte by the `movzx` at `0x1196`.

### Loop 2: rolling state mix (`0x11e7-0x1275`)

```c
uint32_t state = 0x9e3779b9;

for (uint32_t i = 0; i < 12; i++) {
    state = rol32(state, 5);

    uint8_t mix = (uint8_t)(work[(i + 5) % 12] + 3);
    mix ^= work[(i + 1) % 12];
    mix = (uint8_t)(mix + work[i]);
    mix = (uint8_t)(mix - work[(i + 9) % 12]);

    state ^= (uint32_t)mix * 0x045d9f3b;
}

if (state != 0xca7f01b6)
    invalid;
```

The final comparison is at `0x127b`.

### Loop 3: final state and byte relation (`0x1288-0x1311`)

```c
uint32_t state = 0x13579bdf;
uint32_t offset = 0;

for (uint32_t i = 0; i < 12; i++) {
    uint32_t value = (uint32_t)work[i] + offset;
    offset += 0x31;

    value ^= state;
    value = rol32(value, 7);
    value += 0x6d2b79f5;
    state = value ^ (value >> 11);
}

uint16_t byte_check = (uint16_t)(
    ((uint16_t)work[3] * 0x101 + work[7]) ^
    ((uint16_t)work[0] << 8 | work[11])
);

if (byte_check != 0x2bbf || state != 0xbb7c16a9)
    invalid;
```

The byte relation is checked at `0x1307`; the final state is checked at `0x1311`. Passing both checks prints `license accepted` and returns zero. Any failed check prints `invalid license` and returns one.

## Open TODO: recover a valid license

The loop bodies, state updates, lookup-table role, and constants are now recorded, but a valid 12-byte input has not yet been derived or independently tested. Finding one and adding a reproducible invocation with its expected `license accepted` output remains open work; this writeup does not claim that the license algorithm has been fully solved.
