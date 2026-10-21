"""Instruction level tests: the simulator against the real CPU.

For every encoding the decoder implements, the encoding is synthesised,
run through the Python simulator and through the hardware reference
(x87ref32.exe, which executes the same bytes on a real 32-bit x87),
and the two register files are compared bit for bit.

The inputs are generated here - there is no corpus and no retail
binary involved.  The hardware reference is built on demand by
hwprobe/build.py; if that build fails the tests fail, they are never
skipped.
"""

import importlib.util
import os
import struct
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                     # .../tools/x87sim

# The precision the reference runs at (53-bit registers, float32
# spills).  It has to be set before the package is imported.
os.environ['SIMPREC'] = '53'
sys.path.insert(0, ROOT)

from x87sim import Machine, Stop, implemented            # noqa: E402
from x87sim.decode import decode, encode_mem, encode_reg  # noqa: E402


def _load_build_module():
    path = os.path.join(ROOT, 'hwprobe', 'build.py')
    spec = importlib.util.spec_from_file_location('x87sim_hwprobe_build', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


BUILD = _load_build_module()

# Six known, distinct values, loaded in order, so the last one is in
# ST0.  All of them are exact float32 values.
VALUES = [1.5, -2.25, 3.125, 0.5, 100.0, -7.75]
ST_HEX = ''.join('%08X' % struct.unpack('<I', struct.pack('<f', v))[0] for v in VALUES)

# The two float32 values the memory forms read.
MEM_VALUES = [2.5, -1.75]
MEM_BLOB = b''.join(struct.pack('<f', v) for v in MEM_VALUES)

# The indefinite value the hardware leaves in a register that was never
# written; it is not a value the simulator models, so those registers
# are not compared.
INDEFINITE = 0xFFF8000000000000

CODE_BASE = 0x00001000


def _double_bits(v):
    return struct.unpack('<Q', struct.pack('<d', v))[0]


class Reference(object):
    """The result of one run of the hardware reference."""

    def __init__(self, fields):
        self.cw = fields['CW']
        self.sw = fields['SW']
        self.top = fields['TOP']
        self.st = [fields['ST%d' % i] for i in range(8)]
        self.membase = fields['MEMBASE']
        self.mem = fields['MEM']


def run_reference(encoding, mem_blob=b''):
    """Execute `encoding' on the real CPU and return what it left."""
    exe = BUILD.build()
    args = [exe, 'blob', encoding.hex(), '--st', ST_HEX]
    if mem_blob:
        args += ['--mem', mem_blob.hex()]
    proc = subprocess.run(args, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError('x87ref32 failed (%d): %s%s'
                           % (proc.returncode, proc.stdout, proc.stderr))
    fields = {}
    mem_hex = ''
    for line in proc.stdout.splitlines():
        key, _, value = line.partition(' ')
        if key == 'MEM':
            mem_hex = value.strip()
            continue
        if key.startswith('ST'):
            fields[key] = int(value, 16)
        elif key in ('CW', 'SW'):
            fields[key] = int(value, 16)
        elif key == 'TOP':
            fields[key] = int(value)
        elif key == 'MEMBASE':
            fields[key] = int(value, 16)
    fields['MEM'] = bytes.fromhex(mem_hex)
    return Reference(fields)


def run_simulator(encoding, membase, mem_blob=b''):
    """Execute `encoding' in the simulator and return the machine."""
    m = Machine()
    for v in VALUES:
        m.st.push(v)
    m.regs['ebx'] = membase
    if mem_blob:
        m.mem.write_bytes(membase, mem_blob)
    m.image.add(CODE_BASE, encoding + b'\xC3')     # the instruction, then a ret
    m.depth_before = m.st.depth
    m.run(CODE_BASE)
    return m


def reads_empty_register(encoding, depth):
    """Does the instruction use a register that is not live?

    On the hardware a register that was never written - or that was
    popped - holds the indefinite value, so an instruction that uses
    one raises the invalid operation exception and its result is the
    indefinite value.  The simulator keeps the stale contents of the
    physical file there instead (that is what the compiler relies on),
    so the condition codes of such an instruction cannot be compared.
    """
    m = Machine()
    m.image.add(CODE_BASE, encoding)
    insn = decode(m.image, CODE_BASE)
    op = insn.args.get('op')
    if op is None:
        return False
    rm = insn.args.get('rm')
    if op.src == 'st1':
        return depth < 2
    if op.src == 'sti' and rm is not None and rm >= depth:
        return True
    if op.name == 'fxch' and rm is not None and rm >= depth:
        return True
    if op.op in ('add', 'mul', 'sub', 'rsub', 'div', 'rdiv'):
        # the destination is a source as well
        if op.dest == 'sti' and rm is not None and rm >= depth:
            return True
    return False


def compare(testcase, encoding, mem_blob=b''):
    """Run both sides and compare the register files."""
    ref = run_reference(encoding, mem_blob)
    sim = run_simulator(encoding, ref.membase, mem_blob)

    # The stack rotation: TOP says where the top of the stack is, so
    # comparing it checks the pop and push behaviour of the encoding.
    testcase.assertEqual(sim.st.top, ref.top,
                         'stack top: sim %d, hardware %d' % (sim.st.top, ref.top))

    # the condition codes
    cc_mask = 0x0100 | 0x0400 | 0x4000
    if not reads_empty_register(encoding, sim.depth_before):
        testcase.assertEqual(sim.status_word() & cc_mask, ref.sw & cc_mask,
                             'condition codes: sim %04X, hardware %04X'
                             % (sim.status_word(), ref.sw))

    # the register file
    for i in range(8):
        hw = ref.st[i]
        if hw == INDEFINITE:
            # a register the hardware never wrote (or that was popped);
            # the simulator keeps its own value there
            continue
        sv = _double_bits(sim.st.s(i))
        if hw != sv:
            testcase.fail('ST(%d): sim %016X, hardware %016X' % (i, sv, hw))

    # the memory the instruction may have written
    if mem_blob:
        want = ref.mem
        for k in range(len(mem_blob)):
            got = sim.mem.read_bytes(ref.membase + k, 1)
            if got != want[k:k + 1]:
                testcase.fail('memory +%d: sim %02X, hardware %02X'
                              % (k, got[0], want[k]))


def setUpModule():
    # build the hardware reference; a failure here must be loud
    BUILD.build()


class InstructionTest(unittest.TestCase):
    """The base class; the tests themselves are generated below."""


def _register_form_test(spec):
    def test(self):
        encoding = encode_reg(spec['opcode'], spec['reg'], spec['rm'])
        compare(self, encoding)
    return test


def _memory_form_test(spec):
    def test(self):
        encoding = encode_mem(spec['opcode'], spec['reg'], 'ebx', 0)
        compare(self, encoding, MEM_BLOB)
    return test


def _generate():
    """One test method per encoding the decoder implements."""
    count = 0
    for spec in implemented():
        name = 'test_%s_%02X_rrr%d_rm%d_%s' % (spec['form'], spec['opcode'],
                                              spec['reg'], spec['rm'], spec['name'])
        name = name.replace('$', '_')
        if spec['form'] == 'reg':
            method = _register_form_test(spec)
        else:
            method = _memory_form_test(spec)
        method.__name__ = name
        setattr(InstructionTest, name, method)
        count += 1
    return count


TEST_COUNT = _generate()


class CoverageTest(unittest.TestCase):
    """The tests must cover every encoding the decoder implements."""

    def test_every_encoding_has_a_test(self):
        specs = implemented()
        names = set((s['opcode'], s['reg'], s['rm'], s['form']) for s in specs)
        self.assertEqual(len(names), len(specs))
        self.assertEqual(TEST_COUNT, len(specs))

    def test_the_decoder_rejects_what_it_does_not_implement(self):
        # D9 E2 is not one of the special functions the decoder knows
        m = Machine()
        m.image.add(CODE_BASE, bytes((0xD9, 0xE2)))
        with self.assertRaises(Stop):
            m.run(CODE_BASE)


if __name__ == '__main__':
    unittest.main()
