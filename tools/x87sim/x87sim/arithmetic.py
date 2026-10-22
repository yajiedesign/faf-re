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

The 24-bit model rounds the *significand*, not the value: the x87's
register is the 80-bit extended format, so its exponent range is the
extended one (1e-4932 .. 1e4932) whatever the precision control says,
and only the significand narrows.  A value that is not a float32 is
still a value here.  The 53-bit model has the same exponent range, so
the two models differ in the significand only.
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
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except OverflowError:
        # a value outside the float32 range: the hardware's overflow
        # result is an infinity with the sign of the value
        return INF if x > 0 else -INF


def r24(x):
    """Round a result to the precision the control word selects.

    Under 53-bit precision the register keeps the value as it is; the
    spills are rounded separately by f32r.
    """
    if PREC == 53:
        return x
    return _r24_24(x)


def _r24_24(x):
    """Round to a 24-bit significand, keeping the exponent range.

    The x87's register is the 80-bit extended format: a 15-bit
    exponent and a significand whose width the control word selects.
    So _PC_24 rounds the *significand* to 24 bits - it does not turn
    the value into a float32, whose exponent range is four orders of
    magnitude narrower and whose denormals have lost their low bits.

    Rounding to a float32 here was wrong on both ends, and the
    hardware comparison shows it: 1e-50 came out as zero (a float32
    denormal cannot hold it) and 1e60 raised an OverflowError instead
    of being a value - the hardware keeps both, because its register
    has the extended exponent range (1e-4932 .. 1e4932) whatever the
    precision control says.

    The result is a double, so the model covers the double's exponent
    range; a result outside it comes out as an infinity here, where
    the hardware would still hold it.
    """
    if x != x or x in (INF, -INF) or x == 0.0:
        return x
    m, e = math.frexp(x)             # x = m * 2**e, |m| in [0.5, 1)
    scaled = m * (1 << 24)           # exact: |scaled| in [2**23, 2**24)
    r = round(scaled)                # nearest, ties to even
    try:
        return math.ldexp(r, e - 24)
    except OverflowError:
        return INF if x > 0 else -INF


def frac_to_p24(fr):
    """Round an exact rational to the nearest 24-bit significand
    (ties to even) - the rounding the x87 performs under _PC_24.

    The exponent is not limited to the float32 range: the register is
    the extended format, so a quotient of 1e-50 is a value here.
    """
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
    if a == 0.0:
        # the sign of a zero quotient follows the signs of the operands
        neg = (math.copysign(1.0, a) < 0) != (math.copysign(1.0, b) < 0)
        return -0.0 if neg else 0.0
    if not (math.isfinite(a) and math.isfinite(b)):
        return a / b
    if PREC == 53:
        return frac_to_f64(Fraction(a) / Fraction(b))
    return frac_to_p24(Fraction(a) / Fraction(b))
