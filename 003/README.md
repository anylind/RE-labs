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

#### Shift-count schedule (`0x1185-0x119a`)

The shift count follows a four-iteration cycle. At `0x1185`, the current loop index in `esi` is copied to `ecx`. The instruction at `0x1189`, `and ecx, 3`, keeps only the lowest two bits, so the result cycles through `0, 1, 2, 3`. The instruction at `0x118f`, `shl ecx, 3`, then multiplies that result by eight. Finally, `shr edx, cl` at `0x119a` uses `cl` as the shift count.

| Iteration (`esi`) | `ecx` after `mov ecx, esi` | `ecx` after `and ecx, 3` | `ecx` after `shl ecx, 3` | Shift count (`cl`) |
|---:|---:|---:|---:|---:|
| 0 | `0x00` (`0000 0000`) | `0x00` (`0000 0000`) | `0x00` (`0000 0000`) | 0 bits |
| 1 | `0x01` (`0000 0001`) | `0x01` (`0000 0001`) | `0x08` (`0000 1000`) | 8 bits |
| 2 | `0x02` (`0000 0010`) | `0x02` (`0000 0010`) | `0x10` (`0001 0000`) | 16 bits |
| 3 | `0x03` (`0000 0011`) | `0x03` (`0000 0011`) | `0x18` (`0001 1000`) | 24 bits |
| 4 | `0x04` (`0000 0100`) | `0x00` (`0000 0000`) | `0x00` (`0000 0000`) | 0 bits |
| 5 | `0x05` (`0000 0101`) | `0x01` (`0000 0001`) | `0x08` (`0000 1000`) | 8 bits |
| 6 | `0x06` (`0000 0110`) | `0x02` (`0000 0010`) | `0x10` (`0001 0000`) | 16 bits |
| 7 | `0x07` (`0000 0111`) | `0x03` (`0000 0011`) | `0x18` (`0001 1000`) | 24 bits |
| 8 | `0x08` (`0000 1000`) | `0x00` (`0000 0000`) | `0x00` (`0000 0000`) | 0 bits |
| 9 | `0x09` (`0000 1001`) | `0x01` (`0000 0001`) | `0x08` (`0000 1000`) | 8 bits |
| 10 | `0x0a` (`0000 1010`) | `0x02` (`0000 0010`) | `0x10` (`0001 0000`) | 16 bits |
| 11 | `0x0b` (`0000 1011`) | `0x03` (`0000 0011`) | `0x18` (`0001 1000`) | 24 bits |

So the effective shift counts are `0, 8, 16, 24`, repeating for the 12 iterations. The `and` is at `0x1189`; `0x118f` is the left shift that converts the two-bit cycle into a bit-shift count.

Here `table` is the 256-byte array at `0x2040-0x213f`; the table index is reduced to one byte by the `movzx` at `0x1196`.

### Lookup-table index hypothesis

The lookup table is 256 bytes long and its base address is stored in `r10`.
At the beginning of each iteration, `r8d` contains an offset that increases
by `0x11` per iteration, starting from zero.

The input byte is added to this offset. The instruction at `0x1196`,
`movzbl %r11b, %r11d`, retains only the low eight bits of the sum before
the table lookup.

Therefore, for iteration `i`, the effective table index is:

`table_index = (input[i] + 0x11 * i) mod 256`

This means that values exceeding `0xff` before truncation wrap around
within the 256-entry table. For example, at iteration 8, an input byte
of `0x7f` produces `0x107` before truncation and `0x07` as the effective
table index.

A printable-ASCII input range may be used as a search-space assumption,
but the observed instructions alone do not establish the license's
allowed character set.


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

## Recovery notes: hypotheses, constraints, and search strategy

### Current input-domain hypothesis

The input-handling code establishes that the license is exactly 12 bytes long after the newline is removed. It does **not** establish that every byte must be printable ASCII. For the current recovery attempt, use the provisional candidate alphabet `0x20..0x7e` (95 possible byte values per position), and treat this as a hypothesis until confirmed by additional code or testing.

The resulting unconstrained search space is `95^12` (approximately `5.4e23`) candidate strings. The expression `12 * 95` counts position/value checks, not complete candidate strings. A full Cartesian-product brute force is therefore not the intended first approach.

### Reverse the state transform before searching blindly

Loop 1 starts from `state = 0x31415926` and requires the final state to equal `0x2a9a3b3f`. Its arithmetic is 32-bit arithmetic modulo `2^32`. Several operations in the mixing pipeline can be inverted:

- `state ^= state >> k` is invertible for a 32-bit state; for example, `y = x ^ (x >> 16)` gives `x = y ^ (y >> 16)`.
- Multiplication modulo `2^32` by an odd constant is invertible. The constants in this loop are odd, so their modular inverses exist.
- XOR with a known value is invertible by applying the same XOR again.
- `movzbl %dl, %edx` discards the upper 24 bits of the old `edx`. In this loop, however, `edx` was explicitly zero-extended from `dl`, so the value immediately after `movzbl` is known to be in `0..255`.

The recommended direction is to undo the final mixing operations first, then reason about the byte injected by `state ^= (uint32_t)work[i] << shift`. Do not skip the `xor`/shift stages when reversing a multiplication: the inverse must follow the exact reverse instruction order.

### Per-iteration candidate filtering

A practical hypothesis is to recover candidates backward from the final Loop 1 state, while enumerating only the allowed byte values for the current input position. For each candidate, use the known 256-byte table and the iteration-specific offsets to calculate the corresponding table index and `work[i]`, then retain only candidates consistent with the reversed state transition.

The table index for iteration `i` is:

```text
table_index = (input[i] + 0x11 * i) mod 256
```

The derived work byte is:

```text
work[i] = ((state_byte ^ table[table_index]) + 0x1d * i) mod 256
```

where `state_byte = (state >> ((i & 3) * 8)) & 0xff` refers to the state entering that iteration, before the byte injection. Be careful to distinguish the state before injection, the state after injection, and the state after the mixing pipeline.

Because the byte injection changes only one byte lane of the 32-bit state, a reverse step may leave many state bits constrained but not uniquely determined until a candidate `work[i]` is considered. Use this structure to enumerate a small set of candidates and prune inconsistent branches, rather than enumerating all 12-byte strings.

### Use all validation loops as constraints

Loop 1 is not the only verifier. A candidate must also produce a `work[0..11]` buffer that satisfies:

1. Loop 1 final state: `0x2a9a3b3f`.
2. Loop 2 rolling state: `0xca7f01b6`.
3. Loop 3 final state: `0xbb7c16a9`.
4. Loop 3 byte relation: `0x2bbf`.

These checks should be used as constraints during recovery where practical, and all surviving candidates must ultimately be tested by running the original executable and confirming `license accepted`. A match against only one state check is not sufficient proof that a license is valid.

### Suggested solver progression

1. **Validate the model forward.** Reimplement each loop from the disassembly and compare intermediate states and `work` bytes against a debugger trace. This catches off-by-one errors in `r8d`, `r9d`, the loop index, shift counts, and the distinction between pre-mix and post-mix state.
2. **Invert deterministic operations.** Reverse XOR-shifts and odd multiplications in exact reverse order, using arithmetic modulo `2^32`.
3. **Enumerate one byte at a time.** Start with the last iteration and the provisional printable alphabet. For each byte candidate, derive the possible preceding state and reject candidates that violate the transition equations.
4. **Backtrack with pruning.** Preserve only compatible partial assignments and use the other validation loops and byte relation to reject branches early.
5. **Escalate to a constraint solver if needed.** If the reverse search branches too widely, model the 32-bit operations as bit-vectors in Z3. Keep the printable-byte restriction explicit as a hypothesis, not as a proven property of the binary.
6. **Verify independently.** Run every final candidate through the unmodified executable and require the actual `license accepted` result.

### Current arithmetic check

Reversing the known Loop 1 target `0x2a9a3b3f` through the final XOR-shift, multiplication by `0x846ca68b`, XOR-shift, and multiplication by `0x7feb352d` gives the following checkpoints:

| Checkpoint | Value |
|---|---:|
| Final compared state | `0x2a9a3b3f` |
| Value after the final XOR-shift at `0x11d4` is undone; EAX after the multiplication at `0x11c9` | `0x2a9a11a5` |
| State before multiplication by `0x846ca68b` at `0x11c9` | `0x60865e8f` |
| State before multiplication by `0x7feb352d` at `0x11bc` | `0x60869f82` |
| EAX after `xor edx, eax` at `0x11ba` | `0x35e792ca` |
| EDX/EAX at `0x11b5`, before the XOR-shift sequence beginning at `0x11b7` | `0x35e7a72d` |

These last two values are consistent: `0x35e792ca` is the result of `S ^ (S >> 16)`, where `S = 0x35e7a72d`. Therefore, the earlier apparent discrepancy came from assigning `0x35e792ca` to the wrong instruction boundary; it is not the state immediately before byte injection at `0x11b1`.

The modular inverses used for the two multiplications are:

```text
inverse(0x7feb352d mod 2^32) = 0x1d69e2a5
inverse(0x846ca68b mod 2^32) = 0x43021123
```

Both multipliers are odd, so each has a unique inverse modulo `2^32`.

### Current status

The printable-ASCII alphabet, backward per-iteration search, and use of all validation checks are recovery hypotheses and proposed methods—not a recovered license. The next concrete milestone is to validate a forward model against the executable, and then implement a pruned backward search or a Z3 model.
