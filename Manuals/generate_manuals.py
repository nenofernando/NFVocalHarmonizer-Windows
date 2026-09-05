#!/usr/bin/env python3
"""Generate EN/PT user manuals for NF Vocal Harmonizer (embedded in the plugin)."""

from pathlib import Path
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Image, PageBreak, KeepTogether, ListFlowable, ListItem
)

ROOT = Path(__file__).resolve().parent
IMG = ROOT / "images"
OUT = ROOT / "pdf"
OUT.mkdir(parents=True, exist_ok=True)

BRAND = "NF Audio Tools"
AUTHOR = "Nenno Fernando"  # two N's
COPYRIGHT = f"© 2026 {BRAND} / {AUTHOR}. All rights reserved."
CYAN = HexColor("#2fe0ee")
INK = HexColor("#1a2230")
MUTED = HexColor("#4a5564")


def styles():
    base = getSampleStyleSheet()
    return {
        "cover_title": ParagraphStyle(
            "cover_title", parent=base["Title"], fontName="Helvetica-Bold",
            fontSize=26, leading=30, textColor=INK, spaceAfter=6),
        "cover_sub": ParagraphStyle(
            "cover_sub", parent=base["Normal"], fontName="Helvetica",
            fontSize=12, leading=16, textColor=MUTED, spaceAfter=4),
        "h1": ParagraphStyle(
            "h1", parent=base["Heading1"], fontName="Helvetica-Bold",
            fontSize=16, leading=20, textColor=INK, spaceBefore=14, spaceAfter=8),
        "h2": ParagraphStyle(
            "h2", parent=base["Heading2"], fontName="Helvetica-Bold",
            fontSize=12, leading=15, textColor=INK, spaceBefore=10, spaceAfter=5),
        "body": ParagraphStyle(
            "body", parent=base["Normal"], fontName="Helvetica",
            fontSize=10, leading=14, textColor=INK, spaceAfter=6),
        "foot": ParagraphStyle(
            "foot", parent=base["Normal"], fontName="Helvetica",
            fontSize=8, leading=10, textColor=MUTED),
        "bullet": ParagraphStyle(
            "bullet", parent=base["Normal"], fontName="Helvetica",
            fontSize=10, leading=13, textColor=INK),
    }


def img(name, max_w=170 * mm, max_h=95 * mm):
    path = IMG / name
    if not path.exists():
        return Spacer(1, 1)
    im = Image(str(path))
    w, h = im.imageWidth, im.imageHeight
    scale = min(max_w / w, max_h / h, 1.0)
    im.drawWidth = w * scale
    im.drawHeight = h * scale
    return im


def bullets(items, st):
    return ListFlowable(
        [ListItem(Paragraph(t, st["bullet"]), leftIndent=8, bulletColor=CYAN) for t in items],
        bulletType="bullet", start="•", leftIndent=12, bulletFontSize=10)


def footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(HexColor("#d0d6de"))
    canvas.setLineWidth(0.4)
    canvas.line(18 * mm, 14 * mm, A4[0] - 18 * mm, 14 * mm)
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(MUTED)
    canvas.drawString(18 * mm, 8 * mm, COPYRIGHT)
    canvas.drawRightString(A4[0] - 18 * mm, 8 * mm, f"{doc.page}")
    canvas.restoreState()


CONTENT = {
    "en": {
        "file": "NF_Vocal_Harmonizer_User_Manual_English.pdf",
        "title": "NF Vocal Harmonizer",
        "subtitle": "User Manual · v1.0",
        "brand_line": f"{BRAND} / {AUTHOR}",
        "sections": [
            ("Overview",
             "NF Vocal Harmonizer creates a real-time diatonic harmony voice from a dry vocal. "
             "Choose an interval on the rail (default opens on the central VOICE / tonic), press ANALYZE while the DAW plays, "
             "then HARMONIZE. The VOICE / HARMONY editor shows vocal blobs (not MIDI bricks) so you can correct "
             "individual harmony notes without changing the source audio or the pitch-shifter sound engine."),
            ("Quick start", [
                "Insert the plugin on a vocal track (VST3, AU, or AAX).",
                "Enable AUTO KEY, or set Key and Scale manually.",
                "Pick an interval on the rail: ±3rd, ±5th, ±6th, ±8ve, or VOICE (tonic / unison) in the centre.",
                "Press ANALYZE, play the phrase in your DAW, then stop (or press ANALYZE again).",
                "To capture later bars without erasing earlier ones: ANALYZE again, play the next section, stop — notes append forward.",
                "Press HARMONIZE and balance HARMONY, FORMANT, HUMANIZE, WIDTH and MIX.",
                "Use editor tools (SEL / FLAT / LINE / CUT) to refine HARMONY notes if needed.",
            ]),
            ("Header & presets",
             "The header holds presets (← / →), A/B compare, COPY, SAVE, the ≡ menu, and POWER. "
             "Select <b>Default</b> in the preset list (or ≡ → Reset parameters to Default) to restore factory knobs, "
             "interval (VOICE / tonic), key/scale and switches — without clearing your ANALYZE note map. "
             "POWER is a soft bypass; HARMONIZE enables the harmony engine. "
             "Open ≡ for user manuals (EN/PT), window reset, parameter reset, and About. "
             f"Product credit: {BRAND} — By {AUTHOR}."),
            ("ANALYZE & note map",
             "ANALYZE uses an explicit state machine: empty → capturing → ready. Capture runs only while ANALYZE is armed — "
             "not from DAW Play alone. Button labels: ANALYZE / ANALYZING / ANALYZED. "
             "Finish with a second ANALYZE click or Play→Stop. "
             "Later ANALYZE passes append new bars ahead; overlapping recaptures are skipped so earlier notes and edits stay intact. "
             "Frozen vocal visualisation (waveform + pitch) is merged forward the same way."),
            ("VOICE / HARMONY editor",
             "VOICE (cyan blobs) is read-only. HARMONY (violet blobs) is editable. "
             "Pitch curves and amplitude envelopes come from analysis (observe-only for drawing). "
             "Drag HARMONY vertically (1 st steps; Shift = cents). Arrow keys nudge selection (Shift = cents). "
             "Double-click resets a note. Delete/Backspace removes selected harmony notes from the edit map (DSP falls back to auto). "
             "FIT auto-fits the vertical pitch range. Zoom − / + (left / right). "
             "Scroll zooms at the pointer; Option/Alt+scroll pans time. Drag the centre tab under the editor to grow or shrink the panel "
             "(the interval rail may compress — that is expected)."),
            ("Editor tools (toolbar)", [
                "<b>SEL</b> — select and drag pitch (default). Click, Shift+click, Option/Alt marquee.",
                "<b>FLAT</b> — pencil flat: draw a constant pitch correction across a harmony note.",
                "<b>LINE</b> — pencil slope: draw a straight pitch glide between two points on a note.",
                "<b>CUT</b> — scissors: click a note to split it at that time (undoable).",
                "Esc cancels a pencil gesture or returns to SEL. Snap: KEY / CHROMATIC / OFF.",
            ]),
            ("Interval rail",
             "Vertical reference: +8ve, +6th, +5th, +3rd, <b>VOICE</b> (tonic centre), −3rd, −5th, −6th, −8ve. "
             "New instances open on VOICE (unison). One interval is active at a time. "
             "Diatonic intervals follow Key/Scale (or AUTO KEY)."),
            ("Controls",
             "HARMONY — level of the created voice. FORMANT — timbre compensation. "
             "HUMANIZE — controlled organic variation. WIDTH — stereo spread of the harmony doubles. "
             "MIX — dry / harmony balance."),
            ("Formats & install",
             "macOS DMG installs VST3, Audio Unit and AAX system-wide (English installer text). "
             "Windows installer places VST3. Rescan plugins in your DAW after install. "
             f"© 2026 {BRAND} / {AUTHOR}. All rights reserved."),
        ],
        "img_captions": {
            "ui_full.png": "Plugin overview",
            "ui_header.png": "Header cluster",
            "ui_editor.png": "VOICE / HARMONY editor",
            "ui_knobs.png": "Main knobs",
        },
    },
    "pt": {
        "file": "NF_Vocal_Harmonizer_Manual_Portugues.pdf",
        "title": "NF Vocal Harmonizer",
        "subtitle": "Manual do usuário · v1.0",
        "brand_line": f"{BRAND} / {AUTHOR}",
        "sections": [
            ("Visão geral",
             "O NF Vocal Harmonizer cria uma harmonia diatônica em tempo real a partir do vocal seco. "
             "Escolha o intervalo no trilho (abre por padrão na tônica central VOICE), pressione ANALYZE com a DAW em play "
             "e depois HARMONIZE. O editor VOICE / HARMONY mostra blobs vocais (não blocos MIDI) para corrigir "
             "notas individuais sem alterar o áudio original nem o motor de pitch."),
            ("Início rápido", [
                "Insira o plugin numa faixa de voz (VST3, AU ou AAX).",
                "Ative AUTO KEY ou defina Key e Scale manualmente.",
                "Escolha o intervalo: ±3rd, ±5th, ±6th, ±8ve, ou VOICE (tônica / uníssono) no centro.",
                "Pressione ANALYZE, toque o trecho na DAW e pare (ou pressione ANALYZE de novo).",
                "Para capturar compassos seguintes sem apagar os anteriores: ANALYZE de novo, toque a seção seguinte, pare — as notas são anexadas.",
                "Pressione HARMONIZE e ajuste HARMONY, FORMANT, HUMANIZE, WIDTH e MIX.",
                "Use as ferramentas (SEL / FLAT / LINE / CUT) para refinar as notas HARMONY se precisar.",
            ]),
            ("Cabeçalho e presets",
             "O cabeçalho tem presets (← / →), comparação A/B, COPY, SAVE, o menu ≡ e POWER. "
             "Selecione <b>Default</b> na lista (ou ≡ → Reset parameters to Default) para restaurar knobs, "
             "intervalo (VOICE / tônica), key/scale e chaves — sem apagar o mapa do ANALYZE. "
             "POWER é bypass suave; HARMONIZE liga o motor. "
             "Abra ≡ para manuais (EN/PT), reset da janela, reset de parâmetros e About. "
             f"Créditos: {BRAND} — By {AUTHOR}."),
            ("ANALYZE e mapa de notas",
             "ANALYZE usa estados explícitos: empty → capturing → ready. A captura só ocorre com ANALYZE armado — "
             "não apenas com o Play da DAW. Textos: ANALYZE / ANALYZING / ANALYZED. "
             "Finalize com segundo clique em ANALYZE ou Play→Stop. "
             "Novas passagens anexam compassos à frente; recapturas sobrepostas são ignoradas e edições anteriores permanecem. "
             "A visualização vocal congelada (waveform + pitch) também é mesclada para a frente."),
            ("Editor VOICE / HARMONY",
             "VOICE (blobs ciano) é só leitura. HARMONY (blobs violeta) é editável. "
             "Curvas de pitch e envelope vêm da análise (só observação para o desenho). "
             "Arraste HARMONY na vertical (1 st; Shift = cents). Setas do teclado movem a seleção (Shift = cents). "
             "Duplo clique restaura a nota. Delete/Backspace remove notas do mapa de edição. "
             "FIT ajusta a faixa vertical. Zoom − / + (esquerda / direita). "
             "Scroll = zoom no ponteiro; Option/Alt+scroll = pan. A aba central sob o editor alonga ou encolhe o painel "
             "(o trilho de intervalos pode comprimir — é esperado)."),
            ("Ferramentas do editor (barra)", [
                "<b>SEL</b> — seleção e arrasto de pitch (padrão). Clique, Shift+clique, marquee com Option/Alt.",
                "<b>FLAT</b> — lápis plano: correção constante de pitch na nota.",
                "<b>LINE</b> — lápis inclinado: reta de pitch entre dois pontos na nota.",
                "<b>CUT</b> — tesoura: clique na nota para cortar nesse tempo (com undo).",
                "Esc cancela o lápis ou volta para SEL. Snap: KEY / CHROMATIC / OFF.",
            ]),
            ("Trilho de intervalos",
             "Referência vertical: +8ve, +6th, +5th, +3rd, <b>VOICE</b> (tônica central), −3rd, −5th, −6th, −8ve. "
             "Novas instâncias abrem em VOICE (uníssono). Um intervalo ativo por vez. "
             "Intervalos diatônicos seguem Key/Scale (ou AUTO KEY)."),
            ("Controles",
             "HARMONY — nível da voz criada. FORMANT — compensação de timbre. "
             "HUMANIZE — variação orgânica controlada. WIDTH — abertura estéreo da harmonia. "
             "MIX — equilíbrio dry / harmonia."),
            ("Formatos e instalação",
             "O DMG macOS instala VST3, Audio Unit e AAX (textos do instalador em inglês). "
             "O instalador Windows coloca VST3. Refaça o scan de plugins na DAW após instalar. "
             f"© 2026 {BRAND} / {AUTHOR}. Todos os direitos reservados."),
        ],
        "img_captions": {
            "ui_full.png": "Visão geral do plugin",
            "ui_header.png": "Cabeçalho",
            "ui_editor.png": "Editor VOICE / HARMONY",
            "ui_knobs.png": "Knobs principais",
        },
    },
}


def build(lang: str):
    meta = CONTENT[lang]
    st = styles()
    path = OUT / meta["file"]
    doc = SimpleDocTemplate(
        str(path), pagesize=A4,
        leftMargin=18 * mm, rightMargin=18 * mm,
        topMargin=16 * mm, bottomMargin=18 * mm,
        title=f"{meta['title']} — {meta['subtitle']}",
        author=f"{BRAND} / {AUTHOR}",
    )
    story = []
    story.append(Paragraph(meta["title"], st["cover_title"]))
    story.append(Paragraph(meta["subtitle"], st["cover_sub"]))
    story.append(Paragraph(meta["brand_line"], st["cover_sub"]))
    story.append(Spacer(1, 8))
    story.append(img("ui_full.png", max_h=110 * mm))
    story.append(Paragraph(meta["img_captions"]["ui_full.png"], st["foot"]))
    story.append(Spacer(1, 10))
    story.append(Paragraph(COPYRIGHT, st["foot"]))
    story.append(PageBreak())

    for section in meta["sections"]:
        title, body = section[0], section[1]
        block = [Paragraph(title, st["h1"])]
        if isinstance(body, list):
            block.append(bullets(body, st))
        else:
            block.append(Paragraph(body, st["body"]))
        if title.lower().startswith(("header", "cabeçalho")):
            block.append(Spacer(1, 4))
            block.append(img("ui_header.png", max_h=45 * mm))
            block.append(Paragraph(meta["img_captions"]["ui_header.png"], st["foot"]))
        if "ANALYZE" in title or "editor" in title.lower() or "ferramentas" in title.lower() or "tools" in title.lower():
            block.append(Spacer(1, 4))
            block.append(img("ui_editor.png", max_h=55 * mm))
            block.append(Paragraph(meta["img_captions"]["ui_editor.png"], st["foot"]))
        if title.lower().startswith(("controls", "controles")):
            block.append(Spacer(1, 4))
            block.append(img("ui_knobs.png", max_h=50 * mm))
            block.append(Paragraph(meta["img_captions"]["ui_knobs.png"], st["foot"]))
        story.append(KeepTogether(block))

    story.append(Spacer(1, 16))
    story.append(Paragraph(COPYRIGHT, st["body"]))
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print("wrote", path, path.stat().st_size)


if __name__ == "__main__":
    build("en")
    build("pt")
