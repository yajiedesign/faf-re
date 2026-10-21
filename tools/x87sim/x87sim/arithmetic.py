"""Exact arithmetic and the rounding rules the x87 uses.

The x87 rounds the *exact* result of every operation once, to the
precision the control word selects.  Doing that through a double
would round twice (double, then float32) and can land one ulp away,
so the quotients are computed as exact rationals and rounded once.

Two precision models are supported, selected by the SIMPREC
environment variable:

  SIMPREC=24   every operation is rounded to a 24-bit significand
               (what the x87 does under the _PC_24 control word)
  SIMPREC=53   the registers keep 53-bit significands, and only the
               spills to memory are rounded to float32 (the default
               is 24, for compatibility with the original simulator)
"""

import math
import os
import struct
from fractions import Fraction

# 24 = _PC_24 (round every result to float32), 53 = 53-bit registers.
PREC = int(os.environ.get('SIMPREC', '24'))

NAN = float('nan')
INF = float('inf')


def bits(f):
    """The float32 bit pattern of a value."""
    return struct.unpack('<I', struct.pack('<f', f))[0]


def frombits(b):
    """The value of a float32 bit pattern."""
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def f32r(x):
    """Round to float32 (a spill to a 32-bit slot)."""
    if x != x or x in (INF, -INF) or x == 0.0:
        return x
    return struct.unpack('<f', struct.pack('<f', x))[0]


def r24(x):
    """Round a result to the precision the control word selects.

    Under 53-bit precision the register keeps the value as it is; the
    spills are rounded separately by f32r.
    """
    if PREC == 53:
        return x
    return _r24_24(x)


def _r24_24(x):
    """Round to a 24-bit significand, returned as float32.

    The x87 rounds the *exact* result of each operation to the control
    word's precision.  The callers hand us values that a double holds
    exactly (the product of two 24-bit significands needs 48 bits), so a
    single conversion to float32 is the one rounding the hardware does -
    rounding to 25 bits first and then to 24 would be a second rounding
    and can land one ulp away.
    """
    if x != x or x in (INF, -INF) or x == 0.0:
        return x
    return struct.unpack('<f', struct.pack('<f', x))[0]


def frac_to_f32(fr):
    """Round an exact rational to the nearest 24-bit significand
    (float32), ties to even - the rounding the x87 performs under
    _PC_24."""
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
        val = INF
    return -val if neg else val


def frac_to_f64(fr):
    """Round an exact rational to the nearest double (53 bits), ties to
    even.  Fraction.__float__ does exactly that."""
    return float(fr)


def div(a, b):
    """The x87 divides the exact quotient and rounds it once, to the
    control word's precision, so the quotient must not be rounded twice
    (once to double, once to float32)."""
    if b == 0.0:
        if a == 0.0 or a != a:
            return NAN
        neg = (a < 0) != (math.copysign(1.0, b) < 0)
        return -INF if neg else INF
    if not (math.isfinite(a) and math.isfinite(b)):
        return a / b
    if PREC == 53:
        return frac_to_f64(Fraction(a) / Fraction(b))
    return frac_to_f32(Fraction(a) / Fraction(b))
