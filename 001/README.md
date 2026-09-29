# RE-001
### Target: unkown ELF
### Status: done

### Identity
- #####   x86-64 / ELF 64-bit / LSB / PIE / Dinamically linked / stripped
- #####    Entry point: 0x11f0
- #####    Data: 2`s complement, little endian
-  | section | flags | permission | segment  |
   |:--------|:------|:--------|:------------|
   | .dynsym   | A   | R       | LOAD 02     |
   | .plt      | AX  | RE      | LOAD 03     |
   | .text     | AX  | RE      | LOAD 03     |
   | .rodata   | A   | R       | LOAD 04     |
   | .got.plt  | WA  | RW      | LOAD 05     |
   | .data     | WA  | RW      | LOAD 05     |
   | .bss      | WA  | RW      | LOAD 05     |

### Initial observations

- Program asks for an 8-character password.
- Input is read with fgets.
- Newline is removed with strcspn.
- Input length is checked with strlen.
- Validation compares 8 bytes of input against a constant.
- validate returns a boolean-like integer (0 or 1).
- strings may contain false positives from .text/instruction bytes.


### RE-001 — Final Reconstruction

#### Binary behavior:

1. Program reads user input into a stack buffer.
2. Input is limited by fgets.
3. Newline is removed using strcspn.
4. Input length must be exactly 8 bytes.
5. The first 8 bytes are loaded as a QWORD.
6. The QWORD is compared against a constant.
7. Equal → success message.
8. Not equal → failure message.
9. Wrong length → failure message and exit status 1.
10. Stack canary protects the stack frame.

#### Important RE lessons:

- ELF sections ≠ program execution units.
- Program segments determine runtime memory permissions/layout.
- strings() is not semantic analysis.
- Function arguments follow the x86-64 calling convention.
- RDI is the first integer/pointer argument.
- RAX is used for return values.
- Stack offsets reveal local variables.
- Assembly represents behavior, not source syntax.
- Little-endian changes how multi-byte values appear in memory.
- Compiler transformations can make binary logic differ significantly from source expressions.
