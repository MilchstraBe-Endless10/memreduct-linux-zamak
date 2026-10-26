#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build exact-scale Windows/Linux settings comparisons and a Win32 geometry table."""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
from pathlib import Path

from PIL import Image, ImageChops


PAGES = ("general", "memory", "appearance", "tray", "advanced")
LOCALES = {"en": "English", "zh": "zh-CN"}
REGION_LABELS_EN = (
    "Working set", "System file cache", "Modified page list*", "Standby list*",
    "Standby list (without priority)", "Modified file cache", "Registry cache (win8.1+)",
    "Combine memory lists (win10+)",
)
REGION_LABELS_ZH = (
    "工作集", "系统文件缓存", "已修改页列表*", "备用列表*", "无优先级备用列表",
    "已修改文件缓存", "注册表缓存（Windows 8.1+）", "合并内存列表（Windows 10+）",
)
# P2C-2 exact-head baseline, from run 40296808973 comparison artifact
# 12638282898. Keep these as historical measurements; never use them as a
# substitute for the current native screenshot comparison.
P2C2_CHANGED_PERCENT = {
    ("general", "en"): 13.8234, ("general", "zh"): 12.2297,
    ("memory", "en"): 16.7341, ("memory", "zh"): 16.6041,
    ("appearance", "en"): 14.5183, ("appearance", "zh"): 13.8961,
    ("tray", "en"): 16.8042, ("tray", "zh"): 15.8268,
    ("advanced", "en"): 7.3921, ("advanced", "zh"): 6.8528,
}
P2C2_RUN_ID = "40296808973"
P2C2_COMPARISON_ARTIFACT_ID = "12638282898"
EXPECTED_ZH_SOURCE_DIVERGENCES = {
    *((102, "zh", 124, f"row-{index}") for index in range(8)),
    (105, "zh", 153, "label"),
    (105, "zh", 154, "label"),
}
EXPECTED_ZH_LOCAL_TEXT = {
    **{(102, "zh", 124, f"row-{index}"): REGION_LABELS_ZH[index] for index in range(8)},
    (105, "zh", 153, "label"): "允许自动清理备用列表和已修改页列表",
    (105, "zh", 154, "label"): "将清理结果写入调试日志",
}
EXPECTED_ZH_ENGLISH_TEXT = {
    **{(102, "zh", 124, f"row-{index}"): REGION_LABELS_EN[index] for index in range(8)},
    (105, "zh", 153, "label"): "Allow \"Standby lists\" and \"Modified page list\" cleanup on autoreduct",
    (105, "zh", 154, "label"): "Log cleaning results into a debug log",
}
NAV_SELECTED = (217, 217, 217)
NAV_GUIDE = (109, 109, 109)
WINDOW_RE = re.compile(
    r"settings geometry outer=(\d+)x(\d+) client=(\d+)x(\d+).*?client_origin_in_window=(\d+),(\d+)"
)
GEOMETRY_RE = re.compile(
    r"CI_SETTINGS_GEOMETRY page_idd=(\d+) locale=([^ ]+) state=([^ ]+) resource=(\d+) part=([^ ]+) "
    r"x=([-\d.]+) y=([-\d.]+) w=([-\d.]+) h=([-\d.]+)"
)
TEXT_METRIC_RE = re.compile(
    r"CI_SETTINGS_TEXT page_idd=(\d+) locale=([^ ]+) state=([^ ]+) resource=(\d+) part=([^ ]+) "
    r"x=([-\d.]+) y=([-\d.]+) size=([-\d.]+) bold=(true|false) width=([-\d.]+) height=([-\d.]+) text=(.*)$"
)
GTK_FONT_RE = re.compile(
    r'CI_GTK_FONT source=GtkLabelStyle family="([^"]+)" resolved="([^"]+)" base="([^"]+)" '
    r"canvas_size_pt=([-\d.]+) weight=(\d+) ascent_px=([-\d.]+) descent_px=([-\d.]+)"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def locate_windows_capture(root: Path, page: int, locale: str) -> Path:
    names = [
        f"windows-native-settings-{page}-{LOCALES[locale]}.png",
        f"windows-native-settings-{page}-{'en' if locale == 'en' else 'zh-CN'}.png",
    ]
    for name in names:
        path = root / name
        if path.is_file():
            return path
    raise FileNotFoundError(f"Missing Windows capture for page={page} locale={locale}")


def locate_linux_capture(root: Path, page: int, locale: str) -> Path:
    path = root / f"production-settings-{page}-{'en' if locale == 'en' else 'zh'}.png"
    if not path.is_file():
        raise FileNotFoundError(f"Missing Linux production capture: {path.name}")
    return path


def locate_windows_scroll_capture(root: Path, locale: str) -> Path:
    language = "English" if locale == "en" else "zh-CN"
    path = root / f"windows-native-regions-scroll-1-{language}.png"
    if not path.is_file():
        raise FileNotFoundError(f"Missing Windows IDC_REGIONS middle-scroll screenshot: {path.name}")
    return path


def locate_linux_scroll_capture(root: Path, locale: str) -> Path:
    language = "en" if locale == "en" else "zh"
    path = root / f"production-settings-memory-{language}-scroll-1.png"
    if not path.is_file():
        raise FileNotFoundError(f"Missing Linux IDC_REGIONS middle-scroll screenshot: {path.name}")
    return path


def metadata(result_file: Path) -> tuple[int, int, int, int, int, int]:
    text = result_file.read_text(encoding="utf-8-sig", errors="replace")
    match = WINDOW_RE.search(text)
    if not match:
        raise ValueError("Windows result.txt lacks measured client origin and dimensions")
    outer_w, outer_h, client_w, client_h, origin_x, origin_y = map(int, match.groups())
    if (client_w, client_h) != (558, 335):
        raise ValueError(f"Unexpected Windows client size {client_w}x{client_h}; expected 558x335")
    return outer_w, outer_h, client_w, client_h, origin_x, origin_y


def heatmap(diff: Image.Image) -> Image.Image:
    gray = diff.convert("L")
    return Image.merge("RGB", (gray, Image.new("L", gray.size), Image.new("L", gray.size)))


def pixel_metrics(reference: Image.Image, target: Image.Image) -> tuple[int, float, float, int]:
    diff = ImageChops.difference(reference, target)
    channels = diff.getdata()
    pixel_count = 0
    total = 0
    maximum = 0
    for red, green, blue in channels:
        channel_max = max(red, green, blue)
        if channel_max:
            pixel_count += 1
        total += red + green + blue
        maximum = max(maximum, channel_max)
    area = reference.width * reference.height
    return pixel_count, pixel_count * 100.0 / area, total / (area * 3), maximum


def canonical_locale(locale: str) -> str:
    if locale == "English":
        return "en"
    if locale in ("zh", "zh-CN"):
        return "zh"
    return locale


GeometryKey = tuple[int, str, str, int, str]
Geometry = tuple[float, float, float, float]


def parse_windows_geometry(path: Path) -> dict[GeometryKey, Geometry]:
    result = {}
    with path.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            locale = canonical_locale(row["locale"])
            key = (int(row["page_idd"]), locale, row["state"], int(row["resource_id"]), row["part"])
            result[key] = tuple(float(row[name]) for name in ("x", "y", "width", "height"))
    if not result:
        raise ValueError("Windows geometry export is empty")
    return result


def parse_linux_geometry(path: Path) -> dict[GeometryKey, Geometry]:
    result = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = GEOMETRY_RE.search(line)
        if not match:
            continue
        page, locale, state, resource, part, x, y, width, height = match.groups()
        locale = canonical_locale(locale)
        # IDC_NAV is one native Win32 TreeView resource. Its text rows are
        # painted content, not child HWND controls with independent resource IDs;
        # compare the IDC_NAV control rectangle here and assess row/highlight
        # rendering through the unscaled screenshot diff and XTEST clicks.
        if int(resource) == 104 and part.startswith("item-"):
            continue
        key = (int(page), locale, state, int(resource), part)
        # Keep the first rendering: later list-scroll captures intentionally move visible rows.
        result.setdefault(key, tuple(map(float, (x, y, width, height))))
    if not result:
        raise ValueError("Linux production log contains no resource geometry markers")
    return result


def validate_linux_region_rows(geometry: dict[GeometryKey, Geometry]) -> None:
    for locale in LOCALES:
        list_rect = geometry.get((102, locale, "default", 124, "control"))
        if list_rect is None:
            raise ValueError(f"Missing Linux IDC_REGIONS list rectangle for locale={locale}")
        for state, expected in (("scroll-0", range(0, 6)), ("scroll-1", range(1, 7)), ("scroll-2", range(2, 8))):
            rows = {
                key[4]: rect
                for key, rect in geometry.items()
                if key[0] == 102 and key[1] == locale and key[2] == state and key[3] == 124 and key[4].startswith("row-")
            }
            expected_names = {f"row-{index}" for index in expected}
            if set(rows) != expected_names:
                raise ValueError(f"IDC_REGIONS visible rows mismatch for {locale}/{state}: {sorted(rows)}")
            lx, ly, lw, lh = list_rect
            for name, (x, y, width, height) in rows.items():
                if x < lx or y < ly or x + width > lx + lw or y + height > ly + lh:
                    raise ValueError(f"Linux {locale}/{state}/{name} exceeds IDC_REGIONS bounds")


def validate_windows_region_rows(geometry: dict[GeometryKey, Geometry]) -> None:
    for locale in LOCALES:
        for state, expected in (("scroll-0", range(0, 6)), ("scroll-1", range(1, 7)), ("scroll-2", range(2, 8))):
            rows = {
                key[4]
                for key in geometry
                if key[0] == 102 and key[1] == locale and key[2] == state and key[3] == 124 and key[4].startswith("row-")
            }
            expected_names = {f"row-{index}" for index in expected}
            if rows != expected_names:
                raise ValueError(f"Windows IDC_REGIONS visible rows mismatch for {locale}/{state}: {sorted(rows)}")


def text_ink_bounds(image: Image.Image, rect: Geometry, label_inset: int = 22) -> tuple[int, int, int, int] | None:
    x, y, width, height = rect
    left = max(0, round(x + label_inset))
    top = max(0, round(y))
    right = min(image.width, round(x + width))
    bottom = min(image.height, round(y + height))
    points = [
        (px, py)
        for py in range(top, bottom)
        for px in range(left, right)
        if max(image.getpixel((px, py))) < 220
    ]
    if not points:
        return None
    ink_left = min(point[0] for point in points)
    ink_top = min(point[1] for point in points)
    return (
        ink_left,
        ink_top,
        max(point[0] for point in points) + 1 - ink_left,
        max(point[1] for point in points) + 1 - ink_top,
    )


def validate_appearance_swatches(windows_dir: Path, linux_dir: Path, windows: dict[GeometryKey, Geometry], linux: dict[GeometryKey, Geometry]) -> None:
    expected = ((255, 255, 255), (0, 128, 64), (255, 128, 64), (236, 28, 36))
    for locale in LOCALES:
        windows_path = locate_windows_capture(windows_dir, 2, locale)
        linux_path = locate_linux_capture(linux_dir, 2, locale)
        with Image.open(windows_path) as image:
            origin = metadata(windows_dir / "result.txt")[4:6]
            windows_client = image.convert("RGB").crop((origin[0], origin[1], origin[0] + 558, origin[1] + 335))
            for index, color in enumerate(expected):
                key = (103, locale, "default", 140, f"row-{index}")
                rect = windows.get(key)
                if rect is None:
                    raise ValueError(f"Windows color swatch geometry missing: {key}")
                x, y, width, height = rect
                point = (int(x + width / 2), int(y + height / 2))
                if windows_client.getpixel(point) != color:
                    raise ValueError(f"Windows IDC_COLORS row {index} did not render source default {color} at {point}")
        with Image.open(linux_path) as image:
            canvas = image.convert("RGB")
            for index, color in enumerate(expected):
                key = (103, locale, "default", 140, f"row-{index}")
                rect = linux.get(key)
                if rect is None:
                    raise ValueError(f"Linux color swatch geometry missing: {key}")
                x, y, width, height = rect
                point = (int(x + width / 2), int(y + height / 2))
                if canvas.getpixel(point) != color:
                    raise ValueError(f"Linux IDC_COLORS row {index} did not match source default {color} at {point}")


def dark_pixel_bounds(image: Image.Image, region: tuple[int, int, int, int]) -> tuple[int, int, int, int] | None:
    left, top, right, bottom = region
    points = []
    for y in range(top, bottom):
        for x in range(left, right):
            red, green, blue = image.getpixel((x, y))[:3]
            if max(red, green, blue) < 200:
                points.append((x, y))
    if not points:
        return None
    return (
        min(point[0] for point in points),
        min(point[1] for point in points),
        max(point[0] for point in points) + 1,
        max(point[1] for point in points) + 1,
    )


def validate_surface_and_checkbox_raster(windows_dir: Path, linux_dir: Path, output: Path) -> tuple[int, int]:
    """Check measured Windows page-face colors and checkbox outlines separately from text AA."""
    origin_x, origin_y = metadata(windows_dir / "result.txt")[4:6]
    surface_points = {
        "page_gap": (150, 20, (240, 240, 240)),
        "page_footer": (300, 290, (240, 240, 240)),
        "navigation": (20, 200, (255, 255, 255)),
        "IDC_GENERAL_FRAME_left_edge": (156, 20, (220, 220, 220)),
    }
    checkbox_regions = {
        "IDC_ALWAYSONTOP_CHK": (164, 29, 182, 47),
        # Stop before row 1 begins; otherwise its top border is mistaken for
        # extra checkbox height in the same ROI.
        "IDC_REGIONS_row_0": (171, 33, 190, 49),
    }
    passed = 0
    failed = 0
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("locale", "target", "windows", "linux", "dx", "dy", "dw", "dh", "status"))
        for locale in LOCALES:
            windows_images = {}
            linux_images = {}
            for page in (0, 1):
                name = "general" if page == 0 else "memory"
                with Image.open(locate_windows_capture(windows_dir, page, locale)) as source:
                    windows_images[name] = source.convert("RGB").crop(
                        (origin_x, origin_y, origin_x + 558, origin_y + 335)
                    )
                with Image.open(locate_linux_capture(linux_dir, page, locale)) as source:
                    linux_images[name] = source.convert("RGB")

            for target, (x, y, expected) in surface_points.items():
                reference = windows_images["general"].getpixel((x, y))
                actual = linux_images["general"].getpixel((x, y))
                status = "PASS" if reference == expected and actual == expected else "FAIL"
                writer.writerow((locale, target, reference, actual, 0, 0, 0, 0, status))
                if status == "PASS":
                    passed += 1
                else:
                    failed += 1

            for target, region in checkbox_regions.items():
                page_name = "general" if target == "IDC_ALWAYSONTOP_CHK" else "memory"
                reference = dark_pixel_bounds(windows_images[page_name], region)
                actual = dark_pixel_bounds(linux_images[page_name], region)
                if reference is None or actual is None:
                    writer.writerow((locale, target, reference, actual, "", "", "", "", "FAIL"))
                    failed += 1
                    continue
                delta = tuple(actual[index] - reference[index] for index in range(4))
                status = "PASS" if all(abs(value) <= 2 for value in delta) else "FAIL"
                writer.writerow((locale, target, reference, actual, *delta, status))
                if status == "PASS":
                    passed += 1
                else:
                    failed += 1
    return passed, failed


def exact_color_bounds(image: Image.Image, area: tuple[int, int, int, int], color: tuple[int, int, int]) -> tuple[int, int, int, int] | None:
    left, top, right, bottom = area
    points = [
        (x, y)
        for y in range(top, bottom)
        for x in range(left, right)
        if image.getpixel((x, y)) == color
    ]
    if not points:
        return None
    left = min(point[0] for point in points)
    top = min(point[1] for point in points)
    return (
        left,
        top,
        max(point[0] for point in points) + 1 - left,
        max(point[1] for point in points) + 1 - top,
    )


def validate_navigation_raster(windows_dir: Path, linux_dir: Path, output: Path) -> tuple[int, int, int, int]:
    """Compare native selected TreeView rows and dotted branch pixels to Xvfb output."""
    origin_x, origin_y = metadata(windows_dir / "result.txt")[4:6]
    passed = 0
    failed = 0
    guide_passed = 0
    guide_failed = 0
    native_baseline_widths = {
        "en": (45, 88, 66, 52, 56),
        "zh": (32, 56, 32, 56, 32),
    }
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page", "locale", "resource_id", "baseline_x", "baseline_y", "baseline_w", "baseline_h", "win_x", "win_y", "win_w", "win_h", "linux_x", "linux_y", "linux_w", "linux_h", "native_dx", "native_dy", "native_dw", "native_dh", "linux_dx", "linux_dy", "linux_dw", "linux_dh", "status"))
        for locale in LOCALES:
            for page, page_name in enumerate(PAGES):
                with Image.open(locate_windows_capture(windows_dir, page, locale)) as source:
                    windows = source.convert("RGB").crop(
                        (origin_x, origin_y, origin_x + 558, origin_y + 335)
                    )
                with Image.open(locate_linux_capture(linux_dir, page, locale)) as source:
                    linux = source.convert("RGB")

                row_top = 10 + page * 20
                native = exact_color_bounds(windows, (12, row_top, 144, row_top + 22), NAV_SELECTED)
                current = exact_color_bounds(linux, (12, row_top, 144, row_top + 22), NAV_SELECTED)
                baseline = (27, row_top + 2, native_baseline_widths[locale][page], 20)
                if native is None or current is None:
                    native_delta = ("", "", "", "")
                    linux_delta = ("", "", "", "")
                    status = "FAIL"
                else:
                    native_delta = tuple(native[index] - baseline[index] for index in range(4))
                    linux_delta = tuple(current[index] - native[index] for index in range(4))
                    status = "PASS" if all(abs(value) <= 2 for value in native_delta + linux_delta) else "FAIL"
                writer.writerow((page_name, locale, 104, *baseline, *(native or ("", "", "", "")), *(current or ("", "", "", "")), *native_delta, *linux_delta, status))
                if status == "PASS":
                    passed += 1
                else:
                    failed += 1

    guide_points = {(23, 22 + 2 * step) for step in range(41)}
    guide_points.update((x, 22 + 20 * page) for page in range(5) for x in (25, 27))
    for locale in LOCALES:
        with Image.open(locate_windows_capture(windows_dir, 0, locale)) as source:
            windows = source.convert("RGB").crop((origin_x, origin_y, origin_x + 558, origin_y + 335))
        with Image.open(locate_linux_capture(linux_dir, 0, locale)) as source:
            linux = source.convert("RGB")
        for point in sorted(guide_points):
            if windows.getpixel(point) == NAV_GUIDE and linux.getpixel(point) == NAV_GUIDE:
                guide_passed += 1
            else:
                guide_failed += 1
    return passed, failed, guide_passed, guide_failed


def write_scroll_comparison(windows_dir: Path, linux_dir: Path, output_dir: Path, client: tuple[int, int, int, int, int, int], manifest: list[tuple[str, str]]) -> tuple[int, int]:
    outer_w, outer_h, client_w, client_h, origin_x, origin_y = client
    output = output_dir / "scroll-state-metrics.csv"
    passed = 0
    failed = 0
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page", "locale", "state", "client_width", "client_height", "changed_pixels", "changed_percent", "mean_absolute_channel_difference", "windows_sha256", "linux_sha256", "verdict"))
        for locale in LOCALES:
            windows_path = locate_windows_scroll_capture(windows_dir, locale)
            linux_path = locate_linux_scroll_capture(linux_dir, locale)
            with Image.open(windows_path) as source:
                if source.size != (outer_w, outer_h):
                    raise ValueError(f"Unexpected Windows middle-scroll size: {source.size}")
                reference = source.convert("RGB").crop((origin_x, origin_y, origin_x + client_w, origin_y + client_h))
            with Image.open(linux_path) as source:
                actual = source.convert("RGB")
                if actual.size != (client_w, client_h):
                    raise ValueError(f"Unexpected Linux middle-scroll size: {actual.size}")

            locale_name = "en" if locale == "en" else "zh"
            stem = f"memory-scroll-1-{locale_name}"
            crops = output_dir / f"windows-client-{stem}.png"
            reference.save(crops)
            side = Image.new("RGB", (client_w * 2, client_h), "white")
            side.paste(reference, (0, 0)); side.paste(actual, (client_w, 0))
            side_path = output_dir / f"side-by-side-{stem}.png"
            side.save(side_path)
            overlay_path = output_dir / f"overlay-50-{stem}.png"
            Image.blend(reference, actual, 0.5).save(overlay_path)
            difference_path = output_dir / f"difference-{stem}.png"
            heatmap(ImageChops.difference(reference, actual)).save(difference_path)
            pixels, percent, mae, _ = pixel_metrics(reference, actual)
            writer.writerow(("Memory cleaning", locale, "scroll-1", client_w, client_h, pixels, f"{percent:.4f}", f"{mae:.4f}", sha256(windows_path), sha256(linux_path), "COMPARE_ONLY; NOT PIXEL PASS"))
            manifest.extend((item.name, sha256(item)) for item in (windows_path, linux_path, crops, side_path, overlay_path, difference_path))
            passed += 1
    manifest.append((output.name, sha256(output)))
    return passed, failed


def write_region_text_metrics(windows_dir: Path, linux_dir: Path, output_dir: Path, client: tuple[int, int, int, int, int, int], windows_geometry: dict[GeometryKey, Geometry], linux_geometry: dict[GeometryKey, Geometry], manifest: list[tuple[str, str]]) -> None:
    _, _, client_w, client_h, origin_x, origin_y = client
    windows_text = {}
    with (windows_dir / "windows-control-font-metrics.csv").open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            if row["resource_id"] == "124" and row["sample_kind"].startswith("IDC_REGIONS_row-"):
                index = int(row["sample_kind"].rsplit("-", 1)[1])
                windows_text[(canonical_locale(row["locale"]), index)] = (row["sample_text"], row.get("sample_text_source", ""))
    linux_text = parse_linux_text_metrics(linux_dir / "p2c-client.log")
    output = output_dir / "region-text-ink-metrics.csv"
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("locale", "state", "resource_id", "row", "windows_display_text", "windows_text_evidence", "linux_display_text", "windows_ink_x", "windows_ink_y", "windows_ink_w", "windows_ink_h", "linux_ink_x", "linux_ink_y", "linux_ink_w", "linux_ink_h", "dx", "dy", "dw", "dh", "interpretation"))
        for locale in LOCALES:
            windows_path = locate_windows_scroll_capture(windows_dir, locale)
            linux_path = locate_linux_scroll_capture(linux_dir, locale)
            with Image.open(windows_path) as source:
                windows_image = source.convert("RGB").crop((origin_x, origin_y, origin_x + client_w, origin_y + client_h))
            with Image.open(linux_path) as source:
                linux_image = source.convert("RGB")
            for index in range(1, 7):
                key = (102, locale, "scroll-1", 124, f"row-{index}")
                native_rect = windows_geometry.get(key)
                linux_rect = linux_geometry.get(key)
                if native_rect is None or linux_rect is None:
                    raise ValueError(f"Missing measured IDC_REGIONS text row geometry: {key}")
                if (locale, index) not in windows_text:
                    raise ValueError(f"Missing Windows native ListView text for IDC_REGIONS {locale}/row-{index}")
                linux_text_row = linux_text.get((102, locale, 124, f"row-{index}"))
                if linux_text_row is None:
                    raise ValueError(f"Missing Linux production text for IDC_REGIONS {locale}/row-{index}")
                native_ink = text_ink_bounds(windows_image, native_rect)
                linux_ink = text_ink_bounds(linux_image, linux_rect)
                if native_ink is None or linux_ink is None:
                    delta: tuple[int | str, ...] = ("", "", "", "")
                    interpretation = "INK_NOT_FOUND"
                else:
                    delta = tuple(linux_ink[part] - native_ink[part] for part in range(4))
                    interpretation = "MEASURED_TEXT_GEOMETRY; SOURCE_LANGUAGE_DIVERGENCE" if locale == "zh" else "MEASURED_TEXT_GEOMETRY; FONT_RASTER_DIFFERENCE_REMAINS"
                windows_label, text_evidence = windows_text[(locale, index)]
                writer.writerow((locale, "scroll-1", 124, index, windows_label, text_evidence, linux_text_row["text"], *(native_ink or ("", "", "", "")), *(linux_ink or ("", "", "", "")), *delta, interpretation))
    manifest.append((output.name, sha256(output)))


def write_font_resolution_report(windows_dir: Path, linux_dir: Path, output_dir: Path, manifest: list[tuple[str, str]]) -> None:
    source = windows_dir / "windows-control-font-metrics.csv"
    if not source.is_file():
        raise FileNotFoundError("Missing Win32 WM_GETFONT/LOGFONT metrics export")
    environment = (linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    linux_fonts = {
        "en": next((line.split("=", 1)[1] for line in environment.splitlines() if line.startswith("Linux_font_match_en=")), "NOT_RECORDED"),
        "zh": next((line.split("=", 1)[1] for line in environment.splitlines() if line.startswith("Linux_font_match_zh=")), "NOT_RECORDED"),
    }
    client_log = (linux_dir / "p2c-client.log").read_text(encoding="utf-8", errors="replace")
    gtk_match = next((GTK_FONT_RE.search(line) for line in client_log.splitlines() if "CI_GTK_FONT source=" in line), None)
    if gtk_match is None:
        raise ValueError("Missing actual GtkLabelStyle/Pango font metrics from the production Xvfb process")
    gtk_font = gtk_match.groups()
    settings_source = Path(__file__).resolve().parents[2] / "src/settings_ui.zm"
    painter_sizes = sorted(set(re.findall(r"canvas_text\(.*?,\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*(?:true|false)\s*\)", settings_source.read_text(encoding="utf-8"))))
    if not painter_sizes:
        raise ValueError("Could not find explicit Painter.text sizes in production settings source")
    output = output_dir / "font-resolution-comparison.csv"
    with source.open(newline="", encoding="utf-8-sig") as input_stream, output.open("w", newline="", encoding="utf-8") as output_stream:
        reader = csv.DictReader(input_stream)
        rows = list(reader)
        if len(rows) < 10:
            raise ValueError(f"Win32 font metrics export has only {len(rows)} rows; expected multiple native controls")
        writer = csv.writer(output_stream)
        writer.writerow(("page_idd", "locale", "resource_id", "class", "sample_kind", "sample_text", "windows_text_evidence", "windows_logical_face_name", "windows_resolved_face_name", "font_source", "windows_logfont_height_px", "windows_point_size", "windows_logfont_weight", "windows_textmetric_height_px", "windows_ascent_px", "windows_descent_px", "windows_textmetric_weight", "windows_text_width_px", "windows_text_height_px", "linux_gtk_style_family", "linux_gtk_resolved_family", "linux_gtk_base_font", "linux_canvas_size_pt", "linux_gtk_weight", "linux_pango_ascent_px", "linux_pango_descent_px", "linux_fontconfig_fallback_en", "linux_fontconfig_fallback_zh", "linux_painter_text_sizes", "family_selection_status"))
        for row in rows:
            locale = canonical_locale(row["locale"])
            writer.writerow((row["page_idd"], locale, row["resource_id"], row["class"], row["sample_kind"], row["sample_text"], row.get("sample_text_source", ""), row["logical_face_name"], row["resolved_face_name"], row.get("font_source", "WM_GETFONT"), row["logfont_height_px"], row["point_size"], row["logfont_weight"], row["textmetric_height_px"], row["text_ascent_px"], row["text_descent_px"], row["textmetric_weight"], row["text_width_px"], row["text_height_px"], *gtk_font, linux_fonts["en"], linux_fonts["zh"], ";".join(painter_sizes), "GTK system font; Painter.text has no family argument"))
    manifest.append((output.name, sha256(output)))


def parse_linux_text_metrics(path: Path) -> dict[tuple[int, str, int, str], dict[str, str]]:
    result = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = TEXT_METRIC_RE.search(line)
        if not match:
            continue
        page, locale, state, resource, part, x, y, size, bold, width, height, label = match.groups()
        key = (int(page), canonical_locale(locale), int(resource), part)
        result.setdefault(key, {
            "state": state, "x": x, "y": y, "size": size, "bold": bold,
            "width": width, "height": height, "text": label,
        })
    if not result:
        raise ValueError("Linux production log contains no CI_SETTINGS_TEXT metrics")
    return result


def windows_text_part(row: dict[str, str]) -> str:
    sample = row["sample_kind"]
    if sample.startswith(("IDC_REGIONS_", "IDC_COLORS_")):
        return sample.rsplit("_", 1)[1]
    resource = int(row["resource_id"])
    if resource in {107, 108, 109, 110, 111, 112, 113, 114, 115}:
        return "group-title"
    if row["class"] in {"Edit", "ComboBox", "SysComboBox32"}:
        return "value"
    return "label"


def write_text_metrics_comparison(windows_dir: Path, linux_dir: Path, output_dir: Path, manifest: list[tuple[str, str]]) -> tuple[int, int, int, int]:
    source = windows_dir / "windows-control-font-metrics.csv"
    linux = parse_linux_text_metrics(linux_dir / "p2c-client.log")
    native = {}
    with source.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            key = (int(row["page_idd"]), canonical_locale(row["locale"]), int(row["resource_id"]), windows_text_part(row))
            native.setdefault(key, row)
    linux = {key: row for key, row in linux.items() if key[2] not in {0, 104}}
    native = {key: row for key, row in native.items() if key[2] not in {0, 104}}

    environment = (linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    linux_fonts = {
        "en": next((line.split("=", 1)[1] for line in environment.splitlines() if line.startswith("Linux_font_match_en=")), "NOT_RECORDED"),
        "zh": next((line.split("=", 1)[1] for line in environment.splitlines() if line.startswith("Linux_font_match_zh=")), "NOT_RECORDED"),
    }
    client_log = (linux_dir / "p2c-client.log").read_text(encoding="utf-8", errors="replace")
    gtk_match = next((GTK_FONT_RE.search(line) for line in client_log.splitlines() if "CI_GTK_FONT source=" in line), None)
    if gtk_match is None:
        raise ValueError("Missing actual GTK font metrics in Linux production log")
    gtk_font = gtk_match.groups()
    output = output_dir / "text-metrics-diff.csv"
    exact_text = 0
    divergent = 0
    missing = 0
    unexpected_divergences = 0
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page_idd", "locale", "resource_id", "part", "windows_text", "windows_text_evidence", "linux_text", "windows_logical_face", "windows_resolved_face", "windows_font_source", "windows_logfont_height_px", "windows_point_size", "windows_weight", "windows_ascent_px", "windows_descent_px", "windows_text_width_px", "windows_text_height_px", "linux_gtk_style_family", "linux_gtk_resolved_family", "linux_gtk_base_font", "linux_canvas_size_pt", "linux_gtk_weight", "linux_pango_ascent_px", "linux_pango_descent_px", "linux_fontconfig_fallback", "linux_painter_size", "linux_bold", "linux_text_x", "linux_text_y", "linux_text_width_px", "linux_text_height_px", "delta_width_px", "delta_height_px", "verdict"))
        for key in sorted(set(native) | set(linux)):
            win = native.get(key)
            current = linux.get(key)
            if win is None or current is None:
                missing += 1
                verdict = "MISSING_WINDOWS_METRIC" if win is None else "MISSING_LINUX_METRIC"
                writer.writerow((*key, win["sample_text"] if win else "", win.get("sample_text_source", "") if win else "", current["text"] if current else "", win["logical_face_name"] if win else "", win["resolved_face_name"] if win else "", win.get("font_source", "") if win else "", win["logfont_height_px"] if win else "", win["point_size"] if win else "", win["logfont_weight"] if win else "", win["text_ascent_px"] if win else "", win["text_descent_px"] if win else "", *gtk_font, linux_fonts[key[1]] if current else "", current["size"] if current else "", current["bold"] if current else "", current["x"] if current else "", current["y"] if current else "", current["width"] if current else "", current["height"] if current else "", "", "", verdict))
                continue
            if win["sample_text"] != current["text"]:
                divergent += 1
                if key in EXPECTED_ZH_SOURCE_DIVERGENCES:
                    verdict = "UPSTREAM_TRANSLATION_GAP; PRODUCT_DECISION_REQUIRED"
                else:
                    unexpected_divergences += 1
                    verdict = "UNEXPECTED_SOURCE_TEXT_DIVERGENCE"
                delta_width = delta_height = ""
            else:
                exact_text += 1
                delta_width = round(float(current["width"]) - float(win["text_width_px"]), 2)
                delta_height = round(float(current["height"]) - float(win["text_height_px"]), 2)
                verdict = "TEXT_METRICS_WITHIN_2PX" if abs(delta_width) <= 2 and abs(delta_height) <= 2 else "TEXT_METRICS_DIFFER"
            writer.writerow((*key, win["sample_text"], win.get("sample_text_source", ""), current["text"], win["logical_face_name"], win["resolved_face_name"], win.get("font_source", "WM_GETFONT"), win["logfont_height_px"], win["point_size"], win["logfont_weight"], win["text_ascent_px"], win["text_descent_px"], win["text_width_px"], win["text_height_px"], *gtk_font, linux_fonts[key[1]], current["size"], current["bold"], current["x"], current["y"], current["width"], current["height"], delta_width, delta_height, verdict))
    manifest.append((output.name, sha256(output)))

    language_path = output_dir / "source-language-diff.csv"
    with language_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page_idd", "locale", "resource_id", "part", "upstream_english", "upstream_simplified_chinese_resource", "windows_simplified_chinese_actual", "windows_text_evidence", "linux_simplified_chinese_actual", "decision", "status"))
        for key in sorted(EXPECTED_ZH_SOURCE_DIVERGENCES):
            page_idd, locale, resource_id, part = key
            windows_row = native.get(key)
            linux_row = linux.get(key)
            actual_windows = windows_row["sample_text"] if windows_row else "MISSING"
            actual_linux = linux_row["text"] if linux_row else "MISSING"
            source_english = EXPECTED_ZH_ENGLISH_TEXT[key]
            local_translation = EXPECTED_ZH_LOCAL_TEXT[key]
            if not windows_row or not linux_row:
                status = "MISSING_CAPTURE_OR_METRIC"
            elif (actual_windows != source_english or actual_linux != local_translation
                  or windows_row.get("sample_text_source") != "Win32LVM_GETITEMTEXTW"):
                status = "UNEXPECTED_TEXT_DOES_NOT_MATCH_DOCUMENTED_MATRIX"
                unexpected_divergences += 1
            else:
                status = "EXPECTED_UPSTREAM_TRANSLATION_GAP"
            writer.writerow((page_idd, locale, resource_id, part, source_english, "No Simplified Chinese resource; upstream text is hard-coded English", actual_windows, windows_row.get("sample_text_source", "MISSING") if windows_row else "MISSING", actual_linux, "DECISION_REQUIRED; retain project Simplified Chinese translation pending product decision", status))

        for key in sorted(set(native) & set(linux)):
            if native[key]["sample_text"] == linux[key]["text"] or key in EXPECTED_ZH_SOURCE_DIVERGENCES:
                continue
            writer.writerow((*key, "", "", native[key]["sample_text"], native[key].get("sample_text_source", ""), linux[key]["text"], "No approved divergence", "UNEXPECTED_SOURCE_TEXT_DIVERGENCE"))
    manifest.append((language_path.name, sha256(language_path)))
    return exact_text, divergent, missing, unexpected_divergences


def write_visual_regression_table(metrics_path: Path, output_path: Path, manifest: list[tuple[str, str]]) -> tuple[int, int, int]:
    current = {}
    with metrics_path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            key = (row["page"], canonical_locale(row["locale"]))
            current[key] = float(row["changed_percent"])

    improved = regressed = unchanged = 0
    with output_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page", "locale", "p2c2_run_id", "p2c2_artifact_id", "p2c2_changed_percent", "current_changed_percent", "delta_percentage_points", "trend", "verdict"))
        for key, baseline in P2C2_CHANGED_PERCENT.items():
            if key not in current:
                raise ValueError(f"Current visual metrics missing P2C-2 baseline pair {key}")
            delta = round(current[key] - baseline, 4)
            if delta < -0.01:
                trend = "IMPROVED"; improved += 1
            elif delta > 0.01:
                trend = "REGRESSED"; regressed += 1
            else:
                trend = "UNCHANGED_WITHIN_0.01PP"; unchanged += 1
            writer.writerow((*key, P2C2_RUN_ID, P2C2_COMPARISON_ARTIFACT_ID, f"{baseline:.4f}", f"{current[key]:.4f}", f"{delta:.4f}", trend, "COMPARE_ONLY; NOT PIXEL PASS"))
    manifest.append((output_path.name, sha256(output_path)))
    return improved, regressed, unchanged


def write_geometry_table(windows: Path, linux: Path, output: Path) -> tuple[int, int]:
    win = parse_windows_geometry(windows)
    lin = parse_linux_geometry(linux)
    validate_windows_region_rows(win)
    validate_linux_region_rows(lin)
    keys = sorted(set(win) | set(lin))
    passed = 0
    failed = 0
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page_idd", "locale", "state", "resource_id", "part", "win_x", "win_y", "win_w", "win_h", "linux_x", "linux_y", "linux_w", "linux_h", "dx", "dy", "dw", "dh", "status"))
        for key in keys:
            window_rect = win.get(key)
            linux_rect = lin.get(key)
            if window_rect is None or linux_rect is None:
                status = "MISSING_WINDOWS" if window_rect is None else "MISSING_LINUX"
                values = ["" for _ in range(17)]
                values[0:5] = key
                writer.writerow(values + [status])
                failed += 1
                continue
            delta = tuple(linux_rect[index] - window_rect[index] for index in range(4))
            status = "PASS" if all(abs(value) <= 2.0 for value in delta) else "FAIL"
            writer.writerow((*key, *window_rect, *linux_rect, *delta, status))
            if status == "PASS":
                passed += 1
            else:
                failed += 1
    return passed, failed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--windows-dir", type=Path, required=True)
    parser.add_argument("--linux-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    outer_w, outer_h, client_w, client_h, origin_x, origin_y = metadata(args.windows_dir / "result.txt")
    metrics_path = args.output_dir / "visual-metrics.csv"
    manifest_path = args.output_dir / "visual-sha256.csv"
    manifest = []
    with metrics_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("page", "locale", "client_width", "client_height", "changed_pixels", "changed_percent", "mean_absolute_channel_difference", "max_channel_difference", "pixel_verdict"))
        for page, page_name in enumerate(PAGES):
            for locale in LOCALES:
                win_path = locate_windows_capture(args.windows_dir, page, locale)
                linux_path = locate_linux_capture(args.linux_dir, page, locale)
                with Image.open(win_path) as original:
                    if original.size != (outer_w, outer_h):
                        raise ValueError(f"Unexpected Windows dimensions for {win_path.name}: {original.size}")
                    win_client = original.convert("RGB").crop((origin_x, origin_y, origin_x + client_w, origin_y + client_h))
                with Image.open(linux_path) as original:
                    linux_client = original.convert("RGB")
                    if linux_client.size != (client_w, client_h):
                        raise ValueError(f"Unexpected Linux client dimensions for {linux_path.name}: {linux_client.size}")

                stem = f"{page_name}-{locale}"
                crop_path = args.output_dir / f"windows-client-{stem}.png"
                win_client.save(crop_path)
                side = Image.new("RGB", (client_w * 2, client_h), "white")
                side.paste(win_client, (0, 0))
                side.paste(linux_client, (client_w, 0))
                side_path = args.output_dir / f"side-by-side-{stem}.png"
                side.save(side_path)
                overlay_path = args.output_dir / f"overlay-50-{stem}.png"
                Image.blend(win_client, linux_client, 0.5).save(overlay_path)
                diff = ImageChops.difference(win_client, linux_client)
                diff_path = args.output_dir / f"difference-{stem}.png"
                heatmap(diff).save(diff_path)
                pixels, percent, mae, maximum = pixel_metrics(win_client, linux_client)
                writer.writerow((page_name, locale, client_w, client_h, pixels, f"{percent:.4f}", f"{mae:.4f}", maximum, "COMPARE_ONLY; NOT PIXEL PASS"))
                for item in (win_path, linux_path, crop_path, side_path, overlay_path, diff_path):
                    manifest.append((item.name, sha256(item)))

    passed, failed = write_geometry_table(
        args.windows_dir / "windows-settings-geometry.csv",
        args.linux_dir / "p2c-client.log",
        args.output_dir / "control-geometry-diff.csv",
    )
    windows_geometry = parse_windows_geometry(args.windows_dir / "windows-settings-geometry.csv")
    linux_geometry = parse_linux_geometry(args.linux_dir / "p2c-client.log")
    validate_windows_region_rows(windows_geometry)
    validate_appearance_swatches(args.windows_dir, args.linux_dir, windows_geometry, linux_geometry)
    text_exact, text_divergent, text_missing, unexpected_text = write_text_metrics_comparison(
        args.windows_dir, args.linux_dir, args.output_dir, manifest,
    )
    improved, regressed, unchanged = write_visual_regression_table(
        metrics_path, args.output_dir / "visual-regression-vs-p2c2.csv", manifest,
    )
    scroll_passed, scroll_failed = write_scroll_comparison(
        args.windows_dir, args.linux_dir, args.output_dir,
        (outer_w, outer_h, client_w, client_h, origin_x, origin_y), manifest,
    )
    write_region_text_metrics(
        args.windows_dir, args.linux_dir, args.output_dir,
        (outer_w, outer_h, client_w, client_h, origin_x, origin_y),
        windows_geometry, linux_geometry, manifest,
    )
    write_font_resolution_report(args.windows_dir, args.linux_dir, args.output_dir, manifest)
    raster_path = args.output_dir / "control-raster-diff.csv"
    raster_passed, raster_failed = validate_surface_and_checkbox_raster(
        args.windows_dir, args.linux_dir, raster_path
    )
    navigation_path = args.output_dir / "nav-selection-geometry.csv"
    nav_passed, nav_failed, guide_passed, guide_failed = validate_navigation_raster(
        args.windows_dir, args.linux_dir, navigation_path
    )
    geometry_path = args.output_dir / "control-geometry-diff.csv"
    windows_environment = (args.windows_dir / "windows-environment.txt").read_text(encoding="utf-8-sig", errors="replace")
    linux_environment = (args.linux_dir / "p2c-environment.txt").read_text(encoding="utf-8", errors="replace")
    report_path = args.output_dir / "comparison-notes.txt"
    report_path.write_text(
        "Windows reference: Mem Reduct v.3.5.2, source tag 2f5c55d15f69a2c88859e036b16c6f4c971e1968.\n"
        f"Windows measured outer/client: {outer_w}x{outer_h} / {client_w}x{client_h}; client crop origin={origin_x},{origin_y}.\n"
        "Linux source: production Zamak settings window captured by XTEST in an isolated Ubuntu 24.04 Xvfb runner.\n"
        "The Windows PNGs are only cropped at the measured client origin; neither reference nor Linux images are resized, stretched, shifted, or resampled.\n"
        "Geometry verdicts use resource-ID keyed Win32 window/control rectangles and Linux Canvas logical rectangles. PASS requires |dx|, |dy|, |dw|, |dh| <= 2 px for each row. IDC_NAV is compared as one resource control; its non-HWND tree items are checked separately from native selected-row pixels, dotted guide pixels, and XTEST clicks.\n"
        "The memory-region list is compared in matching top (rows 0-5), middle (rows 1-6), and bottom (rows 2-7) states. Windows native screenshots and resource-ID row rectangles are collected at all three offsets for both locales; the middle-state raster comparison is emitted separately and remains compare-only.\n"
        "The navigation table derives selected-row bounds from the current native #D9D9D9 screenshot for each page and locale, compares that measurement to the pinned historical baseline, then compares Linux directly to the current native measurement. Dotted branch points are checked against native #6D6D6D samples.\n"
        "Win32 WM_GETFONT/LOGFONT and GDI TEXTMETRIC/GDI text extents are exported per visible control. Linux Canvas text layout width/height and text origin are exported by the production test build through ui.text_width/ui.text_height; fontconfig family matches are recorded as fallback candidates because the Canvas API exposes the GTK system font, not a per-call family. Exact source strings and source-level language divergences are compared separately from text metric deltas. Middle-scroll label ink boxes are measured from unscaled screenshots, separately from control geometry and antialiasing.\n"
        "Font, text, control raster and geometry are separate evidence categories. Pixel metrics are raw comparisons only and do not imply 1:1 equivalence.\n\n"
        "Windows environment metadata:\n" + windows_environment + "\nLinux environment metadata:\n" + linux_environment,
        encoding="utf-8",
    )
    for item in (metrics_path, geometry_path, raster_path, navigation_path, report_path):
        manifest.append((item.name, sha256(item)))
    with manifest_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("file", "sha256"))
        writer.writerows(manifest)
    print(f"PASS screenshots=10 windows_outer={outer_w}x{outer_h} client_crop={origin_x},{origin_y},{client_w},{client_h} linux_canvas={client_w}x{client_h}")
    print(f"GEOMETRY target=<=2px pass={passed} fail_or_missing={failed} table={geometry_path.name}")
    print(f"CONTROL_RASTER target=<=2px pass={raster_passed} fail={raster_failed} table={raster_path.name}")
    print(f"NAV_SELECTION target=<=2px pass={nav_passed} fail={nav_failed} table={navigation_path.name}")
    print(f"NAV_TREE_DOTS target=exact-native-pixels pass={guide_passed} fail={guide_failed}")
    print(f"MIDDLE_SCROLL screenshots={scroll_passed} fail={scroll_failed} geometry=resource-ID keyed")
    print(f"TEXT_METRICS same_source_text={text_exact} source_text_divergence={text_divergent} missing={text_missing} unexpected={unexpected_text} table=text-metrics-diff.csv")
    print(f"P2C2_REGRESSION improved={improved} regressed={regressed} unchanged={unchanged} table=visual-regression-vs-p2c2.csv")
    print("FONT_METRICS exported=Win32 WM_GETFONT/LOGFONT + GDI text extents; Linux=Canvas Pango layout metrics + fontconfig fallback")
    print("PIXEL_COMPARISON generated=side-by-side,50pct-overlay,difference-heatmap; verdict=NOT_PIXEL_EQUIVALENCE")
    return 0 if failed == 0 and raster_failed == 0 and nav_failed == 0 and guide_failed == 0 and scroll_failed == 0 and text_missing == 0 and unexpected_text == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
