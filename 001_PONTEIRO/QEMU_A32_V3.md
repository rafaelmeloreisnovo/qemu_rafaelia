# QEMU — RAFAELIA A32 v3 execution engine pointer
- ROOT: rafaelmeloreisnovo/qemu_rafaelia; base master@ebf02048e16ad29e8bff4ee7925e8328f0657b99 (PR71 merged).
- PRODUCER: rafcode/qemu_a32_v3/ (new MIT files only); dependent: rafcode/qemu_guest_v2/.
- ROUTE: CPU -> DECODE -> MMU R/W/X -> MEMORY -> IRQ -> BLOCK/DMA -> STEP/RUN.
- SCOPE: A32 subset software model, no replacement QEMU, no QEMU source edits.
- GATES: host GCC/Clang O0/O2/Os + ARMv7/AArch64/i386/x86-64 ELF link-only; review exact-head CI.
- GAPS: real CP15/MMU, TLB, multicore/cache/barriers, precise ARM exception semantics, Android Moto E7 boot E2E = TOKEN_VAZIO/NOT_IMPLEMENTED.
- CLAIM: claim_allowed=false; hold before merge; rollback branch abandonment / revert new files.
- EVIDENCE: PR exact SHA and CI links to be appended as successor receipt.
