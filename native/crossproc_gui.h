/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GUI contract for the experimental "cross-process memory reclaim" surface
 * (Issue #12, ADR-0007). Header-only on purpose: `src/main.zm` belongs to the
 * Integrator, so this file states what the interface must be able to show and
 * what it must never claim, and leaves the wiring alone.
 *
 * This capability is separate from page-cache cleaning. It is not "System
 * working set", and no number in this interface is an amount freed.
 */
#ifndef MEMREDUCT_CROSSPROC_GUI_H
#define MEMREDUCT_CROSSPROC_GUI_H

/* Stable wording keys. The GUI maps a key to a sentence in both languages; the
 * English and Chinese strings live with the rest of the UI text. */
#define MR_CROSSPROC_UI_LABEL_KEY       "crossproc.experimental.label"
#define MR_CROSSPROC_UI_WARNING_KEY     "crossproc.experimental.warning"
#define MR_CROSSPROC_UI_AMOUNT_KEY      "crossproc.experimental.amount"
#define MR_CROSSPROC_UI_SKIPPED_KEY     "crossproc.experimental.skipped"
#define MR_CROSSPROC_UI_DISABLED_KEY    "crossproc.experimental.not_built"

/* Mandatory sentences. Each one exists because a cheaper phrasing would let a
 * user believe something the kernel does not promise. A translation must keep
 * the meaning, and adding a fifth claim requires an ADR amendment. */
#define MR_CROSSPROC_UI_WARNING_EN \
    "Experimental. Marks or pushes out other processes' pages. " \
    "This can cause disk I/O, swap use, and visible stalls in those processes. " \
    "The kernel only accepts a request: no amount of released memory is promised."

#define MR_CROSSPROC_UI_WARNING_ZH \
    "实验功能。会对其他进程的页标记冷页或尝试换出。" \
    "这可能引起磁盘 I/O、占用 swap，并让那些进程出现明显卡顿。" \
    "内核只接受请求，不保证释放任何内存量。"

#define MR_CROSSPROC_UI_AMOUNT_EN \
    "Advised: %llu bytes over %u regions in %u processes. Not an amount freed."

#define MR_CROSSPROC_UI_AMOUNT_ZH \
    "已建议：%llu 字节，涉及 %u 个区域、%u 个进程。这不是实际释放量。"

#define MR_CROSSPROC_UI_SKIPPED_EN \
    "Skipped: %u system, %u other users, %u exited, %u already reused, " \
    "%u shared or writable, %u over budget."

#define MR_CROSSPROC_UI_SKIPPED_ZH \
    "已跳过：%u 个系统进程、%u 个其他用户、%u 个已退出、%u 个 PID 已复用、" \
    "%u 个共享或可写映射、%u 个超出预算。"

#define MR_CROSSPROC_UI_DISABLED_EN \
    "Cross-process reclaim is not available in this build."

#define MR_CROSSPROC_UI_DISABLED_ZH \
    "此构建不包含跨进程回收能力。"

/* Rules the Integrator must keep when wiring this into the window:
 *
 *   1. Do not reuse the page-cache action's label, confirmation text, or result
 *      line. The two actions have different costs and different guarantees and
 *      must never look like one button.
 *   2. Do not display a "system working set", "released RAM", or "freed" value.
 *      Only MR_CROSSPROC_UI_AMOUNT_* is allowed, and it says "advised".
 *   3. Do not compute a summed RSS figure under any name. Linux has no
 *      equivalent of the Windows counter, and inventing one is the defect this
 *      ADR exists to prevent.
 *   4. Never pass a PID, path, or address range from the UI to the planner.
 *      The planner takes budget limits only.
 *   5. Show MR_CROSSPROC_UI_DISABLED_* instead of a disabled-but-clickable
 *      control when mr_crossproc_advise_enabled() answers 0.
 *   6. Production runs require the owner's written approval. Until then the
 *      button must stay absent, not merely greyed out.
 */

#endif /* MEMREDUCT_CROSSPROC_GUI_H */
