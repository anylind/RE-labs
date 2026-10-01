# RE-002.
### target: unkown ELF 64bit.
### status: in progress

### Identity
- x86-64 / ELF 64-bit / LSB / PIE / Dinamically linked / stripped
- Entry point: 0x11f0
- Data: 2`s complement, little endian
-    |Section   |Flags |premission |segment    |
     |----------|------|----------| -----------|
     |.dynsym   | A    | R         | INTERP 02 |
     |.plt      | AX   | RE        | LOAD 03   |
     |.text     | AX   | RE        | LOAD 03   |
     |.rodat    | A    | R         | LOAD 04   |
     |.got.plt  | WA   | RW        | LOAD 05   |
     |.data     | WA   | RW        | LOAD 05   |
     |.bss      | WA   | RW        | LOAD 05   |

### Interesting data
-  "key:"
-  "access denied"
-  "access granted"
-  fgets, stdin, puts, strlen, stdout, strcspn, fwrite

### Initial hypotheses
- H1: Based on the functions used and the strings present, it can likely be said that this is an input validation system—possibly one that checks passwords.
- H2: The 8-byte constant at .rodata[0x2030] controls conditional state transitions through CMOVE.
- Unknown: Whether the correct input causes CMOVE to trigger at specific iterations, all iterations, or not at all.

### Trying to decode the algorithm in 0x115d-0x11e4:
#### Backward analysis — iteration 7

Known final state:

```text
EAX_final = 0x7c1e2a93
```

At the end of iteration 7:

```asm
119e: xor eax,edx
11a4: cmp al,[rdi+rcx]
11a7: cmove edx,eax
```

For iteration 7:

```text
rcx = 7
target[7] = 0x21
```

##### Determining whether `cmove` is taken

Assumption:

```text
cmove is taken
```

Then after `cmove`:

```text
EAX_final = EDX3
```

Therefore:

```text
EDX3 = 0x7c1e2a93
```

But:

```text
EDX3 = EAX2 ^ 0xa5a5a5a5
```

So:

```text
EAX2 = 0x7c1e2a93 ^ 0xa5a5a5a5
     = 0xd9bb8f36
```

The `cmp` tests only the low byte:

```text
EAX2 low byte = 0x36
target[7]     = 0x21
```

Since:

```text
0x36 != 0x21
```

the equality condition is false.

Therefore the assumption was impossible.

#### Conclusion

```text
cmove is NOT taken in iteration 7.
```

Therefore:

```text
EAX2 = EAX_final = 0x7c1e2a93
```

---

#### Reversing the XOR-shift transform

The previous transform was:

```text
EAX2 = Y ^ (Y >> 16)
```

Given:

```text
EAX2 = 0x7c1e2a93
```

reverse it:

```text
Y = 0x7c1e2a93 ^ (0x7c1e2a93 >> 16)
  = 0x7c1e568d
```

The previous multiplication was:

```text
Y = X * 0x045d9f3b mod 2^32
```

The modular inverse is:

```text
0x045d9f3b⁻¹ mod 2^32 = 0x119de1f3
```

Therefore:

```text
X = 0x7c1e568d * 0x119de1f3 mod 2^32
  = 0xec5714d7
```

Reverse the previous XOR-shift:

```text
X = Z ^ (Z >> 16)
```

Thus:

```text
Z = 0xec5714d7 ^ (0xec5714d7 >> 16)
  = 0xec57f880
```

So the state before the XOR at `1186` is:

```text
EAX_before_1186_output = 0xec57f880
```

Equivalently, at `1186`:

```text
(buf[7] + 0x7A) ^ EAX_previous_iteration
    = 0xec57f880
```

This leaves two unknowns:

```text
buf[7]
EAX_previous_iteration
```

so iteration 7 alone is not sufficient to recover `buf[7]`.
