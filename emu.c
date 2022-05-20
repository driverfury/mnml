#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

typedef uint8_t  u8;
typedef uint16_t u16;

#define ZERO_BIT 1
#define CARRY_BIT 2
#define NEG_BIT 4

typedef u8 (*MemoryReadFn)(u16);
typedef void (*MemoryWriteFn)(u8, u16);

typedef struct
Emu
{
    /*
     * Flags register
     * -----NCZ
     */
    u8 flags;
    u8 a;
    u16 pc;
    MemoryReadFn mem_read;
    MemoryWriteFn mem_write;
} Emu;

void
emu_set_flags(Emu *emu, u16 ans)
{
    u8 f = emu->flags;

    if((ans & 0xff) == 0) { f |= ZERO_BIT; } else { f &= ~ZERO_BIT; }
    if(ans > 0xff)        { f |= CARRY_BIT; } else { f &= ~CARRY_BIT; }
    if(ans & 0x80)        { f |= NEG_BIT; } else { f &= ~NEG_BIT; }

    emu->flags = f;
}

void
emu_set_flags16(Emu *emu, u16 ans)
{
    u8 f = emu->flags;

    if(ans & 0x8000)
    {
        f |= ZERO_BIT;
        f |= CARRY_BIT;
        f |= NEG_BIT;
    }
    else
    {
        f &= ~ZERO_BIT;
        f &= ~CARRY_BIT;
        f &= ~NEG_BIT;
    }

    emu->flags = f;
}

u8
emu_fetch(Emu *emu)
{
    u8 b;

    b = emu->mem_read(emu->pc);
    ++emu->pc;

    return(b);
}

u16
emu_fetch16(Emu *emu)
{
    u8 lo, hi;
    u16 w;

    lo = emu_fetch(emu);
    hi = emu_fetch(emu);
    w = (lo | (hi << 8));

    return(w);
}

u16
emu_mem_read16(Emu *emu, u16 addr)
{
    u8 lo, hi;
    u16 w;

    lo = emu->mem_read(addr);
    hi = emu->mem_read(addr + 1);
    w = (lo | (hi << 8));

    return(w);
}

void
emu_mem_write16(Emu *emu, u16 w, u16 addr)
{
    u8 lo, hi;

    lo = ((w & 0x00ff) >> 0);
    hi = ((w & 0xff00) >> 8);
    emu->mem_write(lo, addr);
    emu->mem_write(hi, addr + 1);
}

void
emu_push(Emu *emu, u8 b)
{
    u8 lsb;
    u16 sp;

    lsb = emu->mem_read(0xffff);
    sp = ((u16)((u16)0xff00 + (u16)lsb));
    emu->mem_write(b, sp);
    lsb -= 1;
    emu->mem_write(lsb, 0xffff);
}

u8
emu_pop(Emu *emu)
{
    u8 lsb;
    u16 sp;

    lsb = emu->mem_read(0xffff);
    lsb += 1;
    emu->mem_write(lsb, 0xffff);
    sp = ((u16)((u16)0xff00 + (u16)lsb));
    return(emu->mem_read(sp));
}

int
emu_exec(Emu *emu)
{
#define FETCH() emu_fetch(emu)
#define FETCH16() emu_fetch16(emu)
#define SET_FLAGS(ans) emu_set_flags(emu, (ans))
#define SET_FLAGS16(ans) emu_set_flags16(emu, (ans))
#define ANS_TO_U8(ans) ((u8)((ans)&0xff))
#define ZERO_BIT_IS_SET() (emu->flags & ZERO_BIT)
#define CARRY_BIT_IS_SET() (emu->flags & CARRY_BIT)
#define NEG_BIT_IS_SET() (emu->flags & NEG_BIT)
#define GET_SP() (emu->mem_read(0xffff))
#define GET_SP16() ((u16)((u16)0xff00 + (u16)(GET_SP())))

    int halt;
    u8 opc;
    u16 ans;
    u16 addr;
    u8 op, op2;

    halt = 0;
    opc = FETCH();

    switch(opc)
    {
        /* NOP */
        case 0x00:
        {
            /* Nothing */
        } break;

        /* BNK */
        case 0x01:
        {
            /* TODO: Set FLASH bank (unused for now) */
        } break;

        /* OUT */
        case 0x02:
        {
            /* Output A to the terminal */
            putchar((int)emu->a);
#if 1
            /* I put this here so we can see the output immediately */
            fflush(stdout);
#endif

            /* Flags status after => NCZ: 100 */
            emu->flags |=  NEG_BIT;
            emu->flags &= ~CARRY_BIT;
            emu->flags &= ~ZERO_BIT;
        } break;

        /* CLC */
        case 0x03:
        {
            /* Flags status after => NCZ: 100 */
            emu->flags |=  NEG_BIT;
            emu->flags &= ~CARRY_BIT;
            emu->flags &= ~ZERO_BIT;
        } break;

        /* SEC */
        case 0x04:
        {
            /* Flags status after => NCZ: 011 */
            emu->flags &= ~NEG_BIT;
            emu->flags |=  CARRY_BIT;
            emu->flags |=  ZERO_BIT;
        } break;

        /* LSL */
        /* ASL */
        case 0x05:
        {
            ans = (u16)emu->a << 1;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* ROL */
        case 0x06:
        {
            ans = (u16)emu->a << 1;
            if(emu->a & 0x80)
            {
                ans |= 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* LSR */
        case 0x07:
        {
            ans = (u16)emu->a >> 1;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* ROR */
        case 0x08:
        {
            ans = (u16)emu->a >> 1;
            if(emu->a & 1)
            {
                ans |= 0x80;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* INP */
        case 0x0a:
        {
            /* TODO: Terminal input and CPI 0xff */
            op = 0; /* TODO: Only temporary */

            emu->a = op;
            ans = (u16)emu->a - 0xff;
            SET_FLAGS(ans);
        } break;

        /* NEG */
        case 0x0b:
        {
            ans = ~((u16)emu->a);
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* INC */
        case 0x0c:
        {
            ans = (u16)emu->a + 1;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* DEC */
        case 0x0d:
        {
            ans = (u16)emu->a - 1;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* LDI */
        case 0x0e:
        {
            emu->a = FETCH();
        } break;

        /* ADI */
        case 0x0f:
        {
            op = FETCH();
            ans = (u16)emu->a + (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SBI */
        case 0x10:
        {
            op = FETCH();
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* CPI */
        case 0x11:
        {
            /* TODO: Check if this is correct */
            op = FETCH();
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
        } break;

        /* ACI */
        case 0x12:
        {
            op = FETCH();
            ans = (u16)emu->a + (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans += 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SCI */
        case 0x13:
        {
            op = FETCH();
            ans = (u16)emu->a - (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans -= 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* JPA */
        case 0x14:
        {
            addr = FETCH16();
            emu->pc = addr;
        } break;

        /* LDA */
        case 0x15:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            emu->a = op;
        } break;

        /* STA */
        case 0x16:
        {
            addr = FETCH16();
            emu->mem_write(emu->a, addr);
        } break;

        /* ADA */
        case 0x17:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            ans = (u16)emu->a + (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SBA */
        case 0x18:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* CPA */
        case 0x19:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
        } break;

        /* ACA */
        case 0x1a:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            ans = (u16)emu->a + (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans += 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SCA */
        case 0x1b:
        {
            addr = FETCH16();
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans -= 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* JPR */
        case 0x1c:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            emu->pc = addr;
        } break;

        /* LDR */
        case 0x1d:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            emu->a = op;
        } break;

        /* STR */
        case 0x1e:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            emu->mem_write(emu->a, addr);
        } break;

        /* ADR */
        case 0x1f:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            ans = (u16)emu->a + (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SBR */
        case 0x20:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* CPR */
        case 0x21:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            SET_FLAGS(ans);
        } break;

        /* ACR */
        case 0x22:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            ans = (u16)emu->a + (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans += 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* SCR */
        case 0x23:
        {
            addr = FETCH16();
            addr = emu_mem_read16(emu, addr);
            op = emu->mem_read(addr);
            ans = (u16)emu->a - (u16)op;
            if(CARRY_BIT_IS_SET())
            {
                ans -= 1;
            }
            SET_FLAGS(ans);
            emu->a = ANS_TO_U8(ans);
        } break;

        /* CLB */
        case 0x24:
        {
            addr = FETCH16();
            emu->mem_write(0, addr);
            emu->flags &= ~ZERO_BIT;
            emu->flags |=  CARRY_BIT;
            emu->flags &= ~NEG_BIT;
        } break;

        /* NEB */
        case 0x25:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ~ans;
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* INB */
        case 0x26:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans + 1;
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* DEB */
        case 0x27:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans - 1;
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* ADB */
        case 0x28:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans + emu->a;
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* SBB */
        case 0x29:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans - emu->a;
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* ACB */
        case 0x2a:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans + emu->a;
            if(CARRY_BIT_IS_SET())
            {
                ans += 1;
            }
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* SCB */
        case 0x2b:
        {
            addr = FETCH16();
            ans = (u16)emu->mem_read(addr);
            ans = ans - emu->a;
            if(CARRY_BIT_IS_SET())
            {
                ans -= 1;
            }
            SET_FLAGS(ans);
            emu->mem_write(ANS_TO_U8(ans), addr);
        } break;

        /* CLW */
        case 0x2c:
        {
            addr = FETCH16();
            emu->mem_write(0, addr);
            emu->mem_write(0, addr + 1);
            emu->flags &= ~ZERO_BIT;
            emu->flags |=  CARRY_BIT;
            emu->flags &= ~NEG_BIT;
        } break;

        /* NEW */
        case 0x2d:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ~ans;
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* INW */
        case 0x2e:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans + 1;
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* DEW */
        case 0x2f:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans - 1;
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* ADW */
        case 0x30:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans + emu->a;
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* SBW */
        case 0x31:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans - emu->a;
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* ACW */
        case 0x32:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans + emu->a;
            if(CARRY_BIT_IS_SET())
            {
                ans += 1;
            }
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* SCW */
        case 0x33:
        {
            addr = FETCH16();
            ans = emu_mem_read16(emu, addr);
            ans = ans - emu->a;
            if(CARRY_BIT_IS_SET())
            {
                ans -= 1;
            }
            SET_FLAGS16(ans);
            emu_mem_write16(emu, ans, addr);
        } break;

        /* LDS */
        case 0x34:
        {
            op = FETCH();
            addr = GET_SP16();
            emu->a = emu->mem_read(addr + (u16)op);
        } break;

        /* STS */
        case 0x35:
        {
            op = FETCH();
            addr = GET_SP16();
            emu->mem_write(emu->a, addr + (u16)op);
        } break;

        /* PHS */
        case 0x36:
        {
            emu_push(emu, emu->a);
        } break;

        /* PLS */
        case 0x37:
        {
            emu->a = emu_pop(emu);
        } break;

        /* JPS */
        case 0x38:
        {
            addr = FETCH16();
            emu_push(emu, (u8)((emu->pc & 0x00ff) >> 0));
            emu_push(emu, (u8)((emu->pc & 0xff00) >> 8));
            emu->pc = addr;
        } break;

        /* RTS */
        case 0x39:
        {
            op = emu_pop(emu);
            op2 = emu_pop(emu);
            addr = (u16)op2 + (((u16)op) << 8);
            emu->pc = addr;
        } break;

        /* BNE */
        case 0x3a:
        {
            addr = FETCH16();
            if(ZERO_BIT_IS_SET() == 0)
            {
                emu->pc = addr;
            }
        } break;

        /* BEQ */
        case 0x3b:
        {
            addr = FETCH16();
            if(ZERO_BIT_IS_SET())
            {
                emu->pc = addr;
            }
        } break;

        /* BCC */
        case 0x3c:
        {
            addr = FETCH16();
            if(CARRY_BIT_IS_SET() == 0)
            {
                emu->pc = addr;
            }
        } break;

        /* BCS */
        case 0x3d:
        {
            addr = FETCH16();
            if(CARRY_BIT_IS_SET())
            {
                emu->pc = addr;
            }
        } break;


        /* BPL */
        case 0x3e:
        {
            addr = FETCH16();
            if(NEG_BIT_IS_SET() == 0)
            {
                emu->pc = addr;
            }
        } break;

        /* BMI */
        case 0x3f:
        {
            addr = FETCH16();
            if(NEG_BIT_IS_SET())
            {
                emu->pc = addr;
            }
        } break;

        default:
        {
            halt = 1;
            assert(0);
        } break;
    }

#undef NEG_BIT_IS_SET
#undef CARRY_BIT_IS_SET
#undef ZERO_BIT_IS_SET
#undef ANS_TO_U8
#undef SET_FLAGS16
#undef SET_FLAGS
#undef FETCH16
#undef FETCH

    return(halt);
}

Emu emu;
u8 emu_mem[0x10000];

u8
emu_mem_read(u16 addr)
{
    u8 b;
    b = emu_mem[addr];
    return(b);
}

void
emu_mem_write(u8 b, u16 addr)
{
    emu_mem[addr] = b;
}

void
emu_disasm(Emu *emu)
{
    u16 pc;
    u8 opc;

    pc = emu->pc;
    opc = emu_fetch(emu);

    /* TODO: HERE */
    switch(opc)
    {
        /* NOP */
        case 0x00: { printf("NOP"); } break;

        /* BNK */
        case 0x01: { printf("BNK"); } break;

        /* OUT */
        case 0x02: { printf("OUT"); } break;

        /* CLC */
        case 0x03: { printf("CLC"); } break;

        /* SEC */
        case 0x04: { printf("SEC"); } break;

        /* LSL */
        case 0x05: { printf("LSL"); } break;

        /* ROL */
        case 0x06: { printf("ROL"); } break;

        /* LSR */
        case 0x07: { printf("LSR"); } break;

        /* ROR */
        case 0x08: { printf("ROR"); } break;

        /* INP */
        case 0x0a: { printf("INP"); } break;

        /* NEG */
        case 0x0b: { printf("NEG"); } break;

        /* INC */
        case 0x0c: { printf("INC"); } break;

        /* DEC */
        case 0x0d: { printf("DEC"); } break;

        /* LDI */
        case 0x0e: { printf("LDI %02x", emu_fetch(emu)); } break;

        /* ADI */
        case 0x0f: { printf("ADI %02x", emu_fetch(emu)); } break;

        /* SBI */
        case 0x10: { printf("SBI %02x", emu_fetch(emu)); } break;

        /* CPI */
        case 0x11: { printf("CPI %02x", emu_fetch(emu)); } break;

        /* ACI */
        case 0x12: { printf("ACI %02x", emu_fetch(emu)); } break;

        /* SCI */
        case 0x13: { printf("SCI %02x", emu_fetch(emu)); } break;

        /* JPA */
        case 0x14: { printf("JPA %04x", emu_fetch16(emu)); } break;

        /* LDA */
        case 0x15: { printf("LDA %04x", emu_fetch16(emu)); } break;

        /* STA */
        case 0x16: { printf("STA %04x", emu_fetch16(emu)); } break;

        /* ADA */
        case 0x17: { printf("ADA %04x", emu_fetch16(emu)); } break;

        /* SBA */
        case 0x18: { printf("SBA %04x", emu_fetch16(emu)); } break;

        /* CPA */
        case 0x19: { printf("CPA %04x", emu_fetch16(emu)); } break;

        /* ACA */
        case 0x1a: { printf("ACA %04x", emu_fetch16(emu)); } break;

        /* SCA */
        case 0x1b: { printf("SCA %04x", emu_fetch16(emu)); } break;

        /* JPR */
        case 0x1c: { printf("JPR %04x", emu_fetch16(emu)); } break;

        /* LDR */
        case 0x1d: { printf("LDR %04x", emu_fetch16(emu)); } break;

        /* STR */
        case 0x1e: { printf("STR %04x", emu_fetch16(emu)); } break;

        /* ADR */
        case 0x1f: { printf("ADR %04x", emu_fetch16(emu)); } break;

        /* SBR */
        case 0x20: { printf("SBR %04x", emu_fetch16(emu)); } break;

        /* CPR */
        case 0x21: { printf("CPR %04x", emu_fetch16(emu)); } break;

        /* ACR */
        case 0x22: { printf("ACR %04x", emu_fetch16(emu)); } break;

        /* SCR */
        case 0x23: { printf("SCR %04x", emu_fetch16(emu)); } break;

        /* CLB */
        case 0x24: { printf("CLB %04x", emu_fetch16(emu)); } break;

        /* NEB */
        case 0x25: { printf("NEB %04x", emu_fetch16(emu)); } break;

        /* INB */
        case 0x26: { printf("INB %04x", emu_fetch16(emu)); } break;

        /* DEB */
        case 0x27: { printf("DEB %04x", emu_fetch16(emu)); } break;

        /* ADB */
        case 0x28: { printf("ADB %04x", emu_fetch16(emu)); } break;

        /* SBB */
        case 0x29: { printf("SBB %04x", emu_fetch16(emu)); } break;

        /* ACB */
        case 0x2a: { printf("ACB %04x", emu_fetch16(emu)); } break;

        /* SCB */
        case 0x2b: { printf("SCB %04x", emu_fetch16(emu)); } break;

        /* CLW */
        case 0x2c: { printf("CLW %04x", emu_fetch16(emu)); } break;

        /* NEW */
        case 0x2d: { printf("NEW %04x", emu_fetch16(emu)); } break;

        /* INW */
        case 0x2e: { printf("INW %04x", emu_fetch16(emu)); } break;

        /* DEW */
        case 0x2f: { printf("DEW %04x", emu_fetch16(emu)); } break;

        /* ADW */
        case 0x30: { printf("ADW %04x", emu_fetch16(emu)); } break;

        /* SBW */
        case 0x31: { printf("SBW %04x", emu_fetch16(emu)); } break;

        /* ACW */
        case 0x32: { printf("ACW %04x", emu_fetch16(emu)); } break;

        /* SCW */
        case 0x33: { printf("SCW %04x", emu_fetch16(emu)); } break;

        /* LDS */
        case 0x34: { printf("LDS %02x", emu_fetch(emu)); } break;

        /* STS */
        case 0x35: { printf("STS %02x", emu_fetch(emu)); } break;

        /* PHS */
        case 0x36: { printf("PHS"); } break;

        /* PLS */
        case 0x37: { printf("PLS"); } break;

        /* JPS */
        case 0x38: { printf("JPS %04x", emu_fetch16(emu)); } break;

        /* RTS */
        case 0x39: { printf("RTS"); } break;

        /* BNE */
        case 0x3a: { printf("BNE %04x", emu_fetch16(emu)); } break;

        /* BEQ */
        case 0x3b: { printf("BEQ %04x", emu_fetch16(emu)); } break;

        /* BCC */
        case 0x3c: { printf("BCC %04x", emu_fetch16(emu)); } break;

        /* BCS */
        case 0x3d: { printf("BCS %04x", emu_fetch16(emu)); } break;

        /* BPL */
        case 0x3e: { printf("BPL %04x", emu_fetch16(emu)); } break;

        /* BMI */
        case 0x3f: { printf("BMI %04x", emu_fetch16(emu)); } break;

        default:
        {
            printf("INV");
        } break;
    }

    emu->pc = pc;
}

#ifdef STANDALONE

void
print_usage(void)
{
    printf("USAGE\n ./emu <input.bin> [-d]\n");
}

int
main(int argc, char *argv[])
{
    int halt;
    int dbg;
    int i;
    int c;
    char *fin_name;
    FILE *fin;

    fin_name = 0;
    dbg = 0;

    if(argc < 2)
    {
        print_usage();
        return(1);
    }

    for(i = 1;
        i < argc;
        ++i)
    {
        if(strcmp(argv[i], "-d") == 0)
        {
            dbg = 1;
        }
        else
        {
            fin_name = argv[i];
        }
    }

    if(!fin_name)
    {
        print_usage();
        return(1);
    }

    fin = fopen(fin_name, "rb");
    if(!fin)
    {
        return(2);
    }

    i = 0;
    while(!feof(fin))
    {
        fread((void *)(emu_mem + i), sizeof(u8), 1, fin);
        ++i;
    }
    fclose(fin);

    emu.mem_read = emu_mem_read;
    emu.mem_write = emu_mem_write;
    emu.pc = 0x0000;

    halt = 0;
    while(!halt)
    {
        if(dbg)
        {
            emu_disasm(&emu);
            printf("\nPC:%04x A:%02x FL:-----%c%c%c SP:%04x\n>",
                emu.pc, emu.a,
                (emu.flags & NEG_BIT) ? 'N' : 'n',
                (emu.flags & CARRY_BIT) ? 'C' : 'c',
                (emu.flags & ZERO_BIT) ? 'Z' : 'z',
                (int)((int)0xff00 + ((int)emu.mem_read(0xffff))));
            c = getchar();

            while(c != 's')
            {
                if(c == 'm')
                {
                    i = 0;
                    c = getchar();
                    while(isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                    {
                        i *= 16;
                        if(isdigit(c))
                        {
                            i += (c - '0');
                        }
                        else if(c >= 'a' && c <= 'f')
                        {
                            i += (c - 'a') + 10;
                        }
                        else if(c >= 'A' && c <= 'F')
                        {
                            i += (c - 'A') + 10;
                        }
                        c = getchar();
                    }

                    printf("%04x: %02x\n", (u16)i, (u8)emu.mem_read(i));
                    fflush(stdout);
                }

                i = c;
                while(i != '\n' && i != EOF)
                {
                    i = getchar();
                }

                printf(">");
                fflush(stdout);
                c = getchar();
            }

            i = c;
            while(i != '\n' && i != EOF)
            {
                i = getchar();
            }

            halt = emu_exec(&emu);
        }
        else
        {
            halt = emu_exec(&emu);
        }
    }

    return(0);
}

#endif
