#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHASE "Semantic"
#define BUCKETS 211              /* hash table size (sections 3.3 to 3.5) */
#define MAX_NAME 31              /* longest identifier (section 2.1) */
#define MAX_PARAMS 8             /* FuncSymbol.params[8] */
#define MAX_MEMBERS 16           /* StructSymbol.members[16] */
#define MAX_ARRAY_SIZE 1000000   /* most elements in one array */
#define MAX_SCOPE_BYTES 16777216 /* most bytes of variables in one scope */
#define MAX_TREE_DEPTH 10000     /* deepest tree accepted from prog.ast */

#if defined(__GNUC__)
#define PRINTF_LIKE(a, b) __attribute__((format(printf, a, b)))
#else
#define PRINTF_LIKE(a, b)
#endif

static const char *inputPath = "";

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "semantic: out of memory\n");
        exit(2);
    }
    return p;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "semantic: out of memory\n");
        exit(2);
    }
    return p;
}

/* Grows a dynamic array of pointers by one slot and stores item in it. */
static void pushPointer(void ***items, int *count, int *capacity, void *item) {
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 64;
        *items = xrealloc(*items, (size_t)*capacity * sizeof(void *));
    }
    (*items)[(*count)++] = item;
}

/* The input file does not have the .ast format: stop with exit status 2. */
static void fatal(const char *fmt, ...) PRINTF_LIKE(1, 2);
static void fatal(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "semantic: '%s' is malformed: ", inputPath);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(2);
}

/* ==================== Errors and warnings (section 3.8) ==================== */

typedef struct {
    int  line, column;
    char phase[10];
    char msg[128];
} CompileError;

typedef struct {
    CompileError error;
    int isWarning;
    int sequence;      /* keeps messages on the same line in the order found */
} Diagnostic;

static Diagnostic *diags = NULL;
static int diagCount = 0;
static int diagCapacity = 0;
static int errorTotal = 0;
static int warningTotal = 0;

static void addDiagnostic(int isWarning, int line, const char *fmt, va_list args) {
    Diagnostic *d;
    if (diagCount == diagCapacity) {
        diagCapacity = diagCapacity ? diagCapacity * 2 : 32;
        diags = xrealloc(diags, (size_t)diagCapacity * sizeof(Diagnostic));
    }
    d = &diags[diagCount];
    d->isWarning = isWarning;
    d->sequence = diagCount;
    d->error.line = line;
    d->error.column = 0;      /* prog.ast keeps line numbers only */
    snprintf(d->error.phase, sizeof d->error.phase, "%s", PHASE);
    vsnprintf(d->error.msg, sizeof d->error.msg, fmt, args);
    diagCount++;
    if (isWarning) {
        warningTotal++;
    } else {
        errorTotal++;
    }
}

static void semError(int line, const char *fmt, ...) PRINTF_LIKE(2, 3);
static void semError(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    addDiagnostic(0, line, fmt, args);
    va_end(args);
}

static void semWarning(int line, const char *fmt, ...) PRINTF_LIKE(2, 3);
static void semWarning(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    addDiagnostic(1, line, fmt, args);
    va_end(args);
}

static int compareDiagnostics(const void *a, const void *b) {
    const Diagnostic *x = a, *y = b;
    if (x->error.line != y->error.line) {
        return x->error.line < y->error.line ? -1 : 1;
    }
    return x->sequence < y->sequence ? -1 : (x->sequence > y->sequence);
}

/* ==================== Abstract Syntax Tree (section 3.6) ==================== */

typedef enum {
    N_PROGRAM, N_DECL, N_ASSIGN, N_WHILE, N_IF, N_ELSE, N_BREAK,
    N_PRINT, N_READ, N_COND, N_BINOP, N_UNARY,
    N_IDENT, N_INDEX, N_INT, N_REAL, N_CHAR,
    N_FUNC, N_PARAM, N_RETURN, N_CALL,     /* functions */
    N_STRUCT, N_FIELD, N_MEMBER            /* structs */
} NodeType;

#define NODE_KIND_COUNT 24
static const char *const KIND_NAMES[NODE_KIND_COUNT] = {
    "PROGRAM", "DECL", "ASSIGN", "WHILE", "IF", "ELSE", "BREAK",
    "PRINT", "READ", "COND", "BINOP", "UNARY",
    "IDENT", "INDEX", "INT", "REAL", "CHAR",
    "FUNC", "PARAM", "RETURN", "CALL",
    "STRUCT", "FIELD", "MEMBER"
};

typedef struct ASTNode {
    NodeType  kind;
    char      value[128];      /* operator, name or literal text */
    char      dataType[8];     /* filled in by semantic analysis */
    int       line;
    struct ASTNode **children; /* ordered child list */
    int       childCount;
    struct ASTNode  *elseBody; /* used only by N_IF */
} ASTNode;

static ASTNode **allNodes = NULL;
static int nodeCount = 0;
static int nodeCapacity = 0;

static ASTNode *astNew(NodeType kind, const char *value, int line) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->kind = kind;
    snprintf(node->value, sizeof node->value, "%s", value);
    node->dataType[0] = '\0';
    node->line = line;
    node->children = NULL;
    node->childCount = 0;
    node->elseBody = NULL;
    pushPointer((void ***)&allNodes, &nodeCount, &nodeCapacity, node);
    return node;
}

static void astAddChild(ASTNode *parent, ASTNode *child) {
    parent->children = xrealloc(parent->children,
                                (size_t)(parent->childCount + 1) * sizeof(ASTNode *));
    parent->children[parent->childCount++] = child;
}

static void astFreeAll(void) {
    int i;
    for (i = 0; i < nodeCount; i++) {
        free(allNodes[i]->children);
        free(allNodes[i]);
    }
    free(allNodes);
    allNodes = NULL;
    nodeCount = nodeCapacity = 0;
}

/* ==================== Reading prog.ast (section 4.2) ==================== */

/* Reads one line of any length into *buffer. Returns 0 at end of file. */
static int readLine(FILE *f, char **buffer, size_t *capacity, size_t *length) {
    int c;
    size_t n = 0;

    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (n + 1 >= *capacity) {
            *capacity = *capacity ? *capacity * 2 : 256;
            *buffer = xrealloc(*buffer, *capacity);
        }
        (*buffer)[n++] = (char)c;
    }
    if (c == EOF && n == 0) {
        return 0;
    }
    if (*buffer == NULL) {
        *capacity = 256;
        *buffer = xmalloc(*capacity);
    }
    if (n > 0 && (*buffer)[n - 1] == '\r') {
        n--;
    }
    (*buffer)[n] = '\0';
    *length = n;
    return 1;
}

/* Removes a trailing "(line N)" from text and returns N, or 0 if absent. */
static int takeLineSuffix(char *text) {
    size_t len = strlen(text);
    char *found = NULL;
    char *p;
    size_t digitCount;

    if (len == 0 || text[len - 1] != ')') {
        return 0;
    }
    for (p = strstr(text, "(line "); p != NULL; p = strstr(p + 1, "(line ")) {
        found = p;
    }
    if (found == NULL || (found != text && found[-1] != ' ')) {
        return 0;
    }
    digitCount = (size_t)(text + len - 1 - (found + 6));
    if (digitCount == 0 || digitCount > 9 ||
        strspn(found + 6, "0123456789") != digitCount) {
        return 0;
    }
    len = (size_t)atoi(found + 6);
    if (len == 0) {
        return 0;
    }
    if (found != text) {
        found--;
    }
    *found = '\0';
    return (int)len;
}

static int kindFromName(const char *name) {
    int i;
    for (i = 0; i < NODE_KIND_COUNT; i++) {
        if (strcmp(KIND_NAMES[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Builds the tree from the indented text form: two spaces per level. A node
 * without "(line N)" takes the line number of its parent. */
static ASTNode *readTree(const char *path) {
    FILE *f = fopen(path, "r");
    ASTNode **stack = NULL;
    int stackCapacity = 0;
    int lastDepth = -1;
    ASTNode *root = NULL;
    char *buffer = NULL;
    size_t capacity = 0, length = 0;
    int fileLine = 0;

    if (f == NULL) {
        fprintf(stderr, "semantic: cannot open '%s'\n", path);
        exit(2);
    }

    while (readLine(f, &buffer, &capacity, &length)) {
        size_t indent;
        int depth, kind, line;
        char *text, *rest, *space;
        ASTNode *node;

        fileLine++;
        if (memchr(buffer, '\0', length) != NULL) {
            fatal("line %d of the file contains a NUL byte", fileLine);
        }
        if (length == 0) {
            continue;
        }
        indent = strspn(buffer, " ");
        if (indent % 2 != 0 || indent / 2 > MAX_TREE_DEPTH) {
            fatal("line %d of the file has a bad indentation", fileLine);
        }
        depth = (int)(indent / 2);
        if ((root == NULL && depth != 0) ||
            (root != NULL && (depth == 0 || depth > lastDepth + 1))) {
            fatal("line %d of the file has a bad indentation", fileLine);
        }

        text = buffer + indent;
        space = strchr(text, ' ');
        rest = "";
        if (space != NULL) {
            *space = '\0';
            rest = space + 1;
        }
        kind = kindFromName(text);
        if (kind < 0) {
            fatal("line %d of the file has the unknown node type '%.40s'",
                  fileLine, text);
        }
        line = takeLineSuffix(rest);
        if (strlen(rest) >= sizeof root->value) {
            fatal("line %d of the file has a value that is too long", fileLine);
        }
        if (line == 0) {
            line = depth > 0 ? stack[depth - 1]->line : 1;
        }

        node = astNew((NodeType)kind, rest, line);
        if (root == NULL) {
            if (kind != N_PROGRAM) {
                fatal("the tree must start with a PROGRAM node");
            }
            root = node;
        } else {
            ASTNode *parent = stack[depth - 1];
            if (kind == N_PROGRAM) {
                fatal("line %d of the file has a second PROGRAM node", fileLine);
            }
            if (kind == N_ELSE) {
                if (parent->kind != N_IF || parent->elseBody != NULL) {
                    fatal("line %d of the file has an ELSE outside an IF", fileLine);
                }
                parent->elseBody = node;
            } else {
                if (parent->elseBody != NULL) {
                    fatal("line %d of the file comes after the ELSE of its IF", fileLine);
                }
                astAddChild(parent, node);
            }
        }

        if (depth >= stackCapacity) {
            stackCapacity = stackCapacity ? stackCapacity * 2 : 64;
            if (stackCapacity <= depth) {
                stackCapacity = depth + 1;
            }
            stack = xrealloc(stack, (size_t)stackCapacity * sizeof(ASTNode *));
        }
        stack[depth] = node;
        lastDepth = depth;
    }

    if (ferror(f)) {
        fprintf(stderr, "semantic: cannot read '%s'\n", path);
        exit(2);
    }
    fclose(f);
    free(buffer);
    free(stack);
    if (root == NULL) {
        fatal("the file is empty");
    }
    return root;
}

/* ---------- shape checks for nodes read from the file ---------- */

static void expectChildren(const ASTNode *n, int minimum, int maximum) {
    if (n->childCount < minimum || n->childCount > maximum) {
        fatal("%s node near source line %d has %d children",
              KIND_NAMES[n->kind], n->line, n->childCount);
    }
}

/* Checks that text is an identifier: [a-zA-Z_][a-zA-Z0-9_]*, at most 31. */
static void checkName(const char *text, const ASTNode *n) {
    size_t i, len = strlen(text);
    int ok = len >= 1 && len <= MAX_NAME;

    for (i = 0; ok && i < len; i++) {
        char c = text[i];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        int digit = c >= '0' && c <= '9';
        ok = letter || (digit && i > 0);
    }
    if (!ok) {
        fatal("%s node near source line %d has the bad name '%.40s'",
              KIND_NAMES[n->kind], n->line, text);
    }
}

/* Splits text at single spaces into at most max words of < 128 characters.
 * Returns the number of words, or -1 if there are more than max. */
static int splitWords(const char *text, char words[][128], int max) {
    int count = 0;
    const char *start = text;

    for (;;) {
        const char *end = strchr(start, ' ');
        size_t n = end ? (size_t)(end - start) : strlen(start);
        if (count == max) {
            return -1;
        }
        memcpy(words[count], start, n);
        words[count][n] = '\0';
        count++;
        if (end == NULL) {
            return count;
        }
        start = end + 1;
    }
}

/* ==================== Types (section 2.3, rule 4) ==================== */

typedef enum {
    TY_INT16, TY_INT32, TY_UINT16, TY_UINT32, TY_REAL, TY_CHAR,
    TY_KHALI,     /* only the result of calling a khali function */
    TY_ERROR      /* an error was already reported for this expression */
} Ty;

static const char *const TY_NAMES[] = {
    "int16", "int32", "uint16", "uint32", "real", "char", "khali", "error"
};

/* Returns the scalar type with this name, or -1. */
static int scalarFromName(const char *name) {
    int i;
    for (i = TY_INT16; i <= TY_CHAR; i++) {
        if (strcmp(TY_NAMES[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static Ty scalarOrError(const char *name) {
    int t = scalarFromName(name);
    return t < 0 ? TY_ERROR : (Ty)t;
}

static int isIntegerType(Ty t) {
    return t == TY_INT16 || t == TY_INT32 || t == TY_UINT16 ||
           t == TY_UINT32 || t == TY_CHAR;
}

static int isUnsignedType(Ty t) {
    return t == TY_UINT16 || t == TY_UINT32;
}

static int typeBytes(Ty t) {
    switch (t) {
        case TY_CHAR:   return 1;
        case TY_INT16:
        case TY_UINT16: return 2;
        default:        return 4;
    }
}

/* Result type of an arithmetic operation. Integer types give the wider type;
 * for two types of the same width, the unsigned one wins. Any real gives real. */
static Ty widerType(Ty a, Ty b) {
    if (a == TY_ERROR || b == TY_ERROR) return TY_ERROR;
    if (a == TY_REAL || b == TY_REAL)   return TY_REAL;
    if (typeBytes(a) != typeBytes(b))   return typeBytes(a) > typeBytes(b) ? a : b;
    return isUnsignedType(a) ? a : b;
}

static void typeRange(Ty t, long long *low, long long *high) {
    switch (t) {
        case TY_INT16:  *low = -32768;          *high = 32767;        break;
        case TY_UINT16: *low = 0;               *high = 65535;        break;
        case TY_UINT32: *low = 0;               *high = 4294967295LL; break;
        case TY_CHAR:   *low = 0;               *high = 255;          break;
        default:        *low = -2147483648LL;   *high = 2147483647LL; break;
    }
}

static int alignUp(int value, int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

/* ==================== Tables (sections 3.3 to 3.5) ==================== */

typedef enum { K_VARIABLE, K_ARRAY, K_PARAM, K_STRUCTVAR } SymKind;
static const char *const SYMKIND_NAMES[] = { "VARIABLE", "ARRAY", "PARAM", "STRUCTVAR" };

typedef struct Symbol {
    char    name[32];
    char    type[32];     /* int16, int32, uint16, uint32, real, char, or a struct name */
    SymKind kind;
    int     arraySize;    /* 0 for scalars */
    int     sizeBytes;
    int     offset;       /* global: data segment offset; local: frame offset */
    char    scope[32];    /* "global" or the function name */
    int     declLine;
    struct Symbol *next;  /* chaining */
} Symbol;

typedef struct { Symbol *buckets[BUCKETS]; } SymbolTable;

typedef struct { char name[32]; char type[8]; } Param;

typedef struct FuncSymbol {
    char  name[32];
    char  returnType[8];  /* a type name or "khali" */
    int   paramCount;
    Param params[MAX_PARAMS];
    int   declLine;
    struct FuncSymbol *next;
} FuncSymbol;

typedef struct { FuncSymbol *buckets[BUCKETS]; } FuncTable;

typedef struct { char name[32]; char type[8]; int sizeBytes; int offset; } Member;

typedef struct StructSymbol {
    char   name[32];
    int    memberCount;
    Member members[MAX_MEMBERS];
    int    totalBytes;
    int    declLine;
    struct StructSymbol *next;
} StructSymbol;

typedef struct { StructSymbol *buckets[BUCKETS]; } StructTable;

/* hashpjw (Aho, Sethi and Ullman), used with a prime table size of 211. */
static unsigned hashName(const char *name) {
    unsigned h = 0, g;
    for (; *name != '\0'; name++) {
        h = (h << 4) + (unsigned char)*name;
        g = h & 0xF0000000u;
        if (g != 0) {
            h ^= g >> 24;
            h ^= g;
        }
    }
    return h % BUCKETS;
}

static SymbolTable globalTable;
static SymbolTable localTable;    /* cleared for each function */
static FuncTable   funcTable;
static StructTable structTable;

/* Everything in declaration order, for prog.sym. */
static Symbol       **symbolRows = NULL;
static int            symbolCount = 0, symbolCapacity = 0;
static FuncSymbol   **funcRows = NULL;
static int            funcCount = 0, funcCapacity = 0;
static StructSymbol **structRows = NULL;
static int            structCount = 0, structCapacity = 0;

/* Analysis state. */
static int inFunction = 0;
static const FuncSymbol *currentFunction = NULL;
static int loopDepth = 0;
static int globalOffset = 0, frameOffset = 0;
static int globalTooBig = 0, frameTooBig = 0;

static Symbol *symLookupIn(const SymbolTable *table, const char *name) {
    Symbol *s;
    for (s = table->buckets[hashName(name)]; s != NULL; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* Rule 1: a local variable or parameter hides a global of the same name. */
static Symbol *lookupVariable(const char *name) {
    Symbol *s = inFunction ? symLookupIn(&localTable, name) : NULL;
    return s != NULL ? s : symLookupIn(&globalTable, name);
}

static FuncSymbol *funcLookup(const char *name) {
    FuncSymbol *f;
    for (f = funcTable.buckets[hashName(name)]; f != NULL; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

static StructSymbol *structLookup(const char *name) {
    StructSymbol *s;
    for (s = structTable.buckets[hashName(name)]; s != NULL; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static int structAlignment(const StructSymbol *s) {
    int i, alignment = 1;
    for (i = 0; i < s->memberCount; i++) {
        if (s->members[i].sizeBytes > alignment) {
            alignment = s->members[i].sizeBytes;
        }
    }
    return alignment;
}

/* Gives a variable its offset in the data segment or the function frame. */
static int allocate(int bytes, int alignment, int line) {
    int *cursor = inFunction ? &frameOffset : &globalOffset;
    int *reported = inFunction ? &frameTooBig : &globalTooBig;
    int offset = alignUp(*cursor, alignment);

    *cursor = offset + bytes;
    if (*cursor > MAX_SCOPE_BYTES) {
        if (!*reported) {
            if (inFunction) {
                semError(line, "the variables of '%s' need more than %d bytes",
                         currentFunction->name, MAX_SCOPE_BYTES);
            } else {
                semError(line, "the global variables need more than %d bytes",
                         MAX_SCOPE_BYTES);
            }
        }
        *reported = 1;
        *cursor = MAX_SCOPE_BYTES;
    }
    return offset;
}

/* insert(name, ...): fails on a redeclaration in the same scope (rule 1). */
static void declareVariable(const char *name, const char *type, SymKind kind,
                            int arraySize, int bytes, int alignment, int line) {
    SymbolTable *table = inFunction ? &localTable : &globalTable;
    Symbol *old = symLookupIn(table, name);
    Symbol *s;
    unsigned h;

    if (old != NULL) {
        semError(line, "'%s' is already declared in this scope (on line %d)",
                 name, old->declLine);
        return;
    }
    s = xmalloc(sizeof(Symbol));
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->type, sizeof s->type, "%s", type);
    s->kind = kind;
    s->arraySize = arraySize;
    s->sizeBytes = bytes;
    s->offset = allocate(bytes, alignment, line);
    snprintf(s->scope, sizeof s->scope, "%s",
             inFunction ? currentFunction->name : "global");
    s->declLine = line;

    h = hashName(name);
    s->next = table->buckets[h];
    table->buckets[h] = s;
    pushPointer((void ***)&symbolRows, &symbolCount, &symbolCapacity, s);
}

/* ==================== Expressions ==================== */

typedef enum { USE_VALUE, USE_ASSIGN, USE_READ } Use;
static const char *const USE_TEXT[] = { "used as a value", "assigned", "read" };

static Ty analyzeExpr(ASTNode *n);
static Ty analyzeCall(ASTNode *n);

/* Value of an INT literal, or -1 if uint32 cannot hold it. */
static long long intLiteralValue(const ASTNode *n) {
    const char *p = n->value;
    long long v = 0;

    if (p[0] == '\0' || strspn(p, "0123456789") != strlen(p)) {
        fatal("INT node near source line %d has the bad value '%.40s'", n->line, p);
    }
    for (; *p != '\0'; p++) {
        v = v * 10 + (*p - '0');
        if (v > 4294967295LL) {
            return -1;
        }
    }
    return v;
}

/* Value of a literal, or of '-' applied to a literal. Returns 1 for an
 * integer constant (in *iv), 2 for a real constant (in *rv), 0 otherwise. */
static int constValue(const ASTNode *n, long long *iv, double *rv) {
    int k;
    switch (n->kind) {
        case N_INT:
            *iv = intLiteralValue(n);
            return *iv >= 0;
        case N_CHAR:
            *iv = (unsigned char)n->value[1];
            return 1;
        case N_REAL:
            *rv = strtod(n->value, NULL);
            return 2;
        case N_UNARY:
            if (n->childCount != 1) {
                return 0;
            }
            k = constValue(n->children[0], iv, rv);
            if (k == 1) *iv = -*iv;
            if (k == 2) *rv = -*rv;
            return k;
        default:
            return 0;
    }
}

static void reportUndeclared(const char *name, int line) {
    if (funcLookup(name) != NULL) {
        semError(line, "'%s' is a function, not a variable; call it as %s(...)",
                 name, name);
    } else {
        semError(line, "'%s' is not declared", name);
    }
}

/* IDENT, INDEX or MEMBER (lvalue in the grammar). Rules 2 and 8. */
static Ty analyzeLvalue(ASTNode *n, Use use) {
    Symbol *s;
    long long iv;
    double rv;

    switch (n->kind) {
        case N_IDENT:
            expectChildren(n, 0, 0);
            checkName(n->value, n);
            s = lookupVariable(n->value);
            if (s == NULL) {
                reportUndeclared(n->value, n->line);
                return TY_ERROR;
            }
            if (s->kind == K_ARRAY) {
                semError(n->line, "the whole array '%s' cannot be %s; use an element such as %s[0]",
                         n->value, USE_TEXT[use], n->value);
                return TY_ERROR;
            }
            if (s->kind == K_STRUCTVAR) {
                semError(n->line, "the whole struct '%s' cannot be %s; use its members instead",
                         n->value, USE_TEXT[use]);
                return TY_ERROR;
            }
            return scalarOrError(s->type);

        case N_INDEX: {
            Ty indexType;
            expectChildren(n, 1, 1);
            checkName(n->value, n);
            s = lookupVariable(n->value);
            if (s == NULL) {
                reportUndeclared(n->value, n->line);
            } else if (s->kind != K_ARRAY) {
                semError(n->line, "'%s' is not an array, so it cannot be indexed", n->value);
            }
            indexType = analyzeExpr(n->children[0]);
            if (indexType != TY_ERROR && !isIntegerType(indexType)) {
                semError(n->line, "the index of '%s' must be an integer, not %s",
                         n->value, TY_NAMES[indexType]);
                return TY_ERROR;
            }
            if (s == NULL || s->kind != K_ARRAY) {
                return TY_ERROR;
            }
            if (s->arraySize > 0 && indexType != TY_ERROR &&
                constValue(n->children[0], &iv, &rv) == 1 &&
                (iv < 0 || iv >= s->arraySize)) {
                semError(n->line, "index %lld is out of range for '%s' (valid indexes are 0 to %d)",
                         iv, n->value, s->arraySize - 1);
            }
            return scalarOrError(s->type);
        }

        case N_MEMBER: {
            char variable[128];
            const char *field;
            const char *dot = strchr(n->value, '.');
            StructSymbol *st;
            int i;

            expectChildren(n, 0, 0);
            if (dot == NULL) {
                fatal("MEMBER node near source line %d has no '.'", n->line);
            }
            memcpy(variable, n->value, (size_t)(dot - n->value));
            variable[dot - n->value] = '\0';
            field = dot + 1;
            checkName(variable, n);
            checkName(field, n);

            s = lookupVariable(variable);
            if (s == NULL) {
                reportUndeclared(variable, n->line);
                return TY_ERROR;
            }
            if (s->kind != K_STRUCTVAR) {
                semError(n->line, "'%s' is not a struct variable, so '.%s' cannot be used",
                         variable, field);
                return TY_ERROR;
            }
            st = structLookup(s->type);
            if (st == NULL) {
                return TY_ERROR;   /* the undefined struct was already reported */
            }
            for (i = 0; i < st->memberCount; i++) {
                if (strcmp(st->members[i].name, field) == 0) {
                    return scalarOrError(st->members[i].type);
                }
            }
            semError(n->line, "struct '%s' has no member '%s'", st->name, field);
            return TY_ERROR;
        }

        default:
            fatal("%s node near source line %d is not a variable",
                  KIND_NAMES[n->kind], n->line);
    }
    return TY_ERROR;
}

static Ty analyzeExprBody(ASTNode *n) {
    long long iv;
    double rv;

    switch (n->kind) {
        case N_INT: {
            long long v;
            expectChildren(n, 0, 0);
            v = intLiteralValue(n);
            if (v < 0) {
                semError(n->line, "integer literal %.40s is too large (the largest is 4294967295)",
                         n->value);
                return TY_ERROR;
            }
            return v <= 2147483647LL ? TY_INT32 : TY_UINT32;
        }

        case N_REAL: {
            const char *p = n->value;
            size_t whole = strspn(p, "0123456789");
            expectChildren(n, 0, 0);
            if (whole == 0 || p[whole] != '.' || strlen(p + whole + 1) == 0 ||
                strspn(p + whole + 1, "0123456789") != strlen(p + whole + 1)) {
                fatal("REAL node near source line %d has the bad value '%.40s'", n->line, p);
            }
            return TY_REAL;
        }

        case N_CHAR:
            expectChildren(n, 0, 0);
            if (strlen(n->value) != 3 || n->value[0] != '\'' || n->value[2] != '\'' ||
                n->value[1] < 0x20 || n->value[1] > 0x7E) {
                fatal("CHAR node near source line %d has the bad value '%.40s'",
                      n->line, n->value);
            }
            return TY_CHAR;

        case N_IDENT:
        case N_INDEX:
        case N_MEMBER:
            return analyzeLvalue(n, USE_VALUE);

        case N_UNARY: {
            Ty t;
            expectChildren(n, 1, 1);
            if (strcmp(n->value, "-") != 0) {
                fatal("UNARY node near source line %d has the operator '%.40s'",
                      n->line, n->value);
            }
            t = analyzeExpr(n->children[0]);
            /* -2147483648 is an int32, although 2147483648 alone is a uint32. */
            if (t == TY_UINT32 && n->children[0]->kind == N_INT &&
                intLiteralValue(n->children[0]) == 2147483648LL) {
                return TY_INT32;
            }
            return t;
        }

        case N_BINOP: {
            Ty left, right;
            const char *op = n->value;
            expectChildren(n, 2, 2);
            if (strlen(op) != 1 || strchr("+-*/%", op[0]) == NULL) {
                fatal("BINOP node near source line %d has the operator '%.40s'", n->line, op);
            }
            left = analyzeExpr(n->children[0]);
            right = analyzeExpr(n->children[1]);
            if (left == TY_ERROR || right == TY_ERROR) {
                return TY_ERROR;
            }
            if (op[0] == '%' && (left == TY_REAL || right == TY_REAL)) {
                semError(n->line, "'%%' can only be used with integer types, but an operand is real");
                return TY_ERROR;
            }
            if (op[0] == '/' || op[0] == '%') {
                int k = constValue(n->children[1], &iv, &rv);
                if ((k == 1 && iv == 0) || (k == 2 && rv == 0.0)) {
                    semWarning(n->line, "division by zero");
                }
            }
            return widerType(left, right);
        }

        case N_CALL: {
            Ty t = analyzeCall(n);
            if (t == TY_KHALI) {
                semError(n->line, "'%s' is a 'khali' function, so its call cannot be used as a value",
                         n->value);
                return TY_ERROR;
            }
            return t;
        }

        default:
            fatal("%s node near source line %d cannot be part of an expression",
                  KIND_NAMES[n->kind], n->line);
    }
    return TY_ERROR;
}

/* Analyzes an expression and records its type in the node (section 3.6). */
static Ty analyzeExpr(ASTNode *n) {
    Ty t = analyzeExprBody(n);
    snprintf(n->dataType, sizeof n->dataType, "%s", TY_NAMES[t]);
    return t;
}

/* Checks a value stored into a target of type target: an assignment, an
 * initialisation, an argument or a return value (rules 5, 6 and 7). */
static void checkAssign(Ty target, ASTNode *value, const char *what, int line) {
    Ty source = analyzeExpr(value);
    long long iv = 0, low, high;
    double rv = 0.0;
    int k;

    if (target == TY_ERROR || source == TY_ERROR) {
        return;
    }
    k = constValue(value, &iv, &rv);
    if (isUnsignedType(target) && ((k == 1 && iv < 0) || (k == 2 && rv < 0.0))) {
        semError(line, "a negative value cannot be assigned to %s (%s)",
                 what, TY_NAMES[target]);
        return;
    }
    if (isIntegerType(target) && source == TY_REAL) {
        semWarning(line, "the real value assigned to %s is truncated to %s",
                   what, TY_NAMES[target]);
        return;
    }
    if (isIntegerType(target) && k == 1) {
        typeRange(target, &low, &high);
        if (iv < low || iv > high) {
            semWarning(line, "the value %lld does not fit in %s (%s) and will be truncated",
                       iv, what, TY_NAMES[target]);
        }
    }
}

/* Text for an assignment target in a message. */
static void describeTarget(const ASTNode *n, char *buffer, size_t size) {
    if (n->kind == N_INDEX) {
        snprintf(buffer, size, "an element of '%s'", n->value);
    } else {
        snprintf(buffer, size, "'%s'", n->value);
    }
}

/* A call used as a statement or inside an expression (rule 7). Returns the
 * return type, TY_KHALI for a khali function, or TY_ERROR. */
static Ty analyzeCall(ASTNode *n) {
    FuncSymbol *f;
    int i;

    checkName(n->value, n);
    f = funcLookup(n->value);
    if (f == NULL) {
        if (lookupVariable(n->value) != NULL) {
            semError(n->line, "'%s' is a variable, not a function", n->value);
        } else {
            semError(n->line, "function '%s' is not defined (a function must be defined before it is called)",
                     n->value);
        }
        for (i = 0; i < n->childCount; i++) {
            analyzeExpr(n->children[i]);
        }
        return TY_ERROR;
    }

    if (n->childCount != f->paramCount) {
        semError(n->line, "'%s' expects %d argument%s but %d %s given",
                 f->name, f->paramCount, f->paramCount == 1 ? "" : "s",
                 n->childCount, n->childCount == 1 ? "was" : "were");
        for (i = 0; i < n->childCount; i++) {
            analyzeExpr(n->children[i]);
        }
    } else {
        for (i = 0; i < n->childCount; i++) {
            if (i < MAX_PARAMS) {
                char what[96];
                snprintf(what, sizeof what, "argument %d of '%s'", i + 1, f->name);
                checkAssign(scalarOrError(f->params[i].type), n->children[i], what, n->line);
            } else {
                analyzeExpr(n->children[i]);
            }
        }
    }
    return strcmp(f->returnType, "khali") == 0 ? TY_KHALI : scalarOrError(f->returnType);
}

/* condition ::= expression relop expression */
static void analyzeCondition(ASTNode *n) {
    static const char *const relops[] = { "=", "!=", "<", ">", "<=", ">=" };
    size_t i;
    int known = 0;

    if (n->kind != N_COND) {
        fatal("%s node near source line %d is not a condition", KIND_NAMES[n->kind], n->line);
    }
    expectChildren(n, 2, 2);
    for (i = 0; i < sizeof relops / sizeof relops[0]; i++) {
        known |= strcmp(n->value, relops[i]) == 0;
    }
    if (!known) {
        fatal("COND node near source line %d has the operator '%.40s'", n->line, n->value);
    }
    analyzeExpr(n->children[0]);
    analyzeExpr(n->children[1]);
}

/* ==================== Statements and declarations ==================== */

static void analyzeStatement(ASTNode *n);
static void analyzeDeclaration(ASTNode *n);

static int isLvalueNode(const ASTNode *n) {
    return n->kind == N_IDENT || n->kind == N_INDEX || n->kind == N_MEMBER;
}

/* Analyzes parent's children from start. Declarations are allowed only at
 * top level and directly in a function body. */
static void analyzeItems(ASTNode *parent, int start, int allowDeclarations) {
    int i;
    for (i = start; i < parent->childCount; i++) {
        ASTNode *c = parent->children[i];
        if (c->kind == N_DECL && allowDeclarations) {
            analyzeDeclaration(c);
        } else {
            analyzeStatement(c);
        }
    }
}

static void analyzeStatement(ASTNode *n) {
    char what[160];

    switch (n->kind) {
        case N_ASSIGN: {
            Ty target;
            expectChildren(n, 2, 2);
            if (!isLvalueNode(n->children[0])) {
                fatal("ASSIGN node near source line %d has no variable to assign", n->line);
            }
            target = analyzeLvalue(n->children[0], USE_ASSIGN);
            describeTarget(n->children[0], what, sizeof what);
            checkAssign(target, n->children[1], what, n->line);
            break;
        }

        case N_WHILE:
            expectChildren(n, 1, n->childCount);
            analyzeCondition(n->children[0]);
            loopDepth++;
            analyzeItems(n, 1, 0);
            loopDepth--;
            break;

        case N_IF:
            expectChildren(n, 1, n->childCount);
            analyzeCondition(n->children[0]);
            analyzeItems(n, 1, 0);
            if (n->elseBody != NULL) {
                analyzeItems(n->elseBody, 0, 0);
            }
            break;

        case N_BREAK:   /* rule 3 */
            expectChildren(n, 0, 0);
            if (loopDepth == 0) {
                semError(n->line, "'nikal' can only be used inside a 'jabtak' loop");
            }
            break;

        case N_PRINT:
            expectChildren(n, 1, 1);
            analyzeExpr(n->children[0]);
            break;

        case N_READ:
            expectChildren(n, 1, 1);
            if (!isLvalueNode(n->children[0])) {
                fatal("READ node near source line %d has no variable to read", n->line);
            }
            analyzeLvalue(n->children[0], USE_READ);
            break;

        case N_RETURN:  /* rule 7 */
            expectChildren(n, 0, 1);
            if (currentFunction == NULL) {
                semError(n->line, "'wapas' can only be used inside a function");
                if (n->childCount == 1) {
                    analyzeExpr(n->children[0]);
                }
            } else if (strcmp(currentFunction->returnType, "khali") == 0) {
                if (n->childCount == 1) {
                    semError(n->line, "'%s' is a 'khali' function, so 'wapas' cannot return a value",
                             currentFunction->name);
                    analyzeExpr(n->children[0]);
                }
            } else if (n->childCount == 0) {
                semError(n->line, "'%s' must return a value of type %s with 'wapas'",
                         currentFunction->name, currentFunction->returnType);
            } else {
                snprintf(what, sizeof what, "the return value of '%s'", currentFunction->name);
                checkAssign(scalarOrError(currentFunction->returnType),
                            n->children[0], what, n->line);
            }
            break;

        case N_CALL:
            analyzeCall(n);
            break;

        default:
            fatal("%s node near source line %d is not allowed here",
                  KIND_NAMES[n->kind], n->line);
    }
}

/* DECL "type name", "type name[size]" or "struct Name name" (rules 1, 2, 6, 8). */
static void analyzeDeclaration(ASTNode *n) {
    char words[3][128];
    int count = splitWords(n->value, words, 3);
    int t;
    char *bracket;

    if (count == 3 && strcmp(words[0], "struct") == 0) {
        StructSymbol *st;
        expectChildren(n, 0, 0);
        checkName(words[1], n);
        checkName(words[2], n);
        st = structLookup(words[1]);
        if (st == NULL) {
            semError(n->line, "struct '%s' is not defined (a struct must be defined before its variables)",
                     words[1]);
            declareVariable(words[2], words[1], K_STRUCTVAR, 0, 0, 1, n->line);
        } else {
            declareVariable(words[2], st->name, K_STRUCTVAR, 0, st->totalBytes,
                            structAlignment(st), n->line);
        }
        return;
    }

    if (count != 2 || (t = scalarFromName(words[0])) < 0) {
        fatal("DECL node near source line %d has the bad value '%.60s'", n->line, n->value);
    }

    bracket = strchr(words[1], '[');
    if (bracket != NULL) {
        char *sizeText = bracket + 1;
        size_t len = strlen(sizeText);
        long long size = 0;
        size_t i;

        expectChildren(n, 0, 0);
        if (len < 2 || sizeText[len - 1] != ']' ||
            strspn(sizeText, "0123456789") != len - 1) {
            fatal("DECL node near source line %d has the bad value '%.60s'", n->line, n->value);
        }
        *bracket = '\0';
        checkName(words[1], n);
        for (i = 0; i + 1 < len && size <= MAX_ARRAY_SIZE; i++) {
            size = size * 10 + (sizeText[i] - '0');
        }
        if (size > MAX_ARRAY_SIZE) {
            semError(n->line, "array '%s' is too large (the limit is %d elements)",
                     words[1], MAX_ARRAY_SIZE);
            size = 0;
        } else if (size == 0) {
            semError(n->line, "the size of array '%s' must be a positive integer", words[1]);
        }
        declareVariable(words[1], words[0], K_ARRAY, (int)size,
                        (int)size * typeBytes((Ty)t), typeBytes((Ty)t), n->line);
        return;
    }

    expectChildren(n, 0, 1);
    checkName(words[1], n);
    if (n->childCount == 1) {
        /* The initialiser is checked before the name exists (rule 6). */
        char what[64];
        snprintf(what, sizeof what, "'%.31s'", words[1]);
        checkAssign((Ty)t, n->children[0], what, n->line);
    }
    declareVariable(words[1], words[0], K_VARIABLE, 0, typeBytes((Ty)t),
                    typeBytes((Ty)t), n->line);
}

/* struct_def (rule 8). Members are aligned to their own size and the total
 * is rounded up to the largest member alignment (section 3.5). */
static void analyzeStruct(ASTNode *n) {
    StructSymbol layout;
    StructSymbol *st = &layout;
    StructSymbol *old;
    int i, j, offset = 0, alignment = 1, tooMany = 0;
    unsigned h;

    checkName(n->value, n);
    memset(st, 0, sizeof *st);
    snprintf(st->name, sizeof st->name, "%.31s", n->value);
    st->declLine = n->line;

    for (i = 0; i < n->childCount; i++) {
        ASTNode *c = n->children[i];
        char words[2][128];
        int t, duplicate = 0;

        if (c->kind != N_FIELD) {
            fatal("%s node near source line %d is not allowed in a struct",
                  KIND_NAMES[c->kind], c->line);
        }
        expectChildren(c, 0, 0);
        if (splitWords(c->value, words, 2) != 2 || (t = scalarFromName(words[0])) < 0) {
            fatal("FIELD node near source line %d has the bad value '%.60s'", c->line, c->value);
        }
        checkName(words[1], c);

        for (j = 0; j < st->memberCount; j++) {
            duplicate |= strcmp(st->members[j].name, words[1]) == 0;
        }
        if (duplicate) {
            semError(c->line, "struct '%s' already has a member named '%s'", st->name, words[1]);
            continue;
        }
        if (st->memberCount == MAX_MEMBERS) {
            if (!tooMany) {
                semError(c->line, "struct '%s' has more than %d members", st->name, MAX_MEMBERS);
            }
            tooMany = 1;
            continue;
        }
        offset = alignUp(offset, typeBytes((Ty)t));
        snprintf(st->members[st->memberCount].name, sizeof st->members[0].name, "%.31s", words[1]);
        snprintf(st->members[st->memberCount].type, sizeof st->members[0].type, "%.7s", words[0]);
        st->members[st->memberCount].sizeBytes = typeBytes((Ty)t);
        st->members[st->memberCount].offset = offset;
        st->memberCount++;
        offset += typeBytes((Ty)t);
        if (typeBytes((Ty)t) > alignment) {
            alignment = typeBytes((Ty)t);
        }
    }
    st->totalBytes = alignUp(offset, alignment);
    if (st->memberCount == 0) {
        semError(n->line, "struct '%s' has no members", st->name);
    }

    old = structLookup(st->name);
    if (old != NULL) {
        semError(n->line, "struct '%s' is already defined (on line %d)", st->name, old->declLine);
        return;
    }
    st = xmalloc(sizeof(StructSymbol));
    *st = layout;
    h = hashName(st->name);
    st->next = structTable.buckets[h];
    structTable.buckets[h] = st;
    pushPointer((void ***)&structRows, &structCount, &structCapacity, st);
}

/* True if every path through the statements from start reaches a 'wapas'. */
static int alwaysReturns(const ASTNode *parent, int start) {
    int i;
    for (i = start; i < parent->childCount; i++) {
        const ASTNode *c = parent->children[i];
        if (c->kind == N_RETURN) {
            return 1;
        }
        if (c->kind == N_IF && c->elseBody != NULL &&
            alwaysReturns(c, 1) && alwaysReturns(c->elseBody, 0)) {
            return 1;
        }
    }
    return 0;
}

/* function_def (rule 7). The function is entered in the table before its
 * body is checked, so that it can call itself. */
static void analyzeFunction(ASTNode *n) {
    char words[3][128];
    FuncSymbol *f;
    FuncSymbol *old;
    int i, firstBody = 0, registered = 0;

    if (splitWords(n->value, words, 3) != 2 ||
        (strcmp(words[0], "khali") != 0 && scalarFromName(words[0]) < 0)) {
        fatal("FUNC node near source line %d has the bad value '%.60s'", n->line, n->value);
    }
    checkName(words[1], n);

    f = xmalloc(sizeof(FuncSymbol));
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "%.31s", words[1]);
    snprintf(f->returnType, sizeof f->returnType, "%.7s", words[0]);
    f->declLine = n->line;

    while (firstBody < n->childCount && n->children[firstBody]->kind == N_PARAM) {
        ASTNode *p = n->children[firstBody];
        char pw[2][128];
        expectChildren(p, 0, 0);
        if (splitWords(p->value, pw, 2) != 2 || scalarFromName(pw[0]) < 0) {
            fatal("PARAM node near source line %d has the bad value '%.60s'", p->line, p->value);
        }
        checkName(pw[1], p);
        if (f->paramCount < MAX_PARAMS) {
            snprintf(f->params[f->paramCount].name, sizeof f->params[0].name, "%.31s", pw[1]);
            snprintf(f->params[f->paramCount].type, sizeof f->params[0].type, "%.7s", pw[0]);
        }
        f->paramCount++;
        firstBody++;
    }
    if (f->paramCount > MAX_PARAMS) {
        semError(n->line, "'%s' has %d parameters (the limit is %d)",
                 f->name, f->paramCount, MAX_PARAMS);
    }

    old = funcLookup(f->name);
    if (old != NULL) {
        semError(n->line, "function '%s' is already defined (on line %d)", f->name, old->declLine);
    } else {
        unsigned h = hashName(f->name);
        f->next = funcTable.buckets[h];
        funcTable.buckets[h] = f;
        pushPointer((void ***)&funcRows, &funcCount, &funcCapacity, f);
        registered = 1;
    }

    /* A new local scope: the parameters first, then the body. */
    inFunction = 1;
    currentFunction = f;
    memset(&localTable, 0, sizeof localTable);
    frameOffset = 0;
    frameTooBig = 0;
    loopDepth = 0;

    for (i = 0; i < firstBody; i++) {
        char pw[2][128];
        int t;
        splitWords(n->children[i]->value, pw, 2);
        t = scalarFromName(pw[0]);
        declareVariable(pw[1], pw[0], K_PARAM, 0, typeBytes((Ty)t), typeBytes((Ty)t), n->line);
    }
    analyzeItems(n, firstBody, 1);

    if (strcmp(f->returnType, "khali") != 0 && !alwaysReturns(n, firstBody)) {
        semWarning(n->line, "'%s' can reach its end without 'wapas'", f->name);
    }

    inFunction = 0;
    currentFunction = NULL;
    loopDepth = 0;
    if (!registered) {
        free(f);
    }
}

static void analyzeProgram(ASTNode *root) {
    int i;
    for (i = 0; i < root->childCount; i++) {
        ASTNode *c = root->children[i];
        if (c->kind == N_STRUCT) {
            analyzeStruct(c);
        } else if (c->kind == N_FUNC) {
            analyzeFunction(c);
        } else if (c->kind == N_DECL) {
            analyzeDeclaration(c);
        } else {
            analyzeStatement(c);
        }
    }
}

/* ==================== Writing prog.sym (section 4.3) ==================== */

/* Column width: the longest entry plus two spaces, but at least minimum. */
static int widen(int width, size_t length) {
    return (int)length + 2 > width ? (int)length + 2 : width;
}

static size_t numberLength(int v) {
    char text[16];
    return (size_t)snprintf(text, sizeof text, "%d", v);
}

static void writeSymbols(FILE *out) {
    int wName = 10, wType = 8, wKind = 10, wSize = 6, wBytes = 7, wOffset = 8, wScope = 8;
    int pass, i;

    for (i = 0; i < symbolCount; i++) {
        const Symbol *s = symbolRows[i];
        wName = widen(wName, strlen(s->name));
        wType = widen(wType, strlen(s->type));
        wKind = widen(wKind, strlen(SYMKIND_NAMES[s->kind]));
        wSize = widen(wSize, numberLength(s->arraySize));
        wBytes = widen(wBytes, numberLength(s->sizeBytes));
        wOffset = widen(wOffset, numberLength(s->offset));
        wScope = widen(wScope, strlen(s->scope));
    }
    fprintf(out, "%-*s%-*s%-*s%-*s%-*s%-*s%-*s%s\n", wName, "NAME", wType, "TYPE",
            wKind, "KIND", wSize, "SIZE", wBytes, "BYTES", wOffset, "OFFSET",
            wScope, "SCOPE", "LINE");

    /* Globals first, then the locals and parameters of each function. */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < symbolCount; i++) {
            const Symbol *s = symbolRows[i];
            if ((strcmp(s->scope, "global") == 0) != (pass == 0)) {
                continue;
            }
            fprintf(out, "%-*s%-*s%-*s%-*d%-*d%-*d%-*s%d\n", wName, s->name, wType, s->type,
                    wKind, SYMKIND_NAMES[s->kind], wSize, s->arraySize, wBytes, s->sizeBytes,
                    wOffset, s->offset, wScope, s->scope, s->declLine);
        }
    }
    fprintf(out, "--\n");
}

static void paramText(const FuncSymbol *f, char *buffer, size_t size) {
    int i, shown = f->paramCount < MAX_PARAMS ? f->paramCount : MAX_PARAMS;
    size_t used = 0;

    buffer[0] = '\0';
    if (shown == 0) {
        snprintf(buffer, size, "-");
    }
    for (i = 0; i < shown && used < size; i++) {
        used += (size_t)snprintf(buffer + used, size - used, "%s%s %s", i ? ", " : "",
                                 f->params[i].type, f->params[i].name);
    }
}

static void writeFunctions(FILE *out) {
    char params[512];
    int wName = 10, wReturn = 8, wParams = 8, i;

    if (funcCount == 0) {
        return;
    }
    for (i = 0; i < funcCount; i++) {
        paramText(funcRows[i], params, sizeof params);
        wName = widen(wName, strlen(funcRows[i]->name));
        wReturn = widen(wReturn, strlen(funcRows[i]->returnType));
        wParams = widen(wParams, strlen(params));
    }
    fprintf(out, "FUNCTIONS\n%-*s%-*s%-*s%s\n", wName, "NAME", wReturn, "RETURN",
            wParams, "PARAMS", "LINE");
    for (i = 0; i < funcCount; i++) {
        paramText(funcRows[i], params, sizeof params);
        fprintf(out, "%-*s%-*s%-*s%d\n", wName, funcRows[i]->name, wReturn,
                funcRows[i]->returnType, wParams, params, funcRows[i]->declLine);
    }
    fprintf(out, "--\n");
}

static void writePadding(FILE *out, int bytes, int offset) {
    if (bytes > 0) {
        fprintf(out, "(%d padding byte%s at offset %d)\n", bytes, bytes == 1 ? "" : "s", offset);
    }
}

static void writeStructs(FILE *out) {
    int i, j;

    if (structCount == 0) {
        return;
    }
    fprintf(out, "STRUCTS\n");
    for (i = 0; i < structCount; i++) {
        const StructSymbol *st = structRows[i];
        int wMember = 10, wType = 8, wBytes = 7, expected = 0;

        for (j = 0; j < st->memberCount; j++) {
            wMember = widen(wMember, strlen(st->members[j].name));
            wType = widen(wType, strlen(st->members[j].type));
        }
        if (i > 0) {
            fputc('\n', out);
        }
        fprintf(out, "STRUCT %s (total %d bytes, alignment %d)\n",
                st->name, st->totalBytes, structAlignment(st));
        fprintf(out, "%-*s%-*s%-*s%s\n", wMember, "MEMBER", wType, "TYPE", wBytes, "BYTES", "OFFSET");
        for (j = 0; j < st->memberCount; j++) {
            const Member *m = &st->members[j];
            fprintf(out, "%-*s%-*s%-*d%d\n", wMember, m->name, wType, m->type,
                    wBytes, m->sizeBytes, m->offset);
        }
        for (j = 0; j < st->memberCount; j++) {
            writePadding(out, st->members[j].offset - expected, expected);
            expected = st->members[j].offset + st->members[j].sizeBytes;
        }
        writePadding(out, st->totalBytes - expected, expected);
    }
    fprintf(out, "--\n");
}

static void writeDiagnostics(FILE *out) {
    int pass, i;
    for (pass = 0; pass < 2; pass++) {
        fprintf(out, "%s: %d\n", pass ? "WARNINGS" : "ERRORS", pass ? warningTotal : errorTotal);
        for (i = 0; i < diagCount; i++) {
            if (diags[i].isWarning == pass) {
                fprintf(out, "  line %d: %s\n", diags[i].error.line, diags[i].error.msg);
            }
        }
    }
}

/* ==================== Program entry ==================== */

/* Returns a new string: path with its extension replaced by ext. */
static char *withExtension(const char *path, const char *ext) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = path;
    const char *dot;
    size_t stem;
    char *result;

    if (slash != NULL && slash + 1 > base) base = slash + 1;
    if (backslash != NULL && backslash + 1 > base) base = backslash + 1;

    dot = strrchr(base, '.');
    stem = dot != NULL ? (size_t)(dot - path) : strlen(path);

    result = xmalloc(stem + strlen(ext) + 1);
    memcpy(result, path, stem);
    strcpy(result + stem, ext);
    return result;
}

static void freeTables(void) {
    int i;
    for (i = 0; i < symbolCount; i++) free(symbolRows[i]);
    for (i = 0; i < funcCount; i++)   free(funcRows[i]);
    for (i = 0; i < structCount; i++) free(structRows[i]);
    free(symbolRows);
    free(funcRows);
    free(structRows);
    free(diags);
}

int main(int argc, char **argv) {
    char *symPath;
    ASTNode *root;
    FILE *out;
    int status, i, writeFailed;

    if (argc != 2) {
        fprintf(stderr, "Usage: semantic prog.ast\n");
        return 2;
    }
    inputPath = argv[1];
    symPath = withExtension(argv[1], ".sym");
    if (strcmp(symPath, argv[1]) == 0) {
        fprintf(stderr, "semantic: the input must be a .ast file, not '%s'\n", argv[1]);
        free(symPath);
        return 2;
    }
    remove(symPath);   /* never leave an out-of-date file behind */

    root = readTree(argv[1]);
    analyzeProgram(root);
    if (diagCount > 1) {
        qsort(diags, (size_t)diagCount, sizeof(Diagnostic), compareDiagnostics);
    }

    out = fopen(symPath, "w");
    if (out == NULL) {
        fprintf(stderr, "semantic: cannot write '%s'\n", symPath);
        free(symPath);
        return 2;
    }
    writeSymbols(out);
    writeFunctions(out);
    writeStructs(out);
    writeDiagnostics(out);
    writeFailed = ferror(out) != 0;
    if (fclose(out) != 0) {
        writeFailed = 1;
    }
    if (writeFailed) {
        fprintf(stderr, "semantic: cannot write '%s'\n", symPath);
        remove(symPath);
        free(symPath);
        return 2;
    }

    for (i = 0; i < diagCount; i++) {
        fprintf(stderr, "[%s] line %d: %s%s\n", diags[i].error.phase, diags[i].error.line,
                diags[i].isWarning ? "warning: " : "", diags[i].error.msg);
    }
    printf("Semantic: %d error%s, %d warning%s -> %s\n", errorTotal, errorTotal == 1 ? "" : "s",
           warningTotal, warningTotal == 1 ? "" : "s", symPath);

    status = errorTotal > 0 ? 1 : 0;
    astFreeAll();
    freeTables();
    free(symPath);
    return status;
}
