#!/usr/bin/env python3
"""Same-host control experiment for the balanced-clean policy.

Runs a discardable workload inside its own throwaway cgroup and measures the
five things the policy claims to help, while doing nothing to it (the control
arm). A treated arm is only meaningful against this baseline; without one, any
later drop in a number can be mistaken for benefit.

Nothing global is touched: no drop_caches, no sysctl, no swap/ZRAM change, no
other process is signalled. The workload exits on its own.
"""
import json, os, signal, subprocess, sys, time

BASE = "/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/app.slice"
CG = os.path.join(BASE, "q12-control.scope")

WORKLOAD = r'''
import mmap, os, sys, time
mib = int(sys.argv[1]); hold = float(sys.argv[2])
# A v8-shaped workload: allocate anonymous memory and touch every page, the
# way a renderer's heap actually becomes resident.
buf = bytearray(mib * 1024 * 1024)
for i in range(0, len(buf), 4096):
    buf[i] = 1
print("ready", flush=True)
time.sleep(hold)
'''

def read(path):
    with open(path) as fh:
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
        key = parts[0]
        vals = {}
        for p in parts[1:]:
            k, v = p.split("=")
            vals[k] = v
        out[key] = vals
    return out

def vmstat():
    out = {}
    for line in read("/proc/vmstat").splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] in (
            "pswpin", "pswpout", "pgmajfault",
            "workingset_refault_anon", "workingset_refault_file",
            "pgscan_direct", "pgsteal_direct"):
            out[parts[0]] = int(parts[1])
    return out

def cg_stat(scope):
    out = {}
    for line in read(os.path.join(scope, "memory.stat")).splitlines():
        parts = line.split()
        if len(parts) == 2:
            out[parts[0]] = int(parts[1])
    out["current"] = int(read(os.path.join(scope, "memory.current")))
    try:
        out["swap_current"] = int(read(os.path.join(scope, "memory.swap.current")))
    except OSError:
        out["swap_current"] = -1
    return out

def sample(scope):
    return {"t": time.time(), "meminfo": meminfo(), "psi": psi(),
            "vmstat": vmstat(), "cg": cg_stat(scope)}

def main():
    scope = CG
    if os.path.isdir(scope):
        os.rmdir(scope)
    os.makedirs(scope)
    child = None
    samples = []
    try:
        child = subprocess.Popen([sys.executable, "-c", WORKLOAD, "600", "40"],
                                 stdout=subprocess.PIPE, text=True)
        with open(os.path.join(scope, "cgroup.procs"), "w") as fh:
            fh.write(str(child.pid))
        line = child.stdout.readline()
        if "ready" not in line:
            print("workload failed:", line, file=sys.stderr)
            return 1
        # Control arm: 12 samples over ~24 s, no intervention of any kind.
        for i in range(12):
            samples.append(sample(scope))
            time.sleep(2)
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

    first, last = samples[0], samples[-1]
    def d(a, b):
        return b - a
    report = {
        "arm": "control_no_action",
        "samples": len(samples),
        "span_s": round(last["t"] - first["t"], 2),
        "memavailable_delta_bytes": d(first["meminfo"]["MemAvailable"], last["meminfo"]["MemAvailable"]),
        "memfree_delta_bytes": d(first["meminfo"]["MemFree"], last["meminfo"]["MemFree"]),
        "cached_delta_bytes": d(first["meminfo"]["Cached"], last["meminfo"]["Cached"]),
        "cg_current_delta_bytes": d(first["cg"]["current"], last["cg"]["current"]),
        "cg_anon_delta_bytes": d(first["cg"]["anon"], last["cg"]["anon"]),
        "cg_file_delta_bytes": d(first["cg"]["file"], last["cg"]["file"]),
        "psi_some_us_delta": d(int(first["psi"]["some"]["total"]), int(last["psi"]["some"]["total"])),
        "psi_full_us_delta": d(int(first["psi"]["full"]["total"]), int(last["psi"]["full"]["total"])),
        "pswpin_delta": d(first["vmstat"]["pswpin"], last["vmstat"]["pswpin"]),
        "pswpout_delta": d(first["vmstat"]["pswpout"], last["vmstat"]["pswpout"]),
        "refault_anon_delta": d(first["vmstat"]["workingset_refault_anon"], last["vmstat"]["workingset_refault_anon"]),
        "refault_file_delta": d(first["vmstat"]["workingset_refault_file"], last["vmstat"]["workingset_refault_file"]),
        "series": [{"t": round(s["t"] - first["t"], 1),
                    "available_mib": round(s["meminfo"]["MemAvailable"] / 1048576, 1),
                    "cg_current_mib": round(s["cg"]["current"] / 1048576, 1)} for s in samples],
    }
    print(json.dumps(report, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
