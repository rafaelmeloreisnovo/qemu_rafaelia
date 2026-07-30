# QEMU RAFAELIA — Fork com Integração RAFAELIA/RMR

**Estado:** `ACTIVE`  
**Proprietário lógico:** `runtime-maintainer`

## Propósito

Fork do QEMU com camadas de integração RAFAELIA: Runtime Memory Reduction (RMR), instrumentação determinística de sistema, e roteamento low-latency para emulação ARM64/Android. O objetivo é fornecer uma base de emulação QEMU com primitivas freestanding compatíveis com o ecossistema RAFAELIA.

## Leitura recomendada

| Documento | Papel | Estado |
|---|---|---|
| [`README.rst`](README.rst) | README original do QEMU upstream | `REFERENCE` |
| [`README_RAFAELIA.md`](README_RAFAELIA.md) | Visão geral das modificações RAFAELIA | `ACTIVE` |
| [`QEMU_IMPROVEMENTS_README.md`](QEMU_IMPROVEMENTS_README.md) | Melhorias específicas implementadas | `ACTIVE` |
| [`INTEGRATION_ARCHITECTURE.md`](INTEGRATION_ARCHITECTURE.md) | Arquitetura de integração | `REFERENCE` |
| [`INTEGRATION_GUIDE.md`](INTEGRATION_GUIDE.md) | Guia de integração passo a passo | `REFERENCE` |
| [`RAFAELIA_IMPLEMENTATION.md`](RAFAELIA_IMPLEMENTATION.md) | Detalhes de implementação | `ACTIVE` |
| [`RAFAELIA_QEMU_ARTIFACT_CONTRACT.md`](RAFAELIA_QEMU_ARTIFACT_CONTRACT.md) | Contrato de artefatos RAFAELIA/QEMU | `AUDIT` |
| [`RMR/README.md`](RMR/README.md) | Módulo RMR (Runtime Memory Reduction) | `ACTIVE` |
| [`RMR/INSTRUMENTOS.md`](RMR/INSTRUMENTOS.md) | Instrumentos de sistema RMR | `REFERENCE` |

## Camadas RAFAELIA sobre o QEMU

### RMR — Runtime Memory Reduction

O módulo RMR (`RMR/`) implementa:

- **Alocação sem zero-initialize**: pool de alocação rápida para estruturas hot inicializadas pelo chamador
- **Primitivas memória/string com fast-path ASM**: `memzero`/`memcpy` com `rep stosb/movsb` (x86_64) e fallback por palavras
- **Roteamento determinístico**: seleção de lane (`fallback→kvm`) por score low-level para reduzir jitter
- **Snapshot de sistema low-level**: arquitetura, kernel, CPU, RAM, paginação e disponibilidade de KVM sem camadas extras

### Emulação Android/ARM64

O fork inclui suporte direcionado para emulação Android (ver `android/vectras-vm-android/`) com:

- Configurações de máquina Android
- Integração com o ecossistema Vectras-VM

## Build

Seguir o processo de build padrão do QEMU upstream (ver `README.rst`). As modificações RAFAELIA não alteram o sistema de build.

## Estados de evidência

| Gate | Estado |
|---|---|
| Build QEMU padrão com modificações RMR | `TOKEN_VAZIO` |
| Emulação Android ARM64 funcional | `TOKEN_VAZIO` |
| Benchmarks RMR vs baseline | `TOKEN_VAZIO` |

## Fronteira upstream

Este é um fork. O código upstream (QEMU) mantém suas licenças originais (GPL-2.0+). As adições RAFAELIA estão marcadas nos arquivos `RAFAELIA_*` e no diretório `RMR/`.
