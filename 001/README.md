## RE-001
# Target: unkown ELF
# Status: done


RE-001 — Final Reconstruction

Binary behavior:

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

Important RE lessons:

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
