# Relatório de validação do pacote

## Executado neste ambiente

- Estrutura do pacote e todas as referências do CMake: PASS.
- 16 arquivos SVG analisados como XML válido: PASS.
- Mockup aprovado 1536 x 1024 incluído: PASS.
- IDs permanentes dos 10 parâmetros: PASS.
- Balanceamento básico de delimitadores C++: PASS.
- Compilação isolada do núcleo DSP em C++20 com `-Wall -Wextra -pedantic`: PASS.
- Testes do núcleo DSP: 0 falhas.
- ASan + UBSan direcionados ao núcleo: 0 erros.

## Resultados do detector

| Entrada | Detectado | Confiança |
|---:|---:|---:|
| 90 Hz | 90.0009 Hz | 0.999899 |
| 150 Hz | 150.008 Hz | 1.000000 |
| 220 Hz | 220.011 Hz | 0.998716 |
| 440 Hz | 440.235 Hz | 0.998104 |
| 700 Hz | 701.122 Hz | 0.998670 |

## Música

- C maior: C +3rd = E: PASS.
- C maior: D +3rd = F: PASS.
- Oitava acima/abaixo: PASS.
- Preservação do desvio de vibrato: PASS.
- Analisador identifica C maior numa sequência ponderada: PASS.

## Pitch shifter

- Saída finita, sem NaN/Inf: PASS.
- Pico limitado no teste: PASS.
- Latência positiva reportada: PASS.

## Validação ainda obrigatória no computador do projeto

O JUCE completo, Xcode, Visual Studio e SDK AAX não estão disponíveis neste ambiente. Portanto, o Claude deve compilar o plugin completo, corrigir eventuais diferenças de API da versão JUCE instalada e executar o checklist de hosts, áudio real, presets e recall antes de gerar instalador.

