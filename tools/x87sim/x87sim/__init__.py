"""x87sim - a simulator for the 32-bit x87 FPU.

The simulator models the x87 the way the 32-bit retail code sees it:

  * the eight-register file is physical, with a rotating top, so ST(i)
    is readable past the live depth;
  * every operation is rounded once, to the precision the control word
    selects (SIMPREC=24 or SIMPREC=53, see arithmetic.py);
  * the instructions are decoded from the raw opcode and ModRM bytes,
    because the D8/DC/DE families differ in destination and pop
    behaviour in ways a disassembler's text does not show.

Use it as a library:

    import sys
    sys.path.insert(0, r'../faf-re/tools/x87sim')
    from x87sim import run_function

    m = run_function(code, 0x00A7E660, mem=..., reg=...)
    print(m.result)
"""

from .arithmetic import PREC, bits, div, f32r, frombits, r24
from .cpu import Flags, X87
from .decode import Stop, decode, implemented
from .machine import Machine, run_function
from .memory import Memory

__all__ = [
    'PREC', 'bits', 'div', 'f32r', 'frombits', 'r24',
    'Flags', 'X87', 'Stop', 'decode', 'implemented',
    'Machine', 'run_function', 'Memory',
]
