#!/usr/bin/env python3
"""Render git_view icon PNGs (matches git_view.svg)."""

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 512
MARGIN = 32
RADIUS = 96

X_LEFT = 136
X_RIGHT = 376
Y_TOP = 136
Y_MERGE = 288
Y_BOTTOM = 416

BLUE = (53, 122, 189, 255)
GREEN = (32, 153, 107, 255)
GRAY = (187, 187, 187, 255)
ORANGE = (192, 90, 26, 255)
BG = (30, 30, 30, 255)
BORDER = (58, 58, 58, 255)

ICON_SIZES = (48, 128, 256, 512)


def render(size: int) -> Image.Image:
    scale = size / SIZE
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    def s(v: float) -> float:
        return v * scale

    draw.rounded_rectangle(
        [s(MARGIN), s(MARGIN), size - s(MARGIN), size - s(MARGIN)],
        radius=s(RADIUS),
        fill=BG,
        outline=BORDER,
        width=max(1, round(4 * scale)),
    )

    x_left, x_right = s(X_LEFT), s(X_RIGHT)
    y_top, y_merge, y_bottom = s(Y_TOP), s(Y_MERGE), s(Y_BOTTOM)
    line_w = max(2, round(10 * scale))

    def line(p1, p2):
        draw.line([p1, p2], fill=GRAY, width=line_w)

    line((x_left, y_top), (x_left, y_merge))
    line((x_right, y_top), (x_right, y_merge))
    line((x_left, y_merge), (x_right, y_merge))
    line((x_right, y_merge), (x_right, y_bottom))

    def node(center, radius, fill, outline=None, outline_w=0):
        cx, cy = center
        draw.ellipse(
            [cx - radius, cy - radius, cx + radius, cy + radius],
            fill=fill,
            outline=outline,
            width=outline_w,
        )

    node((x_left, y_top), s(26), BLUE)
    node((x_right, y_top), s(26), GREEN)
    node((x_left, y_merge), s(26), BLUE)
    node((x_right, y_merge), s(28), GREEN, ORANGE, max(1, round(5 * scale)))
    node((x_right, y_bottom), s(26), GREEN)
    return img


def main() -> None:
    root = Path(__file__).resolve().parent
    for icon_size in ICON_SIZES:
        target = root / f"git_view_{icon_size}.png"
        render(icon_size).save(target)
        print(f"Wrote {target}")
    render(512).save(root / "git_view.png")
    print(f"Wrote {root / 'git_view.png'}")


if __name__ == "__main__":
    main()
