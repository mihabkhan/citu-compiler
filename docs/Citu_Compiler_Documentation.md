# Citu Compiler – Project Documentation

**Course:** CS426 – Compiler Construction
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
typed scalar variables with optional initialisation, one-dimensional arrays,
structs (`dhancha`), functions (`kaam`), a `jabtak` (while) loop, an
`agar` (if) / `warna` (else) conditional, a `nikal` (break) statement, and
assignment with `medalo` ("put into").

### Language features checklist

| Required feature | Supported? | Citu syntax |
|---|---|---|
| Functions | Yes | `kaam <type\|khali> name(params) shro … khatam`, `wapas expr;` |
| `while` loop | Yes | `jabtak (cond) shro … khatam` |
| `if` / `else` | Yes | `agar (cond) shro … warna … khatam` |
| Initialisation (init) | Yes | `int32 x medalo 5;` |
| `struct` | Yes | `dhancha Name shro … khatam`, access with `v.field` |
| Classes | **No** (intentionally not part of Citu) | – |
| `do … while` | **No** (intentionally not part of Citu) | – |

### Keywords

| Citu keyword | Meaning | C equivalent |
|---|---|---|
| `jabtak` | while (loop) | `while` |
| `agar` | if | `if` |
| `warna` | else | `else` |
| `shro` | begin block | `{` |
| `khatam` | end block | `}` |
| `nikal` | break out of loop | `break` |
| `medalo` | assignment / initialisation ("put into") | `=` |
| `likho` | print | `printf` |
| `parho` | read input | `scanf` |
| `kaam` | define a function | function definition |
| `wapas` | return from a function | `return` |
| `khali` | "empty": function returns nothing | `void` |
| `dhancha` | structure (struct) | `struct` |

> Because assignment uses `medalo`, the single `=` symbol in Citu is the
> **equality comparison** operator (e.g. `agar (a[i] = 0)`).

### Data types used

| Citu type | Meaning | Size | Range | C equivalent |
|---|---|---|---|---|
| `int16` | signed 16-bit integer | 2 bytes | -32768 … 32767 | `short` |
| `int32` | signed 32-bit integer | 4 bytes | -2147483648 … 2147483647 | `int` |
| `uint16` | unsigned 16-bit integer | 2 bytes | 0 … 65535 | `unsigned short` |
| `uint32` | unsigned 32-bit integer | 4 bytes | 0 … 4294967295 | `unsigned int` |
| `real` | 32-bit floating point | 4 bytes | IEEE-754 single | `float` |
| `char` | one character (8-bit integer) | 1 byte | 0 … 255 | `char` |
| `khali` | no value (only as a function return type) | – | – | `void` |

Struct types are user-defined with `dhancha` and are built from the types above.

---

## 1. Example Programs

### Program 1 – Sum array elements until a zero is found
**Features shown:** array declaration, `jabtak` loop, `agar` condition, `nikal` (break), `medalo` (assignment).

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

### Program 2 – Functions: factorial and average
**Features shown:** functions with parameters and return values (`kaam`, `wapas`), a `khali` (void) function, function calls as expressions and as statements, initialisation of local variables, `jabtak`, `uint32` and `real` types, input with `parho`.

```
// factorial.citu
uint32 n;
uint32 fact;
real   avg;

kaam uint32 factorial(uint32 num) shro
    uint32 result medalo 1;
    uint32 k medalo 1;
    jabtak (k <= num) shro
        result medalo result * k;
        k      medalo k + 1;
    khatam
    wapas result;
khatam

kaam real average(real a, real b) shro
    wapas (a + b) / 2.0;
khatam

kaam khali printTwice(uint32 v) shro
    likho(v);
    likho(v);
khatam

parho(n);
fact medalo factorial(n);
printTwice(fact);    // for n = 5 prints 120 and 120
avg medalo average(10.5, 2.0);
likho(avg);          // prints 6.25
```

### Program 3 – Struct, largest element and grade
**Features shown:** `dhancha` (struct) definition, struct variable and member access (`s.marks`), initialisation of a variable, `int16`, `uint16`, `char`, `agar … warna … khatam`, nested blocks.

```
// largest.citu
dhancha Student shro
    uint16 roll;
    int16  marks;
    char   grade;
khatam

dhancha Student s;
int16  nums[6];
uint16 i medalo 1;

nums[0] medalo 12;
nums[1] medalo -3;
nums[2] medalo 45;
nums[3] medalo 7;
nums[4] medalo 45;
nums[5] medalo 19;

s.roll  medalo 101;
s.marks medalo nums[0];

jabtak (i < 6) shro
    agar (nums[i] > s.marks) shro
        s.marks medalo nums[i];
    khatam
    i medalo i + 1;
khatam

agar (s.marks >= 40) shro
    s.grade medalo 'A';
warna
    s.grade medalo 'B';
khatam

likho(s.roll);       // prints 101
likho(s.marks);      // prints 45
likho(s.grade);      // prints A
```

---

## 2. Citu Language Grammar

### 2.1 Lexical Rules (Tokens)

| Token class | Pattern / Lexemes |
|---|---|
| **Keywords** | `jabtak` `agar` `warna` `shro` `khatam` `nikal` `medalo` `likho` `parho` `kaam` `wapas` `khali` `dhancha` |
| **Type keywords** | `int16` `int32` `uint16` `uint32` `real` `char` |
| **IDENTIFIER** | `[a-zA-Z_][a-zA-Z0-9_]*` (that is not a keyword) |
| **INT_LITERAL** | `[0-9]+` |
| **REAL_LITERAL** | `[0-9]+\.[0-9]+` |
| **CHAR_LITERAL** | `'` any single printable character `'` |
| **Arithmetic operators** | `+` `-` `*` `/` `%` |
| **Relational operators** | `=` `!=` `<` `>` `<=` `>=` |
| **Punctuation** | `(` `)` `[` `]` `;` `,` `.` |
| **Comment** | `//` up to end of line (ignored) |
| **Whitespace** | space, tab, `\r`, `\n` (ignored; `\n` increments the line counter) |

Lexical notes:
- Keywords are case-sensitive and in lowercase.
- Longest match is used (`<=` is one token, not `<` followed by `=`; `10.5` is one REAL_LITERAL, while the `.` in `s.roll` is a separate PUNCT token).
- An identifier can be at most 31 characters long.
- `,` separates parameters and arguments. `.` selects a struct member.
- Any character that does not match a rule is a **lexical error**, reported with its line and column.

### 2.2 Syntax Rules (EBNF)

```ebnf
program        ::= { struct_def | function_def | declaration | statement } EOF ;

(* ---------- structs ---------- *)
struct_def     ::= "dhancha" IDENTIFIER "shro" { type IDENTIFIER ";" } "khatam" ;

(* ---------- declarations (with optional init) ---------- *)
declaration    ::= type IDENTIFIER ";"
                 | type IDENTIFIER "[" INT_LITERAL "]" ";"
                 | type IDENTIFIER "medalo" expression ";"
                 | "dhancha" IDENTIFIER IDENTIFIER ";" ;
type           ::= "int16" | "int32" | "uint16" | "uint32" | "real" | "char" ;

(* ---------- functions ---------- *)
function_def   ::= "kaam" ( type | "khali" ) IDENTIFIER "(" [ param_list ] ")"
                   "shro" { declaration | statement } "khatam" ;
param_list     ::= type IDENTIFIER { "," type IDENTIFIER } ;
arg_list       ::= expression { "," expression } ;

(* ---------- statements ---------- *)
statement      ::= assign_stmt
                 | while_stmt
                 | if_stmt
                 | break_stmt
                 | print_stmt
                 | read_stmt
                 | return_stmt
                 | call_stmt ;

assign_stmt    ::= lvalue "medalo" expression ";" ;
while_stmt     ::= "jabtak" "(" condition ")" "shro" { statement } "khatam" ;
if_stmt        ::= "agar" "(" condition ")" "shro" { statement }
                   [ "warna" { statement } ]
                   "khatam" ;
break_stmt     ::= "nikal" ";" ;
print_stmt     ::= "likho" "(" expression ")" ";" ;
read_stmt      ::= "parho" "(" lvalue ")" ";" ;
return_stmt    ::= "wapas" [ expression ] ";" ;
call_stmt      ::= IDENTIFIER "(" [ arg_list ] ")" ";" ;

(* ---------- expressions ---------- *)
condition      ::= expression relop expression ;
relop          ::= "=" | "!=" | "<" | ">" | "<=" | ">=" ;

expression     ::= term { ( "+" | "-" ) term } ;
term           ::= factor { ( "*" | "/" | "%" ) factor } ;
factor         ::= INT_LITERAL
                 | REAL_LITERAL
                 | CHAR_LITERAL
                 | call_expr
                 | lvalue
                 | "(" expression ")"
                 | "-" factor ;

call_expr      ::= IDENTIFIER "(" [ arg_list ] ")" ;
lvalue         ::= IDENTIFIER
                 | IDENTIFIER "[" expression "]"
                 | IDENTIFIER "." IDENTIFIER ;
```

Parsing notes:
- The parser is a predictive recursive-descent parser. It looks ahead up to 3 tokens with `peek(k)`:
  - `dhancha Name shro` is a struct definition, but `dhancha Name var ;` is a declaration.
  - `name (` starts a function call, while `name medalo`, `name [` or `name .` starts an assignment.
  - Inside an expression, `name (` is a call (`call_expr`) and any other `name` is an `lvalue`.
- `nikal` and `wapas` are accepted by the grammar anywhere a statement is allowed; the semantic analyzer rejects them outside a loop / function (rules 3 and 7).
- `do … while` and classes are **not** part of the grammar.
- Function definitions cannot be nested and cannot appear inside `jabtak`/`agar` blocks. Declarations can appear at top level or directly in a function body (not inside `jabtak`/`agar` blocks).
- Top-level statements form the main program and run in source order.

### 2.3 Semantic Rules
1. A variable must be declared before it is used, and it cannot be declared twice in the same scope. Variables declared at top level are **global**. Variables and parameters declared in a function are **local** to it and hide a global of the same name.
2. An array size must be a positive integer literal. An array index must be an integer expression.
3. `nikal` is allowed only inside a `jabtak` loop.
4. Type rules: integer types can be mixed (the result takes the wider type). Mixing an integer with `real` gives `real`. A `char` is treated as an 8-bit integer in arithmetic. `/` on two integers is integer division. `%` is allowed only on integer types (using it with `real` is an **error**). `likho` prints a `char` as a character, a `real` as a decimal number and every other integer type as a number.
5. Assigning a `real` value to an integer variable is a **warning** (truncation). Assigning a negative literal to `uintXX` is an **error**.
6. **Initialisation:** `type x medalo expr;` is checked exactly like an assignment (rule 5 applies). Arrays cannot be initialised in their declaration.
7. **Functions:**
   - A function must be defined before it is called, and function names are unique.
   - A call must pass the same number of arguments as the function has parameters, and each argument must be type-compatible with its parameter (arguments are passed by value).
   - A `khali` function cannot be used inside an expression.
   - `wapas expr;` is required in a non-`khali` function and its type must be compatible with the return type. `wapas;` (no value) is the only form allowed in a `khali` function.
   - `wapas` is allowed only inside a function. A non-`khali` function with a path that reaches the end without `wapas` gives a **warning**.
   - Parameters and return values are scalar types only (no arrays or structs).
   - A function may call itself (recursion). Mutual recursion is not supported because there are no forward declarations.
8. **Structs:**
   - A struct must be defined before a variable of that type is declared.
   - Member names inside one struct must be unique, and members are scalar types only (no arrays or nested structs).
   - `v.f` is valid only if `v` is a struct variable and `f` is a member of its struct.
   - A whole struct cannot be assigned, compared, printed or read; only its members can.

#### Details fixed by the semantic analyzer (Phase 3)
These points were left open by the rules above and are settled as follows:
- **Literal types:** an integer literal is `int32`, or `uint32` if it is larger than 2147483647. A literal larger than 4294967295 is an **error**. A real literal is `real` and a character literal is `char`.
- **Mixed widths:** when a signed and an unsigned integer of the same width are mixed, the result is the unsigned type (for example `int16 + uint16` gives `uint16`).
- **Integer narrowing:** assigning a wider integer value to a narrower integer variable is allowed without a message, but a *constant* that does not fit the target (for example `int16 x medalo 40000;` or `char c medalo 300;`) gives a **warning**.
- **Negative values:** rule 5 covers any negative constant (`-5`, `-2.5`), and applies equally to arguments and return values. `- - 5` and `-0` are not negative.
- **Constant indexes:** an index that is a constant outside `0 … size-1` is an **error**.
- **Division by zero:** `/` or `%` with the constant `0` as the right operand gives a **warning**.
- **Limits:** an array has at most 1,000,000 elements, a function at most 8 parameters (`Param params[8]`), a struct at most 16 members (`Member members[16]`), and one scope at most 16,777,216 bytes of variables. A struct must have at least one member. Exceeding any limit is an **error**.
- **Missing `wapas`:** a path "reaches the end" unless the last such path ends in `wapas`, or in an `agar … warna … khatam` whose two branches both always return. A `jabtak` loop never counts as always returning.
- **Separate name spaces:** variables, functions and structs have separate tables, so a variable and a function may share a name. `f(…)` always means the function and `f` alone always means the variable.

### 2.4 Operator Precedence (high → low)
| Level | Operators | Associativity |
|---|---|---|
| 0 | function call `f(…)`, index `a[i]`, member `s.f` | left |
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
This is a dynamic array (vector) of `Token`. The lexer fills it, and the parser reads it from front to back with a cursor (`current` index). The parser uses `peek(k)` to look up to 3 tokens ahead (see the parsing notes in 2.2).
```c
typedef struct {
    Token *items;
    int    count;
    int    capacity;
} TokenList;
```

### 3.3 Symbol Table (variables, arrays, parameters, struct variables)
This is a hash table keyed by identifier name. It uses separate chaining with 211 buckets. The semantic analyzer fills it, and the code generators read it.

Scoping: there is one **global** `SymbolTable`, and one **local** `SymbolTable` is created for each function while its body is analysed. `lookup` searches the local table first and then the global table.
```c
typedef enum { K_VARIABLE, K_ARRAY, K_PARAM, K_STRUCTVAR } SymKind;

typedef struct Symbol {
    char    name[32];
    char    type[32];     // int16, int32, uint16, uint32, real, char, or a struct name
    SymKind kind;
    int     arraySize;    // 0 for scalars
    int     sizeBytes;    // int16/uint16 = 2, int32/uint32 = 4, real = 4, char = 1,
                          // struct = StructSymbol.totalBytes, array = arraySize * element size
    int     offset;       // global: offset in the data segment; local: offset in the function frame
    char    scope[32];    // "global" or the function name
    int     declLine;
    struct Symbol *next;  // chaining
} Symbol;

typedef struct {
    Symbol *buckets[211];
} SymbolTable;
```
Operations: `insert(name, …)` (fails on redeclaration in the same scope), `lookup(name)`, and `dump(file)`.

Alignment: every offset is a multiple of the natural alignment of the type (1, 2 or 4 bytes). An array uses its element size, and a struct variable uses the struct's alignment.

### 3.4 Function Table
This is a hash table (211 buckets, chaining) keyed by function name. It is filled when a `kaam` definition is analysed and is used to check every call.
```c
typedef struct { char name[32]; char type[8]; } Param;

typedef struct FuncSymbol {
    char  name[32];
    char  returnType[8];    // a type name or "khali"
    int   paramCount;
    Param params[8];
    int   declLine;
    struct FuncSymbol *next;
} FuncSymbol;

typedef struct { FuncSymbol *buckets[211]; } FuncTable;
```

### 3.5 Struct Table
This is a hash table (211 buckets, chaining) keyed by struct name. It stores the layout of every `dhancha`. Members are laid out in order, each aligned to its own size (2-byte members at even offsets, 4-byte members at multiples of 4), and `totalBytes` is rounded up to the largest member alignment, so the layout is valid on the MIPS target.
```c
typedef struct { char name[32]; char type[8]; int sizeBytes; int offset; } Member;

typedef struct StructSymbol {
    char   name[32];
    int    memberCount;
    Member members[16];
    int    totalBytes;
    int    declLine;
    struct StructSymbol *next;
} StructSymbol;

typedef struct { StructSymbol *buckets[211]; } StructTable;
```

### 3.6 Abstract Syntax Tree (AST)
```c
typedef enum {
    N_PROGRAM, N_DECL, N_ASSIGN, N_WHILE, N_IF, N_ELSE, N_BREAK,
    N_PRINT, N_READ, N_COND, N_BINOP, N_UNARY,
    N_IDENT, N_INDEX, N_INT, N_REAL, N_CHAR,
    N_FUNC, N_PARAM, N_RETURN, N_CALL,     // functions
    N_STRUCT, N_FIELD, N_MEMBER            // structs
} NodeType;

typedef struct ASTNode {
    NodeType  kind;
    char      value[128];      // operator, name or literal text
    char      dataType[8];     // filled in by semantic analysis
    int       line;
    struct ASTNode **children; // ordered child list
    int       childCount;
    struct ASTNode  *elseBody; // used only by N_IF
} ASTNode;
```
Notes: an `N_DECL` with an initialiser has the initial-value expression as its only child. `N_FUNC` has the `N_PARAM` nodes first, then the body statements. `N_CALL` has the arguments as children. `N_MEMBER` is a leaf whose `value` is `var.field`. `elseBody` points to an `N_ELSE` node whose children are the `warna` statements. `value` holds 128 bytes because a declaration such as `struct <31-character name> <31-character name>` does not fit in 64.

### 3.7 Intermediate Code (Three-Address Code / Quadruples)
```c
typedef struct {
    char op[12];       // +, -, *, /, %, =, []=, =[], ., .=, goto, ifFalse, label,
                       // print, read, param, call, return
    char relop[3];     // only for ifFalse: <, >, <=, >=, ==, !=
    char arg1[32];
    char arg2[32];
    char result[32];
    char type[8];      // type of the result (int32, real, ...), used by the code generator
} Quad;

typedef struct { Quad *code; int count, capacity; } QuadList;
```
Supporting counters: `tempCount` creates the temporaries `t1, t2, …`, and `labelCount` creates the labels `L1, L2, …`. A **loop-exit label stack** gives `nikal` the label it should jump to. A **current-function pointer** is used to check `wapas`.

How each instruction is stored in a `Quad`:

| Instruction | `op` | `relop` | `arg1` | `arg2` | `result` |
|---|---|---|---|---|---|
| `t = a + b` (also `-` `*` `/` `%`) | `+` | | a | b | t |
| `x = y` | `=` | | y | | x |
| `t = a[i]` | `=[]` | | a | i | t |
| `a[i] = v` | `[]=` | | i | v | a |
| `t = v.f` | `.` | | v | f | t |
| `v.f = t` | `.=` | | f | t | v |
| `L:` or function entry `name:` | `label` | | | | L / name |
| `goto L` | `goto` | | | | L |
| `ifFalse x < y goto L` | `ifFalse` | `<` | x | y | L |
| `print x`, `read x`, `param x` | `print` / `read` / `param` | | x | | |
| `return e` / `return` | `return` | | e | | |
| `t = call f, n` (`call f, n` for a `khali` function) | `call` | | f | n | t (empty if `khali`) |

A `khali` function gets an implicit `return` quad at its end.

### 3.8 Error List
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
                     [Code Generator] ──► prog.asm (target code: MIPS assembly)
```

All intermediate files are plain UTF-8 text. They use the same base name as the source file.

Inputs of each phase: the parser reads `prog.tok`; the semantic analyzer reads `prog.ast`; the intermediate code generator reads `prog.ast` and `prog.sym` (expression types are re-derived from the symbol, function and struct tables); the code generator reads `prog.tac` and `prog.sym` (the `TEMPS` list at the end of `prog.tac` gives the types of the temporaries).

### 4.1 Lexer output – `prog.tok`
The file has one token per line in tab-separated columns: `LINE  COL  TOKEN_TYPE  LEXEME`. The last line is always `EOF` (its line number is the line after the last source line when the file ends with a newline). The examples below are for Program 1 (`sum.citu`); line 1 is the comment, which the lexer skips.
```
2	1	TYPE	int32
2	7	IDENTIFIER	a
2	8	PUNCT	[
2	9	INT_LIT	5
2	10	PUNCT	]
2	11	PUNCT	;
...
15	1	KEYWORD	jabtak
15	8	PUNCT	(
15	9	IDENTIFIER	i
15	11	RELOP	<
15	13	INT_LIT	5
15	14	PUNCT	)
15	16	KEYWORD	shro
...
24	1	EOF	<EOF>
```

### 4.2 Parser output – `prog.ast`
This is an indented tree with one node per line: `NODE_TYPE [value] (line N)`. Each level of nesting is indented by two spaces. `(line N)` is printed only for declaration, function and statement nodes. Example for Program 1:
```
PROGRAM
  DECL int32 a[5] (line 2)
  DECL int32 i (line 3)
  DECL int32 sum (line 4)
  ASSIGN (line 6)
    INDEX a
      INT 0
    INT 4
  ...
  WHILE (line 15)
    COND <
      IDENT i
      INT 5
    IF (line 16)
      COND =
        INDEX a
          IDENT i
        INT 0
      BREAK (line 17)
    ASSIGN (line 19)
      IDENT sum
      BINOP +
        IDENT sum
        INDEX a
          IDENT i
    ASSIGN (line 20)
      IDENT i
      BINOP +
        IDENT i
        INT 1
  PRINT (line 23)
    IDENT sum
```

Other node forms: `UNARY -`, `REAL 10.5` and `CHAR 'A'` are expression nodes. Parentheses produce no node, because the tree shape already records the grouping. A `warna` part is printed as an `ELSE` node after the other children of its `IF`:
```
IF (line 29)
  COND >=
    MEMBER s.marks
    INT 40
  ASSIGN (line 30)
    MEMBER s.grade
    CHAR 'A'
  ELSE
    ASSIGN (line 32)
      MEMBER s.grade
      CHAR 'B'
```

If the source has a lexical error, no `prog.tok` is written. If it has a syntax error, no `prog.ast` is written. In both cases every error found is printed in this form:
```
[Parser] line 3, column 11: expected ';' after the assignment but found 'y'
```

### 4.3 Semantic analyzer output – `prog.sym`
This is the symbol table as a fixed-width table. Functions and structs (if the program has any) get their own sections below it. Any semantic errors and warnings appear at the end. Each column is as wide as its longest entry plus two spaces, with a minimum width. Globals are listed first, then the parameters and locals of each function in source order. Example for Program 1:
```
NAME      TYPE    KIND      SIZE  BYTES  OFFSET  SCOPE   LINE
a         int32   ARRAY     5     20     0       global  2
i         int32   VARIABLE  0     4      20      global  3
sum       int32   VARIABLE  0     4      24      global  4
--
ERRORS: 0
WARNINGS: 0
```

### 4.4 Intermediate code output – `prog.tac`
This file holds three-address code. Each line is: `index: result = arg1 op arg2`, or a label, `goto`, `ifFalse`, `print`, `read`, `param`, `call` or `return` instruction. After the code, a `TEMPS` section lists every temporary with its type. Example for Program 1:
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
10: ifFalse t1 == 0 goto L3     // the Citu "=" relop is written "==" in TAC
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
--
TEMPS
t1  int32
t2  int32
t3  int32
t4  int32
```

### 4.5 Code generator output – `prog.asm`
This is the final target code. The target is **MIPS assembly** (SPIM/MARS style). A `.data` section is built from the global symbol table, and a `.text` section is built from the TAC. Example (Program 1):
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
For functions and structs: each function becomes a label (`jal` to call, `jr $ra` to return), the first arguments are passed in `$a0`–`$a3`, the return value comes back in `$v0` (`$f0` for `real`), locals and parameters live in a stack frame addressed from `$sp`, and a struct variable is reserved as `.space totalBytes` with each member at its byte offset. Functions are emitted first, then `main:` with the top-level statements. Globals are emitted in offset order, with `.align 2` before `.word` / `.float` and `.align 1` before `.half`.

### 4.6 Extra records for functions, structs and initialisers
The following excerpts show how the new features appear in the same files. They are for Program 2 (`factorial.citu`) and Program 3 (`largest.citu`).

**`.tok` (Program 2, lines 6 and 28; Program 3, line 19)**
```
6	1	KEYWORD	kaam
6	6	TYPE	uint32
6	13	IDENTIFIER	factorial
6	22	PUNCT	(
6	23	TYPE	uint32
6	30	IDENTIFIER	num
6	33	PUNCT	)
6	35	KEYWORD	shro
...
28	1	IDENTIFIER	avg
28	5	KEYWORD	medalo
28	12	IDENTIFIER	average
28	19	PUNCT	(
28	20	REAL_LIT	10.5
28	24	PUNCT	,
28	26	REAL_LIT	2.0
28	29	PUNCT	)
28	30	PUNCT	;
...
19	1	IDENTIFIER	s
19	2	PUNCT	.
19	3	IDENTIFIER	roll
19	9	KEYWORD	medalo
19	16	INT_LIT	101
19	19	PUNCT	;
```

**`.ast` (Program 2: function definition, calls and initialiser)**
```
FUNC uint32 factorial (line 6)
  PARAM uint32 num
  DECL uint32 result (line 7)
    INT 1
  DECL uint32 k (line 8)
    INT 1
  WHILE (line 9)
    COND <=
      IDENT k
      IDENT num
    ASSIGN (line 10)
      IDENT result
      BINOP *
        IDENT result
        IDENT k
    ASSIGN (line 11)
      IDENT k
      BINOP +
        IDENT k
        INT 1
  RETURN (line 13)
    IDENT result
...
ASSIGN (line 26)
  IDENT fact
  CALL factorial
    IDENT n
CALL printTwice (line 27)
  IDENT fact
```

**`.ast` (Program 3: struct definition, struct variable, member assignment)**
```
STRUCT Student (line 2)
  FIELD uint16 roll
  FIELD int16 marks
  FIELD char grade
DECL struct Student s (line 8)
...
ASSIGN (line 19)
  MEMBER s.roll
  INT 101
```

**`.sym` (Program 2: globals, locals/parameters with their function as SCOPE, then the function table)**
```
NAME      TYPE    KIND      SIZE  BYTES  OFFSET  SCOPE       LINE
n         uint32  VARIABLE  0     4      0       global      2
fact      uint32  VARIABLE  0     4      4       global      3
avg       real    VARIABLE  0     4      8       global      4
num       uint32  PARAM     0     4      0       factorial   6
result    uint32  VARIABLE  0     4      4       factorial   7
k         uint32  VARIABLE  0     4      8       factorial   8
a         real    PARAM     0     4      0       average     16
b         real    PARAM     0     4      4       average     16
v         uint32  PARAM     0     4      0       printTwice  20
--
FUNCTIONS
NAME        RETURN  PARAMS          LINE
factorial   uint32  uint32 num      6
average     real    real a, real b  16
printTwice  khali   uint32 v        20
--
ERRORS: 0
WARNINGS: 0
```
Offsets of locals and parameters are relative to the start of their function's frame.

**`.sym` with errors and warnings (end of the file).** `prog.ast` stores line numbers but not columns, so semantic messages give the line only. They are listed in line order:
```
--
ERRORS: 2
  line 3: 'y' is not declared
  line 7: 'nikal' can only be used inside a 'jabtak' loop
WARNINGS: 1
  line 5: the real value assigned to 'x' is truncated to int32
```
The same messages are printed to stderr as `[Semantic] line 3: 'y' is not declared` and `[Semantic] line 5: warning: …`. `prog.sym` is written even when there are errors. The analyzer's exit status is 0 when there are no errors (warnings are allowed), 1 when there are errors, and 2 when `prog.ast` cannot be read or does not have the format of section 4.2.

**`.sym` (Program 3: struct layout and variables)**
```
NAME      TYPE     KIND       SIZE  BYTES  OFFSET  SCOPE   LINE
s         Student  STRUCTVAR  0     6      0       global  8
nums      int16    ARRAY      6     12     6       global  9
i         uint16   VARIABLE   0     2      18      global  10
--
STRUCTS
STRUCT Student (total 6 bytes, alignment 2)
MEMBER    TYPE    BYTES  OFFSET
roll      uint16  2      0
marks     int16   2      2
grade     char    1      4
(1 padding byte at offset 5)
--
ERRORS: 0
WARNINGS: 0
```

**`.tac` (Program 2: whole file)**
Functions are written in source order, then `main:`.
```
0:  factorial:
1:  result = 1
2:  k = 1
3:  L1:
4:  ifFalse k <= num goto L2
5:  t1 = result * k
6:  result = t1
7:  t2 = k + 1
8:  k = t2
9:  goto L1
10: L2:
11: return result
12: average:
13: t3 = a + b
14: t4 = t3 / 2.0
15: return t4
16: printTwice:
17: print v
18: print v
19: return               // implicit return of a khali function
20: main:
21: read n
22: param n
23: t5 = call factorial, 1
24: fact = t5
25: param fact
26: call printTwice, 1
27: param 10.5
28: param 2.0
29: t6 = call average, 2
30: avg = t6
31: print avg
--
TEMPS
t1  uint32
t2  uint32
t3  real
t4  real
t5  uint32
t6  real
```

**`.tac` (Program 3: member write, excerpt)**
```
s.roll = 101
t1 = nums[0]          // TEMPS: t1 int16
s.marks = t1
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
├── doc/
│   └── Citu_Compiler_Documentation.md   ← this document
├── src/
│   ├── lexer.c                          ← lexer: prog.citu → prog.tok
│   ├── parser.c                         ← parser: prog.tok → prog.ast
│   └── semantic.c                       ← semantic analyzer: prog.ast → prog.sym
└── README.md
```
