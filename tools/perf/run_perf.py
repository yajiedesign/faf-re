#!/usr/bin/env python3
"""Run one fixed-duration UI-GC performance sample and write a JSON summary.

The engine must contain the temporary [UIGC-P]/[UIGC-T] diagnostics.  This
runner is deliberately path-agnostic: pass the exe, init script, replay and log
paths explicitly so it can be reused from another checkout or branch.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

from analyze_perf import analyze


PROCESS_QUERY_INFORMATION = 0x0400


def process_cpu_seconds(pid: int) -> float | None:
    """Return user+kernel CPU seconds for a Windows process, if permitted."""
    if os.name != "nt":
        return None
    try:
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION, False, pid)
        if not handle:
            return None
        try:
            creation = wintypes.FILETIME()
            exit_time = wintypes.FILETIME()
            kernel = wintypes.FILETIME()
            user = wintypes.FILETIME()
            ok = kernel32.GetProcessTimes(
                handle,
                ctypes.byref(creation),
                ctypes.byref(exit_time),
                ctypes.byref(kernel),
                ctypes.byref(user),
            )
            if not ok:
                return None

            def to_100ns(value: wintypes.FILETIME) -> int:
                return (value.dwHighDateTime << 32) | value.dwLowDateTime

            return (to_100ns(kernel) + to_100ns(user)) / 10_000_000.0
        finally:
            kernel32.CloseHandle(handle)
    except Exception:
        return None


def replay_id(replay: Path) -> str:
    match = re.match(r"(\d+)", replay.stem)
    return match.group(1) if match else replay.stem


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--init", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--switch", choices=("0", "1"), required=True)
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--simworkers", type=int, default=8)
    parser.add_argument("--warmup", type=float, default=10.0)
    parser.add_argument("--title", default="perf")
    args = parser.parse_args()

    exe = args.exe.resolve()
    init = args.init.resolve()
    replay = args.replay.resolve()
    log = args.log.resolve()
    out = args.out.resolve()
    if not exe.is_file():
        parser.error(f"exe not found: {exe}")
    if not init.is_file():
        parser.error(f"init not found: {init}")
    if not replay.is_file():
        parser.error(f"replay not found: {replay}")
    out.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)

    command = [
        str(exe),
        "/init",
        str(init),
        "/nobugreport",
        "/replay",
        str(replay),
        "/replayid",
        replay_id(replay),
        "/nosound",
        "/nomovie",
        "/windowed",
        "1280",
        "720",
        "/log",
        str(log),
        "/title",
        args.title,
        "/simworkers",
        str(args.simworkers),
        "/con",
        f"lua_IncrementalGC={args.switch}",
    ]
    env = os.environ.copy()
    env["FAF_UIGC_TIMING"] = "1"
    env["FAF_UIGC_TRACE"] = "1"

    print("launch:", " ".join(command), flush=True)
    started_wall = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=str(exe.parent),
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    cpu_start = process_cpu_seconds(process.pid)
    warmup_boundary = min(args.warmup, float(args.seconds))
    time.sleep(warmup_boundary)
    cpu_warmup = process_cpu_seconds(process.pid)
    time.sleep(max(0.0, args.seconds - warmup_boundary))
    cpu_end = process_cpu_seconds(process.pid)
    process.kill()
    try:
        return_code = process.wait(timeout=30)
    except subprocess.TimeoutExpired:
        return_code = None
    wall_seconds = time.monotonic() - started_wall

    if not log.is_file():
        print(f"error: engine log was not created: {log}", file=sys.stderr)
        return 2

    result = analyze(
        log,
        warmup_seconds=args.warmup,
        cpu_start_seconds=cpu_start,
        cpu_warmup_seconds=cpu_warmup,
        cpu_end_seconds=cpu_end,
        warmup_wall_seconds=warmup_boundary,
        wall_seconds=wall_seconds,
        return_code=return_code,
    )
    result["run"] = {
        "exe": str(exe),
        "arch": "x64" if "x64" in exe.stem.lower() else "x86",
        "init": str(init),
        "replay": str(replay),
        "log": str(log),
        "switch": int(args.switch),
        "seconds_requested": args.seconds,
        "seconds_elapsed": wall_seconds,
        "warmup_seconds": args.warmup,
        "simworkers": args.simworkers,
        "cpu_start_seconds": cpu_start,
        "cpu_warmup_seconds": cpu_warmup,
        "cpu_end_seconds": cpu_end,
        "return_code": return_code,
    }
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {out}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
