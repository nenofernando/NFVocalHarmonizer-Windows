# Especificação visual

## Referência obrigatória

Usar `assets/mockup/NF_Vocal_Harmonizer_Approved.png` como referência de proporção, hierarquia, acabamento e posicionamento. Não redesenhar o conceito sem autorização.

## Dimensão

- Arte-base: 1536 x 1024.
- Tamanho inicial sugerido: 1100 x 734.
- Limite mínimo: 820 x 548.
- Redimensionamento proporcional.

## Hierarquia

- Cabeçalho: logo, nome, subtítulo, presets, A/B, Copy, Save e Power.
- Laterais: Input e Output estéreo.
- Superior central: Auto Key, Key, Scale, Analyze e traços Voice/Harmony.
- Centro: régua vertical de intervalos, Voice fixa e somente um intervalo ativo.
- Ação principal: botão HARMONIZE.
- Rodapé: Harmony, Formant, Humanize, Width e Mix.

## Cores

- Fundo: preto azulado `#0b0f14`.
- Prata clara: `#d5d9dc`.
- Prata escura: `#4a5055`.
- Ciano NF: `#2fe0ee`.
- Violeta Harmony: `#a249ed`.
- Texto: `#edf1f4`.

## Estados

- VOICE: sempre ciano, nó grande e fixo.
- Intervalo escolhido: violeta, apenas um por vez.
- Intervalos inativos: prata neutra.
- HARMONIZE ligado: ciano intenso; desligado: fundo escuro.
- Auto Key ligado: Key/Scale manuais visualmente desabilitados.

## Assets

Cada SVG é separado para facilitar reconstrução e exportação. Os SVGs são guias vetoriais; o código JUCE desenha controles em resolução independente.

