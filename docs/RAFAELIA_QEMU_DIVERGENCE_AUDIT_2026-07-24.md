# Auditoria de divergência técnica — QEMU Rafaelia

## Metadados canônicos

- Data da auditoria: `2026-07-24`.
- Repositório: `rafaelmeloreisnovo/qemu_rafaelia`.
- Head auditado: `d62a0028ddb5dcbe5c48c3014abdd78a04f55ada`.
- Baseline histórico disponível: `45c0edebdb260746ca07174532273f46d83268a2` (`2026-03-07`).
- Distância observada desde o baseline: `80 commits ahead`, `0 behind`.
- QEMU versionado no repositório: `10.2.50`.
- Natureza: auditoria estática de árvore, código, build, documentação e rastreabilidade.
- Execução integral da CI no head auditado: `TOKEN_VAZIO`.
- Benchmark comparativo contra QEMU upstream: `TOKEN_VAZIO`.

## 1. Resumo executivo

O repositório preserva o caminho de build do QEMU por Meson/Ninja e adiciona uma segunda trilha compilável de integração RAFAELIA. A divergência não está limitada a nomes, documentação ou arquivos soltos: existem módulos incluídos em `system_ss`, hook de inicialização no ciclo de vida do QEMU, timer periódico, observação de `RunState`, shutdown notifier, ABI de kernel, roteamento por instrumentos do host, pool de blocos, hub de integração, IPC, conectores e ponte Android/NDK.

A conclusão tecnicamente sustentável é:

```yaml
QEMU_UPSTREAM_BUILD_PATH: PRESERVED
RAFAELIA_SYSTEM_MODULES: COMPILED_IN_SYSTEM_TARGETS
RAFAELIA_QEMU_LIFECYCLE_HOOK: VERIFIED_STATICALLY
RAFAELIA_PERIODIC_CYCLE: VERIFIED_STATICALLY
ANDROID_NDK_BRIDGE: IMPLEMENTED
RMR_TCG_CACHE_IN_THIS_REPOSITORY: NOT_FOUND
RMR_TCG_CACHE_IN_VECTRAS: VERIFIED_STATICALLY
TCG_HOT_PATH_INTEGRATION: TOKEN_VAZIO
PROCESS_MONITOR_IMPLEMENTATION: VERIFIED_STATICALLY
PROCESS_MONITOR_PRODUCTION_WIRING: TOKEN_VAZIO
PERFORMANCE_SUPERIORITY: TOKEN_VAZIO
CI_EXECUTION_AT_AUDITED_HEAD: TOKEN_VAZIO
```

Portanto, o fork já é arquiteturalmente diferente do QEMU upstream, mas ainda não é correto transformar toda diferença em ganho de performance comprovado.

## 2. Método de auditoria

A análise separa quatro classes:

| Classe | Critério |
|---|---|
| `FATO` | Existe em arquivo versionado no head auditado. |
| `PROVA_ESTRUTURAL` | O código está ligado ao build, lifecycle ou teste por referência verificável. |
| `PROVA_EXECUTÁVEL` | Existe log/artefato de execução ligado ao commit auditado. |
| `TOKEN_VAZIO` | A evidência necessária ainda não foi localizada ou produzida. |

Regra principal:

```text
ausência de prova executável != falha
presença de código != ganho comprovado
nome de módulo != integração real
```

## 3. Divergência arquitetural comprovada

### 3.1 Duas trilhas de build preservadas

O repositório declara e mantém:

1. caminho QEMU upstream com `configure` + Ninja;
2. caminho de integração RAFAELIA com `hw/core/Makefile.integration`.

Isso é uma decisão correta de engenharia: mantém uma régua de compatibilidade com o QEMU e uma régua própria para o núcleo adicional.

### 3.2 Módulos RAFAELIA compilados no QEMU de sistema

`hw/core/meson.build` adiciona à coleção `system_ss`:

- `rafaelia_bridge.c`;
- `rafaelia-core.c`;
- runtime IPC;
- conectores Llama, Magisk, private, userland e numbase;
- integration hub;
- low-level;
- QEMU shell;
- route table;
- runtime e runtime mode;
- ModulomR;
- RMR e RMR low-level;
- instrumentos POSIX/host;
- symbiosis.

Isso prova que o núcleo não é apenas documentação ou demo isolada. Ele participa do link dos targets de sistema configurados.

### 3.3 Hook no lifecycle real do QEMU

A inicialização `rafaelia_runtime_init()` está chamada em `system/vl.c` e também é exposta pela ponte RAFAELIA.

O runtime registra:

- `VMChangeStateEntry`;
- `shutdown_notifier`;
- `QEMUTimer`;
- `QemuMutex`;
- estado de execução;
- contador total de ticks;
- entropia e coerência observadas.

Conclusão:

```text
QEMU startup
  -> rafaelia_runtime_init
  -> core + integration hub
  -> timer periódico
  -> observação de RunState
  -> shutdown coordenado
```

Esta é uma modificação de lifecycle, não uma biblioteca lateral sem ligação.

### 3.4 Ciclo periódico com recuperação limitada de ticks

O runtime executa `rafaelia_loop_step()` por tick e calcula quantos ticks devem ser recuperados pelo tempo decorrido.

Invariantes relevantes:

- intervalo sanitizado entre `1 ms` e `10.000 ms`;
- VM parada usa intervalo multiplicado por cinco;
- recuperação de atraso limitada por `RAFAELIA_RUNTIME_TICK_CAP = 100`;
- próximo timer é programado a partir do último tick contabilizado;
- transição de estado redefine a âncora temporal;
- init e shutdown são idempotentes por estado.

Esse desenho evita um catch-up ilimitado após atraso. Ele organiza o ciclo, mas não deve ser descrito como cache de chamadas QEMU.

## 4. Distinção essencial: ciclo, monitor e cache

### 4.1 Ciclo RAFAELIA

Local: `hw/core/rafaelia-runtime.c` + `hw/core/rafaelia-core.c`.

Função:

- evoluir estado RAFAELIA;
- observar lifecycle;
- registrar métricas próprias;
- disparar integração em intervalos definidos.

Não faz, por si só:

- memoização de tradução TCG;
- cache de chamadas QEMU por argumento;
- eliminação automática de recompilação de blocos guest.

### 4.2 Process monitor

Local: `system/process-monitor.c`.

Implementa contadores atômicos para:

- iterações do main loop;
- CPU kicks;
- transições de runstate;
- contenções BQL.

Entretanto, a busca no head auditado não encontrou callsites de:

- `qemu_process_monitor_init()` no startup;
- `qemu_process_monitor_record_main_loop()` no main loop;
- demais `record_*` nos pontos produtivos correspondentes.

Estado correto:

```yaml
IMPLEMENTATION: VERIFIED_STATICALLY
WIRING: TOKEN_VAZIO
RUNTIME_METRICS: TOKEN_VAZIO
```

Além disso, os escritores usam atômicos sem adquirir `stats_mutex`. Assim, cada contador é lido atomicamente, mas a leitura de vários contadores não é um snapshot global linearizável. A documentação/comentário deve dizer `best-effort multi-counter snapshot`, ou todos os escritores precisam participar de um protocolo de epoch/seqlock.

### 4.3 Cache RMR/TCG

O mecanismo que mais se aproxima da descrição “não recalcular a chamada/bloco já conhecido” está em outro repositório:

```text
rafaelmeloreisnovo/Vectras-VM-Android
  engine/rmr/src/rmr_tcg_cache.c
```

Ele usa:

- chave `guest_crc32c`;
- bloco host armazenado;
- lookup com hit/miss explícitos;
- reuso do bloco host;
- escrita por delta XOR em reinserção;
- métricas de bits alterados/preservados;
- política de colapso;
- replay determinístico em selftest.

A ligação entre esse cache e o hot path real `accel/tcg` deste QEMU não foi encontrada.

Estado correto:

```yaml
CACHE_ALGORITHM: VERIFIED_IN_VECTRAS
CACHE_SELFTEST: IMPLEMENTED
QEMU_TCG_CALLOUT: TOKEN_VAZIO
ANDROID_VM_WORKLOAD_BENCHMARK: TOKEN_VAZIO
```

## 5. ABI, instrumentos e roteamento

O core introduz uma ABI interna para desacoplar:

- memória;
- pools;
- coleta de instrumentos;
- seleção de rota;
- RNG;
- operações do shell QEMU.

Na inicialização, o sistema:

1. cria pool limitado para blocos;
2. coleta instrumentos do host;
3. seleciona rota conforme arquitetura/aceleração;
4. conserva snapshot da rota no core;
5. define fallback portável explícito.

Essa camada é estruturalmente valiosa porque troca condicionais espalhadas por um contrato interno. Entretanto, valores simbólicos de frequência (`144 kHz`, multiplicadores) devem permanecer classificados como parâmetros autorais, não como frequência física do processador ou benchmark.

## 6. Hub de integração e IPC

O integration hub contém:

- fila estática limitada;
- heap de prioridade;
- sequência estável para desempate;
- capabilities por repositório;
- pesos adaptativos;
- estados de conexão;
- cálculo de score;
- conectores IPC.

Ponto forte:

```text
integração por contrato
  > dependência direta em headers de outros repositórios
```

Ponto crítico:

`rafaelia_integration_connect_repository()` pode marcar um conector como `CONNECTED` quando não existe função `connect`, usando “stub mode”. Esse estado deve ser renomeado ou separado:

```yaml
DECLARED: configuração conhecida
SIMULATED: stub sem transporte
CONNECTED: handshake real concluído
ACTIVE: operação comprovada
```

Sem essa separação, observabilidade pode apresentar conexão simbólica como transporte real.

## 7. Ponte Android/NDK e entrega de artefatos

Desde o baseline histórico foram adicionados ou ampliados:

- cross-files Meson ARMv7/AArch64;
- workflow Android/Vectras;
- bridge C/JNI;
- runtime de processo Android;
- contrato NDK;
- build proot ARM64/musl;
- empacotamento e verificação de artefatos;
- contrato dual ARM32;
- smoke path para binários QEMU.

Essa trilha diferencia o repositório de um mirror QEMU convencional porque trata Android/Termux/Proot como alvo operacional de primeira classe.

Ainda faltam, para fechamento:

- log de build no head auditado;
- artifact hash ligado ao commit;
- smoke real em dispositivo ARM32 e ARM64;
- captura dos argumentos finais de lançamento;
- prova de VM guest inicializada e encerrada corretamente.

## 8. Auditoria das declarações documentais

### 8.1 Hashes placeholder

`RAFAELIA_IMPLEMENTATION.md` registra SHA3-256 e BLAKE3 como placeholders, mas também declara a implementação “COMPLETE” e “fully operational”. As duas afirmações não podem ocupar a mesma classe epistemológica.

Correção recomendada:

```yaml
CORE_STATE_MACHINE: IMPLEMENTED
FORMULA_MAPPING: IMPLEMENTED_AS_AUTHORIAL_MODEL
SHA3_256: PLACEHOLDER
BLAKE3: PLACEHOLDER
FULL_QEMU_RUNTIME_VALIDATION: TOKEN_VAZIO
STATUS: PARTIAL_VERIFIED
```

### 8.2 Percentuais de performance

`QEMU_IMPROVEMENTS_README.md` apresenta expectativas de:

- `2–5%` em overhead de IPI;
- `1–3%` em CPU idle;
- até `10%` em transições no-op;
- `<0,1%` de overhead do monitor.

Nenhum benchmark comparativo ligado ao head foi localizado nesta auditoria.

Esses números devem ser classificados como:

```yaml
EPISTEMIC_CLASS: EXPECTED_HYPOTHESIS
CLAIM_ALLOWED: false
BASELINE: TOKEN_VAZIO
RAW_RESULTS: TOKEN_VAZIO
```

Não precisam ser apagados; precisam ser rotulados e acompanhados do experimento que poderá confirmá-los ou rejeitá-los.

## 9. O que não foi quebrado

A análise estática sustenta que houve intenção explícita de preservar:

- caminho Meson/Ninja do QEMU;
- comportamento upstream como baseline;
- isolamento da trilha de integração;
- seleção host-specific dos instrumentos;
- shutdown e timers usando APIs QEMU;
- ausência de substituição global do TCG upstream.

Porém, “não quebrou” em sentido de todas as plataformas requer matriz de build/teste real. Portanto:

```yaml
STRUCTURAL_COMPATIBILITY: SUPPORTED_BY_TREE
FULL_PLATFORM_COMPATIBILITY: TOKEN_VAZIO
REGRESSION_FREE: TOKEN_VAZIO
```

## 10. Matriz FATO / PROVA / LACUNA / F_NEXT

| Item | FATO | PROVA disponível | Lacuna | F_NEXT |
|---|---|---|---|---|
| Core RAFAELIA compilado | Arquivos em `system_ss`. | `hw/core/meson.build`. | Build corrente sem log. | Executar build de target mínimo e anexar artifact. |
| Hook de lifecycle | `rafaelia_runtime_init()` em `system/vl.c`. | Callsite + runtime. | Smoke de init/shutdown. | Teste QTest ou smoke com trace. |
| Ciclo periódico | Timer, elapsed e cap de 100. | Código do runtime. | Custo e determinismo não medidos. | Benchmark timer off/on e relógio virtual/real. |
| Pool/ABI/route | Contratos e fallback. | Código do core. | Cobertura multi-host. | Fixtures x86_64/AArch64/RISC-V. |
| Integration hub | Heap, capabilities e IPC. | Código compilado. | Stub pode aparecer conectado. | Estados `SIMULATED`/`CONNECTED` distintos. |
| Process monitor | Contadores implementados. | Fonte e header. | Sem callsites produtivos localizados. | Conectar a main loop/kicks/runstate/BQL. |
| Health/recovery | Módulo presente. | Fonte versionada. | Efeito real não demonstrado. | Fault injection de stall/spin/contention. |
| Cache de bloco | Existe no Vectras. | Selftest e código. | Não ligado ao TCG QEMU. | Bridge mínimo para trace shadow/read-only. |
| Ganho de performance | Hipótese documentada. | Nenhuma medição localizada. | Baseline/resultados. | Benchmark A/B com commit, CPU e raw CSV. |
| Android/NDK | Ponte e workflows presentes. | Código/configuração. | Runtime em dispositivo. | Smoke ARM32/ARM64 com logs e hashes. |

## 11. Gates de fechamento

### `Q0 — Upstream smoke`

- configurar QEMU;
- compilar target mínimo;
- produzir `config.log`, `config-host.h` e artifact.

### `Q1 — RAFAELIA integration build`

- `make -f hw/core/Makefile.integration all`;
- `make -f hw/core/Makefile.integration test`;
- anexar logs e hashes.

### `Q2 — Lifecycle smoke`

- iniciar QEMU com runtime desativado;
- iniciar com runtime ativado;
- capturar init, ticks, state changes e shutdown;
- comparar falhas e tempo.

### `Q3 — Monitor wiring`

- conectar os quatro pontos de evento;
- teste com contagens conhecidas;
- corrigir semântica de snapshot.

### `Q4 — TCG shadow integration`

- não substituir TCG inicialmente;
- observar blocos/traces em modo shadow;
- registrar chave, hit/miss e tamanho;
- nenhuma reutilização produtiva antes de equivalência.

### `Q5 — Cache A/B`

- baseline de tradução/regravação integral;
- delta XOR;
- densidades de mutação controladas;
- `ns/op`, hit ratio, bytes/bits tocados, memória e p50/p95/p99.

### `Q6 — Android device proof`

- ARMv7 e AArch64;
- Termux/Proot conforme contrato;
- hash do binário;
- ABI real;
- boot/smoke da VM;
- teardown limpo.

## 12. Organização documental aplicada

Este documento passa a ser a fronteira canônica entre:

- QEMU preservado;
- extensão RAFAELIA compilada;
- ciclo de runtime;
- monitor ainda não conectado;
- cache localizado no Vectras;
- claims de performance ainda não medidos.

Documentos históricos permanecem preservados. Quando houver conflito, deve prevalecer a evidência mais recente ligada a commit, build, teste e artifact.

## 13. Síntese técnica

O repositório já não é apenas um QEMU renomeado. Ele contém uma plataforma adicional de runtime, ABI, integração, IPC e Android construída ao redor do QEMU, mantendo o build upstream como régua.

A frase tecnicamente defensável é:

> O QEMU Rafaelia preserva o núcleo QEMU e adiciona uma camada compilada e conectada ao lifecycle para estado, roteamento, IPC, instrumentação e integração Android; o mecanismo de cache RMR por CRC32C/delta XOR existe no Vectras, mas sua integração ao hot path TCG do QEMU e sua superioridade de desempenho permanecem `TOKEN_VAZIO` até benchmark e artifact reproduzível.

## Retroalimentação

```text
F_ok   = divergência estrutural, lifecycle, runtime, ABI, hub, IPC e Android identificados.
F_gap  = monitor sem wiring localizado; cache fora do hot path QEMU; CI/benchmark atuais sem evidência.
F_next = fechar Q0–Q3 antes de promover cache TCG para execução produtiva.
```
