/*
 * TODO:
 *
 * [ ] Expressions
 * [ ] Function call
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define MAX_IDENT_SIZE 32

#define ALIGN(n,a) ((((n)%(a))>0)?((n)+((a)-((n)%(a)))):(n))

/****************************************************/
/**                      TYPES                     **/
/****************************************************/

enum
{
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_INT,
    TYPE_PTR,
    TYPE_ARR,
    TYPE_STRUCT
};

typedef struct
Type
{
    int kind;
    int width;
    struct Type *base;
} Type;

Type _type_void = { TYPE_VOID, 0, 0 };
Type _type_char = { TYPE_CHAR, 1, 0 };
Type _type_int  = { TYPE_INT,  2, 0 };

Type *
type_void()
{
    return(&_type_void);
}

Type *
type_char()
{
    return(&_type_char);
}

Type *
type_int()
{
    return(&_type_int);
}

void
print_type(Type *type)
{
    if(type == type_char())
    {
        printf("char");
    }
    else if(type == type_int())
    {
        printf("int");
    }
    else if(type->kind == TYPE_PTR)
    {
        printf("[");
        print_type(type->base);
        printf("]");
    }
    else
    {
        assert(0);
    }
}

/****************************************************/
/**                    SYM TABLE                   **/
/****************************************************/

enum
{
    SYM_TYPE,
    SYM_VAR,
    SYM_FUNC,
    SYM_VAR_LOC,
};

typedef struct
Sym
{
    int kind;
    char name[MAX_IDENT_SIZE+1];
    Type *type;
    int offset;
    int poffset;
    struct Sym *next;
} Sym;

Sym *syms;

#if 1

void
dumpsym()
{
    Sym *s;
    int i;

    s = syms;
    i = 0;
    while(s)
    {
        printf(";%04d. %s\n",i, s->name);
        s = s->next;
        ++i;
    }
}

#else

void
dumpsym()
{
    Sym *s;
    int i;
    char *k;

    s = syms;
    i = 0;
    while(s)
    {
        if(s->kind == SYM_VAR)
        {
            k = "var";
        }
        else if(s->kind == SYM_VAR_LOC)
        {
            k = "loc_var";
        }
        else if(s->kind == SYM_FUNC)
        {
            k = "func";
        }
        else if(s->kind == SYM_TYPE)
        {
            k = "typedef";
        }
        else
        {
            assert(0);
        }
        printf("%04d. %s %s: ",i, k, s->name);
        print_type(s->type);
        if(s->kind == SYM_VAR_LOC)
        {
            printf(" - OFF: %03d", s->offset);
        }
        else if(s->kind == SYM_FUNC)
        {
            printf(" - OFF: %03d - POFF: %03d", s->offset, s->poffset);
        }
        printf("\n");
        s = s->next;
        ++i;
    }
}

#endif

Sym *
lookup(char *name)
{
    Sym *res;
    Sym *s;

    res = 0;
    s = syms;
    while(s)
    {
        if(strcmp(name, s->name) == 0)
        {
            res = s;
            break;
        }
        s = s->next;
    }

    return(res);
}

Sym *
addsym(char *name)
{
    Sym *res;

    res = (Sym *)malloc(sizeof(Sym));
    if(res)
    {
        strncpy(res->name, name, MAX_IDENT_SIZE);
        res->name[MAX_IDENT_SIZE] = 0;

        res->next = syms;
        syms = res;
    }
    else
    {
        fprintf(stderr, "Cannot allocate symbol %s\n", name);
        assert(0);
    }

    return(res);
}

/****************************************************/
/**                     SCANNER                    **/
/****************************************************/

FILE *fin;
FILE *fout;

int line;
char pback;

void
putback(char c)
{
    pback = c;
}

char
getch()
{
    char c;

    if(pback)
    {
        c = pback;
        pback = 0;
    }
    else
    {
        if(feof(fin))
        {
            c = 0;
        }
        else
        {
            c = (char)fgetc(fin);
        }
    }

    return(c);
}

enum
{
    END = 128,

    IDENT,
    INTLIT,

    VAR,
    FUNC,

    VOID,
    CHAR,
    INT,
    STRUCT,

    RETURN,
    IF,
    ELSE,
    WHILE,

    SPMEMB,
};

char ident[MAX_IDENT_SIZE + 1];
int intval;
char buff[MAX_IDENT_SIZE + 1];

int
look(int update)
{
    int tok;
    char c;
    int i;
    long pos;
    int newline;

    tok = END;

    newline = line;
    pos = ftell(fin);

    c = getch();
    while(isspace(c))
    {
        if(c == '\n')
        {
            ++newline;
        }
        c = getch();
    }

    if(c <= 0)
    {
        tok = END;
    }
    else if(isdigit(c))
    {
        tok = INTLIT;

        intval = 0;
        while(isdigit(c))
        {
            intval = intval*10 + (int)(c - '0');
            c = getch();
        }
        putback(c);

        /* TODO: Hexadecimal */
    }
    else if(isalpha(c) || c == '_')
    {
        tok = IDENT;
        i = 0;
        ident[0] = 0;

        while(isalnum(c) || c == '_')
        {
            if(i < MAX_IDENT_SIZE)
            {
                ident[i] = c;
                ++i;
            }
            c = getch();
        }
        putback(c);
        ident[i] = 0;

             if(strcmp(ident, "var") == 0)    { tok = VAR; }
        else if(strcmp(ident, "func") == 0)   { tok = FUNC; }
        else if(strcmp(ident, "void") == 0)   { tok = VOID; }
        else if(strcmp(ident, "char") == 0)   { tok = CHAR; }
        else if(strcmp(ident, "int") == 0)    { tok = INT; }
        else if(strcmp(ident, "struct") == 0) { tok = STRUCT; }
        else if(strcmp(ident, "return") == 0) { tok = RETURN; }
        else if(strcmp(ident, "if") == 0)     { tok = IF; }
        else if(strcmp(ident, "else") == 0)   { tok = ELSE; }
        else if(strcmp(ident, "while") == 0)  { tok = WHILE; }
        else if(strcmp(ident, "->") == 0)     { tok = SPMEMB; }
    }
    else
    {
        tok = (int)c;
    }

    if(!update)
    {
        fseek(fin, pos, SEEK_SET);
    }
    else
    {
        line = newline;
    }

    return(tok);
}

int
peek()
{
    return(look(0));
}

int
next()
{
    return(look(1));
}

int
expect(int exp)
{
    int t;
    t = next();
    if(t != exp)
    {
        fprintf(stderr, "LINE %d: Expected token %d, got %d\n", line, exp, t);
        assert(0);
    }
    return(t);
}

/****************************************************/
/**                      PARSER                    **/
/****************************************************/

Sym *currfunc;

void
parse_expr_base()
{
    Sym *sym;
    int t;

    t = next();
    if(t == INTLIT)
    {
        fprintf(fout, " LDI %d STA ___X LDI %d STA ___X+1\n", (intval&0xff), ((intval>>8)&0xff));
    }
    else if(t == IDENT)
    {
        sym = lookup(ident);
        if(!sym)
        {
            fprintf(stderr, "Unknown symbol %s\n", ident);
            assert(0);
        }

        if(sym->kind == SYM_VAR)
        {
            fprintf(fout, " LDA %s STA ___X LDA %s+1 STA ___X+1\n", sym->name, sym->name);
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            /* TODO: Check if this is correct */
            fprintf(fout, " LDA ___BP STA ___Z LDA ___BP+1 STA ___Z+1\n");
            if(sym->offset != 0)
            {
                fprintf(fout, " LDI %d ADW ___Z", sym->offset);
            }
            fprintf(fout, " LDA ___Z STA ___Z INW ___Z LDA ___Z STA ___X+1\n");
        }
        else
        {
            fprintf(stderr, "Invalid symbol %s in expression\n", sym->name);
            assert(0);
        }
    }
    else
    {
        fprintf(stderr, "Invalid base expression\n");
        assert(0);
    }
}

void
parse_expr()
{
    parse_expr_base();
}

Type *
parse_typespec()
{
    Type *res;
    int t;

    t = next();
    if(t == VOID)
    {
        res = type_void();
    }
    else if(t == CHAR)
    {
        res = type_char();
    }
    else if(t == INT)
    {
        res = type_int();
    }
    else
    {
        assert(0);
    }

    return(res);
}

void
parse_decl(int param)
{
    Sym *sym;
    Type *type;

    expect(VAR);
    expect(IDENT);
    sym = addsym(ident);

    if(currfunc)
    {
        sym->kind = SYM_VAR_LOC;
        expect(':');
        type = parse_typespec();
        sym->type = type;

        if(type == type_void())
        {
            fprintf(stderr, "You cannot declare a void variable\n");
            assert(0);
        }

        if(param)
        {
            sym->offset = currfunc->poffset;
            currfunc->poffset += ALIGN(type->width, 2);
        }
        else
        {
            currfunc->offset -= ALIGN(type->width, 2);
            sym->offset = currfunc->offset;
        }
    }
    else
    {
        sym->kind = SYM_VAR;
    }
}

void
parse_lvalue(char *id)
{
    Sym *sym;
    int deref;
    int t;

    deref = 0;
    if(!id)
    {
        t = peek();
        while(t == '*')
        {
            next();
            ++deref;
            t = peek();
        }
        expect(IDENT);
        id = ident;
    }

    sym = lookup(id);
    if(!sym || (sym->kind != SYM_VAR && sym->kind != SYM_VAR_LOC))
    {
        fprintf(stderr, "Invalid lvalue\n");
        assert(0);
    }

    if(sym->kind == SYM_VAR)
    {
        fprintf(fout, " LDI <%s STA ___Z LDI >%s STA ___Z+1\n", sym->name, sym->name);
    }
    else if(sym->kind == SYM_VAR_LOC)
    {
        fprintf(fout, " LDA ___BP STA ___Z LDA ___BP+1 STA ___Z+1\n");
        if(sym->offset != 0)
        {
            fprintf(fout, " LDI %d ADW ___Z\n", sym->offset);
        }
    }

    t = peek();
    if(t == '[')
    {
        if(sym->type->kind != TYPE_ARR && sym->type->kind != TYPE_PTR)
        {
            fprintf(stderr, "Invalid array subscription operator\n");
            assert(0);
        }

        expect('[');
        parse_expr();
        expect(']');

        /* TODO: Check if this is correct */
        fprintf(fout, " LDB ___X ADB ___Z\n");
        fprintf(fout, " LDB ___X+1 ACB ___Z+1\n");
    }
    else if(t == '.')
    {
        if(sym->type->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Invalid struct member access\n");
            assert(0);
        }

        expect('.');
        expect(IDENT);

        /* TODO: HERE */
    }
    else if(t == SPMEMB)
    {
        if(sym->type->kind != TYPE_PTR && sym->type->base->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Invalid struct pointer member access\n");
            assert(0);
        }

        expect(SPMEMB);
        expect(IDENT);

        /* TODO: HERE */
    }
}

void
parse_stmt()
{
    int t;

    t = peek();
    if(t == IF)
    {
        next();
        /* TODO: HERE */
    }
    else if(t == WHILE)
    {
        next();
        /* TODO: HERE */
    }
    else if(t == RETURN)
    {
        next();

        /* TODO: Check function return type compatibility */
        parse_expr();
        expect(';');

        fprintf(fout, " LDA ___BP STA ___Z LDA ___BP+1 STA ___Z+1\n");
        fprintf(fout, " LDI 4 ADW ___Z\n");
        fprintf(fout, " LDA ___X STR ___Z INW ___Z LDA ___X+1 STR ___Z\n");
    }
    else if(t == '{')
    {
        expect('{');
        t = peek();
        while(t != '}')
        {
            parse_stmt();
            t = peek();
        }
        expect('}');
    }
    else
    {
        if(t == IDENT)
        {
            strncpy(buff, ident, MAX_IDENT_SIZE);
            buff[MAX_IDENT_SIZE] = 0;

            next();
            t = peek();
            if(t == '(')
            {
                expect('(');
                /* TODO: Function call */
                /* TODO: HERE */
                expect(')');
            }
            else
            {
                parse_lvalue(buff);
                expect('=');
                parse_expr();

                fprintf(fout, " LDA ___X STR ___Z\n");
                fprintf(fout, " INW ___Z LDA ___X+1 STR ___Z\n");
            }
        }
        else
        {
            parse_lvalue(0);
            expect('=');
            parse_expr();

            fprintf(fout, " LDA ___X STR ___Z\n");
            fprintf(fout, " INW ___Z LDA ___X+1 STR ___Z\n");
        }

        expect(';');
    }
}

void
parse_glob_decl()
{
    Sym *sym;
    Type *type;
    int t;
    int i;

    t = next();
    if(t == VAR)
    {
        expect(IDENT);
        fprintf(fout, "%s:\n", ident);
        sym = addsym(ident);
        sym->kind = SYM_VAR;
        expect(':');
        type = parse_typespec();
        sym->type = type;
        for(i = 0;
            i < type->width;
            ++i)
        {
            fprintf(fout, " 0x%02x", 0);
            if(i > 0 && i%15 == 0)
            {
                fprintf(fout, "\n");
            }
        }
        fprintf(fout, "\n");
        expect(';');
    }
    else if(t == FUNC)
    {
        expect(IDENT);
        fprintf(fout, "%s:\n", ident);
        fprintf(fout, " LDA 0xffff STA ___BP\n");
        sym = addsym(ident);
        sym->kind = SYM_FUNC;
        sym->offset = 2;
        sym->poffset = 2;
        currfunc = sym;

        expect(':');
        type = parse_typespec();
        if(type->width > 0)
        {
            sym->poffset += ALIGN(type->width, 2);
        }
        expect(';');

        t = peek();
        while(t != '{')
        {
            parse_decl(1);
            expect(';');
            t = peek();
        }

        expect('{');

        t = peek();
        while(t == VAR)
        {
            parse_decl(0);
            expect(';');
            t = peek();
        }

        if(sym->offset != 0)
        {
            fprintf(fout, " LDI %d ADB 0xffff\n", sym->offset);
        }

        while(t != '}')
        {
            parse_stmt();
            t = peek();
        }

        expect('}');

        fprintf(fout, " LDA ___BP STA 0xffff\n");
        fprintf(fout, " RTS\n");
    }
    else if(t == STRUCT)
    {
        /* TODO */
        assert(0);
    }
    else
    {
        assert(0);
    }
}

void
parse_unit()
{
    while(peek() != END)
    {
        currfunc = 0;
        parse_glob_decl();
    }
    expect(END);
}

int
main()
{
    fin = fopen("test.min", "r");
    fout = stdout;
    line = 0;

    fprintf(fout, " LDI 0xfe STA 0xffff\n");
    fprintf(fout, " LDI 0x00 PHS PHS\n");
    fprintf(fout, " JPS main PLS PLS OUT\n");
    fprintf(fout, " ___LOOP: JPA ___LOOP\n");
    fprintf(fout, "___X: 0x00 0x00\n");
    fprintf(fout, "___Y: 0x00 0x00\n");
    fprintf(fout, "___Z: 0x00 0x00\n");
    fprintf(fout, "___BP: 0x00 0xff\n");
    parse_unit();

#if 1
    printf(";*** SYM ***\n");
    dumpsym();
    printf(";*** *** ***\n");
#endif

    return(0);
}
