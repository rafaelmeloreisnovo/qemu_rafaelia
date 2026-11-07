# RAFCODE-Φ QEMU freestanding byte kernel v1

Status: isolated authorial candidate, **not integrated into QEMU runtime**.

## Boundary / licensing

- Upstream QEMU stays under the repository's original `COPYING`, `LICENSE`, and individual file notices, including GPLv2 for the emulator as a whole.
- These **newly authored** paths alone are offered under `LICENSE-MIT` (MIT, GPLv2-compatible); this does **not** relicense inherited QEMU or third-party code.
- No borrowed QEMU source, headers, firmware, ABI structs or algorithms are included in this subtree. Rights claims outside these paths remain `TOKEN_VAZIO`.
- Combining these MIT files into a GPLv2 QEMU binary does not free that combined distribution from applicable GPL terms.

## Scope

`raf_fs_load_{le,be}32`, `raf_fs_store_{le,be}32`, and `raf_fs_move` operate on caller-owned bounded byte ranges. No libc headers, heap, malloc, QEMU headers, syscalls, ELF loader, OS services, external runtime calls, or automatic I/O. Invalid ranges return -1 without modifying buffers. The 4-byte operations do not assume alignment or host endian order.

A compiler and linker are **build tools**, not runtime dependencies. C11 plus Clang/GCC builtin integer type macros are required. A freestanding core is not the same as a runnable self-contained QEMU emulator.

## Gates / reproducer

`CC=gcc sh rafcode/qemu_fs_v1/run-local.sh` and `CC=clang sh rafcode/qemu_fs_v1/run-local.sh`. Each performs O0, O2 and Os, 32768 deterministic range/overlap cases, four endian APIs and invalid bounds, plus `nm -u` for the core object. ASan+UBSan can be used for host-only falsification.

Static cross-link **inspection** (not execution, not a guest boot):

```sh
cd rafcode/qemu_fs_v1
for target in armv7a-linux-gnueabi aarch64-linux-gnu i386-linux-gnu x86_64-linux-gnu; do
  clang --target="$target" -O2 -std=c11 -ffreestanding -fno-builtin \
    -fno-stack-protector -nostdinc -fuse-ld=lld -nostdlib -static \
    -Wl,--no-undefined -Wl,-e,raf_fs_move raf_fs.c -o "/tmp/raf-$target.elf"
  test -z "$(nm -u "/tmp/raf-$target.elf")" || exit 1
  ! readelf -l "/tmp/raf-$target.elf" | grep -q INTERP || exit 1
done
```

The chosen link entry is a **link-only sentinel**, not a guest-executable start routine. No actual device boot is represented.

## ASM, bit patterns and performance

Prefer portable byte operations until disassembly and independently reproduced benchmarks justify an architecture-specific branch. Optional inline asm is limited to guarded ABI-specific backends, with byte-for-byte differential tests. Hex/binary immediates and comments do not intrinsically make machine code faster. Flags, instruction selection, alignment and measured cache/branch behavior matter.

## Next integration gate (HOLD)

1. Select an **actual** QEMU call site and independently compare byte semantics; do not edit upstream code before equivalence evidence.
2. Compile/inspect ARMv7/AArch64/i386/x86_64; no interpreter, undefined import, malloc or syscalls in this isolated ELF.
3. Review license matrix, existing QEMU tests, architecture ABI, Android hardware execution and benchmarks.
4. Keep PR draft and `claim_allowed=false` until those gates are met.

Rollback: `master` unchanged; drop/revert only this isolated subtree in a reviewed successor, no history rewrite.
