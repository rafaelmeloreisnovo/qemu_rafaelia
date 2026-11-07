# RAFCODE-Φ — QEMU guest address window v2

**State: DRAFT/HOLD.** Isolated freestanding C11 candidate; NOT linked to QEMU, TCG, Android, or any guest boot path.

## What changed relative to v1

The v1 byte kernel operates on offsets only. This v2 candidate adds a 64-bit guest physical address window over one caller-owned contiguous buffer, with overflow-aware initialization and subtraction-based range validation. It supports unaligned little-endian 32-bit loads/stores and overlap-safe moves entirely inside one window. Invalid ranges return `-1`; outputs and stored bytes remain unchanged. No I/O, MMIO, page tables, KVM, CPU/TCG execution or virtualization is implemented. The caller must provide a genuine allocation of at least `length` bytes and must not alias `out` with the window.

`raf_guest_init` rejects zero capacity and any window whose last byte cannot be represented by a 64-bit address. A valid window may include address `UINT64_MAX` as its final byte. The unrepresentable one-past-end address cannot be requested at that boundary. Zero-length moves use only representable, bounded start addresses.

## License & authority

New files in this directory are MIT licensed (see `LICENSE-MIT`). QEMU upstream files remain under their own original GPL and component-specific licenses. The MIT SPDX for these files is not permission to relicense QEMU or bundled components. Neither source reuse nor ABI compatibility is claimed beyond these isolated APIs.

## Gates

```sh
CC=gcc sh rafcode/qemu_guest_v2/run-local.sh
CC=clang sh rafcode/qemu_guest_v2/run-local.sh
```

Each host compiler builds `-O0/-O2/-Os`, checks core undefined symbols, executes 32,768 deterministic differential move cases and adversarial address-boundary checks. CI additionally uses static **link-only** ARMv7/AArch64/i386/x86-64 ELF inspection. Linker entry is a sentinel, NOT `_start` or a runnable VM. Toolchain and host test CRT are build/test-time dependencies, not linked freestanding runtime dependencies.

## Integration gate / falsifiers

1. Identify a real QEMU guest-memory call site and its required semantics (MMIO, endian, atomicity, transactions, dirty tracking, threading). No substitution unless equivalent behavior is independently demonstrated.
2. Run baseline QEMU unit tests unchanged, plus regression/differential tests and independent code/license review.
3. Inspect per-ABI disassembly/ELF; execute guest boot on actual Android ARMv7/AArch64 before runtime or performance claims.
4. Provider gates and rollback must pass before any runtime wiring, PR promotion, or release.

**claim_allowed=false; runtime_integrated=false; physical_guest_boot=TOKEN_VAZIO.**

Rollback: abandon this draft branch or revert this isolated subtree without touching upstream code.
