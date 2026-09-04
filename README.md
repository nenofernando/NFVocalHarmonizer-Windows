# NF Vocal Harmonizer

Projeto-fonte completo em JUCE/C++ para um harmonizador vocal monofônico em tempo real da NF Audio Tools.

## Conceito

O usuário insere o plugin em uma voz, deixa `AUTO KEY` analisar o contexto musical, seleciona um único intervalo na régua vertical e ativa `HARMONIZE`. O intervalo é diatônico: por exemplo, uma terceira se adapta entre maior e menor conforme a nota e a escala, em vez de aplicar sempre a mesma quantidade de semitons.

## O que está implementado

- AU e VST3 no macOS; VST3 no Windows; Standalone para testes.
- AAX condicional quando o SDK da Avid estiver configurado.
- Detecção monofônica de pitch baseada em YIN com análise em taxa reduzida.
- Análise automática de tonalidade maior/menor por histograma de classes de altura.
- Escalas Major, Natural Minor, Harmonic Minor, Melodic Minor e Chromatic.
- Intervalos `+3rd`, `+5th`, `+6th`, `+8ve`, `-3rd`, `-5th`, `-6th`, `-8ve`.
- Pitch shifter granular de duas cabeças, duas variações para estéreo e transições suavizadas.
- Harmony, Formant, Humanize, Width e Mix.
- Medidores de entrada e saída, traço de pitch original/harmonia e leitura da tonalidade.
- Presets internos com gravação atômica, A/B, Copy e recall completo via APVTS.
- Interface redimensionável e controles automatizáveis.
- Testes iniciais de escala, pitch detector e segurança numérica.

## Build

Pré-requisitos: CMake 3.22+, JUCE 7/8, compilador C++20. No macOS, Xcode; no Windows, Visual Studio 2022.

```bash
cmake -S . -B build -DJUCE_DIR=/caminho/para/JUCE -DNF_BUILD_TESTS=ON
cmake --build build --config Release
```

Para AAX, configure o SDK da Avid no JUCE e gere com:

```bash
cmake -S . -B build-aax -DJUCE_DIR=/caminho/para/JUCE -DNF_ENABLE_AAX=ON
```

O SDK AAX e a assinatura comercial não são redistribuídos neste pacote.

## Estrutura

- `Source/dsp`: detecção, inteligência musical e pitch shifting.
- `Source/ui`: desenho vetorial da interface e componentes.
- `assets/svg`: cada peça visual separada.
- `assets/mockup`: imagem de referência obrigatória.
- `Tests`: testes executáveis.
- `docs`: arquitetura, especificação visual e checklist.
- `CLAUDE_MASTER_PROMPT_PT.txt`: instrução pronta para enviar ao Claude junto com a pasta.

## Importante sobre qualidade vocal

O backend incluído é funcional e independente de bibliotecas externas, mas um pitch shifter vocal comercial de altíssimo nível exige validação auditiva extensa. Antes de lançar, o Claude deve comparar o backend granular com PSOLA/phase-vocoder com preservação espectral ou um SDK comercial licenciado. O controle Formant incluído é uma compensação de envelope/coloração leve, não deve ser anunciado como preservação formântica transparente até passar pelos testes do checklist.

Não afirmar “sonoridade perfeita” apenas porque compila. O critério final é áudio real, ausência de artefatos, estabilidade e testes em diferentes vozes.

