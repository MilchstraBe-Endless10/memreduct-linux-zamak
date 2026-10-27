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
PAGES_ZH = ("常规", "内存清理", "外观", "托盘图标", "高级")
EXPERIMENT_LOCALES = {"A": "English", "B": "zh-CN", "C": "zh-CN", "D": "English"}
PAGE_SENTINELS = ((116, 123), (124, 125, 131), (133, 139, 140), (141, 148, 152), (153, 154))
PAGE_IDENTITY_METHOD = "unique-visible-resource-sentinels+IDC_NAV-text"
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


def locate_windows_light(root: Path, page: int, locale: str) -> Path:
    suffixes = ("English", "en") if locale == "English" else ("zh-CN",)
    for suffix in suffixes:
        path = root / f"windows-native-settings-{page}-{suffix}.png"
        if path.is_file():
            return path
    raise FileNotFoundError(f"Missing Windows light reference for page={page} locale={locale}")


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
    for experiment in EXPERIMENT_LOCALES:
        if f"PASS isolated dark-state experiment={experiment} " not in windows_result:
            raise ValueError(f"Windows artifact lacks a completed fresh-process experiment {experiment}")
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
    outer_w, outer_h, client_w, client_h, legacy_origin_x, legacy_origin_y = map(int, match.groups())
    if (outer_w, outer_h, client_w, client_h) != (574, 374, 558, 335):
        raise ValueError(f"Unexpected Windows reference geometry: {outer_w}x{outer_h}/{client_w}x{client_h}")

    page_manifest = args.windows_dir / "windows-dark-state-pages.csv"
    if not page_manifest.is_file():
        raise FileNotFoundError("Windows artifact lacks the R1 page/locale/theme state manifest")
    page_states: dict[tuple[str, int], dict[str, str]] = {}
    with page_manifest.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            experiment = row["experiment_id"]
            page_index = int(row["page_index"])
            locale = EXPERIMENT_LOCALES.get(experiment)
            expected_nav = PAGES[page_index] if locale == "English" else PAGES_ZH[page_index]
            if locale is None or row["locale_expected"] != locale or row["locale_from_settings"] != locale:
                raise ValueError(f"Unverified language state for experiment={experiment}, page={page_index}: {row}")
            if (row["menu_locale"] != locale or row["dark_checked"].lower() != "true"
                    or int(row["dark_menu_state"], 16) & 0x8 == 0
                    or row["page_resource_id"] != str(101 + page_index)
                    or row.get("page_identity_method") != PAGE_IDENTITY_METHOD
                    or row.get("page_root_control_id") != "0"):
                raise ValueError(f"Unverified dark/menu or Win32 page resource for experiment={experiment}, page={page_index}: {row}")
            if row["selected_nav"] != expected_nav or row["semantic_pair_valid"].lower() != "true":
                raise ValueError(f"Navigation selection does not match the actual page for experiment={experiment}, page={page_index}: {row}")
            visible_ids = {int(value) for value in row["visible_resource_ids"].split("|") if value}
            if not set(PAGE_SENTINELS[page_index]).issubset(visible_ids):
                raise ValueError(f"Visible Win32 child controls do not identify IDD {101 + page_index}: {row}")
            if row["settings_outer"] != "574x374" or row["settings_client"] != "558x335":
                raise ValueError(f"Unexpected fresh-process Settings geometry for experiment={experiment}, page={page_index}")
            origin_x, origin_y = int(row["client_origin_x"]), int(row["client_origin_y"])
            if origin_x < 0 or origin_y < 0 or origin_x + 558 > 574 or origin_y + 335 > 374:
                raise ValueError(f"Measured Win32 client origin is outside the captured window for experiment={experiment}, page={page_index}")
            if experiment not in EXPERIMENT_LOCALES or not (args.windows_dir / row["screenshot"]).is_file():
                raise FileNotFoundError(f"Missing semantically verified screenshot for experiment={experiment}, page={page_index}")
            key = (experiment, page_index)
            if key in page_states:
                raise ValueError(f"Duplicate Windows semantic page state: {key}")
            page_states[key] = row
    expected_states = {(experiment, page) for experiment in EXPERIMENT_LOCALES for page in range(5)}
    if set(page_states) != expected_states:
        raise ValueError(f"Expected 20 verified Windows experiment/page states; got {len(page_states)}: {sorted(page_states)}")

    control_manifest = args.windows_dir / "windows-dark-state-controls.csv"
    if not control_manifest.is_file():
        raise FileNotFoundError("Windows artifact lacks page-scoped control visibility and enabled state")
    controls_by_page: dict[tuple[str, int], dict[int, dict[str, str]]] = {}
    with control_manifest.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            key = (row["experiment_id"], int(row["page_index"]))
            resource_id = int(row["resource_id"])
            if key not in page_states or row["page_resource_id"] != str(101 + key[1]) or resource_id <= 0:
                raise ValueError(f"Control state is not attached to a verified page/resource: {row}")
            if row["visible"].lower() not in {"true", "false"} or row["enabled"].lower() not in {"true", "false"}:
                raise ValueError(f"Invalid Win32 control visibility/enabled state: {row}")
            page_controls = controls_by_page.setdefault(key, {})
            if resource_id in page_controls:
                raise ValueError(f"Duplicate Win32 resource state for {key} resource={resource_id}")
            page_controls[resource_id] = row
    if set(controls_by_page) != expected_states:
        raise ValueError(f"Expected page-scoped control state for all 20 Windows captures, got {len(controls_by_page)}")
    for key, page_row in page_states.items():
        controls = controls_by_page[key]
        visible_ids = {resource for resource, control in controls.items() if control["visible"].lower() == "true"}
        enabled_ids = {resource for resource, control in controls.items() if control["enabled"].lower() == "true"}
        manifest_visible = {int(value) for value in page_row["visible_resource_ids"].split("|") if value}
        manifest_enabled = {int(value) for value in page_row["enabled_resource_ids"].split("|") if value}
        if visible_ids != manifest_visible or enabled_ids != manifest_enabled:
            raise ValueError(f"Win32 page state summary disagrees with per-control visibility/enabled matrix for {key}")
        page_index = key[1]
        if not set(PAGE_SENTINELS[page_index]).issubset(visible_ids):
            raise ValueError(f"Page control matrix does not show all visible identity sentinels for {key}")

    color_manifest = args.windows_dir / "windows-dark-state-colors.csv"
    if not color_manifest.is_file():
        raise FileNotFoundError("Windows artifact lacks per-page dark-theme color samples")
    color_counts: dict[tuple[str, int], int] = {}
    with color_manifest.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            key = (row["experiment_id"], int(row["page_index"]))
            if key not in page_states or row["page_resource_id"] != str(101 + key[1]):
                raise ValueError(f"Dark-theme color sample is not bound to a verified page: {row}")
            if row["semantic_state"] != "dark_menu_checked=True" or not re.fullmatch(r"#[0-9A-Fa-f]{6}", row["rgb"]):
                raise ValueError(f"Invalid theme state or RGB sample in Windows color matrix: {row}")
            color_counts[key] = color_counts.get(key, 0) + 1
    if set(color_counts) != expected_states or any(count < 5 for count in color_counts.values()):
        raise ValueError(f"Expected at least five state-bound color samples for each Windows page; got {color_counts}")

    out = args.output_dir / "dark-theme"
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    transition_rows = []
    manifest = []
    experiment_images: dict[tuple[str, int], Image.Image] = {}
    for (experiment, page_index), row in sorted(page_states.items()):
        locale = EXPERIMENT_LOCALES[experiment]
        locale_key = "en" if locale == "English" else "zh"
        page = PAGES[page_index]
        windows_dark_path = args.windows_dir / row["screenshot"]
        windows_light_path = locate_windows_light(args.windows_dir, page_index, locale)
        linux_dark_path = args.linux_dir / f"production-settings-{page_index}-{locale_key}-dark.png"
        linux_light_path = args.linux_dir / f"production-settings-{page_index}-{locale_key}.png"
        for path in (windows_dark_path, windows_light_path, linux_dark_path, linux_light_path):
            if not path.is_file():
                raise FileNotFoundError(f"Missing paired screenshot: {path.name}")
        # Windows manifests use the display label "English", while Linux
        # capture logs use the locale key "en". Match the recorded Linux
        # capture state using the locale key derived above.
        capture_locale = locale_key
        if f"theme=dark locale={capture_locale} screenshot={linux_dark_path.name}" not in capture_log:
            raise ValueError(f"Linux capture log does not associate dark state with {linux_dark_path.name}")

        win_dark = crop_windows(windows_dark_path, (int(row["client_origin_x"]), int(row["client_origin_y"])))
        # Legacy light screenshots come from the original capture sequence and
        # use its measured client origin. The R1 dark window origin is read
        # independently from each semantic page row above.
        win_light = crop_windows(windows_light_path, (legacy_origin_x, legacy_origin_y))
        with Image.open(linux_dark_path) as source:
            linux_dark = source.convert("RGB")
        with Image.open(linux_light_path) as source:
            linux_light = source.convert("RGB")
        if linux_dark.size != (558, 335) or linux_light.size != (558, 335):
            raise ValueError(f"Linux Canvas screenshot has unexpected dimensions on {page}/{locale}")

        # The fresh dark capture is state-verified. The older light capture is
        # retained as a legacy visual reference; it is not used to validate
        # the R1 transition order or menu state.
        _, win_legacy_light_changed, _, _ = metrics(win_light, win_dark)
        _, linux_theme_changed, _, _ = metrics(linux_light, linux_dark)
        if linux_theme_changed <= 1.0:
            raise ValueError(f"Linux production app-dark screenshot has no material change on {page}/{locale}: {linux_theme_changed:.4f}%")

        diff = ImageChops.difference(win_dark, linux_dark)
        stem = f"r1-{experiment}-{page_index}-{locale_key}"
        side = Image.new("RGB", (1116, 335))
        side.paste(win_dark, (0, 0))
        side.paste(linux_dark, (558, 0))
        overlay = Image.blend(win_dark, linux_dark, 0.5)
        paths = {
            f"{stem}-windows-crop.png": win_dark,
            f"{stem}-side-by-side.png": side,
            f"{stem}-overlay-50.png": overlay,
            f"{stem}-difference.png": diff,
            f"{stem}-heatmap.png": heatmap(diff),
        }
        for name, image in paths.items():
            path = out / name
            image.save(path)
            manifest.append((name, sha256(path)))

        changed, percent, mae, maximum = metrics(win_dark, linux_dark)
        rows.append((experiment, row["sequence"], page, locale, row["page_resource_id"], row["selected_nav"],
                     row["dark_menu_state"], "true", 558, 335, changed, f"{percent:.4f}", f"{mae:.4f}", maximum,
                     f"{win_legacy_light_changed:.4f}", f"{linux_theme_changed:.4f}",
                     sha256(windows_dark_path), sha256(linux_dark_path),
                     "SEMANTIC_STATE_VERIFIED; CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS"))
        experiment_images[(experiment, page_index)] = win_dark
        manifest.extend(((windows_dark_path.name, sha256(windows_dark_path)), (linux_dark_path.name, sha256(linux_dark_path))))

    for page_index, page in enumerate(PAGES):
        for pair_name, first, second in (("B-vs-C", "B", "C"), ("A-vs-D", "A", "D")):
            first_image = experiment_images[(first, page_index)]
            second_image = experiment_images[(second, page_index)]
            diff = ImageChops.difference(first_image, second_image)
            changed, percent, mae, maximum = metrics(first_image, second_image)
            name = f"transition-order-{pair_name}-{page_index}.png"
            heat_name = f"transition-order-{pair_name}-{page_index}-heatmap.png"
            diff.save(out / name)
            heatmap(diff).save(out / heat_name)
            transition_rows.append((pair_name, page, page_index, changed, f"{percent:.4f}", f"{mae:.4f}", maximum,
                                    sha256(args.windows_dir / page_states[(first, page_index)]["screenshot"]),
                                    sha256(args.windows_dir / page_states[(second, page_index)]["screenshot"]),
                                    "SAME_PROCESS_ISOLATION_AND_PAGE_STATE; ORDER_EFFECT_MEASURED"))
            manifest.extend(((name, sha256(out / name)), (heat_name, sha256(out / heat_name))))

    with (out / "dark-theme-comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("experiment_id", "sequence", "page", "locale", "page_resource_id", "selected_nav", "dark_menu_state", "dark_checked", "width", "height", "changed_pixels", "changed_percent", "mean_absolute_error", "max_channel_delta", "windows_dark_vs_legacy_light_changed_percent", "linux_dark_vs_light_changed_percent", "windows_dark_png_sha256", "linux_dark_png_sha256", "verdict"))
        writer.writerows(rows)
    with (out / "transition-order-comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("experiment_pair", "page", "page_index", "changed_pixels", "changed_percent", "mean_absolute_error", "max_channel_delta", "first_windows_png_sha256", "second_windows_png_sha256", "verdict"))
        writer.writerows(transition_rows)
    for name in ("windows-dark-state-experiments.csv", "windows-dark-state-transitions.csv", "windows-dark-state-pages.csv", "windows-dark-state-controls.csv", "windows-dark-state-colors.csv"):
        source = args.windows_dir / name
        if source.is_file():
            target = out / name
            target.write_bytes(source.read_bytes())
            manifest.append((name, sha256(target)))
    manifest_path = out / "dark-theme-sha256.txt"
    manifest_path.write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(set(manifest))), encoding="utf-8")
    if len(rows) != 20 or len(transition_rows) != 10:
        raise AssertionError(f"Expected 20 semantic dark screenshot pairs and 10 transition-order comparisons; got {len(rows)} and {len(transition_rows)}")
    print("PASS fresh-process Windows dark screenshots semantically paired with production Xvfb screenshots: 20/20.")
    print("PASS transition-order comparison generated: Chinese B-vs-C 5/5 and English A-vs-D 5/5; any mixed palette remains classified from native evidence.")
    print(f"PASS dark/light production geometry unchanged: {geometry_pairs} resource rectangles across all 10 page/locale pairs.")
    print("NOT PASS full-page pixel identity: metrics are descriptive; geometry and text/raster classes remain separate.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
