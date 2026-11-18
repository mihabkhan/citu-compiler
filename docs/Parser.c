#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHASE "Parser"


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
        fprintf(stderr, "parser: out of memory\n");
        exit(2);
    }
    return p;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "parser: out of memory\n");
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
#define TYPE_NAME_COUNT ((int)(sizeof TYPE_NAMES / sizeof TYPE_NAMES[0]))

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

static int typeFromName(const char *name) {
    int i;
    for (i = 0; i < TYPE_NAME_COUNT; i++) {
        if (strcmp(TYPE_NAMES[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int parseRecord(char *text, TokenList *list) {
    char *colText, *typeText, *lexeme, *end;
    long line, column;
    int type;

    colText = strchr(text, '\t');
    if (colText == NULL) return 0;
    *colText++ = '\0';

    typeText = strchr(colText, '\t');
    if (typeText == NULL) return 0;
    *typeText++ = '\0';

    lexeme = strchr(typeText, '\t');
    if (lexeme == NULL) return 0;
    *lexeme++ = '\0';

    line = strtol(text, &end, 10);
    if (end == text || *end != '\0' || line < 1) return 0;
    column = strtol(colText, &end, 10);
    if (end == colText || *end != '\0' || column < 1) return 0;

    type = typeFromName(typeText);
    if (type < 0 || lexeme[0] == '\0' || strlen(lexeme) >= 64) return 0;

    tokenListAdd(list, (TokenType)type, lexeme, (int)line, (int)column);
    return 1;
}

static int tokensRead(TokenList *list, const char *path) {
    char buffer[256];
    int recordNumber = 0;
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        fprintf(stderr, "parser: cannot open '%s'\n", path);
        return 0;
    }
    while (fgets(buffer, sizeof buffer, f) != NULL) {
        size_t len = strcspn(buffer, "\r\n");
        recordNumber++;
        buffer[len] = '\0';
        if (!parseRecord(buffer, list)) {
            fprintf(stderr, "parser: '%s' is malformed at record %d\n",
                    path, recordNumber);
            fclose(f);
            return 0;
        }
    }
    fclose(f);

    if (list->count == 0 || list->items[list->count - 1].type != TK_EOF) {
        fprintf(stderr, "parser: '%s' does not end with an EOF token\n", path);
        return 0;
    }
    return 1;
}


typedef enum {
    N_PROGRAM, N_DECL, N_ASSIGN, N_WHILE, N_IF, N_ELSE, N_BREAK,
    N_PRINT, N_READ, N_COND, N_BINOP, N_UNARY,
    N_IDENT, N_INDEX, N_INT, N_REAL, N_CHAR,
    N_FUNC, N_PARAM, N_RETURN, N_CALL,   
    N_STRUCT, N_FIELD, N_MEMBER         
} NodeType;

typedef struct ASTNode {
    NodeType  kind;
    char      value[128];     
    char      dataType[8];   
    int       line;
    struct ASTNode **children; 
    int       childCount;
    struct ASTNode  *elseBody; 
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

    if (nodeCount == nodeCapacity) {
        nodeCapacity = nodeCapacity ? nodeCapacity * 2 : 256;
        allNodes = xrealloc(allNodes, (size_t)nodeCapacity * sizeof(ASTNode *));
    }
    allNodes[nodeCount++] = node;
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

static const char *kindName(NodeType kind) {
    switch (kind) {
        case N_PROGRAM: return "PROGRAM";
        case N_DECL:    return "DECL";
        case N_ASSIGN:  return "ASSIGN";
        case N_WHILE:   return "WHILE";
        case N_IF:      return "IF";
        case N_ELSE:    return "ELSE";
        case N_BREAK:   return "BREAK";
        case N_PRINT:   return "PRINT";
        case N_READ:    return "READ";
        case N_COND:    return "COND";
        case N_BINOP:   return "BINOP";
        case N_UNARY:   return "UNARY";
        case N_IDENT:   return "IDENT";
        case N_INDEX:   return "INDEX";
        case N_INT:     return "INT";
        case N_REAL:    return "REAL";
        case N_CHAR:    return "CHAR";
        case N_FUNC:    return "FUNC";
        case N_PARAM:   return "PARAM";
        case N_RETURN:  return "RETURN";
        case N_CALL:    return "CALL";
        case N_STRUCT:  return "STRUCT";
        case N_FIELD:   return "FIELD";
        case N_MEMBER:  return "MEMBER";
    }
    return "?";
}

static int holdsStatements(NodeType kind) {
    return kind == N_PROGRAM || kind == N_FUNC || kind == N_WHILE ||
           kind == N_IF || kind == N_ELSE;
}

static int showsLine(const ASTNode *node, int parentHoldsStatements) {
    switch (node->kind) {
        case N_DECL: case N_ASSIGN: case N_WHILE: case N_IF: case N_BREAK:
        case N_PRINT: case N_READ: case N_FUNC: case N_RETURN: case N_STRUCT:
            return 1;
        case N_CALL:
            return parentHoldsStatements;
        default:
            return 0;
    }
}

static void printNode(FILE *out, const ASTNode *node, int depth,
                      int parentHoldsStatements) {
    int i;
    int childContext = holdsStatements(node->kind);

    fprintf(out, "%*s%s", depth * 2, "", kindName(node->kind));
    if (node->value[0] != '\0') {
        fprintf(out, " %s", node->value);
    }
    if (showsLine(node, parentHoldsStatements)) {
        fprintf(out, " (line %d)", node->line);
    }
    fputc('\n', out);

    for (i = 0; i < node->childCount; i++) {
        printNode(out, node->children[i], depth + 1, childContext);
    }
    if (node->elseBody != NULL) {
        printNode(out, node->elseBody, depth + 1, childContext);
    }
}

static void astPrint(FILE *out, const ASTNode *root) {
    printNode(out, root, 0, 0);
}


#define MAX_NESTING 200     
#define MAX_FACTORS 4000    

typedef enum { CTX_TOP, CTX_FUNCTION, CTX_CONTROL } Context;

static const TokenList *toks;
static int current;
static int panicMode;
static int nesting;
static int factorCount;

static ASTNode *parseExpression(void);
static ASTNode *parseFactor(void);
static void     parseItem(ASTNode *parent, Context context);

static const Token *peek(int k) {
    int index = current + k;
    if (index >= toks->count) {
        index = toks->count - 1;
    }
    return &toks->items[index];
}

static const Token *previous(void) {
    return &toks->items[current > 0 ? current - 1 : 0];
}

static const Token *advance(void) {
    const Token *token = peek(0);
    if (token->type != TK_EOF) {
        current++;
    }
    return token;
}

static int isToken(int k, TokenType type, const char *lexeme) {
    const Token *token = peek(k);
    return token->type == type && strcmp(token->lexeme, lexeme) == 0;
}

static int isKeyword(int k, const char *word) { return isToken(k, TK_KEYWORD, word); }
static int isPunct(int k, const char *symbol) { return isToken(k, TK_PUNCT, symbol); }
static int isOperator(int k, const char *op)  { return isToken(k, TK_OPERATOR, op); }
static int atEof(void)                        { return peek(0)->type == TK_EOF; }

static int matchKeyword(const char *word) {
    if (isKeyword(0, word)) {
        advance();
        return 1;
    }
    return 0;
}

static int matchPunct(const char *symbol) {
    if (isPunct(0, symbol)) {
        advance();
        return 1;
    }
    return 0;
}


static const char *describe(const Token *token) {
    static char text[80];
    if (token->type == TK_EOF) {
        return "end of file";
    }
    snprintf(text, sizeof text, "'%s'", token->lexeme);
    return text;
}

static void errorExpected(const char *what) {
    if (!panicMode) {
        errorAdd(PHASE, peek(0)->line, peek(0)->column,
                 "expected %s but found %s", what, describe(peek(0)));
    }
    panicMode = 1;
}

static void errorHere(const Token *token, const char *message) {
    if (!panicMode) {
        errorAdd(PHASE, token->line, token->column, "%s", message);
    }
}

static void expectPunct(const char *symbol, const char *where) {
    if (matchPunct(symbol)) {
        return;
    }
    if (!panicMode) {
        const Token *last = previous();
        errorAdd(PHASE, last->line, last->column + (int)strlen(last->lexeme),
                 "expected '%s' %s but found %s",
                 symbol, where, describe(peek(0)));
    }
    panicMode = 1;
}

static void expectKeyword(const char *word) {
    if (!matchKeyword(word)) {
        char what[32];
        snprintf(what, sizeof what, "'%s'", word);
        errorExpected(what);
    }
}

static const char *expectIdentifier(const char *what) {
    const Token *token = peek(0);

    if (token->type == TK_IDENTIFIER) {
        return advance()->lexeme;
    }
    
    if ((token->type == TK_KEYWORD || token->type == TK_TYPE) &&
        !isKeyword(0, "shro") && !isKeyword(0, "khatam")) {
        char message[128];
        snprintf(message, sizeof message,
                 "'%s' is a reserved word and cannot be used as a name",
                 token->lexeme);
        errorHere(token, message);
        panicMode = 1;
        advance();
        return "";
    }
    errorExpected(what);
    return "";
}

static const char *expectType(void) {
    if (peek(0)->type == TK_TYPE) {
        return advance()->lexeme;
    }
    errorExpected("a type");
    return "";
}

static int startsNewItem(void) {
    static const char *const words[] = {
        "jabtak", "agar", "warna", "khatam", "nikal", "likho",
        "parho", "kaam", "wapas", "dhancha"
    };
    size_t i;
    if (peek(0)->type == TK_TYPE) {
        return 1;
    }
    for (i = 0; i < sizeof words / sizeof words[0]; i++) {
        if (isKeyword(0, words[i])) {
            return 1;
        }
    }
    return 0;
}

static void synchronize(void) {
    panicMode = 0;
    while (!atEof()) {
        if (previous()->type == TK_PUNCT && strcmp(previous()->lexeme, ";") == 0) {
            return;
        }
        if (startsNewItem()) {
            return;
        }
        advance();
    }
}

static void parseArguments(ASTNode *call) {
    expectPunct("(", "before the arguments");
    if (!isPunct(0, ")")) {
        do {
            astAddChild(call, parseExpression());
        } while (matchPunct(","));
    }
    expectPunct(")", "after the arguments");
}

static ASTNode *parseLvalue(void) {
    const Token *name = peek(0);
    const char *text = expectIdentifier("a variable name");
    int line = name->line;

    if (matchPunct("[")) {
        ASTNode *node = astNew(N_INDEX, text, line);
        astAddChild(node, parseExpression());
        expectPunct("]", "after the array index");
        return node;
    }
    if (matchPunct(".")) {
        char member[256];
        const char *field = expectIdentifier("a struct member name");
        snprintf(member, sizeof member, "%s.%s", text, field);
        return astNew(N_MEMBER, member, line);
    }
    return astNew(N_IDENT, text, line);
}

static ASTNode *parseFactorBody(void) {
    const Token *token = peek(0);

    switch (token->type) {
        case TK_INT_LIT:
            advance();
            return astNew(N_INT, token->lexeme, token->line);
        case TK_REAL_LIT:
            advance();
            return astNew(N_REAL, token->lexeme, token->line);
        case TK_CHAR_LIT:
            advance();
            return astNew(N_CHAR, token->lexeme, token->line);
        case TK_IDENTIFIER:
            if (isPunct(1, "(")) {
                ASTNode *call = astNew(N_CALL, advance()->lexeme, token->line);
                parseArguments(call);
                return call;
            }
            return parseLvalue();
        default:
            break;
    }

    if (matchPunct("(")) {
        ASTNode *inner = parseExpression();
        expectPunct(")", "to close the expression");
        return inner;
    }
    if (isOperator(0, "-")) {
        ASTNode *unary = astNew(N_UNARY, "-", token->line);
        advance();
        astAddChild(unary, parseFactor());
        return unary;
    }

    errorExpected("an expression");
    return astNew(N_INT, "0", token->line);   
}

static ASTNode *parseFactor(void) {
    ASTNode *node;

    if (nesting >= MAX_NESTING || factorCount >= MAX_FACTORS) {
        errorHere(peek(0), "expression is too deeply nested or too long");
        panicMode = 1;
        return astNew(N_INT, "0", peek(0)->line);
    }
    nesting++;
    factorCount++;
    node = parseFactorBody();
    nesting--;
    return node;
}

static ASTNode *parseTerm(void) {
    ASTNode *left = parseFactor();

    while (isOperator(0, "*") || isOperator(0, "/") || isOperator(0, "%")) {
        const Token *op = advance();
        ASTNode *node = astNew(N_BINOP, op->lexeme, op->line);
        astAddChild(node, left);
        astAddChild(node, parseFactor());
        left = node;
    }
    return left;
}

static ASTNode *parseExpression(void) {
    ASTNode *left = parseTerm();

    while (isOperator(0, "+") || isOperator(0, "-")) {
        const Token *op = advance();
        ASTNode *node = astNew(N_BINOP, op->lexeme, op->line);
        astAddChild(node, left);
        astAddChild(node, parseTerm());
        left = node;
    }
    return left;
}

static ASTNode *parseCondition(void) {
    ASTNode *left = parseExpression();
    const Token *op = peek(0);
    ASTNode *node;

    if (op->type != TK_RELOP) {
        errorExpected("a relational operator (= != < > <= >=)");
        return left;
    }
    advance();
    node = astNew(N_COND, op->lexeme, op->line);
    astAddChild(node, left);
    astAddChild(node, parseExpression());
    return node;
}


static void parseItems(ASTNode *parent, Context context, int stopAtElse) {
    while (!atEof() && !isKeyword(0, "khatam") &&
           !(stopAtElse && isKeyword(0, "warna"))) {
        int start = current;

        parseItem(parent, context);
        if (panicMode) {
            synchronize();
        }
        if (current == start) {
            advance();  
        }
    }
}

static void expectBlockEnd(const char *opener, int openLine) {
    if (!matchKeyword("khatam")) {
        char what[80];
        snprintf(what, sizeof what, "'khatam' to close the %s on line %d",
                 opener, openLine);
        errorExpected(what);
    }
}

static void expectBlockStart(void) {
    if (matchKeyword("shro")) {
        panicMode = 0;
    } else {
        errorExpected("'shro'");
    }
}

static ASTNode *parseWhile(void) {
    int line = advance()->line;
    ASTNode *node = astNew(N_WHILE, "", line);

    expectPunct("(", "after 'jabtak'");
    astAddChild(node, parseCondition());
    expectPunct(")", "after the condition");
    expectBlockStart();
    parseItems(node, CTX_CONTROL, 0);
    expectBlockEnd("'jabtak'", line);
    return node;
}

static ASTNode *parseIf(void) {
    int line = advance()->line;
    ASTNode *node = astNew(N_IF, "", line);

    expectPunct("(", "after 'agar'");
    astAddChild(node, parseCondition());
    expectPunct(")", "after the condition");
    expectBlockStart();
    parseItems(node, CTX_CONTROL, 1);

    if (isKeyword(0, "warna")) {
        node->elseBody = astNew(N_ELSE, "", advance()->line);
        parseItems(node->elseBody, CTX_CONTROL, 0);
    }
    expectBlockEnd("'agar'", line);
    return node;
}

static ASTNode *parsePrint(void) {
    ASTNode *node = astNew(N_PRINT, "", advance()->line);

    expectPunct("(", "after 'likho'");
    astAddChild(node, parseExpression());
    expectPunct(")", "after the value to print");
    expectPunct(";", "after the statement");
    return node;
}

static ASTNode *parseRead(void) {
    ASTNode *node = astNew(N_READ, "", advance()->line);

    expectPunct("(", "after 'parho'");
    astAddChild(node, parseLvalue());
    expectPunct(")", "after the variable to read");
    expectPunct(";", "after the statement");
    return node;
}

static ASTNode *parseReturn(void) {
    ASTNode *node = astNew(N_RETURN, "", advance()->line);

    if (!isPunct(0, ";")) {
        astAddChild(node, parseExpression());
    }
    expectPunct(";", "after the return statement");
    return node;
}

static ASTNode *parseAssign(void) {
    int line = peek(0)->line;
    ASTNode *node = astNew(N_ASSIGN, "", line);

    astAddChild(node, parseLvalue());
    if (isToken(0, TK_RELOP, "=")) {
        errorExpected("'medalo' for assignment ('=' only compares)");
    } else {
        expectKeyword("medalo");
    }
    astAddChild(node, parseExpression());
    expectPunct(";", "after the assignment");
    return node;
}

static ASTNode *parseCallStatement(void) {
    const Token *name = advance();
    ASTNode *node = astNew(N_CALL, name->lexeme, name->line);

    parseArguments(node);
    expectPunct(";", "after the function call");
    return node;
}

static ASTNode *parseStatement(void) {
    const Token *token = peek(0);

    if (isKeyword(0, "jabtak")) return parseWhile();
    if (isKeyword(0, "agar"))   return parseIf();
    if (isKeyword(0, "likho"))  return parsePrint();
    if (isKeyword(0, "parho"))  return parseRead();
    if (isKeyword(0, "wapas"))  return parseReturn();
    if (isKeyword(0, "nikal")) {
        ASTNode *node = astNew(N_BREAK, "", advance()->line);
        expectPunct(";", "after 'nikal'");
        return node;
    }
    if (token->type == TK_IDENTIFIER) {
        return isPunct(1, "(") ? parseCallStatement() : parseAssign();
    }

    if (isKeyword(0, "khatam")) {
        errorHere(token, "'khatam' has no matching 'shro'");
        panicMode = 1;
    } else if (isKeyword(0, "warna")) {
        errorHere(token, "'warna' has no matching 'agar'");
        panicMode = 1;
    } else {
        errorExpected("a declaration or statement");
    }
    return NULL;
}

static ASTNode *parseDeclaration(void) {
    char value[256];
    const Token *start = peek(0);
    const char *type = expectType();
    const char *name = expectIdentifier("a variable name");
    ASTNode *node;

    if (isPunct(0, "[")) {
        const char *size = "";
        advance();
        if (peek(0)->type == TK_INT_LIT) {
            size = advance()->lexeme;
        } else {
            errorExpected("an integer literal as the array size");
        }
        expectPunct("]", "after the array size");
        snprintf(value, sizeof value, "%s %s[%s]", type, name, size);
        node = astNew(N_DECL, value, start->line);
    } else {
        snprintf(value, sizeof value, "%s %s", type, name);
        node = astNew(N_DECL, value, start->line);
        if (matchKeyword("medalo")) {
            astAddChild(node, parseExpression());
        }
    }
    expectPunct(";", "after the declaration");
    return node;
}

static ASTNode *parseStructVariable(void) {
    char value[256];
    int line = advance()->line;
    const char *structName = expectIdentifier("a struct name");
    const char *name = expectIdentifier("a variable name");

    snprintf(value, sizeof value, "struct %s %s", structName, name);
    expectPunct(";", "after the declaration");
    return astNew(N_DECL, value, line);
}

static ASTNode *parseStructDefinition(void) {
    int line = advance()->line;
    ASTNode *node = astNew(N_STRUCT, expectIdentifier("a struct name"), line);

    expectBlockStart();
    while (!atEof() && !isKeyword(0, "khatam")) {
        char value[256];
        const char *type = expectType();
        const char *name = panicMode ? "" : expectIdentifier("a member name");

        snprintf(value, sizeof value, "%s %s", type, name);
        if (!panicMode) {
            expectPunct(";", "after the struct member");
        }
        if (panicMode) {
            
            while (!atEof() && !isKeyword(0, "khatam") && !matchPunct(";")) {
                advance();
            }
            panicMode = 0;
        } else {
            astAddChild(node, astNew(N_FIELD, value, previous()->line));
        }
    }
    expectBlockEnd("'dhancha'", line);
    return node;
}

static void parseParameters(ASTNode *function) {
    do {
        char value[256];
        const Token *start = peek(0);
        const char *type = expectType();
        const char *name = expectIdentifier("a parameter name");

        snprintf(value, sizeof value, "%s %s", type, name);
        astAddChild(function, astNew(N_PARAM, value, start->line));
    } while (matchPunct(","));
}

static ASTNode *parseFunction(void) {
    char value[256];
    const char *returnType = "";
    int line = advance()->line;
    const char *name;
    ASTNode *node;

    if (isKeyword(0, "khali") || peek(0)->type == TK_TYPE) {
        returnType = advance()->lexeme;
    } else {
        errorExpected("a return type or 'khali'");
    }

    name = expectIdentifier("a function name");
    snprintf(value, sizeof value, "%s %s", returnType, name);
    node = astNew(N_FUNC, value, line);

    expectPunct("(", "after the function name");
    if (!isPunct(0, ")")) {
        parseParameters(node);
    }
    expectPunct(")", "after the parameters");
    expectBlockStart();
    parseItems(node, CTX_FUNCTION, 0);
    expectBlockEnd("'kaam'", line);
    return node;
}

static void parseItem(ASTNode *parent, Context context) {
    const Token *token = peek(0);
    ASTNode *node;

    if (nesting >= MAX_NESTING) {
        errorHere(token, "blocks are too deeply nested");
        panicMode = 1;
        return;
    }
    nesting++;
    factorCount = 0;

    if (isKeyword(0, "dhancha") && isKeyword(2, "shro")) {
        if (context != CTX_TOP) {
            errorHere(token, "a struct can only be defined at the top level");
        }
        node = parseStructDefinition();
    } else if (isKeyword(0, "dhancha")) {
        if (context == CTX_CONTROL) {
            errorHere(token, "declarations are not allowed inside 'jabtak' or 'agar' blocks");
        }
        node = parseStructVariable();
    } else if (isKeyword(0, "kaam")) {
        if (context != CTX_TOP) {
            errorHere(token, "a function can only be defined at the top level");
        }
        node = parseFunction();
    } else if (token->type == TK_TYPE) {
        if (context == CTX_CONTROL) {
            errorHere(token, "declarations are not allowed inside 'jabtak' or 'agar' blocks");
        }
        node = parseDeclaration();
    } else {
        node = parseStatement();
    }

    nesting--;
    if (node != NULL) {
        astAddChild(parent, node);
    }
}

static ASTNode *parseProgram(const TokenList *tokens) {
    ASTNode *root = astNew(N_PROGRAM, "", 1);

    toks = tokens;
    current = 0;
    panicMode = 0;
    nesting = 0;
    factorCount = 0;

    while (!atEof()) {
        int start = current;

        parseItem(root, CTX_TOP);
        if (panicMode) {
            synchronize();
        }
        if (current == start) {
            advance();
        }
    }
    return root;
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

int main(int argc, char **argv) {
    TokenList tokens;
    ASTNode *root;
    char *astPath;
    int status = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: parser prog.tok\n");
        return 2;
    }

    astPath = withExtension(argv[1], ".ast");
    remove(astPath);  
    tokenListInit(&tokens);
    if (!tokensRead(&tokens, argv[1])) {
        tokenListFree(&tokens);
        free(astPath);
        return 2;
    }

    root = parseProgram(&tokens);
    if (errorCount() > 0) {
        status = 1;
    } else {
        FILE *out = fopen(astPath, "w");
        if (out == NULL) {
            status = 2;
        } else {
            astPrint(out, root);
            if (ferror(out)) status = 2;
            if (fclose(out) != 0) status = 2;
        }
        if (status == 0) {
            printf("Parser: syntax OK -> %s\n", astPath);
        } else {
            fprintf(stderr, "parser: cannot write '%s'\n", astPath);
        }
    }

    errorPrintAll(stderr);
    errorFreeAll();
    astFreeAll();
    tokenListFree(&tokens);
    free(astPath);
    return status;
}
