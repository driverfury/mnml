/*
 * TODO:
 *
 * [x] Expressions
 * [x] Functions
 * [x] Function calls
 * [ ] Relational exprs
 * [ ] if-else stmts
 * [ ] while stmts
 * [ ] Structs
 * [ ] Arrays
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define MAX_IDENT_LEN 32

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
    if(type == type_void())
    {
        printf("void");
    }
    else if(type == type_char())
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
    char name[MAX_IDENT_LEN+1];
    Type *type;
    struct Sym *params;
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

void
cut(Sym *sym)
{
    Sym *tocut;
    Sym *prev;
    Sym *curr;

    if(syms != sym)
    {
        prev = 0;
        tocut = syms;
        curr = syms;
        while(curr)
        {
            if(curr == sym)
            {
                break;
            }
            prev = curr;
            curr = curr->next;
        }

        if(prev)
        {
            prev->next = 0;
        }

        curr = tocut;
        while(curr)
        {
            prev = curr;
            curr = curr->next;
            free(prev);
        }

        syms = sym;
    }
}

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
mksym(char *name)
{
    Sym *res;

    res = (Sym *)malloc(sizeof(Sym));
    if(res)
    {
        strncpy(res->name, name, MAX_IDENT_LEN);
        res->name[MAX_IDENT_LEN] = 0;

        res->next = 0;
    }

    return(res);
}

Sym *
addsym(char *name)
{
    Sym *res;

    res = mksym(name);
    if(res)
    {
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

    CAST,
    SPMEMB,
};

char ident[MAX_IDENT_LEN + 1];
int intval;
char buff[MAX_IDENT_LEN + 1];

int
look(int update)
{
    int tok;
    char c;
    int i;
    long pos;
    int newline;
    char pbackold;

    tok = END;

    newline = line;
    pos = ftell(fin);
    pbackold = pback;

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
            if(i < MAX_IDENT_LEN)
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
        else if(strcmp(ident, "cast") == 0)   { tok = CAST; }
        else if(strcmp(ident, "->") == 0)     { tok = SPMEMB; }
    }
    else
    {
        tok = (int)c;
    }

    if(update)
    {
        line = newline;
    }
    else
    {
        fseek(fin, pos, SEEK_SET);
        pback = pbackold;
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

enum
{
    EXPR_INTLIT,
    EXPR_ID,

    EXPR_CALL,
    EXPR_ARR,
    EXPR_SMEMB,
    EXPR_SPMEMB,

    EXPR_NEG,
    EXPR_DER,
    EXPR_ADR,
    EXPR_CST,

    EXPR_MUL,
    EXPR_DIV,
    EXPR_MOD,

    EXPR_ADD,
    EXPR_SUB,

    EXPR_COUNT
};

typedef struct
Expr
{
    int kind;

    char id[MAX_IDENT_LEN];
    int val;
    Type *cast;

    struct Expr *l;
    struct Expr *r;

    Type *type;

    struct Expr *next;
} Expr;

Expr *
mkexprint(int val)
{
    Expr *res;

    res = (Expr *)malloc(sizeof(Expr));
    if(res)
    {
        res->type = 0;
        res->kind = EXPR_INTLIT;
        res->val = val;
        res->next = 0;
    }

    return(res);
}

Expr *
mkexprid(char *id)
{
    Expr *res;

    res = (Expr *)malloc(sizeof(Expr));
    if(res)
    {
        res->type = 0;
        res->kind = EXPR_ID;
        strncpy(res->id, id, MAX_IDENT_LEN);
        res->id[MAX_IDENT_LEN] = 0;
        res->next = 0;
    }

    return(res);
}

Expr *
mkexprun(int kind, Expr *l)
{
    Expr *res;

    res = (Expr *)malloc(sizeof(Expr));
    if(res)
    {
        res->type = 0;
        res->kind = kind;
        res->l = l;
        res->next = 0;
    }

    return(res);
}

Expr *
mkexprcast(Type *cast, Expr *l)
{
    Expr *res;

    res = mkexprun(EXPR_CST, l);
    if(res)
    {
        res->type = 0;
        res->cast = cast;
        res->next = 0;
    }

    return(res);
}

Expr *
mkexprbin(int kind, Expr *l, Expr *r)
{
    Expr *res;

    res = (Expr *)malloc(sizeof(Expr));
    if(res)
    {
        res->type = 0;
        res->kind = kind;
        res->l = l;
        res->r = r;
        res->next = 0;
    }

    return(res);
}

Type *
resolve_lvalue(Expr *e)
{
    Type *t;
    Sym *sym;

    t = 0;
    if(e->kind == EXPR_ID)
    {
        sym = lookup(e->id);
        if(!sym)
        {
            fprintf(stderr, "Invalid symbol in expression\n");
            assert(0);
        }

        if(sym->kind == SYM_VAR)
        {
            fprintf(fout, " LDI <%s STA .Z.%s LDI >%s STA .Z.%s+1\n", sym->name, currfunc->name, sym->name, currfunc->name);
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            fprintf(fout, " LDA .BP.%s STA .Z.%s LDA .BP.%s+1 STA .Z.%s+1\n", currfunc->name, currfunc->name, currfunc->name, currfunc->name);
            if(sym->offset != 0)
            {
                fprintf(fout, " LDI %d ADW .Z.%s\n", sym->offset, currfunc->name);
            }
        }
        else
        {
            fprintf(stderr, "Invalid lvalue expression\n");
            assert(0);
        }

        t = sym->type;
    }
    else if(e->kind == EXPR_DER)
    {
        t = resolve_lvalue(e->l);
        if(t->kind != TYPE_PTR)
        {
            fprintf(stderr, "Cannot dereference a non-pointer\n");
        }

        fprintf(fout, " LDA .Z.%s STA .X LDA .Z.%s+1 STA .X+1\n", currfunc->name, currfunc->name);
        fprintf(fout, " LDR .X STA .Z.%s INW .X LDR .X STA .Z.%s+1\n", currfunc->name, currfunc->name);

        t = t->base;
    }
    else
    {
        /* TODO: HERE */
        assert(0);
    }

    assert(t);

    return(t);
}

Type *
resolve_expr(Expr *e, Type *wanted)
{
    Type *t;
    Type *lt;
    Type *rt;
    Sym *sym;
    Expr *arg;
    Sym *param;
    int paramsz;

    assert(e);

    t = 0;
    if(e->kind == EXPR_INTLIT)
    {
        if(e->val > 0xff)
        {
            t = type_int();
        }
        else
        {
            t = type_char();
        }

        if(t == type_char() && wanted == type_int())
        {
            t = type_int();
        }
        else if(t == type_int() && wanted == type_char())
        {
            fprintf(stderr, "Integer overflow, char > 255\n");
            assert(0);
        }

        fprintf(fout, " LDI %d STA .X LDI %d STA .X+1\n", (e->val&0xff), ((e->val>>8)&0xff));
    }
    else if(e->kind == EXPR_ID)
    {
        sym = lookup(e->id);
        if(!sym)
        {
            fprintf(stderr, "Invalid symbol %s in expression\n", e->id);
            assert(0);
        }

        t = sym->type;

        if(sym->kind == SYM_VAR)
        {
            if(t->width == 1)
            {
                fprintf(fout, " LDA %s STA .X\n", sym->name);
            }
            else if(t->width == 2)
            {
                fprintf(fout, " LDA %s STA .X LDA %s+1 STA .X+1\n", sym->name, sym->name);
            }
            else
            {
                assert(0);
            }
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n", currfunc->name, currfunc->name);
            if(sym->offset != 0)
            {
                fprintf(fout, " LDI %d ADW .T", sym->offset);
            }
            if(t->width == 1)
            {
                fprintf(fout, " LDR .T STA .X\n");
            }
            else if(t->width == 2)
            {
                fprintf(fout, " LDR .T STA .X INW .T LDR .T STA .X+1\n");
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            fprintf(stderr, "Invalid var or const %s in expression\n", sym->name);
            assert(0);
        }
    }
    else if(e->kind == EXPR_CALL)
    {
        if(e->l->kind != EXPR_ID)
        {
            fprintf(stderr, "Invalid function call\n");
            assert(0);
        }

        sym = lookup(e->l->id);
        if(!sym || sym->kind != SYM_FUNC)
        {
            fprintf(stderr, "Cannot call a non-function\n");
            assert(0);
        }

        paramsz = 0;
        param = sym->params;
        arg = e->r;
        while(arg)
        {
            if(!param)
            {
                fprintf(stderr, "Invalid param for function %s\n", sym->name);
                assert(0);
            }

            lt = resolve_expr(arg, param->type);
            if(lt != param->type)
            {
                fprintf(stderr, "Function %s param type mismatch\n", sym->name);
                assert(0);
            }

            fprintf(fout, " ;push param %s\n", param->name);
            if(lt->width == 1)
            {
                fprintf(fout, " LDI 0 PHS LDA .X PHS\n");
            }
            else if(lt->width == 2)
            {
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
            }
            else
            {
                assert(0);
            }

            paramsz += ALIGN(lt->width, 2);
            param = param->next;
            arg = arg->next;
        }

        if(param)
        {
            fprintf(stderr, "Invalid num of arguments for function %s\n", sym->name);
            assert(0);
        }

        if(sym->type->width > 0)
        {
            fprintf(fout, " ;reserve space for ret %s\n", sym->name);
            fprintf(fout, " LDI %d SBB 0xffff\n", ALIGN(sym->type->width, 2));
        }

        fprintf(fout, " ;call %s\n", sym->name);
        fprintf(fout, " JPS %s\n", sym->name);

        if(sym->type->width > 0)
        {
            if(ALIGN(sym->type->width, 2) == 2)
            {
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
            }
            else
            {
                assert(0);
            }
        }

        if(paramsz > 0)
        {
            fprintf(fout, " LDI %d ADB 0xffff\n", paramsz);
        }

        t = sym->type;
    }
    else if(e->kind == EXPR_ARR)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_SMEMB)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_SPMEMB)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_NEG)
    {
        lt = resolve_expr(e->l, 0);
        if(lt != type_char() && lt != type_int())
        {
            fprintf(stderr, "Cannot negate a non-arithmetic type\n");
            assert(0);
        }

        t = lt;
        if(t->width == 1)
        {
            fprintf(fout, " LDA .X NEG STA .X\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDA .X STA .T LDA .X+1 STA .T+1\n");
            fprintf(fout, " LDI 0 STA .X STA .X+1\n");
            fprintf(fout, " LDA .T SBB .X LDA .T+1 SCB .X+1\n");
        }
        else
        {
            assert(0);
        }
    }
    else if(e->kind == EXPR_DER)
    {
        lt = resolve_lvalue(e->l);
        if(lt->kind != TYPE_PTR)
        {
            fprintf(stderr, "Cannot dereference a non-pointer");
        }
        t = lt->base;

        if(t->width == 1)
        {
            fprintf(fout, " LDR .X STA .T\n");
            fprintf(fout, " LDA .T STA .X\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDR .X STA .T INW .X LDR .X STA .T+1\n");
            fprintf(fout, " LDA .T STA .X LDA .T+1 STA .X+1\n");
        }
        else
        {
            assert(0);
        }
    }
    else if(e->kind == EXPR_ADR)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_CST)
    {
        t = e->cast;
        lt = resolve_expr(e->l, t);

        if(lt != t)
        {
            if( (lt == type_char() && t == type_int()) ||
                (lt == type_int() && t == type_char()))
            {
                fprintf(fout, " LDI 0 STA .X+1\n");
            }
            else if(lt->kind == TYPE_PTR && t->kind == TYPE_PTR)
            {
                /* Nothing */
            }
            else
            {
                fprintf(fout, "Invalid cast operation\n");
            }
        }
    }
    else if(e->kind == EXPR_MUL)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_DIV)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_MOD)
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(e->kind == EXPR_ADD)
    {
        /* TODO: Pointer aithmetic */
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);
        if( (lt != type_char() && lt != type_int()) ||
            (rt != type_char() && rt != type_int()))
        {
            fprintf(stderr, "Cannot add a non-arithmetic type\n");
            assert(0);
        }

        if(lt == type_char() && rt == type_char())
        {
            t = type_char();
        }
        else
        {
            t = type_int();
        }

        if(t->width == 1)
        {
            fprintf(fout, " LDA .Y ADB .X\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDA .Y ADB .X LDA .Y+1 ACB .X+1\n");
        }
        else
        {
            assert(0);
        }
    }
    else if(e->kind == EXPR_SUB)
    {
        /* TODO: Pointer aithmetic */
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);
        if( (lt != type_char() && lt != type_int()) ||
            (rt != type_char() && rt != type_int()))
        {
            fprintf(stderr, "Cannot add a non-arithmetic type\n");
            assert(0);
        }

        if(lt == type_char() && rt == type_char())
        {
            t = type_char();
        }
        else
        {
            t = type_int();
        }

        if(t->width == 1)
        {
            fprintf(fout, " LDA .Y SBB .X\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDA .Y SBB .X LDA .Y+1 SCB .X+1\n");
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        assert(0);
    }

    assert(t);

    return(t);
}

Expr *parse_expr();

Expr *
parse_expr_base()
{
    Expr *e;
    int t;

    e = 0;
    t = next();
    if(t == INTLIT)
    {
        e = mkexprint(intval);
    }
    else if(t == IDENT)
    {
        e = mkexprid(ident);
    }
    else if(t == '(')
    {
        e = parse_expr();
        expect(')');
    }
    else
    {
        fprintf(stderr, "Invalid base expression\n");
        assert(0);
    }

    return(e);
}

Expr *
parse_expr_first()
{
    Expr *e;
    Expr *args;
    Expr *arg;
    int t;

    e = parse_expr_base();

    t = peek();
    if(t == '(')
    {
        args = 0;
        arg = 0;
        expect('(');
        t = peek();
        while(t != ')')
        {
            if(arg)
            {
                arg->next = parse_expr();
                arg = arg->next;
            }
            else
            {
                arg = parse_expr();
                args = arg;
            }

            t = peek();
            if(t == ',')
            {
                next();
                t = peek();
            }
        }
        expect(')');

        e = mkexprbin(EXPR_CALL, e, args);
    }
    else if(t == '[')
    {
        expect('[');
        e = mkexprun(EXPR_ARR, parse_expr());
        expect(']');
        assert(0);
    }
    else if(t == '.')
    {
        /* TODO: HERE */
        assert(0);
    }
    else if(t == SPMEMB)
    {
        /* TODO: HERE */
        assert(0);
    }

    return(e);
}

Expr *
parse_expr_unary()
{
    Expr *e;
    Type *type;
    int t;

    e = 0;
    t = peek();
    if(t == '+')
    {
        next();
        e = parse_expr_unary();
    }
    else if(t == '-')
    {
        next();
        e = parse_expr_unary();
        e = mkexprun(EXPR_NEG, e);
    }
    else if(t == '*')
    {
        next();
        e = parse_expr_unary();
        e = mkexprun(EXPR_DER, e);
    }
    else if(t == '&')
    {
        next();
        e = parse_expr_unary();
        e = mkexprun(EXPR_ADR, e);
    }
    else if(t == CAST)
    {
        next();
        expect('<');
        type = parse_typespec();
        expect('>');

        e = parse_expr_unary();
        e = mkexprcast(type, e);
    }
    else
    {
        e = parse_expr_first();
    }

    return(e);
}

Expr *
parse_expr_mul()
{
    Expr *l;
    Expr *r;
    int t;
    int k;

    l = 0;
    r = 0;

    l = parse_expr_unary();
    t = peek();
    while(t == '*' || t == '/' || t == '%')
    {
        next();

             if(t == '*') { k = EXPR_MUL; }
        else if(t == '/') { k = EXPR_DIV; }
        else if(t == '%') { k = EXPR_MOD; }
        else { assert(0); }

        r = parse_expr_unary();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr_add()
{
    Expr *l;
    Expr *r;
    int t;
    int k;

    l = 0;
    r = 0;

    l = parse_expr_unary();
    t = peek();
    while(t == '+' || t == '-')
    {
        next();

             if(t == '+') { k = EXPR_ADD; }
        else if(t == '-') { k = EXPR_SUB; }
        else { assert(0); }

        r = parse_expr_unary();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr()
{
    return(parse_expr_add());
}

Sym *
parse_decl(int param)
{
    Sym *sym;
    Type *type;

    type = 0;

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

    assert(sym);

    return(sym);
}

Expr *
parse_lvalue(char *id)
{
    Expr *e;
    int t;

    e = 0;
    if(id)
    {
        e = mkexprid(id);
    }
    else
    {
        t = next();
        if(t == IDENT)
        {
            e = mkexprid(ident);
        }
        else if(t == '*')
        {
            e = parse_lvalue(0);
            e = mkexprun(EXPR_DER, e);
        }
        else if(t == '(')
        {
            e = parse_lvalue(0);
            expect(')');
        }
        else
        {
            fprintf(stderr, "Invalid lvalue\n");
            assert(0);
        }
    }

    t = peek();
    if(t == '[')
    {
        expect('[');
        e = mkexprbin(EXPR_ARR, e, parse_expr());
        expect(']');
    }
    else if(t == '.')
    {
        next();
        expect(IDENT);
        e = mkexprbin(EXPR_SMEMB, e, mkexprid(ident));
    }
    else if(t == SPMEMB)
    {
        next();
        expect(IDENT);
        e = mkexprbin(EXPR_SPMEMB, e, mkexprid(ident));
    }

    assert(e);

    return(e);
}

void
parse_stmt()
{
    Expr *e;
    Type *lt;
    Type *rt;
    Sym *sym;
    Sym *param;
    int t;
    int paramsz;

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

        t = peek();
        if(t != ';')
        {
            e = parse_expr();
            lt = resolve_expr(e, 0);
        }
        else
        {
            lt = type_void();
        }
        expect(';');

        /* TODO: Check function return type compatibility */

        /* TODO: This based on return type width */
        fprintf(fout, " LDA .BP.%s STA .Z.%s LDA .BP.%s+1 STA .Z.%s+1\n", currfunc->name, currfunc->name, currfunc->name, currfunc->name);
        fprintf(fout, " LDI 3 ADW .Z.%s\n", currfunc->name);
        fprintf(fout, " LDA .X STR .Z.%s INW .Z.%s LDA .X+1 STR .Z.%s\n", currfunc->name, currfunc->name, currfunc->name);
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
            strncpy(buff, ident, MAX_IDENT_LEN);
            buff[MAX_IDENT_LEN] = 0;
            sym = lookup(buff);

            next();
            t = peek();
            if(t == '(')
            {
                if(sym->kind != SYM_FUNC)
                {
                    fprintf(stderr, "Cannot call a non-function\n");
                    assert(0);
                }

                paramsz = 0;
                param = sym->params;
                expect('(');
                t = peek();
                while(t != ')')
                {
                    if(!param)
                    {
                        fprintf(stderr, "Invalid param for function %s\n", sym->name);
                        assert(0);
                    }
                    e = parse_expr();
                    lt = resolve_expr(e, param->type);

                    if(lt != param->type)
                    {
                        fprintf(stderr, "Function %s param type mismatch\n", sym->name);
                        assert(0);
                    }

                    fprintf(fout, " ;push param %s\n", param->name);
                    if(lt->width == 1)
                    {
                        fprintf(fout, " LDI 0 PHS LDA .X PHS\n");
                    }
                    else if(lt->width == 2)
                    {
                        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
                    }
                    else
                    {
                        assert(0);
                    }

                    paramsz += ALIGN(lt->width, 2);
                
                    param = param->next;

                    t = peek();
                    if(t != ')')
                    {
                        expect(',');
                        t = peek();
                    }
                }

                if(param)
                {
                    fprintf(stderr, "Invalid num of arguments for function %s\n", sym->name);
                    assert(0);
                }

                expect(')');

                if(sym->type->width > 0)
                {
                    fprintf(fout, " ;reserve space for ret %s\n", sym->name);
                    fprintf(fout, " LDI %d SBB 0xffff\n", ALIGN(sym->type->width, 2));
                    paramsz += ALIGN(sym->type->width, 2);
                }

                fprintf(fout, " ;call %s\n", sym->name);
                fprintf(fout, " JPS %s\n", sym->name);
                if(paramsz > 0)
                {
                    fprintf(fout, " LDI %d ADB 0xffff\n", paramsz);
                }
            }
            else
            {
                e = parse_lvalue(buff);
                expect('=');
                lt = resolve_lvalue(e);
                rt = resolve_expr(parse_expr(), lt);

                if(lt != rt)
                {
                    fprintf(stderr, "Type mismatch in assignment\n");
                    assert(0);
                }

                if(lt->width == 1)
                {
                    fprintf(fout, " LDA .X STR .Z.%s\n", currfunc->name);
                }
                else if(lt->width == 2)
                {
                    fprintf(fout, " LDA .X STR .Z.%s\n", currfunc->name);
                    fprintf(fout, " INW .Z.%s LDA .X+1 STR .Z.%s\n", currfunc->name, currfunc->name);
                }
                else
                {
                    /* TODO: HERE */
                    assert(0);
                }
            }
        }
        else
        {
            e = parse_lvalue(0);
            expect('=');
            lt = resolve_lvalue(e);
            rt = resolve_expr(parse_expr(), lt);

            if(lt != rt)
            {
                fprintf(stderr, "Type mismatch in assignment\n");
                assert(0);
            }

            if(lt->width == 1)
            {
                fprintf(fout, " LDA .X STR .Z.%s\n", currfunc->name);
            }
            else if(lt->width == 2)
            {
                fprintf(fout, " LDA .X STR .Z.%s\n", currfunc->name);
                fprintf(fout, " INW .Z.%s LDA .X+1 STR .Z.%s\n", currfunc->name, currfunc->name);
            }
            else
            {
                /* TODO: HERE */
                assert(0);
            }
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
    Sym *params;
    Sym *param;
    Sym *tmp;

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
        sym = addsym(ident);
        sym->kind = SYM_FUNC;
        sym->offset = 2;
        sym->poffset = 3;
        currfunc = sym;

        fprintf(fout, " LDA 0xffff STA .BP.%s\n", currfunc->name);

        expect(':');
        type = parse_typespec();
        if(type->width > 0)
        {
            sym->poffset += ALIGN(type->width, 2);
        }
        sym->type = type;
        expect(';');

        params = 0;
        param = 0;
        t = peek();
        while(t != '{')
        {
            tmp = parse_decl(1);
            if(param)
            {
                param->next = mksym(tmp->name);
                param = param->next;
            }
            else
            {
                param = mksym(tmp->name);
                params = param;
            }
            param->next = 0;
            param->type = tmp->type;

            expect(';');
            t = peek();
        }
        sym->params = params;

        expect('{');

        t = peek();
        while(t == VAR)
        {
            parse_decl(0);
            expect(';');
            t = peek();
        }

        if(sym->offset > 2)
        {
            fprintf(fout, " LDI %d ADB 0xffff\n", sym->offset);
        }

        while(t != '}')
        {
            parse_stmt();
            t = peek();
        }

        expect('}');

        fprintf(fout, " LDA .BP.%s STA 0xffff\n", currfunc->name);
        fprintf(fout, " RTS\n");

        fprintf(fout, ".BP.%s: 0x00 0xff\n", currfunc->name);
        fprintf(fout, ".Z.%s: 0x00 0xff\n", currfunc->name);

        cut(currfunc);
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
#ifdef GDB
    fout = fopen("test.asm", "w");
#else
    fout = stdout;
#endif
    line = 0;

    fprintf(fout, " LDI 0xfe STA 0xffff\n");
    fprintf(fout, " LDI 0x00 PHS PHS\n");
    fprintf(fout, " JPS main PLS OUT PLS\n");
    fprintf(fout, "___LOOP: JPA ___LOOP\n");
    fprintf(fout, ".X: 0x00 0x00\n");
    fprintf(fout, ".Y: 0x00 0x00\n");
    fprintf(fout, ".T: 0x00 0x00\n");
    parse_unit();

#if 1
    printf(";*** SYM ***\n");
    dumpsym();
    printf(";*** *** ***\n");
#endif

    fclose(fin);
#ifdef GDB
    fclose(fout);
#endif

    return(0);
}
