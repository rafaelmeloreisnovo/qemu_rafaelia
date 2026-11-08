# QEMU RAFAELIA — authorial A32 engine v3 (DRAFT/HOLD)

Source-level freestanding C11 **architectural subset**, NOT QEMU replacement, not a CPU cycle or silicon model. No QEMU code/headers copied. Depends only on pre-existing authorial `qemu_guest_v2` byte-window kernel. Built with compiler/linker tooling; execution core contains no libc/CRT/malloc/heap/syscalls or file/network I/O.

## Specific contracts
| File | Contract |
| --- | --- |
| raf_cpu.c | 16 A32 general registers, CPSR subset, retired count |
| raf_decode.c | exact subset of A32 opcodes: MOV/ADD/SUB immediate (S=0), CMP immediate (S=1), LDR/STR aligned immediate-preindexed words, B, SVC host trap, IRQ return `SUBS PC,LR,#4` |
| raf_mmu.c | deterministic 8-entry 4KiB software page mapping and R/W/X permissions, not ARM CP15 short-descriptor MMU |
| raf_irq.c | single pending IRQ, CPSR mask, vector offset +0x18, banked SP/LR, SPSR save; System/IRQ A32 modes only |
| raf_block.c | 512-byte RAM-backed sector DMA; single mapped-page only, no persistent backend, virtio, or MMIO |
| raf_exec.c | fetch/decode/condition/execute, explicit faults; no JIT/TCG |
| raf_run.c | bounded execution budget, explicit SVC/IRQ traps |

SVC is an explicit host-visible stop, **not** a modeled ARM supervisor-mode exception. Processor modes other than System/IRQ, Thumb, MMIO, cache, TLB, SMP, atomics, barriers, CP15, unaligned ARM memory semantics, asynchronous devices, DMA concurrency, clock cycles, and full ARMv7 are **NOT_IMPLEMENTED**.

## Gates
```sh
CC=gcc sh rafcode/qemu_a32_v3/run-local.sh
CC=clang sh rafcode/qemu_a32_v3/run-local.sh
```
Host-only tests execute a miniature A32 binary in caller-owned RAM: registers, conditional branch, word stores/loads, zero flag, software MMU permission faults, IRQ entry/return, DMA sector roundtrip, unsupported instructions, bounded execution. CI compiles four freestanding target ABIs and inspects static ELF for no dynamic interpreter and no undefined symbols. Link-only ELF sentinel entry is not a runnable Android program.

**Security and correctness boundary:** caller must provide valid nonoverlapping RAM and disk backing. No automatic allocation. Failed accesses do not advance PC, retire an instruction, or partially mutate stored bytes. Code and data reside on distinct R/X and R/W pages. No semantic claim beyond tested subset.

## Promotion
Compare against an independently obtained ARM ARM reference/real QEMU traces for exact A32 semantics; add ARMv7 MMU descriptors, CP15 fault registers, exception modes, multi-page memory transactions, peripheral bus, block controller, hardware-backed benchmarks, and Moto E7 end-to-end test receipts. Preserve upstream QEMU licenses and sources. `claim_allowed=false` for QEMU replacement, hardware equivalence, or Android guest boot.
