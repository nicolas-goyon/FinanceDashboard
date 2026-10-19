#!/usr/bin/env python3
"""make-icons.py — regenerate the app icon set from code.

The icon is drawn here rather than stored as a hand-made asset so it can be
re-rendered at any size and stays in the project's own palette (Catppuccin
Mocha, the same tokens as docs/styling.md). Run it after changing the design:

    python packaging/icons/make-icons.py

Outputs, all committed:
    finance-dashboard.ico       Windows — 16/24/32/48/64/128/256, the icon of
                                the setup (CPACK_INNOSETUP_ICON_FILE)
    finance-dashboard-256.png   Linux — hicolor 256x256 app icon (.deb/AppImage)
    finance-dashboard-512.png   Linux/AppStream — high-DPI source

Needs Pillow (`pip install pillow`); nothing else in the build depends on it.
"""

from pathlib import Path

from PIL import Image, ImageDraw

OUT_DIR = Path(__file__).resolve().parent

# Catppuccin Mocha — same palette as the app (docs/styling.md).
CRUST = (17, 17, 27)
MANTLE = (24, 24, 37)
SURFACE0 = (49, 50, 68)
MAUVE = (203, 166, 247)
BLUE = (137, 180, 250)
SKY = (137, 220, 235)
GREEN = (166, 227, 161)

SS = 4  # supersampling factor — draw big, downscale for clean antialiasing


def rounded_panel(draw: ImageDraw.ImageDraw, size: int) -> None:
    """Dark rounded square with a faint top-light gradient."""
    radius = int(size * 0.22)
    # Base plate.
    draw.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=CRUST)
    # Gradient: a stack of 1px bands from SURFACE0 down to MANTLE, clipped to
    # the rounded shape via a mask so the corners stay crisp.
    gradient = Image.new("RGB", (size, size))
    grad_draw = ImageDraw.Draw(gradient)
    for y in range(size):
        t = y / max(size - 1, 1)
        colour = tuple(
            round(SURFACE0[i] + (MANTLE[i] - SURFACE0[i]) * (t ** 0.75)) for i in range(3)
        )
        grad_draw.line([(0, y), (size, y)], fill=colour)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    draw._image.paste(gradient, (0, 0), mask)  # noqa: SLF001 — internal but stable


def draw_icon(size: int) -> Image.Image:
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    rounded_panel(draw, size)

    # Three ascending bars along the bottom — the "dashboard" half of the mark.
    floor = size * 0.795
    bar_w = size * 0.125
    gap = size * 0.075
    tops = (0.575, 0.505, 0.435)          # fractions of size, measured from the top
    colours = (MAUVE, BLUE, SKY)
    x = size * 0.185
    for top, colour in zip(tops, colours):
        draw.rounded_rectangle(
            [x, size * top, x + bar_w, floor],
            radius=bar_w * 0.30,
            fill=colour + (255,),
        )
        x += bar_w + gap

    # Trend line, monotonically rising and clear of every bar top — the
    # "finance" half. Kept in green so it never merges with a bar.
    points = [
        (size * 0.165, size * 0.365),
        (size * 0.390, size * 0.300),
        (size * 0.585, size * 0.320),
        (size * 0.815, size * 0.195),
    ]
    draw.line(points, fill=GREEN + (255,), width=max(int(size * 0.048), 1), joint="curve")
    head = points[-1]
    r = size * 0.058
    draw.ellipse([head[0] - r, head[1] - r, head[0] + r, head[1] + r], fill=GREEN + (255,))
    r_in = size * 0.025
    draw.ellipse([head[0] - r_in, head[1] - r_in, head[0] + r_in, head[1] + r_in], fill=CRUST + (255,))

    return canvas


def render(size: int) -> Image.Image:
    return draw_icon(size * SS).resize((size, size), Image.LANCZOS)


def main() -> None:
    png_256 = render(256)
    png_256.save(OUT_DIR / "finance-dashboard-256.png")
    render(512).save(OUT_DIR / "finance-dashboard-512.png")

    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    render(256).save(
        OUT_DIR / "finance-dashboard.ico",
        format="ICO",
        sizes=[(s, s) for s in ico_sizes],
    )
    print("wrote", ", ".join(p.name for p in sorted(OUT_DIR.glob("finance-dashboard*"))))


if __name__ == "__main__":
    main()
