"""The interpreter: it runs decoded instructions and holds the state.

The public entry points are

    run_function(code, base, mem=None, reg=None)
        run a block of code at `base' until it returns; the machine is
        returned, with `result' holding the value that was in ST(0).

    Machine(mem=None, reg=None)
        the state itself, for a caller that wants to step.

The FPU condition codes are kept the way the retail sees them: the
comparison instructions set C0/C2/C3, `fnstsw ax' copies them into AH,
and the integer `test ah, imm' sets ZF and PF, which is what the
branches look at.
"""

from .arithmetic import div, r24
from .cpu import Flags, X87
from .decode import REG32, CodeImage, Stop, decode
from .memory import Memory


class Machine:
    """The state of the simulated machine."""

    def __init__(self, mem=None, reg=None, code=None, base=0, mem64=None):
        self.st = X87()
        self.mem = Memory()
        self.flags = Flags()
        self.regs = {'eax': 0, 'ebx': 0, 'ecx': 0, 'edx': 0,
                     'esi': 0, 'edi': 0, 'ebp': 0, 'esp': 0x10000}
        self.image = CodeImage()
        if code is not None:
            self.image.add(base, code)
        if mem:
            self.mem.load_f32(mem)
        if mem64:
            self.mem.load_f64(mem64)
        if reg:
            self.regs.update(reg)
        # The FPU condition codes: C0, C2 and C3.
        self.c0 = 0
        self.c2 = 0
        self.c3 = 0
        self.unordered = False
        self.ah = 0
        self.result = None
        self.trace = None

    # ---- helpers --------------------------------------------------------
    def _addr(self, mem):
        base = self.regs[mem.base] if mem.base else 0
        index = self.regs[mem.index] if mem.index else 0
        return base + index * mem.scale + mem.disp

    def _read_operand(self, op, rm, mem, width):
        """The value of an x87 source operand."""
        if op.src == 'sti':
            return self.st.s(rm)
        if op.src == 'st0':
            return self.st.s(0)
        if op.src == 'st1':
            return self.st.s(1)
        if op.src == 'mem':
            addr = self._addr(mem)
            return self.mem.read_f64(addr) if width == 64 else self.mem.read_f32(addr)
        raise Stop('operand %r' % (op.src,))

    def _write_operand(self, op, rm, mem, width, value):
        """Store an x87 result."""
        if op.dest == 'st0':
            self.st.set(0, value)
        elif op.dest == 'sti':
            self.st.set(rm, value)
        elif op.dest == 'mem':
            addr = self._addr(mem)
            if width == 64:
                self.mem.write_f64(addr, value)
            else:
                self.mem.write_f32(addr, value)
        else:
            raise Stop('destination %r' % (op.dest,))

    def _compare(self, a, b):
        """A comparison sets C0, C2 and C3."""
        if a != a or b != b:
            self.unordered = True
            self.c0 = 1
            self.c2 = 1
            self.c3 = 1
        else:
            self.unordered = False
            self.c0 = 1 if a < b else 0
            self.c2 = 0
            self.c3 = 1 if a == b else 0

    def status_word(self):
        """The status word, as the hardware has it after the last
        comparison: C0 in bit 8, C2 in bit 10, C3 in bit 14."""
        sw = 0
        if self.c0:
            sw |= 0x0100
        if self.c2:
            sw |= 0x0400
        if self.c3:
            sw |= 0x4000
        return sw

    # ---- the x87 side ---------------------------------------------------
    def x87(self, insn):
        args = insn.args
        op = args['op']
        rm = args.get('rm', 0)
        mem = args.get('mem')
        width = args.get('width', 32)

        if insn.name == 'fnstsw':
            self.ah = (self.status_word() >> 8) & 0xff
            return

        if op.push:
            if op.name == 'fld1':
                self.st.push(1.0)
            elif op.src == 'mem':
                addr = self._addr(mem)
                value = self.mem.read_f64(addr) if width == 64 else self.mem.read_f32(addr)
                self.st.push(value)
            else:
                self.st.push(self.st.s(rm))
            return

        if op.op in ('neg', 'abs'):
            value = self.st.s(0)
            self.st.set(0, r24(-value) if op.op == 'neg' else abs(value))
            return

        if op.op == 'free':
            # FFREE empties the register: reading it afterwards gives
            # the indefinite value.
            self.st.set(rm, float('nan'))
            return

        if op.op == 'copy':
            # ST(0) := ST(i), no arithmetic and no push.
            self.st.set(0, self.st.s(rm))
            return

        if op.op == 'cmp':
            a = self.st.s(0)
            b = self._read_operand(op, rm, mem, width)
            self._compare(a, b)
            for _ in range(op.pops):
                self.st.pop()
            return

        if op.name == 'fxch':
            a = self.st.s(0)
            c = self.st.s(rm)
            self.st.set(0, c)
            self.st.set(rm, a)
            return

        if op.name == 'ffree':
            return

        if op.name == 'fst':
            self._write_operand(op, rm, mem, width, self.st.s(0))
            return

        if op.name == 'fstp':
            # FSTP ST(i): ST(i) := ST(0), and *then* pop.
            self._write_operand(op, rm, mem, width, self.st.s(0))
            self.st.pop()
            return

        # the arithmetic forms
        if op.dest == 'st0':
            a = self.st.s(0)
        else:
            a = self.st.s(rm)
        b = self._read_operand(op, rm, mem, width)
        if op.op == 'add':
            value = r24(a + b)
        elif op.op == 'mul':
            value = r24(a * b)
        elif op.op == 'sub':
            value = r24(a - b)
        elif op.op == 'rsub':
            value = r24(b - a)
        elif op.op == 'div':
            value = r24(div(a, b))
        elif op.op == 'rdiv':
            value = r24(div(b, a))
        else:
            raise Stop('unhandled x87 %s' % insn.name)
        self._write_operand(op, rm, mem, width, value)
        for _ in range(op.pops):
            self.st.pop()

    # ---- the integer side ------------------------------------------------
    def _condition(self, cond):
        """Evaluate a jump condition."""
        if cond == 'je':
            return self.flags.zf
        if cond == 'jne':
            return not self.flags.zf
        if cond == 'jp':
            return self.flags.pf
        if cond == 'jnp':
            return not self.flags.pf
        c0 = self.ah & 0x01
        c3 = (self.ah & 0x40) != 0
        if cond == 'jb':
            return c0 == 0 and c3
        if cond == 'jae':
            return c0 != 0 or not c3
        if cond == 'jbe':
            return c0 != 0 or not c3
        if cond == 'ja':
            return c0 == 0 and not c3
        if cond == 'jl':
            return c0 != c3
        if cond == 'jge':
            return c0 == c3
        if cond == 'jle':
            return c0 != c3 or c0 != 0
        if cond == 'jg':
            return c0 == c3 and c0 == 0
        if cond == 'jo':
            return self.flags.pf
        if cond == 'jno':
            return not self.flags.pf
        raise Stop('unhandled condition %s' % cond)

    def cpu(self, insn):
        args = insn.args
        name = insn.name

        if name in ('add', 'sub'):
            imm = args['imm']
            if args['mem'] is not None:
                addr = self._addr(args['mem'])
                value = self.mem.read_f32(addr)
                self.mem.write_f32(addr, value + imm if name == 'add' else value - imm)
            else:
                reg = args['reg']
                self.regs[reg] += imm if name == 'add' else -imm
        elif name == 'mov':
            if args['mem'] is not None:
                value = int(self.mem.read_f32(self._addr(args['mem'])))
            else:
                value = self.regs[REG32[args['src_reg']]]
            self.regs[args['dst']] = value & 0xffffffff
        elif name == 'mov_store':
            reg = REG32[args['reg']]
            value = self.regs[reg]
            addr = self._addr(args['mem'])
            self.mem.write_bytes(addr, (value & 0xffffffff).to_bytes(4, 'little'))
        elif name == 'lea':
            self.regs[args['dst']] = self._addr(args['mem']) & 0xffffffff
        elif name == 'push':
            self.regs['esp'] -= 4
        elif name == 'pop':
            self.regs['esp'] += 4
        elif name == 'test_ah':
            self.flags.test_ah(self.ah, args['imm'])
        elif name == 'jcc':
            if self._condition(args['cond']):
                return args['target']
        elif name == 'jmp':
            return args['target']
        elif name == 'ret':
            self.result = self.st.s(0)
            return None
        elif name == 'nop':
            pass
        else:
            raise Stop('unhandled cpu %s' % name)
        return insn.pc + insn.size

    # ---- the interpreter -------------------------------------------------
    def step(self, pc):
        """Execute the instruction at `pc'; returns the next pc, or None
        when the code returned."""
        insn = decode(self.image, pc)
        if self.trace is not None:
            self.trace.append((pc, insn.name, insn.args, self.st.snapshot()))
        if insn.kind == 'x87':
            # no x87 instruction branches; they all fall through
            self.x87(insn)
            return insn.pc + insn.size
        return self.cpu(insn)

    def run(self, pc, max_steps=500000):
        """Run from `pc' until the code returns."""
        for _ in range(max_steps):
            nxt = self.step(pc)
            if nxt is None:
                return
            pc = nxt
        raise Stop('too many instructions')


def run_function(code, base, mem=None, reg=None, mem64=None, max_steps=500000):
    """Run a block of code at `base' and return the machine.

    `mem` is a {address: value} mapping of 32-bit float slots, `mem64`
    one of 64-bit double slots, `reg` a {register: value} mapping.
    """
    m = Machine(mem=mem, reg=reg, code=code, base=base, mem64=mem64)
    m.run(base, max_steps)
    return m
