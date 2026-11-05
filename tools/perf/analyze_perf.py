#!/usr/bin/env python3
"""Parse the temporary performance log lines into one JSON report.

Warm-up and steady state are reported separately.  Warm-up data is never
folded into the final numbers, but it is kept in full so unexpected calls and
start-up regressions stay visible.

Temporary engine diagnostics understood here:
  [UIGC-P] frame_ms=... t=... blocks=... threshold=...
  [UIGC-T] mode=full ms=... [path=...] [vm=...]
  [UIGC-T] mode=step ms=... [vm=...]
  [UIGC-T] mode=slice ms=... budget=... done=0|1
  [UIGC-T] mode=cycle_start/end/abort ...
"""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Any, Iterable


EVENT_RE = re.compile(r"\[UIGC-T\] (.*)")
FRAME_RE = re.compile(
    r"\[UIGC-P\] frame_ms=([0-9.]+) t=([0-9.]+) blocks=([0-9]+) threshold=([0-9]+)"
)
FULL_RE = re.compile(
    r"mode=full ms=([0-9.]+)(?: path=([A-Za-z0-9_]+))?(?: vm=(\S+))?(?: name=(\S+))?(?: thread=(\S+))?"
)
STEP_RE = re.compile(r"mode=step ms=([0-9.]+)(?: vm=(\S+))?(?: name=(\S+))?(?: thread=(\S+))?")
SLICE_RE = re.compile(
    r"mode=slice ms=([0-9.]+) budget=([0-9]+) done=([01])(?: force=([01]))?(?: vm=(\S+))?(?: name=(\S+))?"
)
CYCLE_START_RE = re.compile(
    r"mode=cycle_start path=([A-Za-z0-9_]+)(?: vm=(\S+))?(?: name=(\S+))?"
)
CYCLE_END_RE = re.compile(
    r"mode=cycle_end path=([A-Za-z0-9_]+) vm=(\S+)(?: name=(\S+))? slices=([0-9]+) ms=([0-9.]+)"
)
CYCLE_ABORT_RE = re.compile(
    r"mode=cycle_abort path=([A-Za-z0-9_]+) vm=(\S+)(?: name=(\S+))? slices=([0-9]+)"
)
VM_RE = re.compile(
    r"\[UIGC-VM\] event=(\w+) name=(.+?) vm=(\S+) blocks=(\d+) threshold=(\d+) incremental=([01])"
)
ROLE_RE = re.compile(
    r"\[UIGC-ROLE\] vm=(\S+) requested_role=(\d+) registered=([01]) run=([01]) setting=([01])"
)
HEAP_RE = re.compile(
    r"Heap:\s*([0-9.]+)\s*([KMG]?)\s*/\s*([0-9.]+)\s*([KMG]?)",
    re.IGNORECASE,
)
GAME_TIME_RE = re.compile(r"Game time:\s*([0-9]+):([0-9]+):([0-9]+)")


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    part = position - lower
    return ordered[lower] + (ordered[upper] - ordered[lower]) * part


def summarize(values: Iterable[float]) -> dict[str, Any]:
    data = list(values)
    if not data:
        return {
            "count": 0,
            "mean_ms": None,
            "median_ms": None,
            "p95_ms": None,
            "p99_ms": None,
            "p999_ms": None,
            "max_ms": None,
            "total_ms": 0.0,
        }
    return {
        "count": len(data),
        "mean_ms": sum(data) / len(data),
        "median_ms": percentile(data, 0.50),
        "p95_ms": percentile(data, 0.95),
        "p99_ms": percentile(data, 0.99),
        "p999_ms": percentile(data, 0.999),
        "max_ms": max(data),
        "total_ms": sum(data),
    }


def summarize_counts(values: Iterable[int]) -> dict[str, Any]:
    data = list(values)
    if not data:
        return {"count": 0, "mean": None, "median": None, "max": None, "total": 0}
    return {
        "count": len(data),
        "mean": sum(data) / len(data),
        "median": percentile([float(value) for value in data], 0.50),
        "max": max(data),
        "total": sum(data),
    }


def parse_heap_mb(number: str, unit: str) -> float:
    multiplier = {"": 1.0, "K": 1.0 / 1024.0, "M": 1.0, "G": 1024.0}
    return float(number) * multiplier[unit.upper()]


def parse_game_seconds(text: str) -> float | None:
    matches = GAME_TIME_RE.findall(text)
    if not matches:
        return None
    hours, minutes, seconds = matches[-1]
    return int(hours) * 3600 + int(minutes) * 60 + int(seconds)


def split(rows: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    return {
        "all": rows,
        "warmup": [row for row in rows if row["in_warmup"]],
        "steady": [row for row in rows if not row["in_warmup"]],
    }


def values(rows: list[dict[str, Any]], key: str = "ms") -> list[float]:
    return [float(row[key]) for row in rows]


def by_key(rows: list[dict[str, Any]], key: str) -> dict[str, int]:
    return {
        name: sum(1 for row in rows if row.get(key) == name)
        for name in sorted({str(row.get(key)) for row in rows})
    }


def analyze(
    log_path: Path,
    *,
    warmup_seconds: float = 10.0,
    cpu_start_seconds: float | None = None,
    cpu_warmup_seconds: float | None = None,
    cpu_end_seconds: float | None = None,
    warmup_wall_seconds: float | None = None,
    wall_seconds: float | None = None,
    return_code: int | None = None,
) -> dict[str, Any]:
    text = log_path.read_text(encoding="latin1", errors="replace")
    lines = text.splitlines()

    frames: list[dict[str, Any]] = []
    fulls: list[dict[str, Any]] = []
    steps: list[dict[str, Any]] = []
    slices: list[dict[str, Any]] = []
    cycle_starts: list[dict[str, Any]] = []
    cycle_ends: list[dict[str, Any]] = []
    cycle_aborts: list[dict[str, Any]] = []
    roles: list[dict[str, Any]] = []
    vm_events: list[dict[str, Any]] = []
    heaps: list[dict[str, Any]] = []

    for number, line in enumerate(lines):
        frame = FRAME_RE.search(line)
        if frame:
            frames.append(
                {
                    "line": number,
                    "frame_ms": float(frame.group(1)),
                    "t": float(frame.group(2)),
                    "blocks": int(frame.group(3)),
                    "threshold": int(frame.group(4)),
                }
            )

        vm_event = VM_RE.search(line)
        if vm_event:
            vm_events.append(
                {
                    "line": number,
                    "event": vm_event.group(1),
                    "name": vm_event.group(2),
                    "vm": vm_event.group(3),
                    "blocks": int(vm_event.group(4)),
                    "threshold": int(vm_event.group(5)),
                    "incremental": int(vm_event.group(6)),
                }
            )

        role = ROLE_RE.search(line)
        if role:
            roles.append(
                {
                    "line": number,
                    "vm": role.group(1),
                    "role": int(role.group(2)),
                    "registered": int(role.group(3)),
                }
            )

        event = EVENT_RE.search(line)
        if event:
            body = event.group(1)
            full = FULL_RE.search(body)
            if full:
                fulls.append(
                    {
                        "line": number,
                        "ms": float(full.group(1)),
                        "path": full.group(2) or "unknown",
                        "vm": full.group(3),
                        "name": full.group(4) or "unregistered",
                        "thread": full.group(5) or "unknown",
                    }
                )
            step = STEP_RE.search(body)
            if step:
                steps.append(
                    {
                        "line": number,
                        "ms": float(step.group(1)),
                        "vm": step.group(2),
                        "name": step.group(3) or "unregistered",
                        "thread": step.group(4) or "unknown",
                    }
                )
            slice_row = SLICE_RE.search(body)
            if slice_row:
                slices.append(
                    {
                        "line": number,
                        "ms": float(slice_row.group(1)),
                        "budget": int(slice_row.group(2)),
                        "done": int(slice_row.group(3)),
                        "force": int(slice_row.group(4) or 0),
                        "vm": slice_row.group(5),
                        "name": slice_row.group(6) or "unregistered",
                    }
                )
            start = CYCLE_START_RE.search(body)
            if start:
                cycle_starts.append(
                    {
                        "line": number,
                        "path": start.group(1),
                        "vm": start.group(2),
                        "name": start.group(3) or "unregistered",
                    }
                )
            end = CYCLE_END_RE.search(body)
            if end:
                cycle_ends.append(
                    {
                        "line": number,
                        "path": end.group(1),
                        "vm": end.group(2),
                        "name": end.group(3) or "unregistered",
                        "slices": int(end.group(4)),
                        "span_ms": float(end.group(5)),
                    }
                )
            abort = CYCLE_ABORT_RE.search(body)
            if abort:
                cycle_aborts.append(
                    {
                        "line": number,
                        "path": abort.group(1),
                        "vm": abort.group(2),
                        "name": abort.group(3) or "unregistered",
                        "slices": int(abort.group(4)),
                    }
                )

        heap = HEAP_RE.search(line)
        if heap:
            heaps.append(
                {
                    "line": number,
                    "heap_mb": parse_heap_mb(heap.group(1), heap.group(2)),
                    "capacity_mb": parse_heap_mb(heap.group(3), heap.group(4)),
                }
            )

    # Every non-frame event gets the time of the nearest preceding frame
    # sample. The log is appended in execution order, so that is an upper bound
    # on the event's own time. Events written before the first frame sample are
    # start-up and always belong to the warm-up window.
    def stamp(rows: list[dict[str, Any]]) -> None:
        cursor = 0
        current: float | None = None
        for row in sorted(rows, key=lambda item: item["line"]):
            while cursor < len(frames) and frames[cursor]["line"] <= row["line"]:
                current = frames[cursor]["t"]
                cursor += 1
            row["t"] = current
            row["in_warmup"] = current is None or current <= warmup_seconds

    for collection in (fulls, steps, slices, cycle_starts, cycle_ends, cycle_aborts, heaps, vm_events):
        stamp(collection)

    for frame in frames:
        frame["in_warmup"] = frame["t"] <= warmup_seconds

    frame_parts = split(frames)
    full_parts = split(fulls)
    step_parts = split(steps)
    slice_parts = split(slices)
    heap_parts = split(heaps)

    def frames_ms(rows: list[dict[str, Any]]) -> list[float]:
        return [row["frame_ms"] for row in rows]

    thresholds = (16.7, 33.0, 50.0, 100.0)

    def threshold_counts(rows: list[dict[str, Any]]) -> dict[str, int]:
        data = frames_ms(rows)
        return {
            f"over_{threshold:g}ms": sum(1 for value in data if value > threshold)
            for threshold in thresholds
        }

    active_all = [row for row in slices if row["done"] == 0]
    active_parts = split(active_all)

    cycle_end_parts = split(cycle_ends)
    cycle_abort_parts = split(cycle_aborts)
    cycle_start_parts = split(cycle_starts)

    def cycle_block(
        starts: list[dict[str, Any]],
        ends: list[dict[str, Any]],
        aborts: list[dict[str, Any]],
    ) -> dict[str, Any]:
        return {
            "started": len(starts),
            "completed": len(ends),
            "aborted": len(aborts),
            "active_at_end": len(starts) - len(ends) - len(aborts),
            "completed_slice_counts": summarize_counts([row["slices"] for row in ends]),
            "completed_span": summarize([row["span_ms"] for row in ends]),
            "by_path": {
                path: {
                    "started": sum(1 for row in starts if row["path"] == path),
                    "completed": sum(1 for row in ends if row["path"] == path),
                    "aborted": sum(1 for row in aborts if row["path"] == path),
                }
                for path in sorted(
                    {row["path"] for row in starts}
                    | {row["path"] for row in ends}
                    | {row["path"] for row in aborts}
                )
            },
        }

    heap_all = heap_parts["all"]
    heap_parts_out = {
        name: {
            "samples": len(rows),
            "start_mb": rows[0]["heap_mb"] if rows else None,
            "peak_mb": max((row["heap_mb"] for row in rows), default=None),
            "end_mb": rows[-1]["heap_mb"] if rows else None,
        }
        for name, rows in heap_parts.items()
    }

    # CPU is sampled by the runner. When the runner cut a sample at the warm-up
    # boundary we can separate start-up cost from steady-state cost.
    if cpu_start_seconds is not None and cpu_end_seconds is not None:
        total_cpu = max(0.0, cpu_end_seconds - cpu_start_seconds)
    else:
        total_cpu = None
    warmup_cpu = (
        max(0.0, cpu_warmup_seconds - cpu_start_seconds)
        if cpu_warmup_seconds is not None and cpu_start_seconds is not None
        else None
    )
    steady_cpu = (
        max(0.0, cpu_end_seconds - cpu_warmup_seconds)
        if cpu_warmup_seconds is not None and cpu_end_seconds is not None
        else None
    )

    frame_t_end = frames[-1]["t"] if frames else None
    steady_span = (
        max(0.0, frame_t_end - warmup_seconds)
        if frame_t_end is not None and frame_t_end > warmup_seconds
        else None
    )
    game_seconds = parse_game_seconds(text)

    full_events = [
        {
            "t": row["t"],
            "ms": row["ms"],
            "path": row["path"],
            "vm": row["vm"],
            "window": "warmup" if row["in_warmup"] else "steady",
        }
        for row in fulls
    ]
    cycle_events = [
        {
            "kind": "start",
            "t": row["t"],
            "path": row["path"],
            "window": "warmup" if row["in_warmup"] else "steady",
        }
        for row in cycle_starts
    ] + [
        {
            "kind": "end",
            "t": row["t"],
            "path": row["path"],
            "vm": row["vm"],
            "slices": row["slices"],
            "span_ms": row["span_ms"],
            "window": "warmup" if row["in_warmup"] else "steady",
        }
        for row in cycle_ends
    ] + [
        {
            "kind": "abort",
            "t": row["t"],
            "path": row["path"],
            "vm": row["vm"],
            "slices": row["slices"],
            "window": "warmup" if row["in_warmup"] else "steady",
        }
        for row in cycle_aborts
    ]
    cycle_events.sort(key=lambda item: (item["t"] if item["t"] is not None else -1.0))

    steady_frames_summary = summarize(frames_ms(frame_parts["steady"]))
    steady_full_summary = summarize(values(full_parts["steady"]))
    steady_active_slices = summarize(values(active_parts["steady"]))
    steady_cycles = cycle_block(
        cycle_start_parts["steady"], cycle_end_parts["steady"], cycle_abort_parts["steady"]
    )

    # Per-VM roll-up. Every VM ever named is listed, including the ones that
    # did no work in the steady window: a zero is a datum here, it shows the
    # collector never ran on that VM instead of hiding it.
    # Resolve one display name per VM *lifetime*, not per address: a
    # short-lived state such as "Startup" is destroyed and the same allocation
    # later holds "UserPrefs". Lifecycle events are therefore replayed in log
    # order into create/destroy segments, and only the name-bearing events of a
    # segment label it.
    segments: list[dict[str, Any]] = []
    open_segment: dict[str, dict[str, Any]] = {}
    for row in sorted(vm_events, key=lambda item: item["line"]):
        address = row["vm"]
        if row["event"] == "create":
            if address in open_segment:
                segments.append(open_segment.pop(address))
            segment = {"vm": address, "name": None, "created_t": row["t"],
                       "created_line": row["line"], "destroyed_t": None,
                       "destroyed_line": None, "peak_blocks": row["blocks"]}
            open_segment[address] = segment
            continue
        segment = open_segment.get(address)
        if segment is None:
            segment = {"vm": address, "name": None, "created_t": None,
                       "created_line": None, "destroyed_t": None,
                       "destroyed_line": None, "peak_blocks": 0}
            open_segment[address] = segment
        segment["peak_blocks"] = max(segment["peak_blocks"], row["blocks"])
        if row["event"] == "name":
            # The destroy line carries the old name; a name event always opens
            # the identity of the segment that is currently open.
            segment["name"] = row["name"]
        elif row["event"] == "destroy":
            # Close whatever segment this address currently holds. The name in
            # the event is the one it had, which is the segment being closed.
            if segment["name"] is None:
                segment["name"] = row["name"]
            segment["destroyed_t"] = row["t"]
            segment["destroyed_line"] = row["line"]
            segments.append(open_segment.pop(address))
    segments.extend(open_segment.values())

    # Map each (address, event line) to the segment that was open at that line.
    def segment_for(address: str | None, line: int) -> dict[str, Any] | None:
        if address is None:
            return None
        best = None
        for segment in segments:
            if segment["vm"] != address:
                continue
            start_line = segment.get("start_line", -1)
            end_line = segment.get("end_line", 1 << 60)
            if start_line <= line <= end_line:
                return segment
            best = segment
        return best

    # Line range owned by each segment. A missing bound means "open".
    for segment in segments:
        segment["start_line"] = (
            segment["created_line"] if segment["created_line"] is not None else -1
        )
        segment["end_line"] = (
            segment["destroyed_line"] if segment["destroyed_line"] is not None else 1 << 60
        )

    # A GC row belongs to the segment open at its line; fall back to the last
    # segment for that address when the log is truncated before a destroy.
    def name_for(vm: str | None, line: int) -> str:
        segment = segment_for(vm, line)
        if segment is not None and segment["name"]:
            return segment["name"]
        return "unregistered"

    vm_names = sorted({segment["name"] for segment in segments if segment["name"]})
    for row in fulls + steps + slices + cycle_ends + cycle_starts + cycle_aborts:
        if not row.get("name"):
            row["name"] = name_for(row.get("vm"), row["line"])

    vm_table = {}
    for name in vm_names:
        owned = [segment for segment in segments if segment["name"] == name]
        full_steady_rows = [
            r for r in fulls if r["name"] == name and not r["in_warmup"]
        ]
        full_warmup_rows = [r for r in fulls if r["name"] == name and r["in_warmup"]]
        active_steady_rows = [
            r for r in active_all if r["name"] == name and not r["in_warmup"]
        ]
        ends_steady = [
            r for r in cycle_ends if r["name"] == name and not r["in_warmup"]
        ]
        vm_table[name] = {
            "incremental": any(
                r["incremental"] == 1 for r in vm_events if r["vm"] in {sg["vm"] for sg in owned}
            ),
            "lifetimes": len(owned),
            "created": sum(1 for segment in owned if segment["created_line"] is not None),
            "destroyed": sum(
                1 for segment in owned if segment["destroyed_line"] is not None
            ),
            "alive_at_end": sum(
                1 for segment in owned if segment["destroyed_t"] is None
            ),
            "first_seen_t": min(
                (segment["created_t"] for segment in owned
                 if segment["created_t"] is not None),
                default=None,
            ),
            "peak_blocks": max(
                (segment["peak_blocks"] for segment in owned), default=0
            ),
            "full_all": summarize([r["ms"] for r in fulls if r["name"] == name]),
            "full_warmup": summarize([r["ms"] for r in full_warmup_rows]),
            "full_steady": summarize([r["ms"] for r in full_steady_rows]),
            "active_slices_steady": summarize([r["ms"] for r in active_steady_rows]),
            "cycles_started": sum(1 for r in cycle_starts if r["name"] == name),
            "cycles_completed_steady": len(ends_steady),
            "cycles_aborted": sum(1 for r in cycle_aborts if r["name"] == name),
        }

    # GC activity on a VM whose name was never resolved (truncated log, or a
    # state destroyed before NameLuaVm). Report it rather than dropping it.
    unowned = {}
    for row in fulls + active_all + cycle_ends:
        if row["name"] == "unregistered" and row.get("vm"):
            unowned.setdefault(row["vm"], {"full": [], "slice": [], "cycles": 0})
            if row is not None and "ms" in row:
                if row in fulls:
                    unowned[row["vm"]]["full"].append(row["ms"])
                elif row in active_all:
                    unowned[row["vm"]]["slice"].append(row["ms"])
    if unowned:
        vm_table["unregistered"] = {
            "incremental": False,
            "lifetimes": len(unowned),
            "created": 0,
            "destroyed": 0,
            "alive_at_end": 0,
            "first_seen_t": None,
            "peak_blocks": 0,
            "full_all": summarize([v for e in unowned.values() for v in e["full"]]),
            "full_warmup": summarize([]),
            "full_steady": summarize([]),
            "active_slices_steady": summarize([]),
            "cycles_started": 0,
            "cycles_completed_steady": 0,
            "cycles_aborted": 0,
        }

    return {
        "input": {
            "log": str(log_path),
            "warmup_seconds": warmup_seconds,
            "wall_seconds": wall_seconds,
            "warmup_wall_seconds": warmup_wall_seconds,
            "return_code": return_code,
        },
        "vms": [{**row, "is_ui_role": row["role"] in (1, 2)} for row in roles],
        "vm_table": vm_table,
        "vm_events": [
            {k: v for k, v in row.items() if k != "line"} for row in vm_events
        ],
        "activity": {
            "frames_all": len(frames),
            "frames_warmup": len(frame_parts["warmup"]),
            "frames_steady": len(frame_parts["steady"]),
            "frame_t_start": frames[0]["t"] if frames else None,
            "frame_t_end": frame_t_end,
            "steady_span_seconds": steady_span,
            "game_seconds": game_seconds,
        },
        "frame": {
            "all": summarize(frames_ms(frame_parts["all"])),
            "warmup": summarize(frames_ms(frame_parts["warmup"])),
            "steady": steady_frames_summary,
            "thresholds_warmup": threshold_counts(frame_parts["warmup"]),
            "thresholds_steady": threshold_counts(frame_parts["steady"]),
        },
        "full": {
            "all": summarize(values(full_parts["all"])),
            "warmup": summarize(values(full_parts["warmup"])),
            "steady": steady_full_summary,
            "by_name_all": {
                name: summarize(values([row for row in fulls if row["name"] == name]))
                for name in sorted({row["name"] for row in fulls})
            },
            "by_name_steady": {
                name: summarize(
                    values([row for row in full_parts["steady"] if row["name"] == name])
                )
                for name in sorted({row["name"] for row in fulls})
            },
            "by_name_warmup": {
                name: summarize(
                    values([row for row in full_parts["warmup"] if row["name"] == name])
                )
                for name in sorted({row["name"] for row in fulls})
            },
            "by_thread_steady": {
                thread: summarize(
                    values([row for row in full_parts["steady"] if row["thread"] == thread])
                )
                for thread in sorted({row["thread"] for row in fulls})
            },
            "by_vm_all": {
                vm: summarize(values([row for row in fulls if row["vm"] == vm]))
                for vm in sorted({str(row["vm"]) for row in fulls})
            },
            "by_vm_warmup": {
                vm: summarize(
                    values([row for row in full_parts["warmup"] if row["vm"] == vm])
                )
                for vm in sorted({str(row["vm"]) for row in fulls})
            },
            "by_vm_steady": {
                vm: summarize(
                    values([row for row in full_parts["steady"] if row["vm"] == vm])
                )
                for vm in sorted({str(row["vm"]) for row in fulls})
            },
            "paths_warmup": by_key(full_parts["warmup"], "path"),
            "paths_steady": by_key(full_parts["steady"], "path"),
            "events": full_events,
        },
        "steps": {
            "all": summarize(values(step_parts["all"])),
            "warmup": summarize(values(step_parts["warmup"])),
            "steady": summarize(values(step_parts["steady"])),
            "by_name_steady": {
                name: summarize(
                    values([row for row in step_parts["steady"] if row["name"] == name])
                )
                for name in sorted({row["name"] for row in steps})
            },
        },
        "slices": {
            "all": summarize(values(slice_parts["all"])),
            "active_all": summarize(values(active_parts["all"])),
            "active_warmup": summarize(values(active_parts["warmup"])),
            "active_steady": steady_active_slices,
            "active_count_steady": len(active_parts["steady"]),
            "noop_count_steady": len(slice_parts["steady"]) - len(active_parts["steady"]),
            "budgets": sorted({row["budget"] for row in slices}),
            "by_name_steady": {
                name: summarize(
                    values(
                        [
                            row
                            for row in active_parts["steady"]
                            if row["name"] == name
                        ]
                    )
                )
                for name in sorted({row["name"] for row in slices})
            },
            "force_steady": sum(1 for row in active_parts["steady"] if row["force"] == 1),
            "threshold_started_steady": sum(
                1 for row in active_parts["steady"] if row["force"] == 0
            ),
        },
        "cycles": {
            "all": cycle_block(cycle_starts, cycle_ends, cycle_aborts),
            "warmup": cycle_block(
                cycle_start_parts["warmup"],
                cycle_end_parts["warmup"],
                cycle_abort_parts["warmup"],
            ),
            "steady": steady_cycles,
            "steady_by_name": {
                name: {
                    "started": sum(
                        1 for row in cycle_start_parts["steady"] if row["name"] == name
                    ),
                    "completed": sum(
                        1 for row in cycle_end_parts["steady"] if row["name"] == name
                    ),
                    "aborted": sum(
                        1 for row in cycle_abort_parts["steady"] if row["name"] == name
                    ),
                }
                for name in sorted(
                    {row["name"] for row in cycle_ends}
                    | {row["name"] for row in cycle_aborts}
                )
            },
            "steady_per_game_minute": (
                steady_cycles["completed"] / (game_seconds / 60.0)
                if game_seconds and game_seconds > 0
                else None
            ),
            "events": cycle_events,
        },
        "heap": {
            **heap_parts_out,
            "blocks_start": frames[0]["blocks"] if frames else None,
            "blocks_peak": max((row["blocks"] for row in frames), default=None),
            "blocks_end": frames[-1]["blocks"] if frames else None,
            "threshold_end": frames[-1]["threshold"] if frames else None,
            "blocks_peak_steady": max(
                (row["blocks"] for row in frame_parts["steady"]), default=None
            ),
        },
        "cpu": {
            "total_seconds": total_cpu,
            "warmup_seconds": warmup_cpu,
            "steady_seconds": steady_cpu,
            "steady_per_minute": (
                steady_cpu / (steady_span / 60.0)
                if steady_cpu is not None and steady_span and steady_span > 0
                else None
            ),
        },
        # Headline numbers. Warm-up is deliberately absent from this block.
        "final": {
            "window": "steady",
            "frames": steady_frames_summary,
            "frame_thresholds": threshold_counts(frame_parts["steady"]),
            "full": steady_full_summary,
            "full_paths": by_key(full_parts["steady"], "path"),
            "active_slices": steady_active_slices,
            "cycles": steady_cycles,
            "heap_peak_mb": heap_parts_out["steady"]["peak_mb"],
            "cpu_seconds": steady_cpu,
            "cpu_per_minute": (
                steady_cpu / (steady_span / 60.0)
                if steady_cpu is not None and steady_span and steady_span > 0
                else None
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--warmup", type=float, default=10.0)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = analyze(args.log, warmup_seconds=args.warmup)
    payload = json.dumps(result, indent=2, sort_keys=True)
    if args.out:
        args.out.write_text(payload + "\n", encoding="utf-8")
    else:
        print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
