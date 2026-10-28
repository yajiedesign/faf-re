#!/usr/bin/env python3
"""
fafdiff :: gen_coverage.py

Generate `tools/fafdiff/COVERAGE.md` -- the per-function coverage ledger for
the x87-tier math surface of the engine rebuild.

Inputs:
    <repo>/.tmp/fafdiff/screen.json        (produced by scripts/screen_math.py)
    <repo>/tools/fafdiff/data/coverage_status.json  (hand-maintained status)

Output:
    <repo>/tools/fafdiff/COVERAGE.md

**Do not edit COVERAGE.md by hand** -- it is regenerated from the two inputs
above.  Edit `data/coverage_status.json` instead and re-run this script.

Usage:

    PYTHONUTF8=1 .tmp/venv/Scripts/python.exe tools/fafdiff/scripts/gen_coverage.py
    PYTHONUTF8=1 .tmp/venv/Scripts/python.exe tools/fafdiff/scripts/gen_coverage.py --init-status

The script is deterministic: the same inputs always produce a byte-identical
COVERAGE.md.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

# repo root: <root>/tools/fafdiff/scripts/gen_coverage.py -> up three levels
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))

DEFAULT_SCREEN = os.path.join(REPO_ROOT, ".tmp", "fafdiff", "screen.json")
DEFAULT_STATUS = os.path.join(REPO_ROOT, "tools", "fafdiff", "data", "coverage_status.json")
DEFAULT_OUT = os.path.join(REPO_ROOT, "tools", "fafdiff", "COVERAGE.md")

# --------------------------------------------------------------------------
# the ledger set -- ONE place, so the denominator cannot drift silently
# --------------------------------------------------------------------------

# The exact predicate, in the order it is applied:
#
#     tier == "x87"
#     and section_is_code
#     and x87_to_first_ret > 0
#     and file under src/sdk/moho/math/** or src/sdk/legacy/math/**
#     and not MathReflection.cpp
#     and not **/backends/** and not *D3D*
#
# `calls <= 3` is NOT part of the set: on the current tree it removes 11 rows
# (SLERP, QuatsNearEqual, MatrixColumnsToQuatCanonical, BuildTiltShortestArcDelta,
# BuildRotationMatrixFromEulerHPR and six inline copies in QuaternionMath.h) and
# yields 184 rows, not the 195 this ledger is specified to hold.  Set
# CALLS_LIMIT to 3 to apply it; the count assertion below then fails loudly,
# which is the point.
CALLS_LIMIT = None  # None = not applied; 3 = apply the `calls <= 3` clause

EXPECTED_SET_SIZE = 195

MATH_DIRS = ("src/sdk/moho/math/", "src/sdk/legacy/math/")
EXCLUDED_FILES = ("MathReflection.cpp",)
EXCLUDED_PATH_PARTS = ("/backends/",)
EXCLUDED_NAME_PARTS = ("D3D",)

LEDGER_PREDICATE_TEXT = (
    "tier == 'x87' AND section_is_code AND x87_to_first_ret > 0 AND "
    "(file starts with 'src/sdk/moho/math/' OR 'src/sdk/legacy/math/') AND "
    "NOT file ends with 'MathReflection.cpp' AND "
    "NOT '/backends/' in file AND NOT 'D3D' in file"
    + (" AND calls <= %d" % CALLS_LIMIT if CALLS_LIMIT is not None else "")
)

# --------------------------------------------------------------------------
# statuses
# --------------------------------------------------------------------------

STATUS_UNMATCHED = "未匹配"
STATUS_MATCHED = "已匹配"
STATUS_FAILED = "失败"
STATUS_NA = "不适用"

STATUSES = (STATUS_UNMATCHED, STATUS_MATCHED, STATUS_FAILED, STATUS_NA)
STATUSES_NEEDING_WHY = (STATUS_NA, STATUS_FAILED)

DEFAULT_NA_WHY = "difffuzz already covers it; fafdiff adds no dimension"


def in_ledger(r, calls_limit=CALLS_LIMIT) -> bool:
    """The one predicate that decides the ledger set."""
    if r.get("tier") != "x87":
        return False
    if not r.get("section_is_code"):
        return False
    if r.get("x87_to_first_ret", 0) <= 0:
        return False
    f = (r.get("file") or "").replace("\\", "/")
    if not f.startswith(MATH_DIRS):
        return False
    if any(f.endswith(x) for x in EXCLUDED_FILES):
        return False
    if any(p in f for p in EXCLUDED_PATH_PARTS):
        return False
    if any(p in f for p in EXCLUDED_NAME_PARTS):
        return False
    if calls_limit is not None and r.get("calls", 0) > calls_limit:
        return False
    return True


# --------------------------------------------------------------------------
# source-level name recovery
# --------------------------------------------------------------------------

RE_IDENT_CALL = re.compile(
    r"(?P<name>(?:[A-Za-z_][A-Za-z0-9_]*::)*(?:~?[A-Za-z_][A-Za-z0-9_]*"
    r"|operator[^(\s]*))\s*\("
)

RE_DOCTITLE = re.compile(r"^\s*\*\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*:")

RE_IDA_NAME = re.compile(r"^(?:sub|func|fun|x)[_]?[0-9A-Fa-f]{4,}$")

# an IDA-style name in the screen data: `sub_6D1E30`, `func_QuatLERP`,
# `fun_RotateQuat`, `callcnv_F3`, `x00BCB540`
RE_IDA_STYLE = re.compile(r"^(?:sub|func|fun|callcnv)_|^x[0-9A-Fa-f]{6,}$")


class SourceFile:
    """A source file with a per-line 'is this code?' mask."""

    def __init__(self, path):
        self.path = path
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            self.lines = fh.read().splitlines()
        self.is_code = []
        in_block = False
        for raw in self.lines:
            s = raw.strip()
            if in_block:
                self.is_code.append(False)
                if "*/" in s:
                    in_block = False
                continue
            if s.startswith("/*"):
                if "*/" not in s:
                    in_block = True
                self.is_code.append(False)
                continue
            if s.startswith("//") or s.startswith("*"):
                self.is_code.append(False)
                continue
            self.is_code.append(True)


class NameResolver:
    """Recovers the source-level name of an annotated function."""

    def __init__(self, repo_root):
        self.repo_root = repo_root
        self.cache = {}

    def _file(self, rel):
        if rel not in self.cache:
            p = os.path.join(self.repo_root, rel.replace("/", os.sep))
            try:
                self.cache[rel] = SourceFile(p)
            except OSError:
                self.cache[rel] = None
        return self.cache[rel]

    def resolve(self, rel, line):
        """Return (display_name, kind, recovered).

        `display_name` is the C++ name with its parameter list; `kind` is
        `function`, `variable` or `unknown`; `recovered` is False when only
        the screen symbol could be used.
        """
        sf = self._file(rel)
        if sf is None:
            return None, "unknown", False
        start = max(0, line - 1)
        # 1. the first code line after the annotation
        for i in range(start, min(len(sf.lines), start + 160)):
            if not sf.is_code[i]:
                continue
            sig = self._signature(sf, i)
            if sig is None:
                continue
            name, params, kind = sig
            if kind == "variable":
                title = self._doctitle(sf, i)
                if title:
                    return "%s (dynamic initializer)" % title, "variable", True
                return "%s (dynamic initializer)" % name, "variable", True
            return "%s(%s)" % (name, params), "function", True
        return None, "unknown", False

    def _signature(self, sf, i):
        """Parse the declaration that starts on code line `i`."""
        text = sf.lines[i]
        m = RE_IDENT_CALL.search(text)
        if not m:
            return None
        name = m.group("name")
        # the identifier must not be a keyword-ish word
        if name in ("if", "for", "while", "switch", "return", "sizeof", "catch"):
            return None
        # gather the parameter list, possibly spanning several lines
        depth = 0
        buf = []
        j = i
        end = None
        while j < len(sf.lines) and j < i + 40:
            line = sf.lines[j]
            k = j if j == i else None
            start_at = m.end() - 1 if j == i else 0
            for ch in line[start_at:]:
                if ch == "(":
                    depth += 1
                    if depth == 1:
                        continue
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        end = j
                        break
                if depth >= 1:
                    buf.append(ch)
            if end is not None:
                break
            j += 1
        if end is None:
            return None
        params = " ".join("".join(buf).split())
        # what follows the closing paren?
        tail = sf.lines[end]
        close = tail.rfind(")")
        tail = tail[close + 1:].strip()
        tail = re.sub(r"^(const|noexcept|override|final|constexpr|mutable|volatile)\b", "", tail).strip()
        is_var = False
        if tail.startswith("=") or tail.startswith("{"):
            is_var = True
        elif tail.startswith(";"):
            # a declaration: `static const T name(...)` is a variable with an
            # initializer, a plain prototype is a function
            head = sf.lines[i][:m.start()]
            if "static" in head or "const" in head:
                is_var = True
            if re.search(r"\d", params):
                is_var = True
        if is_var:
            return (name, params, "variable")
        return (name, params, "function")

    def _doctitle(self, sf, i):
        """The `* name: ...` title of the doc block that owns line `i`."""
        # walk back to the start of the comment block, then take its title
        for j in range(i - 1, max(-1, i - 200), -1):
            s = sf.lines[j].strip()
            if s.startswith("/**"):
                for k in range(j + 1, min(len(sf.lines), j + 6)):
                    m = RE_DOCTITLE.match(sf.lines[k])
                    if m:
                        return m.group("name")
                return None
            if s.startswith("*") and RE_DOCTITLE.match(sf.lines[j]):
                name = RE_DOCTITLE.match(sf.lines[j]).group("name")
                if name != "Address":
                    return name
        return None


# --------------------------------------------------------------------------
# markdown helpers
# --------------------------------------------------------------------------

def md_cell(text) -> str:
    return (text or "").replace("|", "\\|").replace("\n", " ")


def build_markdown(rows, statuses, src_commit, screen_path, status_path,
                   dropped_by_calls=()):
    counts = {s: 0 for s in STATUSES}
    for a in statuses:
        counts[statuses[a]["status"]] += 1

    out = []
    out.append("# fafdiff :: coverage ledger")
    out.append("")
    out.append("Per-function coverage ledger for the x87-tier math surface of the engine")
    out.append("rebuild (`src/sdk/moho/math/**`, `src/sdk/legacy/math/**`) as it exists")
    out.append("in the retail image.")
    out.append("")
    out.append("| item | value |")
    out.append("|---|---|")
    out.append("| functions in the ledger | **%d** |" % len(rows))
    out.append("| `src/sdk` commit | `%s` |" % src_commit)
    out.append("| screen data | `%s` |" % os.path.relpath(screen_path, REPO_ROOT).replace("\\", "/"))
    out.append("| status data | `%s` |" % os.path.relpath(status_path, REPO_ROOT).replace("\\", "/"))
    out.append("| statuses | %s |" % ", ".join("`%s` %d" % (s, counts[s]) for s in STATUSES))
    out.append("")
    out.append("A row's status is one of four:")
    out.append("")
    out.append("* `%s` -- not paired yet." % STATUS_UNMATCHED)
    out.append("* `%s` -- paired, bit-for-bit identical to retail, with both mutation "
               "directions witnessed (see below)." % STATUS_MATCHED)
    out.append("* `%s` -- paired, but the two sides differ (or the pair does not "
               "discriminate); the reason is in `tools/fafdiff/README.md`." % STATUS_FAILED)
    out.append("* `%s` -- deliberately out of scope; the reason is listed below." % STATUS_NA)
    out.append("")
    out.append("## The ledger")
    out.append("")
    out.append("| # | function | address | source | status |")
    out.append("|---:|---|---|---|---|")
    for i, r in enumerate(rows, 1):
        out.append("| %d | `%s` | `%s` | `%s` | %s |" % (
            i, md_cell(r["display_name"]), r["address"],
            md_cell("%s:%d" % (r["file"], r["line"])), r["status"]))
    out.append("")

    # ------------------------------------------------------ 1. why 不适用
    out.append("## Why a row is %s" % STATUS_NA)
    out.append("")
    na_rows = [r for r in rows if r["status"] == STATUS_NA]
    if na_rows:
        for r in na_rows:
            out.append("* `%s` %s - %s" % (r["address"], r["display_name"], r["why"]))
    else:
        out.append("* (none)")
    out.append("")

    # --------------------------------------------------- 2. what 已匹配 means
    out.append("## What %s means" % STATUS_MATCHED)
    out.append("")
    out.append("A row is `%s` only when the pair has passed **both** directions of the" % STATUS_MATCHED)
    out.append("mutation check, at **200,000** and **1,000,000** iterations, and the batch's")
    out.append("five-seed sweep (five seeds x 1,000,000 iterations):")
    out.append("")
    out.append("* **direction 1 - mutate the rebuild.** Change one instruction of the")
    out.append("  rebuilt function (for example the operand of one x87 op, or one float")
    out.append("  constant). The pair must then **fail**. If it still passes, the row does")
    out.append("  not witness what it claims to witness, and it is not `%s`." % STATUS_MATCHED)
    out.append("* **direction 2 - restore the rebuild.** Undo the mutation. The pair must")
    out.append("  **pass** again. If it still fails, the failure was not caused by the")
    out.append("  mutation and the row is not `%s` either." % STATUS_MATCHED)
    out.append("")
    out.append("`200,000` iterations catch gross mismatches; `1,000,000` iterations and the")
    out.append("five-seed sweep catch the ones that only show up in a corner of the input")
    out.append("domain. The per-row evidence -- iteration counts, mutation, seed list --")
    out.append("lives in `tools/fafdiff/README.md`; this table only carries the verdict.")
    out.append("")

    # ----------------------------------------------- 3. how it is generated
    out.append("## How this table is generated")
    out.append("")
    out.append("`tools/fafdiff/scripts/gen_coverage.py` writes this file from two inputs:")
    out.append("")
    out.append("* `tools/fafdiff/scripts/screen_math.py` -> `%s`"
               % os.path.relpath(screen_path, REPO_ROOT).replace("\\", "/"))
    out.append("  (the screen: one row per retail address, with the x87/SSE census and")
    out.append("  the source annotation each address came from)")
    out.append("* `tools/fafdiff/data/coverage_status.json` -- the hand-maintained status")
    out.append("  of every address in the set")
    out.append("")
    out.append("Regenerate with:")
    out.append("")
    out.append("```bash")
    out.append("PATH=.tmp/venv/Scripts:$PATH .tmp/venv/Scripts/python.exe \\")
    out.append("    tools/fafdiff/scripts/screen_math.py --out .tmp/fafdiff")
    out.append("PYTHONUTF8=1 .tmp/venv/Scripts/python.exe \\")
    out.append("    tools/fafdiff/scripts/gen_coverage.py")
    out.append("```")
    out.append("")
    out.append("The set is defined by exactly this predicate, in exactly one place")
    out.append("(`LEDGER_PREDICATE_TEXT` / `in_ledger()` in the generator):")
    out.append("")
    out.append("```")
    out.append(LEDGER_PREDICATE_TEXT)
    out.append("```")
    out.append("")
    out.append("The set currently holds **%d** functions; the generator asserts that number" % len(rows))
    out.append("and fails loudly if the tree, the screen or the predicate changes it.")
    out.append("")
    if dropped_by_calls:
        out.append("The `calls <= 3` clause of the original specification is **not** applied:")
        out.append("on the current tree it removes %d rows and leaves %d, not %d. The rows"
                   % (len(dropped_by_calls), len(rows) - len(dropped_by_calls), len(rows)))
        out.append("it would remove are listed here, so the difference is explicit:")
        out.append("")
        for r in dropped_by_calls:
            out.append("* `%s` %s (`%s:%d`)"
                       % (r["address"], r["symbol"], r["file"], r["line"]))
        out.append("")
        out.append("Set `CALLS_LIMIT = 3` in the generator to apply the clause; the count")
        out.append("assertion then fails loudly, which is the intended behaviour.")
        out.append("")
    out.append("**Do not edit this file by hand.** Every line below the header is written")
    out.append("by the generator. Edit `tools/fafdiff/data/coverage_status.json` and")
    out.append("re-run the generator instead.")
    out.append("")

    # ------------------------------------------------------------- notes
    out.append("## Notes")
    out.append("")
    ida_rows = [r for r in rows
                 if r["name_recovered"] and r["kind"] == "function"
                 and RE_IDA_STYLE.match(r["screen_symbol"] or "")]
    if ida_rows:
        out.append("Rows whose screen symbol is an IDA-style name (`sub_6D1E30`,")
        out.append("`func_QuatLERP`, `x00BCB540`, ...) carry the source-level name read")
        out.append("from the annotated source line instead:")
        out.append("")
        for r in ida_rows:
            out.append("* `%s` screen symbol `%s` -> `%s` (%s:%d)"
                       % (r["address"], r["screen_symbol"], r["display_name"], r["file"], r["line"]))
        out.append("")
    unresolved = [r for r in rows if not r["name_recovered"]]
    if unresolved:
        out.append("Rows whose source-level name could **not** be recovered from the")
        out.append("annotated source line -- these need a human name before pairing:")
        out.append("")
        for r in unresolved:
            out.append("* `%s` `%s` (%s:%d)"
                       % (r["address"], r["screen_symbol"], r["file"], r["line"]))
        out.append("")
    var_rows = [r for r in rows if r["kind"] == "variable"]
    if var_rows:
        out.append("Rows marked *variable* are **compiler-generated dynamic initializers**")
        out.append("of a `static const` object (one copy per including translation unit),")
        out.append("not hand-written functions. There are %d of them; they are x87 code" % len(var_rows))
        out.append("in the retail image and therefore belong to the set, but they cannot be")
        out.append("paired one-by-one -- they are one row per TU copy of the same source")
        out.append("object. (Their screen symbol is the *next* address label, because the")
        out.append("screen script reads the first symbol after the annotation line, which")
        out.append("for these rows is the following `Address:` entry.)")
        out.append("")
    return "\n".join(out) + "\n", counts


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="fafdiff coverage ledger generator")
    ap.add_argument("--screen", default=DEFAULT_SCREEN)
    ap.add_argument("--status", default=DEFAULT_STATUS)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--init-status", action="store_true",
                    help="write a default coverage_status.json (all 未匹配; "
                         "difffuzz-covered rows 不适用) and exit")
    args = ap.parse_args(argv)

    with open(args.screen, "r", encoding="utf-8") as fh:
        screen = json.load(fh)

    rows = [r for r in screen["functions"] if in_ledger(r)]
    rows.sort(key=lambda r: r["addr"])

    # the rows the (unapplied) `calls <= 3` clause would drop
    calls3 = {r["address"] for r in screen["functions"] if in_ledger(r, calls_limit=3)}
    dropped_by_calls = [r for r in rows if r["address"] not in calls3]

    if len(rows) != EXPECTED_SET_SIZE:
        sys.stderr.write(
            "gen_coverage: FATAL: the ledger set holds %d functions, expected %d.\n"
            % (len(rows), EXPECTED_SET_SIZE))
        sys.stderr.write(
            "gen_coverage: the denominator must not drift silently. Either the src/sdk\n"
            "gen_coverage: tree moved, the screen changed, or the predicate changed.\n"
            "gen_coverage: predicate = %s\n" % LEDGER_PREDICATE_TEXT)
        return 2

    # ---------------------------------------------------------- status file
    if args.init_status:
        functions = {}
        for r in sorted(rows, key=lambda r: r["addr"]):
            addr = r["address"]
            if r.get("in_difffuzz"):
                functions[addr] = {"status": STATUS_NA, "why": DEFAULT_NA_WHY}
            else:
                functions[addr] = {"status": STATUS_UNMATCHED, "why": ""}
        data = {"schema": "fafdiff-coverage/1", "functions": functions}
        os.makedirs(os.path.dirname(args.status), exist_ok=True)
        with open(args.status, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(data, fh, indent=1, ensure_ascii=False, sort_keys=False)
            fh.write("\n")
        print("gen_coverage: wrote %s (%d functions)"
              % (os.path.relpath(args.status, REPO_ROOT).replace("\\", "/"), len(rows)))
        return 0

    with open(args.status, "r", encoding="utf-8") as fh:
        status_data = json.load(fh)

    functions = status_data.get("functions")
    if not isinstance(functions, dict):
        sys.stderr.write("gen_coverage: FATAL: %s has no `functions` object\n" % args.status)
        return 2

    set_addrs = {r["address"] for r in rows}
    missing = sorted(set_addrs - set(functions.keys()))
    extra = sorted(set(functions.keys()) - set_addrs)
    bad_status = []
    missing_why = []
    for addr, entry in functions.items():
        st = entry.get("status")
        if st not in STATUSES:
            bad_status.append((addr, st))
            continue
        if st in STATUSES_NEEDING_WHY and not (entry.get("why") or "").strip():
            missing_why.append(addr)
    if missing or extra or bad_status or missing_why:
        for addr in missing:
            sys.stderr.write("gen_coverage: FATAL: %s is in the set but has no status entry\n" % addr)
        for addr in extra:
            sys.stderr.write("gen_coverage: FATAL: %s has a status entry but is not in the set\n" % addr)
        for addr, st in bad_status:
            sys.stderr.write("gen_coverage: FATAL: %s has an invalid status %r\n" % (addr, st))
        for addr in missing_why:
            sys.stderr.write("gen_coverage: FATAL: %s needs a `why` sentence\n" % addr)
        sys.stderr.write("gen_coverage: refusing to write COVERAGE.md\n")
        return 2

    # ------------------------------------------------------- build the rows
    resolver = NameResolver(REPO_ROOT)
    out_rows = []
    for r in rows:
        addr = r["address"]
        entry = functions[addr]
        display, kind, recovered = resolver.resolve(r["file"], r["line"])
        if not display:
            display = r["symbol"]
            kind = "unknown"
            recovered = False
        out_rows.append({
            "address": addr,
            "addr": r["addr"],
            "screen_symbol": r["symbol"],
            "display_name": display,
            "kind": kind,
            "name_recovered": recovered,
            "file": r["file"],
            "line": r["line"],
            "status": entry["status"],
            "why": (entry.get("why") or "").strip(),
            "in_difffuzz": r.get("in_difffuzz", 0),
        })

    md, counts = build_markdown(out_rows, functions, screen.get("src_commit", "unknown"),
                                 args.screen, args.status, dropped_by_calls)

    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(md)

    print("gen_coverage: %d functions -> %s" % (len(out_rows),
                                                os.path.relpath(args.out, REPO_ROOT).replace("\\", "/")))
    for s in STATUSES:
        print("gen_coverage:   %s = %d" % (s, counts[s]))
    print("gen_coverage: total = %d" % sum(counts.values()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
