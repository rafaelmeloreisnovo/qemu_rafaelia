# Android NDK Build Contract — qemu_rafaelia

## Estado

```text
lane = NATIVE_ANDROID_NDK
state = BLOCKED_BY[ANDROID_DEPENDENCY_SYSROOT_AND_NATIVE_LAUNCHER]
claim_allowed = false
```

Este contrato impede que um build Linux/PRoot seja renomeado como Android e impede que um executável Bionic seja entregue a um launcher que ainda o executa dentro de PRoot.

## Distinção canônica

```text
qemu-system-aarch64 = guest target
android-aarch64 = runtime ABI
bionic = libc
native_android = execution mode
```

Os quatro campos devem aparecer em `qemu-exec.json` e `BUILD_INFO.json`.

## Pré-condições do build

Um build Android nativo exige um sysroot/prefixo completo para a mesma ABI e API level. No mínimo, a receita deve resolver de forma pinada:

- Android NDK/Clang;
- GLib;
- Pixman;
- zlib;
- libfdt/dtc;
- Capstone;
- libslirp;
- android-shmem ou equivalente compatível;
- bibliotecas adicionais habilitadas pela configuração QEMU;
- `pkg-config`/Meson cross file apontando somente para o sysroot Android.

Dependências de host Linux não podem vazar para o link Android.

## Prefixo e package identity

As dependências precisam ser compiladas para o package/prefixo do consumidor real. Não é permitido importar pacotes produzidos para outro app/prefixo e assumir compatibilidade.

O manifesto de build deve registrar:

```json
{
  "android": {
    "api_level": 21,
    "ndk_version": "<pin>",
    "package_name": "<consumer-package>",
    "prefix": "<consumer-prefix>",
    "sysroot_digest": "sha256:<digest>",
    "dependency_lock": "<path-or-digest>"
  }
}
```

## Cross file mínimo

A receita deve gerar um Meson cross file com:

- `c`, `cpp`, `ar`, `strip`, `pkg-config` do NDK/toolchain;
- `host_machine.system = android`;
- `host_machine.cpu_family` e `cpu` coerentes com a ABI;
- API level explícito;
- `PKG_CONFIG_LIBDIR` isolado no sysroot Android;
- nenhuma busca em `/usr/lib` ou `/usr/local/lib` do runner.

## Artifact esperado

```text
runtime.os = android
runtime.arch = aarch64 | arm
runtime.libc = bionic
runtime.execution_mode = native_android
```

O checker deve confirmar:

- ELF da arquitetura declarada;
- ausência de interpreter/glibc incompatível;
- dependências dinâmicas resolvíveis no package/prefixo;
- hashes e commit de origem;
- guest targets presentes.

## Gate do consumidor Vectras

A lane Android não pode ser consumida pelo launcher PRoot atual. Antes da integração, o Vectras precisa de:

1. launcher `ProcessBuilder` nativo, sem `proot`;
2. diretório executável controlado pelo app;
3. ambiente `PATH`, `LD_LIBRARY_PATH`, `TMPDIR`, PulseAudio/display coerente;
4. validação do `runtime.execution_mode`;
5. ABI contra `Build.SUPPORTED_ABIS`;
6. instalação atômica e rollback;
7. ADB smoke em ARM64 e, se suportado, ARM32;
8. registro de hashes no ledger.

## Fontes upstream de implementação

- Termux package recipe: `packages/qemu-system-*/build.sh` em `termux/termux-packages`;
- Termux build system: documentação oficial `Building-packages.md`;
- Termux execution environment: documentação oficial do package/prefix environment.

Essas fontes devem ser pinadas por commit quando a lane for implementada.

## Gate de promoção

```text
ANDROID_DEPENDENCY_LOCKED
∧ ANDROID_SYSROOT_REPRODUCIBLE
∧ ELF_BIONIC_VERIFIED
∧ NATIVE_LAUNCHER_IMPLEMENTED
∧ ADB_SMOKE_GREEN
→ NATIVE_ANDROID_PROVEN
```

Até lá, Q3 Android/NDK permanece explicitamente aberto, enquanto a lane PRoot Linux ARM64/musl pode avançar independentemente.
