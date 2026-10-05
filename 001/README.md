# RE-001 — Fixed-Width Password Check

**Status:** Complete

RE-001 is a small x86-64 PIE executable that reads eight bytes, removes a
trailing newline, and compares the resulting buffer directly with a constant
64-bit value. There is no encryption or iterative transformation in this
sample.

## Binary profile

| Property | Value |
| --- | --- |
| Format | ELF 64-bit |
| Architecture | x86-64 |
| Endianness | Little endian |
| PIE | Yes |
| Linkage | Dynamically linked |
| Symbols | Present; the binary is not stripped |
| Entry point | 0x1150 |
| Main function | 0x1090 |
| Stack protection | Stack canary |

## Input handling

The relevant logic is equivalent to:

~~~c
printf("enter password '8char': ");
fgets(buf, 10, stdin);
buf[strcspn(buf, "\n")] = '\0';

if (strlen(buf) != 8) {
    puts("Authentication failure.");
    return 1;
}
~~~

The call to fgets accepts at most nine input bytes before the terminating NUL.
The binary does not check the return value of fgets before using the buffer.

## Validation

At 0x10ee, the program loads this immediate value:

~~~text
0x2d3c0f1e5a4b7869
~~~

It then compares that value with the first eight bytes of the input buffer.
Because x86-64 is little endian, the required bytes must appear in this
order:

~~~text
69 78 4b 5a 1e 0f 3c 2d
 i  x  K  Z        <  -
~~~

The bytes 0x1e and 0x0f are control characters, so the accepted value is not
an ordinary printable eight-character password.

The comparison can be represented as:

~~~c
uint64_t candidate;
memcpy(&candidate, buf, sizeof candidate);

if (candidate != UINT64_C(0x2d3c0f1e5a4b7869)) {
    puts("Authentication failure.");
    return 0;
}

puts("Wellcome.");
return 0;
~~~

The spelling Wellcome. is the exact success message embedded in the binary.

## Known-good input

The following command sends the required bytes without relying on terminal
input for the control characters:

~~~sh
printf 'ixKZ\036\017<-\n' | ./binary/re001
~~~

Expected output:

~~~text
enter password '8char': Wellcome.
~~~

## Observed behavior

| Condition | Output | Exit status |
| --- | --- | --- |
| Exact eight-byte value | Wellcome. | 0 |
| Eight bytes with the wrong value | Authentication failure. | 0 |
| Any other normalized length | Authentication failure. | 1 |

## Reverse engineering notes

- A QWORD comparison must be read together with the target architecture's
  byte order.
- The stack offset 0xe is unaligned, but x86-64 permits this load.
- The prompt says 8char, while the actual accepted byte sequence contains
  non-printable values.
- The success and failure paths share the normal return sequence; only the
  wrong-length path sets a non-zero return value.
