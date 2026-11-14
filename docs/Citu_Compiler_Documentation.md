# Citu Compiler – Project Documentation

**Course:** Compiler Construction
**Student ID:** `Bscs23168`
**Name:** `Mihab Khan`
**Repository URL:** `https://github.com/mihabkhan/citu-compiler`

---

## Table of Contents
1. [Example Programs](#1-example-programs)
2. [Citu Language Grammar](#2-citu-language-grammar)
3. [Data Structures](#3-data-structures)
4. [Intermediate File Formats](#4-intermediate-file-formats)
5. [Git Repository Details](#5-git-repository-details)

---

## Overview

Citu is a small imperative language with Urdu/Roman-Urdu keywords. It supports
typed scalar variables, one-dimensional arrays, a `jabtak` (while) loop, an
`agar` (if) / `warna` (else) conditional, a `nikal` (break) statement, and
assignment with `medalo` ("put into").

| Citu keyword | Meaning | C equivalent |
|---|---|---|
| `jabtak` | while (loop) | `while` |
| `agar` | if | `if` |
| `warna` | else | `else` |
| `shro` | begin block | `{` |
| `khatam` | end block | `}` |
| `nikal` | break out of loop | `break` |
| `medalo` | assignment ("put into") | `=` |
| `likho` | print | `printf` |
| `parho` | read input | `scanf` |
| `int16`, `int32`, `uint16`, `uint32`, `real`, `char` | data types | `short`, `int`, `unsigned short`, `unsigned int`, `float`, `char` |

> Because assignment uses `medalo`, the single `=` symbol in Citu is the
> **equality comparison** operator (e.g. `agar (a[i] = 0)`).

---

## 1. Example Programs

### Program 1 – Sum array elements until a zero is found
Shows array declaration, the `jabtak` loop, an `agar` condition, `nikal` (break) and `medalo` (assignment).

```
// sum.citu
int32 a[5];
int32 i;
int32 sum;

a[0] medalo 4;
a[1] medalo 7;
a[2] medalo 0;
a[3] medalo 9;
a[4] medalo 2;

i   medalo 0;
sum medalo 0;

jabtak (i < 5) shro
    agar (a[i] = 0) shro
        nikal;
    khatam
    sum medalo (sum + a[i]);
    i   medalo (i + 1);
khatam

likho(sum);          // prints 11
```

### Program 2 – Factorial using unsigned and real types
Shows `uint32`, `real`, input with `parho`, arithmetic and printing.

```
// factorial.citu
uint32 n;
uint32 fact;
uint32 k;
real   avg;

parho(n);
fact medalo 1;
k    medalo 1;

jabtak (k <= n) shro
    fact medalo fact * k;
    k    medalo k + 1;
khatam

likho(fact);

avg medalo 10.5 / 2.0;
likho(avg);          // prints 5.25
```

### Program 3 – Find the largest element and search for a character
Shows `int16`, `uint16`, `char`, `agar … warna … khatam`, and nested blocks.

```
// largest.citu
int16  nums[6];
int16  max;
uint16 i;
char   grade;

nums[0] medalo 12;
nums[1] medalo -3;
nums[2] medalo 45;
nums[3] medalo 7;
nums[4] medalo 45;
nums[5] medalo 19;

max medalo nums[0];
i   medalo 1;

jabtak (i < 6) shro
    agar (nums[i] > max) shro
        max medalo nums[i];
    khatam
    i medalo i + 1;
khatam

agar (max >= 40) shro
    grade medalo 'A';
warna
    grade medalo 'B';
khatam

likho(max);          // prints 45
likho(grade);        // prints A
```

---

## 2. Citu Language Grammar

### 2.1 Lexical Rules (Tokens)

| Token class | Pattern / Lexemes |
|---|---|
| **Keywords** | `jabtak` `agar` `warna` `shro` `khatam` `nikal` `medalo` `likho` `parho` |
| **Type keywords** | `int16` `int32` `uint16` `uint32` `real` `char` |
| **IDENTIFIER** | `[a-zA-Z_][a-zA-Z0-9_]*` (that is not a keyword) |
| **INT_LITERAL** | `[0-9]+` |
| **REAL_LITERAL** | `[0-9]+\.[0-9]+` |
| **CHAR_LITERAL** | `'` any single printable character `'` |
| **Arithmetic operators** | `+` `-` `*` `/` `%` |
| **Relational operators** | `=` `!=` `<` `>` `<=` `>=` |
| **Punctuation** | `(` `)` `[` `]` `;` |
| **Comment** | `//` up to end of line (ignored) |
| **Whitespace** | space, tab, `\r`, `\n` (ignored; `\n` increments the line counter) |

Lexical notes:
- Keywords are case-sensitive and in lowercase.
- Longest match is used (`<=` is one token, not `<` followed by `=`).
- An identifier can be at most 31 characters long.
- Any character that does not match a rule is a **lexical error**, reported with its line and column.

### 2.2 Syntax Rules (EBNF)

```ebnf
program        ::= { declaration | statement } EOF ;

declaration    ::= type IDENTIFIER [ "[" INT_LITERAL "]" ] ";" ;
type           ::= "int16" | "int32" | "uint16" | "uint32" | "real" | "char" ;

statement      ::= assign_stmt
                 | while_stmt
                 | if_stmt
                 | break_stmt
                 | print_stmt
                 | read_stmt ;

assign_stmt    ::= lvalue "medalo" expression ";" ;
while_stmt     ::= "jabtak" "(" condition ")" "shro" { statement } "khatam" ;
if_stmt        ::= "agar" "(" condition ")" "shro" { statement }
                   [ "warna" { statement } ]
                   "khatam" ;
break_stmt     ::= "nikal" ";" ;
print_stmt     ::= "likho" "(" expression ")" ";" ;
read_stmt      ::= "parho" "(" lvalue ")" ";" ;

condition      ::= expression relop expression ;
relop          ::= "=" | "!=" | "<" | ">" | "<=" | ">=" ;

expression     ::= term { ( "+" | "-" ) term } ;
term           ::= factor { ( "*" | "/" | "%" ) factor } ;
factor         ::= INT_LITERAL
                 | REAL_LITERAL
                 | CHAR_LITERAL
                 | lvalue
                 | "(" expression ")"
                 | "-" factor ;

lvalue         ::= IDENTIFIER [ "[" expression "]" ] ;
```

### 2.3 Semantic Rules
1. A variable must be declared before it is used, and it cannot be declared twice.
2. An array size must be a positive integer literal. An array index must be an integer expression.
3. `nikal` is allowed only inside a `jabtak` loop.
4. Type rules: integer types can be mixed (the result takes the wider type). Mixing an integer with `real` gives `real`. A `char` is treated as an 8-bit integer in arithmetic.
5. Assigning a `real` value to an integer variable is a **warning** (truncation). Assigning a negative literal to `uintXX` is an **error**.

### 2.4 Operator Precedence (high → low)
| Level | Operators | Associativity |
|---|---|---|
| 1 | unary `-` | right |
| 2 | `*` `/` `%` | left |
| 3 | `+` `-` | left |
| 4 | `=` `!=` `<` `>` `<=` `>=` | none (one per condition) |

---

## 3. Data Structures

### 3.1 Token (Lexer output)
```c
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
```

### 3.2 Token List
This is a dynamic array (vector) of `Token`. The lexer fills it, and the parser reads it from front to back with a cursor (`current` index). The parser uses `peek()` to look one token ahead (LL(1)).
```c
typedef struct {
    Token *items;
    int    count;
    int    capacity;
} TokenList;
```

### 3.3 Symbol Table
This is a hash table keyed by identifier name. It uses separate chaining with 211 buckets. The semantic analyzer fills it, and the code generators read it.
```c
typedef enum { K_VARIABLE, K_ARRAY } SymKind;

typedef struct Symbol {
    char    name[32];
    char    type[8];      // int16, int32, uint16, uint32, real, char
    SymKind kind;
    int     arraySize;    // 0 for scalars
    int     sizeBytes;    // 2, 4, 4 (real), 1 (char)
    int     offset;       // memory offset in the data segment
    int     declLine;
    struct Symbol *next;  // chaining
} Symbol;

typedef struct {
    Symbol *buckets[211];
} SymbolTable;
```
Operations: `insert(name, …)` (fails on redeclaration), `lookup(name)`, and `dump(file)`.

### 3.4 Abstract Syntax Tree (AST)
```c
typedef enum {
    N_PROGRAM, N_DECL, N_ASSIGN, N_WHILE, N_IF, N_BREAK,
    N_PRINT, N_READ, N_COND, N_BINOP, N_UNARY,
    N_IDENT, N_INDEX, N_INT, N_REAL, N_CHAR
} NodeType;

typedef struct ASTNode {
    NodeType  kind;
    char      value[64];       // operator, name or literal text
    char      dataType[8];     // filled in by semantic analysis
    int       line;
    struct ASTNode **children; // ordered child list
    int       childCount;
    struct ASTNode  *elseBody; // used only by N_IF
} ASTNode;
```

### 3.5 Intermediate Code (Three-Address Code / Quadruples)
```c
typedef struct {
    char op[12];       // +, -, *, /, =, []=, =[], goto, ifFalse, label, print, read
    char arg1[32];
    char arg2[32];
    char result[32];
} Quad;

typedef struct { Quad *code; int count, capacity; } QuadList;
```
Supporting counters: `tempCount` creates the temporaries `t1, t2, …`, and `labelCount` creates the labels `L1, L2, …`. A **loop-exit label stack** gives `nikal` the label it should jump to.

### 3.6 Error List
```c
typedef struct { int line, column; char phase[10]; char msg[128]; } CompileError;
```
Each phase adds its errors to this list. The list is printed at the end of compilation.

---

## 4. Intermediate File Formats

### Compiler pipeline
```
prog.citu ──► [Lexer] ──► prog.tok
                              │
                              ▼
                         [Parser] ──► prog.ast
                              │
                              ▼
                   [Semantic Analyzer] ──► prog.sym
                              │
                              ▼
               [Intermediate Code Gen] ──► prog.tac
                              │
                              ▼
                     [Code Generator] ──► prog.asm (target code)
```

All intermediate files are plain UTF-8 text. They use the same base name as the source file.

### 4.1 Lexer output – `prog.tok`
The file has one token per line in tab-separated columns: `LINE  COL  TOKEN_TYPE  LEXEME`. The last line is always `EOF`.
```
1	1	TYPE	int32
1	7	IDENTIFIER	a
1	8	PUNCT	[
1	9	INT_LIT	5
1	10	PUNCT	]
1	11	PUNCT	;
...
16	1	KEYWORD	jabtak
16	8	PUNCT	(
16	9	IDENTIFIER	i
16	11	RELOP	<
16	13	INT_LIT	5
16	14	PUNCT	)
16	16	KEYWORD	shro
...
27	1	EOF	<EOF>
```

### 4.2 Parser output – `prog.ast`
This is an indented tree with one node per line: `NODE_TYPE [value] (line N)`. Each level of nesting is indented by two spaces.
```
PROGRAM
  DECL int32 a[5] (line 1)
  DECL int32 i (line 2)
  DECL int32 sum (line 3)
  ASSIGN (line 5)
    INDEX a
      INT 0
    INT 4
  ...
  WHILE (line 16)
    COND <
      IDENT i
      INT 5
    IF (line 17)
      COND =
        INDEX a
          IDENT i
        INT 0
      BREAK (line 18)
    ASSIGN (line 20)
      IDENT sum
      BINOP +
        IDENT sum
        INDEX a
          IDENT i
    ASSIGN (line 21)
      IDENT i
      BINOP +
        IDENT i
        INT 1
  PRINT (line 24)
    IDENT sum
```

### 4.3 Semantic analyzer output – `prog.sym`
This is the symbol table as a fixed-width table. Any semantic errors and warnings appear below it.
```
NAME      TYPE    KIND      SIZE  BYTES  OFFSET  LINE
a         int32   ARRAY     5     20     0       1
i         int32   VARIABLE  0     4      20      2
sum       int32   VARIABLE  0     4      24      3
--
ERRORS: 0
WARNINGS: 0
```

### 4.4 Intermediate code output – `prog.tac`
This file holds three-address code. Each line is: `index: result = arg1 op arg2`, or a label, `goto`, `ifFalse`, `print` or `read` instruction.
```
0:  a[0] = 4
1:  a[1] = 7
2:  a[2] = 0
3:  a[3] = 9
4:  a[4] = 2
5:  i = 0
6:  sum = 0
7:  L1:
8:  ifFalse i < 5 goto L2
9:  t1 = a[i]
10: ifFalse t1 == 0 goto L3
11: goto L2              // nikal
12: L3:
13: t2 = a[i]
14: t3 = sum + t2
15: sum = t3
16: t4 = i + 1
17: i = t4
18: goto L1
19: L2:
20: print sum
```

### 4.5 Code generator output – `prog.asm`
This is the final target code: x86 / MIPS assembly, or C source if the backend translates to C. A `.data` section is built from the symbol table, and a `.text` section is built from the TAC. Example (MIPS):
```
.data
a:    .space 20
i:    .word 0
sum:  .word 0
.text
main:
    li   $t0, 4
    sw   $t0, a+0
    ...
L1: lw   $t0, i
    li   $t1, 5
    bge  $t0, $t1, L2
    ...
```

---

## 5. Git Repository Details

| Field | Value |
|---|---|
| **Student ID** | Bscs23168 |
| **Repository URL** | https://github.com/mihabkhan/citu-compiler |
| **Commit # (hash) of this submission** | Given in submission text |
| **Collaborators added** | umarsuleman@gmail.com, mscs24052@itu.edu.pk |

### Repository layout
```
citu-compiler/
├── docs/
│   └── Citu_Compiler_Documentation.md   ← this document
└── README.md
```
