#!/usr/bin/env python3
# Draws MViewer's stand-in logo: the "MV" mark as the program icon, and the loading splash.
#
#   pip install pillow
#   python tools/make-logo.py
#
# Writes into resources/: appicon.png (512 px), appicon-16/24/32/48.png, appicon.ico (16 to
# 256 px) and splash.png. The mark is a placeholder until a designed logo replaces it;
# resources/LOGO.md says which files to swap, and none of them needs this script.
#
# The colours are Theme.h's tokens and the letters Cinzel, the wordmark's fallback face, so the
# placeholder at least sits in the program's own palette.

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "resources")
FONTS = os.path.join(RES, "fonts")

BG_CHROME = (0x0A, 0x0A, 0x0C)     # tok::bgChrome -- the splash's ground
ACCENT = (0xA8, 0x55, 0xF7)        # tok::accent
ACCENT_TEXT = (0xC4, 0xB5, 0xFD)   # tok::accentText
WORDMARK = (0xD8, 0xC7, 0xF5)      # tok::brandWordmark
FG_MUTED = (0x8C, 0x8C, 0x95)      # tok::fgMuted

TILE_TOP = (0x2C, 0x18, 0x50)      # the tile: a violet that darkens towards the bottom
TILE_BOTTOM = (0x12, 0x0E, 0x1C)
LETTER_TOP = (0xF6, 0xF0, 0xFF)

ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
# Loose PNGs for the window icon and the title bar, so a small mark is drawn from a small file
# instead of being shrunk from the 512 px one (where the letters come out faint).
PNG_SIZES = [16, 24, 32, 48]


def font(name, px):
    return ImageFont.truetype(os.path.join(FONTS, name), px)


def vertical(w, h, top, bottom):
    """A top-to-bottom gradient, w x h, opaque."""
    strip = Image.new("RGB", (1, 256))
    for y in range(256):
        t = y / 255
        strip.putpixel((0, y), tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    return strip.resize((w, h), Image.BILINEAR).convert("RGBA")


def mark(size):
    """The MV tile at size x size. Drawn at a multiple and reduced, so the edges are smooth.
    Small sizes get heavier letters, or Cinzel's hairlines vanish."""
    k = 8 if size <= 64 else 4
    s = size * k
    inset = round(s * 0.04)
    radius = round(s * 0.22)
    box = (inset, inset, s - inset - 1, s - inset - 1)

    shape = Image.new("L", (s, s), 0)
    ImageDraw.Draw(shape).rounded_rectangle(box, radius, fill=255)

    tile = vertical(s, s, TILE_TOP, TILE_BOTTOM)
    glow = Image.new("L", (s, s), 0)
    ImageDraw.Draw(glow).ellipse((s * 0.16, s * 0.18, s * 0.84, s * 0.82), fill=100)
    glow = glow.filter(ImageFilter.GaussianBlur(s * 0.12))
    tile = Image.composite(Image.new("RGBA", (s, s), ACCENT + (255,)), tile, glow)

    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.paste(tile, (0, 0), shape)

    # The rim, half transparent, so the tile holds its edge on a light and on a dark ground.
    rim = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(rim).rounded_rectangle(box, radius, outline=ACCENT + (170,),
                                          width=max(k, round(s * 0.022)))
    out = Image.alpha_composite(out, rim)

    heavy = round(s * (0.022 if size < 48 else 0.009))
    # At 16 and 24 px the letters take more of the tile; there every pixel of them counts.
    wide, tall = (0.80, 0.50) if size <= 24 else (0.70, 0.42)
    ref = font("Cinzel-Regular.ttf", 1000)
    l, t, r, b = ref.getbbox("MV", stroke_width=round(heavy * 1000 / s))
    f = font("Cinzel-Regular.ttf", round(1000 * min(wide * s / (r - l), tall * s / (b - t))))
    l, t, r, b = f.getbbox("MV", stroke_width=heavy)
    letters = Image.new("L", (s, s), 0)
    ImageDraw.Draw(letters).text(((s - (r - l)) / 2 - l, (s - (b - t)) / 2 - t + s * 0.01), "MV",
                                 font=f, fill=255, stroke_width=heavy, stroke_fill=255)
    out = Image.composite(vertical(s, s, LETTER_TOP, ACCENT_TEXT), out, letters)

    return out.resize((size, size), Image.LANCZOS)


def spaced(draw, centre_x, top, text, f, fill, spacing):
    """Text with letter spacing, centred on centre_x. Pillow has no tracking of its own."""
    widths = [f.getlength(c) for c in text]
    x = centre_x - (sum(widths) + spacing * (len(text) - 1)) / 2
    for c, w in zip(text, widths):
        draw.text((x, top), c, font=f, fill=fill)
        x += w + spacing


def splash():
    # 480 wide like the strip below it is written for (LoadingSplash measures its text against
    # the poster's width). The border stays one flat colour: when the window is wider than the
    # poster, LoadingSplash fills the rest with the poster's corner pixel.
    w, h, k = 480, 360, 2
    W, H = w * k, h * k
    img = Image.new("RGBA", (W, H), BG_CHROME + (255,))
    glow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(glow).ellipse((W * 0.26, H * 0.08, W * 0.74, H * 0.60), fill=80)
    glow = glow.filter(ImageFilter.GaussianBlur(W * 0.07))
    img = Image.composite(Image.new("RGBA", (W, H), ACCENT + (255,)), img, glow)

    tile = mark(132 * k)
    img.alpha_composite(tile, ((W - tile.width) // 2, round(38 * k)))

    d = ImageDraw.Draw(img)
    spaced(d, W / 2, 196 * k, "MVIEWER", font("Cinzel-Regular.ttf", 40 * k), WORDMARK, 7 * k)
    tag = "Model-Viewer für World of Warcraft"
    tf = font("Inter-Regular.ttf", 15 * k)
    d.text(((W - tf.getlength(tag)) / 2, 254 * k), tag, font=tf, fill=FG_MUTED)
    cf = font("Inter-Regular.ttf", 11 * k)
    d.text((16 * k, H - 28 * k), "by Louis Bunt", font=cf, fill=FG_MUTED)

    img.resize((w, h), Image.LANCZOS).convert("RGB").save(os.path.join(RES, "splash.png"),
                                                          optimize=True)


def main():
    mark(512).save(os.path.join(RES, "appicon.png"), optimize=True)
    for size in PNG_SIZES:
        mark(size).save(os.path.join(RES, "appicon-%d.png" % size), optimize=True)
    icons = [mark(size) for size in ICO_SIZES]
    icons[-1].save(os.path.join(RES, "appicon.ico"), format="ICO",
                   sizes=[(i.width, i.height) for i in icons], append_images=icons[:-1])
    splash()


if __name__ == "__main__":
    main()
