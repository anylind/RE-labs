# RE-003
- target: Unknown stripped elf-64bit
- status: in progress

### Level 1: Identify
- x86-64 LSB 2`s complement  |  Strings/data:
- pie                                         |   "license:"   "invalid license"  "license accepted"
- stripped                                    |     fgets, stdin, puts, strlen, stdout, strcspn, fwrite
- dynamically lenked                 |  
- Entry point address: 0x1340  |  and also an sequence of trash bytes between 0x2040-0x213f in .rodata


### Level 2: Reconstruct C style
- ```
    10c6:       be 01 00 00 00          mov    esi,0x1
    10cb:       48 8d 3d 32 0f 00 00    lea    rdi,[rip+0xf32]        # 2004 <__cxa_finalize@plt+0xf74>
    10ea:       48 8b 0d 1f 2f 00 00    mov    rcx,QWORD PTR [rip+0x2f1f]        # 4010 <stdout@GLIBC_2.2.5>
    10f6:       e8 85 ff ff ff          call   1080 <fwrite@plt>

      
- (10c6, 10cb, 10ea, 10f6)  ->  `fwrite(&(0x2004)string, 1, stdout)`

- ```
    10fb:       48 8b 15 1e 2f 00 00    mov    rdx,QWORD PTR [rip+0x2f1e]        # 4020 <stdin@GLIBC_2.2.5>
    1102:       be 80 00 00 00          mov    esi,0x80
    1107:       48 89 df                mov    rdi,rbx
    110a:       e8 61 ff ff ff          call   1070 <fgets@plt>

- (10fb, 1102, 1107, 110a)  ->  `fgets(&(rsp+0x10)buf, 128, stdin)`

- ```
    110f:       48 85 c0                test   rax,rax
    1112:       74 2e                   je     1142 <__cxa_finalize@plt+0xb2>

    1142:       b8 01 00 00 00          mov    eax,0x1
    1147:       48 8b 94 24 98 00 00    mov    rdx,QWORD PTR [rsp+0x98]
    114e:       00 
    114f:       64 48 2b 14 25 28 00    sub    rdx,QWORD PTR fs:0x28
    1156:       00 00 
    1158:       0f 85 d2 01 00 00       jne    1330 <__cxa_finalize@plt+0x2a0>
    115e:       48 81 c4 a0 00 00 00    add    rsp,0xa0
    1165:       5b                      pop    rbx
    1166:       c3                      ret

- ``if (!buf)
   return 1``

- ```
    1114:       48 89 df                mov    rdi,rbx
    1117:       48 8d 35 f0 0e 00 00    lea    rsi,[rip+0xef0]        # 200e <__cxa_finalize@plt+0xf7e>
    111e:       e8 3d ff ff ff          call   1060 <strcspn@plt>

- ``strcspn(&(rsp+0x10)buf, "([rip+0xef0]->0a)\n")``

- ```
    1126:       c6 44 04 10 00          mov    BYTE PTR [rsp+rax*1+0x10],0x0
- ``buf[strcspn(buf, "\n")] = '\0'``
- ```
    1123:       48 89 df                mov    rdi,rbx
    112b:       e8 10 ff ff ff          call   1040 <strlen@plt>
- ``rax = strlen(&(rsp+0x10)buf)``

- ```
    1130:       48 83 f8 0c             cmp    rax,0xc
    1134:       74 31                   je     1167 <__cxa_finalize@plt+0xd7>
- ``if (rax == 12)
    goto 1167``

- ```
    1136:       48 8d 3d d3 0e 00 00    lea    rdi,[rip+0xed3]        # 2010 <__cxa_finalize@plt+0xf80>
    113d:       e8 ee fe ff ff          call   1030 <puts@plt>
    1142:       b8 01 00 00 00          mov    eax,0x1
    1147:       48 8b 94 24 98 00 00    mov    rdx,QWORD PTR [rsp+0x98]
    114e:       00 
    114f:       64 48 2b 14 25 28 00    sub    rdx,QWORD PTR fs:0x28
    1156:       00 00 
    1158:       0f 85 d2 01 00 00       jne    1330 <__cxa_finalize@plt+0x2a0>
    115e:       48 81 c4 a0 00 00 00    add    rsp,0xa0
    1165:       5b                      pop    rbx
    1166:       c3                      ret

- ``else
    puts -> "invalid license"
    return 1``


**H1: *Alright, from this point on, we’re getting into the core encryption algorithm. I took a quick look; there seem to be several loops but they’re separate from one another. After each one, the result is checked against a specific value—if it matches, it proceeds to the next loop.***