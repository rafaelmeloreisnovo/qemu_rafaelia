# QEMU Rafaelia

Documentação operacional do fork **QEMU Rafaelia** com foco em coerência entre código, CI, build nativo e entrega de artefatos.

## 1) Objetivo do repositório

Este repositório mantém duas trilhas complementares:

1. **QEMU upstream build path** (Meson/Ninja) para garantir compilação base do projeto.
2. **RAFAELIA integration path** (`hw/core/Makefile.integration`) para validar o módulo de integração local.

A regra de ouro deste documento é: **o que está aqui deve ser reproduzível por comando e rastreável em CI**.

## 2) Fonte de verdade (build e CI)

- Workflow principal: `.github/workflows/ci-build.yml`
- Workflow de governança de PR no mirror: `.github/workflows/lockdown.yml`
- Build upstream: `./configure` + `ninja -C build ...`
- Build integração RAFAELIA: `make -f hw/core/Makefile.integration ...`

## 3) Execução local rápida

### 3.1 Smoke build alinhado com CI

```bash
./configure \
  --target-list=x86_64-softmmu \
  --disable-werror \
  --disable-docs \
  --disable-gtk \
  --disable-sdl \
  --disable-curses \
  --disable-vnc-jpeg

ninja -C build subprojects/libvhost-user/libvhost-user.a
```

### 3.2 Build do módulo de integração

```bash
make -f hw/core/Makefile.integration all
make -f hw/core/Makefile.integration test
```

## 4) Artefatos esperados

### CI (`ci-build.yml`)

O pipeline empacota:

- `build/subprojects/libvhost-user/libvhost-user.a`
- `build/config.log`
- `build/config-host.h`

Upload via `actions/upload-artifact@v4` com nome `qemu-smoke-build-artifacts`.

### Local

- Binário de teste de integração em `hw/core/` (alvo do `Makefile.integration`)
- Objetos/headers gerados da configuração QEMU em `build/`

## 5) Organização documental

- Auditoria canônica de divergência, ciclo, monitor e fronteira com o cache Vectras: `docs/RAFAELIA_QEMU_DIVERGENCE_AUDIT_2026-07-24.md`
- Documento técnico principal: `docs/RAFAELIA_TECHNICAL_DOCUMENTATION.md`
- Dossiê executivo/profissional: `docs/RAFAELIA_PROFESSIONAL_DOSSIER.md`
- Auditoria de árvore e cobertura: `docs/RAFAELIA_DIRECTORY_AUDIT.md`

## 6) Princípios de manutenção

1. Não publicar métrica sem comando verificável.
2. Não declarar release path sem upload de artefato configurado.
3. Não divergir doc de workflow: **workflow manda, doc explica**.
4. Corrigir causa-raiz estrutural antes de ajustar texto superficial.
5. Distinguir ciclo de runtime, instrumentação e cache; nenhum deles prova performance sem benchmark.
