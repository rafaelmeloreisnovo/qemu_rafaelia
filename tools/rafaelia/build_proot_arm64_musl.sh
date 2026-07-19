#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
OUT_DIR="${OUT_DIR:-dist/rafaelia-qemu-proot-arm64}"
DIAG_DIR="${DIAG_DIR:-dist/proot-arm64-diagnostics}"
TARGET_LIST="${TARGET_LIST:-x86_64-softmmu,aarch64-softmmu,i386-softmmu}"

mkdir -p "${DIAG_DIR}"

runtime_arch="$(uname -m)"
if [[ "${runtime_arch}" != "aarch64" ]]; then
  echo "ERROR: this producer must run in an AArch64 userspace; got ${runtime_arch}" >&2
  exit 1
fi

if ! ldd --version 2>&1 | grep -qi musl; then
  echo "ERROR: this producer requires a musl userspace" >&2
  ldd --version 2>&1 || true
  exit 1
fi

for cmd in bash file git meson ninja pkg-config python3 sha256sum tar; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "ERROR: required command not found: ${cmd}" >&2
    exit 1
  fi
done

{
  printf 'source_commit=%s\n' "$(git rev-parse HEAD 2>/dev/null || echo unknown)"
  printf 'runtime_os=linux\n'
  printf 'runtime_arch=aarch64\n'
  printf 'runtime_libc=musl\n'
  printf 'execution_mode=proot\n'
  printf 'guest_targets=%s\n' "${TARGET_LIST}"
  printf 'uname=%s\n' "$(uname -a)"
  printf 'ldd=%s\n' "$(ldd --version 2>&1 | head -n1)"
} > "${DIAG_DIR}/BUILD_ATTEMPT.txt"

rm -rf "${BUILD_DIR}" "${OUT_DIR}"

./configure \
  --target-list="${TARGET_LIST}" \
  --disable-werror \
  --disable-docs \
  --disable-gtk \
  --disable-sdl \
  --disable-curses \
  --disable-vnc-jpeg \
  --disable-spice \
  --prefix=/usr/local \
  2>&1 | tee "${DIAG_DIR}/configure.log"

set -o pipefail
ninja -C "${BUILD_DIR}" -v \
  qemu-system-x86_64 \
  qemu-system-aarch64 \
  qemu-system-i386 \
  2>&1 | tee "${DIAG_DIR}/build.log"

for binary in \
  "${BUILD_DIR}/qemu-system-x86_64" \
  "${BUILD_DIR}/qemu-system-aarch64" \
  "${BUILD_DIR}/qemu-system-i386"; do
  test -x "${binary}"
  file "${binary}" | tee -a "${DIAG_DIR}/executable-formats.txt"
done

bash tools/rafaelia/package_qemu_artifact.sh \
  --build-dir "${BUILD_DIR}" \
  --out-dir "${OUT_DIR}" \
  --runtime-os linux \
  --runtime-arch aarch64 \
  --runtime-libc musl \
  --execution-mode proot

bash tools/rafaelia/check_qemu_artifact_contract.sh \
  --artifact-root "${OUT_DIR}/qemu-rafaelia-artifact" \
  | tee "${DIAG_DIR}/contract-check.txt"

cp -v "${BUILD_DIR}/config.log" "${DIAG_DIR}/" 2>/dev/null || true
cp -v "${BUILD_DIR}/config-host.h" "${DIAG_DIR}/" 2>/dev/null || true
cp -v "${BUILD_DIR}/meson-logs/meson-log.txt" "${DIAG_DIR}/" 2>/dev/null || true

echo "PROVEN_BUILD: linux-aarch64 musl execution_mode=proot"
