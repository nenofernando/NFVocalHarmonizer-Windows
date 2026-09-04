# Editor cirúrgico VOICE / HARMONY

## Checkpoint

- Tag/commit base: `eaaa71a` (`checkpoint/pre-harmony-note-editor`)

## Uso

1. Pressione **ANALYZE** (fica `ARM...`) e dê play na DAW.
2. Ao parar o transport, os blocos são gerados automaticamente (ou clique ANALYZE de novo para fechar a captura).
3. Faixa **VOICE** (ciano): só leitura.
4. Faixa **HARMONY** (roxo): clique para selecionar; arraste verticalmente para editar só aquela ocorrência.
5. Seleção: clique = uma nota (contorno branco); Shift+clique = adiciona/remove; arrasto em área vazia = marquee (Shift+marquee acrescenta). VOICE e HARMONY do mesmo evento destacam juntos.
6. Arrasto normal na nota = passos de 1 semitom (com SNAP). Shift durante o arrasto de pitch = 1 cent.
7. Delete/Backspace remove só do mapa de edição (não apaga áudio da DAW). Cmd/Ctrl+A seleciona notas visíveis. Exclusão múltipla = um Undo.
8. Duplo clique = restaura o automático da nota. Esc = cancela arrasto/marquee ou limpa seleção.
9. Cmd/Ctrl+Z e Shift+Cmd/Ctrl+Z (ou Ctrl+Y) = Undo/Redo.
10. **SNAP**: KEY (padrão) / CHROMATIC / OFF — controle dentro do painel.
11. Novo ANALYZE reconstrói notas removidas.

## Motor

Offset manual aplicado **depois** do alvo diatônico automático e **antes** do pitch shifter, com suavização de ~30 ms. Notas sem edição permanecem na sonoridade anterior.

## Estado

Edições ficam em `HARMONY_NOTE_EDITS` no state da instância/sessão. Presets globais (APVTS) **não** carregam edições de outra gravação.
