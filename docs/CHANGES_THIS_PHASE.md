# Lista de alterações desta fase

1. Extração achatada do zip em pasta irmã; checkpoint git `77dad00`.
2. `CMakeLists.txt`: JUCE local/FetchContent 9.0.1, AAX opcional com `juce_set_aax_sdk_path`, categorias AU/VST3/AAX, alvo `NFVocalHarmonizerVocalAudit`.
3. `scripts/build-macos.sh`: Unix Makefiles quando não há Xcode.app.
4. Parâmetro novo `power` (schema 2); POWER e HARMONIZE deixam de compartilhar `enabled`.
5. Editor: binding de POWER → `power`; rename interno `processor` → `audioProcessor` (JUCE); casts explícitos de componentes.
6. Look & Feel: botão POWER circular; PitchTrace ajustes menores.
7. `Tests/DspTests.cpp`: latência, blocos, silêncio, IDs, recall, presets, automação.
8. `tools/VocalOfflineAudit.cpp`: auditoria offline com WAVs reais.
9. Docs: `UI_LOCK.md`, `SOUND_AUDIT_REPORT.md`, `AAX_BUILD_NOTES.md`, `VALIDATION_REPORT.md` atualizado.
10. Builds: Standalone/AU/VST3 (`build-macos`) e AAX (`build-aax` com SDK 2.9.0).

IDs estáveis originais **não** renomeados: interval, harmony, formant, humanize, width, mix, key, scale, autoKey, enabled.
