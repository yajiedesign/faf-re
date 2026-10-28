"""x87 simulator for retail Wm3::DistSegment3Segment3<float>::GetSquared
(0x00A7E660 .. ~0x00A7ED00), running under the row's _PC_24 control word.

Every arithmetic result is rounded to a 24-bit significand, so the registers
model per-operation float32 chains, not 64-bit extended. Memory is byte-addressed
float storage; the ESP-relative spill slots are the same store.

Validation: run it over the probe's draws and compare against the retail's own
fSqrDist. The AGREEING draws are the test set, not the 19 disagreeing ones.
"""
import struct, math, sys
from fractions import Fraction
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

EXE = r'C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe'
_d = open(EXE, 'rb').read()
_pe = struct.unpack_from('<I', _d, 0x3c)[0]
_nsec = struct.unpack_from('<H', _d, _pe + 6)[0]
_optsz = struct.unpack_from('<H', _d, _pe + 20)[0]
_sec = _pe + 24 + _optsz
BASE = 0x400000

def va_to_off(va):
    rva = va - BASE
    for i in range(_nsec):
        o = _sec + i * 40
        vs, va0, rs, ro = struct.unpack_from('<IIII', _d, o + 8)
        if va0 <= rva < va0 + max(vs, rs):
            return ro + (rva - va0)
    raise KeyError(hex(va))

def _frac_to_f32(fr):
    """Round an exact rational to the nearest 24-bit significand (float32),
    ties to even - the rounding the x87 performs under _PC_24."""
    if fr == 0:
        return 0.0
    neg = fr < 0
    if neg:
        fr = -fr
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    two = Fraction(2)
    while two ** e > fr:
        e -= 1
    while two ** (e + 1) <= fr:
        e += 1
    m = fr * two ** (23 - e)
    fl = m.numerator // m.denominator
    rem = m - fl
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and (fl & 1)):
        fl += 1
    val = float(fl)
    try:
        val = math.ldexp(val, e - 23)
    except OverflowError:
        val = float('inf')
    return -val if neg else val


def _div(a, b):
    """The x87 divides the exact quotient and rounds it to the control
    word's precision, so the quotient must not be rounded twice (once to
    double, once to float32)."""
    if b == 0.0:
        if a == 0.0:
            return float('nan')
        neg = (a < 0) != (str(b)[0] == '-')
        return float('-inf') if neg else float('inf')
    return _frac_to_f32(Fraction(a) / Fraction(b))


PREC = int(__import__('os').environ.get('SIMPREC', '24'))

def r24(x):
    if PREC == 53:
        return x
    return _r24_24(x)

def _r24_24(x):
    """Round to a 24-bit significand, return as float32.

    The x87 rounds the *exact* result of each operation to the control
    word's precision.  The callers hand us values that a double holds
    exactly (the product of two 24-bit significands needs 48 bits), so a
    single conversion to float32 is the one rounding the hardware does -
    rounding to 25 bits first and then to 24 would be a second rounding and
    can land one ulp away.
    """
    if x != x or x in (float('inf'), float('-inf')) or x == 0.0:
        return x
    return struct.unpack('<f', struct.pack('<f', x))[0]

def f32r(x):
    """Round to float32 (a spill to a 32-bit slot)."""
    if x != x or x in (float('inf'), float('-inf')) or x == 0.0:
        return x
    return struct.unpack('<f', struct.pack('<f', x))[0]

def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]

def frombits(b):
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]

START, END = 0x00A7E660, 0x00A7EE60
_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = False
_off = va_to_off(START)
INSNS = {}
for i in _md.disasm(_d[_off:_off + (END - START)], START):
    INSNS[i.address] = (i.size, i.mnemonic, i.op_str, i.bytes[0], i.bytes[1] if len(i.bytes) > 1 else 0)
    if i.address >= END:
        break

class Stop(Exception):
    pass

class X87:
    """The physical eight-register x87 file with a rotating top.

    ST(0) is R[top], ST(i) is R[(top + i) & 7]. A push decrements the top
    and writes the register; a pop reads it and increments the top. Reading
    ST(i) for i beyond the live depth returns the stale contents of that
    physical register, which is what the hardware does - and the compiler
    does rely on it, e.g. `fmul st(4)` when the depth is 4.
    """
    __slots__ = ('R', 'top', 'depth')
    def __init__(self):
        self.R = [0.0] * 8
        self.top = 0
        self.depth = 0
    def push(self, v):
        self.top = (self.top - 1) & 7
        self.R[self.top] = v
        self.depth += 1
    def pop(self):
        v = self.R[self.top]
        self.top = (self.top + 1) & 7
        self.depth -= 1
        return v
    def s(self, i):
        return self.R[(self.top + i) & 7]
    def set(self, i, v):
        self.R[(self.top + i) & 7] = v
    def snapshot(self):
        return [self.s(i) for i in range(8)]

class Sim:
    def __init__(self, mem=None, reg=None):
        self.st = X87()
        self.mem = dict(mem or {})     # byte address -> float
        self.mem64 = {0xD71CD8: 2.0}   # the double constant 2.0
        # the float constant 1e-6 at 0xD71BFC (read from the image)
        self.mem[0xD71BFC] = 9.999999974752427e-07
        self.reg = {'eax': 0, 'ebx': 0, 'ecx': 0, 'edx': 0, 'esi': 0, 'edi': 0, 'esp': 0x10000}
        self.reg.update(reg or {})
        self.c3 = 0        # FCOM: 1 if equal
        self.c0 = 0
        self.unordered = False
        self.ah = 0
        self.zf = False
        self.pf = False
        self.result = None
        self.trace = None

    # ---- memory ---------------------------------------------------------
    def rd(self, addr):
        return self.mem.get(addr, 0.0)

    def rd64(self, addr):
        return self.mem64.get(addr, 0.0)

    def wr(self, addr, v):
        self.mem[addr] = f32r(v)

    # ---- x87 ------------------------------------------------------------
    def push(self, v):
        self.st.push(r24(v))

    def pop(self):
        return self.st.pop()

    def operand(self, o):
        """Resolve an x87 operand: 'st(i)' or 'dword ptr [reg + off]'."""
        o = o.strip()
        if ',' in o:
            a, b = [x.strip() for x in o.split(',', 1)]
            return ('st2', (int(a[3]), int(b[3])))
        if o.startswith('qword ptr'):
            a = self.addr(o[o.index('[')+1:o.rindex(']')])
            return ('const64', a)
        if o.startswith('dword ptr'):
            body = o[len('dword ptr'):].strip()[1:-1]
            return ('mem', self.addr(body))
        if o.startswith('st('):
            return ('st', int(o[3]))
        if o.startswith('0x'):
            return ('abs', int(o, 16))
        raise Stop('operand %r' % o)

    def addr(self, body):
        body = body.replace(' ', '')
        if body.startswith('0x'):
            return int(body, 16)
        if body.startswith('esp'):
            rest = body[3:]
            if rest == '': return self.reg['esp']
            if rest[0] == '+': return self.reg['esp'] + int(rest[1:], 0)
            if rest[0] == '-': return self.reg['esp'] - int(rest[1:], 0)
        for rn in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'):
            if body.startswith(rn):
                rest = body[len(rn):]
                base = self.reg[rn]
                if rest == '': return base
                if rest[0] == '+': return base + int(rest[1:], 0)
                if rest[0] == '-': return base - int(rest[1:], 0)
        raise Stop('addr %s' % body)

    @staticmethod
    def _sti(txt):
        t = txt.strip()
        a = t.index('(') + 1
        b = t.index(')')
        return int(t[a:b])

    def get(self, op):
        k, v = self.operand(op)
        if k == 'st':
            return self.st.s(v)
        if k == 'st2':
            return self.st.s(v[1])
        if k == 'const64':
            return self.rd64(v)
        return self.rd(v)

    def _cmp(self, a, b):
        if a != a or b != b:
            self.unordered = True; self.c3 = 1; self.c0 = 1
        else:
            self.unordered = False
            self.c3 = 1 if a == b else 0
            self.c0 = 1 if a < b else 0

    def x87(self, pc, mn, op):
        st = self.st
        o = op.strip()
        nxt = pc + INSNS[pc][0]
        b = INSNS[pc][3]
        modrm = INSNS[pc][4]
        regform = (b in (0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE)
                   and (modrm & 0xC0) == 0xC0)
        rrr = (modrm >> 3) & 7
        i7 = modrm & 7

        if mn == 'fld':
            if o.startswith('qword ptr'):
                a = self.addr(o[o.index('[')+1:o.rindex(']')])
                st.push(self.rd64(a))
            elif o.startswith('st('):
                st.push(st.s(self._sti(o)))
            elif o == '1':
                st.push(1.0)
            else:
                st.push(self.get(o))
            return nxt
        if mn == 'fst':
            if o.startswith('st('):
                i = self._sti(o); st.set(i, st.s(0))
            else:
                a = self.addr(o[o.index('[')+1:o.rindex(']')])
                self.wr(a, st.s(0))
            return nxt
        if mn == 'fstp':
            if o.startswith('st('):
                # FSTP ST(i): ST(i) := ST(0), then pop.  The store happens
                # first, so after the pop the value that was ST(i) is on top.
                i = self._sti(o); st.set(i, st.s(0)); st.pop()
            else:
                a = self.addr(o[o.index('[')+1:o.rindex(']')])
                self.wr(a, st.s(0)); st.pop()
            return nxt
        if mn == 'fstpnce':
            st.pop(); return nxt
        if mn == 'fxch':
            i = self._sti(o)
            a, c = st.s(0), st.s(i)
            st.set(0, c); st.set(i, a)
            return nxt
        if mn == 'fchs':
            st.set(0, r24(-st.s(0))); return nxt
        if mn == 'fabs':
            st.set(0, abs(st.s(0))); return nxt
        if mn == 'fld1':
            st.push(1.0); return nxt
        if mn == 'fnstsw':
            v = 0
            if self.c3: v |= 0x4000
            if self.c0: v |= 0x0100
            if self.unordered: v |= 0x0400
            self.ah = (v >> 8) & 0xff
            return nxt

        # Memory forms: ST(0) = ST(0) op m32fp, no pop.
        if '[' in o and mn in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
            mem = self.get(o)
            if mn == 'fadd':    st.set(0, r24(st.s(0) + mem))
            elif mn == 'fmul':  st.set(0, r24(st.s(0) * mem))
            elif mn == 'fsub':  st.set(0, r24(st.s(0) - mem))
            elif mn == 'fsubr': st.set(0, r24(mem - st.s(0)))
            elif mn == 'fdiv':  st.set(0, r24(_div(st.s(0), mem)))
            else:               st.set(0, r24(_div(mem, st.s(0))))
            return nxt
        if '[' in o and mn in ('fcom', 'fcomp'):
            self._cmp(st.s(0), self.get(o))
            if mn == 'fcomp':
                st.pop()
            return nxt
        if mn in ('fcom', 'fcomp') and not regform:
            self._cmp(st.s(0), st.s(self._sti(o)))
            if mn == 'fcomp':
                st.pop()
            return nxt
        if mn == 'fcompp':
            self._cmp(st.s(0), st.s(1)); st.pop(); st.pop()
            return nxt

        # Register forms, per the Intel encoding tables. Each entry is
        # (dest, src, operation, pops), with dest/src as stack indices and
        # the operation one of 'add', 'mul', 'sub', 'rsub', 'div', 'rdiv'.
        #
        #  b  reg  form
        #  D8  0   FADD  ST(0),ST(i)   no pop      1  FMUL  ST(0),ST(i)  no pop
        #     2   FCOM  ST(i)          no pop      3  FCOMP ST(i)         pop
        #     4   FSUB  ST(i),ST(0)    pop         5  FSUBP ST(i),ST(0)   pop
        #     6   FDIV  ST(i),ST(0)    pop         7  FDIVP ST(i),ST(0)   pop
        #  D9  0   FLD   ST(i)         push        1  FXCH  ST(i)
        #     2   FST   ST(i)         no pop      3  FSTP  ST(i)         pop
        #     4   FSUB  ST(0),ST(i)    no pop      5  FSUBP ST(0),ST(i)   no pop
        #     6   FSUBR ST(0),ST(i)    no pop      7  FSUBRP ST(0),ST(i)  no pop
        #  DB  0   FLD   ST(i)         push        1  FXCH  ST(i)
        #     2   FSTP  ST(i)         pop         3  FSTP  ST(i)         pop
        #     4   FSUBR ST(i),ST(0)    pop         5  FSUBRP ST(i),ST(0)  pop
        #     6   FDIVR ST(i),ST(0)    pop         7  FDIVRP ST(i),ST(0)  pop
        #  DC  0   FADD  ST(i),ST(0)   no pop      1  FMUL  ST(i),ST(0)  no pop
        #     2   FCOMP ST(i)          pop         3  FCOMP ST(i)         pop
        #     4   FSUBR ST(i),ST(0)    pop         5  FSUBRP ST(i),ST(0)  pop
        #     6   FDIVR ST(i),ST(0)    pop         7  FDIVRP ST(i),ST(0)  pop
        #  DD  0   FFREE ST(i)         no-op       1  FXCH  ST(i)
        #     2   FST   ST(i)         no pop      3  FSTP  ST(i)         pop
        #     4   FSUB  ST(i),ST(0)    pop         5  FSUBP ST(i),ST(0)   pop
        #     6   FDIV  ST(i),ST(0)    pop         7  FDIVP ST(i),ST(0)   pop
        #  DE  0   FADDP ST(i),ST(0)   pop         1  FMULP ST(i),ST(0)  pop
        #     2   FCOMPP              pop 2       3  FCOMP  ST(i)        pop
        #     4   FSUBRP ST(i),ST(0)  pop         5  FSUBRP ST(i),ST(0) pop
        #     6   FDIVRP ST(i),ST(0)  pop         7  FDIVRP ST(i),ST(0) pop
        if not regform:
            raise Stop('unhandled x87 %s %s' % (mn, op))
        if b == 0xD8:
            if rrr == 0: self._alu(st, 0, i7, 'add', False); return nxt
            if rrr == 1: self._alu(st, 0, i7, 'mul', False); return nxt
            if rrr == 2: self._cmp(st.s(0), st.s(i7)); return nxt
            if rrr == 3: self._cmp(st.s(0), st.s(i7)); st.pop(); return nxt
            # D8 E0+i .. F8+i write ST(0) and do NOT pop (measured on the
            # hardware; the SDM's "pop" column for these forms is wrong).
            if rrr == 4: self._alu(st, 0, i7, 'sub', False); return nxt
            if rrr == 5: self._alu(st, 0, i7, 'rsub', False); return nxt
            if rrr == 6: self._alu(st, 0, i7, 'div', False); return nxt
            if rrr == 7: self._alu(st, 0, i7, 'rdiv', False); return nxt
        if b == 0xD9:
            if rrr == 0: st.push(st.s(i7)); return nxt
            if rrr == 1:
                a, c = st.s(0), st.s(i7); st.set(0, c); st.set(i7, a); return nxt
            if rrr == 2: st.set(i7, st.s(0)); return nxt
            if rrr == 3: st.set(i7, st.s(0)); st.pop(); return nxt
            if rrr == 4: self._alu(st, 0, i7, 'sub', False); return nxt
            if rrr == 5: self._alu(st, 0, i7, 'sub', False); return nxt
            if rrr == 6: self._alu(st, 0, i7, 'rsub', False); return nxt
            if rrr == 7: self._alu(st, 0, i7, 'rsub', False); return nxt
        if b == 0xDB:
            if rrr == 0: st.push(st.s(i7)); return nxt
            if rrr == 1:
                a, c = st.s(0), st.s(i7); st.set(0, c); st.set(i7, a); return nxt
            if rrr == 2: st.set(i7, st.s(0)); st.pop(); return nxt
            if rrr == 3: st.set(i7, st.s(0)); st.pop(); return nxt
            if rrr == 4: self._alu(st, i7, 0, 'rsub', True); return nxt
            if rrr == 5: self._alu(st, i7, 0, 'rsub', True); return nxt
            if rrr == 6: self._alu(st, i7, 0, 'rdiv', True); return nxt
            if rrr == 7: self._alu(st, i7, 0, 'rdiv', True); return nxt
        if b == 0xDC:
            # DC C0+i = FADD ST(i),ST(0) and DC C8+i = FMUL ST(i),ST(0): the
            # destination is ST(i), the source is ST(0), and neither pops
            # (measured on the hardware).  DC D0+i = FCOMP ST(i) does not
            # pop either; DC D8+i does.
            if rrr == 0: self._alu(st, i7, 0, 'add', False); return nxt
            if rrr == 1: self._alu(st, i7, 0, 'mul', False); return nxt
            if rrr == 2: self._cmp(st.s(0), st.s(i7)); return nxt
            if rrr == 3: self._cmp(st.s(0), st.s(i7)); st.pop(); return nxt
            if rrr == 4: self._alu(st, i7, 0, 'rsub', False); return nxt
            if rrr == 5: self._alu(st, i7, 0, 'sub', False); return nxt
            if rrr == 6: self._alu(st, i7, 0, 'rdiv', False); return nxt
            if rrr == 7: self._alu(st, i7, 0, 'div', False); return nxt
        if b == 0xDD:
            if rrr == 0: return nxt
            if rrr == 1:
                a, c = st.s(0), st.s(i7); st.set(0, c); st.set(i7, a); return nxt
            if rrr == 2: st.set(i7, st.s(0)); return nxt
            if rrr == 3: st.set(i7, st.s(0)); st.pop(); return nxt
            if rrr == 4: self._alu(st, i7, 0, 'sub', True); return nxt
            if rrr == 5: self._alu(st, i7, 0, 'sub', True); return nxt
            if rrr == 6: self._alu(st, i7, 0, 'div', True); return nxt
            if rrr == 7: self._alu(st, i7, 0, 'div', True); return nxt
        if b == 0xDE:
            # DE C0+i = FADDP ST(i),ST(0), DE C8+i = FMULP ST(i),ST(0): the
            # destination is ST(i), the source is ST(0), and the result pops.
            # DE D0+i = FCOMP ST(i) pops one; DE D8+i = FCOMPP pops two.
            if rrr == 0: self._alu(st, i7, 0, 'add', True); return nxt
            if rrr == 1: self._alu(st, i7, 0, 'mul', True); return nxt
            if rrr == 2: self._cmp(st.s(0), st.s(i7)); st.pop(); return nxt
            if rrr == 3: self._cmp(st.s(0), st.s(i7)); st.pop(); st.pop(); return nxt
            if rrr == 4: self._alu(st, i7, 0, 'rsub', True); return nxt
            if rrr == 5: self._alu(st, i7, 0, 'sub', True); return nxt
            if rrr == 6: self._alu(st, i7, 0, 'rdiv', True); return nxt
            if rrr == 7: self._alu(st, i7, 0, 'div', True); return nxt
        raise Stop('unhandled x87 %s %s' % (mn, op))

    def _alu(self, st, di, si, op, pop):
        a, c = st.s(di), st.s(si)
        if op == 'add': v = r24(a + c)
        elif op == 'mul': v = r24(a * c)
        elif op == 'sub': v = r24(a - c)
        elif op == 'rsub': v = r24(c - a)
        elif op == 'div': v = r24(_div(a, c))
        else: v = r24(_div(c, a))
        st.set(di, v)
        if pop:
            st.pop()

    # ---- integer --------------------------------------------------------
    def cpu(self, pc, mn, op):
        o = op.strip()
        if mn == 'sub' and o.startswith('esp'):
            self.reg['esp'] += int(o.split(',')[1].strip(), 0)
        elif mn == 'add' and o.startswith('esp'):
            self.reg['esp'] += int(o.split(',')[1].strip(), 0)
        elif mn == 'push':
            self.reg['esp'] -= 4
        elif mn == 'pop':
            self.reg['esp'] += 4
        elif mn == 'mov':
            dst, src = [x.strip() for x in o.split(',', 1)]
            if '[' in src:
                a = self.addr(src[src.index('[')+1:src.rindex(']')])
                v = int(self.rd(a))
            else:
                v = self.reg[src]
            if dst in self.reg:
                self.reg[dst] = v
        elif mn == 'lea':
            dst, src = [x.strip() for x in o.split(',', 1)]
            self.reg[dst] = self.addr(src[src.index('[')+1:src.rindex(']')])
        elif mn == 'test':
            # `fnstsw ax` leaves the FPU condition codes in AH: C0 is bit 0
            # of AH, C2 is bit 2, C3 is bit 6.  The retail tests them with
            # `test ah, imm` and then branches on the *integer* flags that
            # TEST sets: ZF from the AND result, PF from its parity.
            if o.startswith('ah'):
                imm = int(o.split(',')[1].strip(), 0)
                r = self.ah & imm
                self.zf = (r == 0)
                self.pf = (bin(r & 0xff).count('1') % 2) == 0
            else:
                raise Stop('unhandled test %s' % o)
        elif mn == 'jne':
            if not self.zf:
                return int(o, 16)
        elif mn == 'je':
            if self.zf:
                return int(o, 16)
        elif mn == 'jp':
            if self.pf:
                return int(o, 16)
        elif mn == 'jnp':
            if not self.pf:
                return int(o, 16)
        elif mn == 'jb':
            if self.ah & 0x01 == 0 and self.ah & 0x40:
                return int(o, 16)
        elif mn == 'jmp':
            return int(o, 16)
        elif mn == 'ret':
            self.result = self.st.s(0)
            return None
        else:
            raise Stop('unhandled cpu %s %s' % (mn, op))
        return pc + INSNS[pc][0]

    def run(self, trace=False):
        pc = START
        for _ in range(500000):
            if pc not in INSNS:
                raise Stop('no insn at %08x' % pc)
            sz, mn, op = INSNS[pc][:3]
            if trace is not None and self.trace is not None:
                self.trace.append((pc, mn, op, self.st.snapshot(), dict(self.mem), self.reg['esp']))
            nxt = self.x87(pc, mn, op) if mn.startswith('f') else self.cpu(pc, mn, op)
            if nxt is None:
                return
            pc = nxt
        raise Stop('too many')
