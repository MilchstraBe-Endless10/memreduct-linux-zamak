#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare isolated production dark screenshots with the Windows 3.5.2 captures."""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


PAGES = ("General", "Memory cleaning", "Appearance", "Tray icon", "Advanced")
LOCALES = {"en": "English", "zh": "zh-CN"}
WINDOW_RE = re.compile(
    r"settings geometry outer=(\d+)x(\d+) client=(\d+)x(\d+).*?client_origin_in_window=(\d+),(\d+)"
)
GEOMETRY_RE = re.compile(
    r"CI_SETTINGS_GEOMETRY page_idd=(\d+) locale=([^ ]+) state=([^ ]+) resource=(\d+) part=([^ ]+) "
    r"x=([-\d.]+) y=([-\d.]+) w=([-\d.]+) h=([-\d.]+) theme=(light|dark)"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def crop_windows(path: Path, origin: tuple[int, int]) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGB")
    x, y = origin
    crop = image.crop((x, y, x + 558, y + 335))
    if crop.size != (558, 335):
        raise ValueError(f"Windows client crop failed for {path.name}: {crop.size}")
    return crop


def heatmap(diff: Image.Image) -> Image.Image:
    gray = diff.convert("L")
    return Image.merge("RGB", (gray, Image.new("L", gray.size), Image.new("L", gray.size)))


def themed_geometry(log: str) -> dict[tuple[str, ...], tuple[float, ...]]:
    result = {}
    for match in GEOMETRY_RE.finditer(log):
        page, locale, state, resource, part, x, y, width, height, theme = match.groups()
        result[(page, locale, state, resource, part, theme)] = tuple(map(float, (x, y, width, height)))
    return result


def metrics(reference: Image.Image, actual: Image.Image) -> tuple[int, float, float, int]:
    difference = ImageChops.difference(reference, actual)
    changed = 0
    maximum = 0
    for pixel in difference.getdata():
        channel_max = max(pixel)
        changed += channel_max != 0
        maximum = max(maximum, channel_max)
    stat = ImageStat.Stat(difference)
    mae = sum(stat.mean) / 3.0
    return changed, changed * 100.0 / (reference.width * reference.height), mae, maximum


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--windows-dir", type=Path, required=True)
    parser.add_argument("--linux-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    windows_result = (args.windows_dir / "result.txt").read_text(encoding="utf-8-sig", errors="replace")
    linux_log = (args.linux_dir / "p2c-client.log").read_text(encoding="utf-8", errors="replace")
    linux_environment = (args.linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    if "IDM_USEDARKTHEME" not in windows_result or "theme=dark" not in windows_result:
        raise ValueError("Windows artifact does not prove that the application dark-theme command was used")
    if "CI_THEME dark=true" not in linux_log:
        raise ValueError("Linux production log does not prove the View-menu dark-mode transition")
    if "GTK_THEME=Adwaita:light" not in linux_environment:
        raise ValueError("Linux GTK theme source was not recorded independently of the app palette")
    geometry = themed_geometry(linux_log)
    geometry_pairs = 0
    paired_pages: set[tuple[str, str]] = set()
    for key in sorted({key[:-1] for key in geometry}):
        light = geometry.get((*key, "light"))
        dark = geometry.get((*key, "dark"))
        if light is None or dark is None:
            continue
        if light != dark:
            raise ValueError(f"App dark palette changed production geometry for {key}: {light} -> {dark}")
        geometry_pairs += 1
        paired_pages.add((key[0], key[1]))
    expected_pages = {(str(101 + page), locale) for page in range(5) for locale in ("en", "zh-CN")}
    if not expected_pages.issubset(paired_pages) or geometry_pairs < 70:
        raise ValueError(f"Insufficient paired light/dark production geometry: {geometry_pairs} records; pages={sorted(paired_pages)}")
    capture_log = (args.linux_dir / "p2c-captures.log").read_text(encoding="utf-8", errors="replace")
    page_markers = [
        f"CI_SETTINGS_PAGE idd={101 + index} page={page} locale={locale} controls="
        for locale, names in (("en", PAGES), ("zh-CN", ("常规", "内存清理", "外观", "托盘图标", "高级")))
        for index, page in enumerate(names)
    ]
    for marker in page_markers:
        if not any(marker in line and "theme=dark" in line for line in linux_log.splitlines()):
            raise ValueError(f"Missing production dark-state page log: {marker}")

    match = WINDOW_RE.search(windows_result)
    if match is None:
        raise ValueError("Windows result.txt lacks measured settings client geometry")
    outer_w, outer_h, client_w, client_h, origin_x, origin_y = map(int, match.groups())
    if (outer_w, outer_h, client_w, client_h) != (574, 374, 558, 335):
        raise ValueError(f"Unexpected Windows reference geometry: {outer_w}x{outer_h}/{client_w}x{client_h}")

    out = args.output_dir / "dark-theme"
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    manifest = []
    for page_index, page in enumerate(PAGES):
        for locale, windows_locale in LOCALES.items():
            linux_locale = "en" if locale == "en" else "zh"
            windows_dark_path = args.windows_dir / f"windows-native-settings-{page_index}-{windows_locale}-dark.png"
            windows_light_path = args.windows_dir / f"windows-native-settings-{page_index}-{windows_locale}.png"
            linux_dark_path = args.linux_dir / f"production-settings-{page_index}-{linux_locale}-dark.png"
            linux_light_path = args.linux_dir / f"production-settings-{page_index}-{linux_locale}.png"
            for path in (windows_dark_path, windows_light_path, linux_dark_path, linux_light_path):
                if not path.is_file():
                    raise FileNotFoundError(f"Missing paired screenshot: {path.name}")
            if f"theme=dark screenshot={windows_dark_path.name}" not in windows_result:
                raise ValueError(f"Windows runner did not record a dark-mode capture command for {windows_dark_path.name}")
            capture_locale = "en" if locale == "en" else "zh-CN"
            if f"theme=dark locale={capture_locale} screenshot={linux_dark_path.name}" not in capture_log:
                raise ValueError(f"Linux capture log does not associate dark state with {linux_dark_path.name}")

            win_dark = crop_windows(windows_dark_path, (origin_x, origin_y))
            win_light = crop_windows(windows_light_path, (origin_x, origin_y))
            with Image.open(linux_dark_path) as source:
                linux_dark = source.convert("RGB")
            with Image.open(linux_light_path) as source:
                linux_light = source.convert("RGB")
            if linux_dark.size != (558, 335) or linux_light.size != (558, 335):
                raise ValueError(f"Linux Canvas screenshot has unexpected dimensions on {page}/{locale}")

            _, win_theme_changed, _, _ = metrics(win_light, win_dark)
            _, linux_theme_changed, _, _ = metrics(linux_light, linux_dark)
            if win_theme_changed <= 1.0:
                raise ValueError(f"Windows dark-mode screenshot has no material rendered change on {page}/{locale}: {win_theme_changed:.4f}%")
            if linux_theme_changed <= 1.0:
                raise ValueError(f"Linux dark-mode screenshot has no material rendered change on {page}/{locale}: {linux_theme_changed:.4f}%")

            diff = ImageChops.difference(win_dark, linux_dark)
            stem = f"{page_index}-{linux_locale}"
            side = Image.new("RGB", (1116, 335))
            side.paste(win_dark, (0, 0))
            side.paste(linux_dark, (558, 0))
            overlay = Image.blend(win_dark, linux_dark, 0.5)
            paths = {
                f"dark-{stem}-windows-crop.png": win_dark,
                f"dark-{stem}-side-by-side.png": side,
                f"dark-{stem}-overlay-50.png": overlay,
                f"dark-{stem}-difference.png": diff,
                f"dark-{stem}-heatmap.png": heatmap(diff),
            }
            for name, image in paths.items():
                path = out / name
                image.save(path)
                manifest.append((name, sha256(path)))

            changed, percent, mae, maximum = metrics(win_dark, linux_dark)
            rows.append((page, locale, 558, 335, changed, f"{percent:.4f}", f"{mae:.4f}", maximum,
                         f"{win_theme_changed:.4f}", f"{linux_theme_changed:.4f}",
                         sha256(windows_dark_path), sha256(linux_dark_path),
                         "CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS"))
            manifest.extend(((windows_dark_path.name, sha256(windows_dark_path)), (linux_dark_path.name, sha256(linux_dark_path))))

    with (out / "dark-theme-comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page", "locale", "width", "height", "changed_pixels", "changed_percent", "mean_absolute_error", "max_channel_delta", "windows_dark_vs_light_changed_percent", "linux_dark_vs_light_changed_percent", "windows_dark_png_sha256", "linux_dark_png_sha256", "verdict"))
        writer.writerows(rows)
    manifest_path = out / "dark-theme-sha256.txt"
    manifest_path.write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(set(manifest))), encoding="utf-8")
    if len(rows) != 10:
        raise AssertionError(f"Expected 10 dark screenshot pairs; got {len(rows)}")
    print("PASS dark production screenshots paired: 10/10; both app-dark transitions verified with source logs and canvas pixels.")
    print(f"PASS dark/light production geometry unchanged: {geometry_pairs} resource rectangles across all 10 page/locale pairs.")
    print("NOT PASS full-page pixel identity: metrics are descriptive; geometry and text/raster classes remain separate.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
