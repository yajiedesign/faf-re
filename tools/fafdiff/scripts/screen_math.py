#!/usr/bin/env python3
"""
fafdiff :: screen_math.py

First-pass screening filter for the retail floating-point surface.

Reads the Doxygen `Address: 0xXXXXXXXX` annotations under `src/sdk`, slices
each annotated function's body out of the retail PE image, disassembles it with
capstone (x86 / 32-bit) and ranks it by how much floating-point math it does
-- with special attention to whether the retail body still uses x87.

THIS IS A SCREENING TOOL, NOT A VERDICT TOOL.  It deliberately over-includes:
the outputs are a candidate list for a human to triage.

Usage:

    .tmp/venv/Scripts/python.exe tools/fafdiff/scripts/screen_math.py \
        --src src/sdk --image C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe \
        --out .tmp/fafdiff

Defaults: `--src <repo>/src/sdk`, `--image` the retail FA exe, `--out`
`<repo>/.tmp/fafdiff`.  The repo root is derived from this file's own location
(`tools/fafdiff/scripts` -> up three levels).

Outputs (all under --out):
    screen.csv     one row per unique address
    summary.md     totals, unmapped count, top files, priority list
    priority.txt   `0xADDR<TAB>symbol<TAB>file:line`, sorted by address
    screen.json    everything, including per-function distinct call targets

Read-only over src/ and the retail image.  Writes only under --out.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import struct
import subprocess
import sys
import time
from collections import Counter, defaultdict

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86_const import X86_OP_IMM

# --------------------------------------------------------------------------
# configuration
# --------------------------------------------------------------------------

# repo root: <root>/tools/fafdiff/scripts/screen_math.py -> up three levels
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
SRC_ROOT = os.path.join(REPO_ROOT, "src", "sdk")
RETAIL_EXE = r"C:\ProgramData\FAForever\bin\ForgedAlliance_6c6c92f.exe"

OUT_DIR = os.path.join(REPO_ROOT, ".tmp", "fafdiff")

MAX_BODY = 4096           # cap on a single function body
MAX_WINDOW = 150          # cap on the source window (lines) per annotation
SYMBOL_LINES = 8          # spec: symbol must be found within this many lines

# known math entry points in the retail image
MATH_ENTRIES = {
    0x00A91000: "crt_sqrt",
    0x005734E0: "sinf",
    0x005734D0: "cosf",
}

X87_MNEMONICS = {
    "fld", "fst", "fstp", "fild", "fist", "fistp",
    "fadd", "faddp", "fsub", "fsubp", "fsubr", "fsubrp",
    "fmul", "fmulp", "fdiv", "fdivp", "fdivrp",
    "fabs", "fchs", "fsqrt",
    "fcom", "fcomp", "fcompp", "fucom", "fucomp", "fucompp",
    "fxch", "fnstsw", "fstsw", "frndint",
    "f2xm1", "fscale", "fprem", "fprem1", "fyl2x", "fpatan", "fptan",
    "fsin", "fcos", "fincstp", "fdecstp", "ffree", "fwait", "fninit",
    "fldcw", "fnstcw", "fldz", "fld1", "fldpi", "ftst", "fxam",
    "fclex", "fnclex", "fsetpm", "fstp1", "fstp2",
    # fcmov* family
    "fcmove", "fcmovne", "fcmovb", "fcmovae", "fcmovbe", "fcmova",
    "fcmovu", "fcmovnu", "fcmovnb", "fcmovnbe", "fcmovna", "fcmovnau",
    "fcmovs", "fcmovns", "fcmovp", "fcmovnp",
    # fcomi* / fucomi* family
    "fcomi", "fucomi", "fcomip", "fucomip",
}

SSE_SCALAR = {
    "movss", "addss", "subss", "mulss", "divss", "sqrtss",
    "minss", "maxss", "comiss", "ucomiss",
    "cvtss2si", "cvtsi2ss", "cvttss2si",
    "rcpss", "rsqrtss",
    "andps", "orps", "xorps", "shufps", "unpcklps", "unpckhps",
    "movaps", "movups",
}

SSE_DOUBLE = {
    "movsd", "addsd", "subsd", "mulsd", "divsd", "sqrtsd",
    "minsd", "maxsd", "comisd", "ucomisd",
    "cvtsd2ss", "cvtss2sd", "cvtsi2sd", "cvttsd2si",
    "movapd", "movupd",
}

# ---------------------------------------------------------------- source signals

# `Address:` marker.  The marker occasionally wraps onto the next line
# (e.g. "* Address: 0x00946DC0 (...) Address:" / "* 0x00946F60 (...)"), so the
# address may sit on the following line.
RE_ADDR = re.compile(r"Address:[ \t]*(?:\n[ \t]*)?0x([0-9A-Fa-f]{8})")

# Leading comment decoration of a line (`*`, `//`) replaced by spaces so that
# the `Address:` marker can be matched across a line break without disturbing
# byte offsets (offsets are needed to recover line numbers).
RE_DECOR = re.compile(r"^([ \t]*)([*/]+)")

RE_DOUBLE = re.compile(r"\b(?:double|long\s+double)\b")

MATH_NAMES = (
    "sqrt|sin|cos|tan|asin|acos|atan2|atan|pow|exp|log10|log|fmod|floor|ceil|round|trunc|fabs"
)
RE_MATH_STD = re.compile(r"\bstd::(?:" + MATH_NAMES + r")\b")
RE_MATH_C = re.compile(r"(?<![\w:.>])(?:" + MATH_NAMES + r")\s*\(")

RE_X87 = re.compile(r"X87Math|msvc8::|X87SqrtCall|_controlfp|_PC_24|fnstsw|__asm")

RE_WM3 = re.compile(
    r"Wm3::(?:Vector|Quaternion|Matrix|Plane|Box|Sphere|Segment|Line|Ray|Circle|Dist|Intr|Eigen)"
)

# Symbol detection.  The identifier must be immediately followed by `(` (only
# whitespace in between) -- that is what separates a real declaration from
# prose such as "returns the handle (an int32)".  The optional `@<reg>` suffix
# covers IDA-style signatures (`sub_768FF0@<eax>(...)`).
RE_SYMBOL = re.compile(
    r"((?:[A-Za-z_][A-Za-z0-9_]*::)*"
    r"(?:~?[A-Za-z_][A-Za-z0-9_]*|operator[^(\s]*)"
    r"(?:@\s*<[^>]*>)?)"
    r"\s*(?:<[^<>()]*>)?\s*\("
)

# C++ keywords that can precede a '(' but never name a function
CPP_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "catch", "else",
    "do", "case", "throw", "new", "delete", "and", "or", "not", "xor",
    "assert", "static_assert", "decltype", "typeid", "defined",
    "alignof", "noexcept", "requires", "co_await", "co_yield", "co_return",
    "template", "typename", "operator", "constexpr", "consteval", "constinit",
    "explicit", "inline", "friend", "mutable", "register", "volatile",
    "public", "private", "protected", "virtual", "using", "typedef",
}

RE_NS = re.compile(r"\b(?:namespace|class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)")


# --------------------------------------------------------------------------
# PE image
# --------------------------------------------------------------------------

class PEImage:
    """Minimal PE32 reader: sections + RVA -> file offset mapping."""

    def __init__(self, path: str):
        with open(path, "rb") as fh:
            self.data = fh.read()
        d = self.data
        if d[:2] != b"MZ":
            raise ValueError("not an MZ image: %s" % path)
        pe_off = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe_off:pe_off + 4] != b"PE\0\0":
            raise ValueError("not a PE image: %s" % path)
        machine, nsec = struct.unpack_from("<HH", d, pe_off + 4)
        if machine != 0x14C:
            raise ValueError("not PE32 (machine=0x%X)" % machine)
        opt = pe_off + 24
        magic = struct.unpack_from("<H", d, opt)[0]
        if magic == 0x10B:
            self.image_base = struct.unpack_from("<I", d, opt + 28)[0]
            sec_off = opt + 224
        elif magic == 0x20B:
            self.image_base = struct.unpack_from("<Q", d, opt + 24)[0]
            sec_off = opt + 240
        else:
            raise ValueError("unknown optional header magic 0x%X" % magic)

        self.sections = []
        for i in range(nsec):
            o = sec_off + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
            chars = struct.unpack_from("<I", d, o + 36)[0]
            # IMAGE_SCN_CNT_CODE == 0x20 -- only code sections may be executed.
            self.sections.append({
                "name": name, "va": va, "vsize": vsize,
                "rawsize": rawsize, "rawptr": rawptr, "chars": chars,
                "is_code": bool(chars & 0x20),
            })
        self.path = path

    def section_for_rva(self, rva: int):
        for s in self.sections:
            if s["va"] <= rva < s["va"] + max(s["vsize"], s["rawsize"]):
                return s
        return None

    def rva_to_file(self, rva: int):
        s = self.section_for_rva(rva)
        if s is None or s["rawsize"] == 0:
            return None
        off = rva - s["va"]
        if off >= s["rawsize"]:
            return None
        return s["rawptr"] + off

    def readable(self, rva: int, length: int) -> bytes:
        """Return up to `length` readable bytes starting at rva (may be short)."""
        out = bytearray()
        cur = rva
        remaining = length
        while remaining > 0:
            s = self.section_for_rva(cur)
            if s is None or s["rawsize"] == 0:
                break
            off = cur - s["va"]
            avail = s["rawsize"] - off
            if avail <= 0:
                break
            take = min(avail, remaining)
            fo = s["rawptr"] + off
            out += self.data[fo:fo + take]
            cur += take
            remaining -= take
        return bytes(out)

    def describe(self) -> str:
        lines = ["# PE image: %s" % self.path,
                 "# image base 0x%08X" % self.image_base]
        for s in self.sections:
            lines.append(
                "#   %-8s rva 0x%08X vsize 0x%X rawsize 0x%X code=%s -> 0x%08X..0x%08X"
                % (s["name"], s["va"], s["vsize"], s["rawsize"],
                   "yes" if s["is_code"] else "no",
                   self.image_base + s["va"], self.image_base + s["va"] + s["rawsize"])
            )
        return "\n".join(lines)


# --------------------------------------------------------------------------
# source parsing
# --------------------------------------------------------------------------

class Annotation:
    __slots__ = ("addr", "symbol", "symbol_ext", "rel_path", "line", "src",
                 "is_cpp", "ns", "score", "src_rel")

    def __init__(self, addr, symbol, symbol_ext, rel_path, line, src, is_cpp, ns,
                 src_rel=""):
        self.addr = addr
        self.symbol = symbol
        self.symbol_ext = symbol_ext
        self.rel_path = rel_path
        self.line = line
        self.src = src
        self.is_cpp = is_cpp
        self.ns = ns
        self.score = 0
        self.src_rel = src_rel


def strip_decoration(raw: str) -> str:
    """Strip leading whitespace and comment decoration (`*`, `//`)."""
    s = raw.strip()
    for _ in range(4):
        if s.startswith("*"):
            s = s[1:].strip()
        elif s.startswith("//"):
            s = s[2:].strip()
        else:
            break
    return s


def is_comment_line(raw: str) -> bool:
    s = raw.lstrip()
    return s.startswith("*") or s.startswith("//") or s.startswith("/*")


def candidate_symbol(content: str):
    """Return a cleaned symbol name if `content` looks like a declaration."""
    m = RE_SYMBOL.search(content)
    if not m:
        return None
    name = m.group(1).strip()
    name = re.sub(r"\s+", " ", name)
    # drop the IDA register decoration: `sub_768FF0@<eax>` -> `sub_768FF0`
    name = re.sub(r"\s*@\s*<[^>]*>\s*$", "", name)
    if name.startswith("operator"):
        name = name.replace("operator ", "operator")
    if not name:
        return None
    tail = name.split("::")[-1].split()
    last = tail[-1] if tail else name
    if last in CPP_KEYWORDS:
        return None
    return name


def find_symbol(lines, idx, limit=SYMBOL_LINES):
    """Symbol of the function documented by the annotation at `idx`.

    Spec: the symbol lives on the first following line that contains `(` and
    is not comment text (leading `*`, `//`, whitespace stripped).

    A code line (not inside a comment block) is preferred; a doc-block line
    carrying an IDA-style signature is used as a fallback.  Returns
    `(symbol, symbol_source)`.
    """
    end = min(len(lines), idx + 1 + limit)
    fallback = None
    for j in range(idx + 1, end):
        raw = lines[j]
        content = strip_decoration(raw)
        if not content:
            continue
        name = candidate_symbol(content)
        if name is None:
            continue
        if is_comment_line(raw):
            # doc-block line: usable, but only if no real code line wins
            if fallback is None:
                fallback = name
            continue
        return name, "code"
    if fallback is not None:
        return fallback, "doc"
    return "<unknown>", "none"


def find_symbol_extended(lines, idx, limit=60):
    """Same as find_symbol but with a wider window (JSON only)."""
    name, _ = find_symbol(lines, idx, limit)
    return name


def enclosing_namespace(lines, idx):
    """Best-effort nearest enclosing namespace/class name."""
    for j in range(idx, max(-1, idx - 400), -1):
        s = lines[j].strip()
        if s.startswith("}") or s.startswith(");"):
            continue
        m = RE_NS.search(s)
        if m:
            return m.group(1)
    return ""


def walk_sources(src_root):
    files = []
    for dirpath, dirnames, filenames in os.walk(src_root):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.endswith(".cpp") or fn.endswith(".h"):
                files.append(os.path.join(dirpath, fn))
    files.sort()
    return files


def parse_sources(src_root):
    annotations = []
    for path in walk_sources(src_root):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        lines = text.splitlines()
        if not any("Address:" in l for l in lines):
            continue

        # Flatten: leading comment decoration replaced by spaces.  Offsets and
        # line numbers are preserved, but an `Address:` marker that wraps onto
        # the following line becomes matchable by a single regex.
        chunks = []
        for l in lines:
            m = RE_DECOR.match(l)
            if m:
                l = " " * len(m.group(0)) + l[m.end():]
            chunks.append(l)
        flat = "\n".join(chunks)

        matches = list(RE_ADDR.finditer(flat))
        if not matches:
            continue
        rel = os.path.relpath(path, REPO_ROOT).replace("\\", "/")
        src_rel = os.path.relpath(path, src_root).replace("\\", "/")
        is_cpp = path.endswith(".cpp")

        # line index for every character offset in `flat`
        starts = [0]
        for l in lines:
            starts.append(starts[-1] + len(l) + 1)

        def line_of(pos):
            lo, hi = 0, len(starts) - 1
            while lo < hi:
                mid = (lo + hi + 1) // 2
                if starts[mid] <= pos:
                    lo = mid
                else:
                    hi = mid - 1
            return lo

        for k, m in enumerate(matches):
            addr = int(m.group(1), 16)
            ann_line = line_of(m.start())
            sym_line = max(ann_line, line_of(m.end()))
            sym, sym_src = find_symbol(lines, sym_line)
            sym_ext = find_symbol_extended(lines, sym_line)
            ns = enclosing_namespace(lines, ann_line)
            # source window: annotation line .. next annotation line (150 cap)
            nxt_line = line_of(matches[k + 1].start()) if k + 1 < len(matches) else len(lines)
            if nxt_line <= ann_line:
                nxt_line = ann_line + 1
            win = lines[ann_line:min(nxt_line, ann_line + MAX_WINDOW)]
            a = Annotation(addr, sym, sym_ext, rel, ann_line + 1,
                           "\n".join(win), is_cpp, ns, src_rel)
            # dedup score: prefer a file whose name matches the enclosing
            # namespace/class, or whose class name appears in the symbol.
            stem = os.path.splitext(os.path.basename(path))[0]
            score = 0
            if ns and (stem.lower() == ns.lower() or ns.lower() in stem.lower()):
                score += 4
            if ns and ns in sym:
                score += 3
            if ns and stem.lower() in sym.lower():
                score += 1
            a.score = score
            annotations.append(a)
    return annotations


def dedupe(annotations):
    """Deduplicate by address; report how many rows were dropped."""
    by_addr = defaultdict(list)
    for a in annotations:
        by_addr[a.addr].append(a)
    chosen = {}
    dupes = 0
    for addr, cands in by_addr.items():
        if len(cands) > 1:
            dupes += len(cands) - 1
        # .cpp beats .h; then namespace/class match; then sorted path order
        best = sorted(
            cands,
            key=lambda c: (0 if c.is_cpp else 1, -c.score, c.rel_path, c.line),
        )[0]
        chosen[addr] = best
    return chosen, dupes, len(by_addr)


# --------------------------------------------------------------------------
# disassembly
# --------------------------------------------------------------------------

def make_disassembler():
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    md.skipdata = True
    return md


def decode_body(md, pe, addr, end_addr):
    """Decode [addr, end_addr) from the retail image."""
    rva = addr - pe.image_base
    span = max(0, min(end_addr, addr + MAX_BODY) - addr)
    out = {
        "body_bytes": 0, "x87": 0, "ssescalar": 0, "ssedouble": 0,
        "calls": 0, "math_calls": 0, "call_targets": [], "decoded": 0,
        "mapped": False, "section": None, "section_is_code": None,
        "body_capped": False, "x87_mnemonics": [],
        # tighter proxy: counts up to the first `ret` of the body.  The
        # [addr, next_addr) window can spill into the following function when
        # the next annotated address is far away; these numbers bound the
        # effect for later re-slicing.
        "bytes_to_first_ret": 0, "x87_to_first_ret": 0,
        "ssescalar_to_first_ret": 0, "ssedouble_to_first_ret": 0,
    }
    if span == 0:
        return out
    sec = pe.section_for_rva(rva)
    if sec is None:
        return out
    out["section"] = sec["name"]
    out["section_is_code"] = sec["is_code"]
    fo = pe.rva_to_file(rva)
    if fo is None:
        return out
    out["mapped"] = True
    blob = pe.readable(rva, span)
    out["body_bytes"] = len(blob)
    if not blob:
        return out
    if end_addr - addr > MAX_BODY:
        out["body_capped"] = True

    targets = set()
    x87_seen = []
    seen_ret = False
    for ins in md.disasm(blob, addr):
        out["decoded"] += 1
        mn = ins.mnemonic
        is_x87 = mn in X87_MNEMONICS
        is_sse_s = mn in SSE_SCALAR
        is_sse_d = mn in SSE_DOUBLE
        if is_x87:
            out["x87"] += 1
            x87_seen.append(mn)
        elif is_sse_s:
            out["ssescalar"] += 1
        elif is_sse_d:
            out["ssedouble"] += 1
        if not seen_ret:
            if is_x87:
                out["x87_to_first_ret"] += 1
            elif is_sse_s:
                out["ssescalar_to_first_ret"] += 1
            elif is_sse_d:
                out["ssedouble_to_first_ret"] += 1
            if mn.startswith("ret"):
                seen_ret = True
                out["bytes_to_first_ret"] = ins.address + ins.size - addr
        if mn.startswith("call"):
            out["calls"] += 1
            tgt = None
            try:
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    tgt = op.imm & 0xFFFFFFFF
            except Exception:
                tgt = None
            if tgt is not None:
                targets.add(tgt)
                if tgt in MATH_ENTRIES:
                    out["math_calls"] += 1
    out["call_targets"] = sorted(targets)
    out["x87_mnemonics"] = x87_seen
    return out


# --------------------------------------------------------------------------
# source-window signals
# --------------------------------------------------------------------------

def source_signals(text):
    return {
        "src_double": 1 if RE_DOUBLE.search(text) else 0,
        "src_mathlib": 1 if (RE_MATH_STD.search(text) or RE_MATH_C.search(text)) else 0,
        "src_x87": 1 if RE_X87.search(text) else 0,
        "src_wm3": 1 if RE_WM3.search(text) else 0,
    }


def tier_of(d):
    if d["x87"] > 0:
        return "x87"
    if (d["ssescalar"] + d["ssedouble"]) > 0:
        return "sse"
    return "int"


# ---------------------------------------------------------------- third party

# Bundled third-party code that ships inside the engine tree.  A row that
# belongs to one of these is *not* engine code: pairing it against the retail
# image tells us nothing about the rebuild.
THIRD_PARTY_DIRS = {
    "libpng", "zlib", "lua", "boost", "ogg", "vorbis",
    "jpeg", "tiff", "freetype",
}

# file-name fragments that mark third-party translation units even when the
# directory name is not one of the above
THIRD_PARTY_FILE_HINTS = (
    "zlib", "zconf", "png", "pngconf", "lua", "boost", "vorbis", "ogg",
    "jpeg", "jpg", "tiff", "freetype", "ft2build",
)

# symbol fragments that mark third-party code
THIRD_PARTY_SYMBOL_HINTS = (
    "lua_", "luaL_", "png_", "pngstruct", "deflate", "inflate",
    "z_stream", "crc32", "adler32", "vorbis_", "ogg_", "jpeg_",
    "tiff_", "FT_", "freetype",
)

RE_THIRD_PARTY_SYMBOL = re.compile(
    r"(?:^|[^A-Za-z0-9_])(?:%s)" % "|".join(re.escape(h) for h in THIRD_PARTY_SYMBOL_HINTS)
)


def is_thirdparty(src_rel: str, symbol: str) -> bool:
    """1 when the file or the symbol belongs to bundled third-party code."""
    parts = src_rel.split("/")
    if any(p.lower() in THIRD_PARTY_DIRS for p in parts[:-1]):
        return True
    stem = parts[-1].lower()
    if any(h in stem for h in THIRD_PARTY_FILE_HINTS):
        return True
    if RE_THIRD_PARTY_SYMBOL.search(symbol or ""):
        return True
    return False


# ------------------------------------------------------------------ tu class

MATH_PATHS = (
    "moho/math/",
    "legacy/math/",
)

MATH_FILES = (
    "gpg/gal/StringUtils.cpp",
)


def tu_class_of(src_rel: str, symbol: str, section_is_code) -> str:
    """`math`, `thirdparty`, `noncode` or `engine`."""
    rel = src_rel.replace("\\", "/")
    if rel.startswith(MATH_PATHS) or rel in MATH_FILES:
        return "math"
    if is_thirdparty(rel, symbol):
        return "thirdparty"
    if not section_is_code:
        return "noncode"
    return "engine"


# ---------------------------------------------------------------- difffuzz

DIFFFUZZ_TARGETS = os.path.join(REPO_ROOT, "tools", "difffuzz", "src", "targets_math.cpp")

RE_DIFFFUZZ_NAME = re.compile(r"\{\"([^\"]+)\"")


def difffuzz_names(path=DIFFFUZZ_TARGETS):
    """Leaf identifiers of every target already present in difffuzz."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return set()
    names = set()
    for n in RE_DIFFFUZZ_NAME.findall(text):
        names.add(n)
        leaf = n.split("::")[-1]
        names.add(leaf)
    return names


def in_difffuzz(symbol: str, names) -> int:
    """1 when the symbol is already a difffuzz target (leaf identifier match).

    `func_QuatLERP` matches the difffuzz target `QuatLERP`.
    """
    if not symbol or symbol == "<unknown>":
        return 0
    leaf = symbol.split("::")[-1].strip()
    leaf = re.sub(r"@\s*<[^>]*>$", "", leaf)
    leaf = leaf.split("(")[0].strip()
    if not leaf:
        return 0
    cands = {leaf}
    if leaf.startswith("func_"):
        cands.add(leaf[5:])
    for c in cands:
        if c in names:
            return 1
    return 0


# ------------------------------------------------------------------- git

def git_commit(path: str) -> str:
    """`git -C <path> rev-parse --short HEAD`, or `unknown`."""
    for d in (path, REPO_ROOT):
        try:
            out = subprocess.run(
                ["git", "-C", d, "rev-parse", "--short", "HEAD"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                check=False,
            )
        except OSError:
            continue
        if out.returncode == 0:
            txt = out.stdout.decode("utf-8", "replace").strip()
            if txt:
                return txt
    return "unknown"


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

CSV_COLUMNS = ["address", "tier", "symbol", "file", "line", "body_bytes",
               "x87", "ssescalar", "ssedouble", "calls", "math_calls",
               "src_double", "src_mathlib", "src_x87", "src_wm3",
               "x87_to_first_ret", "ssescalar_to_first_ret",
               "ssedouble_to_first_ret", "thirdparty", "tu_class",
               "in_difffuzz"]


def main(argv=None):
    ap = argparse.ArgumentParser(description="fafdiff math screening filter")
    ap.add_argument("--image", "--exe", dest="image", default=RETAIL_EXE,
                    help="retail PE image to disassemble")
    ap.add_argument("--src", default=SRC_ROOT,
                    help="source tree to scan for `Address:` annotations")
    ap.add_argument("--out", default=OUT_DIR,
                    help="output directory for screen.csv/json, summary.md, priority.txt")
    args = ap.parse_args(argv)

    t0 = time.time()
    os.makedirs(args.out, exist_ok=True)

    pe = PEImage(args.image)
    md = make_disassembler()

    src_commit = git_commit(args.src)
    dfz_names = difffuzz_names()

    annotations = parse_sources(args.src)
    total_rows = len(annotations)
    chosen, dupes, unique_addrs = dedupe(annotations)

    addrs = sorted(chosen.keys())
    next_of = {}
    for i, a in enumerate(addrs):
        next_of[a] = addrs[i + 1] if i + 1 < len(addrs) else (a + MAX_BODY)

    rows = []
    unmapped = 0
    for a in addrs:
        ann = chosen[a]
        d = decode_body(md, pe, a, next_of[a])
        sig = source_signals(ann.src)
        tier = tier_of(d)
        if not d["mapped"] or d["body_bytes"] == 0:
            unmapped += 1
            tier = "unmapped"
        rows.append({
            "address": "0x%08X" % a,
            "addr": a,
            "tier": tier,
            "symbol": ann.symbol,
            "symbol_extended": ann.symbol_ext,
            "file": ann.rel_path,
            "line": ann.line,
            "body_bytes": d["body_bytes"],
            "x87": d["x87"],
            "ssescalar": d["ssescalar"],
            "ssedouble": d["ssedouble"],
            "calls": d["calls"],
            "math_calls": d["math_calls"],
            "src_double": sig["src_double"],
            "src_mathlib": sig["src_mathlib"],
            "src_x87": sig["src_x87"],
            "src_wm3": sig["src_wm3"],
            "section": d["section"] or "",
            "section_is_code": d["section_is_code"],
            "decoded_insns": d["decoded"],
            "body_capped": d["body_capped"],
            "bytes_to_first_ret": d["bytes_to_first_ret"],
            "x87_to_first_ret": d["x87_to_first_ret"],
            "ssescalar_to_first_ret": d["ssescalar_to_first_ret"],
            "ssedouble_to_first_ret": d["ssedouble_to_first_ret"],
            "x87_mnemonics": d["x87_mnemonics"],
            "call_targets": ["0x%08X" % t for t in d["call_targets"]],
            "math_call_targets": ["0x%08X" % t for t in d["call_targets"] if t in MATH_ENTRIES],
            "next_addr": "0x%08X" % next_of[a],
            "thirdparty": 1 if is_thirdparty(ann.src_rel, ann.symbol) else 0,
            "tu_class": tu_class_of(ann.src_rel, ann.symbol, d["section_is_code"]),
            "in_difffuzz": in_difffuzz(ann.symbol, dfz_names),
        })

    # -------------------------------------------------------------- CSV
    csv_path = os.path.join(args.out, "screen.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=CSV_COLUMNS, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(r)

    # ------------------------------------------------------- priority list
    def is_priority(r):
        if r["tier"] == "x87":
            return True
        src_sig = r["src_double"] or r["src_mathlib"] or r["src_x87"]
        return bool(src_sig) and (r["ssescalar"] + r["ssedouble"]) > 0

    priority = sorted((r for r in rows if is_priority(r)), key=lambda r: r["addr"])

    prio_path = os.path.join(args.out, "priority.txt")
    with open(prio_path, "w", encoding="utf-8", newline="\n") as fh:
        for r in priority:
            fh.write("%s\t%s\t%s:%d\n" % (r["address"], r["symbol"], r["file"], r["line"]))

    # ------------------------------------------------------------- JSON
    json_path = os.path.join(args.out, "screen.json")
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump({
            "generated_by": "screen_math.py",
            "src_commit": src_commit,
            "retail_image": os.path.abspath(args.image),
            "image_base": "0x%08X" % pe.image_base,
            "sections": pe.sections,
            "math_entries": {("0x%08X" % k): v for k, v in MATH_ENTRIES.items()},
            "total_annotation_rows": total_rows,
            "unique_addresses": unique_addrs,
            "duplicates_dropped": dupes,
            "unmapped": unmapped,
            "counts_per_tier": dict(Counter(r["tier"] for r in rows)),
            "counts_per_tu_class": dict(Counter(r["tu_class"] for r in rows)),
            "counts_per_thirdparty": dict(Counter(r["thirdparty"] for r in rows)),
            "in_difffuzz_count": sum(1 for r in rows if r["in_difffuzz"]),
            "priority_count": len(priority),
            "functions": rows,
        }, fh, indent=1, sort_keys=False)

    # --------------------------------------------------------- summary.md
    tiers = Counter(r["tier"] for r in rows)
    file_x87 = Counter(r["file"] for r in rows if r["tier"] == "x87")
    top_files = file_x87.most_common(20)

    x87_rows = [r for r in rows if r["tier"] == "x87"]
    x87_with_double = [r for r in x87_rows if r["src_double"]]
    x87_with_math = [r for r in x87_rows if r["math_calls"] > 0]
    math_any = [r for r in rows if r["math_calls"] > 0]
    noncode = [r for r in rows if r["section"] and not r["section_is_code"]]
    noncode_x87 = [r for r in noncode if r["tier"] == "x87"]
    capped = [r for r in rows if r["body_capped"]]
    capped_x87 = [r for r in capped if r["tier"] == "x87"]
    unknown_sym = sum(1 for r in rows if r["symbol"] == "<unknown>")

    tier_by_section = Counter((r["section"] or "<none>", r["tier"]) for r in rows)

    lines = []
    lines.append("# fafdiff :: math screening summary")
    lines.append("")
    lines.append("Generated by `screen_math.py` (first-pass screen, over-inclusive by design).")
    lines.append("")
    lines.append("## Inputs")
    lines.append("")
    lines.append("```")
    lines.append(pe.describe())
    lines.append("```")
    lines.append("")
    lines.append("| item | value |")
    lines.append("|---|---|")
    lines.append("| source root | `%s` |" % os.path.relpath(args.src, REPO_ROOT).replace("\\", "/"))
    lines.append("| src/sdk git commit (`git rev-parse --short HEAD`) | `%s` |" % src_commit)
    lines.append("| annotation rows scanned | %d |" % total_rows)
    lines.append("| unique addresses | %d |" % unique_addrs)
    lines.append("| duplicate rows dropped | %d |" % dupes)
    lines.append("| addresses with body unmapped / undecodable | %d |" % unmapped)
    lines.append("| addresses whose symbol could not be resolved | %d |" % unknown_sym)
    lines.append("| addresses whose body hit the 4096-byte cap | %d |" % len(capped))
    lines.append("")
    lines.append("## Totals per tier")
    lines.append("")
    lines.append("| tier | count |")
    lines.append("|---|---|")
    for t in ("x87", "sse", "int", "unmapped"):
        lines.append("| %s | %d |" % (t, tiers.get(t, 0)))
    lines.append("| **total** | **%d** |" % len(rows))
    lines.append("")
    lines.append("- `unmapped` = address outside every mapped section of the image (or "
                 "zero readable bytes). These rows are **kept** in the CSV/JSON with "
                 "`body_bytes = 0` and are NOT counted in x87/sse/int.")
    lines.append("")
    lines.append("### Tier x section")
    lines.append("")
    lines.append("| section | tier | count |")
    lines.append("|---|---|---|")
    for (sec, t), c in sorted(tier_by_section.items()):
        lines.append("| %s | %s | %d |" % (sec, t, c))
    lines.append("")
    lines.append("> Addresses inside non-code sections (`.rdata`, `.data`) are data "
                 "objects, not functions. Their bytes are still disassembled (the "
                 "screen must not drop rows) but any x87/SSE hit there is an artefact "
                 "of decoding data as code -- **%d** such rows, **%d** of them x87. "
                 "Filter on `section_is_code` before trusting a tier."
                 % (len(noncode), len(noncode_x87)))
    lines.append("")
    lines.append("## Floating-point activity")
    lines.append("")
    lines.append("| signal | count |")
    lines.append("|---|---|")
    lines.append("| functions calling a known math entry (0x00A91000 / 0x005734E0 / 0x005734D0) | %d |" % len(math_any))
    lines.append("| x87-tier functions whose source window mentions `double`/`long double` | %d |" % len(x87_with_double))
    lines.append("| x87-tier functions that call a known math entry | %d |" % len(x87_with_math))
    lines.append("| source window mentions `double`/`long double` (any tier) | %d |" % sum(1 for r in rows if r["src_double"]))
    lines.append("| source window mentions a standard math call (any tier) | %d |" % sum(1 for r in rows if r["src_mathlib"]))
    lines.append("| source window contains an x87 hint (any tier) | %d |" % sum(1 for r in rows if r["src_x87"]))
    lines.append("| source window mentions a Wild Magic math type (any tier) | %d |" % sum(1 for r in rows if r["src_wm3"]))
    lines.append("")
    lines.append("### Body-bleed caveat")
    lines.append("")
    lines.append("A body is `[addr, next_annotated_addr)`, so when the next annotated "
                 "address is far away the window spills into the following, "
                 "*unannotated* function. That inflates the raw x87/SSE counts. "
                 "`screen.json` therefore also carries `bytes_to_first_ret`, "
                 "`x87_to_first_ret`, `ssescalar_to_first_ret` and "
                 "`ssedouble_to_first_ret` -- the same counts bounded by the first "
                 "`ret` of the body -- which is a much tighter proxy for the real "
                 "function. `body_capped` marks the 78 bodies that hit the 4096-byte "
                 "cap; %d of those are x87-tier." % len(capped_x87))
    lines.append("")
    lines.append("## Top 20 files by x87-function count")
    lines.append("")
    lines.append("| # | file | x87 functions |")
    lines.append("|---|---|---|")
    for i, (f, c) in enumerate(top_files, 1):
        lines.append("| %d | `%s` | %d |" % (i, f, c))
    lines.append("")
    lines.append("## Priority list")
    lines.append("")
    lines.append("Definition (generous): `tier == x87` **OR** "
                 "(`src_double` OR `src_mathlib` OR `src_x87`) **AND** at least one SSE op "
                 "(`ssescalar + ssedouble >= 1`).")
    lines.append("")
    lines.append("| bucket | count |")
    lines.append("|---|---|")
    lines.append("| tier == x87 | %d |" % tiers.get("x87", 0))
    lines.append("| (src_double OR src_mathlib OR src_x87) AND has SSE op | %d |"
                 % sum(1 for r in rows if r["tier"] != "x87"
                       and (r["src_double"] or r["src_mathlib"] or r["src_x87"])
                       and (r["ssescalar"] + r["ssedouble"]) > 0))
    lines.append("| **total priority** | **%d** |" % len(priority))
    lines.append("")
    lines.append("Full list: `priority.txt` (sorted by address), "
                 "full data incl. call targets: `screen.json`, flat table: `screen.csv`.")
    lines.append("")
    lines.append("## New columns")
    lines.append("")
    lines.append("| column | meaning |")
    lines.append("|---|---|")
    lines.append("| `x87_to_first_ret` | x87 instructions before the body's first `ret` |")
    lines.append("| `ssescalar_to_first_ret` | scalar SSE instructions before the first `ret` |")
    lines.append("| `ssedouble_to_first_ret` | double SSE instructions before the first `ret` |")
    lines.append("| `thirdparty` | 1 when the file or the symbol belongs to bundled "
                 "third-party code (libpng, zlib, lua, boost, ogg, vorbis, jpeg, tiff, freetype) |")
    lines.append("| `tu_class` | `math` (src/sdk/moho/math, src/sdk/legacy/math, "
                 "gpg/gal/StringUtils.cpp), `thirdparty`, `noncode` (address not in a "
                 "code section) or `engine` |")
    lines.append("| `in_difffuzz` | 1 when the symbol is already a target in "
                 "`tools/difffuzz/src/targets_math.cpp` (leaf-identifier match) |")
    lines.append("")
    lines.append("| tu_class | count |")
    lines.append("|---|---|")
    for k, c in sorted(Counter(r["tu_class"] for r in rows).items()):
        lines.append("| %s | %d |" % (k, c))
    lines.append("")
    lines.append("| flag | count |")
    lines.append("|---|---|")
    lines.append("| `thirdparty` | %d |" % sum(1 for r in rows if r["thirdparty"]))
    lines.append("| `in_difffuzz` | %d |" % sum(1 for r in rows if r["in_difffuzz"]))
    lines.append("")

    summary_path = os.path.join(args.out, "summary.md")
    with open(summary_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))

    elapsed = time.time() - t0
    print("screen_math: %d unique addresses (from %d rows, %d dupes dropped) | "
          "tiers: x87=%d sse=%d int=%d unmapped=%d | priority=%d | %.1fs"
          % (unique_addrs, total_rows, dupes,
             tiers.get("x87", 0), tiers.get("sse", 0), tiers.get("int", 0),
             tiers.get("unmapped", 0), len(priority), elapsed))
    print("screen_math: src_commit=%s | tu_class: %s | thirdparty=%d | in_difffuzz=%d"
          % (src_commit,
             dict(Counter(r["tu_class"] for r in rows)),
             sum(1 for r in rows if r["thirdparty"]),
             sum(1 for r in rows if r["in_difffuzz"])))
    print("screen_math: new columns: %s"
          % ", ".join(CSV_COLUMNS[15:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
