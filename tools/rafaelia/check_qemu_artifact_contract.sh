#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: tools/rafaelia/check_qemu_artifact_contract.sh --artifact-root <dir>

Producer-side contract check for qemu_rafaelia artifacts.
Validates:
  - qemu-exec.json
  - BUILD_INFO.json
  - SHA256SUMS.txt
  - at least one executable bin/qemu-system-*
  - JSON consistency and SHA256 integrity
  - explicit runtime OS/architecture/ABI
  - declared runtime architecture against executable file format
USAGE
}

ROOT=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact-root)
      ROOT="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "${ROOT}" ]]; then
  echo "ERROR: --artifact-root is required" >&2
  usage >&2
  exit 2
fi

if [[ ! -d "${ROOT}" ]]; then
  echo "ERROR: artifact root does not exist: ${ROOT}" >&2
  exit 1
fi

for cmd in python3 sha256sum find; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "ERROR: required command not found: ${cmd}" >&2
    exit 1
  fi
done

for required in qemu-exec.json BUILD_INFO.json SHA256SUMS.txt; do
  if [[ ! -f "${ROOT}/${required}" ]]; then
    echo "ERROR: missing ${required}" >&2
    exit 1
  fi
done

if ! find "${ROOT}/bin" -maxdepth 1 -type f -name 'qemu-system-*' -perm -111 | grep -q .; then
  echo "ERROR: no executable qemu-system-* binary found under ${ROOT}/bin" >&2
  exit 1
fi

(
  cd "${ROOT}"
  sha256sum -c SHA256SUMS.txt
)

python3 - "${ROOT}" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
qemu_exec = json.loads((root / "qemu-exec.json").read_text(encoding="utf-8"))
build_info = json.loads((root / "BUILD_INFO.json").read_text(encoding="utf-8"))

errors = []
if qemu_exec.get("source_repo") != "rafaelmeloreisnovo/qemu_rafaelia":
    errors.append("source_repo must be rafaelmeloreisnovo/qemu_rafaelia")
if not qemu_exec.get("source_commit"):
    errors.append("source_commit is empty in qemu-exec.json")
if qemu_exec.get("source_commit") != build_info.get("source_commit"):
    errors.append("source_commit mismatch")

runtime = qemu_exec.get("runtime")
build_runtime = build_info.get("runtime")
if not isinstance(runtime, dict):
    errors.append("qemu-exec.json runtime object is missing")
    runtime = {}
if runtime != build_runtime:
    errors.append("runtime mismatch between qemu-exec.json and BUILD_INFO.json")

runtime_os = str(runtime.get("os", "")).strip().lower()
runtime_arch = str(runtime.get("arch", "")).strip().lower()
runtime_abi = str(runtime.get("abi", "")).strip().lower()
if not runtime_os:
    errors.append("runtime.os is empty")
if not runtime_arch:
    errors.append("runtime.arch is empty")
if runtime_abi != f"{runtime_os}-{runtime_arch}":
    errors.append("runtime.abi must equal <runtime.os>-<runtime.arch>")
if runtime_os not in {"linux", "android", "darwin", "windows"}:
    errors.append(f"unsupported or ambiguous runtime.os: {runtime_os!r}")
if runtime_arch not in {"x86_64", "i386", "aarch64", "arm", "riscv32", "riscv64"}:
    errors.append(f"unsupported or ambiguous runtime.arch: {runtime_arch!r}")

binary_map = qemu_exec.get("binary", {})
sha_map = qemu_exec.get("sha256", {})
if not binary_map:
    errors.append("qemu-exec.json binary map is empty")
if not sha_map:
    errors.append("qemu-exec.json sha256 map is empty")

build_entries = {
    entry.get("path"): entry
    for entry in build_info.get("binaries", [])
    if isinstance(entry, dict) and entry.get("path")
}


def format_matches_runtime(fmt: str, arch: str) -> bool:
    value = fmt.lower()
    if arch == "x86_64":
        return "x86-64" in value or "x86_64" in value
    if arch == "i386":
        return "80386" in value or "intel 386" in value or "i386" in value
    if arch == "aarch64":
        return "aarch64" in value or "arm64" in value
    if arch == "arm":
        return "arm" in value and "aarch64" not in value and "arm64" not in value
    if arch == "riscv32":
        return "risc-v" in value and "32-bit" in value
    if arch == "riscv64":
        return "risc-v" in value and "64-bit" in value
    return False


for guest_arch, rel in binary_map.items():
    path = root / rel
    if not path.is_file():
        errors.append(f"missing binary for guest {guest_arch}: {rel}")
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if sha_map.get(rel) != digest:
        errors.append(f"sha mismatch for {rel}")

    entry = build_entries.get(rel)
    if entry is None:
        errors.append(f"BUILD_INFO missing binary path: {rel}")
        continue
    executable_format = str(entry.get("executable_format", "")).strip()
    if not executable_format or executable_format.startswith("UNRESOLVED:"):
        errors.append(f"unresolved executable format for {rel}: {executable_format!r}")
    elif not format_matches_runtime(executable_format, runtime_arch):
        errors.append(
            f"runtime architecture {runtime_arch} does not match {rel}: {executable_format}"
        )

for rel in sha_map:
    if rel not in build_entries:
        errors.append(f"BUILD_INFO missing checksummed path: {rel}")

if errors:
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    sys.exit(1)

print("Producer artifact contract OK")
print(f"source_commit={qemu_exec['source_commit']}")
print(f"runtime_abi={runtime_abi}")
print("guest_architectures=" + ",".join(sorted(binary_map)))
PY
