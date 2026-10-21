"""The x87 register file and the integer flags the retail branches on.

The eight-register file is *physical* with a rotating top: ST(0) is
R[top], ST(i) is R[(top + i) & 7].  A push decrements the top and
writes the register; a pop reads it and increments the top.  Reading
ST(i) for i beyond the live depth returns the stale contents of that
physical register, which is what the hardware does - and the compiler
does rely on it, e.g. `fmul st(4)` with a depth of 4.
"""


class X87:
    """The physical eight-register x87 file with a rotating top."""

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
        """ST(i)."""
        return self.R[(self.top + i) & 7]

    def set(self, i, v):
        """ST(i) := v."""
        self.R[(self.top + i) & 7] = v

    def snapshot(self):
        """ST(0)..ST(7) in logical order."""
        return [self.s(i) for i in range(8)]

    def physical(self):
        """The eight physical registers, in physical order."""
        return list(self.R)


class Flags:
    """The integer flags the retail tests the FPU condition codes with.

    `fnstsw ax` leaves the condition codes in AH; the retail then does
    `test ah, imm` and branches on the *integer* flags that TEST sets:
    ZF from the AND result, PF from its parity.
    """

    __slots__ = ('zf', 'pf')

    def __init__(self):
        self.zf = False
        self.pf = False

    def test_ah(self, ah, imm):
        r = ah & imm
        self.zf = (r == 0)
        self.pf = (bin(r & 0xff).count('1') % 2) == 0
