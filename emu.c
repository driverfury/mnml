#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

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
            printf("asdone %c", (char)emu->a);

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

int
main(int argc, char *argv[])
{
    int halt;
    int dbg;
    int i;
    char *fin;
    FILE *f;

    dbg = 0;

    if(argc < 2)
    {
        printf("USAGE\n ./emu <input.bin> [-d]\n");
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
            fin = argv[i];
        }
    }

    f = fopen(fin, "rb");
    if(!f)
    {
        return(2);
    }

    i = 0;
    while(!feof(f))
    {
        fread((void *)(emu_mem + i), sizeof(u8), 1, f);
        ++i;
    }
    fclose(f);

    emu.mem_read = emu_mem_read;
    emu.mem_write = emu_mem_write;
    emu.pc = 0x0000;

    halt = 0;
    while(!halt)
    {
        if(dbg)
        {
            char buff[256];
            printf("PC:%hx A:%hhx FL:-----%c%c%c\n>",
                emu.pc, emu.a,
                (emu.flags & NEG_BIT) ? 'N' : 'n',
                (emu.flags & CARRY_BIT) ? 'C' : 'c',
                (emu.flags & ZERO_BIT) ? 'Z' : 'z');
            scanf("%s", buff);
            halt = emu_exec(&emu);
        }
        else
        {
            halt = emu_exec(&emu);
        }
    }

    return(0);
}
