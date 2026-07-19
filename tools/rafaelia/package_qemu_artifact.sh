#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: tools/rafaelia/package_qemu_artifact.sh [options]

Options:
  --build-dir DIR
  --out-dir DIR
  --source-repo OWNER/REPO
  --version VERSION
  --runtime-os OS          Runtime OS of packaged executables (default: detected host)
  --runtime-arch ARCH      Runtime CPU architecture (default: detected host)
  --allow-missing

Packages built qemu-system-* binaries into a RAFAELIA artifact:
  - qemu-exec.json
  - BUILD_INFO.json
  - SHA256SUMS.txt
  - qemu-rafaelia-artifact-<short-sha>.tar.gz

The script does not build QEMU. It packages binaries already produced by a QEMU build.
For cross builds, --runtime-os and --runtime-arch are mandatory semantic inputs and
are checked against each executable's detected file format by the contract checker.
USAGE
}

BUILD_DIR="build"
OUT_DIR="dist/rafaelia-qemu"
SOURCE_REPO="rafaelmeloreisnovo/qemu_rafaelia"
VERSION=""
RUNTIME_OS=""
RUNTIME_ARCH=""
ALLOW_MISSING="false"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --source-repo)
      SOURCE_REPO="$2"
      shift 2
      ;;
    --version)
      VERSION="$2"
      shift 2
      ;;
    --runtime-os)
      RUNTIME_OS="$2"
      shift 2
      ;;
    --runtime-arch)
      RUNTIME_ARCH="$2"
      shift 2
      ;;
    --allow-missing)
      ALLOW_MISSING="true"
      shift
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

if [[ -z "${VERSION}" ]]; then
  if [[ -f VERSION ]]; then
    VERSION="$(tr -d '[:space:]' < VERSION)-rafaelia"
  else
    VERSION="unknown-rafaelia"
  fi
fi

for cmd in python3 sha256sum file; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "ERROR: ${cmd} is required" >&2
    exit 1
  fi
done

normalize_os() {
  local value="${1,,}"
  case "${value}" in
    linux|gnu/linux) echo "linux" ;;
    android) echo "android" ;;
    darwin|macos|macosx) echo "darwin" ;;
    windows|mingw*|msys*) echo "windows" ;;
    *) echo "${value}" ;;
  esac
}

normalize_arch() {
  local value="${1,,}"
  case "${value}" in
    amd64|x64|x86-64) echo "x86_64" ;;
    arm64|arm64-v8a) echo "aarch64" ;;
    armeabi-v7a|armv7|armv7l|armhf) echo "arm" ;;
    x86|i686|i386) echo "i386" ;;
    *) echo "${value}" ;;
  esac
}

if [[ -z "${RUNTIME_OS}" ]]; then
  RUNTIME_OS="$(normalize_os "$(uname -s)")"
else
  RUNTIME_OS="$(normalize_os "${RUNTIME_OS}")"
fi

if [[ -z "${RUNTIME_ARCH}" ]]; then
  RUNTIME_ARCH="$(normalize_arch "$(uname -m)")"
else
  RUNTIME_ARCH="$(normalize_arch "${RUNTIME_ARCH}")"
fi

if [[ -z "${RUNTIME_OS}" || -z "${RUNTIME_ARCH}" ]]; then
  echo "ERROR: runtime OS and architecture must be resolvable" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
SOURCE_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
SHORT_SHA="${SOURCE_COMMIT:0:12}"
ARTIFACT_ROOT="${OUT_DIR}/qemu-rafaelia-artifact"
BIN_DIR="${ARTIFACT_ROOT}/bin"
LICENSE_DIR="${ARTIFACT_ROOT}/LICENSES"

rm -rf "${ARTIFACT_ROOT}"
mkdir -p "${BIN_DIR}" "${LICENSE_DIR}"

candidates=(
  "qemu-system-x86_64-rafacodephi"
  "qemu-system-aarch64-rafacodephi"
  "qemu-system-i386-rafacodephi"
  "qemu-system-ppc-rafacodephi"
  "qemu-system-x86_64-rafaelia"
  "qemu-system-aarch64-rafaelia"
  "qemu-system-i386-rafaelia"
  "qemu-system-ppc-rafaelia"
  "qemu-system-x86_64"
  "qemu-system-aarch64"
  "qemu-system-i386"
  "qemu-system-ppc"
)

found=0
for name in "${candidates[@]}"; do
  src=""
  for base in "${BUILD_DIR}" "${BUILD_DIR}/qemu-system-${name#qemu-system-}" "."; do
    if [[ -x "${base}/${name}" ]]; then
      src="${base}/${name}"
      break
    fi
  done
  if [[ -n "${src}" ]]; then
    cp "${src}" "${BIN_DIR}/${name}"
    chmod 0755 "${BIN_DIR}/${name}"
    found=$((found + 1))
    echo "[artifact] included ${src}"
  fi
done

if [[ ${found} -eq 0 && "${ALLOW_MISSING}" != "true" ]]; then
  echo "ERROR: no qemu-system-* binaries found under ${BUILD_DIR}. Build QEMU first or pass --allow-missing for contract-only packaging." >&2
  exit 1
fi

if [[ -f LICENSE ]]; then
  cp LICENSE "${LICENSE_DIR}/QEMU-LICENSE"
fi
if [[ -f COPYING ]]; then
  cp COPYING "${LICENSE_DIR}/COPYING"
fi

(
  cd "${ARTIFACT_ROOT}"
  find bin -maxdepth 1 -type f -print0 | sort -z | xargs -0 -r sha256sum > SHA256SUMS.txt
)

python3 - \
  "${ARTIFACT_ROOT}" \
  "${SOURCE_REPO}" \
  "${SOURCE_COMMIT}" \
  "${SOURCE_BRANCH}" \
  "${VERSION}" \
  "${RUNTIME_OS}" \
  "${RUNTIME_ARCH}" <<'PY'
import hashlib
import json
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

root = Path(sys.argv[1])
source_repo, source_commit, source_branch, version = sys.argv[2:6]
runtime_os, runtime_arch = sys.argv[6:8]
bin_dir = root / "bin"

arch_order = [
    ("x86_64", ["qemu-system-x86_64-rafacodephi", "qemu-system-x86_64-rafaelia", "qemu-system-x86_64"]),
    ("arm64", ["qemu-system-aarch64-rafacodephi", "qemu-system-aarch64-rafaelia", "qemu-system-aarch64"]),
    ("i386", ["qemu-system-i386-rafacodephi", "qemu-system-i386-rafaelia", "qemu-system-i386"]),
    ("ppc", ["qemu-system-ppc-rafacodephi", "qemu-system-ppc-rafaelia", "qemu-system-ppc"]),
]

runtime = {
    "os": runtime_os,
    "arch": runtime_arch,
    "abi": f"{runtime_os}-{runtime_arch}",
}

sha_map = {}
binaries = []
for path in sorted(bin_dir.glob("qemu-system-*")):
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    rel = path.relative_to(root).as_posix()
    try:
        executable_format = subprocess.check_output(
            ["file", "-b", str(path)], text=True, stderr=subprocess.STDOUT
        ).strip()
    except Exception as exc:
        executable_format = f"UNRESOLVED:{type(exc).__name__}"
    sha_map[rel] = digest
    binaries.append({
        "path": rel,
        "sha256": digest,
        "size_bytes": path.stat().st_size,
        "executable_format": executable_format,
    })

binary_map = {}
for guest_arch, names in arch_order:
    for name in names:
        rel = f"bin/{name}"
        if rel in sha_map:
            binary_map[guest_arch] = rel
            break

qemu_exec = {
    "source_repo": source_repo,
    "source_commit": source_commit,
    "version": version,
    "runtime": runtime,
    "binary": binary_map,
    "sha256": sha_map,
}

build_info = {
    "source_repo": source_repo,
    "source_commit": source_commit,
    "source_branch": source_branch,
    "qemu_version": version,
    "built_at_utc": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
    "build_runner": platform.platform(),
    "runtime": runtime,
    "binaries": binaries,
}

(root / "qemu-exec.json").write_text(json.dumps(qemu_exec, indent=2, sort_keys=True) + "\n", encoding="utf-8")
(root / "BUILD_INFO.json").write_text(json.dumps(build_info, indent=2, sort_keys=True) + "\n", encoding="utf-8")
PY

mkdir -p "${OUT_DIR}"
TARBALL="${OUT_DIR}/qemu-rafaelia-artifact-${SHORT_SHA}.tar.gz"
tar -C "${OUT_DIR}" -czf "${TARBALL}" qemu-rafaelia-artifact

echo "Artifact ready: ${TARBALL}"
echo "Runtime ABI: ${RUNTIME_OS}-${RUNTIME_ARCH}"
echo "Files:"
find "${ARTIFACT_ROOT}" -maxdepth 2 -type f | sort
