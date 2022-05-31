/*
 * TODO:
 *
 * [x] Function calls
 * [x] Structs
 * [x] Arrays
 * [X] Function def better syntax (C-like)
 * [x] Relational exprs
 * [x] Equality exprs
 * [x] if-else stmts
 * [x] while stmts
 * [x] Logic AND and OR (&& ||)
 * [x] Char literals
 * [x] String literals
 * [x] Comments
 * [x] Typedefs
 * [x] Modulo operator
 * [ ] Multiplication operator
 * [ ] Division operator
 * [ ] Logical NOT (!)
 * [x] Eval const expr
 * [/] Better resolve_expr function
 * [x] Constants
 * [ ] Enums
 * [ ] What about structs as:
 *       - return value: this is how gcc solves it, the caller reserve space for the
 *         struct return value, then it pushes a pointer to it instead of the entire struct.
 *         When the callee returns, the struct whose address is pushed gets modified.
 *       - params: the struct is pushed entirely onto the stack
 *
 * BUG:
 * [x] Function call params order is inverted
 * [x] while stmt is bugged? or logical exprs?
 * [x] Signed cast
 * [x] Local variables are bugged. Check bugged.min
 * [x] Comments
 * [ ] Complex expressions like 97 + 7%15
 *     97 gets stored in X, then Y <- X
 *     But when resolving the right operand, Y gets overwritten.
 *     Solution: push the temp result onto the stack.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

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
    SYM_CONST,
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
    int val;
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
FILE *fstr;
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

int lblcount;

int
newlbl()
{
    int res;
    res = lblcount;
    ++lblcount;
    return(res);
}

enum
{
    END = 128,

    IDENT,
    INTLIT,
    STRLIT,

    VAR,
    FUNC,
    CONST,

    VOID,
    CHAR,
    INT,
    STRUCT,
    TYPEDEF,

    RETURN,
    IF,
    ELSE,
    WHILE,

    CAST,

    SPMEMB,

    LESSEQ,
    GRTREQ,
    EQ,
    NEQ,
    AND,
    OR,
};

char ident[MAX_IDENT_LEN + 1];
int intval;
char buff[MAX_IDENT_LEN + 1];
char strlbl[MAX_IDENT_LEN + 1];
int isstrlit = -1;

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

    /* Skip whitespaces */
    while(isspace(c))
    {
        if(c == '\n')
        {
            ++newline;
        }
        c = getch();
    }

    /* Skip comments */
    if(c == '/')
    {
        i = getch();
        if(i == '/')
        {
            while(i != '\n' && i != EOF)
            {
                i = getch();
            }

            if(i == EOF)
            {
                return END;
            }

            /* Skip whitespaces */
            c = i;
            while(isspace(c))
            {
                if(c == '\n')
                {
                    ++newline;
                }
                c = getch();
            }
        }
        else
        {
            putback(i);
        }
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

             if(strcmp(ident, "var") == 0)     { tok = VAR; }
        else if(strcmp(ident, "func") == 0)    { tok = FUNC; }
        else if(strcmp(ident, "const") == 0)   { tok = CONST; }
        else if(strcmp(ident, "void") == 0)    { tok = VOID; }
        else if(strcmp(ident, "char") == 0)    { tok = CHAR; }
        else if(strcmp(ident, "int") == 0)     { tok = INT; }
        else if(strcmp(ident, "struct") == 0)  { tok = STRUCT; }
        else if(strcmp(ident, "typedef") == 0) { tok = TYPEDEF; }
        else if(strcmp(ident, "return") == 0)  { tok = RETURN; }
        else if(strcmp(ident, "if") == 0)      { tok = IF; }
        else if(strcmp(ident, "else") == 0)    { tok = ELSE; }
        else if(strcmp(ident, "while") == 0)   { tok = WHILE; }
        else if(strcmp(ident, "cast") == 0)    { tok = CAST; }
    }
    else if(c == '-')
    {
        tok = (int)c;
        c = getch();
        if(c == '>')
        {
            tok = SPMEMB;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '<')
    {
        tok = (int)c;
        c = getch();
        if(c == '=')
        {
            tok = LESSEQ;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '>')
    {
        tok = (int)c;
        c = getch();
        if(c == '=')
        {
            tok = GRTREQ;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '=')
    {
        tok = (int)c;
        c = getch();
        if(c == '=')
        {
            tok = EQ;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '!')
    {
        tok = (int)c;
        c = getch();
        if(c == '=')
        {
            tok = NEQ;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '&')
    {
        tok = (int)c;
        c = getch();
        if(c == '&')
        {
            tok = AND;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '|')
    {
        tok = (int)c;
        c = getch();
        if(c == '|')
        {
            tok = OR;
        }
        else
        {
            putback(c);
        }
    }
    else if(c == '\'')
    {
        tok = INTLIT;

        c = getch();
        /* TODO: Escapes */
        intval = (int)c;

        c = getch();
        if(c != '\'')
        {
            fprintf(stderr, "Invalid char literal\n");
            assert(0);
        }
    }
    else if(c == '"')
    {
        tok = STRLIT;

        /* NOTE: There's no snprintf in ANSI C90 */
        if(!isstrlit)
        {
            isstrlit = 1;
            sprintf(strlbl, ".S%d", newlbl());
            fprintf(fstr, "%s:\n", strlbl);

            i = 0;
            c = getch();
            while(c != '"')
            {
                /* TODO: Escapes */
                fprintf(fstr, " 0x%02x", c);
                if((i > 0) && (i % 16 == 0))
                {
                    fprintf(fstr, "\n");
                }

                ++i;
                c = getch();
            }
            fprintf(fstr, " 0x00\n");
        }
        else
        {
            /* String literal already defined, skip it */
            c = getch();
            while(c != '"')
            {
                c = getch();
            }
        }

        if(c != '"')
        {
            fprintf(stderr, "Invalid string literal\n");
            assert(0);
        }
    }
    else
    {
        tok = (int)c;
    }

    if(update)
    {
        line = newline;
        isstrlit = 0;
    }
    else
    {
        fseek(fin, pos, SEEK_SET);
        pback = pbackold;
        if(tok == STRLIT)
        {
            isstrlit = 1;
        }
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
    EXPR_STRLIT,
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

    EXPR_LESS,
    EXPR_LESSEQ,
    EXPR_GRTR,
    EXPR_GRTREQ,

    EXPR_EQ,
    EXPR_NEQ,

    EXPR_AND,
    EXPR_OR,

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
mkexprstr(char *id)
{
    Expr *res;

    res = (Expr *)malloc(sizeof(Expr));
    if(res)
    {
        res->type = 0;
        res->kind = EXPR_STRLIT;
        strncpy(res->id, id, MAX_IDENT_LEN);
        res->id[MAX_IDENT_LEN] = 0;
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
isconstexpr(Expr *e)
{
    int res;
    Sym *sym;

    res = 0;
    if(e->kind == EXPR_INTLIT)
    {
        res = 1;
    }
    else if(e->kind == EXPR_ID)
    {
        sym = lookup(e->id);
        if(!sym || sym->kind != SYM_CONST)
        {
            res = 0;
        }
        else
        {
            res = 1;
        }
    }
    else if(e->kind == EXPR_NEG)    { res = isconstexpr(e->l); }
    else if(e->kind == EXPR_MUL)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_DIV)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_MOD)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_ADD)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_SUB)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_LESS)   { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_LESSEQ) { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_GRTR)   { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_GRTREQ) { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_EQ)     { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_NEQ)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_AND)    { res = isconstexpr(e->l) && isconstexpr(e->r); }
    else if(e->kind == EXPR_OR)     { res = isconstexpr(e->l) && isconstexpr(e->r); }

    return(res);
}

int
evalexpr(Expr *e)
{
    int val;
    Sym *sym;

    val = 0;
    if(e->kind == EXPR_INTLIT)
    {
        val = e->val;
    }
    else if(e->kind == EXPR_ID)
    {
        sym = lookup(e->id);
        if(!sym || sym->kind != SYM_CONST)
        {
            fprintf(stderr, "Invalid symbol %s in expression\n", e->id);
            assert(0);
        }

        val = sym->val;
    }
    else if(e->kind == EXPR_NEG)
    {
        val = -evalexpr(e->l);
    }
    else if(e->kind == EXPR_MUL)
    {
        val = evalexpr(e->l) * evalexpr(e->r);
    }
    else if(e->kind == EXPR_DIV)
    {
        val = evalexpr(e->l) / evalexpr(e->r);
    }
    else if(e->kind == EXPR_MOD)
    {
        val = evalexpr(e->l) % evalexpr(e->r);
    }
    else if(e->kind == EXPR_ADD)
    {
        val = evalexpr(e->l) + evalexpr(e->r);
    }
    else if(e->kind == EXPR_SUB)
    {
        val = evalexpr(e->l) - evalexpr(e->r);
    }
    else if(e->kind == EXPR_LESS)
    {
        val = (evalexpr(e->l) < evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_LESSEQ)
    {
        val = (evalexpr(e->l) <= evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_GRTR)
    {
        val = (evalexpr(e->l) > evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_GRTREQ)
    {
        val = (evalexpr(e->l) >= evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_EQ)
    {
        val = (evalexpr(e->l) == evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_NEQ)
    {
        val = (evalexpr(e->l) != evalexpr(e->r)) ? 0xff : 0;
    }
    else if(e->kind == EXPR_AND)
    {
        if(evalexpr(e->l) != 0 && evalexpr(e->r) != 0)
        {
            val = 0xff;
        }
        else
        {
            val = 0;
        }
    }
    else if(e->kind == EXPR_OR)
    {
        if(evalexpr(e->l) != 0 || evalexpr(e->r) != 0)
        {
            val = 0xff;
        }
        else
        {
            val = 0;
        }
    }
    else
    {
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
    Expr *e;

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
        e = parse_expr();
        if(!isconstexpr(e))
        {
            fprintf(stderr, "Invalid constant expression as array length\n");
        }
        len = evalexpr(e);
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
            if(sym->offset > 0)
            {
                fprintf(fout, " LDI %d ADW .X\n", sym->offset);
            }
            else if(sym->offset < 0)
            {
                fprintf(fout, " LDI %d SBW .X\n", -sym->offset);
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
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");

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

        fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
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

#if 1

Type *
resolve_expr(Expr *e, Type *wanted)
{
    Type *t;
    Sym *sym;
    Expr *tmpexpr;

    t = 0;
    if(isconstexpr(e))
    {
        tmpexpr = mkexprint(evalexpr(e));
        t = resolve_expr(tmpexpr, wanted);
        free(tmpexpr);
    }
    else if(e->kind == EXPR_INTLIT)
    {
        if(e->val > 0xff)
        {
            t = type_int();
            fprintf(fout, " LDI %d STA .X LDI %d STA .X+1\n",
                (e->val&0xff), ((e->val>>8)&0xff));
        }
        else
        {
            t = type_char();
            fprintf(fout, " LDI %d STA .X\n");
        }

        if(t == type_int() && wanted == type_char())
        {
            fprintf(stderr, "Integer overflow, char > 255\n");
            assert(0);
        }
    }
    else if(e->kind == EXPR_STRLIT)
    {
        fprintf(fout, " LDI <%s STA .X LDI >%s STA .X+1\n", e->id, e->id);
        t = type_ptr(type_char());
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
            if(t == type_char())
            {
                fprintf(fout, " LDA %s STA .X\n", sym->name);
            }
            else if(t == type_int() || t->kind == TYPE_PTR)
            {
                fprintf(fout, " LDA %s STA .X LDA %s+1 STA .X+1\n", sym->name, sym->name);
            }
            else if(t->kind == TYPE_ARR)
            {
                resolve_lvalue(e);
                t = type_ptr(t->base);
            }
            else
            {
                resolve_lvalue(e);
            }
        }
        else if(sym->kind == SYM_VAR_LOC)
        {
            fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n",
                currfunc->name, currfunc->name);
            if(sym->offset > 0)
            {
                fprintf(fout, " LDI %d ADW .T\n", sym->offset);
            }
            else if(sym->offset < 0)
            {
                fprintf(fout, " LDI %d SBW .T\n", -sym->offset);
            }

            if(t == type_char())
            {
                fprintf(fout, " LDR .T STA .X\n");
            }
            else if(t == type_int() || t->kind == TYPE_PTR)
            {
                fprintf(fout, " LDR .T STA .X INW .T LDR .T STA .X+1\n");
            }
            else if(t->kind == TYPE_ARR)
            {
                fprintf(fout, " LDA .T STA .X LDA .T+1 STA .X+1\n");
                t = type_ptr(t->base);
            }
            else
            {
                resolve_lvalue(e);
            }
        }
        else if(sym->kind == SYM_CONST)
        {
            tmpexpr = mkexprint(sym->val);
            t = resolve_expr(tmpexpr, wanted);
            free(tmpexpr);
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

            assert(lt->width > 0);

            /* TODO: HERE */
            /* TODO: What about types with width > 1 */
            /**
             * TODO: Should we check based on width or on type_char()/type_int(), etc..?
             * Problem 1: if width > 2 (for example a struct), you push the whole
             * struct onto the stack, but when you retrieve the return value you can't
             * save it into X since X is only 2 bytes. How to solve this?
             * Problem 2: the arrays must be pushed ALWAYS by reference (decay to pointers),
             * to solve this problem with need 2 things:
             *   - Array to ptr decay in resolve_expr of EXPR_ID
             *   - Array to ptr decay of the function param: every function param that is
             *     declared as array must be set to a pointer.
             * Problem 3: when t is array but its width is <= 2, you must push its addres
             * not its content.
             */
            if(lt == type_char())
            {
                fprintf(fout, " LDA .X PHS\n");
            }
            else if(lt == type_int())
            {
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
            }
            else if(lt->kind == TYPE_PTR || lt->kind == TYPE_ARR)
            {
                fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
            }
            else
            {
                fprintf(stderr, "Cannot push structs onto the stack yet\n");
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

        fprintf(fout, " JPS %s\n", sym->name);

        if(paramsz > 0)
        {
            fprintf(fout, " LDI %d ADB 0xffff\n", paramsz);
        }

        if(sym->type != type_void())
        {
            /* Nothing */
        }
        if(sym->type == type_char())
        {
            fprintf(fout, " PLS STA .X\n");
        }
        else if(sym->type == type_int())
        {
            fprintf(fout, " PLS STA .X PLS STA .X+1\n");
        }
        else if(sym->type->kind == TYPE_PTR || sym->type->kind == TYPE_ARR)
        {
            fprintf(fout, " PLS STA .X PLS STA .X+1\n");
        }
        else
        {
            fprintf(stderr, "Cannot pop structs from the stack yet\n");
            assert(0);
        }

        t = sym->type;
    }
    else
    {
        fprintf(stderr, "Cannot resolve the expression\n");
        assert(0);
    }

    if(t == type_char() && wanted == type_int())
    {
        /* Implicit cast */
        fprintf(fout, " PHS PHS LDA .X PHS\n");
        fprintf(fout, " JPS .BTOW PLS\n");
        fprintf(fout, " PLS STA .X PLS STA .X+1\n");
    }

    assert(t);

    return(t);
}

#else

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
    char *ins;
    int lbl1;
    int lbl2;

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
    else if(e->kind == EXPR_STRLIT)
    {
        fprintf(fout, " LDI <%s STA .X LDI >%s STA .X+1\n", e->id, e->id);
        t = type_ptr(type_char());
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
            fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n",
                currfunc->name, currfunc->name);
            if(sym->offset > 0)
            {
                fprintf(fout, " LDI %d ADW .T\n", sym->offset);
            }
            else if(sym->offset < 0)
            {
                fprintf(fout, " LDI %d SBW .T\n", -sym->offset);
            }

            if(t->kind == TYPE_ARR)
            {
                fprintf(fout, " LDA .T STA .X LDA .T+1 STA .X+1\n");
                t = type_ptr(t->base);
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
            if(lt == type_char() && t == type_int())
            {
                fprintf(fout, " PHS PHS LDA .X PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
            }
            else if(lt == type_int() && t == type_char())
            {
                fprintf(fout, " PHS LDA .X+1 PHS LDA .X PHS\n");
                fprintf(fout, " JPS .WTOB PLS PLS\n");
                fprintf(fout, " PLS STA .X\n");
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
                /* NOTE: Implicit cast */
                fprintf(fout, " PHS PHS LDA .Y PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
                t = type_int();
            }
            else if(lt == type_int() && rt == type_char())
            {
                /* Implicit cast */
                fprintf(fout, " PHS PHS LDA .X PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
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

        fprintf(fout, " PHS PHS\n");
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
        fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
        fprintf(fout, " JPS .WMOD PLS PLS PLS PLS\n");
        fprintf(fout, " PLS STA .X PLS STA .X+1\n");
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
                /* NOTE: Implicit cast */
                fprintf(fout, " PHS PHS LDA .Y PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
                t = type_int();
            }
            else if(lt == type_int() && rt == type_char())
            {
                /* Implicit cast */
                fprintf(fout, " PHS PHS LDA .X PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
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
                fprintf(fout, " PHS PHS LDA .Y PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
                t = type_int();
            }
            else if(lt == type_int() && rt == type_char())
            {
                /* Implicit cast */
                fprintf(fout, " PHS PHS LDA .X PHS\n");
                fprintf(fout, " JPS .BTOW PLS\n");
                fprintf(fout, " PLS STA .X PLS STA .X+1\n");
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
            fprintf(fout, " LDA .Y STA .X\n");
        }
        else if(t->width == 2)
        {
            fprintf(fout, " LDA .X SBB .Y LDA .X+1 SCB .Y+1\n");
            fprintf(fout, " LDA .Y STA .X LDA .Y+1 STA .X+1\n");
        }
        else
        {
            assert(0);
        }
    }
    else if(e->kind == EXPR_LESS || e->kind == EXPR_LESSEQ ||
            e->kind == EXPR_GRTR || e->kind == EXPR_GRTREQ)
    {
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);

        if(lt != rt)
        {
            fprintf(stderr, "Cannot compare two different types\n");
            assert(0);
        }

        if(lt == type_char())
        {
            fprintf(fout, " PHS PHS LDA .X PHS\n");
            fprintf(fout, " JPS .BTOW PLS\n");
            fprintf(fout, " PLS STA .X PLS STA .X+1\n");

            fprintf(fout, " PHS PHS LDA .Y PHS\n");
            fprintf(fout, " JPS .BTOW PLS\n");
            fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
        }
        else if(lt == type_int() || lt->kind == TYPE_PTR)
        {
            /* Nothing */
        }
        else
        {
            fprintf(stderr, "Invalid types in comparison\n");
            assert(0);
        }

             if(e->kind == EXPR_LESS)   { ins = ".WLESS"; }
        else if(e->kind == EXPR_LESSEQ) { ins = ".WLESSEQ"; }
        else if(e->kind == EXPR_GRTR)   { ins = ".WGRTR"; }
        else if(e->kind == EXPR_GRTREQ) { ins = ".WGRTREQ"; }
        else { assert(0); }

        fprintf(fout, " PHS\n");
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");
        fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
        fprintf(fout, " JPS %s\n", ins);
        fprintf(fout, " PLS PLS PLS PLS\n");
        fprintf(fout, " PLS STA .X LDI 0 STA .X+1\n");

        t = type_char();
    }
    else if(e->kind == EXPR_EQ || e->kind == EXPR_NEQ)
    {
        lt = resolve_expr(e->l, 0);
        fprintf(fout, " LDA .X STA .Y LDA .X+1 STA .Y+1\n");
        rt = resolve_expr(e->r, lt);

        if(lt != rt)
        {
            fprintf(stderr, "Cannot compare two different types\n");
            assert(0);
        }

        if(lt == type_char())
        {
            fprintf(fout, " LDI 0 STA .X+1 STA .Y+1\n");
        }
        else if(lt == type_int() || lt->kind == TYPE_PTR)
        {
            /* Nothing */
        }
        else
        {
            fprintf(stderr, "Invalid types in comparison\n");
            assert(0);
        }

        fprintf(fout, " LDA .X SBB .Y LDA .X+1 SCB .Y+1\n");
        fprintf(fout, " PHS\n");
        fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
        fprintf(fout, " JPS .WZERO PLS PLS PLS\n");
        if(e->kind == EXPR_NEQ)
        {
            fprintf(fout, " NEG\n");
        }
        fprintf(fout, " STA .X LDI 0 STA .X+1\n");

        t = type_char();
    }
    else if(e->kind == EXPR_AND || e->kind == EXPR_OR)
    {
        lbl1 = newlbl();
        lbl2 = newlbl();

        lt = resolve_expr(e->l, type_char());
        if(lt != type_char())
        {
            fprintf(stderr, "Invalid logic expr (only type char is allowed as operand)\n");
        }
        if(e->kind == EXPR_AND)
        {
            fprintf(fout, " LDA .X CPI 0 BEQ .L%d\n", lbl1);
        }
        else
        {
            fprintf(fout, " LDA .X CPI 0 BNE .L%d\n", lbl1);
        }

        rt = resolve_expr(e->r, type_char());
        if(rt != type_char())
        {
            fprintf(stderr, "Invalid logic expr (only type char is allowed as operand)\n");
        }
        if(e->kind == EXPR_AND)
        {
            fprintf(fout, " LDA .X CPI 0 BEQ .L%d\n", lbl1);
            fprintf(fout, " LDI 0xff STA .X JPA .L%d\n", lbl2);
        }
        else
        {
            fprintf(fout, " LDA .X CPI 0 BNE .L%d\n", lbl1);
            fprintf(fout, " LDI 0 STA .X JPA .L%d\n", lbl2);
        }

        fprintf(fout, ".L%d:\n", lbl1);
        if(e->kind == EXPR_AND)
        {
            fprintf(fout, " LDI 0 STA .X\n");
        }
        else
        {
            fprintf(fout, " LDI 0xff STA .X\n");
        }
        fprintf(fout, ".L%d:\n", lbl2);

        t = type_char();
    }
    else
    {
        assert(0);
    }

    assert(t);

    return(t);
}

#endif

Expr *
revargs(Expr *args)
{
    Expr *res;

    res = 0;
    if(!args || !(args->next))
    {
        res = args;
    }
    else
    {
        res = revargs(args->next);
        args->next->next = args;
        args->next = 0;
    }

    return(res);
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
    else if(t == STRLIT)
    {
        e = mkexprstr(strlbl);
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

        args = revargs(args);

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

    l = parse_expr_mul();
    t = peek();
    while(t == '+' || t == '-')
    {
        next();

             if(t == '+') { k = EXPR_ADD; }
        else if(t == '-') { k = EXPR_SUB; }
        else { assert(0); }

        r = parse_expr_mul();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr_rel()
{
    Expr *l;
    Expr *r;
    int t;
    int k;

    l = 0;
    r = 0;

    l = parse_expr_add();
    t = peek();
    while(t == '<' || t == LESSEQ || t == '>' || t == GRTREQ)
    {
        next();

             if(t == '<')    { k = EXPR_LESS; }
        else if(t == LESSEQ) { k = EXPR_LESSEQ; }
        else if(t == '>')    { k = EXPR_GRTR; }
        else if(t == GRTREQ) { k = EXPR_GRTREQ; }
        else { assert(0); }

        r = parse_expr_add();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr_eq()
{
    Expr *l;
    Expr *r;
    int t;
    int k;

    l = 0;
    r = 0;

    l = parse_expr_rel();
    t = peek();
    while(t == EQ || t == NEQ)
    {
        next();

             if(t == EQ)  { k = EXPR_EQ; }
        else if(t == NEQ) { k = EXPR_NEQ; }
        else { assert(0); }

        r = parse_expr_rel();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr_logic()
{
    Expr *l;
    Expr *r;
    int t;
    int k;

    l = 0;
    r = 0;

    l = parse_expr_eq();
    t = peek();
    while(t == AND || t == OR)
    {
        next();

             if(t == AND) { k = EXPR_AND; }
        else if(t == OR)  { k = EXPR_OR; }
        else { assert(0); }

        r = parse_expr_eq();
        l = mkexprbin(k, l, r);

        t = peek();
    }

    return(l);
}

Expr *
parse_expr()
{
    return(parse_expr_logic());
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
    if(type == type_void())
    {
        fprintf(stderr, "Invalid void type param\n");
        assert(0);
    }
    else if(type->kind == TYPE_ARR)
    {
        type = type_ptr(type->base);
    }
    sym->type = type;

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
        else if(type->width <= 0)
        {
            fprintf(stderr, "You cannot declare a void variable\n");
            assert(0);
        }

        sym->offset = currfunc->offset - (type->width - 1);
        currfunc->offset -= type->width;
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
    int haselse;
    int lbl1;
    int lbl2;
    Expr *args;
    Expr *arg;

    t = peek();
    if(t == IF)
    {
        lbl1 = newlbl();
        lbl2 = newlbl();

        next();
        expect('(');
        e = parse_expr();
        expect(')');
        lt = resolve_expr(e, type_char());

        fprintf(fout, " LDA .X CPI 0 BEQ .L%d\n", lbl1);
        parse_stmt();

        haselse = 0;
        t = peek();
        if(t == ELSE)
        {
            next();
            haselse = 1;
            fprintf(fout, " JPA .L%d\n", lbl2);
        }
        fprintf(fout, ".L%d:\n", lbl1);
        if(haselse)
        {
            parse_stmt();
            fprintf(fout, ".L%d:\n", lbl2);
        }
    }
    else if(t == WHILE)
    {
        lbl1 = newlbl();
        lbl2 = newlbl();

        next();
        expect('(');
        fprintf(fout, ".L%d:\n", lbl1);
        e = parse_expr();
        expect(')');
        lt = resolve_expr(e, type_char());

        fprintf(fout, " LDA .X CPI 0 BEQ .L%d\n", lbl2);
        parse_stmt();
        fprintf(fout, " JPA .L%d\n", lbl1);
        fprintf(fout, ".L%d:\n", lbl2);
    }
    else if(t == RETURN)
    {
        next();

        t = peek();
        if(t != ';')
        {
            e = parse_expr();
            lt = resolve_expr(e, currfunc->type);
        }
        else
        {
            lt = type_void();
        }
        expect(';');

        if(lt != currfunc->type)
        {
            fprintf(fout, "Type mismatch in return expressions\n");
            assert(0);
        }

        if(lt != type_void())
        {
            fprintf(fout, " LDA .BP.%s STA .T LDA .BP.%s+1 STA .T+1\n",
                currfunc->name, currfunc->name);
            fprintf(fout, " LDI %d ADW .T\n", currfunc->poffset);
            if(lt->width == 1)
            {
                fprintf(fout, " LDA .X STR .T\n");
            }
            else if(lt->width == 2)
            {
                fprintf(fout, " LDA .X STR .T INW .T LDA .X+1 STR .T\n");
            }
            else
            {
                /* TODO: HERE */
                assert(0);
            }
        }

        fprintf(fout, " PLS STA .X PLS STA .X+1\n");
        fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
        fprintf(fout, " LDA .BP.%s STA 0xffff\n", currfunc->name);
        fprintf(fout, " RTS\n");
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
                    if(t != ')')
                    {
                        expect(',');
                        t = peek();
                    }
                }

                args = revargs(args);
                arg = args;
                param = sym->params;
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
    int paramsz;
    Sym *tmp;
    SMemb *members;
    SMemb *memb;
    char buff[MAX_IDENT_LEN+1];
    Expr *e;

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
        fprintf(fout, " LDA .Y+1 PHS LDA .Y PHS\n");
        fprintf(fout, " LDA .X+1 PHS LDA .X PHS\n");

        paramsz = 0;
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

            paramsz += param->type->width;

            t = peek();
            if(t != ')')
            {
                expect(',');
                t = peek();
            }
        }
        expect(')');
            
        /* NOTE: Adjust params offsets */
        param = params;
        while(param)
        {
            param->offset = paramsz + 2 - (param->type->width - 1);
            paramsz -= param->type->width;

            tmp = addsym(param->name);
            tmp->kind = param->kind;
            tmp->type = param->type;
            tmp->offset = param->offset;

            param = param->next;
        }

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

        fprintf(fout, " PLS STA .X PLS STA .X+1\n");
        fprintf(fout, " PLS STA .Y PLS STA .Y+1\n");
        fprintf(fout, " LDA .BP.%s STA 0xffff\n", currfunc->name);
        fprintf(fout, " RTS\n");

        fprintf(fout, ".BP.%s: 0x00 0xff\n", currfunc->name);
        fprintf(fout, ".Z.%s: 0x00 0xff\n", currfunc->name);

        cut(currfunc);
    }
    else if(t == CONST)
    {
        expect(IDENT);
        sym = addsym(ident);
        sym->kind = SYM_CONST;
        expect('=');
        e = parse_expr();
        if(!isconstexpr(e))
        {
            fprintf(stderr, "Invalid const expr for const '%s'\n", sym->name);
            assert(0);
        }
        sym->val = evalexpr(e);
        expect(';');
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
    else if(t == TYPEDEF)
    {
        expect(IDENT);
        sym = addsym(ident);
        sym->kind = SYM_TYPE;
        expect(':');
        sym->type = parse_typespec();
        expect(';');
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

void
usage(char *exe)
{
    printf("USAGE:\n %s <source.min> [<output.bin>]\n", exe);
}

int
main(int argc, char *argv[])
{
    Sym *sym;
    Sym *param;
    int i;

    if(argc < 2)
    {
        usage(argv[0]);
        return(1);
    }

    fin = fopen(argv[1], "r");
    if(!fin)
    {
        fprintf(stderr, "Cannot open input file '%s' in read mode\n", argv[1]);
        return(2);
    }

    if(argc > 2)
    {
        fout = fopen(argv[2], "w");
        if(!fout)
        {
            fprintf(stderr, "Cannot open input file '%s' in write mode\n", argv[2]);
            return(2);
        }
    }
    else
    {
        fout = stdout;
    }

    fstr = fopen("test.str.asm", "w");
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
    fclose(fstr);

    fprintf(fout, "\n\n;+++ STRINGS +++\n\n");
    fstr = fopen("test.str.asm", "r");
    while((i = fgetc(fstr)) != EOF)
    {
        fputc(i, fout);
    }
    fclose(fstr);

    if(fout != stdout)
    {
        fclose(fout);
    }

    return(0);
}
