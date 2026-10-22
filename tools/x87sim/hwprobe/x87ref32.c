/*
 * x87ref32.c - the hardware reference for one x87 encoding.
 *
 * The instruction under test is executed as raw machine code in a
 * PAGE_EXECUTE_READWRITE block, so the exact encoding is under our
 * control - which is the point: we measure the hardware's semantics
 * for each encoding rather than trusting a disassembler's spelling
 * of it.
 *
 * Six values are loaded, so ST0..ST5 are known and distinct; the
 * instruction runs; then the whole register file is dumped.  A form
 * that pops one shifts the window by one, a form that pops two by
 * two, so the dump reveals both the destination and the pop
 * behaviour.
 *
 * Usage:
 *   x87ref32 op   <op-hex> <reg> <rm> --st <6x float32 hex> [--mem <hex blob>]
 *   x87ref32 blob <hex bytes>                 --st <6x float32 hex> [--mem <hex blob>]
 *
 * [--cw <hex>] is the control word in force during the run; it is
 * loaded with fldcw after fninit and before the values are loaded.
 * The default is 0x127F (53-bit precision, round to nearest, affine
 * infinity); 0x007F is the 24-bit precision (_PC_24) the engine runs
 * under.
 *
 * <op-hex> is the escape byte (d8, d9, db, dc, dd, de, df) in hex;
 * <reg> and <rm> are decimal and select the ModRM fields of the
 * register form.  --st is six float32 bit patterns (8 hex digits
 * each) loaded in order, so the last one ends up in ST0.  --mem is a
 * blob of bytes; it is placed in a buffer whose address is printed
 * as MEMBASE, and EBX is set to that address so a blob can address
 * it with [ebx+disp].
 *
 * Output (all values are printed on their own "key value" lines):
 *   CW <hex>       control word in force during the run
 *   SW <hex>       status word after the run (TOP is bits 11-13)
 *   TOP <n>        the stack top
 *   ST<n> <hex>    register n as a double's bit pattern (ST0 = top)
 *   MEMBASE <hex>  address of the memory blob
 *   MEM <hex>      the memory blob after the run
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#define ALLOC_SZ  0x4000
#define CODE_OFF  0x0000
#define PAT_OFF   0x0800
#define OUT_OFF   0x1000
#define MEM_OFF   0x1800
#define SW_OFF    0x2000
#define CW_OFF    0x2100

#define MAX_MEM   0x400

static unsigned char *g_base;
static unsigned char *cur;

static void put1(unsigned char b) { *cur++ = b; }
static void put4(unsigned int v) { memcpy(cur, &v, 4); cur += 4; }

static unsigned int addr_of(int off) { return (unsigned int)(g_base + off); }

static void print_double_bits(const char *key, double d)
{
    unsigned __int64 bits;
    memcpy(&bits, &d, 8);
    printf("%s %016llX\n", key, (unsigned long long)bits);
}

int main(int argc, char **argv)
{
    unsigned int st_bits[6];
    int i, argi;
    unsigned char op = 0, modrm = 0;
    int mode_blob = 0;
    unsigned char mem[MAX_MEM];
    int mem_len = 0;
    unsigned char *code, *pat, *out, *membuf, *swbuf, *cwbuf;
    unsigned int cw_addr;
    int have_st = 0;
    /* the control word in force during the run; 0x127F is the
     * 53-bit default, 0x007F the 24-bit (_PC_24) one. */
    unsigned short cw = 0x127F;

    if (argc < 2) {
        printf("usage: x87ref32 op <op> <reg> <rm> --st <hex> [--mem <hex>] [--cw <hex>]\n");
        printf("       x87ref32 blob <hex bytes> --st <hex> [--mem <hex>] [--cw <hex>]\n");
        return 2;
    }
    if (strcmp(argv[1], "op") == 0) {
        if (argc < 5) { printf("op needs <op> <reg> <rm>\n"); return 2; }
        op = (unsigned char)strtoul(argv[2], 0, 16);
        modrm = (unsigned char)(0xC0 | ((atoi(argv[3]) & 7) << 3) | (atoi(argv[4]) & 7));
    } else if (strcmp(argv[1], "blob") == 0) {
        mode_blob = 1;
        if (argc < 3) { printf("blob needs hex bytes\n"); return 2; }
    } else {
        printf("unknown mode %s\n", argv[1]);
        return 2;
    }

    memset(st_bits, 0, sizeof st_bits);
    memset(mem, 0, sizeof mem);

    for (argi = (mode_blob ? 3 : 5); argi < argc; argi++) {
        if (strcmp(argv[argi], "--st") == 0 && argi + 1 < argc) {
            const char *hex = argv[++argi];
            size_t n = strlen(hex);
            if (n < 48) { printf("--st needs six 8-digit float32 patterns\n"); return 2; }
            for (i = 0; i < 6; i++) {
                char tmp[16];
                memcpy(tmp, hex + 8 * i, 8);
                tmp[8] = 0;
                st_bits[i] = (unsigned int)strtoul(tmp, 0, 16);
            }
            have_st = 1;
        } else if (strcmp(argv[argi], "--cw") == 0 && argi + 1 < argc) {
            const char *hex = argv[++argi];
            cw = (unsigned short)strtoul(hex, 0, 16);
        } else if (strcmp(argv[argi], "--mem") == 0 && argi + 1 < argc) {
            const char *hex = argv[++argi];
            size_t n = strlen(hex), k;
            if (n % 2) { printf("--mem hex must have even length\n"); return 2; }
            mem_len = (int)(n / 2);
            if (mem_len > MAX_MEM) { printf("--mem too long\n"); return 2; }
            for (k = 0; k < (size_t)mem_len; k++) {
                char tmp[3];
                tmp[0] = hex[2 * k]; tmp[1] = hex[2 * k + 1]; tmp[2] = 0;
                mem[k] = (unsigned char)strtoul(tmp, 0, 16);
            }
        }
    }
    if (!have_st) { printf("ERROR: --st is required\n"); return 2; }

    g_base = (unsigned char *)VirtualAlloc(NULL, ALLOC_SZ, MEM_COMMIT | MEM_RESERVE,
                                           PAGE_EXECUTE_READWRITE);
    if (!g_base) { printf("VirtualAlloc failed\n"); return 1; }
    code   = g_base + CODE_OFF;
    pat    = g_base + PAT_OFF;
    out    = g_base + OUT_OFF;
    membuf = g_base + MEM_OFF;
    swbuf  = g_base + SW_OFF;
    cwbuf  = g_base + CW_OFF;
    cw_addr = addr_of(CW_OFF);

    /* the six known values, as float32 */
    for (i = 0; i < 6; i++) {
        float f;
        unsigned int b = st_bits[i];
        memcpy(&f, &b, 4);
        memcpy(pat + 4 * i, &f, 4);
    }
    if (mem_len) memcpy(membuf, mem, mem_len);

    /* the control word in force during the run: the default 0x127F is
     * precision control 53 bits (10), round to nearest (00), affine
     * infinity (1) - the IEEE-like behaviour the simulator models;
     * 0x007F is the 24-bit (_PC_24) precision the engine runs under. */
    { memcpy(cwbuf, &cw, 2); }

    /* build the code */
    cur = code;
    put1(0x53);                                      /* push ebx */
    put1(0xDB); put1(0xE3);                          /* fninit */
    put1(0xD9); put1(0x2D); put4(cw_addr);           /* fldcw [cw] */
    put1(0xD9); put1(0x3D); put4(addr_of(SW_OFF));   /* fnstcw [cw] */
    {
        /* X87REF_LOADS overrides how many of the six values are
         * loaded; the default is all six. */
        int nload = 6;
        const char *e = getenv("X87REF_LOADS");
        if (e) nload = atoi(e);
        if (nload < 0) nload = 0;
        if (nload > 6) nload = 6;
        for (i = 0; i < nload; i++) {                 /* fld dword [pat+4i] */
            put1(0xD9); put1(0x05); put4(addr_of(PAT_OFF + 4 * i));
        }
    }
    put1(0xBB); put4(addr_of(MEM_OFF));               /* mov ebx, membuf */
    if (mode_blob) {
        const char *hex = argv[2];
        size_t k, n = strlen(hex);
        for (k = 0; k + 1 < n; k += 2) {
            char tmp[3];
            tmp[0] = hex[k]; tmp[1] = hex[k + 1]; tmp[2] = 0;
            put1((unsigned char)strtoul(tmp, 0, 16));
        }
    } else {
        put1(op); put1(modrm);
    }
    put1(0xDF); put1(0xE0);                          /* fnstsw ax */
    put1(0x66); put1(0x89); put1(0x05); put4(addr_of(SW_OFF + 2)); /* mov [sw], ax */
    for (i = 0; i < 8; i++) {                         /* fstp qword [out+8i] */
        put1(0xDD); put1(0x1D); put4(addr_of(OUT_OFF + 8 * i));
    }
    put1(0x5B);                                      /* pop ebx */
    put1(0xC3);                                      /* ret */

    ((void(__cdecl *)())code)();

    /* dump */
    {
        unsigned short fcw, fsw;
        memcpy(&fcw, swbuf, 2);
        memcpy(&fsw, swbuf + 2, 2);
        printf("CW %04X\n", fcw);
        printf("SW %04X\n", fsw);
        printf("TOP %d\n", (fsw >> 11) & 7);
        for (i = 0; i < 8; i++) {
            char key[16];
            double d = ((double *)out)[i];
            sprintf(key, "ST%d", i);
            print_double_bits(key, d);
        }
        printf("MEMBASE %08X\n", addr_of(MEM_OFF));
        printf("MEM ");
        for (i = 0; i < mem_len; i++) printf("%02X", membuf[i]);
        printf("\n");
    }
    return 0;
}
