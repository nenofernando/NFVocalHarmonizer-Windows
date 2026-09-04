# Parâmetros e defaults

| ID permanente | Nome | Faixa | Default | Função |
|---|---|---:|---:|---|
| `interval` | Interval | 8 escolhas | +3rd | Grau diatônico acima/abaixo |
| `harmony` | Harmony | 0-100% | 70% | Nível da voz criada |
| `formant` | Formant | -12 a +12 | 0 | Compensação de timbre/formante |
| `humanize` | Humanize | 0-100% | 35% | Variação orgânica controlada |
| `width` | Width | 0-100% | 100% | Abertura entre as duas variações da harmonia |
| `mix` | Mix | 0-100% | 50% | Crossfade dry/harmony |
| `key` | Key | C-B | G | Tônica manual/fallback |
| `scale` | Scale | 5 escalas | Natural Minor | Escala manual/fallback |
| `autoKey` | Auto Key | Off/On | On | Análise automática |
| `enabled` | Harmonize | Off/On | Off | Ativa a harmonia |

## Regras de recall

- Nunca renomear IDs depois do primeiro lançamento.
- Adicionar novos parâmetros com novos IDs e versão interna de schema.
- Todo parâmetro audível deve ser automatizável e salvo pelo APVTS.
- Tamanho de janela pode ser propriedade extra, nunca parâmetro de áudio.
- A/B é salvo como subárvore do estado da DAW.

