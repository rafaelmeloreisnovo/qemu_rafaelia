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

binary_map = qemu_exec.get("binary", {})
sha_map = qemu_exec.get("sha256", {})
if not binary_map:
    errors.append("qemu-exec.json binary map is empty")
if not sha_map:
    errors.append("qemu-exec.json sha256 map is empty")

for arch, rel in binary_map.items():
    path = root / rel
    if not path.is_file():
        errors.append(f"missing binary for {arch}: {rel}")
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if sha_map.get(rel) != digest:
        errors.append(f"sha mismatch for {rel}")

build_paths = {entry.get("path") for entry in build_info.get("binaries", [])}
for rel in binary_map.values():
    if rel not in build_paths:
        errors.append(f"BUILD_INFO missing binary path: {rel}")

if errors:
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    sys.exit(1)

print("Producer artifact contract OK")
print(f"source_commit={qemu_exec['source_commit']}")
print("architectures=" + ",".join(sorted(binary_map)))
PY
