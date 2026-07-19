# RAFAELIA QEMU Artifact Contract

## Objetivo

Este contrato define como `qemu_rafaelia` entrega artifacts auditáveis sem transformar o app Android em vendor interno da árvore QEMU.

```text
qemu_rafaelia = produtor do motor QEMU/RAFAELIA
Vectras-VM-Android = consumidor Android, instalador e auditor do motor
```

## Invariante

```text
source commit pinned
+ runtime ABI explícita
+ binaries packaged
+ SHA256SUMS
+ executable format
+ qemu-exec.json
+ BUILD_INFO.json
```

O nome `qemu-system-aarch64` descreve o **guest target emulado**. Ele não prova que o executável roda em host AArch64/Android.

Exemplo:

```text
qemu-system-aarch64
guest target = aarch64
runtime host do artifact observado em CI = linux-x86_64
```

Essa distinção é obrigatória.

## Classes de artifact

| Classe | runtime.os | runtime.arch | Consumível pelo Vectras Android |
|---|---|---|---|
| CI host Linux | `linux` | `x86_64` | não; somente build/teste/contrato |
| Android ARM64 | `android` | `aarch64` | sim, após ABI/hash/preflight |
| Android ARM32 | `android` | `arm` | sim, após ABI/hash/preflight |
| outro host | explícito | explícito | somente consumidor compatível |

Nenhum artifact pode ser promovido de uma classe para outra por renomear arquivos.

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

A presença de um arquivo é opcional por guest target; a identidade de runtime não é opcional.

## `qemu-exec.json`

Formato mínimo:

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "version": "10.2.50-rafaelia",
  "runtime": {
    "os": "android",
    "arch": "aarch64",
    "abi": "android-aarch64"
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

As chaves de `binary` representam guests; `runtime` representa onde os executáveis realmente podem rodar.

## `BUILD_INFO.json`

Campos mínimos:

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "source_branch": "<ref>",
  "qemu_version": "10.2.50-rafaelia",
  "built_at_utc": "<iso8601>",
  "build_runner": "<runner que empacotou>",
  "runtime": {
    "os": "android",
    "arch": "aarch64",
    "abi": "android-aarch64"
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

```bash
tools/rafaelia/package_qemu_artifact.sh \
  --build-dir build \
  --out-dir dist/rafaelia-qemu \
  --runtime-os linux \
  --runtime-arch x86_64

tools/rafaelia/check_qemu_artifact_contract.sh \
  --artifact-root dist/rafaelia-qemu/qemu-rafaelia-artifact
```

Em cross-build Android, os parâmetros devem declarar o runtime alvo:

```bash
--runtime-os android --runtime-arch aarch64
```

O checker cruza a arquitetura declarada com `executable_format`; um ELF x86-64 não passa como `android-aarch64`.

## Responsabilidade do Vectras

O Vectras deve:

1. importar artifact por commit/digest pinado;
2. verificar `SHA256SUMS.txt`;
3. exigir `runtime.os == android`;
4. exigir `runtime.arch` compatível com `Build.SUPPORTED_ABIS`;
5. rejeitar `linux-*`, mesmo que os guest targets estejam corretos;
6. instalar binários em caminho controlado;
7. executar preflight antes da VM;
8. registrar source commit, runtime ABI, binary path e SHA no ledger.

## Responsabilidade do qemu_rafaelia

O produtor deve:

1. manter código QEMU/RAFAELIA e IPC;
2. construir por runtime ABI e guest target;
3. declarar runtime sem inferência pelo nome do binário;
4. empacotar checksums e formatos executáveis;
5. publicar artifacts separados por runtime;
6. preservar licença/fonte GPLv2;
7. nunca exigir que o Vectras importe a árvore completa para iniciar uma VM.

## Gate de aceitação geral

Um artifact é válido como artifact QEMU quando:

- possui ao menos um `qemu-system-*` executável;
- possui `SHA256SUMS.txt` válido;
- possui `qemu-exec.json` e `BUILD_INFO.json` coerentes;
- possui runtime OS/arch/ABI explícitas;
- a arquitetura declarada corresponde ao formato executável;
- o source commit está pinado.

## Gate adicional Android

Para consumo pelo Vectras:

```text
runtime.os = android
runtime.arch ∈ {aarch64, arm}
+ dependências Android/prefix compatíveis
+ instalação/preflight/ADB
```

O artifact Linux produzido pelo CI multi-target fecha Q1/Q2 e prova o código QEMU, mas **não fecha Q3 Android/NDK**.

## Fórmula compacta

```text
guest target ≠ runtime ABI
QEMU compila fora; artifact declara; Vectras verifica; dispositivo prova.
```
