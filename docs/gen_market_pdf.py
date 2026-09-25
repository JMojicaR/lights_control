#!/usr/bin/env python3
# Convert docs/market_analysis_UART.md to a styled PDF via ReportLab.
import re, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                TableStyle, HRFlowable, KeepTogether)

SRC = "/home/hermesbot/lights_control/docs/market_analysis_UART.md"
OUT = "/home/hermesbot/lights_control/docs/market_analysis_UART.pdf"

ACCENT = colors.HexColor("#1f4e79")
ACCENT2 = colors.HexColor("#2e75b6")
LIGHT = colors.HexColor("#eef4fa")
GREY = colors.HexColor("#666666")

ss = getSampleStyleSheet()
styles = {
    "title": ParagraphStyle("title", parent=ss["Title"], fontSize=20, leading=25,
                            textColor=ACCENT, spaceAfter=2*mm),
    "subtitle": ParagraphStyle("subtitle", parent=ss["Normal"], fontSize=10,
                               leading=13, textColor=GREY, spaceAfter=6*mm),
    "h1": ParagraphStyle("h1", parent=ss["Heading1"], fontSize=14, leading=18,
                         textColor=colors.white, backColor=ACCENT, spaceBefore=6*mm,
                         spaceAfter=3*mm, leftIndent=0, borderPadding=4),
    "h2": ParagraphStyle("h2", parent=ss["Heading2"], fontSize=12, leading=15,
                         textColor=ACCENT2, spaceBefore=4*mm, spaceAfter=2*mm),
    "h3": ParagraphStyle("h3", parent=ss["Heading3"], fontSize=10.5, leading=13,
                         textColor=ACCENT, spaceBefore=3*mm, spaceAfter=1.5*mm),
    "body": ParagraphStyle("body", parent=ss["Normal"], fontSize=9.5, leading=13,
                           spaceAfter=1.6*mm),
    "bullet": ParagraphStyle("bullet", parent=ss["Normal"], fontSize=9.5,
                             leading=13, leftIndent=6*mm, bulletIndent=2*mm,
                             spaceAfter=0.8*mm),
    "quote": ParagraphStyle("quote", parent=ss["Normal"], fontSize=9, leading=12.5,
                            leftIndent=8*mm, textColor=GREY, spaceAfter=2*mm,
                            borderPadding=0),
    "cell": ParagraphStyle("cell", parent=ss["Normal"], fontSize=7.6, leading=9.5),
    "cellhead": ParagraphStyle("cellhead", parent=ss["Normal"], fontSize=7.8,
                               leading=9.5, textColor=colors.white),
    "code": ParagraphStyle("code", parent=ss["Normal"], fontName="Courier",
                           fontSize=8, leading=10, leftIndent=4*mm,
                           spaceAfter=2*mm, textColor=colors.HexColor("#333333")),
}


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def md_inline(s):
    s = esc(s)
    s = re.sub(r"`([^`]+)`", r'<font face="Courier" size="8">\1</font>', s)
    s = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", s)
    s = re.sub(r"\*(.+?)\*", r"<i>\1</i>", s)
    return s


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]


def is_sep_row(cells):
    return all(re.fullmatch(r":?-{2,}:?", c) for c in cells if c != "")


def build_story(md_text):
    story = []
    lines = md_text.split("\n")
    i = 0
    n = len(lines)
    first_h1 = True
    while i < n:
        line = lines[i]
        stripped = line.strip()

        # blank
        if stripped == "":
            i += 1
            continue

        # fenced code block
        if stripped.startswith("```"):
            i += 1
            buf = []
            while i < n and not lines[i].strip().startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1  # skip closing fence
            story.append(Paragraph("<br/>".join(esc(x) for x in buf), styles["code"]))
            continue

        # horizontal rule
        if re.fullmatch(r"(-{3,}|\*{3,})", stripped):
            story.append(Spacer(1, 2*mm))
            story.append(HRFlowable(width="100%", thickness=0.7, color=LIGHT))
            story.append(Spacer(1, 2*mm))
            i += 1
            continue

        # table block
        if stripped.startswith("|"):
            # gather consecutive table lines
            tbl = []
            while i < n and lines[i].strip().startswith("|"):
                cells = split_row(lines[i])
                tbl.append(cells)
                i += 1
            if len(tbl) >= 2:
                header = tbl[0]
                body = []
                for r in tbl[1:]:
                    if is_sep_row(r):
                        continue
                    body.append(r)
                ncols = max(len(r) for r in tbl)
                data = []
                data.append([Paragraph(md_inline(c), styles["cellhead"]) for c in header])
                for r in body:
                    r = (r + [""] * ncols)[:ncols]
                    data.append([Paragraph(md_inline(c), styles["cell"]) for c in r])
                avail = A4[0] - 30*mm
                colw = avail / ncols
                t = Table(data, colWidths=[colw] * ncols, repeatRows=1)
                t.setStyle(TableStyle([
                    ("BACKGROUND", (0, 0), (-1, 0), ACCENT),
                    ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, LIGHT]),
                    ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#b8c9dc")),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 3),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                    ("TOPPADDING", (0, 0), (-1, -1), 2.5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 2.5),
                ]))
                story.append(KeepTogether(t))
                story.append(Spacer(1, 2.5*mm))
            continue

        # headers
        m = re.match(r"^(#{1,4})\s+(.*)$", stripped)
        if m:
            level = len(m.group(1))
            text = md_inline(m.group(2))
            if level == 1:
                if first_h1:
                    story.append(Paragraph(text, styles["title"]))
                    first_h1 = False
                else:
                    story.append(Paragraph(text, styles["h1"]))
            elif level == 2:
                story.append(Paragraph(text, styles["h1"]))
            elif level == 3:
                story.append(Paragraph(text, styles["h2"]))
            else:
                story.append(Paragraph(text, styles["h3"]))
            i += 1
            continue

        # blockquote
        if stripped.startswith(">"):
            buf = []
            while i < n and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip().lstrip(">").strip())
                i += 1
            story.append(Paragraph("▸ " + md_inline(" ".join(buf)), styles["quote"]))
            continue

        # bullet list
        if re.match(r"^[-*]\s+", stripped):
            story.append(Paragraph("• " + md_inline(re.sub(r"^[-*]\s+", "", stripped)),
                                   styles["bullet"]))
            i += 1
            continue

        # numbered list
        m = re.match(r"^(\d+)\.\s+(.*)$", stripped)
        if m:
            story.append(Paragraph(f"{m.group(1)}. " + md_inline(m.group(2)),
                                   styles["bullet"]))
            i += 1
            continue

        # plain paragraph
        story.append(Paragraph(md_inline(stripped), styles["body"]))
        i += 1
    return story


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(GREY)
    canvas.drawString(15*mm, 10*mm, "lights_control — uart_option")
    canvas.drawRightString(A4[0]-15*mm, 10*mm, f"Página {canvas.getPageNumber()}")
    canvas.restoreState()


def main():
    with open(SRC, "r", encoding="utf-8") as f:
        md_text = f.read()
    story = build_story(md_text)
    doc = SimpleDocTemplate(OUT, pagesize=A4,
                            leftMargin=15*mm, rightMargin=15*mm,
                            topMargin=14*mm, bottomMargin=18*mm,
                            title="Market Analysis — UART/mmWave DIY Kit")
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print("WROTE", OUT)


if __name__ == "__main__":
    main()
