# Roadmap — fork do Artemis-Switch (latência e suavidade)

Atualizado em 2026-10-25 (v3: acrescenta o log de sessão e o ciclo de teste). Itens marcados (inferência) não foram testados no aparelho. Tudo sobre Punktfunk e V+ vem de leitura de código, não de medição.

## 1. Fatos verificados no código
- `rendered_fps` do Artemis está enviesado: a janela abre no fim da anterior e divide por N−1. Com 16 draws por janela dá 15/16×60 = 56.25. O app oficial (XITRIX) abre a janela no 1º frame e dá 60.01. O valor real é ~60 draws/s. Arquivos: `app/src/streaming/video/deko3d/DKVideoRendererLegacy.inc` (~linha 1348-1367) vs `DKVideoRenderer.cpp` do oficial (~linha 1105-1170).
- "Average rendering time: 0.00" = tempo de submissão truncado para ms inteiros. Ignorar.
- O pacer padrão do Artemis (low-latency desligado) é praticamente o do oficial. Os contadores do overlay (paced skips, holds, resyncs, queue depth) já existem no `main` do oficial.
- Fila: target = min(queue size−1, 2) no modo padrão e min(queue size−1, 1) no low-latency; capacidade = queue size + 5. `0|1|8` = low-latency ligado, `2|2|8` = desligado (queue size 3).
- Low-latency do Artemis: filtro alfa-beta sobre o tempo de CHEGADA dos frames, portão que libera o frame perto de chegada + intervalo − folga (~3 ms + 1,5×jitter), depois "mais novo vence". (inferência) segura cada frame ~12 ms e gera pares hold + skip.
- Contadores observados: low-latency ligado holds 16328 / skipped 11049; desligado holds 22896 / skipped 15754. Os totais de ticks e frames não fecham; não tirar taxa de um print só.
- Host (JSON Punktfunk, 720p60 HEVC NVENC): encode p50 ~2,0 ms; frames_dropped 0; fps médio 58.8 + repeat_fps 1.4 ≈ 60. Pior engasgo do jogo em t=58,6 s (fps 46.3, repeat 14.8).

## 2. Ideia central (vem do Punktfunk)
Hoje o Artemis decide o ritmo de exibição pelo instante em que o frame CHEGA na rede (jitter da rede entra direto na imagem). O Punktfunk decide pelo timestamp que o HOST colocou no frame: o relógio de cadência recupera o ritmo original e a exibição fica regular mesmo com a rede irregular.

## 3. O que copiar do Punktfunk (em ordem de valor / custo)
Referência: `reference/punktfunk/` (cópia somente leitura; origem: `clients/android/native/src/present/*`, `crates/core/punktfunk-core/src/phase.rs`, `crates/client/pf-presenter/src/present_pace.rs`).

1. **Régua de juder (`SlotIntervals`)** — barato, e é o que prova se algo melhorou. Contar quantos vsyncs passam entre dois frames NOVOS apresentados: n0 (dois no mesmo vsync), n1 (ideal a 60/60), n2, n3+. Juder‰ = fração fora da moda. Mostrar no overlay junto com contadores por segundo (holds, skips).
2. **`CadenceClock`** — o núcleo. due = timestamp_origem + offset + folga. Offset e skew (diferença de frequência entre relógios) rastreados; timestamps nunca suavizados; MAD (desvio) sobrevive a reset; folga limitada a 1 intervalo de frame; contadores `late` e `reanchors` como saúde; reancora se o buraco passa de 500 ms. Aritmética de inteiros (fácil em C++). Presets: snapping, free_running, vrr_latency (no Switch só interessa o snapping, tela fixa 60 Hz).
3. **`pace_slot` / `SlotClock`** — a cada vsync mostra o frame mais novo cujo due já venceu; nunca no mesmo slot do anterior; no máximo `max_ahead` slots à frente; repete só se nada está pronto.
4. **`FrameStore`** — newest-wins (latência) ou FIFO de 1 a 3 (suavidade). Frame retido nunca é sobrescrito, o sucessor entra atrás.
5. **`PresentPriority`** — duas intenções na UI em vez de botões técnicos: Latência (padrão) e Suave (buffer 1..3).
6. **`verdict.rs`** — watchdog: se o store descarta ≥20% dos frames decodificados por 5 s ininterruptos, desliga o pacing novo na sessão e volta ao modo antigo. Rede de segurança contra regressão.
7. **Testes de simulação** — o Punktfunk exige que a cadência bata a apresentação por chegada em mais de 2× com jitter de ±6 ms. Vetores em `reference/punktfunk/vectors/`. Dá para portar como teste do cálculo (sem aparelho).

## 4. Timestamps do host (verificado em `crates/host/punktfunk-host/src/gamestream/stream.rs`, ~linha 1850)
- O timestamp RTP 90 kHz do plano GameStream é `stream_start.elapsed() × 90000`, ou seja, relógio de parede do TICK do host, não o instante de captura do jogo. Frames repetidos (tick sem frame novo) também recebem timestamp.
- Consequência (inferência): o relógio de cadência recebe um metrônomo regular. Corrige o ritmo de exibição causado pela rede, mas não esconde engasgos do jogo.
- No common-c do XITRIX o campo é `DECODE_UNIT.presentationTimeMs` (resolução de 1 ms). O Artemis não usa esse campo hoje.
- Passo 0 do fork: registrar os deltas de `presentationTimeMs` por alguns minutos para confirmar.
- Se o host não for Punktfunk (Sunshine/GFE), o timestamp pode ter outro significado; o modo novo precisa ficar opcional.

## 5. Descartado
- Moonlight V+ (Android): a ideia de relógio de cadência é a mesma do Punktfunk, só que mais simples (PI Kp 1/64, Ki 1/2048, folga 0,5×MAD, snap no vsync). Não implementar separado. Framegen do V+ é esqueleto (Vulkan + Lossless.dll). Ajustes de MediaCodec e ADPF são específicos de Android.
- Protocolo nativo `punktfunk/2` (QUIC, FEC Leopard-RS, ABR): exigiria reescrever o cliente (Rust).
- PyroWave (Vulkan compute, precisa de cabo e host).
- Licenças: Punktfunk é MIT/Apache-2.0 (compatível com GPL-3.0). Artemis é GPL-3.0. Reimplementar em C++ e manter atribuição.

## 6. Plano do fork (mantendo C++)
Cada etapa gera um build no GitHub Actions (.nro), o Bruno testa no Switch e envia o log de sessão (seção 8) antes da etapa seguinte.

1. Corrigir `rendered_fps`. Registrar `presentationTimeMs` (passo 0). Criar o **log de sessão** (seção 8) e identificar o build (hash do commit) no log e no About/overlay. O log vem primeiro porque é o que permite avaliar todas as etapas seguintes.
2. (feito, testado: legado 35‰ vs low-latency 103‰) Régua de juder + contadores por segundo no overlay e no log (item 1 da seção 3). Também: trocar o autor do .nro (metadados NACP) de `antoinebou12` para `broslife` (`PROJECT_AUTHOR` em `CMakeLists.txt`, ~linha 183). Só o autor do .nro; manter nome/branding Artemis e os créditos. Os textos "by antoinebou12" do About (`resources/i18n/*/main.json`) ficam como estão, salvo pedido. O AGENTS.md já foi atualizado: todo .nro deve ter `broslife` como autor.
3. Portar `CadenceClock` + `pace_slot` como modo novo e opcional (itens 2 e 3).
   - Feito e testado no aparelho (branch `step3-cadence`, ainda sem merge na `main`): `CadenceClock.hpp`, `CadenceFrameSelection.hpp`, opção **Cadence pacing (experimental)** (Settings e overlay Options), colunas `netdrops/late/reanchors/cushion` no log. Sem vsync do Borealis, o tick de draw faz o papel do slot; simulação mostrou que a folga de meio período não ajuda, então a regra é: mostra o frame mais novo com `due <= agora`.
4. **Reduzir a espera da Cadence (intenção Latência).** Dados atuais (build 814c633, handheld, 720p60): `cpipe` 17,5 ms (legado 49,3 ms), `qwait` 13,7 ms. O low latency chega a 5-9 ms só porque descarta metade dos frames (30 fps efetivos), então não é meta. Sub-passos:
   - 4a. Medir: sessão longa (3-4 min) com Cadence para ver se o `qwait` oscila em dente de serra (fase entre o relógio do host e o tick de 60 Hz) e qual é a média de longo prazo. Aceite: gráfico/tabela do `qwait` por segundo.
   - 4b. Folga (cushion): hoje 2x MAD, piso 0,5 ms. Testar 1x MAD (preset `vrr_latency` do Punktfunk) e piso menor, A/B por log. Custo esperado da folga = a própria folga em média (1-4 ms nos logs). Ganho esperado pequeno (1-2 ms); só vale se o juder e o `late/s` não piorarem.
   - 4c. Investigar a fila depois do envio ao display: quantas imagens tem a swapchain do deko3d dentro do Borealis (não medido pelo `cpipe`). Pode valer 1-2 vsyncs (17-33 ms), a maior alavanca possível, mas mexe em biblioteca fora do repo. Só ler e medir; decidir antes de alterar.
   - Aceite do passo: `cpipe` menor que 17,5 ms com juder (vs esperado) não pior que ~30‰ e `shown` >= 57 fps, no mesmo jogo/cena.
   - Expectativa honesta: o piso sem descartar frames é cerca de meio tick (8 ms) de espera, mais o decode (3,5 ms). O ganho de 4a-4b é de poucos ms; o potencial maior está em 4c.
5. **Intenção Suave (sub-opção dentro da Cadence), depois.** `FrameStore` com buffer de 1 a 3 frames de reserva (itens 4 e 5 da seção 3). Mais suave em rede instável, com 1 a 3 frames (17-50 ms) a mais de latência. A Latência é o comportamento que a Cadence já tem; esta etapa só acrescenta a escolha Prioridade: Latência / Suave. Menor prioridade: nos logs do Bruno os segundos calmos já têm 0‰ de juder e o resíduo vem de buracos do host.
6. Watchdog de fallback (item 6 da seção 3).
7. A/B final no mesmo jogo e cena, comparando os logs: juder (vs esperado), `shown`, skips e holds por segundo, `qwait` e `cpipe`.

Resultados já medidos (host Punktfunk, 192.168.0.133, 720p60 HEVC 10 Mbps, handheld; uma a duas sessões por modo, sem a rede controlada):
- Legado: juder 35-86‰, `cpipe` 49 ms.
- Low latency: instável nos primeiros 20 s e depois trava em 30 fps efetivos (`n2` ~30/s, `shown` ~30), `cpipe` 5-9 ms.
- Cadence: juder 14-28‰, `shown` ~57, `cpipe` 17,5 ms.
- Buracos de ~60 ms (`rcv_fps` 48-52, `netdrops` 0) aparecem em todos os modos; a pista aponta para o host (frames repetidos pela captura, `repeat_fps` no JSON do host). Investigação separada, fora deste roadmap.

## 7. Pendências
- Fazer o fork na conta do Bruno e adicionar o repo à sessão.
- Confirmar no aparelho se o oficial mostra os mesmos contadores.
- Verificar se o loop do Borealis dá o tempo do vsync ou se só dá para usar steady_clock (ver `reference/switch-api/vi.h`, `viGetDisplayVsyncEvent`).
- Confirmar nos logs que `presentationTimeMs` tem deltas regulares com o host Punktfunk.
- Confirmar onde o export de Benchmark (JSON/CSV) já grava no SD e se dá para reaproveitá-lo no log de sessão, em vez de criar outro mecanismo.

## 8. Log de sessão (como o Claude Code "enxerga" o teste no Switch)
O agente não roda o app no Switch. O Bruno testa cada build e envia o arquivo de log para o agente analisar.

**Onde:** arquivo de texto no SD, em `sdmc:/switch/Artemis-Switch/logs/`, um por sessão, com data/hora e o modo no nome. Reaproveitar o mecanismo de export do Benchmark se ele já fizer isso (ver pendências).

**Cabeçalho (uma vez):** hash/versão do build, modo de pacing (legado / low-latency / novo), tamanho da fila, resolução, fps, codec, bitrate configurado, tipo de host (se detectado), modo do Switch (dock/portátil).

**Uma linha por segundo:** `rendered_fps` (corrigido), fps recebido e decodificado, holds/s, skips/s, underflows/s, descartes por overflow/s, profundidade da fila, tempos de receive e decode, juder‰ e n0/n1/n2/n3+ (a partir da etapa 2), deltas de `presentationTimeMs` (média, máximo, desvio) e, no modo novo, `late` e `reanchors`.

**Fim da sessão:** totais e mínimo/máximo/p95 dos principais valores.

**Regras de implementação:**
- Nada de gravar em arquivo a cada frame na thread de render. Acumular em memória e gravar em lote a cada alguns segundos, em outra thread, e no fim/desconexão.
- Custo desprezível e sempre ligado (ou uma opção simples no Debug), para o Bruno nunca precisar lembrar de ativar.
- Falha ao gravar (SD cheio ou ausente) não pode derrubar o stream.

**Protocolo de teste (para os logs serem comparáveis):** mesmo jogo, mesma cena, 90 s; um só fator muda por vez (ex.: pacing legado vs. modo novo); registrar no nome do arquivo ou na anotação qual modo foi usado. Enviar o log com uma frase dizendo como pareceu a imagem (fluida, engasga, atraso nos controles).
