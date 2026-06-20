# RAFAELIA QEMU Artifact Contract

## Objetivo

Este contrato define como `qemu_rafaelia` deve entregar artifacts consumíveis pelo `Vectras-VM-Android` sem transformar o app Android em vendor interno do QEMU.

```text
qemu_rafaelia = produtor do motor QEMU/RAFAELIA
Vectras-VM-Android = consumidor Android, executor e auditor do motor
```

## Invariante

```text
source commit pinned + binaries packaged + SHA256SUMS + qemu-exec.json + BUILD_INFO.json
```

O Vectras não deve depender de `master` flutuante nem procurar binários por nome quando o artifact não foi validado.

## Layout esperado do artifact

```text
qemu-rafaelia-artifact-<commit>.tar.gz
└── qemu-rafaelia-artifact/
    ├── bin/
    │   ├── qemu-system-x86_64-rafacodephi      # opcional, se construído
    │   ├── qemu-system-aarch64-rafacodephi     # opcional, se construído
    │   ├── qemu-system-i386-rafacodephi        # opcional, se construído
    │   ├── qemu-system-ppc-rafacodephi         # opcional, se construído
    │   ├── qemu-system-x86_64                  # opcional, se construído
    │   ├── qemu-system-aarch64                 # opcional, se construído
    │   ├── qemu-system-i386                    # opcional, se construído
    │   └── qemu-system-ppc                     # opcional, se construído
    ├── qemu-exec.json
    ├── BUILD_INFO.json
    ├── SHA256SUMS.txt
    └── LICENSES/
```

## `qemu-exec.json`

Arquivo consumível pelo `Vectras-VM-Android` para alimentar `QemuExecConfig`/`QemuBinaryResolver` após instalação local do artifact.

Formato mínimo:

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "version": "10.2.50-rafaelia",
  "binary": {
    "x86_64": "bin/qemu-system-x86_64-rafacodephi",
    "arm64": "bin/qemu-system-aarch64-rafacodephi",
    "i386": "bin/qemu-system-i386-rafacodephi",
    "ppc": "bin/qemu-system-ppc-rafacodephi"
  },
  "sha256": {
    "bin/qemu-system-x86_64-rafacodephi": "..."
  }
}
```

Se os binários `-rafacodephi` não estiverem presentes, o empacotador pode registrar os nomes QEMU base, desde que o SHA256 esteja presente.

## `BUILD_INFO.json`

Campos mínimos:

```json
{
  "source_repo": "rafaelmeloreisnovo/qemu_rafaelia",
  "source_commit": "<git-sha>",
  "source_branch": "master",
  "qemu_version": "10.2.50",
  "built_at_utc": "<iso8601>",
  "host": "<uname>",
  "binaries": [
    {
      "path": "bin/qemu-system-aarch64-rafacodephi",
      "sha256": "...",
      "size_bytes": 0
    }
  ]
}
```

## Responsabilidade do Vectras

O Vectras deve:

1. importar ou receber o artifact;
2. verificar `SHA256SUMS.txt`;
3. instalar binários em caminho controlado (`filesDir/distro/usr/bin` ou `filesDir/usr/bin`);
4. copiar/gerar `qemu-exec.json` com paths locais finais;
5. executar `VectrasRuntimePreflight` antes de iniciar VM;
6. registrar `source_repo`, `source_commit`, `binary path` e SHA no ledger/runtime report.

## Responsabilidade do qemu_rafaelia

O `qemu_rafaelia` deve:

1. manter o código QEMU/RAFAELIA e IPC;
2. gerar binários por alvo quando possível;
3. empacotar os binários com checksums;
4. publicar artifacts em CI/release;
5. preservar licença/fonte conforme a GPLv2 do QEMU;
6. nunca exigir que o Vectras importe a árvore completa do QEMU para iniciar uma VM.

## Gate de aceitação

Um artifact só é consumível pelo Vectras se:

- possui ao menos um `qemu-system-*` executável;
- possui `SHA256SUMS.txt`;
- possui `qemu-exec.json` válido;
- possui `BUILD_INFO.json` com commit de origem;
- o commit de origem está pinado no manifesto do Vectras.

## Fórmula compacta

```text
QEMU compila fora; artifact prova; Vectras valida e executa.
```
