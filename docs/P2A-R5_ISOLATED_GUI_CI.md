# P2A-R5 — isolated GUI CI implementation

## Scope and safety

This test-only branch adds the P2A-R4 isolated GUI harness. It does not change production Mem Reduct behavior, the P1 build implementation, or privileged functionality. The probe uses simulated labels and contains no memory reads, cleanup requests, Polkit calls, or Swap/ZRAM operations.

Branch: `test/issue-3-isolated-gui`
Base: `fix/issue-1-zamak-build` at `11b0b9444cd94f0026b855abe3849a06106426f2`
Target: Issue #3; separate Draft PR, no merge.

The workflow runs on GitHub-hosted Ubuntu 24.04 only. It has `contents: read`, no repository secrets, and no `pull_request_target`. System dependencies are installed inside disposable CI runners.

## Window size

The test window requests a compact 336×348 logical-pixel top-level size. Official Mem Reduct v.3.5.2 `IDD_MAIN` is 224×214 Dialog Units and declares `FONT 8, "Ms Shell Dlg"`. At 96 DPI, an estimated 6×13 px dialog base unit gives `224×6/4 = 336` by `214×13/8 ≈ 348`. The Ubuntu GTK Canvas viewport is expected to be 312×324 because the toolkit reserves 12 px on each edge. This top-level area is about 12% smaller than the previous 338×395 test fixture. The base-unit estimate has not been measured on real Windows, so this is a compact test estimate rather than authenticated Windows geometry or pixel parity. The earlier 941×1030 result was a tiled desktop viewport, not the intended window size. A real Windows capture with font/DPI metadata remains necessary for a pixel baseline.

## Test lanes

- **Wayland Headless/Pixman:** checks Weston runtime help/version, mode-0700 `XDG_RUNTIME_DIR`, unique socket, `wayland-info` connection, GTK Canvas draw, timer-driven test quit, `app.run` return, clean exit 0, and Wayland protocol markers. Timeout is a failure. No mouse/keyboard injection is claimed in this lane.
- **Xvfb/XTEST:** checks isolated display and XTEST extension; sends real X11 pointer and keyboard events through `xdotool`; verifies hover/press/release target routing, pressed-state pixel change, split menu selection, English/简体中文 switch, Tab, Enter, Escape, fixed geometry, screenshots, F12 clean quit, and exit 0. It is explicitly X11-only.
- **Zamak fixture:** requires version 0.2.0, source hash match, nine preserved routing tests, and successful check/test/build.

Each lane uploads environment and version information, source/binary checksums, logs, test results, and probe-only X11 screenshots for 14 days. Runtime directories and child processes are isolated and cleaned up by their owning script.

## Local verification before push

| Command | Result |
|---|---|
| `python3 scripts/verify_project.py` | PASS — 29 required checks |
| `python3 -m unittest discover -s tests -v` | PASS — 3 C safety tests |
| `bash -n` root and `scripts/ci/*.sh` | PASS |
| workflow YAML trigger/permission parse | PASS — push branch and PR base match requested refs; read-only permissions |
| `zam --version` | PASS — 0.2.0 |
| root `zam check .` | PASS — `CHECK OK memreduct_zamak` |
| root `zam test .` | PASS — 5 passed, 0 failed |
| root `zam build .` | PASS |
| `bash scripts/build-arch.sh` | PASS — native helper compiled; no install or privileged action run |
| probe `zam check .` | PASS — `CHECK OK p2a_r4_canvas_probe` |
| probe `zam test .` | PASS — 9 passed, 0 failed; function bodies match R2 source |
| probe `zam build .` | PASS |

## Hosted CI status

**Pending.** This report is committed before the first push-triggered run. Wayland and X11 runtime statuses must be updated from actual Actions logs and uploaded artifacts. Do not treat the local logic tests as GUI input acceptance.

## Remaining limits

- No Weston, Xvfb, GUI, or active Hyprland session was run or manipulated on the user's workstation.
- No Windows v3.5.2 capture or image-difference comparison is included; pixel acceptance remains BLOCKED / NOT RUN.
- The Wayland lane is a headless socket/window lifecycle test, not Hyprland compositor parity.
