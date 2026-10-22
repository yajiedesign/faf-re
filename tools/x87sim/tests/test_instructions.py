"""Instruction level tests: the simulator against the real CPU.

For every encoding the decoder implements, the encoding is synthesised,
run through the Python simulator and through the hardware reference
(x87ref32.exe, which executes the same bytes on a real 32-bit x87),
and the two register files are compared bit for bit.

The inputs are generated here - there is no corpus and no retail
binary involved.  The hardware reference is built on demand by
hwprobe/build.py; if that build fails the tests fail, they are never
skipped.

Two precision models are covered:

  * `53' - 53-bit registers, the reference runs with the control word
    0x127F.  This is the default and runs in this process.
  * `24' - every operation is rounded to a 24-bit significand, the
    reference runs with the control word 0x007F (_PC_24, the word the
    engine runs under).  The simulator reads SIMPREC from the
    environment *at import time*, so this model runs in a child process
    with SIMPREC=24 set before the package is imported; the parent
    drives that child and fails if it fails, so a disagreement in the
    24-bit model is just as loud as one in the 53-bit model.

Every encoding is checked with two sets of inputs:

  * `base' - the ordinary values: exact float32 numbers of a moderate
    size, so every operation is exact or one rounding away.
  * `range' - the ends of the range: a signed zero, a very small and a
    very large value.  The x87's register is the 80-bit extended
    format, so its exponent range does not depend on the precision
    control; the 24-bit model has to keep a value like 1e-30 and
    1e30, and a quotient's sign has to survive.

The model is selected by the environment variable X87SIM_PREC ('53' or
'24'); the tests are otherwise identical for both.
"""

import importlib.util
import os
import struct
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                     # .../tools/x87sim

# Which precision model this process tests.  The parent runs the 53-bit
# model and drives the 24-bit model in a child process; the child is
# told so by X87SIM_CHILD.
PREC = os.environ.get('X87SIM_PREC', '53')
if PREC not in ('24', '53'):
    raise SystemExit('X87SIM_PREC must be 24 or 53, not %r' % PREC)
IS_CHILD = os.environ.get('X87SIM_CHILD') == '1'

# The precision the simulator models, and the control word the hardware
# reference runs at.  0x127F is 53-bit precision / round to nearest /
# affine infinity; 0x007F is 24-bit precision (_PC_24) / round to
# nearest.
CONTROL_WORD = {'53': '127F', '24': '007F'}

# It has to be set before the package is imported.
os.environ['SIMPREC'] = PREC
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

# The memory the memory forms read: two float32 values, which the
# 64-bit forms read as one double (a value with 53 significant bits,
# so the load is not a 24-bit value).
MEM_VALUES = [2.5, -1.75]
MEM_BLOB = b''.join(struct.pack('<f', v) for v in MEM_VALUES)


def _st_hex(values):
    """The six float32 patterns the reference loads, in order."""
    return ''.join('%08X' % struct.unpack('<I', struct.pack('<f', v))[0]
                   for v in values)


# The sets of inputs every encoding is checked with.  Every value is
# exactly a float32, because that is what the reference loads (`fld
# dword'); the simulator sees the same value.
CORPORA = [
    ('base', [1.5, -2.25, 3.125, 0.5, 100.0, -7.75]),
    ('range', [0.0, -0.0, 2.0 ** -100, 2.0 ** 100, 3.0, 7.0]),
]

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


def run_reference(encoding, values, mem_blob=b''):
    """Execute `encoding' on the real CPU and return what it left.

    The reference runs with the control word of this process's
    precision model.
    """
    exe = BUILD.build()
    args = [exe, 'blob', encoding.hex(), '--st', _st_hex(values),
            '--cw', CONTROL_WORD[PREC]]
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


def run_simulator(encoding, values, membase, mem_blob=b''):
    """Execute `encoding' in the simulator and return the machine."""
    m = Machine()
    for v in values:
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


def compare(testcase, encoding, corpus, values, mem_blob=b''):
    """Run both sides and compare the register files."""
    ref = run_reference(encoding, values, mem_blob)
    sim = run_simulator(encoding, values, ref.membase, mem_blob)
    where = 'corpus %s: ' % corpus

    # the control word the hardware really ran at
    testcase.assertEqual(ref.cw, int(CONTROL_WORD[PREC], 16),
                         where + 'control word: hardware %04X, expected %s'
                         % (ref.cw, CONTROL_WORD[PREC]))

    # The stack rotation: TOP says where the top of the stack is, so
    # comparing it checks the pop and push behaviour of the encoding.
    testcase.assertEqual(sim.st.top, ref.top,
                         where + 'stack top: sim %d, hardware %d'
                         % (sim.st.top, ref.top))

    # the condition codes
    cc_mask = 0x0100 | 0x0400 | 0x4000
    if not reads_empty_register(encoding, sim.depth_before):
        testcase.assertEqual(sim.status_word() & cc_mask, ref.sw & cc_mask,
                             where + 'condition codes: sim %04X, hardware %04X'
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
            testcase.fail(where + 'ST(%d): sim %016X, hardware %016X'
                          % (i, sv, hw))

    # the memory the instruction may have written
    if mem_blob:
        want = ref.mem
        for k in range(len(mem_blob)):
            got = sim.mem.read_bytes(ref.membase + k, 1)
            if got != want[k:k + 1]:
                testcase.fail(where + 'memory +%d: sim %02X, hardware %02X'
                              % (k, got[0], want[k]))


def compare_all(testcase, encoding, mem_blob=b''):
    """Compare the encoding with every set of inputs."""
    for corpus, values in CORPORA:
        compare(testcase, encoding, corpus, values, mem_blob)


def setUpModule():
    # build the hardware reference; a failure here must be loud
    BUILD.build()


class InstructionTest(unittest.TestCase):
    """The base class; the tests themselves are generated below."""


def _register_form_test(spec):
    def test(self):
        encoding = encode_reg(spec['opcode'], spec['reg'], spec['rm'])
        compare_all(self, encoding)
    return test


def _memory_form_test(spec):
    def test(self):
        encoding = encode_mem(spec['opcode'], spec['reg'], 'ebx', 0)
        compare_all(self, encoding, MEM_BLOB)
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

    def test_the_precision_model_is_the_one_asked_for(self):
        # the simulator reads SIMPREC at import time; this process must
        # model the precision it claims to
        from x87sim import arithmetic
        self.assertEqual(arithmetic.PREC, int(PREC))
        self.assertEqual(os.environ['SIMPREC'], PREC)

    def test_the_inputs_are_float32_values(self):
        # the reference loads float32 patterns; the simulator has to see
        # the same values, or the two sides compute different things
        for corpus, values in CORPORA:
            for v in values:
                self.assertEqual(v, struct.unpack('<f', struct.pack('<f', v))[0],
                                 'corpus %s: %r is not a float32' % (corpus, v))


class OtherPrecisionModelTest(unittest.TestCase):
    """The other precision model, run in a child process.

    The simulator reads SIMPREC when the package is imported, so the
    24-bit model cannot be switched on inside a running process
    without patching the package's internals.  Instead the whole test
    module is run again in a child process with SIMPREC=24 and the
    hardware reference at the control word 0x007F; the child runs the
    same per-encoding tests, so a disagreement there fails this test
    and is reported in full.
    """

    def test_24_bit_model_matches_the_hardware(self):
        if IS_CHILD:
            self.skipTest('this process is the 24-bit child')
        env = dict(os.environ)
        env['X87SIM_PREC'] = '24'
        env['X87SIM_CHILD'] = '1'
        env['SIMPREC'] = '24'
        proc = subprocess.run(
            [sys.executable, '-m', 'unittest', '-v', 'test_instructions'],
            cwd=HERE, env=env, capture_output=True, text=True)
        if proc.returncode != 0:
            self.fail('the 24-bit model does not match the hardware at '
                      'control word 0x007F (child exit code %d)\n%s%s'
                      % (proc.returncode, proc.stdout, proc.stderr))
        # the child must really have compared the encodings
        self.assertIn('Ran %d tests' % (TEST_COUNT + 5), proc.stderr)


if __name__ == '__main__':
    unittest.main()
