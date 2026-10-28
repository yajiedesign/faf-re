"""Symbolic post-processor over the simulator's own trace.

The simulator (`.tmp/wmdiff/sim.py`) is the validated oracle: it reproduces
the retail's fSqrDist bit for bit.  Its trace records, for every executed
instruction, the address, the mnemonic, the operand text, the register file
and the memory.  This script walks that trace and maintains a *symbolic*
register file, so the final value comes out as an expression rather than a
number - the block's arithmetic, with the op order the retail used.

Two things are kept at once per register: the numeric value (computed with the
same rules the simulator uses, so the two can be compared) and the expression
text.  At every step the numeric values are compared against the simulator's
own snapshot, so this script's semantics are checked against the oracle step
by step; and the final value is compared against the retail's.

The memory model: a store to a stack slot records the expression in that
slot, so a reload of it yields the expression again.  The *value* stored is
rounded to float32, because the retail spills to 32-bit slots.
"""
import re
import struct
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sim as S
import simrun


def f32(x):
    if x != x or x in (float('inf'), float('-inf')) or x == 0.0:
        return x
    return struct.unpack('<f', struct.pack('<f', x))[0]


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Slot(object):
    __slots__ = ('v', 'e')

    def __init__(self, v, e):
        self.v = v
        self.e = e


def _alu(a, b, op):
    """The numeric side, at the retail's 53-bit precision (the simulator's
    own _alu with the rounding turned off is the same thing)."""
    if op == 'add':
        return a + b
    if op == 'mul':
        return a * b
    if op == 'sub':
        return a - b
    if op == 'rsub':
        return b - a
    if op == 'div':
        return a / b
    if op == 'rdiv':
        return b / a
    raise ValueError(op)


class Tracer(object):
    def __init__(self, names=None, regs0=None):
        self.R = [Slot(0.0, '?')] * 8
        self.top = 0
        self.mem = {}          # address -> Slot (the symbolic spill slots)
        self.names = names or {}   # value bits -> name
        self.regs0 = regs0 or {}
        self.stores = {}   # pc -> (address, expression, value)
        self.cmps = []     # (pc, expr0, expr1, flags)
        self.n = 0             # live depth

    def compare(self, a, b):
        """The FPU condition codes for a compare of a against b:
        C0 = a < b, C3 = a == b, and the unordered flag."""
        if a != a or b != b:
            self.cmp_flags = (1, 1, 1)
        else:
            self.cmp_flags = (1 if a < b else 0, 1 if a == b else 0, 0)
        return self.cmp_flags

    # ---- register file -------------------------------------------------
    def push(self, v, e):
        self.top = (self.top - 1) & 7
        self.R[self.top] = Slot(v, e)
        self.n += 1

    def pop(self):
        s = self.R[self.top]
        self.top = (self.top + 1) & 7
        self.n -= 1
        return s

    def s(self, i):
        return self.R[(self.top + i) & 7]

    def set(self, i, v, e):
        self.R[(self.top + i) & 7] = Slot(v, e)

    # ---- helpers -------------------------------------------------------
    def named(self, v):
        return self.names.get(bits(v))

    def load(self, addr, value):
        """A load: the symbolic spill slot if we wrote it, else a name if the
        value is one we know, else the raw address."""
        if addr in self.mem:
            return self.mem[addr]
        nm = self.named(value)
        if nm:
            return Slot(value, nm)
        return Slot(value, 'm[%08x]' % addr)


def parse_addr(op, esp, regs):
    """Resolve a memory operand's address from the trace's operand text."""
    body = op[op.index('[') + 1:op.rindex(']')].replace(' ', '')
    if body.startswith('esp'):
        rest = body[3:]
        if rest == '':
            return esp
        if rest[0] == '+':
            return esp + int(rest[1:], 0)
        if rest[0] == '-':
            return esp - int(rest[1:], 0)
    for rn in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'):
        if body.startswith(rn):
            rest = body[len(rn):]
            base = regs.get(rn, 0)
            if rest == '':
                return base
            if rest[0] == '+':
                return base + int(rest[1:], 0)
            if rest[0] == '-':
                return base - int(rest[1:], 0)
    if body.startswith('0x'):
        return int(body, 16)
    raise ValueError('addr %r' % body)


def st_i(op):
    return int(op[op.index('(') + 1:op.index(')')])


def run_trace(trace, names=None, verbose=False, regs0=None, mem64=None):
    """Walk the trace and return (expression, value) for the final result."""
    t = Tracer(names, regs0)
    regs = dict(t.regs0)
    esp = 0
    errs = []
    for ti, (pc, mn, op, snap, mem, esp) in enumerate(trace):
        o = op.strip()
        if verbose:
            n = t.n
            mine = [t.s(i).v for i in range(n)]
            theirs = list(snap[:n])
            bad = [(i, mine[i], theirs[i]) for i in range(n)
                   if not (mine[i] == theirs[i]
                           or (mine[i] != mine[i] and theirs[i] != theirs[i]))]
            if bad:
                print('  MISMATCH before %08x %s %s  depth %d  (trace index %d)' % (pc, mn, o, n, ti))
                for i, a, b in bad[:3]:
                    print('    st(%d) mine %r theirs %r' % (i, a, b))
                break
        try:
            if mn == 'fld':
                if o.startswith('qword ptr'):
                    a = parse_addr(o, esp, regs)
                    v = (mem64 or {}).get(a, 0.0)
                    t.push(v, 'c64[%08x]' % a)
                elif o.startswith('st('):
                    i = st_i(o)
                    src = t.s(i)
                    t.push(src.v, src.e)
                elif o == '1':
                    t.push(1.0, '1.0')
                else:
                    a = parse_addr(o, esp, regs)
                    v = mem.get(a, 0.0)
                    sl = t.load(a, v)
                    t.push(sl.v, sl.e)
            elif mn == 'fstp':
                if o.startswith('st('):
                    i = st_i(o)
                    src = t.s(0)
                    t.set(i, src.v, src.e)
                    t.pop()
                else:
                    a = parse_addr(o, esp, regs)
                    src = t.s(0)
                    t.mem[a] = Slot(f32(src.v), 'r32(%s)' % src.e)
                    t.stores[pc] = (a, src.e, src.v)
                    t.pop()
            elif mn == 'fst':
                if o.startswith('st('):
                    i = st_i(o)
                    src = t.s(0)
                    t.set(i, src.v, src.e)
                else:
                    a = parse_addr(o, esp, regs)
                    src = t.s(0)
                    t.mem[a] = Slot(f32(src.v), 'r32(%s)' % src.e)
                    t.stores[pc] = (a, src.e, src.v)
            elif mn == 'fxch':
                i = st_i(o)
                a, c = t.s(0), t.s(i)
                t.set(0, c.v, c.e)
                t.set(i, a.v, a.e)
            elif mn == 'fchs':
                c = t.s(0)
                t.set(0, -c.v, '-(%s)' % c.e)
            elif mn == 'fabs':
                c = t.s(0)
                t.set(0, abs(c.v), 'abs(%s)' % c.e)
            elif mn == 'fld1':
                t.push(1.0, '1.0')
            elif mn in ('fadd', 'fsub', 'fsubr', 'fmul', 'fdiv', 'fdivr',
                        'faddp', 'fsubp', 'fsubrp', 'fmulp', 'fdivp', 'fdivrp'):
                if '[' in o:
                    a = parse_addr(o, esp, regs)
                    m = mem.get(a, 0.0)
                    memexpr = t.load(a, m).e
                    cur = t.s(0)
                    opn = {'fadd': 'add', 'faddp': 'add', 'fsub': 'sub',
                           'fsubp': 'sub', 'fsubr': 'rsub', 'fsubrp': 'rsub',
                           'fmul': 'mul', 'fmulp': 'mul', 'fdiv': 'div',
                           'fdivp': 'div', 'fdivr': 'rdiv',
                           'fdivrp': 'rdiv'}[mn]
                    if opn in ('rsub', 'rdiv'):
                        # x = the destination's old value, y = the other
                        # operand; 'rsub'/'rdiv' mean y - x and y / x.
                        v = _alu(cur.v, m, opn)
                        e = '(%s %s %s)' % (memexpr, '-' if opn == 'rsub' else '/', cur.e)
                    else:
                        v = _alu(cur.v, m, opn)
                        sym = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/'}[opn]
                        e = '(%s %s %s)' % (cur.e, sym, memexpr)
                    t.set(0, v, e)
                else:
                    # Register forms.  The mnemonic and the operand text are
                    # enough, given the hardware-measured table:
                    #   D8  "fadd st(i)"        ST(0) := ST(0) op ST(i), no pop
                    #   D8  "fsub st(i)"        ST(0) := ST(0) - ST(i)
                    #   D8  "fsubr st(i)"       ST(0) := ST(i) - ST(0)
                    #   DC  "fadd st(i), st(0)" ST(i) := ST(i) op ST(0), no pop
                    #   DE  "faddp st(i)"       ST(i) := ST(i) op ST(0), pops
                    #   DE  "fsubrp st(i)"      ST(i) := ST(0) - ST(i), pops
                    #   DE  "fsubp st(i)"       ST(i) := ST(i) - ST(0), pops
                    mm = re.match(r'\s*st\((\d+)\)(?:\s*,\s*st\((\d+)\))?', o)
                    mnem, i1, i2 = mn, int(mm.group(1)), mm.group(2)
                    i2 = int(i2) if i2 else None
                    popping = mnem.endswith('p')
                    if i2 is None:
                        if popping:
                            dst, src = i1, 0
                        else:
                            dst, src = 0, i1
                    else:
                        dst, src = i1, i2
                    a, b = t.s(dst), t.s(src)
                    opn = {'fadd': 'add', 'faddp': 'add', 'fsub': 'sub',
                           'fsubp': 'sub', 'fsubr': 'rsub', 'fsubrp': 'rsub',
                           'fmul': 'mul', 'fmulp': 'mul', 'fdiv': 'div',
                           'fdivp': 'div', 'fdivr': 'rdiv',
                           'fdivrp': 'rdiv'}[mnem]
                    if opn in ('rsub', 'rdiv'):
                        v = _alu(a.v, b.v, opn)
                        e = '(%s %s %s)' % (b.e, '-' if opn == 'rsub' else '/', a.e)
                    else:
                        v = _alu(a.v, b.v, opn)
                        sym = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/'}[opn]
                        e = '(%s %s %s)' % (a.e, sym, b.e)
                    t.set(dst, v, e)
                    if popping:
                        t.pop()
            elif mn in ('fcom', 'fcomp', 'fcompp'):
                # record the comparison: what was compared with what, and how
                if mn == 'fcompp':
                    t.compare(t.s(0).v, t.s(1).v)
                    t.cmps.append((pc, t.s(0).e, t.s(1).e, t.cmp_flags))
                    t.pop()
                    t.pop()
                elif mn == 'fcomp':
                    if o.startswith('st('):
                        t.compare(t.s(0).v, t.s(st_i(o)).v)
                        t.cmps.append((pc, t.s(0).e, t.s(st_i(o)).e, t.cmp_flags))
                    else:
                        a = parse_addr(o, esp, regs)
                        t.compare(t.s(0).v, mem.get(a, 0.0))
                        t.cmps.append((pc, t.s(0).e, t.load(a, mem.get(a, 0.0)).e, t.cmp_flags))
                    t.pop()
                else:
                    if o.startswith('st('):
                        t.compare(t.s(0).v, t.s(st_i(o)).v)
                        t.cmps.append((pc, t.s(0).e, t.s(st_i(o)).e, t.cmp_flags))
                    else:
                        a = parse_addr(o, esp, regs)
                        t.compare(t.s(0).v, mem.get(a, 0.0))
                        t.cmps.append((pc, t.s(0).e, t.load(a, mem.get(a, 0.0)).e, t.cmp_flags))
            elif mn == 'fnstsw':
                pass
            elif mn in ('mov', 'lea', 'test', 'push', 'pop', 'add', 'sub', 'cmp'):
                if mn == 'mov':
                    dst, src = [x.strip() for x in o.split(',', 1)]
                    if '[' in src:
                        regs[dst] = int(mem.get(parse_addr(src, esp, regs), 0.0))
                    else:
                        regs[dst] = regs.get(src, 0)
                elif mn == 'lea':
                    dst, src = [x.strip() for x in o.split(',', 1)]
                    regs[dst] = parse_addr(src, esp, regs)
            elif mn in ('jp', 'jne', 'je', 'jnp', 'jb', 'jmp', 'cmp', 'test'):
                pass
            elif mn == 'ret':
                break
            else:
                errs.append((pc, mn, op))
        except Exception as ex:
            errs.append((pc, mn, op, str(ex)))

    return t, errs
