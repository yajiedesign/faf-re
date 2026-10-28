import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sim as S

def fb(h):
    return S.frombits(int(h, 16))

def load(path):
    rows = []
    for line in open(path, encoding='utf-8', errors='ignore'):
        i = line.find('SIMD ')
        if i < 0: continue
        t = line[i+5:].replace('|', ' ').split()
        t = [x for x in t if x != 'retail']
        if len(t) < 15: continue
        s0 = [fb(x) for x in t[0:7]]
        s1 = [fb(x) for x in t[7:14]]
        rows.append((s0, s1, int(t[14], 16)))
    return rows

# The retail's DistSegment3Segment3 object: two Segment3 references at
# this+0x34 and this+0x38, and a vtable. Each Segment3 is 0x1C bytes:
# origin[3] at +0x00, direction[3] at +0x0C, extent at +0x18.
THIS = 0x1000
SEG0 = 0x2000
SEG1 = 0x2100

def build(s0, s1):
    mem = {
        THIS + 0x34: float(SEG0),
        THIS + 0x38: float(SEG1),
    }
    # Segment3 layout in the retail: origin[3] at +0x00, direction[3] at
    # +0x0C, extent at +0x18 (the disassembly reads [seg+0x0C], [seg+0x10],
    # [seg+0x14] for the direction and [seg+0x18] for the extent).
    def lay(base, a):
        for k in range(3): mem[base + 0x00 + 4 * k] = a[k]
        for k in range(3): mem[base + 0x0C + 4 * k] = a[3 + k]
        mem[base + 0x18] = a[6]
    lay(SEG0, s0)
    lay(SEG1, s1)
    reg = {'ecx': THIS}
    return S.Sim(mem, reg)

if __name__ == '__main__':
    rows = load(sys.argv[1])
    ok = bad = err = 0
    shown = 0
    for s0, s1, retail in rows:
        s = build(s0, s1)
        try:
            s.run()
        except Exception as e:
            err += 1
            if shown < 3:
                print('STOP:', e)
                shown += 1
            continue
        got = s.result
        if got is None:
            bad += 1
            if shown < 6:
                print('no result; esp=%x' % s.reg['esp'])
                shown += 1
            continue
        if S.bits(got) == retail:
            ok += 1
        else:
            bad += 1
            if bad <= 5:
                print('MISMATCH retail %08X sim %08X' % (retail, S.bits(got)))
    print('sim == retail: %d / %d   (mismatch %d, error %d)' % (ok, len(rows), bad, err))
