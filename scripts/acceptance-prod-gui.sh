#!/bin/bash
# 生产 GUI 对照验收脚本（无 CI 钩子）：记录即时/30s/数分钟四指标
# 用法：scripts/acceptance-prod-gui.sh <phase: before|after> <label>
set -u
PHASE="${1:-before}"; LABEL="${2:-run}"
OUT="/home/arlune_innersea/.codex/worktrees/q12-dof/memreduct-linux-zamak/deploy-backup/acceptance-${PHASE}-${LABEL}.log"
snap() {
  echo "== $1 $(date '+%F %T') ==" >> "$OUT"
  awk '/^MemAvailable/{printf "MemAvailable_kib=%s\n",$2}' /proc/meminfo >> "$OUT"
  awk 'NR==1{printf "PSI_mem %s %s %s\n",$2,$3,$4}' /proc/pressure/memory >> "$OUT"
  grep -E "pswpin|pswpout" /proc/vmstat | tr '\n' ' ' >> "$OUT"; echo >> "$OUT"
}
: > "$OUT"
snap immediate
sleep 30; snap 30s
sleep 180; snap 3min
echo "wrote $OUT"
