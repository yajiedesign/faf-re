"""Byte-addressed memory, and the ESP-relative spill slots.

The simulator's memory is a flat byte array: every store rounds the
way the hardware rounds it - a 32-bit slot holds a float32, a 64-bit
slot holds a double.  The retail's spill slots are just ordinary
stores relative to ESP, so they need no special treatment beyond
being reachable at `esp + offset`.
"""

import struct

from .arithmetic import f32r


class Memory:
    """A flat, byte-addressed memory."""

    __slots__ = ('_b',)

    def __init__(self):
        self._b = {}

    # ---- raw bytes -----------------------------------------------------
    def write_bytes(self, addr, data):
        for k, b in enumerate(data):
            self._b[addr + k] = b

    def read_bytes(self, addr, n):
        return bytes(self._b.get(addr + k, 0) for k in range(n))

    def clear(self, addr, n):
        for k in range(n):
            self._b.pop(addr + k, None)

    # ---- 32-bit float slots --------------------------------------------
    def write_f32(self, addr, value):
        """Store a float32 (rounded once, as the hardware does)."""
        self.write_bytes(addr, struct.pack('<f', f32r(value)))

    def read_f32(self, addr):
        """Read a float32; unmapped memory reads as 0.0."""
        return struct.unpack('<f', self.read_bytes(addr, 4))[0]

    # ---- 64-bit float slots --------------------------------------------
    def write_f64(self, addr, value):
        self.write_bytes(addr, struct.pack('<d', value))

    def read_f64(self, addr):
        return struct.unpack('<d', self.read_bytes(addr, 8))[0]

    # ---- convenience ---------------------------------------------------
    def load_f32(self, mapping):
        """Fill 32-bit slots from {address: value}."""
        for addr, value in mapping.items():
            self.write_f32(addr, value)

    def load_f64(self, mapping):
        """Fill 64-bit slots from {address: value}."""
        for addr, value in mapping.items():
            self.write_f64(addr, value)

    def dump_f32(self, addr, n=1):
        return [self.read_f32(addr + 4 * k) for k in range(n)]

    # ---- the ESP-relative spill slots ---------------------------------
    @staticmethod
    def spill_addr(esp, offset):
        """The address of the spill slot `offset` bytes above ESP."""
        return esp + offset

    def spill_f32(self, esp, offset, value):
        """Store a float32 into the spill slot at ESP+offset."""
        self.write_f32(self.spill_addr(esp, offset), value)

    def spill_f64(self, esp, offset, value):
        self.write_f64(self.spill_addr(esp, offset), value)
