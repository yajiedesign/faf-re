"""Build the x87 hardware reference (x87ref32.exe) for 32-bit Windows.

The reference has to run in 32-bit mode: the x87 semantics we measure
are the ones the 32-bit retail executes, and a 64-bit process has a
different FPU environment by default.

The repository rule about cmd quoting applies: a cmd command with
nested quotes does not survive being typed at an interactive cmd
prompt, so the vcvars call and the cl/link calls go into a temporary
batch file which is then executed.  This module is the entry point -
the batch file is a generated artifact, not a build script anybody
edits.

Usage:
    python tools/x87sim/hwprobe/build.py [--force]
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(HERE, 'build')
SOURCE = os.path.join(HERE, 'x87ref32.c')
EXE = os.path.join(BUILD_DIR, 'x87ref32.exe')
LOG = os.path.join(BUILD_DIR, 'build.log')
BAT = os.path.join(BUILD_DIR, 'build.bat')

# The VS 2022 environment.  On this machine Visual Studio lives on G:,
# not C: - the default path in the repository's own build script does
# not exist here.
VCVARS_CANDIDATES = [
    r'G:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_x86.bat',
    r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_x86.bat',
    r'G:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat',
    r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat',
]


def find_vcvars():
    for path in VCVARS_CANDIDATES:
        if os.path.isfile(path):
            return path
    return None


def _pe_machine(path):
    """Return the PE machine type of an executable (0x14c = i386)."""
    with open(path, 'rb') as fh:
        data = fh.read(0x400)
    if data[:2] != b'MZ':
        raise ValueError('%s is not a PE image' % path)
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('%s has no PE signature' % path)
    return struct.unpack_from('<H', data, pe + 4)[0]


def _write_bat(vcvars):
    os.makedirs(BUILD_DIR, exist_ok=True)
    lines = [
        '@echo off',
        'call "%s" >nul' % vcvars,
        'if errorlevel 1 exit /b 1',
        'cd /d "%s"' % BUILD_DIR,
        # No trailing backslash inside a quoted /Fo: path - the C runtime
        # would eat the closing quote.
        'cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS '
        '/Fo:.\\ /Fe:x87ref32.exe "%s"' % SOURCE,
        'exit /b %errorlevel%',
    ]
    with open(BAT, 'w') as fh:
        fh.write('\r\n'.join(lines) + '\r\n')
    return BAT


def _run(bat):
    proc = subprocess.run([bat], cwd=BUILD_DIR, capture_output=True, text=True)
    out = (proc.stdout or '') + (proc.stderr or '')
    with open(LOG, 'w') as fh:
        fh.write(out)
    return proc.returncode, out


# cl reports "file(line): error Cxxxx: ..." - the exit code alone is not
# enough (a stale exe must never be mistaken for a fresh build).
_ERROR_RE = re.compile(r': error [A-Z]+[0-9]+')


def build(force=False, verbose=False):
    """Build the reference if it is missing or older than the source.

    Returns the path of the executable.  Raises RuntimeError when the
    build fails - the caller is expected to fail loudly, never to
    skip the hardware comparison.
    """
    if not force and os.path.isfile(EXE) and os.path.getmtime(EXE) >= os.path.getmtime(SOURCE):
        return EXE

    vcvars = find_vcvars()
    if vcvars is None:
        raise RuntimeError(
            'vcvarsamd64_x86.bat not found; looked at:\n  ' +
            '\n  '.join(VCVARS_CANDIDATES))
    bat = _write_bat(vcvars)
    rc, out = _run(bat)
    if verbose:
        sys.stdout.write(out)
    if rc != 0 or _ERROR_RE.search(out):
        raise RuntimeError('build of x87ref32.exe failed (rc=%d); see %s\n%s'
                           % (rc, LOG, out))
    if not os.path.isfile(EXE):
        raise RuntimeError('build of x87ref32.exe produced no executable; see %s\n%s'
                           % (LOG, out))
    machine = _pe_machine(EXE)
    if machine != 0x014C:
        raise RuntimeError('x87ref32.exe is not a 32-bit image (machine=0x%X); '
                           'the x87 semantics must be measured in 32-bit mode' % machine)
    return EXE


def main(argv):
    force = '--force' in argv
    path = build(force=force, verbose=True)
    print('built %s' % path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
