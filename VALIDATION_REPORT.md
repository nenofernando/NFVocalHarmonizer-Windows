# Relatório de validação — NF Vocal Harmonizer (máquina do projeto)

Atualizado: 2026-09-04

## Isolamento

- Vocal Verb **não alterado** (`update/space-selector`, working tree limpo).
- Trabalho isolado em `.../ARQUIVOS PLUGINS OFICIAIS/NF Vocal Harmonizer`.

## Compilação

| Formato | Build dir | Arch | Status |
|---|---|---|---|
| Standalone | `build-macos` | x86_64 | PASS |
| AU | `build-macos` | x86_64 | PASS |
| VST3 | `build-macos` | x86_64 | PASS |
| AAX | `build-aax` + SDK `aax-sdk-2-9-0` | x86_64 | PASS |

JUCE: 9.0.1 (`/Users/nenofernando/Downloads/JUCE`)  
AAX SDK: `/Users/nenofernando/Downloads/aax-sdk-2-9-0` (mesmo do Vocal Verb)  
Gerador: Unix Makefiles (sem Xcode.app nesta máquina)

Hashes: `dist/validation/binary-hashes.txt`

AU instalado em `~/Library/Audio/Plug-Ins/Components` e validado com `auvaltool -v aufx NfVh NfAt`: **PASS** (`dist/validation/auval.txt`).

## Testes DSP / estado

`NFVocalHarmonizerTests`: **0 falhas**

Inclui: escala diatônica, YIN, Auto Key, shifter NaN, latência dry, invariância de bloco, silêncio sem nota fantasma, IDs estáveis + `power`, recall APVTS, presets TemporaryFile, automação audível.

`tools/validate_package.py`: **PASS**

## UI

Ver `docs/UI_LOCK.md`. POWER separado de HARMONIZE via ID novo `power` (schema 2). Mockup preservado; sem reposicionar controles.

## Sonoridade (vozes reais)

Ver `docs/SOUND_AUDIT_REPORT.md` e `dist/validation/vocal-offline-audit.txt`.

- Takes masculinos Neno: mediana ~3–4 cents — PASS comercial de pitch.
- Alto mixed: ~13 cents — PASS fase, abaixo do marketing comercial.
- Fala paired speech: ~35 cents — FAIL cents (esperado para não-melodia).
- **Não** declarar sonoridade comercial final; formant ainda é coloração leve.

## AAX

Binário gerado: `build-aax/.../AAX/NF Vocal Harmonizer.aaxplugin`  
Assinatura PACE/iLok e instalador **não** feitos nesta fase.

## Próximos passos sugeridos

1. Escuta cega +8ve/formantes no Standalone / Reaper / LUNA / Pro Tools.
2. Decisão de backend (manter granular vs PSOLA/SDK) sem mudar IDs/UI.
3. Universal binary + assinatura/notarização + instalador de teste.
4. Validação Avid das ferramentas oficiais no `.aaxplugin`.
