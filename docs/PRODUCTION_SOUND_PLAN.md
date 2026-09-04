# Plano para chegar à sonoridade comercial

## O que já existe

O motor incluído produz harmonia em tempo real, segue a escala, preserva o desvio expressivo da nota e evita gerar harmonia quando a confiança vocal cai. É uma base funcional completa para desenvolvimento e comparação.

## O que deve ser decidido por áudio, não por aparência

1. Granular interno versus PSOLA versus phase-vocoder com envelope espectral.
2. Preservação formântica real em +8ve/-8ve e sextas.
3. Tratamento de consoantes não periódicas sem smear.
4. Continuidade em portamento e troca de nota.
5. Tempo mínimo de detecção sem instabilidade.
6. Humanize sem chorus metálico.

## Banco de testes sugerido

- Vozes masculinas e femininas, graves e agudas, secas e com ruído de sala.
- Frases faladas/cantadas, staccato, legato, vibrato e melisma.
- Sibilantes, plosivas e respirações isoladas.
- Intervalos extremos em tonalidades maiores e menores.
- Arquivos de referência com notas MIDI conhecidas para medir erro em cents.

## Critérios mínimos

- Mediana de erro tonal menor que 10 cents em regiões estáveis.
- Nenhuma nota harmônica aleatória em silêncio/respiração.
- Trocas sem clique e sem NaN/Inf.
- Latência constante e corretamente reportada.
- Recall idêntico entre AU/VST3/AAX.
- Teste auditivo cego contra pelo menos duas referências profissionais.

## Licenciamento

Caso seja escolhido um SDK comercial de pitch/formant, registrar licença, plataformas permitidas, obrigação de créditos e política de redistribuição antes de incorporar binários ao repositório.

