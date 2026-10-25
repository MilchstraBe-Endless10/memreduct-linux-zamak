# 初始 GitHub Issue backlog（建仓后按优先级创建）

| Priority | Proposed title | Exit criterion |
|---|---|---|
| P0 | Bootstrap PUBLIC repository and verify initial commit SHA | remote main SHA matches local; visibility PUBLIC; CI exists |
| P1 | Make Zamak 0.2.0 check/test/build pass on Arch | actual logs + native executable |
| P1 | Hyprland GTK3 GUI smoke and Polkit agent requirements | screenshot/logs and non-root execution |
| P2 | Meminfo parser: missing values and labeling | fixture regression tests; no misleading cache count |
| P3 | Privileged helper/async bridge denial and crash path | error-path tests; no repeated authorization |
| P4 | Design safe automation and persistence ADR | explicit user-approved security design |
| P5 | Read-only ZRAM probe without swap changes | exact sysfs/proc evidence, no kernel write |
| P6 | Package and release candidate | clean Arch install/uninstall, checksums, all gates |

Each issue must refer to PRD feature(s), affected modules, constraints and concrete tests. Avoid packing all of P1–P6 into one PR.
