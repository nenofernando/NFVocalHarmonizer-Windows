#!/usr/bin/env python3
"""Generate EN/PT user manuals for NF Vocal Harmonizer (embedded in the plugin)."""

from pathlib import Path
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
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
        "file": "NF_Vocal_Harmonizer_Manual_EN.pdf",
        "title": "NF Vocal Harmonizer",
        "subtitle": "User Manual · v1.0",
        "brand_line": f"{BRAND} / {AUTHOR}",
        "sections": [
            ("Overview",
             "NF Vocal Harmonizer creates a real-time diatonic harmony voice from a dry vocal. "
             "Choose an interval, press ANALYZE while the DAW plays, then HARMONIZE. "
             "The VOICE / HARMONY editor lets you correct individual harmony notes without changing the source audio."),
            ("Quick start", [
                "Insert the plugin on a vocal track (VST3, AU, or AAX).",
                "Enable AUTO KEY, or set Key and Scale manually.",
                "Pick an interval on the rail (+3rd, −3rd, +5th, …).",
                "Press ANALYZE, play the phrase in your DAW, then stop (or press ANALYZE again).",
                "Press HARMONIZE and balance HARMONY, FORMANT, HUMANIZE, WIDTH and MIX.",
                "Edit individual notes in the HARMONY lane if needed.",
            ]),
            ("Header & presets",
             "The header holds presets (← / →), A/B compare, COPY, SAVE, the ≡ menu, and POWER. "
             "POWER is a soft bypass; HARMONIZE enables the harmony engine. "
             "Open ≡ for the user manuals, window reset, and About."),
            ("ANALYZE & note editor",
             "ANALYZE captures pitch while transport runs and builds note blocks when capture ends. "
             "VOICE (cyan) is read-only. HARMONY (purple) can be selected and dragged vertically to retune one occurrence. "
             "Select tool: click, Shift+click, marquee, Delete/Backspace, Undo/Redo. "
             "Hand / Pan tool: drag horizontally to scroll a zoomed timeline (notes and DSP are unchanged). "
             "Scroll zooms; Option/Alt+scroll pans. SNAP: KEY / CHROMATIC / OFF."),
            ("Controls",
             "HARMONY — level of the created voice. FORMANT — timbre compensation. "
             "HUMANIZE — controlled organic variation. WIDTH — stereo spread of the harmony doubles. "
             "MIX — dry / harmony balance."),
            ("Formats & install",
             "macOS installer places VST3, Audio Unit and AAX system-wide. Rescan plugins in your DAW after install."),
        ],
        "img_captions": {
            "ui_full.png": "Plugin overview",
            "ui_header.png": "Header cluster",
            "ui_editor.png": "VOICE / HARMONY editor",
            "ui_knobs.png": "Main knobs",
        },
    },
    "pt": {
        "file": "NF_Vocal_Harmonizer_Manual_PT.pdf",
        "title": "NF Vocal Harmonizer",
        "subtitle": "Manual do usuário · v1.0",
        "brand_line": f"{BRAND} / {AUTHOR}",
        "sections": [
            ("Visão geral",
             "O NF Vocal Harmonizer cria uma harmonia diatônica em tempo real a partir do vocal seco. "
             "Escolha o intervalo, pressione ANALYZE com a DAW em play e depois HARMONIZE. "
             "O editor VOICE / HARMONY permite corrigir notas individuais sem alterar o áudio original."),
            ("Início rápido", [
                "Insira o plugin numa faixa de voz (VST3, AU ou AAX).",
                "Ative AUTO KEY ou defina Key e Scale manualmente.",
                "Escolha o intervalo no trilho (+3rd, −3rd, +5th, …).",
                "Pressione ANALYZE, toque o trecho na DAW e pare (ou pressione ANALYZE de novo).",
                "Pressione HARMONIZE e ajuste HARMONY, FORMANT, HUMANIZE, WIDTH e MIX.",
                "Edite notas individuais na faixa HARMONY se precisar.",
            ]),
            ("Cabeçalho e presets",
             "O cabeçalho tem presets (← / →), comparação A/B, COPY, SAVE, o menu ≡ e POWER. "
             "POWER é bypass suave; HARMONIZE liga o motor de harmonia. "
             "Abra ≡ para manuais, reset da janela e About."),
            ("ANALYZE e editor de notas",
             "ANALYZE captura o pitch com o transport rodando e monta os blocos ao fim da captura. "
             "VOICE (ciano) é só leitura. HARMONY (roxo) pode ser selecionada e arrastada verticalmente. "
             "Ferramenta Select: clique, Shift+clique, marquee, Delete/Backspace, Undo/Redo. "
             "Ferramenta Hand / Pan: arraste na horizontal para navegar a timeline com zoom (notas e DSP intactos). "
             "Scroll = zoom; Option/Alt+scroll = pan. SNAP: KEY / CHROMATIC / OFF."),
            ("Controles",
             "HARMONY — nível da voz criada. FORMANT — compensação de timbre. "
             "HUMANIZE — variação orgânica controlada. WIDTH — abertura estéreo da harmonia. "
             "MIX — equilíbrio dry / harmonia."),
            ("Formatos e instalação",
             "O instalador macOS coloca VST3, Audio Unit e AAX no sistema. Refaça o scan de plugins na DAW após instalar."),
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
        if "ANALYZE" in title or "ANALYZE" in title.upper() or "editor" in title.lower():
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
