#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare a process-local Liberation Sans trial with the default Canvas font."""

from __future__ import annotations

import argparse
import csv
import re
import statistics
from pathlib import Path

from PIL import Image, ImageChops, ImageStat

from compare_p2c_visuals import P2C2_CHANGED_PERCENT, PAGES, canonical_locale, parse_linux_text_metrics, sha256, windows_text_part

WINDOW_RE = re.compile(
    r"settings geometry outer=(\d+)x(\d+) client=(\d+)x(\d+).*?client_origin_in_window=(\d+),(\d+)"
)

PANGO_FONT_RE = re.compile(
    r'CI_GTK_FONT source=GtkLabelStyle family="([^"]+)" resolved="([^"]+)" base="([^"]+)" '
    r"canvas_size_pt=([\d.]+) weight=(\d+) ascent_px=([\d.]+) descent_px=([\d.]+)"
)


def pango_font_metrics(log_path: Path) -> tuple[str, str, str, float, int, float, float]:
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = PANGO_FONT_RE.search(line)
        if match:
            family, resolved, base, size, weight, ascent, descent = match.groups()
            return family, resolved, base, float(size), int(weight), float(ascent), float(descent)
    raise ValueError(f"Missing Pango font metrics in {log_path}")


def crop_windows_client(path: Path, origin: tuple[int, int]) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGB")
    x, y = origin
    crop = image.crop((x, y, x + 558, y + 335))
    if crop.size != (558, 335):
        raise ValueError(f"Windows settings client crop failed for {path.name}: {crop.size}")
    return crop


def changed_percent(reference: Image.Image, actual: Image.Image) -> tuple[int, float, float]:
    difference = ImageChops.difference(reference, actual)
    changed = sum(max(pixel) != 0 for pixel in difference.getdata())
    percent = changed * 100.0 / (reference.width * reference.height)
    mean_absolute_error = sum(ImageStat.Stat(difference).mean) / 3.0
    return changed, percent, mean_absolute_error


def heatmap(diff: Image.Image) -> Image.Image:
    gray = diff.convert("L")
    return Image.merge("RGB", (gray, Image.new("L", gray.size), Image.new("L", gray.size)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--windows-dir", type=Path, required=True)
    parser.add_argument("--default-linux-dir", type=Path, required=True)
    parser.add_argument("--candidate-linux-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    environment = (args.candidate_linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    if "P2C_font_candidate=liberation" not in environment or "Linux_font_match_en=Liberation Sans" not in environment:
        raise ValueError("The process-local Liberation Sans fontconfig candidate was not resolved")
    candidate_log = (args.candidate_linux_dir / "p2c-client.log").read_text(encoding="utf-8", errors="replace")
    if 'CI_GTK_FONT source=GtkLabelStyle' not in candidate_log or 'resolved="Liberation Sans"' not in candidate_log:
        raise ValueError("GTK/Pango did not report the expected candidate family")
    default_pango = pango_font_metrics(args.default_linux_dir / "p2c-client.log")
    candidate_pango = pango_font_metrics(args.candidate_linux_dir / "p2c-client.log")
    windows_result = (args.windows_dir / "result.txt").read_text(encoding="utf-8-sig", errors="replace")
    window_geometry = WINDOW_RE.search(windows_result)
    if window_geometry is None:
        raise ValueError("Windows result.txt lacks measured settings client geometry")
    outer_w, outer_h, client_w, client_h, origin_x, origin_y = map(int, window_geometry.groups())
    if (outer_w, outer_h, client_w, client_h) != (574, 374, 558, 335):
        raise ValueError(f"Unexpected Windows client geometry: {outer_w}x{outer_h}/{client_w}x{client_h}")
    default_environment = (args.default_linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    default_zh_fallback = next((line.split("=", 1)[1] for line in default_environment.splitlines() if line.startswith("Linux_font_match_zh=")), "NOT_RECORDED")

    windows_rows = {}
    with (args.windows_dir / "windows-control-font-metrics.csv").open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            key = (int(row["page_idd"]), canonical_locale(row["locale"]), int(row["resource_id"]), windows_text_part(row))
            windows_rows.setdefault(key, row)
    default_rows = parse_linux_text_metrics(args.default_linux_dir / "p2c-client.log")
    candidate_rows = parse_linux_text_metrics(args.candidate_linux_dir / "p2c-client.log")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    details = []
    groups: dict[tuple[str, str], list[dict[str, float]]] = {}
    skipped_divergences = 0
    for key in sorted(set(windows_rows) & set(default_rows) & set(candidate_rows)):
        win = windows_rows[key]
        base = default_rows[key]
        candidate = candidate_rows[key]
        if win["sample_text"] != base["text"] or win["sample_text"] != candidate["text"]:
            skipped_divergences += 1
            continue
        win_width = float(win["text_width_px"])
        base_width = float(base["width"])
        candidate_width = float(candidate["width"])
        win_height = float(win["text_height_px"])
        base_height = float(base["height"])
        candidate_height = float(candidate["height"])
        base_delta = abs(base_width - win_width)
        candidate_delta = abs(candidate_width - win_width)
        base_height_delta = abs(base_height - win_height)
        candidate_height_delta = abs(candidate_height - win_height)
        class_name = win["class"]
        locale = key[1]
        groups.setdefault((locale, class_name), []).append({
            "default_width": base_delta, "candidate_width": candidate_delta,
            "default_height": base_height_delta, "candidate_height": candidate_height_delta,
        })
        verdict = "IMPROVED" if candidate_delta < base_delta and candidate_height_delta <= base_height_delta else \
            "WIDTH_IMPROVED_HEIGHT_REGRESSED" if candidate_delta < base_delta and candidate_height_delta > base_height_delta else \
            "REGRESSED" if candidate_delta > base_delta or candidate_height_delta > base_height_delta else "UNCHANGED"
        details.append((
            key[0], locale, key[2], key[3], class_name, win["sample_text"], len(win["sample_text"]),
            win["logical_face_name"], win["gdi_reported_face_name"], win["font_substitution"],
            win["point_size"], win["logfont_weight"], win["textmetric_weight"], win["text_ascent_px"],
            win["text_descent_px"], win["average_char_width_px"], win_width, win_height,
            default_pango[0], default_pango[1], default_pango[3], default_pango[4], default_pango[5], default_pango[6],
            base_width, base_height, base_delta, base_height_delta,
            candidate_pango[0], candidate_pango[1], candidate_pango[3], candidate_pango[4], candidate_pango[5], candidate_pango[6],
            candidate_width, candidate_height, candidate_delta, candidate_height_delta,
            base["x"], base["y"], candidate["x"], candidate["y"],
            round(float(candidate["y"]) - float(base["y"]), 2), verdict,
        ))

    with (args.output_dir / "font-candidate-metrics.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow((
            "page_idd", "locale", "resource_id", "part", "win32_class", "text", "character_count",
            "windows_logical_face", "windows_gdi_reported_face", "windows_font_substitution", "windows_point_size",
            "windows_logfont_weight", "windows_textmetric_weight", "windows_gdi_ascent_px", "windows_gdi_descent_px",
            "windows_average_char_width_px", "windows_gdi_text_width_px", "windows_gdi_text_height_px",
            "default_pango_family", "default_pango_resolved_family", "default_pango_canvas_size_pt", "default_pango_weight",
            "default_pango_ascent_px", "default_pango_descent_px", "default_canvas_width_px", "default_canvas_height_px",
            "default_abs_width_delta_px", "default_abs_height_delta_px", "candidate_pango_family", "candidate_pango_resolved_family",
            "candidate_pango_canvas_size_pt", "candidate_pango_weight", "candidate_pango_ascent_px", "candidate_pango_descent_px",
            "liberation_candidate_width_px", "liberation_candidate_height_px", "candidate_abs_width_delta_px",
            "candidate_abs_height_delta_px", "default_canvas_text_origin_x_px", "default_canvas_text_origin_y_px",
            "candidate_canvas_text_origin_x_px", "candidate_canvas_text_origin_y_px", "candidate_minus_default_origin_y_px", "verdict",
        ))
        writer.writerows(details)

    summary = []
    for (locale, class_name), values in sorted(groups.items()):
        base_width = [item["default_width"] for item in values]
        candidate_width = [item["candidate_width"] for item in values]
        base_height = [item["default_height"] for item in values]
        candidate_height = [item["candidate_height"] for item in values]
        summary.append((locale, class_name, len(values),
                       f"{statistics.median(base_width):.2f}", f"{statistics.median(candidate_width):.2f}",
                       f"{statistics.median(base_height):.2f}", f"{statistics.median(candidate_height):.2f}",
                       sum(item["default_width"] <= 2 and item["default_height"] <= 2 for item in values),
                       sum(item["candidate_width"] <= 2 and item["candidate_height"] <= 2 for item in values),
                       sum(item["candidate_width"] < item["default_width"] and item["candidate_height"] <= item["default_height"] for item in values),
                       sum(item["candidate_width"] > item["default_width"] or item["candidate_height"] > item["default_height"] for item in values)))
    with (args.output_dir / "font-candidate-summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("locale", "win32_class", "same_source_samples", "default_median_abs_width_delta_px", "candidate_median_abs_width_delta_px", "default_median_abs_height_delta_px", "candidate_median_abs_height_delta_px", "default_within_2px_width_and_height", "candidate_within_2px_width_and_height", "improved", "regressed"))
        writer.writerows(summary)

    with (args.output_dir / "font-candidate-font-metrics.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        native_ascent = statistics.median(float(row["text_ascent_px"]) for row in windows_rows.values())
        native_descent = statistics.median(float(row["text_descent_px"]) for row in windows_rows.values())
        writer.writerow(("trial", "family", "resolved_family", "base_font", "canvas_size_pt", "weight",
                         "windows_gdi_median_ascent_px", "windows_gdi_median_descent_px", "pango_ascent_px", "pango_descent_px",
                         "pango_minus_gdi_ascent_px", "pango_minus_gdi_descent_px", "zh_fontconfig_fallback"))
        for trial, metrics in (("default", default_pango), ("liberation", candidate_pango)):
            writer.writerow((trial, *metrics[:5], f"{native_ascent:.2f}", f"{native_descent:.2f}", metrics[5], metrics[6],
                             f"{metrics[5] - native_ascent:.2f}", f"{metrics[6] - native_descent:.2f}", default_zh_fallback))

    visual_rows = []
    visual_dir = args.output_dir / "font-candidate-visual"
    visual_dir.mkdir(exist_ok=True)
    for page_index in range(5):
        for locale in ("en", "zh"):
            name = f"production-settings-{page_index}-{locale}.png"
            windows_locale = "English" if locale == "en" else "zh-CN"
            windows_path = args.windows_dir / f"windows-native-settings-{page_index}-{windows_locale}.png"
            base_path = args.default_linux_dir / name
            candidate_path = args.candidate_linux_dir / name
            if not windows_path.is_file():
                raise FileNotFoundError(f"Missing native light screenshot: {windows_path.name}")
            windows = crop_windows_client(windows_path, (origin_x, origin_y))
            with Image.open(base_path) as source:
                base = source.convert("RGB")
            with Image.open(candidate_path) as source:
                candidate = source.convert("RGB")
            if base.size != (558, 335) or candidate.size != base.size:
                raise ValueError(f"Unexpected candidate screenshot size: {name}")
            changed, percent, _ = changed_percent(base, candidate)
            default_win_changed, default_win_percent, default_win_mae = changed_percent(windows, base)
            candidate_win_changed, candidate_win_percent, candidate_win_mae = changed_percent(windows, candidate)
            base_name = f"{page_index}-{locale}"
            overlay = Image.blend(base, candidate, 0.5)
            side = Image.new("RGB", (1116, 335))
            side.paste(base, (0, 0))
            side.paste(candidate, (558, 0))
            overlay.save(visual_dir / f"{base_name}-default-candidate-overlay-50.png")
            ImageChops.difference(base, candidate).save(visual_dir / f"{base_name}-default-candidate-difference.png")
            side.save(visual_dir / f"{base_name}-side-by-side.png")
            default_win_diff = ImageChops.difference(windows, base)
            candidate_win_diff = ImageChops.difference(windows, candidate)
            Image.blend(windows, base, 0.5).save(visual_dir / f"{base_name}-default-vs-windows-overlay-50.png")
            Image.blend(windows, candidate, 0.5).save(visual_dir / f"{base_name}-candidate-vs-windows-overlay-50.png")
            default_win_diff.save(visual_dir / f"{base_name}-default-vs-windows-difference.png")
            candidate_win_diff.save(visual_dir / f"{base_name}-candidate-vs-windows-difference.png")
            heatmap(candidate_win_diff).save(visual_dir / f"{base_name}-candidate-vs-windows-heatmap.png")
            historical_page = PAGES[page_index]
            historical_percent = P2C2_CHANGED_PERCENT[(historical_page, locale)]
            visual_rows.append((page_index, locale, changed, f"{percent:.4f}",
                                default_win_changed, f"{default_win_percent:.4f}", f"{default_win_mae:.4f}",
                                candidate_win_changed, f"{candidate_win_percent:.4f}", f"{candidate_win_mae:.4f}",
                                f"{historical_percent:.4f}", f"{candidate_win_percent - historical_percent:.4f}",
                                sha256(windows_path), sha256(base_path), sha256(candidate_path),
                                "CANDIDATE_COMPARE_ONLY; P2C2_RATE_IS_HISTORICAL_CONTEXT; NOT A VISUAL_PASS"))
    with (args.output_dir / "font-candidate-visual-metrics.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page_idd", "locale", "candidate_vs_default_changed_pixels", "candidate_vs_default_changed_percent",
                         "default_vs_current_windows_changed_pixels", "default_vs_current_windows_changed_percent", "default_vs_current_windows_mae",
                         "candidate_vs_current_windows_changed_pixels", "candidate_vs_current_windows_changed_percent", "candidate_vs_current_windows_mae",
                         "p2c2_historical_changed_percent", "candidate_minus_p2c2_percentage_points_descriptive_only",
                         "windows_png_sha256", "default_png_sha256", "candidate_png_sha256", "verdict"))
        writer.writerows(visual_rows)

    all_pairs = [item for values in groups.values() for item in values]
    if not all_pairs:
        raise ValueError("No same-source Windows/Linux text measurements could be paired")
    base_median = statistics.median(item["default_width"] for item in all_pairs)
    candidate_median = statistics.median(item["candidate_width"] for item in all_pairs)
    base_height_median = statistics.median(item["default_height"] for item in all_pairs)
    candidate_height_median = statistics.median(item["candidate_height"] for item in all_pairs)
    print(f"Font samples paired={len(details)} source-language-divergences-skipped={skipped_divergences}")
    print(f"Median absolute width delta: default={base_median:.2f}px candidate={candidate_median:.2f}px")
    print(f"Median absolute height delta: default={base_height_median:.2f}px candidate={candidate_height_median:.2f}px")
    print(f"Samples within 2 px width+height: default={sum(item['default_width'] <= 2 and item['default_height'] <= 2 for item in all_pairs)}/{len(all_pairs)} candidate={sum(item['candidate_width'] <= 2 and item['candidate_height'] <= 2 for item in all_pairs)}/{len(all_pairs)}")
    print(f"Pango default family={default_pango[1]} ascent/descent={default_pango[5]:.2f}/{default_pango[6]:.2f}px; candidate={candidate_pango[1]} ascent/descent={candidate_pango[5]:.2f}/{candidate_pango[6]:.2f}px; zh fallback={default_zh_fallback}")
    for row in visual_rows:
        print(f"FONT_SCREEN page={PAGES[row[0]]} locale={row[1]} default_windows={row[5]}% candidate_windows={row[8]}% candidate_delta={float(row[8]) - float(row[5]):.4f}pp")
    print("Character-spacing differences are represented by measured GDI/Pango string widths and average GDI character width; native character tracking was not changed.")
    print("Font candidate remains isolated to this process; no production default is changed by this script.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
