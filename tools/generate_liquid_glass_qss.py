#!/usr/bin/env python3
"""Generate Qt stylesheet for the Liquid Glass design tokens."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

TOKEN_SOURCE = Path("design/tokens/liquid_glass.json")
OUTPUT_QSS = Path("resources/ui/liquid_glass.qss")


def rgba_from_hex(value: str) -> str:
    value = value.lstrip("#")
    if len(value) == 8:
        r, g, b, a = value[:2], value[2:4], value[4:6], value[6:8]
        alpha = int(a, 16) / 255
        return f"rgba({int(r, 16)}, {int(g, 16)}, {int(b, 16)}, {alpha:.2f})"
    if len(value) == 6:
        r, g, b = value[:2], value[2:4], value[4:6]
        return f"rgb({int(r, 16)}, {int(g, 16)}, {int(b, 16)})"
    raise ValueError(f"Unexpected color format: {value}")


def generate(tokens: dict) -> str:
    colors = tokens["colors"]
    spacing = tokens["spacing"]
    typography = tokens["typography"]

    def c(tok_light: str, tok_dark: str) -> str:
        return f"qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 {rgba_from_hex(colors[tok_light])}, stop:1 {rgba_from_hex(colors[tok_dark])})"

    qss = []
    qss.append("/* Generated from design/tokens/liquid_glass.json */")
    qss.append("/* Light mode defaults */")
    qss.append(
        "QWidget {\n"
        f"  background-color: {rgba_from_hex(colors['lg.surface.primary.light'])};\n"
        f"  color: {rgba_from_hex(colors['lg.text.primary.light'])};\n"
        f"  font-family: '{typography['lg.font.body']['family']}';\n"
        f"  font-size: {typography['lg.font.body']['size_pt']}pt;\n"
        "}"
    )
    qss.append(
        "QDockWidget::title {\n"
        f"  background-color: {rgba_from_hex(colors['lg.surface.secondary.light'])};\n"
        f"  padding: {spacing['lg.spacing.sm']}px;\n"
        f"  margin: 0px;\n"
        f"  font-family: '{typography['lg.font.display']['family']}';\n"
        f"  font-size: {typography['lg.font.display']['size_pt']}pt;\n"
        f"  font-weight: {typography['lg.font.display']['weight']};\n"
        "}"
    )
    qss.append(
        "QPushButton {\n"
        f"  background: {c('lg.accent.blue.light', 'lg.accent.blue.dark')};\n"
        f"  border-radius: {spacing['lg.corner.radius.button']}px;\n"
        f"  padding: {spacing['lg.spacing.sm']}px {spacing['lg.spacing.md']}px;\n"
        f"  color: {rgba_from_hex(colors['lg.surface.secondary.light'])};\n"
        "}"
    )
    qss.append(
        "QPushButton:disabled {\n"
        f"  background: {rgba_from_hex(colors['lg.surface.secondary.light'])};\n"
        f"  color: {rgba_from_hex(colors['lg.text.secondary.light'])};\n"
        "}"
    )
    qss.append(
        "QTreeView::item:selected {\n"
        f"  background: {c('lg.accent.blue.light', 'lg.accent.blue.dark')};\n"
        f"  color: {rgba_from_hex(colors['lg.surface.secondary.light'])};\n"
        "}"
    )

    qss.append("/* Dark mode overrides (to be toggled dynamically) */")
    qss.append(
        '[data-theme="dark"] QWidget {\n'
        f"  background-color: {rgba_from_hex(colors['lg.surface.primary.dark'])};\n"
        f"  color: {rgba_from_hex(colors['lg.text.primary.dark'])};\n"
        "}"
    )
    qss.append(
        '[data-theme="dark"] QDockWidget::title {\n'
        f"  background-color: {rgba_from_hex(colors['lg.surface.secondary.dark'])};\n"
        f"  color: {rgba_from_hex(colors['lg.text.primary.dark'])};\n"
        "}"
    )
    qss.append(
        '[data-theme="dark"] QPushButton {\n'
        f"  background: {c('lg.accent.blue.dark', 'lg.accent.blue.light')};\n"
        "}"
    )

    return "\n\n".join(qss) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Liquid Glass QSS file")
    parser.add_argument("--src", type=Path, default=TOKEN_SOURCE)
    parser.add_argument("--out", type=Path, default=OUTPUT_QSS)
    args = parser.parse_args()

    tokens = json.loads(args.src.read_text(encoding="utf-8"))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    qss = generate(tokens)
    args.out.write_text(qss, encoding="utf-8")
    print(f"Generated {args.out} from {args.src}")


if __name__ == "__main__":
    main()
