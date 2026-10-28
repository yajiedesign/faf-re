"""Drive the symbolic tracer over the simulator's trace and print the traced
expression for the result, in the retail's own terms.

The names come from the simulator's own trace:

* the segment fields, by address (origin at +0x00, direction at +0x0C, extent
  at +0x18 in each Segment3);
* the header values, by the expression that was in the register when
  GetSquared spilled it - so `dA01` is the 53-bit value retail kept in the
  register, and `fA01` is the float32 copy it spilled and reloaded.

The value the expression evaluates to is compared with the retail's, so the
transcription cannot drift from the oracle.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sim as S
import simrun
import symtrace
from symtrace import f32

SEG0 = 0x2000
SEG1 = 0x2100

# The retail's header spills, and the register each one leaves behind.
HEADER_SPILLS = [
    (0xA7E66E, 'kDiff.x'),
    (0xA7E678, 'kDiff.y'),
    (0xA7E682, 'kDiff.z'),
    (0xA7E6A6, 'dA01'),
    (0xA7E6CF, 'dB0'),
    (0xA7E6F0, 'dB1'),
    (0xA7E702, 'dC'),
    (0xA7E71C, 'dDet'),
    (0xA7E74C, 'dS0'),
    (0xA7E756, 'dS1'),
]


def segment_names():
    names = {}
    for base, tag in ((SEG0, 's0'), (SEG1, 's1')):
        for k in range(3):
            names[base + 0x00 + 4 * k] = '%s.o.%s' % (tag, 'xyz'[k])
            names[base + 0x0C + 4 * k] = '%s.d.%s' % (tag, 'xyz'[k])
        names[base + 0x18] = '%s.ext' % tag
    return names


def substitute(expr, subs):
    """Replace the known expressions by their names, longest first."""
    for old, new in sorted(subs.items(), key=lambda kv: -len(kv[0])):
        expr = expr.replace(old, new)
    return expr


def simplify(e):
    e = re.sub(r'\(\(([^()]*)\)', r'(\1)', e)
    e = re.sub(r'-\(([a-zA-Z_][\w.]*)\)', r'-\1', e)
    e = re.sub(r'\(-([a-zA-Z_][\w.]*)\)', r'(-\1)', e)
    e = e.replace('+ -', '- ')
    e = re.sub(r'\s+', ' ', e)
    return e


def trace_draw(idx, rows, show=True):
    s0, s1, retail = rows[idx]
    sm = simrun.build(s0, s1)
    init_regs = dict(sm.reg)
    sm.trace = []
    sm.run(trace=True)
    tr = sm.trace
    t, errs = symtrace.run_trace(tr, {}, regs0=init_regs, mem64=sm.mem64)
    final = t.s(0)

    # the segment fields, by address
    seg = segment_names()

    def rename(e):
        for addr, nm in seg.items():
            e = e.replace('m[%08x]' % addr, nm)
        return e

    # the header names: the expression that was in the register at each spill
    subs = {}
    for pc, name in HEADER_SPILLS:
        if pc not in t.stores:
            continue
        addr, expr, val = t.stores[pc]
        expr = rename(expr)
        subs[expr] = name
        subs['r32(%s)' % expr] = name.replace('d', 'f', 1)
    expr = substitute(rename(final.e), subs)
    expr = simplify(expr)
    ok = (S.bits(f32(final.v)) == retail)
    print('draw %-4d retail %08X  traced %08X  %s' % (
        idx, retail, S.bits(f32(final.v)), 'OK' if ok else 'DIFF'))
    if errs:
        print('   unhandled: %s' % (errs[:4],))
    if show:
        print('   %s' % expr)
    return expr, ok


if __name__ == '__main__':
    rows = simrun.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'simd.txt'))
    idxs = [int(a) for a in sys.argv[1:]] or [0]
    nok = 0
    for i in idxs:
        _, ok = trace_draw(i, rows, show=(len(idxs) <= 3))
        nok += 1 if ok else 0
    print('traced == retail: %d / %d' % (nok, len(idxs)))
