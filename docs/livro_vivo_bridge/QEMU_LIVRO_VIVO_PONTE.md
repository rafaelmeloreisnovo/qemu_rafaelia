# Ponte Livro Vivo — QEMU, Emulação e Smoke Tests

> Modo: ponte operacional entre `qemu_rafaelia` e o Livro Vivo RAFAELIA  
> Status inicial: `RESULTADO_COMPUTACIONAL` quando houver execução reproduzível  
> Regra: emulação não é dispositivo real; é laboratório controlado

## Parábola do mapa e da pedra

O cartógrafo desenhou uma montanha perfeita.

O discípulo treinou olhando o desenho e disse:

— Já subi.

O mestre apontou para a montanha verdadeira:

— O desenho ensina o caminho. O joelho conhece a pedra.

Assim é QEMU: mapa poderoso, mas não substitui o dispositivo real.

## Invariante

```text
arquitetura alvo → imagem → emulação → comando → log → smoke test
```

Forma compacta:

```math
Inv(QEMU)=Arch\rightarrow Image\rightarrow Emulator\rightarrow Command\rightarrow Observable\ Log
```

## Risco principal

| Risco | Correção |
|---|---|
| confundir emulação com hardware real | marcar camada QEMU vs device |
| imagem sem checksum | registrar origem e hash |
| comando pesado demais | criar smoke test mínimo |
| arquitetura indefinida | declarar arm/aarch64/x86 alvo |
| log sem artefato | salvar saída em arquivo versionável |

## Próximos passos

1. Criar `QEMU_OPERATIONAL_MATRIX.md`.
2. Definir alvo mínimo: `qemu-system-arm` ou `qemu-system-aarch64`.
3. Declarar imagem, kernel, initrd e checksum.
4. Criar smoke test que apenas inicia, imprime marcador e encerra.
5. Separar limites de emulação e limites de dispositivo físico.

## Ficha Livro Vivo

```yaml
repo: rafaelmeloreisnovo/qemu_rafaelia
familia: Emulacao/QEMU
invariante: "arquitetura alvo → imagem → emulação → comando → log → smoke test"
selo: RESULTADO_COMPUTACIONAL
risco: "confundir QEMU com dispositivo real, imagem sem checksum e comando sem smoke test"
proximo_passo: "criar QEMU_OPERATIONAL_MATRIX.md e smoke test mínimo"
```

## Retroalimentar[3]

- **F_ok:** QEMU recebe ponte para separar laboratório emulado de dispositivo real.
- **F_gap:** falta inventário de imagens, comandos e arquiteturas alvo.
- **F_next:** criar `QEMU_OPERATIONAL_MATRIX.md`.
