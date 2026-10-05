# RE-002 — Stateful Password Validation

**Status:** Complete

RE-002 is a stripped x86-64 PIE executable that validates an eight-byte input
through a stateful transformation. The validation uses two reversible
XOR-shifts, an odd multiplication modulo 2^32, and a conditional XOR selected
by CMOVE.

## Binary profile

| Property | Value |
| --- | --- |
| Format | ELF 64-bit |
| Architecture | x86-64 |
| Endianness | Little endian |
| PIE | Yes |
| Linkage | Dynamically linked |
| Symbols | Stripped |
| Entry point | 0x11f0 |
| Main function | 0x10c0 |
| Stack protection | Stack canary |

Important strings are stored in .rodata:

| Address | Data |
| --- | --- |
| 0x2004 | key: |
| 0x200c | access denied |
| 0x201a | access granted |
| 0x2030 | 17 2b 6d 39 4f 12 58 21 |

The eight bytes at 0x2030 are branch-control bytes. They are not the password
and are not compared directly with the input.

## Input handling

The input path is equivalent to:

~~~c
fwrite("key: ", 1, 5, stdout);

if (fgets(buf, 0x20, stdin) == NULL)
    return 1;

buf[strcspn(buf, "\n")] = '\0';

if (strlen(buf) != 8) {
    puts("access denied");
    return 1;
}
~~~

The binary accepts up to 31 input bytes from fgets, but only a normalized
length of exactly eight bytes reaches the state machine.

## State machine

The validation loop starts with state 0x13579bdf and an additive offset of 3.
All operations are 32-bit operations, so multiplication and additions wrap
modulo 2^32.

~~~c
static const uint8_t branch_bytes[8] = {
    0x17, 0x2b, 0x6d, 0x39,
    0x4f, 0x12, 0x58, 0x21
};

uint32_t state = 0x13579bdf;
uint32_t offset = 3;

for (uint32_t i = 0; i < 8; i++) {
    uint32_t value = state
        ^ ((uint32_t)(uint8_t)buf[i] + offset);

    value ^= value >> 16;
    value *= 0x045d9f3b;
    value ^= value >> 16;

    if ((uint8_t)value == branch_bytes[i])
        value ^= 0xa5a5a5a5;

    state = value;
    offset += 0x11;
}

if (state != 0x7c1e2a93) {
    puts("access denied");
    return 1;
}

puts("access granted");
return 0;
~~~

The comparison before CMOVE only controls whether the state is XORed with
0xa5a5a5a5. It is not an independent failure check. A mismatch continues
through the loop with the unmodified state.

## Why the transform is reversible

For a 32-bit value, the operation

~~~text
y = x ^ (x >> 16)
~~~

is its own inverse:

~~~text
x = y ^ (y >> 16)
~~~

The multiplier 0x045d9f3b is odd, so it has an inverse modulo 2^32:

~~~text
0x045d9f3b⁻¹ = 0x119de1f3
~~~

This makes each non-conditional part of an iteration reversible. The CMOVE
branch must still be modeled explicitly because it changes the next state
when the low byte matches the corresponding branch byte.

## Reverse check of the final iteration

The final state is:

~~~text
state_8 = 0x7c1e2a93
branch_bytes[7] = 0x21
offset_7 = 0x7a
~~~

If CMOVE had been taken in iteration 7, the state immediately before CMOVE
would have been:

~~~text
0x7c1e2a93 ^ 0xa5a5a5a5 = 0xd9bb8f36
~~~

Its low byte is 0x36, not 0x21, so that branch cannot have been taken.
Reversing the two XOR-shifts and the multiplication gives:

~~~text
0x7c1e2a93 ^ (0x7c1e2a93 >> 16) = 0x7c1e568d
0x7c1e568d × 0x119de1f3 mod 2^32 = 0xec5714d7
0xec5714d7 ^ (0xec5714d7 >> 16) = 0xec57f880
~~~

Therefore the previous state and the final input byte satisfy:

~~~text
state_7 ^ (buf[7] + 0x7a) = 0xec57f880
~~~

A single iteration does not determine both unknowns. The complete eight-step
transition must be solved together.

## Known-good input

One printable ASCII input that passes the original binary is:

~~~text
4 T.Az&p
34 20 54 2e 41 7a 26 70
~~~

Reproduce it with:

~~~sh
printf '4 T.Az&p\n' | ./binary/re002
~~~

Expected output:

~~~text
key: access granted
~~~

The state after each iteration is:

| Iteration | Input byte | State |
| ---: | :---: | ---: |
| 0 | 4 | 0x7c7e597b |
| 1 | space | 0x50f951b2 |
| 2 | T | 0x884bdccd |
| 3 | . | 0x912e7f38 |
| 4 | A | 0xc36be301 |
| 5 | z | 0x55da87b2 |
| 6 | & | 0xec57f86a |
| 7 | p | 0x7c1e2a93 |

## Solver note

The helper at scripts/002-password-cracker.c mirrors the forward transition
over printable ASCII candidates. Its recursive search currently explores the
full 95^8 space and does not prune on the CMOVE condition. The condition is a
state update, not a rejection rule, so treating a mismatch as a failed
candidate would be incorrect.

The known-good input above was recovered with a 32-bit bit-vector model and
then verified against the original executable.

## Reverse engineering lessons

- Register names do not have one fixed meaning throughout a function.
- CMOVE must be interpreted using the flags produced by the immediately
  preceding comparison.
- Intermediate states are not complete solutions; only the final state proves
  acceptance.
- Width, signedness, byte order, and modulo behavior must be preserved in a
  solver.
- A small symbolic model can be more useful than an impractical exhaustive
  search when the transition is compact and deterministic.
