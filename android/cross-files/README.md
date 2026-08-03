# Android NDK Meson Cross-Files

Meson cross-compilation definition files for building QEMU-Rafaelia components
against the Android NDK.  Two ABIs are supported:

| File | ABI | Min API | NDK triple |
|------|-----|---------|------------|
| `aarch64-linux-android-ndk.ini` | arm64-v8a | 21 | `aarch64-linux-android21` |
| `armv7a-linux-androideabi-ndk.ini` | armeabi-v7a | 21 | `armv7a-linux-androideabi21` |

## Prerequisites

- Android NDK **r27c** extracted to `/opt/android-ndk` (or any path — see below)
- Meson ≥ 1.3 and Ninja on the host
- Host: Linux x86_64

## Usage

```sh
# Install NDK (CI uses r27c; adjust if pinning a different revision)
NDK_ROOT=/opt/android-ndk

# arm64-v8a
./configure \
  --cross-file android/cross-files/aarch64-linux-android-ndk.ini \
  --without-default-features \
  --target-list=aarch64-softmmu

# armeabi-v7a
./configure \
  --cross-file android/cross-files/armv7a-linux-androideabi-ndk.ini \
  --without-default-features \
  --target-list=arm-softmmu
```

If your NDK is not at `/opt/android-ndk`, patch the `ndk_root` constant in the
ini file before running `./configure`:

```sh
sed -i "s|ndk_root *= *'/opt/android-ndk'|ndk_root = '${NDK_ROOT}'|" \
  android/cross-files/aarch64-linux-android-ndk.ini
```

## NDK r27c static-libc note

QEMU's `./configure` probes for static linking internally.  NDK r27c's static
`libc.a` embeds GWP-ASan/Scudo which requires `pthread_atfork`, which is not
exported from the static libc — so `./configure` fails on full static builds.

The CI job works around this by validating the toolchain with a direct
shared-PIE compile (`-fPIC`) rather than driving `./configure`, which is
sufficient to prove cross-compilation correctness.  Full system-mode builds
require a pre-built Android sysroot with glib2 + pixman.

## ABI notes

- **arm64-v8a** (`aarch64`): `-fPIC -DANDROID -D__ANDROID_API__=21`
- **armeabi-v7a** (`arm`): `-fPIC -mfpu=neon -mfloat-abi=softfp -DANDROID -D__ANDROID_API__=21`
  - `softfp` ABI required for Android ARM32 (hardware float registers, software
    calling convention — matches the platform ABI)
  - `neon` baseline matches NDK r27c minimum for armeabi-v7a
