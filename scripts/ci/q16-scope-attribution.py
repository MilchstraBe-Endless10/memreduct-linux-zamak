#!/usr/bin/env python3
"""Attribute anonymous-memory growth to specific app scopes, in two separate rankings.

What this answers, and what it deliberately does not
----------------------------------------------------
Two questions that look alike and are not:

  stock   which scopes hold the most anonymous memory right now
  growth  which scopes *added* the most anonymous memory over the sample window

A large quiet process and a small fast-growing one are different problems with
different responses, so the two rankings are printed separately and never
merged into one "memory hog" list.

Three claims this file refuses to make
--------------------------------------
1. It never attributes machine-wide AnonPages to one scope. /proc/meminfo
   reports a total across every process and every cgroup, plus kernel
   accounting that belongs to no scope at all. The sum of scope counters is
   recorded for comparison and labelled as such.

2. It never calls a cgroup counter "process RSS". memory.stat anon is what the
   cgroup is charged for; a process inside it may have a different resident
   set, and pages can be charged to a scope whose process has already exited.

3. It never calls growth "a leak". One window is a trend, not a diagnosis.

A measured hazard worth knowing before reading any number here
--------------------------------------------------------------
The fields of memory.stat are not read atomically. On this host, sampling
active_anon and inactive_anon in one pass produced a sample where
active_anon + inactive_anon (42,807,296) exceeded anon (41,271,296) by 1.5 MB.
So the identity anon == active_anon + inactive_anon does not hold at a single
point in time, and this script does not assume it does. Deltas are formed from
two complete samples taken the same way, which is the best available
approximation and is reported as such.

Usage
-----
  q16-scope-attribution.py            sample, then print both rankings
  q16-scope-attribution.py --top 8    show 8 rows per ranking
  q16-scope-attribution.py --window 90  sample for 90 seconds

Output is plain text, meant to be pasted into a report. Nothing is written to
any cgroup; every file opened here is read-only.
"""

import argparse
import glob
import os
import sys
import time

SCOPE_ROOTS = [
    # The per-user app slice. The path is user-specific by construction and is
    # discovered rather than hardcoded, because the UID and the slice name vary.
    # Nested slices under it are reached by walking, not by listing them here.
    "/sys/fs/cgroup/user.slice/user-{uid}.slice/user@{uid}.service/app.slice",
]

FIELDS = ("anon", "file", "active_file", "inactive_file",
          "workingset_refault_file", "active_anon", "inactive_anon")


def roots(uid):
    out = []
    for template in SCOPE_ROOTS:
        path = template.format(uid=uid)
        if os.path.isdir(path):
            out.append(path)
    return out


def read_stat(path):
    """One complete pass over memory.stat. Missing fields are None, not zero."""
    values = {}
    try:
        with open(os.path.join(path, "memory.stat"), encoding="utf-8") as handle:
            for line in handle:
                parts = line.split()
                if len(parts) == 2:
                    values[parts[0]] = int(parts[1])
    except OSError:
        return None
    return {name: values.get(name) for name in FIELDS}


def scope_label(path):
    """`app-org.chromium.Chromium-9007.scope` -> `org.chromium.Chromium`."""
    name = os.path.basename(path)
    if name.startswith("app-"):
        name = name[4:]
    for suffix in UNIT_SUFFIXES:
        if name.endswith(suffix):
            name = name[:-len(suffix)]
            break
    # systemd escapes '-' inside unit names as \x2d; restore for display.
    name = name.replace("\\x2d", "-")
    # Drop the trailing instance number so two runs of the same app group.
    head, sep, tail = name.rpartition("-")
    if sep and tail.isdigit():
        name = head
    return name


# A slice holds more than .scope units: slices and services live there too.
# Matching only app-*.scope was a real defect in the first version - it dropped
# app-graphical.slice (1384 MB anon on this host), two memguard slices, and
# omniroute.service, which together were most of the difference between the
# slice total and what the tool reported.
UNIT_SUFFIXES = (".scope", ".slice", ".service")


def sample(uid, recurse=True, include_slices=False):
    """Return {label: stats} for every readable unit, merged across roots.

    Walks into nested slices when recurse is set: app-graphical.slice holds
    Hyprland's own scopes and would otherwise be one opaque row instead of the
    processes actually inside it.
    """
    result = {}
    for root in roots(uid):
        base_depth = root.rstrip("/").count("/")
        for path, dirs, _files in os.walk(root):
            depth = path.rstrip("/").count("/") - base_depth
            if not recurse and depth > 0:
                dirs[:] = []
                continue
            if os.path.basename(path).endswith(UNIT_SUFFIXES) is False:
                continue
            stats = read_stat(path)
            if stats is None or stats.get("anon") is None:
                continue
            if include_slices is False and os.path.basename(path).endswith(".slice"):
                # A slice aggregates its children. Keeping it in the ranking
                # double-counts and, worse, makes the parent outrank the actual
                # processes: on this host `app` reported 6831 MB against a
                # largest leaf of 2551 MB, which reads as one huge consumer
                # when it is really the sum of everything below it.
                continue
            label = scope_label(path)
            # Two scopes can share a label (two Chromium instances). Keep the
            # larger one so a label appears once and its number is meaningful.
            if label in result and result[label]["anon"] >= stats["anon"]:
                continue
            result[label] = stats
    return result


def machine_anon_bytes():
    """AnonPages from /proc/meminfo, for comparison only - never apportioned."""
    try:
        with open("/proc/meminfo", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("AnonPages:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        return None
    return None


def mib(value):
    if value is None:
        return "     -"
    return "%6.1f" % (value / (1024 * 1024))


def print_ranking(title, rows, total_note=None):
    print(title)
    if not rows:
        print("  (no scopes)")
        return
    print("  %-38s %9s %9s %9s %9s" % ("scope", "anon", "file", "inact_f", "refault"))
    for label, stats, delta in rows:
        line = "  %-38s %9s %9s %9s %9s" % (
            label[:38], mib(stats.get("anon")), mib(stats.get("file")),
            mib(stats.get("inactive_file")),
            mib(stats.get("workingset_refault_file")))
        if delta is not None:
            line += "  %+9s" % mib(delta)
        print(line)
    if total_note:
        print("  " + total_note)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--window", type=int, default=60,
                        help="sample window in seconds (default 60)")
    parser.add_argument("--top", type=int, default=6,
                        help="rows per ranking (default 6)")
    args = parser.parse_args()

    uid = os.getuid()
    found = roots(uid)
    if not found:
        print("no cgroup app slice found for uid %d" % uid, file=sys.stderr)
        print("looked for:", ", ".join(t.format(uid=uid) for t in SCOPE_ROOTS),
              file=sys.stderr)
        return 2

    print("scope roots:")
    for root in found:
        print("  " + root)

    started = time.time()
    first = sample(uid)
    print("\nsampling %.0f seconds over %d scopes ..." % (args.window, len(first)))
    time.sleep(args.window)
    second = sample(uid)
    ended = time.time()

    elapsed = ended - started
    print("\nwindow: %.1f s  (%s .. %s)"
          % (elapsed,
             time.strftime("%H:%M:%S", time.localtime(started)),
             time.strftime("%H:%M:%S", time.localtime(ended))))

    # Stock ranking: whatever the counters hold at the end of the window.
    stock = sorted(second.items(), key=lambda kv: kv[1]["anon"] or 0, reverse=True)
    stock_rows = [(label, stats, None) for label, stats in stock[:args.top]]
    print()
    print_ranking("== STOCK: anonymous memory held now ==", stock_rows)

    # Growth ranking: end minus start, over scopes present at both ends.
    growth_rows = []
    for label, after in second.items():
        before = first.get(label)
        if before is None or before.get("anon") is None:
            continue
        delta = after["anon"] - before["anon"]
        growth_rows.append((label, after, delta))
    growth_rows.sort(key=lambda row: row[2], reverse=True)
    positive = [row for row in growth_rows if row[2] > 0][:args.top]
    print()
    print_ranking("== GROWTH: anon added during this window ==", positive,
                  "only scopes that grew are listed; a big but flat scope is"
                  " absent here by design")

    print()
    print("Are the two rankings the same?")
    stock_top = {label for label, _, _ in stock_rows}
    growth_top = {label for label, _, _ in positive}
    overlap = stock_top & growth_top
    if not positive:
        print("  no scope grew during the window, so the growth ranking is empty.")
    elif overlap:
        print("  overlap: %d of %d (%s)" % (len(overlap), len(growth_top),
                                            ", ".join(sorted(overlap))))
        print("  the others differ: largest now is not the same as fastest growing.")
    else:
        print("  no overlap at all. The scopes holding the most anon are not the"
              " ones that grew.")

    machine = machine_anon_bytes()
    scopes_total = sum(stats["anon"] or 0 for stats in second.values())
    print()
    print("Not to be confused with:")
    if machine is not None:
        print("  machine AnonPages      %s" % mib(machine))
    print("  sum over these scopes  %s" % mib(scopes_total))
    print("  Machine AnonPages is not the sum of these scopes, and no part of it"
          " is attributed to any one of them by this tool.")
    print("  These are cgroup charges, not process RSS.")
    print("  NOTE: the walk recurses into nested slices, so a parent slice and a")
    print("  scope inside it can both appear. The sum above therefore double-counts")
    print("  by design; it is printed to show the gap to machine AnonPages, not as")
    print("  a total.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
