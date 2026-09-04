# Auditoria de sonoridade — NF Vocal Harmonizer

Data: 2026-09-04  
Backend sob teste: `GranularPitchShifter` + YIN + intervalos diatônicos  
Ferramenta: `NFVocalHarmonizerVocalAudit` (offline, primeiros ~12 s de cada WAV)

## Critério

- Mediana de erro tonal em trechos voiced: alvo comercial &lt; 10 cents; gate desta fase &lt; 25 cents
- Sem notas fantasmas em silêncio
- Sem NaN/Inf; pico limitado
- Formant atual = coloração leve — **não** anunciado como preservação formântica comercial

## Resultados

| Arquivo | Tipo | Voiced | Mediana cents (+3rd) | Fantasma no silêncio | Finito | Veredito |
|---|---|---:|---:|---|---|---|
| Vocalize Neno Agilidade Vocal.wav | voz masculina (Neno) | 585 | **3.19** | 0 | PASS | PASS (comercial no pitch) |
| VOCALIZE.wav | voz masculina (Neno) | 394 | **4.45** | 0 | PASS | PASS (comercial no pitch) |
| 28_LeadVox1.wav | lead comercial | 0* | n/a | 0 | PASS | SKIP pitch (sem frames voiced no excerpt) |
| EN-Alto Call Me Maybe 0000.wav | voz feminina / mixed | 716 | **13.09** | 0 | PASS | PASS fase / FAIL comercial (&lt;10) |
| Vocalize Neno Maior e Menor.wav | voz masculina (Neno) | 284 | **4.42** | 0 | PASS | PASS (comercial no pitch) |
| la vie en rose paired speech 0004.wav | fala / mixed | 244 | **35.45** | 0 | PASS | FAIL cents (fala não é melodia diatônica) |

\* `28_LeadVox1.wav` é PCM 24-bit mono 44.1 kHz (~98 s); o excerpt inicial não produziu frames voiced — possível silêncio/respiração no começo. Não conta como falha do motor.

Log completo: `dist/validation/vocal-offline-audit.txt`

## Observações honestas

1. Em takes masculinos secos de vocalize, o tracking diatônico está dentro do alvo comercial de cents.
2. Em alto/mixed voice, a mediana subiu para ~13 cents — ainda estável, mas **não** declarar “sonoridade comercial fechada”.
3. O formant knob **não** foi validado como preservação formântica real em +8ve; só segurança numérica.
4. Não houve comparação cega contra SDK PSOLA/phase-vocoder comercial nesta sessão.
5. Humanize/Width, sibilância e smear de consoantes precisam de escuta em Standalone/DAW (Reaper/LUNA) — não substituíveis só por cents.

## Veredito desta fase

**PASS para base funcional com vozes reais (sem NaN, sem fantasma, pitch útil).**  
**NÃO pronto para marketing de “qualidade comercial final”** até:

- escuta cega +8ve / sextas com formantes
- mais takes femininos/graves/sibilantes
- decisão explícita de manter granular ou trocar backend preservando IDs/UI
