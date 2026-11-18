#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHASE "Lexer"


typedef struct {
    int  line, column;
    char phase[10];
    char msg[128];
} CompileError;

static CompileError *errorList = NULL;
static int errorTotal = 0;
static int errorCapacity = 0;

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "lexer: out of memory\n");
        exit(2);
    }
    return p;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "lexer: out of memory\n");
        exit(2);
    }
    return p;
}

static void errorAdd(const char *phase, int line, int column, const char *fmt, ...) {
    CompileError *e;
    va_list args;

    if (errorTotal == errorCapacity) {
        errorCapacity = errorCapacity ? errorCapacity * 2 : 16;
        errorList = xrealloc(errorList, (size_t)errorCapacity * sizeof(CompileError));
    }
    e = &errorList[errorTotal++];
    e->line = line;
    e->column = column;
    snprintf(e->phase, sizeof e->phase, "%s", phase);

    va_start(args, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, args);
    va_end(args);
}

static int errorCount(void) {
    return errorTotal;
}

static void errorPrintAll(FILE *out) {
    int i;
    for (i = 0; i < errorTotal; i++) {
        fprintf(out, "[%s] line %d, column %d: %s\n",
                errorList[i].phase, errorList[i].line, errorList[i].column, errorList[i].msg);
    }
    if (errorTotal > 0) {
        fprintf(out, "%d error%s found.\n", errorTotal, errorTotal == 1 ? "" : "s");
    }
}

static void errorFreeAll(void) {
    free(errorList);
    errorList = NULL;
    errorTotal = errorCapacity = 0;
}


typedef enum {
    TK_KEYWORD, TK_TYPE, TK_IDENTIFIER,
    TK_INT_LIT, TK_REAL_LIT, TK_CHAR_LIT,
    TK_OPERATOR, TK_RELOP, TK_PUNCT, TK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char      lexeme[64];
    int       line;
    int       column;
} Token;

typedef struct {
    Token *items;
    int    count;
    int    capacity;
} TokenList;

static const char *const TYPE_NAMES[] = {
    "KEYWORD", "TYPE", "IDENTIFIER",
    "INT_LIT", "REAL_LIT", "CHAR_LIT",
    "OPERATOR", "RELOP", "PUNCT", "EOF"
};
static void tokenListInit(TokenList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void tokenListAdd(TokenList *list, TokenType type, const char *lexeme,
                  int line, int column) {
    Token *t;
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 256;
        list->items = xrealloc(list->items,
                               (size_t)list->capacity * sizeof(Token));
    }
    t = &list->items[list->count++];
    t->type = type;
    snprintf(t->lexeme, sizeof t->lexeme, "%s", lexeme);
    t->line = line;
    t->column = column;
}

static void tokenListFree(TokenList *list) {
    free(list->items);
    tokenListInit(list);
}

static int tokensWrite(const TokenList *list, const char *path) {
    int i;
    int ok;
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        return 0;
    }
    for (i = 0; i < list->count; i++) {
        const Token *t = &list->items[i];
        fprintf(f, "%d\t%d\t%s\t%s\n", t->line, t->column,
                TYPE_NAMES[t->type], t->lexeme);
    }
    ok = !ferror(f);
    if (fclose(f) != 0) {
        ok = 0;
    }
    return ok;
}



#define MAX_IDENTIFIER 31
#define MAX_LEXEME 63

static const char *const KEYWORDS[] = {
    "jabtak", "agar", "warna", "shro", "khatam", "nikal", "medalo",
    "likho", "parho", "kaam", "wapas", "khali", "dhancha"
};

static const char *const TYPE_KEYWORDS[] = {
    "int16", "int32", "uint16", "uint32", "real", "char"
};

static const char *src;
static size_t srcLen;
static size_t pos;
static int line;
static int column;

static char at(size_t offset) {
    return pos + offset < srcLen ? src[pos + offset] : '\0';
}

static int atEnd(void) {
    return pos >= srcLen;
}

static void step(void) {
    if (src[pos] == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    pos++;
}

static void stepBy(size_t n) {
    while (n-- > 0 && !atEnd()) {
        step();
    }
}

static int isDigit(char c)  { return c >= '0' && c <= '9'; }
static int isAlpha(char c)  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int isPrintable(char c) { return c >= 0x20 && c <= 0x7E; }

static int inList(const char *word, const char *const *list, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (strcmp(word, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void copyLexeme(char *dest, size_t start, size_t n) {
    memcpy(dest, src + start, n);
    dest[n] = '\0';
}

static void scanWord(TokenList *out) {
    char word[MAX_IDENTIFIER + 1];
    size_t start = pos;
    int startLine = line, startColumn = column;
    size_t n;

    while (isAlpha(at(0)) || isDigit(at(0))) {
        step();
    }
    n = pos - start;

    if (n > MAX_IDENTIFIER) {
        errorAdd(PHASE, startLine, startColumn,
                 "identifier is %lu characters long (the limit is %d)",
                 (unsigned long)n, MAX_IDENTIFIER);
        return;
    }
    copyLexeme(word, start, n);

    if (inList(word, KEYWORDS, sizeof KEYWORDS / sizeof KEYWORDS[0])) {
        tokenListAdd(out, TK_KEYWORD, word, startLine, startColumn);
    } else if (inList(word, TYPE_KEYWORDS,
                      sizeof TYPE_KEYWORDS / sizeof TYPE_KEYWORDS[0])) {
        tokenListAdd(out, TK_TYPE, word, startLine, startColumn);
    } else {
        tokenListAdd(out, TK_IDENTIFIER, word, startLine, startColumn);
    }
}


static void scanNumber(TokenList *out) {
    char text[MAX_LEXEME + 1];
    size_t start = pos;
    int startLine = line, startColumn = column;
    TokenType type = TK_INT_LIT;
    size_t n;

    while (isDigit(at(0))) {
        step();
    }
    if (at(0) == '.' && isDigit(at(1))) {
        type = TK_REAL_LIT;
        step();
        while (isDigit(at(0))) {
            step();
        }
    }
    n = pos - start;

    if (n > MAX_LEXEME) {
        errorAdd(PHASE, startLine, startColumn,
                 "numeric literal is too long (the limit is %d characters)",
                 MAX_LEXEME);
        return;
    }
    copyLexeme(text, start, n);
    tokenListAdd(out, type, text, startLine, startColumn);
}

/* CHAR_LITERAL: a quote, exactly one printable character, and a quote. */
static void scanChar(TokenList *out) {
    int startLine = line, startColumn = column;
    size_t scan = 1;

    if (pos + 2 < srcLen && isPrintable(at(1)) && at(2) == '\'') {
        char text[4];
        copyLexeme(text, pos, 3);
        tokenListAdd(out, TK_CHAR_LIT, text, startLine, startColumn);
        stepBy(3);
        return;
    }

    /* Bad literal: report it once and continue after it. */
    while (pos + scan < srcLen && at(scan) != '\'' && at(scan) != '\n') {
        scan++;
    }
    if (pos + scan < srcLen && at(scan) == '\'') {
        errorAdd(PHASE, startLine, startColumn,
                 "a character literal must hold exactly one printable character");
        stepBy(scan + 1);
    } else {
        errorAdd(PHASE, startLine, startColumn,
                 "unterminated character literal");
        stepBy(scan);
    }
}


static void scanInvalid(void) {
    unsigned char c = (unsigned char)at(0);

    if (isPrintable((char)c)) {
        errorAdd(PHASE, line, column, "invalid character '%c'", (char)c);
    } else {
        errorAdd(PHASE, line, column, "invalid character (byte 0x%02X)", c);
    }
    step();
    if (c >= 0xC0) {
        while (!atEnd() && ((unsigned char)at(0) & 0xC0) == 0x80) {
            pos++;
        }
    }
}


static void scanSymbol(TokenList *out) {
    char c = at(0);
    char next = at(1);
    char text[3] = { c, '\0', '\0' };
    int startLine = line, startColumn = column;

    if ((c == '<' || c == '>' || c == '!') && next == '=') {
        text[1] = next;
        tokenListAdd(out, TK_RELOP, text, startLine, startColumn);
        stepBy(2);
    } else if (c == '<' || c == '>' || c == '=') {
        tokenListAdd(out, TK_RELOP, text, startLine, startColumn);
        step();
    } else if (c != '\0' && strchr("+-*/%", c) != NULL) {
        tokenListAdd(out, TK_OPERATOR, text, startLine, startColumn);
        step();
    } else if (c != '\0' && strchr("()[];,.", c) != NULL) {
        tokenListAdd(out, TK_PUNCT, text, startLine, startColumn);
        step();
    } else {
        scanInvalid();
    }
}

static void lexSource(const char *source, size_t length, TokenList *out) {
    src = source;
    srcLen = length;
    pos = 0;
    line = 1;
    column = 1;

    /* Skip the UTF-8 byte-order mark that some Windows editors add. */
    if (length >= 3 && memcmp(source, "\xEF\xBB\xBF", 3) == 0) {
        pos = 3;
    }

    while (!atEnd()) {
        char c = at(0);

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            step();
        } else if (c == '/' && at(1) == '/') {
            while (!atEnd() && at(0) != '\n') {
                step();
            }
        } else if (isAlpha(c)) {
            scanWord(out);
        } else if (isDigit(c)) {
            scanNumber(out);
        } else if (c == '\'') {
            scanChar(out);
        } else {
            scanSymbol(out);
        }
    }
    tokenListAdd(out, TK_EOF, "<EOF>", line, column);
}


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

static char *readFile(const char *path, size_t *length) {
    size_t capacity = 65536;
    size_t used = 0;
    size_t got;
    char *data;
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        fprintf(stderr, "lexer: cannot open '%s'\n", path);
        return NULL;
    }
    data = xmalloc(capacity);
    while ((got = fread(data + used, 1, capacity - used, f)) > 0) {
        used += got;
        if (used == capacity) {
            capacity *= 2;
            data = xrealloc(data, capacity);
        }
    }
    if (ferror(f)) {
        fprintf(stderr, "lexer: cannot read '%s'\n", path);
        fclose(f);
        free(data);
        return NULL;
    }
    fclose(f);
    *length = used;
    return data;
}

int main(int argc, char **argv) {
    TokenList tokens;
    size_t length = 0;
    int status = 0;
    char *source;
    char *tokPath;

    if (argc != 2) {
        fprintf(stderr, "Usage: lexer prog.citu\n");
        return 2;
    }
    source = readFile(argv[1], &length);
    if (source == NULL) {
        return 2;
    }

    tokPath = withExtension(argv[1], ".tok");
    tokenListInit(&tokens);
    lexSource(source, length, &tokens);
    free(source);

    remove(tokPath);  
    if (errorCount() > 0) {
        status = 1;
    } else if (!tokensWrite(&tokens, tokPath)) {
        fprintf(stderr, "lexer: cannot write '%s'\n", tokPath);
        status = 2;
    } else {
        printf("Lexer: %d tokens -> %s\n", tokens.count, tokPath);
    }

    errorPrintAll(stderr);
    errorFreeAll();
    tokenListFree(&tokens);
    free(tokPath);
    return status;
}
