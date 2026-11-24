# Citu Compiler

CS426 Compiler Construction. Student ID: Bscs23168. Name: Mihab Khan.

This repository contains the lexer, parser and semantic analyzer. The language
and file formats are specified in
[doc/Citu_Compiler_Documentation.md](doc/Citu_Compiler_Documentation.md).

## Build

```sh
gcc -std=c99 -Wall -o lexer src/lexer.c
gcc -std=c99 -Wall -o parser src/parser.c
gcc -std=c99 -Wall -o semantic src/semantic.c
```

## Run

```sh
./lexer prog.citu     # reads prog.citu, writes prog.tok
./parser prog.tok     # reads prog.tok, writes prog.ast
./semantic prog.ast   # reads prog.ast, writes prog.sym
```

Lexical and syntax errors are printed with their line and column numbers.
Semantic errors and warnings are printed with their line numbers, and are also
listed at the end of `prog.sym`.
