#!/usr/bin/env python3
"""Same-host treated arm: file-first reclaim on a discardable cgroup.

The workload deliberately loads *file-backed* page cache in its own cgroup, so
the reclaim has eligible cold file pages to take. That is the condition under
which the policy claims a benefit; running it against an anonymous-only cgroup
would test nothing but the refusal path.

Measures the same fields as the control arm, so the two are directly comparable.
Nothing global: no drop_caches, no sysctl, no swap/ZRAM change, nothing signalled
outside this cgroup.
"""
import json, os, subprocess, sys, time

BASE = "/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/app.slice"
CG = os.path.join(BASE, "q12-treated.scope")
CACHE = "/home/arlune_innersea/q12-evidence/filecache.bin"

WORKLOAD = r'''
import os, sys, time
path = sys.argv[1]; mib = int(sys.argv[2]); hold = float(sys.argv[3])
fd = os.open(path, os.O_RDONLY)
try:
    left = mib * 1024 * 1024
    while left > 0:
        chunk = os.read(fd, min(1 << 20, left))
        if not chunk:
            break
        left -= len(chunk)
finally:
    os.close(fd)
print("ready", flush=True)
time.sleep(hold)
'''

def read(p):
    with open(p) as fh:
        return fh.read()

def meminfo():
    out = {}
    for line in read("/proc/meminfo").splitlines():
        parts = line.split()
        if len(parts) >= 2:
            out[parts[0].rstrip(":")] = int(parts[1]) * 1024
    return out

def psi():
    out = {}
    for line in read("/proc/pressure/memory").splitlines():
        parts = line.split()
        out[parts[0]] = {p.split("=")[0]: p.split("=")[1] for p in parts[1:]}
    return out

def vmstat():
    keep = {"pswpin","pswpout","pgmajfault","workingset_refault_anon",
            "workingset_refault_file","pgscan_direct","pgsteal_direct"}
    out = {}
    for line in read("/proc/vmstat").splitlines():
        p = line.split()
        if len(p) == 2 and p[0] in keep:
            out[p[0]] = int(p[1])
    return out

def cg(scope):
    out = {}
    for line in read(os.path.join(scope, "memory.stat")).splitlines():
        p = line.split()
        if len(p) == 2:
            out[p[0]] = int(p[1])
    out["current"] = int(read(os.path.join(scope, "memory.current")))
    try:
        out["swap_current"] = int(read(os.path.join(scope, "memory.swap.current")))
    except OSError:
        out["swap_current"] = -1
    return out

def sample(scope):
    return {"t": time.time(), "meminfo": meminfo(), "psi": psi(),
            "vmstat": vmstat(), "cg": cg(scope)}

def reclaim(scope, value):
    p = os.path.join(scope, "memory.reclaim")
    fd = os.open(p, os.O_WRONLY)
    try:
        os.write(fd, value.encode())
        return None
    except OSError as e:
        return e.errno
    finally:
        os.close(fd)

def main():
    with open(CACHE, "wb") as fh:
        fh.write(os.urandom(400 * 1024 * 1024))
    scope = CG
    if os.path.isdir(scope):
        os.rmdir(scope)
    os.makedirs(scope)
    child = None
    samples = []
    batches = []
    try:
        child = subprocess.Popen([sys.executable, "-c", WORKLOAD, CACHE, "350", "45"],
                                 stdout=subprocess.PIPE, text=True)
        with open(os.path.join(scope, "cgroup.procs"), "w") as fh:
            fh.write(str(child.pid))
        if "ready" not in child.stdout.readline():
            print("workload failed", file=sys.stderr)
            return 1
        time.sleep(1.0)
        # Baseline samples before any action.
        for _ in range(3):
            samples.append(sample(scope))
            time.sleep(1.0)
        baseline = samples[-1]
        # Treated: 8 batches of 32 MiB, swappiness=0, the exact production form.
        for i in range(8):
            err = reclaim(scope, "33554432 swappiness=0")
            batches.append({"n": i + 1, "errno": err})
            time.sleep(0.25)
        samples.append(sample(scope))
        # Immediate, 30 s and multi-minute observations.
        for delay, label in ((30, "30s"), (90, "120s"), (120, "240s")):
            time.sleep(delay)
            s = sample(scope)
            s["label"] = label
            samples.append(s)
    finally:
        if child is not None:
            try:
                child.kill(); child.wait(timeout=5)
            except Exception:
                pass
        try:
            os.rmdir(scope)
        except OSError:
            pass

    base = baseline
    def d(f):
        return {s.get("label", "post"): f(s) - f(base) for s in samples[3:]}
    report = {
        "arm": "treated_file_first",
        "batches": batches,
        "batch_errors": [b["errno"] for b in batches if b["errno"] is not None],
        "cg_file_delta_bytes": d(lambda s: s["cg"]["file"]),
        "cg_anon_delta_bytes": d(lambda s: s["cg"]["anon"]),
        "cg_current_delta_bytes": d(lambda s: s["cg"]["current"]),
        "cg_swap_delta_bytes": d(lambda s: s["cg"]["swap_current"]),
        "memavailable_delta_bytes": d(lambda s: s["meminfo"]["MemAvailable"]),
        "psi_some_us_delta": d(lambda s: int(s["psi"]["some"]["total"])),
        "psi_full_us_delta": d(lambda s: int(s["psi"]["full"]["total"])),
        "pswpin_delta": d(lambda s: s["vmstat"]["pswpin"]),
        "pswpout_delta": d(lambda s: s["vmstat"]["pswpout"]),
        "baseline_cg_file_mib": round(base["cg"]["file"] / 1048576, 1),
        "baseline_cg_current_mib": round(base["cg"]["current"] / 1048576, 1),
    }
    print(json.dumps(report, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
