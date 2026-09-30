# RE-002.
## target: unkown ELF 64bit.
## status: in progress

### Identity
####     x86-64 / ELF 64-bit / LSB / PIE / Dinamically linked / stripped
####     Entry point: 0x11f0
####     Data: 2`s complement, little endian
####    |Section   |Flags |premission |segment    |
        |----------|------|----------| -----------|
        |.dynsym   | A    | R         | INTERP 02 |
        |.plt      | AX   | RE        | LOAD 03   |
        |.text     | AX   | RE        | LOAD 03   |
        |.rodat    | A    | R         | LOAD 04   |
        |.got.plt  | WA   | RW        | LOAD 05   |
        |.data     | WA   | RW        | LOAD 05   |
        |.bss      | WA   | RW        | LOAD 05   |

### Interesting data
#### "key:"
#### "access denied"
#### "access granted"
#### fgets, stdin, puts, strlen, stdout, strcspn, fwrite

### Initial hypotheses
#### H1: Based on the functions used and the strings present, it can likely be said that this is an input validation system—possibly one that checks passwords.
#### H2: The 8-byte constant at .rodata[0x2030] controls conditional state transitions through CMOVE.
#### Unknown: Whether the correct input causes CMOVE to trigger at specific iterations, all iterations, or not at all.
