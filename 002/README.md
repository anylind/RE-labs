# RE-002.
### target: unkown ELF 64bit.
### status: done.

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


## The Final:

# RE-002

## 1. Objective

The goal of this exercise was to analyze an unknown ELF binary and reconstruct its validation logic.

Initially, based only on static information, it was clear that the program accepts input and prints `access granted` if the input is correct.

The main goal:

```text
assembly
→ data flow
→ algorithm
→ state transition
→ validation condition
→ solver
→ verification
```

---

# 2. Identity

```text
Architecture: x86-64
Format: ELF 64-bit
Endianness: little-endian
PIE: yes
Dynamically linked: yes
Stripped: yes
```

Notable functions/data:

```text
fgets
strcspn
strlen
fwrite
puts
stdin
stdout
```

Strings:

```text
key:
access denied
access granted
```

An 8-byte sequence was also found in `.rodata`:

```text
17 2b 6d 39 4f 12 58 21
```

---

# 3. Initial Hypothesis

### Hypothesis

The program is probably a password/input validation system.

### Evidence

The presence of:

```text
key:
access denied
access granted
fgets
strlen
```

as well as the presence of an unprintable 8-byte data sequence.

### Conclusion

The initial hypothesis was confirmed: the binary contains an input validation mechanism.

---

# 4. Input Processing

The main function first reads the input into a buffer:

```asm
fgets(...)
```

Then:

```asm
strcspn(buffer, "\n")
```

is used to find the newline character, and the newline is replaced with `0`.

Then:

```asm
strlen(buffer)
cmp rax, 8
```

Therefore, the input must be exactly 8 bytes long.

---

# 5. State Initialization

At the beginning of the validation loop:

```asm
115d: mov $0x3,%esi
1162: mov $0x13579bdf,%eax
1167: xor %ecx,%ecx
```

Therefore:

```text
EAX0 = 0x13579bdf
ESI0 = 3
ECX  = 0
```

`ECX` acts as the index.

---

# 6. Per-Iteration Values

During each iteration:

```asm
movzbl (%rbx,%rcx,1), %edx
add    %esi,%edx
xor    %edx,%eax
```

Therefore:

```text
input = buf[i]

value = EAX_i ^ (buf[i] + ESI_i)
```

The value of `ESI` increases by `0x11` on every iteration:

```text
i = 0  → 0x03
i = 1  → 0x14
i = 2  → 0x25
i = 3  → 0x36
i = 4  → 0x47
i = 5  → 0x58
i = 6  → 0x69
i = 7  → 0x7A
```

---

# 7. First Transformation

After the XOR:

```asm
mov %eax,%edx
shr $0x10,%edx
xor %eax,%edx
```

Therefore:

```text
Y = X ^ (X >> 16)
```

This transformation is reversible.

If:

```text
Y = X ^ (X >> 16)
```

then for a 32-bit value:

```text
X = Y ^ (Y >> 16)
```

This property was used during the reverse analysis.

---

# 8. Modular Multiplication

Then:

```asm
imul $0x45d9f3b,%edx,%edx
```

which means:

```text
Y = X * 0x045d9f3b mod 2^32
```

Because this is 32-bit arithmetic, only the low 32 bits must be retained.

The multiplier:

```text
0x045d9f3b
```

is odd, therefore it has a multiplicative inverse modulo `2^32`.

The modular inverse is:

```text
0x119de1f3
```

and:

```text
0x045d9f3b × 0x119de1f3 ≡ 1 (mod 2^32)
```

Therefore, the multiplication is also reversible.

---

# 9. Second XOR-Shift

Then:

```asm
mov %edx,%eax
shr $0x10,%eax
xor %edx,%eax
```

Again, we have the same transformation:

```text
Y = X ^ (X >> 16)
```

Therefore, this part is also reversible.

---

# 10. CMOVE Logic

After the transformation:

```asm
mov %eax,%edx
xor $0xa5a5a5a5,%edx

cmp %al,(%rdi,%rcx,1)
cmove %edx,%eax
```

Target bytes:

```text
17 2b 6d 39 4f 12 58 21
```

Behavior:

```text
if (EAX_low8 == target[i]):
    EAX = EAX ^ 0xa5a5a5a5
else:
    EAX unchanged
```

Important point:

This `cmp` is not an independent validation failure.

If the condition is not satisfied, the program continues with the same iteration.

The condition only determines whether `CMOVE` executes.

---

# 11. Complete State Transition

During iteration `i`:

```text
EAX_i
   ↓
X = EAX_i ^ (buf[i] + ESI_i)
   ↓
Y = X ^ (X >> 16)
   ↓
Y = Y * 0x045d9f3b mod 2^32
   ↓
Z = Y ^ (Y >> 16)
   ↓
if low8(Z) == target[i]:
       Z = Z ^ 0xa5a5a5a5
   ↓
EAX_(i+1) = Z
```

And:

```text
ESI_(i+1) = ESI_i + 0x11
```

---

# 12. Final Validation

After 8 iterations:

```asm
11b7: cmp $0x7c1e2a93,%eax
11bc: jne 1132
```

Therefore:

```text
EAX8 must equal:

0x7c1e2a93
```

This is the first and most important acceptance condition.

The initial state is:

```text
EAX0 = 0x13579bdf
```

Therefore, the problem can be viewed as:

```text
EAX0 = 0x13579bdf

     → iteration 0
     → iteration 1
     → iteration 2
     → iteration 3
     → iteration 4
     → iteration 5
     → iteration 6
     → iteration 7

EAX8 = 0x7c1e2a93
```

During reverse analysis, the same relationship is examined from the end toward the beginning.

---

# 13. Reverse Analysis

The analysis started from:

```text
EAX8 = 0x7c1e2a93
```

In iteration 7:

```text
target[7] = 0x21
ESI7 = 0x7A
```

First, we checked whether `CMOVE` could have been taken:

```text
0x7c1e2a93 ^ 0xa5a5a5a5
= 0xd9bb8f36
```

Low byte:

```text
0x36
```

But the target is:

```text
0x21
```

Therefore, `CMOVE` could not have been taken in iteration 7.

Thus:

```text
EAX_after_cmove = 0x7c1e2a93
```

Reverse the first XOR-shift:

```text
0x7c1e2a93 ^ (0x7c1e2a93 >> 16)
= 0x7c1e568d
```

Reverse the multiplication:

```text
0x7c1e568d × 0x119de1f3 mod 2^32
= 0xec5714d7
```

Reverse the XOR-shift:

```text
0xec5714d7 ^ (0xec5714d7 >> 16)
= 0xec57f880
```

Therefore:

```text
EAX7 ^ (buf[7] + 0x7A)
= 0xec57f880
```

At this point, `buf[7]` was unknown.

Therefore, a predecessor state was constructed for every printable candidate.

---

# 14. Important Realization

During reverse analysis:

```text
candidate byte
→ predecessor EAX
```

Every `EAX_prev` that is consistently obtained from the transition is, for the moment:

```text
POTENTIALLY VALID
```

It must not be considered a correct or incorrect password at that iteration.

The only final validation is whether, after reversing all 8 iterations, we reach:

```text
EAX0 = 0x13579bdf
```

Therefore:

```text
intermediate state ≠ final solution
```

---

# 15. Forward Solver

After understanding the algorithm, a forward approach was used to implement the solver.

Candidate space:

```text
0x20 .. 0x7E
```

That is, printable ASCII.

At each position, all candidates are tested.

For every candidate:

```text
state
→ transformation
→ CMOVE condition
→ next state
```

Only paths that do not satisfy the condition of that iteration are pruned.

Therefore, the solver has the structure:

```text
search(position, EAX, ESI)
```

If:

```text
position == 8
```

then:

```text
if EAX == 0x7c1e2a93:
    Match
```

Otherwise:

```text
for candidate in printable:
    calculate next state
    apply algorithm
    continue recursively
```

---

# 16. Why This Is Not Naive Brute Force

Naive brute force:

```text
95^8
```

checks the entire state space until the end.

However, the solver uses the structure of the algorithm and applies the transition constraints at every level.

Therefore:

```text
Brute force
+
algorithm constraints
+
pruning
=
constrained search
```

In this exercise, the recursion is effectively a DFS over the candidate space.

---

# 17. Verification

The solver was able to find several paths that reached the final state.

Examples found:

```text
@Rdle...
!.1b/p..
!X.q1`..
```

Because spaces are printable, some of these outputs may contain invisible trailing spaces.

For verification, the password should also be printed as hexadecimal to eliminate ambiguity.

Final verification with the original binary:

```text
./binary/re002
```

was performed, and one of the discovered paths caused:

```text
key:
access granted
```

---

# 18. Final Understanding

The most important thing learned from RE-002 is not just the password.

The analysis process was:

```text
unknown ELF
    ↓
identify input handling
    ↓
trace registers
    ↓
reconstruct loop
    ↓
model state transition
    ↓
identify branch semantics
    ↓
identify invertible operations
    ↓
analyze forward and reverse state flow
    ↓
build constrained search
    ↓
verify candidate against original binary
```

Reverse engineering is not just about reading instructions.

The goal is to reconstruct the behavior and logic of a program from its machine code.

---

# 19. Methodology Lessons

### Fact

`EAX` is a changing state variable, and its meaning is determined relative to the execution point in time during each iteration.

### Fact

`EDX` is also a temporary variable and is overwritten multiple times.

### Lesson

A single fixed meaning should not be assumed for a register throughout an entire function.

---

### Fact

`CMOVE` makes its decision based on the flags produced by `CMP`.

### Lesson

When reversing a branch, both the value and the branch condition must be examined.

---

### Fact

Intermediate states alone do not indicate that the password is correct.

### Lesson

A distinction must be made between:

```text
valid state
```

and:

```text
valid complete solution
```

---

### Fact

The algorithm allowed the state transition to be modeled both forward and backward.

### Lesson

Before using brute force, determine whether the structure of the algorithm can reduce the search space.

---

# 20. Open Questions / Next Improvements

For the next exercise, I should be able to do the following without assistance:

```text
1. Find function boundaries
2. Determine input/data flow
3. Extract the state machine
4. Model branches
5. Separate constraints
6. Design a solver skeleton myself
7. After writing the solver, verify it against the original binary
```

The next goal is not to find the password faster.

The goal is to be able to independently move from an unknown binary to:

```text
Observation → Model → Hypothesis → Evidence → Conclusion
```
