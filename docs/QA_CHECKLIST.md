# Checklist obrigatório antes do instalador

## Afinação e inteligência

- Voz masculina, feminina, infantil e regiões grave/aguda.
- Vibrato lento/rápido, portamento, notas retas, ataques aspirados e drive moderado.
- Canto afinado e desafinado em até +/-100 cents.
- Todas as tonalidades e escalas.
- Cada intervalo ascendente e descendente.
- Terças corretas nota por nota em escalas maior e menor.
- Silêncio, ruído, respiração, consoantes e voz não musical não podem gerar notas aleatórias.

## Áudio

- 44.1, 48, 88.2, 96, 176.4 e 192 kHz.
- Buffers 16, 32, 64, 128, 256, 512, 1024 e blocos variáveis.
- Sem NaN/Inf, denormals, estouro, cliques ao trocar intervalo ou ativar Harmonize.
- Latência real deve coincidir com a reportada.
- Testar cancelamento e continuidade entre blocos.
- Comparar formantes, sibilância e consoantes com referências profissionais.

## Hosts e formatos

- macOS: AU no LUNA/Logic; VST3 no Reaper; AAX no Pro Tools.
- Windows: VST3 no Reaper/Cubase; AAX no Pro Tools.
- Scanner, criação/destruição repetida, abrir/fechar editor e múltiplas instâncias.
- Intel e Apple Silicon; Windows x64.

## Estado e presets

- Salvar/carregar preset com transporte parado e tocando.
- Sobrescrever, cancelar, nome inválido e pasta sem permissão.
- Recall de sessão com editor aberto/fechado.
- A/B e Copy.
- Automação de todos os parâmetros.
- Nunca destruir FileChooser/diálogo dentro do próprio callback.

## Performance

- Zero alocação e lock na thread de áudio.
- ASan/UBSan nos testes direcionados e suite completa quando viável.
- CPU com 1, 8, 16 e 32 instâncias.
- Nenhum caminho do DSP depende de a interface estar aberta.

