/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pure presentation helpers for the balanced clean entry point.
 *
 * No file is opened here, no syscall is made, no allocation happens. The
 * strings are static so a GUI redraw cannot leak and a test can compare them
 * byte for byte.
 */
#include <stdio.h>

#include "balance_gui.h"

int64_t mr_bal_gui_entry(const struct mr_bal_gui_availability *a)
{
    if (a == NULL || !a->built_in) return MR_BAL_ENTRY_HIDDEN;
    if (!a->gate_enabled) return MR_BAL_ENTRY_HIDDEN;
    if (a->busy) return MR_BAL_ENTRY_DISABLED;
    if (!a->helper_ready) return MR_BAL_ENTRY_DISABLED;
    if (a->no_target) return MR_BAL_ENTRY_DISABLED;
    return MR_BAL_ENTRY_READY;
}

/* "<n> MiB" with one decimal, no float formatting.
 *
 * Two rotating buffers, not one: a single static buffer is overwritten by the
 * second call, so a line that names two sizes ("requested X, accepted Y")
 * printed the same number twice. This was caught by the counter-line test. */
static void mib_into(char *buf, size_t cap, uint64_t bytes)
{
    uint64_t tenths = (bytes * 10ull) / (1024ull * 1024ull);
    snprintf(buf, cap, "%llu.%llu MiB",
             (unsigned long long)(tenths / 10ull),
             (unsigned long long)(tenths % 10ull));
}

static const char *mib(uint64_t bytes)
{
    static char buf[32];
    mib_into(buf, sizeof(buf), bytes);
    return buf;
}

static const char *state_reason_en(int64_t state)
{
    switch (state) {
    case 1: return "Memory is healthy; there is nothing worth reclaiming.";
    case 2: return "The cache is holding reclaimable pages; dropping them would only cause re-reads.";
    case 3: return "Cold file pages dominate; they are the safest thing to release.";
    case 4: return "The anonymous working set dominates; dropping file cache would not help.";
    case 5: return "The machine is already swapping and stalling; reclaim would make it worse.";
    default: return "Not enough evidence to decide.";
    }
}

static const char *state_reason_zh(int64_t state)
{
    switch (state) {
    case 1: return "内存健康，没有值得回收的页。";
    case 2: return "缓存持有的都是可回收页，丢掉只会导致重读。";
    case 3: return "冷文件页占主导，是当前最安全的回收对象。";
    case 4: return "匿名工作集占主导，丢文件缓存帮不上忙。";
    case 5: return "已经在换页并停顿，再回收只会更糟。";
    default: return "证据不足，无法判断。";
    }
}

const char *mr_bal_gui_plan_line(int64_t line, int32_t zh,
                                 int64_t state, int64_t action,
                                 uint64_t request_bytes,
                                 int32_t have_pressure_evidence)
{
    static char buf[256];
    switch (line) {
    case MR_BAL_LINE_ACTION:
        if (action == 1) return zh ? "均衡清理：优先回收冷文件页。" : "Balanced clean: release cold file pages first.";
        if (action == 2) return zh ? "均衡清理：分批回收匿名页。" : "Balanced clean: release anonymous pages in batches.";
        return zh ? "均衡清理：本次不执行。" : "Balanced clean: nothing will run.";
    case MR_BAL_LINE_WHY:
        if (!have_pressure_evidence) return zh ? "缺少压力证据，不做判断。" : "No pressure evidence; refusing to guess.";
        return zh ? state_reason_zh(state) : state_reason_en(state);
    case MR_BAL_LINE_BUDGET:
        if (action == 0) return zh ? "请求预算：0（不提交）" : "Request budget: 0 (nothing will be submitted)";
        snprintf(buf, sizeof(buf), "%s %s",
                 zh ? "请求预算（不是释放量）：" : "Request budget (not an amount freed):",
                 mib(request_bytes));
        return buf;
    case MR_BAL_LINE_SKIPS:
        return zh ? "跳过的应用会逐条列出原因，不会静默省略。"
                  : "Every skipped application is listed with its reason; none are silently dropped.";
    default:
        return zh ? "未知" : "unknown";
    }
}

static const char *stop_en(int64_t reason)
{
    switch (reason) {
    case 1: return "Stopped: no eligible file pages remained.";
    case 2: return "Stopped: memory pressure rose while reclaiming.";
    case 3: return "Stopped: this kernel does not support file-only reclaim.";
    case 4: return "Stopped: the kernel returned an error.";
    default: return "Finished the requested batches.";
    }
}

static const char *stop_zh(int64_t reason)
{
    switch (reason) {
    case 1: return "已停止：没有合格的冷文件页。";
    case 2: return "已停止：回收过程中内存压力上升。";
    case 3: return "已停止：此内核不支持仅回收文件页。";
    case 4: return "已停止：内核返回错误。";
    default: return "已按请求批次完成。";
    }
}

const char *mr_bal_gui_result_line(int64_t line, int32_t zh,
                                   int64_t status, int64_t stop_reason,
                                   uint64_t requested_bytes,
                                   uint64_t accepted_bytes,
                                   uint32_t batches_accepted,
                                   uint32_t batches_rejected,
                                   int64_t available_delta_bytes)
{
    static char buf[256];
    switch (line) {
    case MR_BAL_RESULT_HEADLINE:
        if (status == 0) return zh ? "均衡清理完成。" : "Balanced clean finished.";
        if (status == 1) return zh ? "均衡清理部分完成。" : "Balanced clean partially finished.";
        if (status == 2) return zh ? "均衡清理未执行任何批次。" : "Balanced clean ran no batch.";
        if (status == 3) return zh ? "此环境不支持均衡清理。" : "Balanced clean is not supported here.";
        return zh ? "均衡清理结果不确定。" : "Balanced clean outcome is uncertain.";
    case MR_BAL_RESULT_COUNTERS: {
        /* Two separate buffers: passing mib() twice in one call would hand the
         * same rotating buffer to both format specifiers. */
        char requested[32];
        char accepted[32];
        mib_into(requested, sizeof(requested), requested_bytes);
        mib_into(accepted, sizeof(accepted), accepted_bytes);
        snprintf(buf, sizeof(buf),
                 zh ? "请求 %s · 内核接受 %s · 批次 %u 成功 / %u 拒绝"
                    : "Requested %s · kernel accepted %s · %u batch(es) accepted / %u rejected",
                 requested, accepted, batches_accepted, batches_rejected);
        return buf;
    }
    case MR_BAL_RESULT_STOP:
        return zh ? stop_zh(stop_reason) : stop_en(stop_reason);
    case MR_BAL_RESULT_OBSERVED:
        if (available_delta_bytes == 0)
            return zh ? "MemAvailable 无变化。" : "MemAvailable did not change.";
        snprintf(buf, sizeof(buf),
                 zh ? "MemAvailable 变化 %s%s（观测值，不是释放量）"
                    : "MemAvailable changed by %s%s (an observation, not an amount freed)",
                 available_delta_bytes < 0 ? "-" : "+",
                 mib(available_delta_bytes < 0
                         ? (uint64_t)(-available_delta_bytes)
                         : (uint64_t)available_delta_bytes));
        return buf;
    default:
        return zh ? "未知" : "unknown";
    }
}

int32_t mr_bal_gui_result_is_meaningful(int64_t status, uint64_t accepted_bytes)
{
    /* "Accepted" means the kernel took the request, not that pages left. A run
     * that accepted nothing must not be dressed up as a successful clean. */
    if (status == 3) return 0;
    if (accepted_bytes == 0) return 0;
    return 1;
}
