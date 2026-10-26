"""The x87 semantics: opcode + ModRM -> what the instruction does.

The decode is done from the *raw* opcode and ModRM bytes, not from a
disassembler's text: the D8/DC/DE families differ in destination
register and pop behaviour, and the SDM's spelling of them is not
trustworthy.  The rules below are the ones measured on the hardware
(see ../hwprobe):

  D8's arithmetic forms write ST(0) and do *not* pop;
  DC writes ST(i) and does *not* pop;
  DE is the popping twin of DC;
  FSTP ST(i) stores ST(0) into ST(i) *and then* pops.

The eight-register file is physical with a rotating top, so ST(i) is
readable past the live depth - the compiler relies on this
(`fmul st(4)` with a depth of 4).

An opcode that is not in the tables raises Stop('unhandled x87 ...')
rather than guessing.
"""

import struct
from collections import namedtuple

# The general purpose registers, in ModRM order.
REG32 = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')

# A memory operand: (base register or None, index register or None,
# scale, displacement).
MemOp = namedtuple('MemOp', 'base index scale disp')


class Stop(Exception):
    """Raised for an instruction the simulator does not implement."""


class X87Op(namedtuple('X87Op', 'name dest src op pops push')):
    """What one x87 encoding does.

    dest/src are 'st0', 'sti' or 'mem'; op is 'add', 'mul', 'sub',
    'rsub', 'div', 'rdiv', 'cmp', 'neg', 'abs', 'one' or None; pops
    is the number of registers the instruction pops; push says whether
    it pushes.
    """

    __slots__ = ()


# DF E0 = FNSTSW AX.
_FNSTSW_AX = X87Op('fnstsw', None, None, None, 0, False)

# ---------------------------------------------------------------------------
# The register forms (ModRM with mod == 3).
#
# Not every encoding the manuals mention exists on the hardware: the
# x87 decoder only accepts a subset of them, and the rest raise an
# invalid instruction exception.  The `rm' sets below are the ones
# measured on the machine (see ../hwprobe); an encoding outside them
# raises Stop, like any other unimplemented opcode.
# ---------------------------------------------------------------------------
_ALL = tuple(range(8))

_REG_TABLE = {
    0xD8: {
        0: (X87Op('fadd', 'st0', 'sti', 'add', 0, False), _ALL),
        1: (X87Op('fmul', 'st0', 'sti', 'mul', 0, False), _ALL),
        2: (X87Op('fcom', 'st0', 'sti', 'cmp', 0, False), _ALL),
        3: (X87Op('fcomp', 'st0', 'sti', 'cmp', 1, False), _ALL),
        # D8 E0+i .. F8+i write ST(0) and do NOT pop (measured on the
        # hardware; the SDM's "pop" column for these forms is wrong).
        4: (X87Op('fsub', 'st0', 'sti', 'sub', 0, False), _ALL),
        5: (X87Op('fsubr', 'st0', 'sti', 'rsub', 0, False), _ALL),
        6: (X87Op('fdiv', 'st0', 'sti', 'div', 0, False), _ALL),
        7: (X87Op('fdivr', 'st0', 'sti', 'rdiv', 0, False), _ALL),
    },
    0xD9: {
        0: (X87Op('fld', 'sti', None, None, 0, True), _ALL),
        1: (X87Op('fxch', 'sti', None, None, 0, False), _ALL),
        # FST ST(i) exists for i == 0 only.
        2: (X87Op('fst', 'sti', None, None, 0, False), (0,)),
        3: (X87Op('fstp', 'sti', None, None, 1, False), _ALL),
    },
    0xDB: {
        # The DB family does not do what the manuals say: measured on
        # the hardware, DB C0+i .. DB D8+i copy ST(i) into ST(0)
        # without pushing, and the arithmetic forms (DB E0+i, E8+i,
        # F0+i) do nothing at all.  The copy is what is implemented
        # here; the rest is left unimplemented, because the retail
        # never uses the DB family and the measured behaviour of the
        # arithmetic forms is not something to model.
        0: (X87Op('fldst', 'st0', 'sti', 'copy', 0, False), _ALL),
        1: (X87Op('fldst', 'st0', 'sti', 'copy', 0, False), _ALL),
        2: (X87Op('fstst', 'st0', 'sti', 'copy', 0, False), _ALL),
        3: (X87Op('fstpst', 'st0', 'sti', 'copy', 0, False), _ALL),
    },
    0xDC: {
        0: (X87Op('fadd', 'sti', 'st0', 'add', 0, False), _ALL),
        1: (X87Op('fmul', 'sti', 'st0', 'mul', 0, False), _ALL),
        2: (X87Op('fcom', 'st0', 'sti', 'cmp', 0, False), _ALL),
        3: (X87Op('fcomp', 'st0', 'sti', 'cmp', 1, False), _ALL),
        4: (X87Op('fsubr', 'sti', 'st0', 'rsub', 0, False), _ALL),
        5: (X87Op('fsub', 'sti', 'st0', 'sub', 0, False), _ALL),
        6: (X87Op('fdivr', 'sti', 'st0', 'rdiv', 0, False), _ALL),
        7: (X87Op('fdiv', 'sti', 'st0', 'div', 0, False), _ALL),
    },
    0xDD: {
        # FFREE empties the register: reading it afterwards gives the
        # indefinite value.
        0: (X87Op('ffree', 'sti', None, 'free', 0, False), _ALL),
        1: (X87Op('fxch', 'sti', None, None, 0, False), _ALL),
        2: (X87Op('fst', 'sti', None, None, 0, False), _ALL),
        3: (X87Op('fstp', 'sti', None, None, 1, False), _ALL),
        # DD E0+i and E8+i are the unordered comparisons FUCOM and
        # FUCOMP, not subtractions (measured on the hardware).
        4: (X87Op('fucom', 'st0', 'sti', 'cmp', 0, False), _ALL),
        5: (X87Op('fucomp', 'st0', 'sti', 'cmp', 1, False), _ALL),
    },
    0xDE: {
        0: (X87Op('faddp', 'sti', 'st0', 'add', 1, False), _ALL),
        1: (X87Op('fmulp', 'sti', 'st0', 'mul', 1, False), _ALL),
        2: (X87Op('fcomp', 'st0', 'sti', 'cmp', 1, False), _ALL),
        # FCOMPP is DE D9; the other rm values are not encodings.
        3: (X87Op('fcompp', 'st0', 'st1', 'cmp', 2, False), (1,)),
        4: (X87Op('fsubrp', 'sti', 'st0', 'rsub', 1, False), _ALL),
        5: (X87Op('fsubp', 'sti', 'st0', 'sub', 1, False), _ALL),
        6: (X87Op('fdivrp', 'sti', 'st0', 'rdiv', 1, False), _ALL),
        7: (X87Op('fdivp', 'sti', 'st0', 'div', 1, False), _ALL),
    },
    0xDF: {
        # DF E0 = FNSTSW AX; the other forms of the DF family are not
        # implemented here.
        4: (_FNSTSW_AX, (0,)),
    },
}

# D9 with mod == 3 encodes the special functions in the whole ModRM
# range E0..FF; the ones the hardware implements are E0, E1, E4, E5,
# E8..EE and F0..FF.  Of those, the simulator implements FCHS, FABS
# and FLD1; the rest raise Stop.
_D9_SPECIAL = {
    0xE0: X87Op('fchs', 'st0', None, 'neg', 0, False),
    0xE1: X87Op('fabs', 'st0', None, 'abs', 0, False),
    0xE8: X87Op('fld1', None, None, 'one', 0, True),
}

# ---------------------------------------------------------------------------
# The memory forms (ModRM with mod != 3).  `size' is the width of the
# memory operand: 32 for a float32, 64 for a double.
# ---------------------------------------------------------------------------
_MEM_TABLE = {
    0xD8: {
        0: (X87Op('fadd', 'st0', 'mem', 'add', 0, False), 32),
        1: (X87Op('fmul', 'st0', 'mem', 'mul', 0, False), 32),
        2: (X87Op('fcom', 'st0', 'mem', 'cmp', 0, False), 32),
        3: (X87Op('fcomp', 'st0', 'mem', 'cmp', 1, False), 32),
        4: (X87Op('fsub', 'st0', 'mem', 'sub', 0, False), 32),
        5: (X87Op('fsubr', 'st0', 'mem', 'rsub', 0, False), 32),
        6: (X87Op('fdiv', 'st0', 'mem', 'div', 0, False), 32),
        7: (X87Op('fdivr', 'st0', 'mem', 'rdiv', 0, False), 32),
    },
    0xD9: {
        0: (X87Op('fld', None, 'mem', None, 0, True), 32),
        2: (X87Op('fst', 'mem', 'st0', None, 0, False), 32),
        3: (X87Op('fstp', 'mem', 'st0', None, 1, False), 32),
    },
    0xDB: {
        # The DB family does not behave as the manuals say on this
        # hardware: its memory forms load integers and store zero.
        # None of them is implemented.
    },
    0xDC: {
        # The DC family's memory forms read a 64-bit double on this
        # hardware (measured), not a 32-bit float.
        0: (X87Op('fadd', 'st0', 'mem', 'add', 0, False), 64),
        1: (X87Op('fmul', 'st0', 'mem', 'mul', 0, False), 64),
        # DC /2 and DC /3 are FCOM and FCOMP on a 64-bit double - the
        # forms the retail's double code uses for its comparisons (the
        # retail's DistSegment3Segment3<double>::GetSquared has several).
        2: (X87Op('fcom', 'st0', 'mem', 'cmp', 0, False), 64),
        3: (X87Op('fcomp', 'st0', 'mem', 'cmp', 1, False), 64),
        4: (X87Op('fsub', 'st0', 'mem', 'sub', 0, False), 64),
        5: (X87Op('fsubr', 'st0', 'mem', 'rsub', 0, False), 64),
        6: (X87Op('fdiv', 'st0', 'mem', 'div', 0, False), 64),
        7: (X87Op('fdivr', 'st0', 'mem', 'rdiv', 0, False), 64),
    },
    0xDD: {
        0: (X87Op('fld', None, 'mem', None, 0, True), 64),
        2: (X87Op('fst', 'mem', 'st0', None, 0, False), 64),
        3: (X87Op('fstp', 'mem', 'st0', None, 1, False), 64),
    },
}

# The jump conditions the integer side understands.
_CONDITIONS = {
    0x70: 'jo', 0x71: 'jno', 0x72: 'jb', 0x73: 'jae',
    0x74: 'je', 0x75: 'jne', 0x76: 'jbe', 0x77: 'ja',
    0x78: 'js', 0x79: 'jns', 0x7A: 'jp', 0x7B: 'jnp',
    0x7C: 'jl', 0x7D: 'jge', 0x7E: 'jle', 0x7F: 'jg',
    0x80: 'jo', 0x81: 'jno', 0x82: 'jb', 0x83: 'jae',
    0x84: 'je', 0x85: 'jne', 0x86: 'jbe', 0x87: 'ja',
    0x88: 'js', 0x89: 'jns', 0x8A: 'jp', 0x8B: 'jnp',
    0x8C: 'jl', 0x8D: 'jge', 0x8E: 'jle', 0x8F: 'jg',
}


class Insn:
    """One decoded instruction.

    `kind' is 'x87' for the FPU instructions and 'cpu' for the integer
    ones; the interpreter dispatches on it.
    """

    __slots__ = ('pc', 'size', 'name', 'args', 'kind')

    def __init__(self, pc, size, name, args, kind='cpu'):
        self.pc = pc
        self.size = size
        self.name = name
        self.args = args
        self.kind = kind

    def __repr__(self):
        return 'Insn(%08X %s %r)' % (self.pc, self.name, self.args)


class CodeImage:
    """The bytes of the code being run, addressed by virtual address."""

    def __init__(self):
        self.pages = {}

    def add(self, base, data):
        self.pages[base] = bytes(data)

    def byte(self, addr):
        for base, data in self.pages.items():
            if base <= addr < base + len(data):
                return data[addr - base]
        raise Stop('no code at %08X' % addr)

    def read(self, addr, n):
        return bytes(self.byte(addr + k) for k in range(n))


def _s8(v):
    return v - 256 if v > 127 else v


def _s32(v):
    return v - (1 << 32) if v > 0x7fffffff else v


def decode_modrm(image, pc):
    """Decode a ModRM byte (and its SIB/displacement).

    Returns (modrm, reg, rm, mem, size) where mem is None for the
    register form and a MemOp for the memory form; size is the number
    of bytes the whole ModRM group occupies.
    """
    modrm = image.byte(pc + 1)
    mod = modrm >> 6
    reg = (modrm >> 3) & 7
    rm = modrm & 7
    if mod == 3:
        return modrm, reg, rm, None, 2

    if rm == 4:                                   # a SIB byte follows
        sib = image.byte(pc + 2)
        size = 3
        scale = 1 << (sib >> 6)
        index = (sib >> 3) & 7
        base = sib & 7
        if index == 4:
            index = None                         # no index
        if mod == 0 and base == 5:
            disp = _s32(struct.unpack('<i', image.read(pc + 3, 4))[0])
            size += 4
            base = None
        else:
            base = REG32[base]
            disp = 0
            if mod == 1:
                disp = _s8(image.byte(pc + size))
                size += 1
            elif mod == 2:
                disp = _s32(struct.unpack('<i', image.read(pc + size, 4))[0])
                size += 4
        return modrm, reg, rm, MemOp(base, index, scale, disp), size

    if mod == 0 and rm == 5:                      # a plain displacement
        disp = _s32(struct.unpack('<i', image.read(pc + 2, 4))[0])
        return modrm, reg, rm, MemOp(None, None, 1, disp), 6

    base = REG32[rm]
    disp = 0
    size = 2
    if mod == 1:
        disp = _s8(image.byte(pc + size))
        size += 1
    elif mod == 2:
        disp = _s32(struct.unpack('<i', image.read(pc + size, 4))[0])
        size += 4
    return modrm, reg, rm, MemOp(base, None, 1, disp), size


def decode_x87(image, pc):
    """Decode one x87 escape instruction."""
    opcode = image.byte(pc)
    modrm = image.byte(pc + 1)
    mod = modrm >> 6
    reg = (modrm >> 3) & 7
    rm = modrm & 7

    if mod == 3:
        if opcode == 0xD9:
            special = _D9_SPECIAL.get(modrm)
            if special is not None:
                return Insn(pc, 2, special.name, {'op': special}, 'x87')
            table = _REG_TABLE[0xD9]
            if reg not in table or rm not in table[reg][1]:
                raise Stop('unhandled x87 %02X %02X' % (opcode, modrm))
            op = table[reg][0]
            return Insn(pc, 2, op.name, {'op': op, 'rm': rm}, 'x87')
        table = _REG_TABLE.get(opcode)
        if table is None or reg not in table or rm not in table[reg][1]:
            raise Stop('unhandled x87 %02X %02X' % (opcode, modrm))
        op = table[reg][0]
        return Insn(pc, 2, op.name, {'op': op, 'rm': rm}, 'x87')

    modrm, reg, rm, mem, size = decode_modrm(image, pc)
    table = _MEM_TABLE.get(opcode)
    if table is None or reg not in table:
        raise Stop('unhandled x87 %02X %02X' % (opcode, modrm))
    op, width = table[reg]
    return Insn(pc, size, op.name, {'op': op, 'mem': mem, 'width': width}, 'x87')


def decode(image, pc):
    """Decode the instruction at `pc'."""
    b = image.byte(pc)

    if 0xD8 <= b <= 0xDF:
        return decode_x87(image, pc)

    if b == 0x83:                                # group 1: r/m32, imm8
        modrm, reg, rm, mem, size = decode_modrm(image, pc)
        imm = _s8(image.byte(pc + size))
        size += 1
        if reg == 0:
            name = 'add'
        elif reg == 5:
            name = 'sub'
        else:
            raise Stop('unhandled cpu 83 /%d' % reg)
        return Insn(pc, size, name, {'reg': REG32[rm] if mem is None else None,
                                     'mem': mem, 'imm': imm})

    if b == 0x8B:                                # mov r32, r/m32
        modrm, reg, rm, mem, size = decode_modrm(image, pc)
        return Insn(pc, size, 'mov', {'dst': REG32[reg], 'src_reg': rm if mem is None else None,
                                      'mem': mem})

    if b == 0x89:                                # mov r/m32, r32
        modrm, reg, rm, mem, size = decode_modrm(image, pc)
        return Insn(pc, size, 'mov_store', {'dst_reg': reg, 'mem': mem, 'reg': rm})

    if b == 0x8D:                                # lea r32, m
        modrm, reg, rm, mem, size = decode_modrm(image, pc)
        return Insn(pc, size, 'lea', {'dst': REG32[reg], 'mem': mem})

    if 0x50 <= b <= 0x57:                        # push r32
        return Insn(pc, 1, 'push', {'reg': REG32[b - 0x50]})

    if 0x58 <= b <= 0x5F:                        # pop r32
        return Insn(pc, 1, 'pop', {'reg': REG32[b - 0x58]})

    if b == 0xF6:                                # group 3: test r/m8, imm8
        modrm, reg, rm, mem, size = decode_modrm(image, pc)
        if reg != 0:
            raise Stop('unhandled cpu F6 /%d' % reg)
        imm = image.byte(pc + size)
        size += 1
        if mem is not None or rm != 4:
            raise Stop('unhandled cpu F6 %02X' % modrm)
        return Insn(pc, size, 'test_ah', {'imm': imm})

    if b == 0x0F:                                # jcc rel32
        op = image.byte(pc + 1)
        if op not in _CONDITIONS:
            raise Stop('unhandled cpu 0F %02X' % op)
        rel = _s32(struct.unpack('<i', image.read(pc + 2, 4))[0])
        return Insn(pc, 6, 'jcc', {'cond': _CONDITIONS[op], 'target': pc + 6 + rel})

    if 0x70 <= b <= 0x7F:                        # jcc rel8
        rel = _s8(image.byte(pc + 1))
        return Insn(pc, 2, 'jcc', {'cond': _CONDITIONS[b], 'target': pc + 2 + rel})

    if b == 0xE9:                                # jmp rel32
        rel = _s32(struct.unpack('<i', image.read(pc + 1, 4))[0])
        return Insn(pc, 5, 'jmp', {'target': pc + 5 + rel})

    if b == 0xEB:                                # jmp rel8
        rel = _s8(image.byte(pc + 1))
        return Insn(pc, 2, 'jmp', {'target': pc + 2 + rel})

    if b == 0xC3:                                # ret
        return Insn(pc, 1, 'ret', {})

    if b == 0x90:                                # nop
        return Insn(pc, 1, 'nop', {})

    raise Stop('unhandled cpu %02X at %08X' % (b, pc))


# ---------------------------------------------------------------------------
# The encodings the decoder implements.  The instruction level test
# walks this list; every entry is measured against the hardware.
# ---------------------------------------------------------------------------
def implemented():
    """Every x87 encoding the decoder implements.

    Each entry is a dict:
        opcode  the escape byte
        reg     the ModRM reg field
        rm      the ModRM rm field (register forms only)
        form    'reg' or 'mem'
        name    the mnemonic
    """
    out = []
    for opcode in sorted(_REG_TABLE):
        for reg in sorted(_REG_TABLE[opcode]):
            op, rms = _REG_TABLE[opcode][reg]
            for rm in rms:
                out.append({'opcode': opcode, 'reg': reg, 'rm': rm,
                            'form': 'reg', 'name': op.name})
    for modrm, op in sorted(_D9_SPECIAL.items()):
        out.append({'opcode': 0xD9, 'reg': (modrm >> 3) & 7, 'rm': modrm & 7,
                    'form': 'reg', 'name': op.name, 'modrm': modrm})
    for opcode in sorted(_MEM_TABLE):
        for reg in sorted(_MEM_TABLE[opcode]):
            op, width = _MEM_TABLE[opcode][reg]
            out.append({'opcode': opcode, 'reg': reg, 'rm': 0,
                        'form': 'mem', 'name': op.name, 'width': width})
    return out


def encode_reg(opcode, reg, rm):
    """The bytes of the register form of an x87 instruction."""
    return bytes((opcode, 0xC0 | ((reg & 7) << 3) | (rm & 7)))


def encode_mem(opcode, reg, base='ebx', disp=0):
    """The bytes of a memory form, addressing [base + disp].

    `base' is one of the register names; disp is a byte displacement
    (or a 32-bit one when it does not fit).
    """
    rm = REG32.index(base)
    if -128 <= disp <= 127:
        return bytes((opcode, 0x40 | (reg << 3) | rm, disp & 0xff))
    return bytes((opcode, 0x80 | (reg << 3) | rm)) + struct.pack('<i', disp)
