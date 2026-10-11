"""Create the A4 DEFN poster and a reusable, static QR code.

Run these commands from the repository root:

    python -m pip install reportlab Pillow
    python scripts/capture.py --shot winter_fireline --stills --fixture max_roster --no-cursor --out-dir build/capture/screenshots/takes
    python scripts/create_defn_poster.py

Before capturing, build the game's GDExtension and configure Godot as described
in BUILD_ENV.md. The capture command finds Godot through GODOT_BIN or PATH;
alternatively, append --godot <path-to-godot>. See defn/CAPTURE_TOOLING.md for
the capture rig's setup and options.

The winter_fireline shot saves its 47-second "payoff" frame to
build/capture/screenshots/takes/stills/winter_fireline_payoff.png. The max_roster
fixture unlocks all units in an isolated save; --no-cursor hides capture overlays.
Stills use the window's render resolution (4K on the original HiDPI setup).
The poster crops the captured frame to hide the HUD and deployment cards.

The poster generator requires Pillow, ReportLab, and the Windows Arial and Impact
fonts. It writes defn-a4-poster.pdf, defn-qr.svg, and defn-qr.png to output/pdf/.
Capture inputs and poster outputs are resolved relative to this repository,
regardless of the working directory used to invoke the poster generator.
"""

from pathlib import Path
from xml.sax.saxutils import escape

from PIL import Image, ImageDraw
from reportlab.graphics.barcode import qrencoder
from reportlab.lib.colors import HexColor, black, white
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "output/pdf"
URL = "https://terrible-entertainment.itch.io/defn"
ADDRESS = URL.removeprefix("https://")
ART = ROOT / "build/capture/screenshots/takes/stills/winter_fireline_payoff.png"
PDF = OUTPUT / "defn-a4-poster.pdf"
INK = HexColor("#172330")
MUTED = HexColor("#506170")
AMBER = HexColor("#F1BD58")
PAPER = HexColor("#FFFDF8")
PAGE_WIDTH, PAGE_HEIGHT = A4


def register_fonts():
    fonts = Path("C:/Windows/Fonts")
    for name, filename in (
        ("Display", "impact.ttf"),
        ("Body", "arial.ttf"),
        ("Bold", "arialbd.ttf"),
    ):
        pdfmetrics.registerFont(TTFont(name, str(fonts / filename)))


def rectangle(pdf, x, top, width, height, color):
    pdf.setFillColor(color)
    pdf.rect(x * mm, PAGE_HEIGHT - (top + height) * mm,
             width * mm, height * mm, fill=1, stroke=0)


def label(pdf, text, x, baseline, font="Body", size=10, color=INK,
          align="left", tracking=0):
    width = pdfmetrics.stringWidth(text, font, size)
    width += max(0, len(text) - 1) * tracking
    anchor = x * mm
    if align == "center":
        anchor -= width / 2
    elif align == "right":
        anchor -= width
    assert anchor >= 10 * mm - 0.01, text
    assert anchor + width <= PAGE_WIDTH - 10 * mm + 0.01, text
    run = pdf.beginText(anchor, PAGE_HEIGHT - baseline * mm)
    run.setFont(font, size)
    run.setFillColor(color)
    run.setCharSpace(tracking)
    run.textOut(text)
    pdf.drawText(run)


def make_qr():
    qr = qrencoder.QRCode(None, qrencoder.QRErrorCorrectLevel.Q)
    qr.addData(URL)
    qr.make()
    # Include the four-module white quiet zone in every output.
    border = 4
    size = qr.getModuleCount() + border * 2
    modules = [
        [border <= row < size - border
         and border <= col < size - border
         and qr.isDark(row - border, col - border)
         for col in range(size)]
        for row in range(size)
    ]
    return modules, qr.version


def write_qr_assets(modules):
    count = len(modules)
    pixels_per_module = 24
    image = Image.new("RGB", (count * pixels_per_module,) * 2, "white")
    draw = ImageDraw.Draw(image)
    svg_squares = []
    for row, cells in enumerate(modules):
        for col, dark in enumerate(cells):
            if dark:
                draw.rectangle((col * pixels_per_module, row * pixels_per_module,
                                (col + 1) * pixels_per_module - 1,
                                (row + 1) * pixels_per_module - 1), fill="black")
                svg_squares.append(f"M{col},{row}h1v1h-1z")
    image.save(OUTPUT / "defn-qr.png")
    svg = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" width="72mm" height="72mm" '
        f'viewBox="0 0 {count} {count}" shape-rendering="crispEdges">\n'
        f'  <title>DEFN on itch.io</title>\n'
        f'  <desc>{escape(URL)}</desc>\n'
        f'  <rect width="{count}" height="{count}" fill="#fff"/>\n'
        f'  <path d="{"".join(svg_squares)}" fill="#000"/>\n'
        '</svg>\n'
    )
    (OUTPUT / "defn-qr.svg").write_text(svg, encoding="utf-8")


def draw_qr(pdf, modules, x=20, top=176, side=82):
    count = len(modules)
    unit = side * mm / count
    origin_x = x * mm
    origin_y = PAGE_HEIGHT - (top + side) * mm
    rectangle(pdf, x, top, side, side, white)
    pdf.setFillColor(black)
    path = pdf.beginPath()
    # Merge adjacent dark cells into horizontal runs: all edges remain vector.
    for row, cells in enumerate(modules):
        col = 0
        while col < count:
            if not cells[col]:
                col += 1
                continue
            start = col
            while col < count and cells[col]:
                col += 1
            path.rect(origin_x + start * unit,
                      origin_y + (count - row - 1) * unit,
                      (col - start) * unit, unit)
    pdf.drawPath(path, fill=1, stroke=0)
    pdf.linkURL(URL, (origin_x, origin_y,
                      origin_x + side * mm, origin_y + side * mm), relative=0)


def draw_art(pdf):
    # Layout clipping removes the capture HUD and deployment cards without
    # altering the source image or inventing gameplay.
    x, top, width, height = 10, 66, 190, 83
    source_width, source_height = Image.open(ART).size
    scale = width * mm / source_width
    # Match the original 190-pixel crop at 3840-wide capture resolution.
    source_top = source_width * 190 / 3840
    pdf.saveState()
    crop = pdf.beginPath()
    crop.rect(x * mm, PAGE_HEIGHT - (top + height) * mm,
              width * mm, height * mm)
    pdf.clipPath(crop, stroke=0, fill=0)
    image_top = PAGE_HEIGHT - top * mm + source_top * scale
    pdf.drawImage(str(ART), x * mm, image_top - source_height * scale,
                  width=source_width * scale, height=source_height * scale)
    pdf.restoreState()
    rectangle(pdf, 14, top + 4, 28, 4.2, INK)
    label(pdf, "ACTUAL GAMEPLAY", 16, top + 7,
          "Bold", 5.8, white, tracking=0.3)


def create_poster(modules):
    pdf = canvas.Canvas(str(PDF), pagesize=A4, pageCompression=1)
    pdf.setTitle("DEFN - A4 Poster")
    pdf.setAuthor("Terrible Entertainment")
    pdf.setSubject(f"A4 game poster with a static QR code for {URL}")
    rectangle(pdf, 0, 0, 210, 297, white)

    rectangle(pdf, 10, 10, 190, 56, INK)
    rectangle(pdf, 10, 10, 190, 1.4, AMBER)
    label(pdf, "TERRIBLE ENTERTAINMENT PRESENTS", 17, 21,
          "Bold", 7.2, PAPER, tracking=1.5)
    label(pdf, "DEFN", 16, 59.5, "Display", 116, PAPER, tracking=1)

    # A compact genre label balances the large game title.
    rectangle(pdf, 153, 32, 1, 24, AMBER)
    for text, baseline in (
        ("TACTICAL", 38), ("TUG-OF-WAR", 45), ("DEFENSE", 52)
    ):
        label(pdf, text, 158, baseline, "Bold", 8.8, PAPER, tracking=0.7)

    draw_art(pdf)
    rectangle(pdf, 10, 149, 190, 1.2, AMBER)

    draw_qr(pdf, modules)
    label(pdf, "SCAN TO", 114, 204, "Bold", 11, MUTED, tracking=2)
    label(pdf, "PLAY DEFN", 113.5, 219, "Display", 34, INK, tracking=0.5)
    label(pdf, "Open the game on itch.io", 114, 229, "Body", 10.5, INK)
    rectangle(pdf, 114, 241, 27, 1.2, AMBER)

    label(pdf, ADDRESS, 105, 268, "Bold", 10.5, INK, "center")
    url_width = pdfmetrics.stringWidth(ADDRESS, "Bold", 10.5)
    pdf.linkURL(URL, ((PAGE_WIDTH - url_width) / 2, PAGE_HEIGHT - 269 * mm,
                      (PAGE_WIDTH + url_width) / 2, PAGE_HEIGHT - 264 * mm))
    pdf.showPage()
    pdf.save()


if __name__ == "__main__":
    if not ART.is_file():
        raise SystemExit(
            f"Poster artwork is missing: {ART}\n"
            "Run this command from the repository root first:\n"
            "  python scripts/capture.py --shot winter_fireline --stills "
            "--fixture max_roster --no-cursor "
            "--out-dir build/capture/screenshots/takes"
        )
    OUTPUT.mkdir(parents=True, exist_ok=True)
    register_fonts()
    modules, version = make_qr()
    write_qr_assets(modules)
    create_poster(modules)
    print(f"PDF: {PDF}")
    print(f"QR: version {version}, correction Q, {len(modules)} modules including quiet zone")
    print(f"Encoded URL: {URL}")
