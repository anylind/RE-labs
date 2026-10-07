# Reverse Engineering Labs

A growing collection of hands-on reverse engineering exercises focused on
understanding small x86-64 ELF binaries. Each lab starts with static evidence
and builds toward a clear description of the program's input handling,
transforms, checks, and expected behavior.

## Labs

| Lab | Focus | Status |
| --- | --- | --- |
| [RE-001](001/README.md) | Fixed-length password validation, QWORD comparison, calling conventions, and endianness | Complete |
| [RE-002](002/README.md) | Iterative state transforms, conditional data flow, backward reasoning, and a candidate solver | Complete |
| [RE-003](003/README.md) | Three-stage license validation, a 256-byte lookup table, and chained state checks | In progress |

Each lab contains its own writeup and the binary or scripts used during the
analysis.

## Analysis workflow

The writeups generally follow this sequence:

1. Identify the file format, architecture, linkage, and protection features.
2. Inspect strings, sections, segments, imports, and relocation data.
3. Trace input handling from the entry path into the validation logic.
4. Reconstruct important assembly blocks as readable pseudocode.
5. Record state variables, constants, table lookups, and branch conditions.
6. Validate the reconstruction with a debugger, a small helper, or a known
   input/output pair.

Useful starting commands are:

```sh
file 001/binary/re001
readelf -hW -SW 001/binary/re001
readelf -lW 001/binary/re001
strings -a -tx 001/binary/re001
objdump -d -M intel 001/binary/re001
```

Adjust the path and address range for the lab under investigation. For PIE
executables, distinguish static file addresses from runtime addresses when
using a debugger.

## Documentation conventions

- Keep hypotheses separate from conclusions supported by the binary.
- Use hexadecimal addresses and constants exactly as they appear in the
  disassembly.
- State integer width and wraparound behavior when describing arithmetic.
- Explain the purpose of lookup tables and generated buffers instead of
  labeling unexplained data as padding or noise.
- Include a reproducible input and expected output whenever a check has been
  solved.
- Mark unresolved reasoning as an explicit TODO so the writeup does not imply
  that an incomplete reconstruction is final.

## Current open work

RE-003 still needs a valid 12-byte license input and an independently verified
successful run. The validation pipeline and its three state checks are
documented in [003/README.md](003/README.md).

## Responsible use

Analyze only binaries and systems that you own or are authorized to examine.
Run unfamiliar samples in an isolated environment and keep derived artifacts
separate from the original binaries.
