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
WINDOW_RE = re.compile(
    r"settings geometry outer=(\d+)x(\d+) client=(\d+)x(\d+).*?client_origin_in_window=(\d+),(\d+)"
)
GEOMETRY_RE = re.compile(
    r"CI_SETTINGS_GEOMETRY page_idd=(\d+) locale=([^ ]+) state=([^ ]+) resource=(\d+) part=([^ ]+) "
    r"x=([-\d.]+) y=([-\d.]+) w=([-\d.]+) h=([-\d.]+)"
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


def write_geometry_table(windows: Path, linux: Path, output: Path) -> tuple[int, int]:
    win = parse_windows_geometry(windows)
    lin = parse_linux_geometry(linux)
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
                intermediate_region_row = (
                    window_rect is None and key[0] == 102 and key[2] == "scroll-1"
                    and key[3] == 124 and key[4].startswith("row-")
                )
                status = "NOT_COMPARABLE_NATIVE_STATE" if intermediate_region_row else "MISSING_WINDOWS" if window_rect is None else "MISSING_LINUX"
                values = ["" for _ in range(17)]
                values[0:5] = key
                if intermediate_region_row and linux_rect is not None:
                    values[9:13] = linux_rect
                writer.writerow(values + [status])
                if not intermediate_region_row:
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
    validate_appearance_swatches(args.windows_dir, args.linux_dir, windows_geometry, linux_geometry)
    raster_path = args.output_dir / "control-raster-diff.csv"
    raster_passed, raster_failed = validate_surface_and_checkbox_raster(
        args.windows_dir, args.linux_dir, raster_path
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
        "Geometry verdicts use resource-ID keyed Win32 window/control rectangles and Linux Canvas logical rectangles. PASS requires |dx|, |dy|, |dw|, |dh| <= 2 px for each row. IDC_NAV is compared as one resource control; its non-HWND tree items are assessed by screenshots and XTEST clicks.\n"
        "The memory-region list is compared in matching top (rows 0-5) and bottom (rows 2-7) scroll states. Linux also tests the intermediate offset; its rows are bounds-checked and marked NOT_COMPARABLE_NATIVE_STATE because the Windows runner does not collect that scroll state. Appearance preview rows are pixel-sampled against the pinned source colors. The control-raster table separately checks native page-face samples and checkbox outline bounds; it does not treat text antialiasing as geometry.\n"
        "Font and rasterization differences are not treated as control geometry errors. Pixel metrics are raw comparisons only and do not imply 1:1 equivalence.\n\n"
        "Windows environment metadata:\n" + windows_environment + "\nLinux environment metadata:\n" + linux_environment,
        encoding="utf-8",
    )
    for item in (metrics_path, geometry_path, raster_path, report_path):
        manifest.append((item.name, sha256(item)))
    with manifest_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("file", "sha256"))
        writer.writerows(manifest)
    print(f"PASS screenshots=10 windows_outer={outer_w}x{outer_h} client_crop={origin_x},{origin_y},{client_w},{client_h} linux_canvas={client_w}x{client_h}")
    print(f"GEOMETRY target=<=2px pass={passed} fail_or_missing={failed} table={geometry_path.name}")
    print(f"CONTROL_RASTER target=<=2px pass={raster_passed} fail={raster_failed} table={raster_path.name}")
    print("PIXEL_COMPARISON generated=side-by-side,50pct-overlay,difference-heatmap; verdict=NOT_PIXEL_EQUIVALENCE")
    return 0 if failed == 0 and raster_failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
