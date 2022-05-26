/*
 * TODO:
 *
 * [x] Function calls
 * [x] Structs
 * [x] Arrays
 * [X] Function def better syntax (C-like)
 * [ ] Relational exprs
 * [ ] if-else stmts
 * [ ] while stmts
 *
 * BUG:
 * [ ] Bug on function call + expr (Ex: a = sum(50, 50) - 3)
 * [ ] Local variables (not params) don't work properly
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

typedef struct Type Type;

typedef struct
SMemb
{
    char name[MAX_IDENT_LEN+1];
    Type *type;
    int offset;
    struct SMemb *next;
} SMemb;

enum
{
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_INT,
    TYPE_PTR,
    TYPE_ARR,
    TYPE_STRUCT
};

struct
Type
{
    int kind;
    int width;
    Type *base;
    int len; /* array length */
    SMemb *members;
};

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

#define PTRS_CACHE_SIZE 100
Type _type_ptrs[PTRS_CACHE_SIZE];
int _type_ptrs_count;

Type *
type_ptr(Type *base)
{
    Type *t;
    int i;

    t = 0;
    for(i = 0;
        i < _type_ptrs_count;
        ++i)
    {
        if(_type_ptrs[i].base == base)
        {
            t = &(_type_ptrs[i]);
        }
    }

    if(!t)
    {
        t = &(_type_ptrs[_type_ptrs_count]);
        ++_type_ptrs_count;

        t->kind = TYPE_PTR;
        t->width = 2;
        t->base = base;
    }

    return(t);
}

#define ARR_CACHE_SIZE 100
Type _type_arr[ARR_CACHE_SIZE];
int _type_arr_count;

Type *
type_array(Type *base, int len)
{
    Type *t;
    int i;

    t = 0;
    for(i = 0;
        i < _type_arr_count;
        ++i)
    {
        if(_type_arr[i].base == base && _type_arr[i].len == len)
        {
            t = &(_type_arr[i]);
        }
    }

    if(!t)
    {
        t = &(_type_arr[_type_arr_count]);
        ++_type_arr_count;

        t->kind = TYPE_ARR;
        t->base = base;
        t->len = len;
        t->width = base->width*len;
    }

    return(t);
}

SMemb *
mksmemb(char *name, Type *type, int offset)
{
    SMemb *res;

    res = (SMemb *)malloc(sizeof(SMemb));
    if(res)
    {
        strncpy(res->name, name, MAX_IDENT_LEN);
        res->name[MAX_IDENT_LEN] = 0;
        res->type = type;
        res->offset = offset;
        res->next = 0;
    }

    return(res);
}

SMemb *
getsmemb(Type *s, char *name)
{
    SMemb *res;
    SMemb *m;

    res = 0;
    assert(s->kind == TYPE_STRUCT);

    m = s->members;
    while(m)
    {
        if(strcmp(m->name, name) == 0)
        {
            res = m;
            break;
        }
        m = m->next;
    }

    return(res);
}

Type *
type_struct(SMemb *members)
{
    Type *res;
    SMemb *m;

    res = (Type *)malloc(sizeof(Type));
    if(res)
    {
        res->kind = TYPE_STRUCT;
        res->width = 0;
        res->members = members;

        m = members;
        while(m)
        {
            res->width += m->type->width;
            m = m->next;
        }
    }

    return(res);
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
FILE *flib;

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

Expr *parse_expr();

int
evalexpr(Expr *e)
{
    int val;

    val = 0;
    if(e->kind == EXPR_INTLIT)
    {
        val = e->val;
    }
    else
    {
        fprintf(stderr, "Invalid constant expression\n");
        assert(0);
    }

    return(val);
}

Type *
parse_typespec()
{
    Type *res;
    Sym *sym;
    int t;
    int len;

    res = 0;
    t = next();
    if(t == IDENT)
    {
        sym = lookup(ident);
        if(!sym || sym->kind != SYM_TYPE)
        {
            fprintf(stderr, "Invalid type %s\n", ident);
            assert(0);
        }
        res = sym->type;
    }
    else if(t == VOID)
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
    else if(t == '[')
    {
        res = type_ptr(parse_typespec());
        expect(']');
    }
    else
    {
        assert(0);
    }

    t = peek();
    if(t == '[')
    {
        expect('[');
        len = evalexpr(parse_expr());
        expect(']');

        res = type_array(res, len);
    }

    return(res);
}

Type *resolve_expr(Expr *e, Type *wanted);

Type *
resolve_lvalue(Expr *e)
{
    Type *t;
    Type *lt;
    Type *rt;
    Sym *sym;
    SMemb *memb;

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
            fprintf(fout, " LDI <%s STA .X LDI >%s STA .X+1\n", sym->name, sym->name);
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            fprintf(fout, " LDA .BP.%s STA .X LDA .BP.%s+1 STA .X+1\n", currfunc->name, currfunc->name);
            if(sym->offset != 0)
            {
                fprintf(fout, " LDI %d ADW .X\n", sym->offset);
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

        fprintf(fout, " LDA .X STA .T LDA .X+1 STA .T+1\n");
        fprintf(fout, " LDR .T STA .X INW .T LDR .T STA .X+1\n");

        t = t->base;
    }
    else if(e->kind == EXPR_ARR)
    {
        lt = resolve_lvalue(e->l);
        if(lt->kind != TYPE_ARR && lt->kind != TYPE_PTR)
        {
            fprintf(stderr, "Invalid array subscription\n");
            assert(0);
        }
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");

        rt = resolve_expr(e->r, type_int());
        if(rt != type_int())
        {
            fprintf(stderr, "Invalid array subscription\n");
            assert(0);
        }

        assert(lt->base->width > 0 && lt->base->width < 256);
        fprintf(fout, " PHS PHS\n");
        fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
        fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
        fprintf(fout, " PLS STA .X PLS STA .X+1\n");

        fprintf(fout, " LDA .X ADB .Y LDA .X+1 ACB .Y+1\n");
        fprintf(fout, " LDA .Y STA .X LDA .Y+1 STA .X+1\n");

        t = lt->base;
    }
    else if(e->kind == EXPR_SMEMB)
    {
        t = resolve_lvalue(e->l);
        if(t->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Cannot access a member of a non-struct\n");
            assert(0);
        }

        if(e->r->kind != EXPR_ID)
        {
            fprintf(stderr, "Invalid struct member access\n");
            assert(0);
        }

        memb = getsmemb(t, e->r->id);
        if(!memb)
        {
            fprintf(stderr, "Invalid struct member %s\n", e->r->id);
            assert(0);
        }

        if(memb->offset != 0)
        {
            assert(memb->offset > 0 && memb->offset < 256);
            fprintf(fout, " LDI %d ADW .X\n", memb->offset);
        }

        t = memb->type;
    }
    else if(e->kind == EXPR_SPMEMB)
    {
        t = resolve_expr(e->l, 0);
        if(t->kind != TYPE_PTR || t->base->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Cannot access a member of a non-struct\n");
            assert(0);
        }

        if(e->r->kind != EXPR_ID)
        {
            fprintf(stderr, "Invalid struct member access\n");
            assert(0);
        }

        t = t->base;
        memb = getsmemb(t, e->r->id);
        if(!memb)
        {
            fprintf(stderr, "Invalid struct member %s\n", e->r->id);
            assert(0);
        }

        if(memb->offset != 0)
        {
            assert(memb->offset > 0 && memb->offset < 256);
            fprintf(fout, " LDI %d ADW .X\n", memb->offset);
        }

        t = memb->type;
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
    SMemb *memb;

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
            if(t->kind == TYPE_ARR)
            {
                resolve_lvalue(e);
                t = type_ptr(t->base);
            }
            else
            {
                if(t->width == 1)
                {
                    fprintf(fout, " LDA %s STA .X LDI 0 STA .X+1\n", sym->name);
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
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            /* TODO: Maybe we can use LDS */
            fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n",
                currfunc->name, currfunc->name);
            if(sym->offset != 0)
            {
                fprintf(fout, " LDI %d ADW .T", sym->offset);
            }

            if(t->kind == TYPE_ARR)
            {
                resolve_lvalue(e);
                t = type_ptr(t->base);
                fprintf(fout, " LDA .T STA .X LDA .T+1 STA .X+1\n");
            }
            else
            {
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

        if(sym->type->width > 0)
        {
            fprintf(fout, " ;reserve space for ret %s\n", sym->name);
            fprintf(fout, " LDI %d SBB 0xffff\n", sym->type->width);
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
                fprintf(fout, " LDA .X PHS\n");
            }
            else if(lt->width == 2)
            {
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
            }
            else
            {
                assert(0);
            }

            paramsz += lt->width;
            param = param->next;
            arg = arg->next;
        }

        if(param)
        {
            fprintf(stderr, "Invalid num of arguments for function %s\n", sym->name);
            assert(0);
        }

        fprintf(fout, " ;call %s\n", sym->name);
        fprintf(fout, " JPS %s\n", sym->name);

        if(paramsz > 0)
        {
            fprintf(fout, " LDI %d ADB 0xffff\n", paramsz);
        }

        if(sym->type->width > 0)
        {
            if(sym->type->width == 1)
            {
                fprintf(fout, " PLS STA .X\n");
            }
            else if(sym->type->width == 2)
            {
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
            }
            else
            {
                /* TODO: What if the return type size is > 2? */
                assert(0);
            }
        }

        t = sym->type;
    }
    else if(e->kind == EXPR_ARR)
    {
        lt = resolve_lvalue(e->l);
        if(lt->kind != TYPE_ARR && lt->kind != TYPE_PTR)
        {
            fprintf(stderr, "Invalid array subscription\n");
            assert(0);
        }
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");

        rt = resolve_expr(e->r, type_int());
        if(rt != type_int())
        {
            fprintf(stderr, "Invalid array subscription\n");
            assert(0);
        }

        assert(lt->base->width > 0 && lt->base->width < 256);
        fprintf(fout, " PHS PHS\n");
        fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
        fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
        fprintf(fout, " PLS STA .X PLS STA .X+1\n");

        fprintf(fout, " LDA .X ADB .Y LDA .X+1 ACB .Y+1\n");
        fprintf(fout, " LDA .Y STA .T LDA .Y+1 STA .T+1\n");

        t = lt->base;
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
    else if(e->kind == EXPR_SMEMB)
    {
        lt = resolve_lvalue(e->l);
        if(lt->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Cannot access a member of a non-struct\n");
            assert(0);
        }

        if(e->r->kind != EXPR_ID)
        {
            fprintf(stderr, "Invalid struct member access\n");
            assert(0);
        }

        memb = getsmemb(lt, e->r->id);
        if(!memb)
        {
            fprintf(stderr, "Invalid struct member %s\n", e->r->id);
            assert(0);
        }

        if(memb->offset != 0)
        {
            assert(memb->offset > 0 && memb->offset < 256);
            fprintf(fout, " LDI %d ADW .X\n", memb->offset);
        }

        fprintf(fout, " LDA .X STA .T LDA .X+1 STA .T+1\n");
        if(memb->type->width == 1)
        {
            fprintf(fout, " LDR .T STA .X\n");
        }
        else if(memb->type->width == 2)
        {
            fprintf(fout, " LDR .T STA .X INW .T LDR .T STA .X+1\n");
        }
        else
        {
            assert(0);
        }

        t = memb->type;
    }
    else if(e->kind == EXPR_SPMEMB)
    {
        lt = resolve_expr(e->l, 0);
        if(lt->kind != TYPE_PTR || lt->base->kind != TYPE_STRUCT)
        {
            fprintf(stderr, "Cannot access a member of a non-struct-ptr\n");
            assert(0);
        }

        if(e->r->kind != EXPR_ID)
        {
            fprintf(stderr, "Invalid struct member access\n");
            assert(0);
        }

        memb = getsmemb(lt->base, e->r->id);
        if(!memb)
        {
            fprintf(stderr, "Invalid struct member %s\n", e->r->id);
            assert(0);
        }

        if(memb->offset != 0)
        {
            assert(memb->offset > 0 && memb->offset < 256);
            fprintf(fout, " LDI %d ADW .X\n", memb->offset);
        }
        fprintf(fout, " LDA .X STA .T LDA .X+1 STA .T+1\n");

        if(memb->type->width == 1)
        {
            fprintf(fout, " LDR .T STA .X\n");
        }
        else if(memb->type->width == 2)
        {
            fprintf(fout, " LDR .T STA .X INW .T LDT .T STA .X+1\n");
        }
        else
        {
            assert(0);
        }

        t = memb->type;
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
        lt = resolve_lvalue(e->l);
        t = type_ptr(lt);
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
                /* TODO: NOTE: This is not always correct. If the number is
                 * negative and you cast from char to int, the MSB must be 0xff
                 */
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
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);

        if(lt->kind == TYPE_PTR || rt->kind == TYPE_PTR)
        {
            if(lt->kind == TYPE_PTR && (rt == type_char() || rt == type_int()))
            {
                t = lt;

                if(lt->base->width == 0)
                {
                    fprintf(stderr, "Cannot apply ptr arithmetic on a void ptr\n");
                    assert(0);
                }

                assert(lt->base->width > 0 && lt->base->width < 256);
                fprintf(fout, " PHS PHS\n");
                fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
                fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
            }
            else if(rt->kind == TYPE_PTR && (lt == type_char() || lt == type_int()))
            {
                t = rt;

                if(rt->base->width == 0)
                {
                    fprintf(stderr, "Cannot apply ptr arithmetic on a void ptr\n");
                    assert(0);
                }

                assert(rt->base->width > 0 && rt->base->width < 256);
                fprintf(fout, " PHS PHS\n");
                fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
                fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
                fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
                fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
            }
            else
            {
                fprintf(stderr, "Invalid pointer arithmetic\n");
                assert(0);
            }
        }
        else if((lt == type_char() || lt == type_int()) &&
                (rt == type_char() || rt == type_int()))
        {
            if(lt == type_char() && rt == type_char())
            {
                t = type_char();
            }
            else if(lt == type_char() && rt == type_int())
            {
                /* Implicit cast */
                /* TODO: What about negative numbers? */
                fprintf(fout, " LDI 0 STA .Y+1\n");
                t = type_int();
            }
            else if(lt == type_int() && rt == type_char())
            {
                /* Implicit cast */
                /* TODO: What about negative numbers? */
                fprintf(fout, " LDI 0 STA .X+1\n");
                t = type_int();
            }
            else
            {
                t = type_int();
            }
        }
        else
        {
            fprintf(stderr, "Cannot add a non-arithmetic type\n");
            assert(0);
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
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);

        if(lt->kind == TYPE_PTR || rt->kind == TYPE_PTR)
        {
            if(lt->kind == TYPE_PTR && (rt == type_char() || rt == type_int()))
            {
                t = lt;

                if(lt->base->width == 0)
                {
                    fprintf(stderr, "Cannot apply ptr arithmetic on a void ptr\n");
                    assert(0);
                }

                assert(lt->base->width > 0 && lt->base->width < 256);
                fprintf(fout, " PHS PHS\n");
                fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
                fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
            }
            else if(rt->kind == TYPE_PTR && (lt == type_char() || lt == type_int()))
            {
                t = rt;

                if(rt->base->width == 0)
                {
                    fprintf(stderr, "Cannot apply ptr arithmetic on a void ptr\n");
                    assert(0);
                }

                assert(rt->base->width > 0 && rt->base->width < 256);
                fprintf(fout, " PHS PHS\n");
                fprintf(fout, " LDI 0 PHS LDI %d PHS\n", lt->base->width);
                fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
                fprintf(fout, " JPS .WMUL PLS PLS PLS PLS\n");
                fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
            }
            else
            {
                fprintf(stderr, "Invalid pointer arithmetic\n");
                assert(0);
            }
        }
        else if((lt == type_char() || lt == type_int()) &&
                (rt == type_char() || rt == type_int()))
        {
            if(lt == type_char() && rt == type_char())
            {
                t = type_char();
            }
            else if(lt == type_char() && rt == type_int())
            {
                /* Implicit cast */
                /* TODO: What about negative numbers? */
                fprintf(fout, " LDI 0 STA .Y+1\n");
                t = type_int();
            }
            else if(lt == type_int() && rt == type_char())
            {
                /* Implicit cast */
                /* TODO: What about negative numbers? */
                fprintf(fout, " LDI 0 STA .X+1\n");
                t = type_int();
            }
            else
            {
                t = type_int();
            }
        }
        else
        {
            fprintf(stderr, "Cannot add a non-arithmetic type\n");
            assert(0);
        }

        if(t->width == 1)
        {
            fprintf(fout, " LDA .X SBB .Y\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDA .X SBB .Y LDA .X+1 SCB .Y+1\n");
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
        e = mkexprbin(EXPR_ARR, e, parse_expr());
        expect(']');
    }
    else if(t == '.')
    {
        expect('.');
        expect(IDENT);
        e = mkexprbin(EXPR_SMEMB, e, mkexprid(ident));
    }
    else if(t == SPMEMB)
    {
        expect(SPMEMB);
        expect(IDENT);
        e = mkexprbin(EXPR_SPMEMB, e, mkexprid(ident));
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
parse_param()
{
    Sym *sym;
    Type *type;

    type = 0;

    expect(IDENT);
    sym = mksym(ident);
    sym->kind = SYM_VAR_LOC;
    expect(':');
    type = parse_typespec();
    sym->type = type;

    if(type == type_void())
    {
        fprintf(stderr, "Invalid void type param\n");
        assert(0);
    }

    sym->offset = currfunc->poffset;
    currfunc->poffset += type->width;

    assert(sym);

    return(sym);
}

Sym *
parse_decl()
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

        if(type->width == 1)
        {
            sym->offset = currfunc->offset;
            currfunc->offset -= 1;
        }
        else
        {
            currfunc->offset -= (type->width - 1);
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

        /* TODO: This should be based on return type width */
        fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n",
            currfunc->name, currfunc->name);
        fprintf(fout, " LDI %d ADW .T\n", currfunc->poffset);
        fprintf(fout, " LDA .X STR .T INW .T LDA .X+1 STR .T\n");
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

                if(sym->type->width > 0)
                {
                    fprintf(fout, " ;reserve space for ret %s\n", sym->name);
                    fprintf(fout, " LDI %d SBB 0xffff\n", sym->type->width);
                    paramsz += sym->type->width;
                }

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
                        fprintf(fout, " LDA .X PHS\n");
                    }
                    else if(lt->width == 2)
                    {
                        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
                    }
                    else
                    {
                        assert(0);
                    }

                    paramsz += lt->width;
                
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
                fprintf(fout, " LDA .X STA .Z.%s LDA .X+1 STA .Z.%s+1\n",
                    currfunc->name, currfunc->name);
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
            fprintf(fout, " LDA .X STA .Z.%s LDA .X+1 STA .Z.%s+1\n",
                currfunc->name, currfunc->name);
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
    SMemb *members;
    SMemb *memb;
    char buff[MAX_IDENT_LEN+1];

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
        sym->offset = 0;
        sym->poffset = 3;
        currfunc = sym;

        fprintf(fout, " LDA 0xffff STA .BP.%s\n", currfunc->name);

        params = 0;
        param = 0;
        expect('(');
        t = peek();
        while(t != ')')
        {
            if(param)
            {
                param->next = parse_param();
                param = param->next;
            }
            else
            {
                param = parse_param();
                params = param;
            }

            tmp = addsym(param->name);
            tmp->kind = param->kind;
            tmp->type = param->type;
            tmp->offset = param->offset;

            t = peek();
            if(t != ')')
            {
                expect(',');
                t = peek();
            }
        }
        expect(')');

        sym->params = params;

        expect(':');
        type = parse_typespec();
        sym->type = type;

        expect('{');

        t = peek();
        while(t == VAR)
        {
            parse_decl();
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
#if 0
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
#endif

        fprintf(fout, " LDA .BP.%s STA 0xffff\n", currfunc->name);
        fprintf(fout, " RTS\n");

        fprintf(fout, ".BP.%s: 0x00 0xff\n", currfunc->name);
        fprintf(fout, ".Z.%s: 0x00 0xff\n", currfunc->name);

        cut(currfunc);
    }
    else if(t == STRUCT)
    {
        expect(IDENT);
        sym = addsym(ident);
        sym->kind = SYM_TYPE;
        sym->type = type_struct(0);

        expect('{');

        i = 0;
        t = peek();
        while(t != '}')
        {
            expect(IDENT);
            strncpy(buff, ident, MAX_IDENT_LEN);
            buff[MAX_IDENT_LEN] = 0;

            expect(':');

            type = parse_typespec();

            /**
             * NOTE: This avoids cyclic dependency (parent struct as member or a
             * void variable).
             *
             */
            if(type == 0)
            {
                fprintf(stderr, "Invalid member %s in struct %s\n", buff, sym->name);
                assert(0);
            }

            if(memb)
            {
                memb->next = mksmemb(buff, type, i);
                memb = memb->next;
            }
            else
            {
                memb = mksmemb(buff, type, i);
                members = memb;
            }
            i += type->width;

            expect(';');

            t = peek();
        }

        if(!members)
        {
            fprintf(stderr, "Cannot declare a void struct\n");
            assert(0);
        }

        expect('}');

        sym->type->members = members;
        memb = sym->type->members;
        while(memb)
        {
            sym->type->width += memb->type->width;
            memb = memb->next;
        }
    }
    else
    {
        fprintf(stderr, "Invalid global decl\n");
        fprintf(stderr, "T: %d\n", t);
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

void
regmul(char *x, char *y)
{
    fprintf(fout, " LDA X");
}

int
main()
{
    Sym *sym;
    Sym *param;
    int i;

    fin = fopen("test.min", "r");
#ifdef GDB
    fout = fopen("test.asm", "w");
#else
    fout = stdout;
#endif
    line = 1;

    /* std lib */
#if 1
    flib = fopen("minlib.asm", "r");
    while(!feof(flib))
    {
        i = fgetc(flib);
        if(i >= 0)
        {
            fputc(i, fout);
        }
    }
    fputc('\n', fout);
    fclose(flib);

    sym = addsym("puts");
    sym->kind = SYM_FUNC;
    sym->type = type_void();
    sym->offset = 0;
    sym->poffset = 3+2;
    param = mksym("s");
    param->type = type_ptr(type_char());
    param->offset = 3;
    param->next = 0;
    sym->params = param;
#else
#endif

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
