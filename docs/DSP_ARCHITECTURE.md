# Arquitetura DSP

## Fluxo

1. Entrada mono ou estéreo.
2. Soma mono exclusiva para análise e geração da harmonia.
3. YIN detecta frequência, nota MIDI e confiança.
4. `KeyAnalyzer` acumula classes de altura e estima tônica/modo quando Auto Key está ativo.
5. `MusicalScale` encontra o grau mais próximo e calcula o intervalo diatônico.
6. O desvio fino da nota original é preservado para que o vibrato acompanhe a harmonia.
7. Dois pitch shifters recebem pequenas variações lentas de cents quando Humanize/Width estão ativos.
8. Compensação leve de formante/timbre.
9. Equal-power Mix entre dry alinhado e harmonia.
10. Medição de saída e entrega ao host.

## Política para voz não detectada

Quando a confiança cai, a harmonia é suavemente silenciada. Nunca usar uma nota aleatória, a última nota indefinidamente ou ruído como decisão tonal.

## Latência

O backend granular reporta latência fixa ao host. O caminho dry é atrasado com a mesma referência. O Claude deve medir a latência por impulso em todas as taxas de amostragem e ajustar o valor reportado, se necessário.

## Auto Key

O analisador usa perfis maior/menor e decaimento lento. `ANALYZE` limpa o histograma. Para lançamento, testar modulações, músicas modais e trechos curtos. Sempre manter Key e Scale manuais como fallback profissional.

## Formant

O código atual oferece uma compensação leve de envelope. Para máxima transparência, implementar um backend de produção com envelope espectral/LPC ou SDK licenciado. Manter a interface `HarmonyEngine` e os parâmetros estáveis para não quebrar recall.

## Regras de tempo real

- Zero alocação, arquivo, lock, log ou UI dentro de `processBlock`.
- Buffers preparados em `prepareToPlay`.
- Mudanças de parâmetros suavizadas.
- Reset seguro em mudança de sample rate e layout.
- Sem exceções atravessando a thread de áudio.

