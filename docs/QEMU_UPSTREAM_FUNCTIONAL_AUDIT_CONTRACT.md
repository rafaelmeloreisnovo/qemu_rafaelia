# QEMU RAFAELIA — upstream functional audit contract

This contract defines a CI audit; it is not a release approval, a performance
claim, or proof of Android-native system-mode execution.

## Scope and source of truth

- The audited tree is the current `qemu_rafaelia` commit.
- The comparison control is the configured `qemu/qemu` upstream ref, fetched in
  CI and recorded by immutable commit ID.
- The merge base, changed paths, numerical diff, and review surfaces are emitted
  as JSON. A missing upstream ref or merge base is `TOKEN_VAZIO` and fails the
  job rather than falling back to an unrecorded control.
- The CI only reads the upstream remote. It never merges, rebases, pushes, or
  changes the upstream tree.

## Evidence pipeline

1. Fetch the named QEMU upstream ref and resolve `HEAD`, upstream commit, and
   merge base.
2. Classify every path changed since the merge base. TCG, CPU target, system,
   hardware-core, QMP, migration, block, and command-line changes are marked for
   human functional review; a difference alone is neither a failure nor proof of
   a feature.
3. Inspect static wiring for the RAFAELIA runtime and the QEMU Meson source list.
4. Build and execute `hw/core/Makefile.integration`.
5. Build `qemu-system-x86_64` from the audited tree and run `--version`.
6. Re-write the report with the two executed observations and upload it as a CI
   artifact retained for 30 days.

The normal build CI remains responsible for its existing multi-target and
Android cross-toolchain coverage. This audit does not re-label a host build as
native Android execution.

## Evidence states

| State | Meaning |
| --- | --- |
| `OBSERVED_IN_CI` | The named command ran in this workflow and returned success. |
| `STATIC_EVIDENCE` | The code or build metadata contains the inspected wiring. |
| `TEST_DECLARED` | A test entrypoint exists, but this report has no successful run observation. |
| `REVIEW_REQUIRED` | The source delta is comparable but needs technical review. |
| `TOKEN_VAZIO` | Evidence is absent or insufficient; it is never a pass. |

The audit intentionally retains `TOKEN_VAZIO` for TCG hot-path optimisation,
Android native system execution, and performance improvement until a controlled
benchmark contract supplies an upstream control, environment, workload, samples,
and raw results.

## Generated artifacts

- `qemu_audit_summary.json`: provenance, review surfaces, observations, and gate
  outcome.
- `qemu_upstream_delta.json` and `.md`: exact file-level delta from merge base.
- `qemu_capability_inventory.json`: static RAFAELIA source/wiring inventory.
- `qemu_claims.json`: explicit claims and their evidence state.
- `qemu-system-x86_64.version.txt`: executable-startup observation.

## Gate policy

The workflow fails if the upstream comparison cannot be established, the
integration test fails, the QEMU system executable cannot be built or started,
or the fork delta has whitespace errors. It does not auto-fail simply because a
review-required QEMU subsystem differs: that result needs a reviewer to inspect
the recorded diff and decide whether a new test, benchmark, or design review is
required.

License and distribution decisions stay outside this CI contract. The audit
preserves source evidence and does not combine or re-license external projects.
