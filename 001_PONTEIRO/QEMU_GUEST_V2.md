# QEMU RAFAELIA — hot pointer, guest-address candidate v2

- **Authority:** `rafaelmeloreisnovo/qemu_rafaelia`, branch `master` retains upstream sources and licenses; draft changes isolated.
- **Current baseline:** `master` commit `2931c6544ca9e37f823402beb18dd20cda22d952` (PR #70 merged, 2026-11-07).
- **Producer source:** `rafcode/qemu_guest_v2/raf_guest.h`, `raf_guest.c`.
- **Tests:** `test_raf_guest.c`, `run-local.sh`, `.github/workflows/rafcode-qemu-guest-v2.yml`.
- **Licenses:** newly authored subtree `LICENSE-MIT`; upstream QEMU inherits `COPYING`/`LICENSE` and path-specific notices. No relicensing of the upstream work.
- **Contract:** 64-bit guest addressing, caller-owned bounded arena, transactional failure semantics, no malloc/libc/syscalls; NOT a complete QEMU replacement.
- **Evidence:** local GCC/Clang O0/O2/Os and static 4-ABI link check; GitHub exact-head CI/guest boot remain unverified until independent run.
- **Gate:** `DRAFT/HOLD`; `claim_allowed=false`; `VM_BOOT=TOKEN_VAZIO`; `ANDROID_E2E=TOKEN_VAZIO`; `QEMU_RUNTIME_INTEGRATION=NOT_IMPLEMENTED`.
- **Next:** compare with one real QEMU call site and test semantics before integration. Rollback: discard isolated branch, preserve `master`.
