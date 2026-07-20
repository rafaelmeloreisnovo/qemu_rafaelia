# RAFAELIA QEMU Artifact Contract

## Objetivo

Este contrato define como `qemu_rafaelia` entrega artifacts auditáveis sem transformar o app Android em vendor interno da árvore QEMU.

```text
qemu_rafaelia = produtor do motor QEMU/RAFAELIA
Vectras-VM-Android = consumidor, instalador e auditor do motor
```

## Invariante

```text
source commit pinned
+ guest targets explícitos
+ runtime OS/arch/ABI/libc
+ execution_mode
+ binaries packaged
+ SHA256SUMS
+ executable format
+ qemu-exec.json
+ BUILD_INFO.json
```

O nome `qemu-system-aarch64` descreve o **guest target emulado**. Ele não prova onde o executável roda.

```text
qemu-system-aarch64
├── guest target: aarch64
└── runtime possível: linux-x86_64, linux-aarch64 ou android-aarch64
```

Nenhum artifact pode mudar de classe por renomear arquivos.

## Quatro dimensões independentes

| Dimensão | Exemplo | Significado |
|---|---|---|
| guest target | `arm64` | arquitetura da máquina virtual emulada |
| runtime ABI | `linux-aarch64` | OS e CPU que executam o processo QEMU |
| libc | `musl` | ABI de userspace exigida pelo executável |
| execution mode | `proot` | mecanismo usado pelo Vectras para lançar o processo |

## Classes de artifact

| Classe | runtime | libc | execution_mode | Uso |
|---|---|---|---|---|
| CI host | `linux-x86_64` | `glibc` | `host_ci` | prova build, testes e contrato; não vai ao aparelho |
| PRoot ARM64 | `linux-aarch64` | `musl` ou `glibc` | `proot` | caminho coerente com `filesDir/distro` se a rootfs usar a mesma libc |
| PRoot ARM32 | `linux-arm` | `musl` ou `glibc` | `proot` | aparelho ARM32 com rootfs correspondente |
| Android nativo | `android-aarch64`/`android-arm` | `bionic` | `native_android` | exige launcher sem PRoot e dependências recompiladas para o prefixo Android |

O launcher canônico atual do Vectras executa o QEMU dentro de PRoot. Portanto, a classe prioritária de consumo é `proot`, não `native_android`.

## Layout esperado

```text
qemu-rafaelia-artifact-<commit>.tar.gz
└── qemu-rafaelia-artifact/
    ├── bin/
    │   ├── qemu-system-x86_64
    │   ├── qemu-system-aarch64
    │   ├── qemu-system-i386
    │   └── ...
    ├── qemu-exec.json
    ├── BUILD_INFO.json
    ├── SHA256SUMS.txt
    └── LICENSES/
```

## `qemu-exec.json`

Exemplo PRoot ARM64/musl:

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "version": "10.2.50-rafaelia",
  "runtime": {
    "os": "linux",
    "arch": "aarch64",
    "abi": "linux-aarch64",
    "libc": "musl",
    "execution_mode": "proot"
  },
  "binary": {
    "x86_64": "bin/qemu-system-x86_64",
    "arm64": "bin/qemu-system-aarch64",
    "i386": "bin/qemu-system-i386"
  },
  "sha256": {
    "bin/qemu-system-aarch64": "..."
  }
}
```

As chaves de `binary` representam guests; `runtime` representa o processo real.

## `BUILD_INFO.json`

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "source_branch": "<ref>",
  "qemu_version": "10.2.50-rafaelia",
  "built_at_utc": "<iso8601>",
  "build_runner": "<runner que empacotou>",
  "runtime": {
    "os": "linux",
    "arch": "aarch64",
    "abi": "linux-aarch64",
    "libc": "musl",
    "execution_mode": "proot"
  },
  "binaries": [
    {
      "path": "bin/qemu-system-aarch64",
      "sha256": "...",
      "size_bytes": 0,
      "executable_format": "ELF 64-bit LSB ... ARM aarch64 ..."
    }
  ]
}
```

## Scripts produtores

### Host CI Linux x86_64

```bash
tools/rafaelia/package_qemu_artifact.sh \
  --build-dir build \
  --out-dir dist/rafaelia-qemu \
  --runtime-os linux \
  --runtime-arch x86_64 \
  --runtime-libc glibc \
  --execution-mode host_ci
```

### PRoot Linux AArch64/musl

```bash
tools/rafaelia/package_qemu_artifact.sh \
  --build-dir build \
  --out-dir dist/rafaelia-qemu \
  --runtime-os linux \
  --runtime-arch aarch64 \
  --runtime-libc musl \
  --execution-mode proot
```

### Android/Bionic futuro

```bash
tools/rafaelia/package_qemu_artifact.sh \
  --runtime-os android \
  --runtime-arch aarch64 \
  --runtime-libc bionic \
  --execution-mode native_android
```

O checker cruza arquitetura declarada, formato executável e combinações de OS/libc/modo.

## Responsabilidade do Vectras

O consumidor deve:

1. importar artifact por commit e digest pinados;
2. verificar `SHA256SUMS.txt`;
3. selecionar somente `execution_mode` suportado pelo launcher;
4. para `proot`, exigir `runtime.os=linux` e libc igual à rootfs instalada;
5. para `native_android`, exigir `runtime.os=android`, `libc=bionic` e launcher nativo dedicado;
6. exigir `runtime.arch` compatível com o aparelho;
7. instalar binários em caminho controlado;
8. executar preflight antes da VM;
9. registrar source commit, runtime, caminho e SHA no ledger.

Um artifact `host_ci` nunca é consumível pelo app.

## Responsabilidade do qemu_rafaelia

O produtor deve:

1. construir por runtime e guest target;
2. declarar OS, CPU, libc e modo sem inferência pelo nome do binário;
3. empacotar checksums e formatos executáveis;
4. publicar artifacts separados por classe;
5. preservar licença/fonte GPLv2;
6. manter receita reproduzível das dependências;
7. nunca exigir que o Vectras importe a árvore completa para iniciar uma VM.

## Gates

### Gate geral

- ao menos um `qemu-system-*` executável;
- `SHA256SUMS.txt` válido;
- JSONs coerentes;
- runtime completo;
- arquitetura declarada compatível com ELF/PE/Mach-O;
- source commit pinado.

### Gate PRoot

```text
execution_mode = proot
runtime.os = linux
runtime.libc ∈ {musl, glibc}
runtime.arch compatível com aparelho
rootfs.libc = runtime.libc
```

### Gate Android nativo

```text
execution_mode = native_android
runtime.os = android
runtime.libc = bionic
prefix/package recompilados
launcher sem PRoot
ADB + preflight
```

## Estado de Q1–Q3

| Gap | Estado |
|---|---|
| Q1 binários `qemu-system-*` | `PROVEN_CI[LINUX_X86_64_HOST]` |
| Q2 packaging/contrato | `PROVEN_CI`, agora endurecido com runtime/libc/modo |
| Q3 runtime móvel | dividido em `PROOT_LINUX_ARM64` prioritário e `NATIVE_ANDROID_NDK` futuro |

O artifact Linux x86_64 verde prova o código e o empacotamento, mas não é executável no aparelho.

## Fórmula compacta

```text
guest target ≠ runtime ABI ≠ libc ≠ execution mode
QEMU compila; artifact declara; Vectras verifica; dispositivo prova.
```
