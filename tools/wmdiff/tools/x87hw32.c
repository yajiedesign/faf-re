/* x87 register-form ground truth, one (opcode, reg, rm) per invocation.
 *
 * The instruction under test is executed as raw machine code in a
 * PAGE_EXECUTE_READWRITE block, so the exact encoding is under our control -
 * which is the point: we measure the hardware's semantics for each encoding
 * rather than trusting a disassembler's spelling of it.
 *
 * Six values are loaded, so ST0..ST5 are known and distinct; the instruction
 * runs; then the top four are dumped. A form that pops one shifts the
 * window by one, a form that pops two by two, so the dump reveals both the
 * destination and the pop behaviour.
 *
 * Usage: x87probe32 <op> <reg> <rm>
 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define CODE_OFF 0x000
#define PAT_OFF  0x200
#define OUT_OFF  0x300
#define SIZE     0x1000

static unsigned char* code;
static double* pat;
static double* out;

static void run_one(unsigned char op, unsigned char modrm)
{
    unsigned char* c = code + CODE_OFF;
    int i;
    for (i = 0; i < 5; ++i) out[i] = -999.0;
    c[0] = 0xDB; c[1] = 0xE3; c += 2;      /* fninit, inside the code */
    for (i = 0; i < 6; ++i) {
        c[0] = 0xDD; c[1] = 0x05;
        *(unsigned int*)(c + 2) = (unsigned int)((unsigned char*)pat + 8 * i);
        c += 6;
    }
    c[0] = op; c[1] = modrm; c += 2;
    for (i = 0; i < 4; ++i) {
        c[0] = 0xDD; c[1] = 0x1D;
        *(unsigned int*)(c + 2) = (unsigned int)((unsigned char*)out + 8 * i);
        c += 6;
    }
    c[0] = 0xC3;
    ((void(__cdecl*)())code)();
}

int main(int argc, char** argv)
{
    int oi, reg, rm;
    unsigned char op;

    if (argc < 4) { printf("usage: x87probe32 <op> <reg> <rm>\n"); return 2; }
    op = (unsigned char)strtoul(argv[1], 0, 16);
    reg = atoi(argv[2]);
    rm = atoi(argv[3]);

    code = (unsigned char*)VirtualAlloc(NULL, SIZE, MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
    if (!code) { printf("VirtualAlloc failed\n"); return 1; }
    pat = (double*)(code + PAT_OFF);
    out = (double*)(code + OUT_OFF);
    for (oi = 0; oi < 6; ++oi) pat[oi] = 100.0 * (oi + 1);

    run_one(op, (unsigned char)(0xC0 | (reg << 3) | rm));
    printf("%02X %d %d | %11.4f %11.4f %11.4f %11.4f %11.4f\n", op, reg, rm,
           out[0], out[1], out[2], out[3], out[4]);
    return 0;
}
