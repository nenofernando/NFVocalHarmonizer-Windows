# UI Lock — NF Vocal Harmonizer

Referência: `assets/mockup/NF_Vocal_Harmonizer_Approved.png` (1536×1024) + `docs/UI_SPEC.md`.

## Comparação com o mockup

| Área | Mockup | Código atual | Status |
|---|---|---|---|
| Chassis prata / vidro preto / fundo `#0b0f14` | Sim | Gradiente metal + moldura arredondada | PASS |
| Logo NF Audio Tools + título + subtítulo | Sim | Header pintado em `PluginEditor::paint` | PASS |
| Presets / A / B / Copy / Save | Sim | Presentes no header | PASS |
| POWER circular ciano | Ícone circular | `POWER` desenha círculo + glyph (`NFLookAndFeel`) | PASS |
| Meters INPUT / OUTPUT laterais | Sim | `StereoMeter` L/R | PASS |
| AUTO KEY + Analyze + leitura tonal | Sim | Auto Key, Key, Scale, Analyze, label detectado | PASS* |
| Traços VOICE (ciano) / HARMONY (violeta) | Sim | `PitchTrace` | PASS |
| Régua vertical com VOICE fixa e um intervalo | Sim | `IntervalRail` | PASS |
| HARMONIZE grande central | Sim | Botão toggle ligado a `enabled` | PASS |
| 5 knobs: Harmony, Formant, Humanize, Width, Mix | Sim | Ordem e defaults alinhados | PASS |
| Humanize em violeta | Sim | Cor violeta no knob | PASS |

\* O mockup compacta a leitura em “AUTO KEY: G MINOR”. A UI_SPEC exige controles Auto Key / Key / Scale / Analyze — o código segue a UI_SPEC. Não reposicionar sem autorização.

## POWER vs HARMONIZE

No pacote inicial, POWER e HARMONIZE apontavam para o mesmo ID `enabled`.

Correção (schema 2):

- `enabled` = HARMONIZE (inalterado; um dos 10 IDs estáveis)
- `power` = POWER (ID **novo**, default On) — bypass suave; não altera recall dos IDs originais

## Controles

Nenhum controle foi adicionado, removido ou reposicionado em relação ao layout do editor. Apenas o binding de POWER mudou para o ID `power`.

## Dimensões

- Inicial: 1100×734
- Mínimo: 820×548
- Máximo: 1536×1024
- Redimensionável proporcionalmente

## Veredito UI

Layout e hierarquia alinhados ao mockup/UI_SPEC para esta fase. Polimento visual fino (chrome de preset, typography do header “AUTO KEY: …”) pode evoluir depois sem mover a hierarquia.
